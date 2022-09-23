﻿/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2020 Vitaly Novichkov <admin@wohlnet.ru>
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


#include <malloc.h>

#include <gccore.h>

#include <set>

#include <Logger/logger.h>

#include "globals.h"
#include "video.h"
#include "frame_timer.h"

#include "core/window.h"

#include "core/render.h"
#include "core/minport/render_minport_shared.h"


namespace XRender
{

bool g_in_frame = false;

#define DEFAULT_FIFO_SIZE   (256*1024)

void *frameBuffer[2] = { NULL, NULL};
int cur_buffer = 0;
GXRModeObj *rmode = nullptr;
Mtx view;

int s_num_textures_loaded = 0;

int g_rmode_w = 640;
int g_rmode_h = 480;

int robust_TPL_GetTexture(TPLFile* file, int i, GXTexObj* gxtex)
{
    int ret = TPL_GetTexture(file, i, gxtex);
    if(ret == 0 || errno != ENOMEM)
        return ret;

    pLogWarning("Failed to load due to lack of memory");
    minport_freeTextureMemory();

    return TPL_GetTexture(file, i, gxtex);
}

void s_loadTexture(StdPicture &target, int i)
{
    if(target.d.texture_file_init[i])
    {
        u32 info;
        TPL_GetTextureInfo(&target.d.texture_file[i], 0, &info, &target.d.tex_w[i], &target.d.tex_h[i]);

        if(!target.w)
        {
            target.w = target.d.tex_w[i]*2;
            target.h = target.d.tex_h[i]*2;
        }

        if(robust_TPL_GetTexture(&target.d.texture_file[i], 0, &target.d.texture[i]) != 0)
        {
            TPL_CloseTPLFile(&target.d.texture_file[i]);
            target.d.texture_file_init[i] = false;
        }
        else
        {
            GX_InitTexObjFilterMode(&target.d.texture[i], GX_NEAR, GX_NEAR);
            GX_InitTexObjWrapMode(&target.d.texture[i], GX_CLAMP, GX_CLAMP);
            target.d.texture_init[i] = true;
        }
    }
}

void gxDrawDoneCB()
{
    cur_buffer ^= 1;
}

void videoPreTraceCB(u32 /*retraceCnt*/)
{
    VIDEO_SetNextFramebuffer(frameBuffer[cur_buffer]);
    VIDEO_Flush();
}

bool init()
{
    f32 yscale;
    u32 xfbHeight;

    GXColor background = {0, 0, 0, 0xff};

    // init the vi.
    VIDEO_Init();

    rmode = VIDEO_GetPreferredMode(NULL);

    // allocate 2 framebuffers for double buffering
    frameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    frameBuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(frameBuffer[cur_buffer]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
    VIDEO_SetPreRetraceCallback(videoPreTraceCB);

    // setup the fifo and then init the flipper
    void *gp_fifo = NULL;
    gp_fifo = memalign(32,DEFAULT_FIFO_SIZE);
    memset(gp_fifo,0,DEFAULT_FIFO_SIZE);

    GX_Init(gp_fifo,DEFAULT_FIFO_SIZE);

    // clears the bg to color and clears the z buffer
    GX_SetCopyClear(background, 0x00ffffff);
    GX_SetDrawDoneCallback(gxDrawDoneCB);

    // other gx setup
    GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
    yscale = GX_GetYScaleFactor(rmode->efbHeight,rmode->xfbHeight);
    xfbHeight = GX_SetDispCopyYScale(yscale);
    GX_SetScissor(0,0,rmode->fbWidth,rmode->efbHeight);
    GX_SetDispCopySrc(0,0,rmode->fbWidth,rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth,xfbHeight);
    GX_SetCopyFilter(rmode->aa,rmode->sample_pattern,GX_TRUE,rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering,((rmode->viHeight==2*rmode->xfbHeight)?GX_ENABLE:GX_DISABLE));
    GX_SetColorUpdate(1);
    GX_SetAlphaUpdate(1);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);


    printf("%d %d\n", (int)rmode->viWidth, (int)rmode->viHeight);

    g_rmode_w = rmode->fbWidth;
    g_rmode_h = rmode->efbHeight;

    GX_SetCullMode(GX_CULL_NONE);
    GX_CopyDisp(frameBuffer[cur_buffer],GX_TRUE);
    GX_SetDispCopyGamma(GX_GM_1_0);


    // setup the vertex descriptor
    // tells the flipper to expect direct data
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

    // setup the vertex attribute table
    // describes the data
    // args: vat location 0-7, type of data, data format, size, scale
    // so for ex. in the first call we are sending position data with
    // 3 values X,Y,Z of size F32. scale sets the number of fractional
    // bits for non float data.
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_U16, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

    GX_SetNumChans(1);
    GX_SetNumTexGens(1);

    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 1, 1);

