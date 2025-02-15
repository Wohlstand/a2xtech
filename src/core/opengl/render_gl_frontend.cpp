/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2025 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <SDL2/SDL_events.h>
#endif

#include "core/opengl/gl_inc.h"

#include "core/opengl/render_gl.h"
#include "core/opengl/gl_program_object.h"
#include "core/opengl/gl_shader_translator.h"

#include <FreeImageLite.h>
#include <Graphics/graphics_funcs.h>

#include <fmt_format_ne.h>

#include "sdl_proxy/sdl_assert.h"

#include "core/window.h"
#include "core/render.h"

#include "main/cheat_code.h"

#include "globals.h"
#include "sound.h"
#include "config.h"

#include "graphics.h"
#include "controls.h"
#include "config.h"


#ifdef THEXTECH_WIP_FEATURES
#define LIGHTING_DEMO

#include "npc_id.h"
#include "eff_id.h"
#endif

#ifdef MUTABLE_PARTICLES_DEMO
#include "core/opengl/gl_program_bank.h"
StdPicture* s_sparkle = nullptr;
#endif


#ifndef UNUSED
#define UNUSED(x) (void)x
#endif

#define F_TO_B(color) {color.r, color.g, color.b, color.a}


RenderGL::RenderGL() :
    AbstractRender_t()
{}

RenderGL::~RenderGL()
{
    if(m_window)
        RenderGL::close();
}

unsigned int RenderGL::SDL_InitFlags()
{
    return SDL_WINDOW_OPENGL;
}

bool RenderGL::isWorking()
{
    return m_gContext;
}

bool RenderGL::hasFrameBuffer()
{
    return m_has_fbo;
}

RenderGL::VertexList& RenderGL::getOrderedDrawVertexList(RenderGL::DrawContext_t context, int depth)
{
    // combine draw calls issued in sequence (except bitmask calls)
    if(context == m_recent_draw_context && (!context.texture || !context.texture->d.mask_texture_id))
        return m_ordered_draw_queue[{m_recent_draw_context_depth, context}];

    // optimization for buffer-read shaders: only use a single depth per frame
    bool accel_mask = !m_use_logicop;
    bool is_mask = context.program == &m_standard_program && context.texture && context.texture->d.mask_texture_id;

    if((context.program && (context.program->get_flags() & GLProgramObject::read_buffer)) || (accel_mask && is_mask))
    {
        int& saved_context_depth = m_mask_draw_context_depth[context];

        // if new, store current depth for all draws of the mask
        if(saved_context_depth == 0)
            saved_context_depth = depth;
        // if old, use stored depth
        else
            depth = saved_context_depth;
    }

    m_recent_draw_context = context;
    m_recent_draw_context_depth = depth;

    return m_ordered_draw_queue[{depth, context}];
}

// Adds vertices to a VertexList
inline void RenderGL::addVertices(VertexList& list, const RectI& loc, const RectF& tc, GLshort depth, const Vertex_t::Tint& tint)
{
    const GLshort x1 = loc.tl.x;
    const GLshort x2 = loc.br.x;
    const GLshort y1 = loc.tl.y;
    const GLshort y2 = loc.br.y;

    const GLfloat u1 = tc.tl.x;
    const GLfloat u2 = tc.br.x;
    const GLfloat v1 = tc.tl.y;
    const GLfloat v2 = tc.br.y;

    list.vertices.push_back({{x1, y1, depth}, tint, {u1, v1}});
    list.vertices.push_back({{x1, y2, depth}, tint, {u1, v2}});
    list.vertices.push_back({{x2, y1, depth}, tint, {u2, v1}});
    list.vertices.push_back({{x1, y2, depth}, tint, {u1, v2}});
    list.vertices.push_back({{x2, y1, depth}, tint, {u2, v1}});
    list.vertices.push_back({{x2, y2, depth}, tint, {u2, v2}});
}

inline void RenderGL::addVertices(VertexList& list, const QuadI& loc, const RectF& tc, GLshort depth, const Vertex_t::Tint& tint)
{
    const GLfloat u1 = tc.tl.x;
    const GLfloat u2 = tc.br.x;
    const GLfloat v1 = tc.tl.y;
    const GLfloat v2 = tc.br.y;

    list.vertices.push_back({{loc.tl.x, loc.tl.y, depth}, tint, {u1, v1}});
    list.vertices.push_back({{loc.bl.x, loc.bl.y, depth}, tint, {u1, v2}});
    list.vertices.push_back({{loc.tr.x, loc.tr.y, depth}, tint, {u2, v1}});
    list.vertices.push_back({{loc.bl.x, loc.bl.y, depth}, tint, {u1, v2}});
    list.vertices.push_back({{loc.tr.x, loc.tr.y, depth}, tint, {u2, v1}});
    list.vertices.push_back({{loc.br.x, loc.br.y, depth}, tint, {u2, v2}});
}

// simple helper function to make a triangle strip for a single-quad draw
std::array<RenderGL::Vertex_t, 4> RenderGL::genTriangleStrip(const RectI& loc, const RectF& tc, GLshort depth, const Vertex_t::Tint& tint)
{
    const GLshort x1 = loc.tl.x;
    const GLshort x2 = loc.br.x;
    const GLshort y1 = loc.tl.y;
    const GLshort y2 = loc.br.y;

    const GLfloat u1 = tc.tl.x;
    const GLfloat u2 = tc.br.x;
    const GLfloat v1 = tc.tl.y;
    const GLfloat v2 = tc.br.y;

    return {
        Vertex_t{{x1, y1, depth}, tint, {u1, v1}},
        Vertex_t{{x1, y2, depth}, tint, {u1, v2}},
        Vertex_t{{x2, y1, depth}, tint, {u2, v1}},
        Vertex_t{{x2, y2, depth}, tint, {u2, v2}},
    };
}

void RenderGL::addLights(const GLPictureLightInfo& light_info, const QuadI& loc, const RectF& texcoord, GLshort depth)
{
#ifdef THEXTECH_BUILD_GL_MODERN
    if(!m_lighting_calc_program.inited() || m_light_count >= (int)m_light_queue.lights.size())
        return;

    const GLPictureLightInfo* target = &light_info;

    while(target)
    {
        const auto& light = target->light;

        float light_l = light.pos[0];
        float light_t = light.pos[1];
        float light_r = light.pos[0];
        float light_b = light.pos[1];

        // get full coordinates for boxes and bars
        if(light.type == GLLightType::box || light.type == GLLightType::bar)
        {
            light_l = SDL_min(light_l, light.pos[2]);
            light_t = SDL_min(light_t, light.pos[3]);
            light_r = SDL_max(light_r, light.pos[2]);
            light_b = SDL_max(light_b, light.pos[3]);
        }

        // test light collision with draw rect
        if(light_r >= texcoord.tl.x && light_l <= texcoord.br.x && light_b >= texcoord.tl.y && light_t <= texcoord.br.y)
        {
            auto& new_light = m_light_queue.lights[m_light_count++];

            new_light = light;

            // crop box lights to drawn rect
            if(new_light.type == GLLightType::box)
            {
                if(new_light.pos[0] < texcoord.tl.x)
                    new_light.pos[0] = texcoord.tl.x;
                if(new_light.pos[1] < texcoord.tl.y)
                    new_light.pos[1] = texcoord.tl.y;
                if(new_light.pos[2] > texcoord.br.x)
                    new_light.pos[2] = texcoord.br.x;
                if(new_light.pos[3] > texcoord.br.y)
                    new_light.pos[3] = texcoord.br.y;
            }

            // project bar lights to drawn rect
            if(new_light.type == GLLightType::bar)
            {
                // ...
            }

            // place the light in the scene, including any rotoscale transformations
            bool onscreen = false;

            int num_pts = (new_light.type == GLLightType::box || new_light.type == GLLightType::bar) ? 2 : 1;
            for(int pt = 0; pt < num_pts; pt++)
            {
                auto& x_dest = new_light.pos[pt * 2 + 0];
                auto& y_dest = new_light.pos[pt * 2 + 1];

                float x_coord = (x_dest - texcoord.tl.x) / (texcoord.br.x - texcoord.tl.x);
                float y_coord = (y_dest - texcoord.tl.y) / (texcoord.br.y - texcoord.tl.y);

                x_dest = (loc.br.x * x_coord * y_coord) + (loc.bl.x * (1 - x_coord) * y_coord) + (loc.tr.x * x_coord * (1 - y_coord)) + (loc.tl.x * (1 - x_coord) * (1 - y_coord));
                y_dest = (loc.br.y * x_coord * y_coord) + (loc.bl.y * (1 - x_coord) * y_coord) + (loc.tr.y * x_coord * (1 - y_coord)) + (loc.tl.y * (1 - x_coord) * (1 - y_coord));

                // use a 16 pixel margin for detecting onscreen draws (cross-ref the lighting shader)
                if(x_dest >= -16 && x_dest < m_viewport.w + 16 && y_dest >= -16 && y_dest < m_viewport.h + 16)
                    onscreen = true;
            }

            // set depth
            new_light.depth = depth;

            // cancel light if offscreen
            if(!onscreen)
                m_light_count--;

            if(m_light_count >= (int)m_light_queue.lights.size())
                break;
        }

        // access next light (if it exists)
        if(target->next)
            target = &*target->next;
        else
            target = nullptr;
    }
#else
    UNUSED(light_info);
    UNUSED(loc);
    UNUSED(texcoord);
    UNUSED(depth);
#endif
}

