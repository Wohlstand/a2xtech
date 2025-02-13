/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2025 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <set>

#include <nds.h>

#include <Logger/logger.h>
#include <Utils/files.h>
#include <SDL2/SDL_rwops.h>
#include <PGE_File_Formats/file_formats.h>

#include "sdl_proxy/sdl_timer.h"

#include "globals.h"
#include "config.h"
#include "frame_timer.h"

#include "fontman/hardcoded_font.h"

#include "core/render.h"
#include "core/minport/render_minport_shared.h"

#include "core/render_planes.h"

#include "draw_planes.h"

#include "videoGL_alloc_palette.hpp"

#ifdef DEBUG_PLANES
#    define POLY_FOG_Q POLY_FOG
#else
#    define POLY_FOG_Q 0
#endif

namespace XRender
{

static bool g_is_working = false;

bool g_in_frame = false;
uint64_t s_last_frame_start = 0;

std::set<StdPicture*> s_texture_bank;
uint32_t s_loadedVRAM = 0;

struct tex_load_data
{
    Files::Data data;
    GL_TEXTURE_SIZE_ENUM w, last_h;
    GL_TEXTURE_TYPE_ENUM type;
    int name[3] = {};
    int params;

    inline const uint16_t* palette() const
    {
        if(!data.valid() || data.size() < 32)
            return nullptr;

        return reinterpret_cast<const uint16_t*>(data.begin());
    }

    inline GL_TEXTURE_SIZE_ENUM h(int i) const
    {
        bool is_last = (i == 2) || (name[i + 1] == 0);

        return (is_last) ? last_h : TEXTURE_SIZE_1024;
    }