    GX_InvVtxCache();
    GX_InvalidateTexAll();

    // setup our camera at the origin
    // looking down the -z axis with y up
    guVector cam = {0.0F, 0.0F, 0.0F},
            up = {0.0F, 1.0F, 0.0F},
          look = {0.0F, 0.0F, -1.0F};
    guLookAt(view, &cam, &up, &look);

    updateViewport();

    return true;
}

void quit()
{
}

void setTargetTexture()
{
    if(g_in_frame)
        return;

    // do this before drawing
    GX_SetViewport(g_screen_phys_x, g_screen_phys_y, g_screen_phys_w, g_screen_phys_h, 0, 1);
    // load the view matrix into matrix memory
    GX_LoadPosMtxImm(view, GX_PNMTX0);

    g_in_frame = true;

    struct mallinfo info = mallinfo();

    if(info.uordblks > 292000000)
    {
        pLogWarning("Memory low, triggering free texture memory");
        minport_freeTextureMemory();
    }

    minport_initFrame();
}

void setTargetScreen()
{
}

void clearBuffer()
{
    if(!g_in_frame)
    {
        setTargetTexture();
        resetViewport();
        renderRect(0, 0, ScreenW, ScreenH, 0, 0, 0);
        repaint();
    }
}

void repaint()
{
    if(!g_in_frame)
        return;

    // do this stuff after drawing
    int next_buffer = cur_buffer ^ 1;
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetColorUpdate(GX_TRUE);
    GX_CopyDisp(frameBuffer[next_buffer],GX_TRUE);
    GX_DrawDone();

    GX_Flush();

    g_microStats.start_sleep();

    // video settings: VSync
    #if 0
    if(g_videoSettings.scaleMode == SCALE_DYNAMIC_LINEAR)
    {
        VIDEO_WaitVSync();
        if(rmode->viTVMode & VI_NON_INTERLACE)
            VIDEO_WaitVSync();
    }
    #endif
    g_microStats.start_task(MicroStats::Graphics);

    g_in_frame = false;
}

void mapToScreen(int x, int y, int *dx, int *dy)
{
    *dx = (x - g_screen_phys_x) * ScreenW / g_screen_phys_w;
    *dy = (y - g_screen_phys_y) * ScreenH / g_screen_phys_h;
}

void mapFromScreen(int scr_x, int scr_y, int *window_x, int *window_y)
{
    *window_x = (scr_x * g_screen_phys_w / ScreenW) + g_screen_phys_x;
    *window_y = (scr_y * g_screen_phys_h / ScreenH) + g_screen_phys_y;
}

void minport_TransformPhysCoords()
{
    int hardware_w, hardware_h;
    XWindow::getWindowSize(&hardware_w, &hardware_h);

    if(false) // widescreen_stretch
    {
        g_screen_phys_h = g_screen_phys_h * g_rmode_h / hardware_h;
        if(g_screen_phys_h > g_rmode_h)
        {
            g_screen_phys_w = g_screen_phys_w * g_screen_phys_h / g_rmode_h;
            g_screen_phys_h = g_rmode_h;
        }
        if(g_screen_phys_w > g_rmode_w)
        {
            g_screen_phys_w = g_rmode_w;
            g_screen_phys_h = g_screen_phys_h * g_screen_phys_w / g_rmode_w;
        }

        pLogDebug("Phys screen stretched to %d x %d", g_screen_phys_w, g_screen_phys_h);
    }

    g_screen_phys_x = g_rmode_w / 2 - g_screen_phys_w / 2;
    g_screen_phys_y = g_rmode_h / 2 - g_screen_phys_h / 2;

    if(g_screen_phys_x < 0)
        g_screen_phys_x = 0;
    if(g_screen_phys_y < 0)
        g_screen_phys_y = 0;
}