#ifdef __EMSCRIPTEN__
static EM_BOOL s_emscriptenHandleResize(int, const EmscriptenUiEvent *, void *)
{
    SDL_Event event;
    event.type = SDL_WINDOWEVENT;
    event.window.event = SDL_WINDOWEVENT_RESIZED;
    SDL_PushEvent(&event);
    return 0;
}
#endif

bool RenderGL::initRender(SDL_Window *window)
{
    pLogDebug("Init renderer settings...");

    m_window = window;

    if(!AbstractRender_t::init())
        return false;

    if(!initOpenGL()
        || !initDebug()
        || !initShaders()
        || !initFramebuffers()
        || !initState()
        || !initVertexArrays())
    {
        close();
        return false;
    }

    std::string init_string = fmt::format_ne("Initialized OpenGL {0}.{1}-{2}", m_gl_majver, m_gl_minver, get_profile_letter(m_gl_profile));

    init_string += " with draw features: ";
    if(m_use_logicop)
        init_string += "logicOp, ";
    if(m_use_depth_buffer)
        init_string += "depth buffer, ";
    if(m_client_side_arrays)
        init_string += "client-side arrays, ";
    if(m_has_npot_textures)
        init_string += "NPOT textures, ";
    if(m_has_bgra_textures)
        init_string += "BGRA format, ";
    if(m_use_shaders)
        init_string += "ES2 shaders, ";
    if(m_has_es3_shaders)
        init_string += "ES3 shaders, ";

    init_string += "and framebuffers: ";
    if(m_buffer_texture[BUFFER_GAME])
        init_string += "game, ";
    if(m_buffer_texture[BUFFER_FB_READ])
        init_string += "fb read, ";
    if(m_buffer_texture[BUFFER_INT_PASS_2])
        init_string += "multipass, ";
    if(m_buffer_texture[BUFFER_LIGHTING])
        init_string += "lighting, ";
    if(m_depth_read_texture)
        init_string += "depth read";

    pLogInfo(init_string.c_str());

    GLenum err = glGetError();
    if(err)
    {
        pLogWarning("Render GL: GL error %d occurred during init process, falling back to SDL.", err);
        close();
        return false;
    }


    // Clean-up from a possible start-up junk
    clearBuffer();

    updateViewport();

#ifdef MUTABLE_PARTICLES_DEMO
    LoadedGLProgramRef_t sparkle = ResolveGLParticleSystem("sparkle");
    if(sparkle)
        s_sparkle = sparkle->get();
#endif

#ifdef __EMSCRIPTEN__
    // need to manually add resize event handler due to likely SDL2-side bug suppressing the events
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, 0, s_emscriptenHandleResize);
#endif

    return true;
}

void RenderGL::close()
{
    RenderGL::clearAllTextures();
    AbstractRender_t::close();

    XTechShaderTranslator::EnsureQuit();

#ifdef RENDERGL_HAS_SHADERS
    m_standard_program.reset();
    m_bitmask_program.reset();
    m_output_program.reset();
    m_program_circle.reset();
    m_program_circle_hole.reset();
    m_program_rect_filled.reset();
    m_program_rect_unfilled.reset();
    m_lighting_calc_program.reset();
    m_lighting_apply_program.reset();
    m_distance_field_1_program.reset();
    m_distance_field_2_program.reset();
#endif

    for(int i = BUFFER_GAME; i < BUFFER_MAX; i++)
        destroyFramebuffer((BufferIndex_t)i);

#ifdef RENDERGL_HAS_SHADERS
    if(m_null_light_texture)
    {
        glDeleteTextures(1, &m_null_light_texture);
        m_null_light_texture = 0;
    }
#endif

#ifdef RENDERGL_HAS_VBO
    if(m_vertex_buffer[0])
    {
        glDeleteBuffers(s_num_vertex_buffers, m_vertex_buffer);
        for(int i = 0; i < s_num_vertex_buffers; i++)
            m_vertex_buffer[i] = 0;
    }

    if(m_light_ubo)
    {
        glDeleteBuffers(1, &m_light_ubo);
        m_light_ubo = 0;
    }

#endif

#ifdef RENDERGL_HAS_VAO
    if(m_glcore_vao)
    {
        glDeleteVertexArrays(1, &m_glcore_vao);
        m_glcore_vao = 0;
    }
#endif

    if(m_gContext)
        SDL_GL_DeleteContext(m_gContext);

    m_gContext = nullptr;
}