    inline const uint8_t* pixels(int i) const
    {
        if(!data.valid())
            return nullptr;

        unsigned w_px = 1 << (3 + w);
        unsigned pixels_per_byte = (type == GL_RGB32_A3) ? 1 : 2;
        unsigned pixel_data_offset = 32 + i * w_px * 1024 / pixels_per_byte;

        if(data.size() < pixel_data_offset)
            return nullptr;

        return data.begin() + pixel_data_offset;
    }
};

static std::vector<tex_load_data> s_texture_load_queue;

static inline uint8_t s_fb_lookup(uint8_t p)
{
    return ((p & 3) == 3) ? 0x11 :
        (p & 2) ? 0x01 :
        (p & 1) ? 0x10 :
        0x00;
}

inline uint8_t* s_load_hardcoded_font()
{
    uint8_t* ret = (uint8_t*)calloc(32 + 1024 * 4, 1);

    if(!ret)
        return nullptr;

    ret[2] = 255;
    ret[3] = 255;

    auto i = c_hardcoded_font_bytes.cbegin();
    uint8_t* o = ret + 32;

    while(i != c_hardcoded_font_bytes.cend())
    {
        uint8_t row = *(i++);
        *(o++) = s_fb_lookup(row >> 6);
        *(o++) = s_fb_lookup(row >> 4);
        *(o++) = s_fb_lookup(row >> 2);
        *(o++) = s_fb_lookup(row >> 0);
    }

    return ret;
}

static int s_loadTextureToRAM(tex_load_data& tex, const std::string& path, int logical_w, int logical_h, int flags)
{
    int texture_w = logical_w >> (1 + (flags & 15));
    int texture_h = logical_h >> (1 + (flags & 15));

    int complete_texture_count = (texture_h - 1) / 1024;
    int last_texture_h = ((texture_h - 1) % 1024) + 1;

    // only load 3 textures at most
    if(texture_h > 3072)
    {
        complete_texture_count = 2;
        last_texture_h = 1024;
    }

    if(texture_w > 1024)
        return 0;
    else if(texture_w > 512)
        tex.w = TEXTURE_SIZE_1024;
    else if(texture_w > 256)
        tex.w = TEXTURE_SIZE_512;
    else if(texture_w > 128)
        tex.w = TEXTURE_SIZE_256;
    else if(texture_w > 64)
        tex.w = TEXTURE_SIZE_128;
    else if(texture_w > 32)
        tex.w = TEXTURE_SIZE_64;
    else if(texture_w > 16)
        tex.w = TEXTURE_SIZE_32;
    else if(texture_w > 8)
        tex.w = TEXTURE_SIZE_16;
    else
        tex.w = TEXTURE_SIZE_8;

    if(last_texture_h > 1024)
        return 0;
    else if(last_texture_h > 512)
        tex.last_h = TEXTURE_SIZE_1024;
    else if(last_texture_h > 256)
        tex.last_h = TEXTURE_SIZE_512;
    else if(last_texture_h > 128)
        tex.last_h = TEXTURE_SIZE_256;
    else if(last_texture_h > 64)
        tex.last_h = TEXTURE_SIZE_128;
    else if(last_texture_h > 32)
        tex.last_h = TEXTURE_SIZE_64;
    else if(last_texture_h > 16)
        tex.last_h = TEXTURE_SIZE_32;
    else if(last_texture_h > 8)
        tex.last_h = TEXTURE_SIZE_16;
    else
        tex.last_h = TEXTURE_SIZE_8;

    // calculate number of bytes desired
    // TEXTURE_SIZE_8 is an int with value 0
    uint16_t w_px = 1 << (3 + tex.w);
    uint16_t h_px = (1 << (3 + tex.last_h)) + (1024 * complete_texture_count);
    uint32_t data_size = w_px * h_px / 2;

    if(flags & 32)
        data_size *= 2;

    // don't even try to load a texture that would require >=25% of VRAM
    if(data_size > 131072)
    {
        pLogWarning("Refused to allocate %d bytes", data_size);
        return 0;
    }


    // check for hardcoded font
    const char* path_str = path.c_str();

    if(path_str[0] == '!' && path_str[1] == 'F' && path_str[2] == '\0')
    {
        uint8_t* data = s_load_hardcoded_font();

        if(data)
            tex.data.take_ownership_of_mem(data, 32 + 1024 * 4);
    }
    // load the file to RAM!
    else
        tex.data = Files::load_file(path);

    // this can go wrong if the file doesn't exist or if the allocation fails
    if(!tex.data.valid() || tex.data.size() != data_size + 32)
    {
        pLogWarning("Failed to load file %s (%d bytes, got %d)", path.c_str(), data_size + 32, (int)tex.data.size());
        return 0;
    }

    return complete_texture_count + 1;
}

static int s_loadTexture(const std::string& path, int* tex_out, int* data_size, int logical_w, int logical_h, int flags)
{
    if(!tex_out || !data_size)
    {
        pLogWarning("Got a null pointer to s_loadTexture");
        return false;
    }

    tex_load_data tex;

    int tex_count = s_loadTextureToRAM(tex, path, logical_w, logical_h, flags);

    if(tex_count == 0)
        return false;

    int loaded = 0;
    uint32_t loaded_data_size = 0;

    for(int i = 0; i < tex_count; i++)
    {
        if(!glGenTextures(1, &tex.name[i]))
        {
            pLogWarning("Could not generate texture");
            break;
        }

        uint16_t h_enum = (i == tex_count - 1) ? tex.last_h : TEXTURE_SIZE_1024;

        uint16_t w_px = 1 << (3 + tex.w);
        uint16_t h_px = 1 << (3 + h_enum);
        uint32_t part_data_size = w_px * h_px / 2;

        tex.params = TEXGEN_OFF;
        if((flags & 16) == 0)
            tex.params |= GL_TEXTURE_COLOR0_TRANSPARENT;

        tex.type = GL_RGB16;
        if(flags & 32)
            tex.type = GL_RGB32_A3;

        glBindTexture(0, tex.name[i]);

        // allocate, but do not load to VRAM yet
        if(!glTexImage2D(0, 0, tex.type, tex.w, h_enum, 0, tex.params, nullptr))
        {
            pLogWarning("Could not load texture (%u bytes) to VRAM (%u/524288 used). Requesting free texture memory.", part_data_size, s_loadedVRAM);
            minport_freeTextureMemory();

            if(!glTexImage2D(0, 0, tex.type, tex.w, h_enum, 0, tex.params, nullptr))
            {
                pLogWarning("Still could not load texture (%u bytes) to VRAM (%u/524288 used).", part_data_size, s_loadedVRAM);
                glDeleteTextures(1, &tex.name[i]);
                break;
            }
        }

        glColorTableEXT_alloc_only(0, 0, 16, 0, 0);

        tex_out[i] = tex.name[i];

        loaded++;
        loaded_data_size += part_data_size;
    }

    if(loaded != tex_count)
    {
        for(int i = 0; i < loaded; i++)
        {
            glDeleteTextures(1, &tex.name[i]);
            tex.name[i] = 0;
            tex_out[i] = 0;
        }

        return 0;
    }

    s_loadedVRAM += loaded_data_size;
    *data_size = loaded_data_size;

    s_texture_load_queue.push_back(std::move(tex));

    // pLogDebug("Loaded tex from %s!", path.c_str());

    return loaded;
}

static RenderPlanes_t s_render_planes;
// static uint8_t s_poly_id;

// sourced from gl2d
static inline void s_gxTexcoord2i(t16 u, t16 v)
{
    GFX_TEX_COORD = (v << 20) | ( (u << 4) & 0xFFFF );
}

static inline void s_gxVertex3i(v16 x, v16 y, v16 z)
{
    GFX_VERTEX16 = (y << 16) | (x & 0xFFFF);
    GFX_VERTEX16 = ((u32)(u16)z);
}

static inline void s_gxVertex2i(v16 x, v16 y)
{
    GFX_VERTEX_XY = (y << 16) | (x & 0xFFFF);
}

void minport_RenderBoxFilled( int x1, int y1, int x2, int y2, XTColor color)
{
    x2++;
    y2++;

    glBindTexture(0, 0);
    if((color.a >> 3) < 31)
        glPolyFmt(POLY_ID(s_render_planes.m_current_plane / 8) | POLY_ALPHA((color.a >> 3) + 1) | POLY_CULL_NONE | POLY_FOG_Q);
    glColor3b(color.r, color.g, color.b);

    glBegin( GL_QUADS );
        s_gxVertex3i( x1, y1, s_render_planes.next() );       // use 3i for first vertex so that we increment HW depth
        s_gxVertex2i( x1, y2 );                // no need for 3 vertices as 2i would share last depth call
        s_gxVertex2i( x2, y2 );
        s_gxVertex2i( x2, y1 );
    glEnd();

    for(int i = 0; i < 7; ++i)
        s_render_planes.next(); // actually advance 8 slots rather than 1, to handle some imprecision of the DSi depth buffer (unknown cause)

    if((color.a >> 3) < 31)
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG_Q);
}