void minport_ApplyPhysCoords()
{
    GXColor background = {0, 0, 0, 0xff};
    GX_SetCopyClear(background, 0x00ffffff);

    // setup our projection matrix
    GX_SetViewport(g_screen_phys_x, g_screen_phys_y, g_screen_phys_w, g_screen_phys_h, 0, 1);
    GX_SetScissor(g_screen_phys_x, g_screen_phys_y, g_screen_phys_w, g_screen_phys_h);

    Mtx44 perspective;
    guOrtho(perspective, 0.0f, ScreenH / 2, 0.0f, ScreenW / 2, -1.0f, 1.0f);
    GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);
}

void minport_ApplyViewport()
{
    int phys_offset_x = g_viewport_x * g_screen_phys_w * 2 / ScreenW;
    int phys_width = g_viewport_w * g_screen_phys_w * 2 / ScreenW;

    int phys_offset_y = g_viewport_y * g_screen_phys_h * 2 / ScreenH;
    int phys_height = g_viewport_h * g_screen_phys_h * 2 / ScreenH;

    if(g_screen_phys_x + phys_offset_x < 0)
        phys_offset_x = -g_screen_phys_x;
    if(g_screen_phys_y + phys_offset_y < 0)
        phys_offset_y = -g_screen_phys_y;

    GX_SetViewport(g_screen_phys_x + phys_offset_x, g_screen_phys_y + phys_offset_y, phys_width, phys_height, 0, 1);
    GX_SetScissor(g_screen_phys_x + phys_offset_x, g_screen_phys_y + phys_offset_y, phys_width, phys_height);

    Mtx44 perspective;

    if(g_viewport_offset_ignore)
        guOrtho(perspective, 0, g_viewport_h, 0, g_viewport_w, -1.0f, 1.0f);
    else
        guOrtho(perspective,
            g_viewport_offset_y, g_viewport_h + g_viewport_offset_y,
            g_viewport_offset_x, g_viewport_w + g_viewport_offset_x,
            -1.0f, 1.0f);

    GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

    // int ox = g_viewport_x + g_viewport_offset_x;
    // int oy = g_viewport_y + g_viewport_offset_y;

    // ox &= ~1;
    // oy &= ~1;

    // GX_SetScissorBoxOffset(-ox, -oy);
}

StdPicture LoadPicture(const std::string& path, const std::string& maskPath, const std::string& maskFallbackPath)
{
    (void)maskPath;
    (void)maskFallbackPath;

    StdPicture target;

    if(!GameIsActive)
        return target; // do nothing when game is closed

    target.inited = false;
    target.l.path = path;
    if(target.l.path.empty())
        return target;

    target.inited = true;
    target.l.lazyLoaded = false;

    if(TPL_OpenTPLFromFile(&target.d.texture_file[0], target.l.path.c_str()) == 1)
    {
        target.d.texture_file_init[0] = true;
        s_loadTexture(target, 0);
        s_num_textures_loaded ++;
    }

    if(!target.d.hasTexture())
    {
        pLogWarning("FAILED TO LOAD!!! %s\n", path.c_str());
        target.d.destroy();
        target.inited = false;
    }

    return target;
}