void RenderGL::repaint()
{
#ifdef USE_RENDER_BLOCKING
    if(m_blockRender)
        return;
#endif

#ifdef MUTABLE_PARTICLES_DEMO
    if(s_sparkle)
        renderParticleSystem(*s_sparkle, vScreen[1].X, vScreen[1].Y);
#endif

    if(XRender::g_BitmaskTexturePresent && g_ForceBitmaskMerge)
        SuperPrintScreenCenter("Simulating GIFs2PNG (active)", 5, 2, XTColorF(1.0f, 0.7f, 0.5f));
    else if(g_ForceBitmaskMerge)
        SuperPrintScreenCenter("Simulating GIFs2PNG (inactive)", 5, 2, XTColorF(1.0f, 0.7f, 0.5f));

    flushDrawQueues();

    m_render_planes.reset();
    if(m_use_depth_buffer)
        glClear(GL_DEPTH_BUFFER_BIT);

#ifdef USE_SCREENSHOTS_AND_RECS
    if(TakeScreen)
    {
        makeShot();
        PlaySoundMenu(SFX_GotItem);
        TakeScreen = false;
    }
#endif

#ifdef PGE_ENABLE_VIDEO_REC
    processRecorder();
#endif

    setTargetScreen();

    if(m_use_depth_buffer)
        glDisable(GL_DEPTH_TEST);

    if(m_game_texture_fb && m_game_texture)
    {
        clearBuffer();

        int hardware_w, hardware_h;
        getRenderSize(&hardware_w, &hardware_h);

        // draw screen at correct physical coordinates
        RectI draw_loc = RectI(m_phys_x, m_phys_y, m_phys_x + m_phys_w, m_phys_y + m_phys_h);
        RectF draw_source = RectF(0.0, 0.0, 1.0, 1.0);
        Vertex_t::Tint tint = {255, 255, 255, 255};

        std::array<Vertex_t, 4> vertex_attribs = genTriangleStrip(draw_loc, draw_source, 0, tint);

        fillVertexBuffer(vertex_attribs.data(), 4);

        glBindTexture(GL_TEXTURE_2D, m_game_texture);

#ifdef RENDERGL_HAS_SHADERS
        if(m_use_shaders)
        {
            m_output_program.use_program();
            m_output_program.update_transform(m_transform_tick, m_transform_matrix.data(), m_shader_read_viewport.data(), m_shader_clock);
        }
#endif

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    Controls::RenderTouchControls();

    flushDrawQueues();

    if(m_use_depth_buffer)
        glEnable(GL_DEPTH_TEST);

    m_render_planes.reset();

    SDL_GL_SwapWindow(m_window);

    m_current_frame++;
    if(m_current_frame % 512 == 0)
        cleanupDrawQueues();

    m_shader_clock = (GLfloat)((CommonFrame_NotFrozen) % (65 * 60)) / 65.0f;

#if 0
    GLuint err;
    while((err = glGetError()) != 0)
        pLogWarning("Render GL: got GL error code %d on repaint", (int)err);
#endif

    // clear buffer in legacy GL
    if(!m_game_texture_fb)
        clearBuffer();
}

void RenderGL::applyViewport()
{
    if(m_recentTargetScreen || !m_gContext)
        return;

    PointI off = m_viewport_offset_ignore ? PointI(0, 0) : m_viewport_offset;

    // fix offscreen coordinates
    RectSizeI viewport = m_viewport;

    s_normalize_coords(viewport);

    off += viewport.xy;
    off -= m_viewport.xy;

    int y_sign = 1;

    if(m_has_fbo && m_game_texture_fb)
    {
        RectSizeI viewport_scaled = viewport * m_render_scale_factor;

        glViewport(viewport_scaled.x, viewport_scaled.y, viewport_scaled.w, viewport_scaled.h);
    }
    else
    {
        int phys_offset_x = viewport.x * m_phys_w / XRender::TargetW;
        int phys_width = viewport.w * m_phys_w / XRender::TargetW;

        int phys_offset_y = viewport.y * m_phys_h / XRender::TargetH;
        int phys_height = viewport.h * m_phys_h / XRender::TargetH;

        y_sign = -1;

        glViewport(m_phys_x + phys_offset_x,
                m_phys_y + m_phys_h - phys_height - phys_offset_y, // relies on fact that m_phys_y is a symmetric border
                phys_width,
                phys_height);
    }

    if(m_use_shaders)
    {
        m_transform_matrix = {
            2.0f / (float)viewport.w, 0.0f, 0.0f, 0.0f,
            0.0f, y_sign * 2.0f / (float)viewport.h, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f / (float)(1 << 15), 0.0f,
            -(float)(viewport.w + off.x + off.x) / (viewport.w), -y_sign * (float)(viewport.h + off.y + off.y) / (viewport.h), 0.0f, 1.0f,
        };

        m_shader_read_viewport = {
            // multiply
            0.5f * (float)viewport.w / (float)XRender::TargetW,
            0.5f * (float)viewport.h / (float)XRender::TargetH,

            // add
            ((float)(viewport.x + off.x) + 0.5f * (float)viewport.w) / (float)XRender::TargetW,
            ((float)(viewport.y + off.y) + 0.5f * (float)viewport.h) / (float)XRender::TargetH,
        };

        m_transform_tick++;
    }
    else
    {
#ifdef RENDERGL_HAS_FIXED_FUNCTION
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        m_Ortho(off.x, viewport.w + off.x, (y_sign == -1) * viewport.h + off.y, (y_sign == 1) * viewport.h + off.y, (1 << 15), -(1 << 15));
#endif
    }
}

void RenderGL::updateViewport()
{
    flushDrawQueues();

    int hardware_w, hardware_h;
    getRenderSize(&hardware_w, &hardware_h);

    // quickly update the HiDPI scaling factor
    int window_w, window_h;
    XWindow::getWindowSize(&window_w, &window_h);
    m_hidpi_x = (float)hardware_w / (float)window_w;
    m_hidpi_y = (float)hardware_h / (float)window_h;

    float scale_x = (float)hardware_w / XRender::TargetW;
    float scale_y = (float)hardware_h / XRender::TargetH;

    float scale = SDL_min(scale_x, scale_y);

    if(g_config.scale_mode == Config_t::SCALE_FIXED_05X && scale > 0.5f)
        scale = 0.5f;
    if(g_config.scale_mode == Config_t::SCALE_DYNAMIC_INTEGER && scale > 1.f)
        scale = std::floor(scale);
    if(g_config.scale_mode == Config_t::SCALE_FIXED_1X && scale > 1.f)
        scale = 1.f;
    if(g_config.scale_mode == Config_t::SCALE_FIXED_2X && scale > 2.f)
        scale = 2.f;

    m_phys_w = XRender::TargetW * scale;
    m_phys_h = XRender::TargetH * scale;

    pLogDebug("Window resolution is %d x %d; physical draw screen is %d x %d", hardware_w, hardware_h, m_phys_w, m_phys_h);

    m_phys_x = hardware_w / 2 - m_phys_w / 2;
    m_phys_y = hardware_h / 2 - m_phys_h / 2;

    resetViewport();

    if(ScaleWidth != XRender::TargetW || ScaleHeight != XRender::TargetH || m_current_scale_mode != g_config.scale_mode)
    {
        // update render targets
        if(ScaleWidth != XRender::TargetW || ScaleHeight != XRender::TargetH)
        {
#ifdef PGE_ENABLE_VIDEO_REC
            // invalidates GIF recorder handle
            if(recordInProcess())
                toggleGifRecorder();
#endif

            initFramebuffers();

            ScaleWidth = XRender::TargetW;
            ScaleHeight = XRender::TargetH;
        }

        // update render texture scaling mode
        bool use_linear = (g_config.scale_mode == Config_t::SCALE_DYNAMIC_LINEAR || scale < 0.5f);

        if(m_game_texture)
        {
            glBindTexture(GL_TEXTURE_2D, m_game_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, use_linear ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, use_linear ? GL_LINEAR : GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        m_current_scale_mode = g_config.scale_mode;
    }

    SDL_GL_SetSwapInterval(g_config.render_vsync);
}

void RenderGL::resetViewport()
{
    // NOTE: resetViewport should not affect the viewport offset variable. Commented out logic.

    bool viewport_same = (m_viewport.x == 0 && m_viewport.y == 0 && m_viewport.w == XRender::TargetW && m_viewport.h == XRender::TargetH);
    // bool no_offset = m_viewport_offset_ignore || (m_viewport_offset.x == 0 && m_viewport_offset.y == 0);
    bool viewport_changed = (!viewport_same /*|| !no_offset*/);

    if(viewport_changed)
        flushDrawQueues();

    m_viewport = RectSizeI(0, 0, XRender::TargetW, XRender::TargetH);

    // m_viewport_offset = PointI(0, 0);
    // m_viewport_offset_ignore = false;

    if(viewport_changed)
        applyViewport();
}

void RenderGL::setViewport(int x, int y, int w, int h)
{
    if(x == m_viewport.x && y == m_viewport.y && w == m_viewport.w && h == m_viewport.h)
        return;

    flushDrawQueues();

    m_viewport = RectSizeI(x, y, w, h);

    applyViewport();
}

void RenderGL::offsetViewport(int x, int y)
{
    if(x == m_viewport_offset.x && y == m_viewport_offset.y)
        return;

    flushDrawQueues();

    m_viewport_offset = PointI(x, y);

    applyViewport();
}

void RenderGL::offsetViewportIgnore(bool en)
{
    if(en == m_viewport_offset_ignore)
        return;

    if(m_viewport_offset.x == 0 && m_viewport_offset.y == 0)
    {
        m_viewport_offset_ignore = en;
        return;
    }

    flushDrawQueues();

    m_viewport_offset_ignore = en;

    applyViewport();
}

void RenderGL::mapToScreen(int x, int y, int *dx, int *dy)
{
    *dx = static_cast<int>((static_cast<float>(x) * m_hidpi_x - m_phys_x) * XRender::TargetW / m_phys_w);
    *dy = static_cast<int>((static_cast<float>(y) * m_hidpi_y - m_phys_y) * XRender::TargetH / m_phys_h);
}

void RenderGL::mapFromScreen(int scr_x, int scr_y, int *window_x, int *window_y)
{
    *window_x = ((float)scr_x * m_phys_w / XRender::TargetW + m_phys_x) / m_hidpi_x;
    *window_y = ((float)scr_y * m_phys_h / XRender::TargetH + m_phys_y) / m_hidpi_y;
}

#ifdef __EMSCRIPTEN__

EM_JS(int, get_canvas_width, (), { return canvas.width; });
EM_JS(int, get_canvas_height, (), { return canvas.height; });

void RenderGL::getRenderSize(int *w, int *h)
{
    *w = get_canvas_width();
    *h = get_canvas_height();
}

#else

void RenderGL::getRenderSize(int *w, int *h)
{
    if(!m_window)
    {
        pLogDebug("Render GL: getRenderSize called prior to GL initialization, returning 800 x 600");
        *w = 800;
        *h = 600;
    }

    SDL_GL_GetDrawableSize(m_window, w, h);

    if(*w <= 0 || *h <= 0)
    {
        pLogWarning("Render GL: SDL_GL_GetDrawableSize returned <= 0 (%d x %d)", *w, *h);
        SDL_GetWindowSize(m_window, w, h);
    }
}

#endif

void RenderGL::setTargetTexture()
{
    if(!m_recentTargetScreen)
        return;

    flushDrawQueues();

#ifdef RENDERGL_HAS_FBO
    if(m_game_texture_fb && m_game_texture)
        glBindFramebuffer(GL_FRAMEBUFFER, m_game_texture_fb);
#endif

    m_recentTargetScreen = false;
    applyViewport();
}

void RenderGL::setTargetScreen()
{
    if(m_recentTargetScreen)
        return;

    flushDrawQueues();

#ifdef RENDERGL_HAS_FBO
    if(m_has_fbo && m_game_texture_fb)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif

    m_recentTargetScreen = true;

    int hardware_w, hardware_h;
    getRenderSize(&hardware_w, &hardware_h);

    glViewport(0,
            0,
            hardware_w,
            hardware_h);

    if(m_use_shaders)
    {
        m_transform_matrix = {
            2.0f / (float)hardware_w, 0.0f, 0.0f, 0.0f,
            0.0f, -2.0f / (float)hardware_h, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f / (float)(1 << 15), 0.0f,
            -1.0f, 1.0f, 0.0f, 1.0f,
        };

        m_shader_read_viewport = {
            0.0f, 0.0f, 0.0f, 0.0f,
        };

        m_transform_tick++;
    }
    else
    {
#ifdef RENDERGL_HAS_FIXED_FUNCTION
        glMatrixMode( GL_PROJECTION );
        glLoadIdentity();
        m_Ortho(0, hardware_w, hardware_h, 0, (1 << 15), -(1 << 15));
#endif
    }
}

void RenderGL::setDrawPlane(uint8_t plane)
{
    m_render_planes.set_plane(plane);

    m_recent_draw_context = DrawContext_t(nullptr);
    m_mask_draw_context_depth.clear();
}

static int s_nextPowerOfTwo(int val)
{
    int power = 1;
    while(power < val)
        power *= 2;
    return power;
}

#ifndef GL_BGRA_EXT
// from OpenGL 1.2 desktop spec and OES extension GL_EXT_texture_format_BGRA8888
#define GL_BGRA_EXT 0x80E1
#endif

void RenderGL::loadTextureInternal(StdPicture &target, uint32_t width, uint32_t height, uint8_t *RGBApixels, uint32_t pitch, bool is_mask, uint32_t least_width, uint32_t least_height)
{
    // clear pre-existing errors
    GLuint err;
    while((err = glGetError()) != 0)
        pLogWarning("Render GL: got GL error code %d prior to texture load", (int)err);

    // SDL_Surface *surface;
    // SDL_Texture *texture = nullptr;

#ifdef THEXTECH_BIG_ENDIAN
    target.d.format = GL_RGBA;
    const GLenum cpu_format = GL_RGBA;
#else
    target.d.format = (m_has_bgra_textures) ? GL_BGRA_EXT : GL_RGBA;
    const GLenum cpu_format = GL_BGRA_EXT;
#endif

    target.d.nOfColors = target.d.format;

    GLuint tex_id;

    uint32_t pad_w = SDL_max(least_width, width);
    uint32_t pad_h = SDL_max(least_height, height);

    if(!m_has_npot_textures)
    {
        pad_w = s_nextPowerOfTwo(pad_w);
        pad_h = s_nextPowerOfTwo(pad_h);
    }

    uint8_t* padded_pixels = nullptr;
    uint8_t* use_pixels = nullptr;

    pitch /= 4;

    // can't do because of pixel substitution
    if(pad_w == pitch && height == pad_h && target.d.format == cpu_format)
    {
        use_pixels = RGBApixels;
    }
    else
    {
        padded_pixels = (uint8_t*) malloc(pad_w * pad_h * 4);
        if(!padded_pixels)
        {
            pLogWarning("Render GL: Failed to allocate padded texture memory");
            if(!is_mask)
                target.d = StdPictureData();
            return;
        }

        if(is_mask)
            SDL_memset(padded_pixels, 255, pad_w * pad_h * 4);
        else
            SDL_memset(padded_pixels, 0, pad_w * pad_h * 4);

        if(target.d.format == cpu_format)
        {
            uint32_t* padded_pixels_q = reinterpret_cast<uint32_t*>(padded_pixels);
            uint32_t* input_pixels_q = reinterpret_cast<uint32_t*>(RGBApixels);
            for(uint32_t y = 0; y < height; y++)
            {
                for(uint32_t x = 0; x < width; x++)
                    padded_pixels_q[(y * pad_w + x)] = input_pixels_q[(y * pitch + x)];
            }
        }
        else
        {
            for(uint32_t y = 0; y < height; y++)
            {
                for(uint32_t x = 0; x < width; x++)
                {
                    padded_pixels[(y * pad_w + x) * 4 + 0] = RGBApixels[(y * pitch + x) * 4 + 2];
                    padded_pixels[(y * pad_w + x) * 4 + 1] = RGBApixels[(y * pitch + x) * 4 + 1];
                    padded_pixels[(y * pad_w + x) * 4 + 2] = RGBApixels[(y * pitch + x) * 4 + 0];
                    padded_pixels[(y * pad_w + x) * 4 + 3] = RGBApixels[(y * pitch + x) * 4 + 3];
                }
            }
        }

        use_pixels = padded_pixels;
    }

    if(!use_pixels && is_mask)
    {
        pLogWarning("Render GL: Couldn't find mask data");
        return;
    }
    else if(!use_pixels)
    {
        pLogWarning("Render GL: Couldn't find texture data");
        target.d = StdPictureData();
        return;
    }

    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);

#ifdef THEXTECH_BUILD_GL_DESKTOP_LEGACY
    auto texborder = (m_gl_majver == 1 && m_gl_profile == SDL_GL_CONTEXT_PROFILE_COMPATIBILITY)
        ? GL_CLAMP : GL_CLAMP_TO_EDGE;
#else
    auto texborder = GL_CLAMP_TO_EDGE;
#endif

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texborder);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texborder);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    auto internal_format = (m_gl_profile == SDL_GL_CONTEXT_PROFILE_ES) ? target.d.format : GL_RGBA;

    glTexImage2D(GL_TEXTURE_2D,
        0,
        internal_format,
        pad_w,
        pad_h,
        0,
        target.d.format,
        GL_UNSIGNED_BYTE,
        use_pixels);

    glBindTexture(GL_TEXTURE_2D, 0);

    if(padded_pixels)
    {
        free(padded_pixels);
        padded_pixels = nullptr;
    }

    use_pixels = nullptr;

    if(!is_mask)
    {
        target.d.w_scale = static_cast<float>(width) / target.w;
        target.d.h_scale = static_cast<float>(height) / target.h;

        target.d.w_scale /= pad_w;
        target.d.h_scale /= pad_h;
    }

    // check for errors as a result of texture load
    err = glGetError();
    if(err != GL_NO_ERROR && is_mask)
    {
        glDeleteTextures(1, &tex_id);
        pLogWarning("Render GL: Couldn't load mask data (GL error code %d)", (int)err);
        return;
    }
    else if(err != GL_NO_ERROR)
    {
        glDeleteTextures(1, &tex_id);

        pLogWarning("Render GL: Couldn't load texture data (GL error code %d)", (int)err);
        target.d = StdPictureData();
        return;
    }

    if(is_mask)
        target.d.mask_texture_id = tex_id;
    else
    {
        target.d.texture_id = tex_id;
        target.inited = true;
    }
    m_loadedPictures.insert(&target);

#if defined(__APPLE__) && defined(USE_APPLE_X11)
    // SDL_GL_UnbindTexture(texture); // Unbind texture after it got been loaded (otherwise a white screen will happen)
#endif
}