static void s_glBoxFilledGradient( int x1, int y1, int x2, int y2,
                          int color1, int color2, int color3, int color4)
{

    x2++;
    y2++;

    glBindTexture( 0,0 );
    glBegin( GL_QUADS );
        glColor( color1 ); s_gxVertex3i( x1, y1, s_render_planes.next() );       // use 3i for first vertex so that we increment HW depth
        glColor( color2 ); s_gxVertex2i( x1, y2 );                // no need for 3 vertices as 2i would share last depth call
        glColor( color3 ); s_gxVertex2i( x2, y2 );
        glColor( color4 ); s_gxVertex2i( x2, y1 );
    glEnd();
}

inline bool GL_DrawImage_Custom(int name, int flags,
    int16_t x, int16_t y, uint16_t w, uint16_t h,
    uint16_t src_x, uint16_t src_y, uint16_t src_w, uint16_t src_h,
    unsigned int flip,
    XTColor color)
{
    uint16_t u1 = src_x >> (flags & 15);
    uint16_t u2 = (src_x + src_w) >> (flags & 15);
    uint16_t v1 = src_y >> (flags & 15);
    uint16_t v2 = (src_y + src_h) >> (flags & 15);

    if(flip & X_FLIP_HORIZONTAL)
        std::swap(u1, u2);
    if(flip & X_FLIP_VERTICAL)
        std::swap(v1, v2);

    uint8_t r = color.r;
    uint8_t g = color.g;
    uint8_t b = color.b;
    uint8_t a = color.a;

    // GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    // GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

    // pLogDebug("u1 u2 v1 v2: %u %u %u %u; x1 x2 y1 y2: %d %d %d %d", u1, u2, v1, v2, x, x + w, y, y + h);

    glBindTexture(0, name);
    if((a >> 3) < 31)
        glPolyFmt(/*POLY_ID(s_poly_id++) | */ POLY_ID(s_render_planes.m_current_plane / 8) | POLY_ALPHA((a >> 3) + 1) | POLY_CULL_NONE | POLY_FOG_Q);
    else if(flags & 32)
        glPolyFmt(POLY_ID(s_render_planes.m_current_plane / 8) | POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG_Q);

    glBegin(GL_QUADS);

    glColor3b(r, g, b);

    s_gxTexcoord2i(u1, v1);
    s_gxVertex3i(x, y, s_render_planes.next());
    for(int i = 0; i < 7; ++i)
        s_render_planes.next(); // actually advance 8 slots rather than 1, to handle some imprecision of the DSi depth buffer (unknown cause)

    s_gxTexcoord2i(u1, v2);
    s_gxVertex2i(x, y + h);

    s_gxTexcoord2i(u2, v2);
    s_gxVertex2i(x + w, y + h);

    s_gxTexcoord2i(u2, v1);
    s_gxVertex2i(x + w, y);

    glEnd();

    if((a >> 3) < 31 || flags & 32)
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG_Q);

    return true;
}