StdPicture lazyLoadPictureFromList(FILE* f, const std::string& dir)
{
    StdPicture target;
    if(!GameIsActive)
        return target; // do nothing when game is closed

    int length;

    char filename[256];
    if(fscanf(f, "%255[^\n]%n%*[^\n]\n", filename, &length) != 1)
    {
        pLogWarning("Could not load image path from load list");
        return target;
    }

    if(length == 255)
    {
        pLogWarning("Image path %s was truncated in load list", filename);
        return target;
    }

    target.inited = true;
    target.l.path = dir;
    target.l.path += filename;
    target.l.lazyLoaded = true;

    int w, h;
    if(fscanf(f, "%d\n%d\n", &w, &h) != 2 || w < 0 || w > 8192 || h < 0 || h > 8192)
    {
        pLogWarning("Could not load image %s dimensions from load list", filename);
        target.inited = false;
        return target;
    }

    // pLogDebug("Successfully loaded %s (%d %d)", target.l.path.c_str(), w, h);

    target.w = w;
    target.h = h;

    return target;
}

StdPicture lazyLoadPicture(const std::string& path, const std::string& maskPath, const std::string& maskFallbackPath)
{
    (void)maskPath;
    (void)maskFallbackPath;

    StdPicture target;
    if(!GameIsActive)
        return target; // do nothing when game is closed

    target.inited = false;
    target.l.path = path;
    if(target.l.path.empty())
        return target;

    target.inited = true;

    target.l.lazyLoaded = true;

    // We need to figure out the height and width!
    std::string sizePath = path + ".size";
    FILE* fs = fopen(sizePath.c_str(), "r");

    // NOT null-terminated: wwww\nhhhh\n
    char contents[10];

    if(fs != nullptr)
    {
        fread(&contents[0], 1, 10, fs);
        contents[4] = '\0';
        contents[9] = '\0';
        target.w = atoi(&contents[0]);
        target.h = atoi(&contents[5]);
        if(fclose(fs))
            pLogWarning("lazyLoadPicture: Couldn't close file.");
    }
    // lazy load and unload to read dimensions if it doesn't exist.
    // unload is essential because lazy load would save the address incorrectly.
    else
    {
        pLogWarning("lazyLoadPicture: Couldn't open size file.");
        lazyLoad(target);
        lazyUnLoad(target);
    }

    return target;
}

int robust_OpenTPLFromFile(TPLFile* target, const char* path)
{
    int ret = TPL_OpenTPLFromFile(target, path);
    if(ret == 1 || errno != ENOMEM)
        return ret;

    pLogWarning("Failed to load %s due to lack of memory", path);
    minport_freeTextureMemory();

    return TPL_OpenTPLFromFile(target, path);
}

void lazyLoad(StdPicture &target)
{
    if(!target.inited || !target.l.lazyLoaded || target.d.hasTexture())
        return;

    std::string suppPath;

    target.inited = true;

    if(robust_OpenTPLFromFile(&target.d.texture_file[0], target.l.path.c_str()) != 1)
    {
        pLogWarning("Permanently failed to load %s", target.l.path.c_str());
        pLogWarning("Error: %d (%s)", errno, strerror(errno));
        target.inited = false;
        return;
    }

    target.d.texture_file_init[0] = true;
    s_loadTexture(target, 0);

    if(!target.d.hasTexture())
    {
        pLogWarning("Permanently failed to load %s", target.l.path.c_str());
        pLogWarning("Error: %d (%s)", errno, strerror(errno));
        target.d.destroy();
        target.inited = false;
        return;
    }

    if(target.h > 2048)
    {
        suppPath = target.l.path + '1';
        if(robust_OpenTPLFromFile(&target.d.texture_file[1], suppPath.c_str()) != 1)
        {
            pLogWarning("Permanently failed to load %s", suppPath.c_str());
            pLogWarning("Error: %d (%s)", errno, strerror(errno));
        }
        else
        {
            target.d.texture_file_init[1] = true;
            s_loadTexture(target, 1);
        }
    }
    if(target.h > 4096)
    {
        suppPath = target.l.path + '2';
        if(robust_OpenTPLFromFile(&target.d.texture_file[2], suppPath.c_str()) != 1)
        {
            pLogWarning("Permanently failed to load %s", suppPath.c_str());
            pLogWarning("Error: %d (%s)", errno, strerror(errno));
        }
        else
        {
            target.d.texture_file_init[2] = true;
            s_loadTexture(target, 2);
        }
    }

    s_num_textures_loaded++;
}