void RenderGL::loadTextureInternal(StdPicture &target, uint32_t width, uint32_t height, uint8_t *RGBApixels, uint32_t pitch, uint32_t mask_width, uint32_t mask_height)
{
    loadTextureInternal(target, width, height, RGBApixels, pitch, false, mask_width, mask_height);
}

void RenderGL::loadTextureMask(StdPicture &target, uint32_t mask_width, uint32_t mask_height, uint8_t *RGBApixels, uint32_t pitch, uint32_t image_width, uint32_t image_height)
{
    loadTextureInternal(target, mask_width, mask_height, RGBApixels, pitch, true, image_width, image_height);
}

void RenderGL::compileShaders(StdPicture &target)
{
#ifdef RENDERGL_HAS_SHADERS
    if(!m_use_shaders || target.d.shader_program)
        return;

    pLogDebug("Render GL: compiling shader...");

    if(target.l.particleVertexShaderSource.empty())
    {
        target.d.shader_program.reset(new GLProgramObject(
            s_es2_advanced_vert_src,
            target.l.fragmentShaderSource.c_str()
        ));
    }
    else
    {
        target.d.shader_program.reset(new GLProgramObject(
            target.l.particleVertexShaderSource.c_str(),
            target.l.fragmentShaderSource.empty() ? s_es2_standard_frag_src : target.l.fragmentShaderSource.c_str(),
            true
        ));

        target.d.particle_system.reset(new GLParticleSystem());
        target.d.particle_system->init(256);
    }

    if(target.d.shader_program && !target.d.shader_program->inited())
    {
        target.d.shader_program.reset();
        pLogDebug("Render GL: using default program due to failed compile/link...");
    }

    if(target.d.shader_program)
        target.d.shader_program->restore_uniforms(target);
#else
    UNUSED(target);
#endif
}