inline bool GL_DrawImage_Custom_Basic(int name, int flags,
    int16_t x, int16_t y, uint16_t w, uint16_t h,
    uint16_t src_x, uint16_t src_y,
    XTColor color)
{
    uint16_t u1 = src_x >> (flags & 15);
    uint16_t u2 = (src_x + w) >> (flags & 15);
    uint16_t v1 = src_y >> (flags & 15);
    uint16_t v2 = (src_y + h) >> (flags & 15);

    uint8_t r = color.r;
    uint8_t g = color.g;
    uint8_t b = color.b;
    uint8_t a = color.a;

    glBindTexture(0, name);
    if((a >> 3) < 31)
        glPolyFmt(/*POLY_ID(s_poly_id++) | */ POLY_ID(s_render_planes.m_current_plane / 8) | POLY_ALPHA((a >> 3) + 1) | POLY_CULL_NONE | POLY_FOG_Q);
    else if(flags & 32)
        glPolyFmt(POLY_ID(s_render_planes.m_current_plane / 8) | POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG_Q);

    glBegin(GL_QUADS);

    glColor3b(r, g, b);

    s_gxTexcoord2i(u1, v1);
    s_gxVertex3i(x, y, s_render_planes.next());
    for(int i = 0; i < 7; ++i)
        s_render_planes.next(); // actually advance 8 slots rather than 1, to handle some imprecision of the DSi depth buffer (unknown cause)

    s_gxTexcoord2i(u1, v2);
    s_gxVertex2i(x, y + h);

    s_gxTexcoord2i(u2, v2);
    s_gxVertex2i(x + w, y + h);

    s_gxTexcoord2i(u2, v1);
    s_gxVertex2i(x + w, y);

    glEnd();

    if((a >> 3) < 31 || flags & 32)
        glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG_Q);

    return true;
}

bool isWorking()
{
    return g_is_working;
}

bool hasFrameBuffer()
{
    return true;
}

bool init()
{
    lcdMainOnBottom();

    vramSetBankA(VRAM_A_TEXTURE);
    vramSetBankB(VRAM_B_TEXTURE);
    vramSetBankC(VRAM_C_TEXTURE);
    vramSetBankD(VRAM_D_TEXTURE);
    vramSetBankE(VRAM_E_TEX_PALETTE);
    vramSetBankF(VRAM_F_TEX_PALETTE_SLOT4);
    vramSetBankG(VRAM_G_TEX_PALETTE_SLOT5);

    videoSetMode(MODE_5_3D);

    glInit();

    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_POSITION);
    glLoadIdentity();

    glColor(0x7FFF);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_FOG_Q);

#ifdef DEBUG_PLANES
    glEnable(GL_FOG);

    glFogColor(255, 0, 0, 255);
    for(int i = 0; i < 32; i++)
        glFogDensity(i, 0);

    // visibly mark a draw plane for debugging
    glFogDensity(31 - PLANE_LVL_SBLOCK / 8, 127);

    glFogOffset(0);
    glFogShift(0);
#endif

    g_is_working = true;

    updateViewport();

    return true;
}

void quit()
{
    g_is_working = false;
}

bool ready_for_frame()
{
    // in unlimited framerate or frameskip modes, only start at most 60 frames per second (because the game will get limited to 60 frames per second in the hardware anyway)
    return (!g_config.unlimited_framerate && !g_config.enable_frameskip) || (SDL_GetMicroTicks() - s_last_frame_start) > 16722;
}

void setTargetTexture()
{
    if(g_in_frame)
    {
        printf("OOPS\n");
        return;
    }

    s_last_frame_start = SDL_GetMicroTicks();
    g_in_frame = true;

    minport_initFrame();

    s_render_planes.reset();
    // s_poly_id = 0;
    glClearColor(0, 0, 0, 31);

    minport_ApplyViewport();
}