void lazyPreLoad(StdPicture &target)
{
    lazyLoad(target);
}

void lazyUnLoad(StdPicture &target)
{
    if(!target.inited || !target.l.lazyLoaded || !target.d.hasTexture())
        return;

    deleteTexture(target, true);
}

void deleteTexture(StdPicture &tx, bool lazyUnload)
{
    if(!tx.inited)
        return;

    minport_unlinkTexture(&tx);

    if(tx.d.hasTexture())
        s_num_textures_loaded--;

    tx.d.destroy();

    if(!lazyUnload)
    {
        tx.inited = false;
        tx.l.lazyLoaded = false;
        tx.w = 0;
        tx.h = 0;
        tx.frame_w = 0;
        tx.frame_h = 0;
    }
}

inline int ROUNDDIV2(int x)
{
    return (x<0)?(x - 1) / 2:x / 2;
}

inline float ROUNDDIV2(float x)
{
    return std::nearbyintf(std::roundf(x) / 2.0f);
}

inline float ROUNDDIV2(double x)
{
    return std::nearbyintf(std::roundf((float)x / 2.0f));
}

inline float FLOORDIV2(float x)
{
    return std::floor(x / 2.0f);
}

void wii_RenderBox(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool filled)
{
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);

    GX_Begin(filled ? GX_QUADS : GX_LINESTRIP, GX_VTXFMT0, filled ? 4 : 5);
        GX_Position3s16(x1, y1, 0);
        GX_Color4u8(r, g, b, a);
        GX_TexCoord2u16(0, 0);

        GX_Position3s16(x2, y1, 0);
        GX_Color4u8(r, g, b, a);
        GX_TexCoord2u16(0, 0);

        GX_Position3s16(x2, y2, 0);
        GX_Color4u8(r, g, b, a);
        GX_TexCoord2u16(0, 0);

        GX_Position3s16(x1, y2, 0);
        GX_Color4u8(r, g, b, a);
        GX_TexCoord2u16(0, 0);

        // complete the rect
        if(!filled)
        {
            GX_Position3s16(x1, y1, 0);
            GX_Color4u8(r, g, b, a);
            GX_TexCoord2u16(0, 0);
        }
    GX_End();
}

void minport_RenderBoxFilled(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    wii_RenderBox(x1, y1, x2, y2, r, g, b, a, true);
}

void minport_RenderBoxUnfilled(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    wii_RenderBox(x1, y1, x2, y2, r, g, b, a, false);
}

inline bool GX_DrawImage_Custom(GXTexObj* img,
    int16_t x, int16_t y, uint16_t w, uint16_t h,
    uint16_t src_x, uint16_t src_y, uint16_t src_w, uint16_t src_h,
    unsigned int flip,
    float _r, float _g, float _b, float _a)
{
    uint16_t u1 = src_x;
    uint16_t u2 = src_x + src_w;
    uint16_t v1 = src_y;
    uint16_t v2 = src_y + src_h;

    if(flip & X_FLIP_HORIZONTAL)
        std::swap(u1, u2);
    if(flip & X_FLIP_VERTICAL)
        std::swap(v1, v2);

    uint8_t r = _r * 255.0f + 0.5f;
    uint8_t g = _g * 255.0f + 0.5f;
    uint8_t b = _b * 255.0f + 0.5f;
    uint8_t a = _a * 255.0f + 0.5f;

    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_LoadTexObj(img, GX_TEXMAP0);

    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
        GX_Position3s16(x, y, 0);
        GX_Color4u8(r, g, b, a);
        GX_TexCoord2u16(u1, v1);

        GX_Position3s16(x + w, y, 0);
        GX_Color4u8(r, g, b, a);
        GX_TexCoord2u16(u2, v1);

        GX_Position3s16(x + w, y + h, 0);
        GX_Color4u8(r, g, b, a);
        GX_TexCoord2u16(u2, v2);

        GX_Position3s16(x, y + h, 0);
        GX_Color4u8(r, g, b, a);
        GX_TexCoord2u16(u1, v2);
    GX_End();

    return true;
}