bool RenderGL::textureMaskSupported()
{
    return (m_use_logicop) || (m_use_shaders && m_buffer_texture[BUFFER_FB_READ]);
}

bool RenderGL::depthTestSupported()
{
    return m_use_depth_buffer;
}

bool RenderGL::userShadersSupported()
{
    return m_use_shaders && m_buffer_texture[BUFFER_FB_READ] && m_buffer_texture[BUFFER_INT_PASS_1];
}

void RenderGL::unloadTexture(StdPicture &tx)
{
    auto corpseIt = m_loadedPictures.find(&tx);
    if(corpseIt != m_loadedPictures.end())
        m_loadedPictures.erase(corpseIt);

    D_pLogDebug("RenderGL: unloading texture at %p, new texture count %d...", &tx, (int)m_loadedPictures.size());

    if(tx.d.texture_id)
        glDeleteTextures(1, &tx.d.texture_id);

    if(tx.d.mask_texture_id)
        glDeleteTextures(1, &tx.d.mask_texture_id);

    tx.d = StdPictureData();

    if(!tx.l.canLoad())
        static_cast<StdPicture_Sub&>(tx) = StdPicture_Sub();

    return;
}

void RenderGL::unloadGifTextures()
{
    // need to backup because unloadTexture modifies m_loadedPictures
    auto pictures_bak = m_loadedPictures;

    for(StdPicture *tx : pictures_bak)
    {
        if(!tx->l.rawMask.empty())
        {
            D_pLogDebug("RenderGL: unloading texture at %p on unloadGifTextures()", tx);
            unloadTexture(*tx);
        }
    }
}

void RenderGL::clearAllTextures()
{
    for(StdPicture *tx : m_loadedPictures)
    {
        D_pLogDebug("RenderGL: unloading texture at %p on clearAllTextures()", tx);

        if(tx->d.texture_id)
            glDeleteTextures(1, &tx->d.texture_id);

        if(tx->d.mask_texture_id)
            glDeleteTextures(1, &tx->d.mask_texture_id);

        tx->d = StdPictureData();

        if(!tx->l.canLoad())
            static_cast<StdPicture_Sub&>(*tx) = StdPicture_Sub();
    }

    m_loadedPictures.clear();
}

void RenderGL::clearBuffer()
{
    if(!m_gContext)
        return;

//#ifdef USE_RENDER_BLOCKING
//    SDL_assert(!m_blockRender);
//#endif

    clearDrawQueues();

    glBindTexture(GL_TEXTURE_2D, 0);
    glClear(GL_COLOR_BUFFER_BIT);
}


#ifdef THEXTECH_BUILD_GL_MODERN

int RenderGL::registerUniform(StdPicture &target, const char* name)
{

    if(userShadersSupported() && target.d.shader_program && target.d.shader_program->inited())
        return target.d.shader_program->register_uniform(name, target);
    else
        return AbstractRender_t::registerUniform(target, name);
}

void RenderGL::assignUniform(StdPicture &target, int index, const UniformValue_t& value)
{
    if(userShadersSupported() && target.d.shader_program && target.d.shader_program->inited())
        return target.d.shader_program->assign_uniform(index, value, target);
    else
        return AbstractRender_t::assignUniform(target, index, value);
}

void RenderGL::spawnParticle(StdPicture &target, double worldX, double worldY, ParticleVertexAttrs_t attrs)
{
    if(!target.inited)
        return;

    if(!target.d.texture_id && target.l.lazyLoaded)
        lazyLoad(target);

    if(!target.d.particle_system)
    {
        D_pLogWarningNA("Attempt to spawn particle in non-particle system StdPicture!");
        return;
    }

    ParticleVertexMutable_t particle;
    particle.position = {(GLfloat)worldX, (GLfloat)worldY};
    particle.spawn_time = m_shader_clock;
    particle.attribs = attrs;

    target.d.particle_system->add_particle(particle);
}

void RenderGL::addLight(const GLLight &light)
{
    if(!m_lighting_calc_program.inited() || m_light_count >= (int)m_light_queue.lights.size())
        return;

    m_light_queue.lights[m_light_count++] = light;
}

void RenderGL::setupLighting(const GLLightSystem &system)
{
    m_light_queue.header = system;
}

void RenderGL::renderLighting()
{
    if(!m_light_queue.header || !m_has_es3_shaders || !m_light_ubo || !m_lighting_calc_program.inited() || !m_lighting_apply_program.inited())
        return;

    int16_t cur_depth = m_render_planes.next();

    DrawContext_t context = {m_lighting_apply_program};

    Vertex_t::Tint tint = F_TO_B(XTColor());

    auto& vertex_list = getOrderedDrawVertexList(context, cur_depth);

    PointI off = m_viewport_offset_ignore ? PointI(0, 0) : m_viewport_offset;

    RectI draw_loc = RectI(0 + off.x, 0 + off.y, m_viewport.w + off.x, m_viewport.h + off.y);
    RectF draw_source = RectF(0.0f, 0.0f, 1.0f, 1.0f);

    addVertices(vertex_list, draw_loc, draw_source, cur_depth, tint);

    cur_depth++;
    m_drawQueued = true;

}

#endif // THEXTECH_BUILD_GL_MODERN