void setTargetScreen()
{
}

void setDrawPlane(uint8_t plane)
{
    s_render_planes.set_plane(plane);
}

void clearBuffer()
{
    if(g_in_frame)
        return;

    setTargetTexture();
    repaint();
}

void repaint()
{
    if(!g_in_frame)
    {
        printf("OOPS2\n");
        return;
    }

    g_microStats.start_sleep();

    glFlush(0);

    if(g_config.render_vsync || !s_texture_load_queue.empty())
        swiWaitForVBlank();

    if(!s_texture_load_queue.empty())
    {
        for(const auto& tex : s_texture_load_queue)
        {
            for(int i = 0; i < 2; i++)
            {
                if(!tex.name[i])
                    break;

                glBindTexture(0, tex.name[i]);
                glColorSubTableEXT(0, 0, 16, 0, 0, tex.palette());
                glTexImage2D(0, 0, tex.type, tex.w, tex.h(i), 0, tex.params, tex.pixels(i));
            }
        }

        s_texture_load_queue.clear();
    }

    // Note that when vsync is disabled, the glFlush call will actually cause the geometry engine to wait for vblank on the next issued command. We can't track that timing easily.

    g_microStats.start_task(MicroStats::Graphics);

    g_in_frame = false;
}

void mapToScreen(int x, int y, int *dx, int *dy)
{
    *dx = (x - g_screen_phys_x) * XRender::TargetW / g_screen_phys_w;
    *dy = (y - g_screen_phys_y) * XRender::TargetH / g_screen_phys_h;
}

void mapFromScreen(int scr_x, int scr_y, int *window_x, int *window_y)
{
    *window_x = (scr_x * g_screen_phys_w / XRender::TargetW) + g_screen_phys_x;
    *window_y = (scr_y * g_screen_phys_h / XRender::TargetH) + g_screen_phys_y;
}

void minport_TransformPhysCoords()
{
    if(g_screen_phys_x < 0)
    {
        g_screen_phys_x = 0;
        g_screen_phys_w = 256;
    }

    if(g_screen_phys_y < 0)
    {
        g_screen_phys_y = 0;
        g_screen_phys_h = 192;
    }
}

void minport_ApplyPhysCoords()
{
    glViewport(g_screen_phys_x,
            g_screen_phys_y,
            g_screen_phys_x + g_screen_phys_w - 1,
            g_screen_phys_y + g_screen_phys_h - 1);

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glOrthof32( 0, XRender::TargetW / 2, XRender::TargetH / 2, 0, -0x7FFF, 0x7FFF );
}

void minport_ApplyViewport()
{
    int phys_offset_x = g_viewport_x * g_screen_phys_w * 2 / XRender::TargetW;
    int phys_width = g_viewport_w * g_screen_phys_w * 2 / XRender::TargetW;

    int phys_offset_y = g_viewport_y * g_screen_phys_h * 2 / XRender::TargetH;
    int phys_height = g_viewport_h * g_screen_phys_h * 2 / XRender::TargetH;

    if(g_screen_phys_x + phys_offset_x < 0)
        phys_offset_x = -g_screen_phys_x;
    if(g_screen_phys_y + phys_offset_y < 0)
        phys_offset_y = -g_screen_phys_y;

    if(g_screen_phys_x + phys_offset_x + phys_width >= 256)
        phys_width = 256 - (g_screen_phys_x + phys_offset_x);
    if(g_screen_phys_y + phys_offset_y + phys_height >= 192)
        phys_height = 192 - (g_screen_phys_y + phys_offset_y);

    glViewport(g_screen_phys_x + phys_offset_x,
            192 - (g_screen_phys_y + phys_offset_y + phys_height),
            g_screen_phys_x + phys_offset_x + phys_width - 1,
            192 - (g_screen_phys_y + phys_offset_y) - 1);

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();

    int off_x = g_viewport_offset_x;
    int off_y = g_viewport_offset_y;
    // if linear mode is set, consider doing flicker scaling. (DOESN'T WORK)
    // if((phys_width < g_viewport_w || phys_height < g_viewport_h) && (g_config.scale_mode == SCALE_DYNAMIC_LINEAR) && (s_current_frame & 1))
    // {
    //     // want to know which pixel in world coordinates is pixel 0.5 in screen coordinates
    //     int half_pixel_x = 0.5 * g_viewport_w / phys_width + 0.5;
    //     int half_pixel_y = 0.5 * g_viewport_h / phys_height + 0.5;

    //     off_x += 1;
    //     off_y += 1;
    // }
    glOrthof32( off_x, g_viewport_w + off_x, g_viewport_h + off_y, off_y, -0x7FFF, 0x7FFF );
}