inline bool GX_DrawImage_Custom_Rotated(GXTexObj* img,
    float x, float y, float w, float h,
    float src_x, float src_y, float src_w, float src_h,
    unsigned int flip, FPoint_t *center, float angle,
    float r, float g, float b, float a)
{
    Mtx rotated;

    float cx, cy;
    if(center)
    {
        cx = center->x / 2.0f;
        cy = center->y / 2.0f;
    }
    else
    {
        cx = w / 2.0f;
        cy = h / 2.0f;
    }

    for(int r = 0; r < 3; r++)
    {
        for(int c = 0; c < 4; c++)
            rotated[r][c] = view[r][c];
    }

    guMtxRotDeg(rotated, 'z', angle);
    guMtxTransApply(rotated, rotated, x + cx, y + cy, 0.0f);

    GX_LoadPosMtxImm(rotated, GX_PNMTX0);

    GX_DrawImage_Custom(img,
        -cx, -cy, w, h,
        src_x, src_y, src_w, src_h,
        flip,
        r, g, b, a);

    GX_LoadPosMtxImm(view, GX_PNMTX0);

    return true;
}

void minport_RenderTexturePrivate(int16_t xDst, int16_t yDst, int16_t wDst, int16_t hDst,
                             StdPicture &tx,
                             int16_t xSrc, int16_t ySrc, int16_t wSrc, int16_t hSrc,
                             float rotateAngle, FPoint_t *center, unsigned int flip,
                             float red, float green, float blue, float alpha)
{
    if(!tx.inited)
        return;

    if(!tx.d.hasTexture() && tx.l.lazyLoaded)
        lazyLoad(tx);

    if(!tx.d.hasTexture())
        return;

    // automatic flipping based on SMBX style!
    unsigned int mode = 0;
    while(ySrc >= tx.h / 2 && mode < 3)
    {
        ySrc -= tx.h / 2;
        mode += 1;
    }
    flip ^= mode;

    GXTexObj* to_draw = nullptr;
    GXTexObj* to_draw_2 = nullptr;

    if(ySrc + hSrc > 1024)
    {
        if(ySrc + hSrc > 2048)
        {
            if(tx.d.texture_init[2])
                to_draw = &tx.d.texture[2];
            if(ySrc < 2048 && tx.d.texture_init[1])
                to_draw_2 = &tx.d.texture[1];
            ySrc -= 1024;
        }
        else
        {
            if(tx.d.texture_init[1])
                to_draw = &tx.d.texture[1];
            if(ySrc < 1024)
                to_draw_2 = &tx.d.texture[0];
        }
        // draw the top pic
        if(to_draw_2 != nullptr)
        {
            if(rotateAngle != 0.0)
            {
                // TODO: use correct center to support big textures being rotated
                GX_DrawImage_Custom_Rotated(to_draw_2, xDst, yDst, wDst, (1024 - ySrc) * hDst / hSrc,
                                    xSrc, ySrc, wSrc, 1024 - ySrc, flip, center, rotateAngle, red, green, blue, alpha);
            }
            else
                GX_DrawImage_Custom(to_draw_2, xDst, yDst, wDst, (1024 - ySrc) * hDst / hSrc,
                                    xSrc, ySrc, wSrc, 1024 - ySrc, flip, red, green, blue, alpha);
            yDst += (1024 - ySrc) * hDst / hSrc;
            hDst -= (1024 - ySrc) * hDst / hSrc;
            hSrc -= (1024 - ySrc);
            ySrc = 0;
        }
        else
            ySrc -= 1024;
    }
    else
    {
        to_draw = &tx.d.texture[0];
    }

    if(to_draw == nullptr) return;

    if(rotateAngle != 0.0)
        GX_DrawImage_Custom_Rotated(to_draw, xDst, yDst, wDst, hDst,
                             xSrc, ySrc, wSrc, hSrc, flip, center, rotateAngle, red, green, blue, alpha);
    else
        GX_DrawImage_Custom(to_draw, xDst, yDst, wDst, hDst,
                            xSrc, ySrc, wSrc, hSrc, flip, red, green, blue, alpha);
}

}; // namespace XRender