void RenderGL::renderRect(int x, int y, int w, int h, XTColor color, bool filled)
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif

    if(!m_use_shaders && !filled)
    {
        renderRect(x, y, 2, h, color, true);
        renderRect(x + w - 2, y, 2, h, color, true);
        renderRect(x, y, w, 2, color, true);
        renderRect(x, y + h - 2, w, 2, color, true);
        return;
    }

    int16_t cur_depth = m_render_planes.next();

    DrawContext_t context = {(filled) ? m_program_rect_filled : m_program_rect_unfilled};

    Vertex_t::Tint tint = F_TO_B(color);

    auto& vertex_list = ((m_use_depth_buffer && tint[3] == 255) ? m_unordered_draw_queue[context] : getOrderedDrawVertexList(context, cur_depth));

    RectI draw_loc = RectI(x, y, x + w, y + h);

    // want interpolated value to be <= 0 for first two pixels, >= 1 for last two pixels
    float u1 = -2.0f / w;
    float u2 = (w + 2.0f) / w;

    float v1 = -2.0f / h;
    float v2 = (h + 2.0f) / h;

    RectF draw_source = RectF(u1, v1, u2, v2);

    addVertices(vertex_list, draw_loc, draw_source, cur_depth, tint);

    cur_depth++;
    m_drawQueued = true;
}

void RenderGL::renderRectBR(int _left, int _top, int _right, int _bottom, XTColor color)
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif

    renderRect(_left, _top, _right - _left, _bottom - _top, color);
}

void RenderGL::renderCircle(int cx, int cy, int radius, XTColor color, bool filled)
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif
    UNUSED(filled);

    if(radius <= 0)
        return; // Nothing to draw

    DrawContext_t context = {m_program_circle};

    Vertex_t::Tint tint = F_TO_B(color);

    int16_t cur_depth = m_render_planes.next();

    auto& vertex_list = ((m_use_depth_buffer && tint[3] == 255) ? m_unordered_draw_queue[context] : getOrderedDrawVertexList(context, cur_depth));

    if(m_use_shaders)
    {
        RectI draw_loc = RectI(cx - radius, cy - radius, cx + radius, cy + radius);
        RectF draw_source = RectF(0.0, 0.0, 1.0, 1.0);

        addVertices(vertex_list, draw_loc, draw_source, cur_depth, tint);
    }
    else
    {
        auto& vertex_attribs = vertex_list.vertices;

        // manually fill with  a full circle poly here
        const int verts = 20;
        const float two_pi = (float)(2 * M_PI);

        GLshort cx_s = cx;
        GLshort cy_s = cy;

        for(int i = 0; i < verts + 1; i++)
        {
            float theta = i * two_pi / verts;
            GLshort x = cx_s + (GLshort)roundf(radius * cosf(theta));
            GLshort y = cy_s + (GLshort)roundf(radius * sinf(theta));

            if(i != 0)
            {
                vertex_attribs.push_back({{x, y, cur_depth}, tint, {0.0, 0.0}});
                vertex_attribs.push_back({{cx_s, cy_s, cur_depth}, tint, {0.0, 0.0}});
            }
            if(i != verts)
                vertex_attribs.push_back({{x, y, cur_depth}, tint, {0.0, 0.0}});
        }
    }

    m_drawQueued = true;
}

void RenderGL::renderCircleHole(int cx, int cy, int radius, XTColor color)
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif

    if(radius <= 0)
        return; // Nothing to draw

    DrawContext_t context = {m_program_circle_hole};

    Vertex_t::Tint tint = F_TO_B(color);

    int16_t cur_depth = m_render_planes.next();

    auto& vertex_list = ((m_use_depth_buffer && tint[3] == 255) ? m_unordered_draw_queue[context] : getOrderedDrawVertexList(context, cur_depth));

    if(m_use_shaders)
    {
        RectI draw_loc = RectI(cx - radius, cy - radius, cx + radius, cy + radius);
        RectF draw_source = RectF(0.0, 0.0, 1.0, 1.0);

        addVertices(vertex_list, draw_loc, draw_source, cur_depth, tint);
    }
    else
    {
        auto& vertex_attribs = vertex_list.vertices;

        // manually fill with  a full circle poly here
        const int verts = 32;
        const float two_pi = (float)(2 * M_PI);

        GLshort cx_s = cx;
        GLshort cy_s = cy;

        for(int i = 0; i < verts; i++)
        {
            float theta1 = i * two_pi / verts;
            float theta2 = (i + 1) * two_pi / verts;

            GLshort x1_perim = cx_s + (GLshort)roundf(radius * cosf(theta1));
            GLshort y1_perim = cy_s + (GLshort)roundf(radius * sinf(theta1));

            GLshort x2_perim = cx_s + (GLshort)roundf(radius * cosf(theta2));
            GLshort y2_perim = cy_s + (GLshort)roundf(radius * sinf(theta2));

            // project onto box
            GLfloat box_radius1 = radius * 2 / (fabsf(cosf(theta1) - sinf(theta1)) + fabsf(cosf(theta1) + sinf(theta1)));
            GLshort x1_box = cx_s + (GLshort)roundf(box_radius1 * cosf(theta1));
            GLshort y1_box = cy_s + (GLshort)roundf(box_radius1 * sinf(theta1));

            GLfloat box_radius2 = radius * 2 / (fabsf(cosf(theta2) - sinf(theta2)) + fabsf(cosf(theta2) + sinf(theta2)));
            GLshort x2_box = cx_s + (GLshort)roundf(box_radius2 * cosf(theta2));
            GLshort y2_box = cy_s + (GLshort)roundf(box_radius2 * sinf(theta2));

            vertex_attribs.push_back({{x1_perim, y1_perim, cur_depth}, tint, {0.0, 0.0}});
            vertex_attribs.push_back({{x1_box, y1_box, cur_depth}, tint, {0.0, 0.0}});
            vertex_attribs.push_back({{x2_perim, y2_perim, cur_depth}, tint, {0.0, 0.0}});
            vertex_attribs.push_back({{x1_box, y1_box, cur_depth}, tint, {0.0, 0.0}});
            vertex_attribs.push_back({{x2_perim, y2_perim, cur_depth}, tint, {0.0, 0.0}});
            vertex_attribs.push_back({{x2_box, y2_box, cur_depth}, tint, {0.0, 0.0}});
        }
    }

    m_drawQueued = true;
}


void RenderGL::renderTextureScaleEx(double xDstD, double yDstD, double wDstD, double hDstD,
                                       StdPicture &tx,
                                       int xSrc, int ySrc,
                                       int wSrc, int hSrc,
                                       double rotateAngle, FPoint_t *center, unsigned int flip,
                                       XTColor color)
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif
    if(!tx.inited)
        return;

    if(!tx.d.texture_id && tx.l.lazyLoaded)
        lazyLoad(tx);

    if(!tx.d.texture_id)
    {
        D_pLogWarningNA("Attempt to render an empty texture!");
        return;
    }

    SDL_assert_release(tx.d.texture_id);

    int xDst = Maths::iRound(xDstD);
    int yDst = Maths::iRound(yDstD);
    int wDst = Maths::iRound(wDstD);
    int hDst = Maths::iRound(hDstD);

    // Don't go more than size of texture
    if(xSrc + wSrc > tx.w)
    {
        wSrc = tx.w - xSrc;
        if(wSrc < 0)
            wSrc = 0;
    }
    if(ySrc + hSrc > tx.h)
    {
        hSrc = tx.h - ySrc;
        if(hSrc < 0)
            hSrc = 0;
    }

    QuadI draw_loc;

    if(rotateAngle != 0.0)
    {
        int cx = center ? center->x : wDst / 2;
        int cy = center ? center->y : hDst / 2;

        RectI offset = RectI(-cx, -cy, wDst - cx, hDst - cy);

        draw_loc = QuadI(offset).rotate(rotateAngle);
        draw_loc += PointI(xDst + cx, yDst + cy);
    }
    else
    {
        draw_loc = QuadI(RectI(xDst, yDst, xDst + wDst, yDst + hDst));
    }

    RectF draw_source_raw = RectF(xSrc, ySrc, xSrc + wSrc, ySrc + hSrc);
    RectF draw_source = draw_source_raw * PointF(tx.d.w_scale, tx.d.h_scale);

    if(flip & X_FLIP_HORIZONTAL)
        std::swap(draw_source.tl.x, draw_source.br.x);

    if(flip & X_FLIP_VERTICAL)
        std::swap(draw_source.tl.y, draw_source.br.y);

    DrawContext_t context = {tx.d.shader_program ? *tx.d.shader_program : m_standard_program, &tx};

    Vertex_t::Tint tint = F_TO_B(color);

    int16_t cur_depth = m_render_planes.next();

    bool draw_opaque = (tx.d.use_depth_test && tint[3] == 255 && !tx.d.shader_program);
    auto& vertex_list = (draw_opaque ? m_unordered_draw_queue[context] : getOrderedDrawVertexList(context, cur_depth));

    addVertices(vertex_list, draw_loc, draw_source, cur_depth, tint);