void lazyLoadPicture(StdPicture_Sub& target, const std::string& path, int scaleFactor, const std::string& maskPath, const std::string& maskFallbackPath)
{
    (void)scaleFactor;
    (void)maskPath;
    (void)maskFallbackPath;

    if(!GameIsActive)
        return; // do nothing when game is closed

    target.inited = true;
    target.l.path = path;
    target.l.lazyLoaded = true;

    // We need to figure out the height and width!
    std::string sizePath = path + ".size";
    Files::Data fs = Files::load_file(sizePath);

    if(!fs.empty())
    {
        int w, h, flags;
        if(sscanf(fs.c_str(), "%d\n%d\n%d\n", &w, &h, &flags) != 3 || w < 0 || w > 8192 || h < 0 || h > 8192)
        {
            pLogWarning("Could not load image %s dimensions from size file", path.c_str());
            target.inited = false;
        }
        else
        {
            target.w = w;
            target.h = h;
            target.l.flags = flags;
        }
    }
    else
    {
        pLogWarning("loadPicture: Couldn't open size file.");
        target.inited = false;
    }
}

void lazyLoadPictureFromList(StdPicture_Sub& target, PGE_FileFormats_misc::TextInput& t, std::string& line_buf, const std::string& dir)
{
    if(!GameIsActive)
        return; // do nothing when game is closed

    t.readLine(line_buf);
    if(line_buf.empty())
    {
        pLogWarning("Could not load image path from load list");
        return;
    }

    target.inited = true;
    target.l.path = dir;
    target.l.path += std::move(line_buf);
    target.l.lazyLoaded = true;

    bool okay = false;

    int w, h, flags;
    t.readLine(line_buf);
    if(sscanf(line_buf.c_str(), "%d", &w) == 1)
    {
        t.readLine(line_buf);

        if(sscanf(line_buf.c_str(), "%d", &h) == 1)
        {
            t.readLine(line_buf);

            if(sscanf(line_buf.c_str(), "%d", &flags) == 1)
                okay = true;
        }
    }

    if(!okay || w < 0 || w > 8192 || h < 0 || h > 8192)
    {
        pLogWarning("Could not load image %s dimensions from load list", target.l.path);
        target.inited = false;
        return;
    }

    // pLogDebug("Successfully loaded %s (%d %d)", target.l.path.c_str(), w, h);

    target.w = w;
    target.h = h;
    target.l.flags = flags;

    return;
}

void lazyLoad(StdPicture &target)
{
    if(!target.inited || !target.l.lazyLoaded || target.d.hasTexture())
        return;

    target.d.attempted_load = true;
    s_texture_bank.insert(&target);

    std::string suppPath;

    if(!s_loadTexture(target.l.path, target.d.texture, &target.d.data_size, target.w, target.h, target.l.flags))
    {
        pLogWarning("Permanently failed to load %s", target.l.path.c_str());
        return;
    }

    return;
}

void lazyPreLoad(StdPicture &target)
{
    lazyLoad(target);
}

void clearAllTextures()
{
    for(StdPicture* tx : s_texture_bank)
    {
        s_loadedVRAM -= tx->d.data_size;

        minport_unlinkTexture(tx);
        tx->d.destroy();
    }

    s_texture_bank.clear();
    s_texture_load_queue.clear();

    if(s_loadedVRAM != 0)
        pLogWarning("VRAM use not 0 after clear (%u instead). At risk of use-after-free.", s_loadedVRAM);

    // useful to combat fragmentation
    s_loadedVRAM = 0;
    glResetTextures();
}

void unloadTexture(StdPicture &tx)
{
    if(!tx.inited)
        return;

    if(s_texture_bank.find(&tx) != s_texture_bank.end())
        s_texture_bank.erase(&tx);

    minport_unlinkTexture(&tx);

    if(tx.d.reallyHasTexture())
    {
        D_pLogDebug("Freeing %d bytes from %s", tx.d.data_size);
        s_loadedVRAM -= tx.d.data_size;
    }

    tx.d.destroy();

    if(tx.l.path.empty())
        static_cast<StdPicture_Sub&>(tx) = StdPicture_Sub();
}

void minport_RenderTexturePrivate(int16_t xDst, int16_t yDst, int16_t wDst, int16_t hDst,
                             StdPicture &tx,
                             int16_t xSrc, int16_t ySrc, int16_t wSrc, int16_t hSrc,
                             float rotateAngle, FPoint_t *center, unsigned int flip,
                             XTColor color)
{
    if(!tx.inited)
        return;

    if(tx.l.lazyLoaded && !tx.d.hasTexture() && !tx.d.reallyHasTexture())
        lazyLoad(tx);

    // handle rotation NOW
    if(rotateAngle)
    {
        int16_t cx, cy;
        glPushMatrix();

        if(center)
        {
            cx = center->x / 2.0f + 0.5f;
            cy = center->y / 2.0f + 0.5f;
        }
        else
        {
            cx = wDst / 2;
            cy = hDst / 2;
        }

        glTranslatef32(xDst + cx, yDst + cy, 0);
        glRotateZ(rotateAngle);

        xDst = -cx;
        yDst = -cy;
    }

    if(!tx.d.reallyHasTexture())
    {
        s_glBoxFilledGradient( xDst, yDst, xDst + wDst - 1, yDst + hDst - 1,
                             RGB15( 15, 2, 1 ),
                             RGB15( 1, 15, 1 ),
                             RGB15( 1, 2, 15 ),
                             RGB15( 1, 15, 15 )
                           );

        return;
    }

    int to_draw = 0;
    int to_draw_2 = 0;

    int tex_part_height = 1024 << (tx.l.flags & 15);

    if(ySrc + hSrc > tex_part_height)
    {
        if(ySrc + hSrc > tex_part_height * 2)
        {
            if(tx.d.texture[2])
                to_draw = tx.d.texture[2];
            if(ySrc < tex_part_height * 2 && tx.d.texture[1])
                to_draw_2 = tx.d.texture[1];

            ySrc -= tex_part_height;
        }
        else
        {
            if(tx.d.texture[1])
                to_draw = tx.d.texture[1];
            if(ySrc < tex_part_height)
                to_draw_2 = tx.d.texture[0];
        }

        // draw the top pic
        if(to_draw_2)
        {
            GL_DrawImage_Custom(to_draw_2, tx.l.flags, xDst, yDst, wDst, (tex_part_height - ySrc) * hDst / hSrc,
                                xSrc, ySrc, wSrc, tex_part_height - ySrc, flip, color);
            yDst += (tex_part_height - ySrc) * hDst / hSrc;
            hDst -= (tex_part_height - ySrc) * hDst / hSrc;
            hSrc -= (tex_part_height - ySrc);
            ySrc = 0;
        }
        else
            ySrc -= tex_part_height;
    }
    else
    {
        to_draw = tx.d.texture[0];
    }

    if(!to_draw) return;

    GL_DrawImage_Custom(to_draw, tx.l.flags, xDst, yDst, wDst, hDst,
                        xSrc, ySrc, wSrc, hSrc, flip, color);

    // finalize rotation HERE
    if(rotateAngle)
        glPopMatrix(1);
}

void minport_RenderTexturePrivate_Basic(int16_t xDst, int16_t yDst, int16_t wDst, int16_t hDst,
                             StdPicture &tx,
                             int16_t xSrc, int16_t ySrc,
                             XTColor color)
{
    if(!tx.inited)
        return;

    if(tx.l.lazyLoaded && !tx.d.hasTexture() && !tx.d.reallyHasTexture())
        lazyLoad(tx);

    if(!tx.d.reallyHasTexture())
        return;

    if(tx.d.texture[1])
    {
        return minport_RenderTexturePrivate(xDst, yDst, wDst, hDst,
                             tx,
                             xSrc, ySrc, wDst, hDst,
                             0, nullptr, 0,
                             color);
    }

    int to_draw = tx.d.texture[0];

    if(!to_draw) return;

    GL_DrawImage_Custom_Basic(to_draw, tx.l.flags, xDst, yDst, wDst, hDst, xSrc, ySrc, color);
}

}; // namespace XRender