#ifdef THEXTECH_BUILD_GL_MODERN
    if(tx.l.light_info)
        addLights(*tx.l.light_info, QuadI(draw_loc), draw_source_raw, cur_depth);
#endif

    m_drawQueued = true;
}

void RenderGL::renderTextureScale(double xDst, double yDst, double wDst, double hDst,
                                     StdPicture &tx,
                                     XTColor color)
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif

    if(!tx.inited)
        return;

    if(!tx.d.texture_id && tx.l.lazyLoaded)
        lazyLoad(tx);

    if(!tx.d.texture_id)
    {
        D_pLogWarningNA("Attempt to render an empty texture!");
        return;
    }

    RectI draw_loc = RectI(xDst, yDst, xDst + wDst, yDst + hDst);

    RectF draw_source_raw = RectF(0.0, 0.0, tx.w, tx.h);
    RectF draw_source = draw_source_raw * PointF(tx.d.w_scale, tx.d.h_scale);

    DrawContext_t context = {tx.d.shader_program ? *tx.d.shader_program : m_standard_program, &tx};

    Vertex_t::Tint tint = F_TO_B(color);

    int16_t cur_depth = m_render_planes.next();

    bool draw_opaque = (tx.d.use_depth_test && tint[3] == 255 && !tx.d.shader_program);
    auto& vertex_list = (draw_opaque ? m_unordered_draw_queue[context] : getOrderedDrawVertexList(context, cur_depth));

    addVertices(vertex_list, draw_loc, draw_source, cur_depth, tint);

#ifdef THEXTECH_BUILD_GL_MODERN
    if(tx.l.light_info)
        addLights(*tx.l.light_info, QuadI(draw_loc), draw_source_raw, cur_depth);
#endif

    m_drawQueued = true;
}

void RenderGL::renderTexture(double xDstD, double yDstD, double wDstD, double hDstD,
                                StdPicture &tx,
                                int xSrc, int ySrc,
                                XTColor color)
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif

    if(!tx.inited)
        return;

    if(!tx.d.texture_id && tx.l.lazyLoaded)
        lazyLoad(tx);

    if(!tx.d.texture_id)
    {
        D_pLogWarningNA("Attempt to render an empty texture!");
        return;
    }

    SDL_assert_release(tx.d.texture_id);

#ifdef MUTABLE_PARTICLES_DEMO
    if(&tx >= &GFXMario[1] && &tx <= &GFXMario[7])
    {
        if(s_sparkle)
            spawnParticle(*s_sparkle, xDstD - vScreen[1].X, yDstD - vScreen[1].Y, ParticleVertexAttrs_t());
    }
#endif

    int xDst = Maths::iRound(xDstD);
    int yDst = Maths::iRound(yDstD);
    int wDst = Maths::iRound(wDstD);
    int hDst = Maths::iRound(hDstD);

    // Don't go more than size of texture
    if(xSrc + wDst > tx.w)
    {
        wDst = tx.w - xSrc;
        if(wDst < 0)
            wDst = 0;
    }

    if(ySrc + hDst > tx.h)
    {
        hDst = tx.h - ySrc;
        if(hDst < 0)
            hDst = 0;
    }

    RectI draw_loc = RectI(xDst, yDst, xDst + wDst, yDst + hDst);

    RectF draw_source_raw = RectF(xSrc, ySrc, xSrc + wDst, ySrc + hDst);
    RectF draw_source = draw_source_raw * PointF(tx.d.w_scale, tx.d.h_scale);

    DrawContext_t context = {tx.d.shader_program ? *tx.d.shader_program : m_standard_program, &tx};

    Vertex_t::Tint tint = F_TO_B(color);

    int16_t cur_depth = m_render_planes.next();

#ifdef LIGHTING_DEMO
    if(m_lighting_calc_program.inited() && m_light_count < (int)m_light_queue.lights.size())
    {
        if(&tx == &GFXNPC[NPCID_HOMING_BALL])
            m_light_queue.lights[m_light_count++] = GLLight::Point(xDstD + wDstD / 2.0, yDstD + hDstD / 2.0, cur_depth, GLLightColor(64, 32, 32), 250.0);

        if(&tx == &GFXEffect[EFFID_SMOKE_S3])
            m_light_queue.lights[m_light_count++] = GLLight::Point(xDstD + wDstD / 2.0, yDstD + hDstD / 2.0, cur_depth, GLLightColor(96, 96, 96), 350.0);

        if(&tx == &GFXNPC[NPCID_CANNONITEM])
        {
            int frame = (int)ySrc / 32;
            if(frame >= 0 || frame < 5)
            {
                GLLightColor colors[5] = {GLLightColor(0, 0, 48), GLLightColor(0, 32, 0), GLLightColor(24, 24, 0), GLLightColor(24, 0, 24), GLLightColor(24, 16, 32)};

                m_light_queue.lights[m_light_count++] = GLLight::Point(xDstD + wDstD / 2.0, yDstD + hDstD / 2.0, cur_depth, colors[frame], 400.0);
            }
        }
    }
#endif

    bool draw_opaque = (tx.d.use_depth_test && tint[3] == 255 && !tx.d.shader_program);
    auto& vertex_list = (draw_opaque ? m_unordered_draw_queue[context] : getOrderedDrawVertexList(context, cur_depth));

    addVertices(vertex_list, draw_loc, draw_source, cur_depth, tint);

#ifdef THEXTECH_BUILD_GL_MODERN
    if(tx.l.light_info)
        addLights(*tx.l.light_info, QuadI(draw_loc), draw_source_raw, cur_depth);
#endif

    m_drawQueued = true;
}

void RenderGL::renderTextureFL(double xDstD, double yDstD, double wDstD, double hDstD,
                                  StdPicture &tx,
                                  int xSrc, int ySrc,
                                  double rotateAngle, FPoint_t *center, unsigned int flip,
                                  XTColor color)
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif
    if(!tx.inited)
        return;

    if(!tx.d.texture_id && tx.l.lazyLoaded)
        lazyLoad(tx);

    if(!tx.d.texture_id)
    {
        D_pLogWarningNA("Attempt to render an empty texture!");
        return;
    }

    SDL_assert_release(tx.d.texture_id);

    int xDst = Maths::iRound(xDstD);
    int yDst = Maths::iRound(yDstD);
    int wDst = Maths::iRound(wDstD);
    int hDst = Maths::iRound(hDstD);

    // Don't go more than size of texture
    if(xSrc + wDst > tx.w)
    {
        wDst = tx.w - xSrc;
        if(wDst < 0)
            wDst = 0;
    }

    if(ySrc + hDst > tx.h)
    {
        hDst = tx.h - ySrc;
        if(hDst < 0)
            hDst = 0;
    }

    QuadI draw_loc;

    if(rotateAngle != 0.0)
    {
        int cx = center ? center->x : wDst / 2;
        int cy = center ? center->y : hDst / 2;

        RectI offset = RectI(-cx, -cy, wDst - cx, hDst - cy);

        draw_loc = QuadI(offset).rotate(rotateAngle);
        draw_loc += PointI(xDst + cx, yDst + cy);
    }
    else
    {
        draw_loc = QuadI(RectI(xDst, yDst, xDst + wDst, yDst + hDst));
    }

    RectF draw_source_raw = RectF(xSrc, ySrc, xSrc + wDst, ySrc + hDst);
    RectF draw_source = draw_source_raw * PointF(tx.d.w_scale, tx.d.h_scale);

    if(flip & X_FLIP_HORIZONTAL)
        std::swap(draw_source.tl.x, draw_source.br.x);

    if(flip & X_FLIP_VERTICAL)
        std::swap(draw_source.tl.y, draw_source.br.y);

    DrawContext_t context = {tx.d.shader_program ? *tx.d.shader_program : m_standard_program, &tx};

    Vertex_t::Tint tint = F_TO_B(color);

    int16_t cur_depth = m_render_planes.next();

    bool draw_opaque = (tx.d.use_depth_test && tint[3] == 255 && !tx.d.shader_program);
    auto& vertex_list = (draw_opaque ? m_unordered_draw_queue[context] : getOrderedDrawVertexList(context, cur_depth));

    addVertices(vertex_list, draw_loc, draw_source, cur_depth, tint);

#ifdef THEXTECH_BUILD_GL_MODERN
    if(tx.l.light_info)
        addLights(*tx.l.light_info, QuadI(draw_loc), draw_source_raw, cur_depth);
#endif

    m_drawQueued = true;
}

void RenderGL::renderTexture(float xDst, float yDst,
                                StdPicture &tx,
                                XTColor color)
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif

    if(!tx.inited)
        return;

    if(!tx.d.texture_id && tx.l.lazyLoaded)
        lazyLoad(tx);

    if(!tx.d.texture_id)
    {
        D_pLogWarningNA("Attempt to render an empty texture!");
        return;
    }

    RectI draw_loc = RectI(xDst, yDst, xDst + tx.w, yDst + tx.h);

    RectF draw_source_raw = RectF(0.0, 0.0, tx.w, tx.h);
    RectF draw_source = draw_source_raw * PointF(tx.d.w_scale, tx.d.h_scale);

    DrawContext_t context = {tx.d.shader_program ? *tx.d.shader_program : m_standard_program, &tx};

    Vertex_t::Tint tint = F_TO_B(color);

    int16_t cur_depth = m_render_planes.next();

    bool draw_opaque = (tx.d.use_depth_test && tint[3] == 255 && !tx.d.shader_program);
    auto& vertex_list = (draw_opaque ? m_unordered_draw_queue[context] : getOrderedDrawVertexList(context, cur_depth));

    addVertices(vertex_list, draw_loc, draw_source, cur_depth, tint);

#ifdef THEXTECH_BUILD_GL_MODERN
    if(tx.l.light_info)
        addLights(*tx.l.light_info, QuadI(draw_loc), draw_source_raw, cur_depth);
#endif

    m_drawQueued = true;
}

void RenderGL::renderParticleSystem(StdPicture &tx,
                          double camX,
                          double camY)
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif

    if(!tx.inited)
        return;

    if(!tx.d.texture_id && tx.l.lazyLoaded)
        lazyLoad(tx);

    if(!tx.d.texture_id || !tx.d.shader_program)
    {
        D_pLogWarningNA("Attempt to render an uninitialized particle system!");
        return;
    }

    int16_t cur_depth = m_render_planes.next();

    // assign depth
    assignUniform(tx, 0, UniformValue_t(cur_depth * m_transform_matrix[2 * 4 + 2] + m_transform_matrix[3 * 4 + 2]));

    // assign camera pos
    assignUniform(tx, 1, UniformValue_t((GLfloat)camX, (GLfloat)camY));

    DrawContext_t context = {*tx.d.shader_program, &tx};

    auto& vertex_attribs = getOrderedDrawVertexList(context, cur_depth).vertices;

    vertex_attribs.emplace_back();

    m_drawQueued = true;
}

void RenderGL::getScreenPixels(int x, int y, int w, int h, unsigned char *pixels)
{
    int phys_x, phys_y;

    mapFromScreen(x, y, &phys_x, &phys_y);

    int phys_w = w * m_phys_w / XRender::TargetW;
    int phys_h = h * m_phys_h / XRender::TargetH;

    // allocate buffer for screen-space pixels
    uint8_t* phys_pixels = (uint8_t*)malloc(phys_w * phys_h * 3);
    if(!phys_pixels)
        return;

    // read screen-space pixels
    glReadPixels(phys_x, phys_y, phys_w, phys_h,
        GL_RGB, GL_UNSIGNED_BYTE, phys_pixels);

    // rescale and move to target
    for(int r = 0; r < h; r++)
    {
        int phys_r_max = phys_h - 1;
        int phys_r_ind = r * m_phys_h / XRender::TargetH;

        // vertical flip from OpenGL to image
        int phys_r = phys_r_max - phys_r_ind;

        for(int c = 0; c < w; c++)
        {
            int phys_c = c * m_phys_w / XRender::TargetW;

            pixels[(r * w + c) * 3 + 0] = phys_pixels[(phys_r * phys_w + phys_c) * 3 + 0];
            pixels[(r * w + c) * 3 + 1] = phys_pixels[(phys_r * phys_w + phys_c) * 3 + 1];
            pixels[(r * w + c) * 3 + 2] = phys_pixels[(phys_r * phys_w + phys_c) * 3 + 2];
        }
    }

    free(phys_pixels);
}

void RenderGL::getScreenPixelsRGBA(int x, int y, int w, int h, unsigned char *pixels)
{
    bool direct_screenshot = (m_has_fbo && m_game_texture_fb && m_game_texture);

    int phys_x, phys_y, phys_w, phys_h;

#ifdef RENDERGL_HAS_FBO
    GLint prev_fb = -1;

    if(direct_screenshot)
    {
        phys_x = x * m_render_scale_factor;
        phys_y = y * m_render_scale_factor;
        phys_w = w * m_render_scale_factor;
        phys_h = h * m_render_scale_factor;

        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fb);
        glBindFramebuffer(GL_FRAMEBUFFER, m_game_texture_fb);
    }
    else
#endif
    {
        mapFromScreen(x, y, &phys_x, &phys_y);

        phys_x *= m_hidpi_x;
        phys_y *= m_hidpi_y;
        phys_w = w * m_phys_w / XRender::TargetW;
        phys_h = h * m_phys_h / XRender::TargetH;
    }

    // allocate buffer for screen-space pixels (if rescaling is needed)
    uint8_t* phys_pixels = (direct_screenshot) ? pixels : (uint8_t*)malloc(phys_w * phys_h * 4);
    if(!phys_pixels)
        return;

    // read screen-space pixels
    glReadPixels(phys_x, phys_y, phys_w, phys_h,
        GL_RGBA, GL_UNSIGNED_BYTE, phys_pixels);

#ifdef RENDERGL_HAS_FBO
    if(prev_fb != -1)
        glBindFramebuffer(GL_FRAMEBUFFER, prev_fb);
#endif

    // if rescaling is not needed, we're done!
    if(direct_screenshot)
        return;

    // rescale and move to target
    for(int r = 0; r < h; r++)
    {
        int phys_r = r * phys_h / h;

        // vertical flip from legacy OpenGL to image
        if(!m_has_fbo || !m_game_texture_fb || !m_game_texture)
            phys_r = (phys_h - 1) - phys_r;

        for(int c = 0; c < w; c++)
        {
            int phys_c = c * phys_w / w;

            ((uint32_t*) pixels)[r * w + c] = ((uint32_t*) phys_pixels)[phys_r * phys_w + phys_c];
        }
    }

    free(phys_pixels);
}

int RenderGL::getPixelDataSize(const StdPicture &tx)
{
    if(!tx.d.texture_id)
        return 0;
    return (tx.w * tx.h * 4);
}

void RenderGL::getPixelData(const StdPicture &tx, unsigned char *pixelData)
{
    UNUSED(tx);
    UNUSED(pixelData);
}
