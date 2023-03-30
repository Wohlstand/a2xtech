/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2023 Vitaly Novichkov <admin@wohlnet.ru>
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

#include <SDL2/SDL_version.h>
#include <SDL2/SDL_render.h>

#include "core/sdl/gl_inc.h"

#include <FreeImageLite.h>
#include <Graphics/graphics_funcs.h>

#include <Logger/logger.h>
#include <Utils/maths.h>

#include "core/sdl/render_gl.h"
#include "core/sdl/gl_program_object.h"

#include "video.h"
#include "core/window.h"
#include "graphics.h"


#include "sdl_proxy/sdl_stdinc.h"
#include <fmt_format_ne.h>

#include "controls.h"
#include "sound.h"

#ifndef UNUSED
#define UNUSED(x) (void)x
#endif

#define F_TO_B(r, g, b, a) {static_cast<GLubyte>((r) * 255.0f), static_cast<GLubyte>((g) * 255.0f), static_cast<GLubyte>((b) * 255.0f), static_cast<GLubyte>((a) * 255.0f)}


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


// arguments
constexpr bool s_prefer_fb_to_fb_render = true;
static bool s_enable_debug_output = false;
static int s_num_pass = 2;


// doesn't work on GL ES for now due to limited SDL support

void RenderGL::framebufferCopy(BufferIndex_t dest, BufferIndex_t source, int x, int y, int w, int h)
{
#ifdef RENDERGL_HAS_FBO
    if(x < 0)
    {
        w += x;
        x = 0;
    }

    if(y < 0)
    {
        h += y;
        y = 0;
    }

    if(x + w >= ScreenW)
        w = ScreenW - x;

    if(y + h >= ScreenH)
        h = ScreenH - y;

    if(w <= 0 || h <= 0)
        return;

    if((!s_prefer_fb_to_fb_render || !m_buffer_fb[dest]) && m_buffer_fb[source])
    {
        if(source != BUFFER_GAME)
            glBindFramebuffer(GL_FRAMEBUFFER, m_buffer_fb[source]);

        glBindTexture(GL_TEXTURE_2D, m_buffer_texture[dest]);
        glCopyTexSubImage2D(GL_TEXTURE_2D,
            0,
            x,
            ScreenH - (y + h),
            x,
            ScreenH - (y + h),
            w,
            h);

        if(source != BUFFER_GAME)
            glBindFramebuffer(GL_FRAMEBUFFER, m_game_texture_fb);

        return;
    }
    else if(!m_buffer_fb[dest])
    {
        pLogWarning("Render GL: invalid buffer copy called");
        return;
    }

    if(dest != BUFFER_GAME)
        glBindFramebuffer(GL_FRAMEBUFFER, m_buffer_fb[dest]);

    glBindTexture(GL_TEXTURE_2D, m_buffer_texture[source]);

#ifdef RENDERGL_HAS_SHADERS
    m_standard_program.use_program();
    m_standard_program.update_transform(m_transform_tick, m_transform_matrix.data(), m_shader_read_viewport.data(), (GLfloat)((m_transform_tick / 3) % (65 * 60)) / 65.0f);
#endif

    GLshort x1 = x;
    GLshort x2 = x + w;
    GLshort y1 = y;
    GLshort y2 = y + h;

    GLfloat u1 = (float)x1 / ScreenW;
    GLfloat u2 = (float)x2 / ScreenW;
    GLfloat v1 = (float)(ScreenH - y1) / ScreenH;
    GLfloat v2 = (float)(ScreenH - y2) / ScreenH;

    x1 -= m_viewport_x;
    x2 -= m_viewport_x;
    y1 -= m_viewport_y;
    y2 -= m_viewport_y;

    RenderGL::Vertex_t copy_triangle_strip[] =
    {
        {{x1, y1, m_cur_depth}, {255, 255, 255, 255}, {u1, v1}},
        {{x1, y2, m_cur_depth}, {255, 255, 255, 255}, {u1, v2}},
        {{x2, y1, m_cur_depth}, {255, 255, 255, 255}, {u2, v1}},
        {{x2, y2, m_cur_depth}, {255, 255, 255, 255}, {u2, v2}},
    };

    fillVertexBuffer(copy_triangle_strip, 4);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if(dest != BUFFER_GAME)
        glBindFramebuffer(GL_FRAMEBUFFER, m_game_texture_fb);
#else // #ifdef RENDERGL_HAS_FBO
    UNUSED(dest);
    UNUSED(source);
    UNUSED(x);
    UNUSED(y);
    UNUSED(w);
    UNUSED(h);
    pLogWarning("Render GL: framebufferCopy called without framebuffer subsystem -> no-op");
#endif
}

void RenderGL::fillVertexBuffer(const RenderGL::Vertex_t* vertex_attribs, int count)
{
#ifndef RENDERGL_HAS_VBO
    UNUSED(count);
#endif

    const uint8_t* array_start = 0;

    if(m_client_side_arrays)
    {
        array_start = reinterpret_cast<const uint8_t*>(vertex_attribs);
    }
    else
    {
#ifdef RENDERGL_HAS_VBO
        m_cur_vertex_buffer_index++;
        if(m_cur_vertex_buffer_index >= s_num_vertex_buffers)
            m_cur_vertex_buffer_index = 0;

        GLsizeiptr buffer_size = sizeof(RenderGL::Vertex_t) * count;

        glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer[m_cur_vertex_buffer_index]);

        if(m_vertex_buffer_size[m_cur_vertex_buffer_index] < buffer_size)
        {
#   ifdef RENDERGL_HAS_STREAM_DRAW
            const auto draw_mode = (m_gl_majver == 1 ? GL_DYNAMIC_DRAW : GL_STREAM_DRAW);
#   else
            const auto draw_mode = GL_DYNAMIC_DRAW;
#   endif
            glBufferData(GL_ARRAY_BUFFER, buffer_size, vertex_attribs, draw_mode);
            m_vertex_buffer_size[m_cur_vertex_buffer_index] = buffer_size;
        }
        else
        {
            glBufferSubData(GL_ARRAY_BUFFER, 0, buffer_size, vertex_attribs);
        }
#endif // #ifdef RENDERGL_HAS_VBO
    }

    if(m_use_shaders)
    {
#ifdef RENDERGL_HAS_SHADERS
        glVertexAttribPointer(0, 3, GL_SHORT,         GL_FALSE, sizeof(RenderGL::Vertex_t), array_start + offsetof(RenderGL::Vertex_t, position));
        glVertexAttribPointer(1, 2, GL_FLOAT,         GL_FALSE, sizeof(RenderGL::Vertex_t), array_start + offsetof(RenderGL::Vertex_t, texcoord));
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(RenderGL::Vertex_t), array_start + offsetof(RenderGL::Vertex_t, tint));
#endif
    }
    else
    {
#ifdef RENDERGL_HAS_FIXED_FUNCTION
        glVertexPointer(   3, GL_SHORT,         sizeof(RenderGL::Vertex_t), array_start + offsetof(RenderGL::Vertex_t, position));
        glTexCoordPointer( 2, GL_FLOAT,         sizeof(RenderGL::Vertex_t), array_start + offsetof(RenderGL::Vertex_t, texcoord));
        glColorPointer(    4, GL_UNSIGNED_BYTE, sizeof(RenderGL::Vertex_t), array_start + offsetof(RenderGL::Vertex_t, tint));
#endif
    }
}

void RenderGL::refreshDrawQueues()
{
    for(auto it = m_unordered_draw_queue.begin(); it != m_unordered_draw_queue.end();)
    {
        if(!it->second.active)
        {
            it = m_unordered_draw_queue.erase(it);
        }
        else
        {
            it->second.active = false;
            ++it;
        }
    }

    for(auto it = m_ordered_draw_queue.begin(); it != m_ordered_draw_queue.end();)
    {
        if(!it->second.active)
        {
            it = m_ordered_draw_queue.erase(it);
        }
        else
        {
            it->second.active = false;
            ++it;
        }
    }
}

void RenderGL::clearDrawQueues()
{
    for(auto& i : m_unordered_draw_queue)
        i.second.vertices.clear();

    for(auto& i : m_ordered_draw_queue)
        i.second.vertices.clear();

    m_recent_draw_context = DrawContext_t();
    m_mask_draw_context_depth.clear();
}

void RenderGL::flushDrawQueues()
{
    // pass 1: opaque textures
    for(auto& i : m_unordered_draw_queue)
    {
        const DrawContext_t& context = i.first;
        std::vector<Vertex_t>& vertex_attribs = i.second.vertices;

        if(!vertex_attribs.empty())
            i.second.active = true;
        else
            continue;

        fillVertexBuffer(vertex_attribs.data(), vertex_attribs.size());

#ifdef RENDERGL_HAS_SHADERS
        if(m_use_shaders)
        {
            context.program->use_program();
            context.program->update_transform(m_transform_tick, m_transform_matrix.data(), m_shader_read_viewport.data(), (GLfloat)((m_transform_tick / 3) % (65 * 60)) / 65.0f);
        }
#endif

        if(context.texture)
            glBindTexture(GL_TEXTURE_2D, context.texture->d.texture_id);
        else if(!m_use_shaders)
            glBindTexture(GL_TEXTURE_2D, 0);

        glDrawArrays(GL_TRIANGLES, 0, vertex_attribs.size());

        vertex_attribs.clear();
    }

    // pass 2: translucent / interesting textures
    bool no_translucent_objects = true;
    int num_pass = 1;

    // save the opaque state if there are any multipass shaders
    for(auto& i : m_ordered_draw_queue)
    {
        if(i.second.vertices.empty())
            continue;

        no_translucent_objects = false;

        const GLProgramObject* program = i.first.second.program;

        if(program && program->get_type() >= GLProgramObject::multipass)
        {
            num_pass = s_num_pass;
            framebufferCopy(BUFFER_INIT_PASS, BUFFER_GAME, m_viewport_x, m_viewport_y, m_viewport_w, m_viewport_h);
            break;
        }
    }

    // return without needing to call glDepthMask
    if(no_translucent_objects)
        return;

    glDepthMask(GL_FALSE);

    for(int pass = 0; pass < num_pass; pass++)
    {
#ifdef RENDERGL_HAS_FBO
        if(pass != 0)
        {
            framebufferCopy(BUFFER_PREV_PASS, BUFFER_GAME, m_viewport_x, m_viewport_y, m_viewport_w, m_viewport_h);
            framebufferCopy(BUFFER_GAME, BUFFER_INIT_PASS, m_viewport_x, m_viewport_y, m_viewport_w, m_viewport_h);

            // slot for reading previous pass (only need to reset it once)
            if(pass == 1)
            {
                glActiveTexture(TEXTURE_UNIT_PREVPASS);
                glBindTexture(GL_TEXTURE_2D, m_buffer_texture[BUFFER_PREV_PASS]);
                glActiveTexture(TEXTURE_UNIT_IMAGE);
            }
        }
        else if(m_buffer_texture[BUFFER_INIT_PASS])
        {
            // slot for reading previous pass
            glActiveTexture(TEXTURE_UNIT_PREVPASS);
            glBindTexture(GL_TEXTURE_2D, m_buffer_texture[BUFFER_INIT_PASS]);
            glActiveTexture(TEXTURE_UNIT_IMAGE);
        }
#endif

        for(auto& i : m_ordered_draw_queue)
        {
            // depth is i.first.first
            const DrawContext_t& context = i.first.second;
            std::vector<Vertex_t>& vertex_attribs = i.second.vertices;

            if(!vertex_attribs.empty())
                i.second.active = true;
            else
                continue;

            SDL_assert(context.program);
            GLProgramObject* program = context.program;

            bool need_logic_op = (context.texture && context.texture->d.mask_texture_id && program == &m_standard_program);

            // emulate the logic op if we can't use it directly
            if(!m_use_logicop && need_logic_op)
            {
                program = &m_bitmask_program;
                need_logic_op = false;
            }

#ifdef RENDERGL_HAS_FBO
            if(program->get_type() >= GLProgramObject::read_buffer)
            {
                if(vertex_attribs.size() > 6)
                {
                    framebufferCopy(BUFFER_FB_READ, BUFFER_GAME, m_viewport_x, m_viewport_y, m_viewport_w, m_viewport_h);
                }
                else if(vertex_attribs.size() == 6)
                {
                    framebufferCopy(BUFFER_FB_READ, BUFFER_GAME,
                        m_viewport_x + m_viewport_offset_x + vertex_attribs[0].position[0],
                        m_viewport_y + m_viewport_offset_y + vertex_attribs[0].position[1],
                        vertex_attribs[5].position[0] - vertex_attribs[0].position[0],
                        vertex_attribs[5].position[1] - vertex_attribs[0].position[1]);
                }

                // only program allowed to use mask texture
                if(program == &m_bitmask_program)
                {
                    glActiveTexture(TEXTURE_UNIT_MASK);
                    glBindTexture(GL_TEXTURE_2D, context.texture->d.mask_texture_id);
                    glActiveTexture(TEXTURE_UNIT_IMAGE);
                }
            }
#endif // #ifdef RENDERGL_HAS_FBO

            fillVertexBuffer(vertex_attribs.data(), vertex_attribs.size());

#ifdef RENDERGL_HAS_SHADERS
            if(m_use_shaders)
            {
                SDL_assert(program->inited());
                program->use_program();
                program->update_transform(m_transform_tick, m_transform_matrix.data(), m_shader_read_viewport.data(), (GLfloat)((m_transform_tick / 3) % (65 * 60)) / 65.0f);
            }
#endif

            if(need_logic_op)
            {
                prepareDrawMask();
                glBindTexture(GL_TEXTURE_2D, context.texture->d.mask_texture_id);
                glDrawArrays(GL_TRIANGLES, 0, vertex_attribs.size());
                prepareDrawImage();
            }

            if(context.texture)
                glBindTexture(GL_TEXTURE_2D, context.texture->d.texture_id);
            else if(!m_use_shaders)
                glBindTexture(GL_TEXTURE_2D, 0);

            glDrawArrays(GL_TRIANGLES, 0, vertex_attribs.size());

            if(need_logic_op)
                leaveMaskContext();

            // clear on final pass
            if(pass == num_pass - 1)
                vertex_attribs.clear();
        }
    }

    glDepthMask(GL_TRUE);
}

RenderGL::VertexList& RenderGL::getOrderedDrawVertexList(RenderGL::DrawContext_t context, int depth)
{
    if(context == m_recent_draw_context)
        return m_ordered_draw_queue[{m_recent_draw_context_depth, context}];

    // optimization for buffer-read shaders: only use a single depth per frame
    if((context.program && context.program->get_type() >= GLProgramObject::read_buffer) || (!m_use_logicop && context.program == &m_standard_program && context.texture && context.texture->d.mask_texture_id))
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

bool RenderGL::initRender(const CmdLineSetup_t &setup, SDL_Window *window)
{
    pLogDebug("Init renderer settings...");

    m_window = window;

    if(!AbstractRender_t::init())
        return false;

    if(!initOpenGL(setup)
        || !initDebug()
        || !initShaders()
        || !initState()
        || !initFramebuffers()
        || !initVertexArrays())
    {
        close();
        return false;
    }



    // Clean-up from a possible start-up junk
    clearBuffer();

    updateViewport();

    return true;
}

void RenderGL::close()
{
    RenderGL::clearAllTextures();
    AbstractRender_t::close();

#ifdef RENDERGL_HAS_SHADERS
    m_standard_program.reset();
    m_bitmask_program.reset();
    m_output_program.reset();
    m_program_circle.reset();
    m_program_circle_hole.reset();
    m_program_rect_filled.reset();
    m_program_rect_unfilled.reset();
#endif

    for(int i = BUFFER_GAME; i < BUFFER_MAX; i++)
        destroyFramebuffer((BufferIndex_t)i);

#ifdef RENDERGL_HAS_VBO
    if(m_vertex_buffer[0])
    {
        glDeleteBuffers(s_num_vertex_buffers, m_vertex_buffer);
        for(int i = 0; i < s_num_vertex_buffers; i++)
            m_vertex_buffer[i] = 0;
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

void RenderGL::togglehud()
{
#ifdef RENDERGL_HAS_LOGICOP
    if(m_use_shaders && m_gl_profile != SDL_GL_CONTEXT_PROFILE_ES)
    {
        m_use_logicop = !m_use_logicop;
        PlaySoundMenu(SFX_Raccoon);
    }
    else
#endif
        PlaySoundMenu(SFX_BlockHit);
}

void RenderGL::repaint()
{
#ifdef USE_RENDER_BLOCKING
    if(m_blockRender)
        return;
#endif

    SuperPrintScreenCenter(fmt::format_ne("DEV BUILD - OpenGL {0}.{1} {2}", m_gl_majver, m_gl_minver, get_profile_name(m_gl_profile)), 3, 0, 0.8f, 1.0f, 0.0f);

    std::string feature_string = "Draw: ";
    if(m_use_logicop)
        feature_string += "LogicOp, ";
    if(m_use_shaders)
        feature_string += "shaders, ";
    if(m_client_side_arrays)
        feature_string += "client arr, ";

    feature_string.resize(feature_string.size() - 2);
    if(feature_string.size() == 4)
        feature_string += ": NONE";

    SuperPrintScreenCenter(feature_string, 3, 20, 0.8f, 1.0f, 0.0f);

    feature_string = "FBO: ";
    if(m_buffer_texture[BUFFER_GAME])
        feature_string += "game, ";
    if(m_buffer_texture[BUFFER_FB_READ])
        feature_string += "local fx, ";
    if(m_buffer_texture[BUFFER_PREV_PASS])
        feature_string += "n-pass fx, ";

    feature_string.resize(feature_string.size() - 2);
    if(feature_string.size() == 3)
        feature_string += ": NONE";

    SuperPrintScreenCenter(feature_string, 3, 40, 0.8f, 1.0f, 0.0f);

    flushDrawQueues();

    m_cur_depth = 1;
    glClear(GL_DEPTH_BUFFER_BIT);

#ifdef USE_SCREENSHOTS_AND_RECS
    if(TakeScreen)
    {
        makeShot();
        PlaySoundMenu(SFX_GotItem);
        TakeScreen = false;
    }
#endif

#ifdef USE_SCREENSHOTS_AND_RECS
    processRecorder();
#endif

#ifdef USE_DRAW_BATTERY_STATUS
    drawBatteryStatus();
#endif

    setTargetScreen();

    glDisable(GL_DEPTH_TEST);

    if(m_game_texture_fb && m_game_texture)
    {
        clearBuffer();

        int hardware_w, hardware_h;
        getRenderSize(&hardware_w, &hardware_h);

        // draw screen at correct physical coordinates
        GLshort x1 = m_phys_x;
        GLshort x2 = m_phys_x + m_phys_w;
        GLshort y1 = m_phys_y;
        GLshort y2 = m_phys_y + m_phys_h;

        const Vertex_t vertex_attribs[] =
        {
            {{x1, y1, 0}, {255, 255, 255, 255}, {0.0, 1.0}},
            {{x1, y2, 0}, {255, 255, 255, 255}, {0.0, 0.0}},
            {{x2, y1, 0}, {255, 255, 255, 255}, {1.0, 1.0}},
            {{x2, y2, 0}, {255, 255, 255, 255}, {1.0, 0.0}},
        };

        fillVertexBuffer(vertex_attribs, 4);

        glBindTexture(GL_TEXTURE_2D, m_game_texture);

#ifdef RENDERGL_HAS_SHADERS
        if(m_use_shaders)
        {
            m_output_program.use_program();
            m_output_program.update_transform(m_transform_tick, m_transform_matrix.data(), m_shader_read_viewport.data(), (GLfloat)((m_transform_tick / 3) % (65 * 60)) / 65.0f);
        }
#endif

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    Controls::RenderTouchControls();

    flushDrawQueues();

    glEnable(GL_DEPTH_TEST);
    m_cur_depth = 1;

    SDL_GL_SwapWindow(m_window);

    m_current_frame++;
    if(m_current_frame % 512 == 0)
        refreshDrawQueues();

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
    if(m_recentTargetScreen)
        return;

    int off_x = m_viewport_offset_ignore ? 0 : m_viewport_offset_x;
    int off_y = m_viewport_offset_ignore ? 0 : m_viewport_offset_y;

    // fix offscreen coordinates
    int viewport_x = m_viewport_x;
    int viewport_y = m_viewport_y;
    int viewport_w = m_viewport_w;
    int viewport_h = m_viewport_h;

    if(viewport_x < 0)
    {
        off_x += viewport_x;
        viewport_w += viewport_x;
        viewport_x = 0;
    }

    if(viewport_y < 0)
    {
        off_y += viewport_y;
        viewport_h += viewport_y;
        viewport_y = 0;
    }

    if(viewport_y + viewport_h > ScreenH)
        viewport_h = ScreenH - viewport_y;

    if(viewport_x + viewport_w > ScreenW)
        viewport_w = ScreenW - viewport_x;

    if(m_has_fbo && m_game_texture_fb)
    {
        glViewport(viewport_x, ScreenH - (viewport_y + viewport_h),
            viewport_w, viewport_h);

        if(m_use_shaders)
        {
            m_transform_matrix = {
                2.0f / (float)viewport_w, 0.0f, 0.0f, 0.0f,
                0.0f, -2.0f / (float)viewport_h, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f / (float)(1 << 15), 0.0f,
                -float(viewport_w + off_x + off_x) / (viewport_w), float(viewport_h + off_y + off_y) / (viewport_h), 0.0f, 1.0f,
            };

            m_shader_read_viewport = {
                // multiply
                0.5f * (float)viewport_w / (float)ScreenW,
                0.5f * (float)viewport_h / (float)ScreenH,

                // add
                ((float)(viewport_x + off_x) + 0.5f * (float)viewport_w) / (float)ScreenW,
                ((float)(ScreenH - (viewport_y + viewport_h + off_y)) + 0.5f * (float)viewport_h) / (float)ScreenH,
            };

            m_transform_tick++;
        }
        else
        {
#ifdef RENDERGL_HAS_FIXED_FUNCTION
            glMatrixMode( GL_PROJECTION );
            glLoadIdentity();
#endif

            if(m_gl_profile == SDL_GL_CONTEXT_PROFILE_ES)
            {
#ifdef RENDERGL_HAS_ORTHOF
                glOrthof( off_x, viewport_w + off_x, viewport_h + off_y, off_y, (1 << 15), -(1 << 15));
#endif
            }
            else
            {
#ifdef RENDERGL_HAS_ORTHO
                glOrtho( off_x, viewport_w + off_x, viewport_h + off_y, off_y, (1 << 15), -(1 << 15));
#endif
            }
        }

        return;
    }

    int phys_offset_x = viewport_x * m_phys_w / ScreenW;
    int phys_width = viewport_w * m_phys_w / ScreenW;

    int phys_offset_y = viewport_y * m_phys_h / ScreenH;
    int phys_height = viewport_h * m_phys_h / ScreenH;

    glViewport(m_phys_x + phys_offset_x,
            m_phys_y + m_phys_h - phys_height - phys_offset_y, // relies on fact that m_phys_y is a symmetric border
            phys_width,
            phys_height);

    if(m_use_shaders)
    {
        m_transform_matrix = {
            2.0f / (float)viewport_w, 0.0f, 0.0f, 0.0f,
            0.0f, -2.0f / (float)viewport_h, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f / (float)(1 << 15), 0.0f,
            -(viewport_w + off_x + off_x) / (GLfloat)(viewport_w), (viewport_h + off_y + off_y) / (GLfloat)(viewport_h), 0.0f, 1.0f,
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
#endif

        if(m_gl_profile == SDL_GL_CONTEXT_PROFILE_ES)
        {
#ifdef RENDERGL_HAS_ORTHOF
            glOrthof( off_x, viewport_w + off_x, viewport_h + off_y, off_y, (1 << 15), -(1 << 15));
#endif
        }
        else
        {
#ifdef RENDERGL_HAS_ORTHO
            glOrtho( off_x, viewport_w + off_x, viewport_h + off_y, off_y, (1 << 15), -(1 << 15));
#endif
        }
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

    // if(g_videoSettings.scaleMode == SCALE_DYNAMIC_LINEAR || g_videoSettings.scaleMode == SCALE_DYNAMIC_NEAREST)
    {
        int res_h = hardware_h;
        int res_w = ScreenW * hardware_h / ScreenH;

        if(res_w > hardware_w)
        {
            res_w = hardware_w;
            res_h = ScreenH * res_w / ScreenW;
        }

        m_phys_w = res_w;
        m_phys_h = res_h;
    }

    m_viewport_x = 0;
    m_viewport_y = 0;
    m_viewport_w = ScreenW;
    m_viewport_h = ScreenH;

    m_viewport_offset_x = 0;
    m_viewport_offset_y = 0;
    m_viewport_offset_ignore = false;

    pLogDebug("Phys screen is %d x %d", m_phys_w, m_phys_h);

    m_phys_x = hardware_w / 2 - m_phys_w / 2;
    m_phys_y = hardware_h / 2 - m_phys_h / 2;

    applyViewport();
}

void RenderGL::resetViewport()
{
    updateViewport();
}

void RenderGL::setViewport(int x, int y, int w, int h)
{
    if(x == m_viewport_x && y == m_viewport_y && w == m_viewport_w && h == m_viewport_h)
        return;

    flushDrawQueues();

    m_viewport_x = x;
    m_viewport_y = y;
    m_viewport_w = w;
    m_viewport_h = h;

    applyViewport();
}

void RenderGL::offsetViewport(int x, int y)
{
    if(x == m_viewport_offset_x && y == m_viewport_offset_y)
        return;

    flushDrawQueues();

    m_viewport_offset_x = x;
    m_viewport_offset_y = y;

    applyViewport();
}

void RenderGL::offsetViewportIgnore(bool en)
{
    if(en == m_viewport_offset_ignore)
        return;

    if(m_viewport_offset_x == 0 && m_viewport_offset_y == 0)
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
    *dx = static_cast<int>((static_cast<float>(x) * m_hidpi_x - m_phys_x) * ScreenW / m_phys_w);
    *dy = static_cast<int>((static_cast<float>(y) * m_hidpi_y - m_phys_y) * ScreenH / m_phys_h);
}

void RenderGL::mapFromScreen(int scr_x, int scr_y, int *window_x, int *window_y)
{
    *window_x = ((float)scr_x * m_phys_w / ScreenW + m_phys_x) / m_hidpi_x;
    *window_y = ((float)scr_y * m_phys_h / ScreenH + m_phys_y) / m_hidpi_y;
}

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
#endif

        if(m_gl_profile == SDL_GL_CONTEXT_PROFILE_ES)
        {
#ifdef RENDERGL_HAS_ORTHOF
            glOrthof(0, hardware_w, hardware_h, 0, (1 << 15), -(1 << 15));
#endif
        }
        else
        {
#ifdef RENDERGL_HAS_ORTHO
            glOrtho(0, hardware_w, hardware_h, 0, (1 << 15), -(1 << 15));
#endif
        }
    }
}

void RenderGL::prepareDrawMask()
{
    if(!m_use_logicop)
        return;

#ifdef RENDERGL_HAS_LOGICOP
    // bitwise and
    glEnable(GL_COLOR_LOGIC_OP);
    glLogicOp(GL_AND);
#endif // #ifdef RENDERGL_HAS_LOGICOP
}

void RenderGL::prepareDrawImage()
{
    if(!m_use_logicop)
        return;

#ifdef RENDERGL_HAS_LOGICOP
    // bitwise or
    glDisable(GL_COLOR_LOGIC_OP);
    glEnable(GL_COLOR_LOGIC_OP);
    glLogicOp(GL_OR);
#endif // #ifdef RENDERGL_HAS_LOGICOP
}

void RenderGL::leaveMaskContext()
{
    if(!m_use_logicop)
        return;

#ifdef RENDERGL_HAS_LOGICOP
    // no bitwise op
    glDisable(GL_COLOR_LOGIC_OP);
#endif // #ifdef RENDERGL_HAS_LOGICOP
}

static int s_nextPowerOfTwo(int val)
{
    int power = 1;
    while (power < val)
        power *= 2;
    return power;
}

void RenderGL::loadTexture(StdPicture &target, uint32_t width, uint32_t height, uint8_t *RGBApixels, uint32_t pitch, bool is_mask, uint32_t least_width, uint32_t least_height)
{
    // clear pre-existing errors
    GLuint err;
    while((err = glGetError()) != 0)
        pLogWarning("Render GL: got GL error code %d prior to texture load", (int)err);

    // SDL_Surface *surface;
    // SDL_Texture *texture = nullptr;

    target.d.nOfColors = GL_RGBA;
    target.d.format = GL_RGBA;

    GLuint tex_id;

    int pad_w = s_nextPowerOfTwo(SDL_max(least_width, width));
    int pad_h = s_nextPowerOfTwo(SDL_max(least_height, height));

    uint8_t* padded_pixels = nullptr;
    uint8_t* use_pixels = nullptr;

    pitch /= 4;

    // can't do because of pixel substitution
    // if(pad_w == pitch && height == pad_h)
    // {
        // use_pixels = RGBApixels;
    // }
    // else
    {
        padded_pixels = (uint8_t*) malloc(pad_w * pad_h * 4);
        if(!padded_pixels)
        {
            pLogWarning("Render GL: Failed to allocate padded texture memory");
            if(!is_mask)
            {
                target.d.clear();
                target.inited = false;
            }
            return;
        }

        if(is_mask)
            SDL_memset(padded_pixels, 255, pad_w * pad_h * 4);
        else
            SDL_memset(padded_pixels, 0, pad_w * pad_h * 4);

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
        target.d.clear();
        target.inited = false;
        return;
    }

    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D,
        0,
        GL_RGBA,
        pad_w,
        pad_h,
        0,
        GL_RGBA,
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
        if(target.l.w_scale == 0)
            target.l.w_scale = 1;
        if(target.l.h_scale == 0)
            target.l.h_scale = 1;

        target.l.w_scale /= pad_w;
        target.l.h_scale /= pad_h;
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
        target.d.clear();
        target.inited = false;
        return;
    }

    if(is_mask)
        target.d.mask_texture_id = tex_id;
    else
    {
        target.d.texture_id = tex_id;
        target.inited = true;
    }
    m_textureBank.insert(&target);

#if defined(__APPLE__) && defined(USE_APPLE_X11)
    // SDL_GL_UnbindTexture(texture); // Unbind texture after it got been loaded (otherwise a white screen will happen)
#endif
}

void RenderGL::loadTexture(StdPicture &target, uint32_t width, uint32_t height, uint8_t *RGBApixels, uint32_t pitch)
{
    loadTexture(target, width, height, RGBApixels, pitch, false, width, height);
}

void RenderGL::loadTextureMask(StdPicture &target, uint32_t mask_width, uint32_t mask_height, uint8_t *RGBApixels, uint32_t pitch, uint32_t image_width, uint32_t image_height)
{
    loadTexture(target, mask_width, mask_height, RGBApixels, pitch, true, image_width, image_height);
}

void RenderGL::compileShaders(StdPicture &target)
{
#ifdef RENDERGL_HAS_SHADERS
    if(!m_use_shaders)
        return;

    pLogDebug("Render GL: compiling shader...");

    target.d.shader_program = std::make_shared<GLProgramObject>(
        s_es2_advanced_vert_src,
        target.l.fragmentShaderSource.data()
    );

    if(target.d.shader_program && !target.d.shader_program->inited())
    {
        target.d.shader_program.reset();
        pLogDebug("Render GL: using default program due to failed compile/link...");
    }
#else
    UNUSED(target);
#endif
}

bool RenderGL::textureMaskSupported()
{
    return (m_use_logicop) || (m_use_shaders && m_buffer_texture[1]);
}

bool RenderGL::userShadersSupported()
{
    // should check for GLES 2/3
    return m_use_shaders;
}

void RenderGL::deleteTexture(StdPicture &tx, bool lazyUnload)
{
    if(!tx.inited || !tx.d.texture_id)
    {
        if(!lazyUnload)
            tx.inited = false;
        return;
    }

    auto corpseIt = m_textureBank.find(&tx);
    if(corpseIt != m_textureBank.end())
    {
        m_textureBank.erase(corpseIt);
        return;
    }

    if(tx.d.texture_id)
        glDeleteTextures(1, &tx.d.texture_id);

    if(tx.d.mask_texture_id)
        glDeleteTextures(1, &tx.d.mask_texture_id);

    tx.d.texture_id = 0;
    tx.d.mask_texture_id = 0;

    if(!lazyUnload)
        tx.resetAll();

    tx.d.format = 0;
    tx.d.nOfColors = 0;

    tx.resetColors();
}

void RenderGL::clearAllTextures()
{
    for(StdPicture *tx : m_textureBank)
        deleteTexture(*tx, false);

    m_textureBank.clear();
}

void RenderGL::clearBuffer()
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif

    clearDrawQueues();

    glBindTexture(GL_TEXTURE_2D, 0);
    glClear(GL_COLOR_BUFFER_BIT);
}

void RenderGL::renderRect(int x, int y, int w, int h, float red, float green, float blue, float alpha, bool filled)
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif

    GLshort x1 = x;
    GLshort x2 = x + w;
    GLshort y1 = y;
    GLshort y2 = y + h;

    // want interpolated value to be <= 0 for first two pixels, >= 1 for last two pixels
    float u1 = -2.0f / w;
    float u2 = (w + 2.0f) / w;

    float v1 = -2.0f / h;
    float v2 = (h + 2.0f) / h;

    DrawContext_t context = {nullptr, (filled) ? &m_program_rect_filled : &m_program_rect_unfilled};

    auto& vertex_attribs = ((alpha == 1.0f) ? m_unordered_draw_queue[context] : getOrderedDrawVertexList(context, m_cur_depth)).vertices;

    vertex_attribs.push_back({{x1, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v1}});
    vertex_attribs.push_back({{x1, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v2}});
    vertex_attribs.push_back({{x2, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v1}});
    vertex_attribs.push_back({{x1, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v2}});
    vertex_attribs.push_back({{x2, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v1}});
    vertex_attribs.push_back({{x2, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v2}});

    m_cur_depth++;
}

void RenderGL::renderRectBR(int _left, int _top, int _right, int _bottom, float red, float green, float blue, float alpha)
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif

    renderRect(_left, _top, _right - _left, _bottom - _top, red, green, blue, alpha);
}

void RenderGL::renderCircle(int cx, int cy, int radius, float red, float green, float blue, float alpha, bool filled)
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif
    UNUSED(filled);

    if(radius <= 0)
        return; // Nothing to draw

    DrawContext_t context = {nullptr, &m_program_circle};

    auto& vertex_attribs = ((alpha == 1.0f) ? m_unordered_draw_queue[context] : getOrderedDrawVertexList(context, m_cur_depth)).vertices;

    if(m_use_shaders)
    {
        GLshort x1 = cx - radius;
        GLshort x2 = cx + radius;
        GLshort y1 = cy - radius;
        GLshort y2 = cy + radius;

        vertex_attribs.push_back({{x1, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {0.0, 0.0}});
        vertex_attribs.push_back({{x1, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {0.0, 1.0}});
        vertex_attribs.push_back({{x2, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {1.0, 0.0}});
        vertex_attribs.push_back({{x1, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {0.0, 1.0}});
        vertex_attribs.push_back({{x2, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {1.0, 0.0}});
        vertex_attribs.push_back({{x2, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {1.0, 1.0}});
    }
    else
    {
        // make a full circle poly here
        const int verts = 20;
        const float two_pi = (float)(2 * 3.1415926535897932384626433832795);

        GLshort cx_s = cx;
        GLshort cy_s = cy;

        for(int i = 0; i < verts + 1; i++)
        {
            float theta = i * two_pi / verts;
            GLshort x = cx_s + (GLshort)roundf(radius * cosf(theta));
            GLshort y = cy_s + (GLshort)roundf(radius * sinf(theta));

            if(i != 0)
            {
                vertex_attribs.push_back({{x, y, m_cur_depth}, F_TO_B(red, green, blue, alpha), {0.0, 0.0}});
                vertex_attribs.push_back({{cx_s, cy_s, m_cur_depth}, F_TO_B(red, green, blue, alpha), {0.0, 0.0}});
            }
            if(i != verts)
                vertex_attribs.push_back({{x, y, m_cur_depth}, F_TO_B(red, green, blue, alpha), {0.0, 0.0}});
        }
    }

    m_cur_depth++;
}

void RenderGL::renderCircleHole(int cx, int cy, int radius, float red, float green, float blue, float alpha)
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif

    if(radius <= 0)
        return; // Nothing to draw

    DrawContext_t context = {nullptr, &m_program_circle_hole};

    auto& vertex_attribs = ((alpha == 1.0f) ? m_unordered_draw_queue[context] : getOrderedDrawVertexList(context, m_cur_depth)).vertices;

    if(m_use_shaders)
    {
        GLshort x1 = cx - radius;
        GLshort x2 = cx + radius;
        GLshort y1 = cy - radius;
        GLshort y2 = cy + radius;

        vertex_attribs.push_back({{x1, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {0.0, 0.0}});
        vertex_attribs.push_back({{x1, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {0.0, 1.0}});
        vertex_attribs.push_back({{x2, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {1.0, 0.0}});
        vertex_attribs.push_back({{x1, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {0.0, 1.0}});
        vertex_attribs.push_back({{x2, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {1.0, 0.0}});
        vertex_attribs.push_back({{x2, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {1.0, 1.0}});
    }
    else
    {
        // make a full circle poly here
    }

    m_cur_depth++;
}


void RenderGL::renderTextureScaleEx(double xDstD, double yDstD, double wDstD, double hDstD,
                                       StdPicture &tx,
                                       int xSrc, int ySrc,
                                       int wSrc, int hSrc,
                                       double rotateAngle, FPoint_t *center, unsigned int flip,
                                       float red, float green, float blue, float alpha)
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

    GLshort x1 = xDst;
    GLshort x2 = xDst + wDst;
    GLshort y1 = yDst;
    GLshort y2 = yDst + hDst;

    float u1 = tx.l.w_scale * xSrc;
    float u2 = tx.l.w_scale * (xSrc + wSrc);
    float v1 = tx.l.h_scale * ySrc;
    float v2 = tx.l.h_scale * (ySrc + hSrc);

    if(flip & X_FLIP_HORIZONTAL)
        std::swap(u1, u2);

    if(flip & X_FLIP_VERTICAL)
        std::swap(v1, v2);

    DrawContext_t context = {&tx, tx.d.shader_program ? tx.d.shader_program.get() : &m_standard_program};

    auto& vertex_attribs = ((tx.d.use_depth_test && alpha == 1.0f && !tx.d.shader_program) ? m_unordered_draw_queue[context] : getOrderedDrawVertexList(context, m_cur_depth)).vertices;

    vertex_attribs.push_back({{x1, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v1}});
    vertex_attribs.push_back({{x1, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v2}});
    vertex_attribs.push_back({{x2, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v1}});
    vertex_attribs.push_back({{x1, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v2}});
    vertex_attribs.push_back({{x2, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v1}});
    vertex_attribs.push_back({{x2, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v2}});

    m_cur_depth++;
}

void RenderGL::renderTextureScale(double xDst, double yDst, double wDst, double hDst,
                                     StdPicture &tx,
                                     float red, float green, float blue, float alpha)
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

    GLshort x1 = xDst;
    GLshort x2 = xDst + wDst;
    GLshort y1 = yDst;
    GLshort y2 = yDst + hDst;

    float u1 = tx.l.w_scale * 0;
    float u2 = tx.l.w_scale * (tx.w);
    float v1 = tx.l.h_scale * 0;
    float v2 = tx.l.h_scale * (tx.h);

    DrawContext_t context = {&tx, tx.d.shader_program ? tx.d.shader_program.get() : &m_standard_program};

    auto& vertex_attribs = ((tx.d.use_depth_test && alpha == 1.0f && !tx.d.shader_program) ? m_unordered_draw_queue[context] : getOrderedDrawVertexList(context, m_cur_depth)).vertices;

    vertex_attribs.push_back({{x1, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v1}});
    vertex_attribs.push_back({{x1, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v2}});
    vertex_attribs.push_back({{x2, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v1}});
    vertex_attribs.push_back({{x1, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v2}});
    vertex_attribs.push_back({{x2, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v1}});
    vertex_attribs.push_back({{x2, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v2}});

    m_cur_depth++;
}

void RenderGL::renderTexture(double xDstD, double yDstD, double wDstD, double hDstD,
                                StdPicture &tx,
                                int xSrc, int ySrc,
                                float red, float green, float blue, float alpha)
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

    GLshort x1 = xDst;
    GLshort x2 = xDst + wDst;
    GLshort y1 = yDst;
    GLshort y2 = yDst + hDst;

    float u1 = tx.l.w_scale * xSrc;
    float u2 = tx.l.w_scale * (xSrc + wDst);
    float v1 = tx.l.h_scale * ySrc;
    float v2 = tx.l.h_scale * (ySrc + hDst);

    DrawContext_t context = {&tx, tx.d.shader_program ? tx.d.shader_program.get() : &m_standard_program};

    auto& vertex_attribs = ((tx.d.use_depth_test && alpha == 1.0f && !tx.d.shader_program) ? m_unordered_draw_queue[context] : getOrderedDrawVertexList(context, m_cur_depth)).vertices;

    vertex_attribs.push_back({{x1, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v1}});
    vertex_attribs.push_back({{x1, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v2}});
    vertex_attribs.push_back({{x2, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v1}});
    vertex_attribs.push_back({{x1, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v2}});
    vertex_attribs.push_back({{x2, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v1}});
    vertex_attribs.push_back({{x2, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v2}});

    m_cur_depth++;
}

void RenderGL::renderTextureFL(double xDstD, double yDstD, double wDstD, double hDstD,
                                  StdPicture &tx,
                                  int xSrc, int ySrc,
                                  double rotateAngle, FPoint_t *center, unsigned int flip,
                                  float red, float green, float blue, float alpha)
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

    float cx = center ? center->x : xDst + wDst / 2.0f;
    float cy = center ? center->y : yDst + hDst / 2.0f;

    // glPushMatrix();

    // glTranslatef(xDst + cx, yDst + cy, 0);
    // glRotatef(rotateAngle, 0, 0, 1);

    GLshort x1 = -cx;
    GLshort x2 = -cx + wDst;
    GLshort y1 = -cy;
    GLshort y2 = -cy + hDst;

    float u1 = tx.l.w_scale * xSrc;
    float u2 = tx.l.w_scale * (xSrc + wDst);
    float v1 = tx.l.h_scale * ySrc;
    float v2 = tx.l.h_scale * (ySrc + hDst);

    if(flip & X_FLIP_HORIZONTAL)
        std::swap(u1, u2);

    if(flip & X_FLIP_VERTICAL)
        std::swap(v1, v2);

    DrawContext_t context = {&tx, tx.d.shader_program ? tx.d.shader_program.get() : &m_standard_program};

    auto& vertex_attribs = ((tx.d.use_depth_test && alpha == 1.0f && !tx.d.shader_program) ? m_unordered_draw_queue[context] : getOrderedDrawVertexList(context, m_cur_depth)).vertices;

    vertex_attribs.push_back({{x1, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v1}});
    vertex_attribs.push_back({{x1, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v2}});
    vertex_attribs.push_back({{x2, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v1}});
    vertex_attribs.push_back({{x1, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v2}});
    vertex_attribs.push_back({{x2, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v1}});
    vertex_attribs.push_back({{x2, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v2}});

    m_cur_depth++;
}

void RenderGL::renderTexture(float xDst, float yDst,
                                StdPicture &tx,
                                float red, float green, float blue, float alpha)
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

    GLshort x1 = xDst;
    GLshort x2 = xDst + tx.w;
    GLshort y1 = yDst;
    GLshort y2 = yDst + tx.h;

    float u1 = tx.l.w_scale * 0;
    float u2 = tx.l.w_scale * (tx.w);
    float v1 = tx.l.h_scale * 0;
    float v2 = tx.l.h_scale * (tx.h);

    DrawContext_t context = {&tx, tx.d.shader_program ? tx.d.shader_program.get() : &m_standard_program};

    auto& vertex_attribs = ((tx.d.use_depth_test && alpha == 1.0f && !tx.d.shader_program) ? m_unordered_draw_queue[context] : getOrderedDrawVertexList(context, m_cur_depth)).vertices;

    vertex_attribs.push_back({{x1, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v1}});
    vertex_attribs.push_back({{x1, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v2}});
    vertex_attribs.push_back({{x2, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v1}});
    vertex_attribs.push_back({{x1, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u1, v2}});
    vertex_attribs.push_back({{x2, y1, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v1}});
    vertex_attribs.push_back({{x2, y2, m_cur_depth}, F_TO_B(red, green, blue, alpha), {u2, v2}});

    m_cur_depth++;
}

void RenderGL::getScreenPixels(int x, int y, int w, int h, unsigned char *pixels)
{
    glFlush();

    int phys_x, phys_y;

    mapFromScreen(x, y, &phys_x, &phys_y);

    int phys_w = w * m_phys_w / ScreenW;
    int phys_h = h * m_phys_h / ScreenH;

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
        int phys_r_ind = r * m_phys_h / ScreenH;

        // vertical flip from OpenGL to image
        int phys_r = phys_r_max - phys_r_ind;

        for(int c = 0; c < w; c++)
        {
            int phys_c = c * m_phys_w / ScreenW;

            pixels[(r * w + c) * 3 + 0] = phys_pixels[(phys_r * phys_w + phys_c) * 3 + 0];
            pixels[(r * w + c) * 3 + 1] = phys_pixels[(phys_r * phys_w + phys_c) * 3 + 1];
            pixels[(r * w + c) * 3 + 2] = phys_pixels[(phys_r * phys_w + phys_c) * 3 + 2];
        }
    }

    free(phys_pixels);
}

void RenderGL::getScreenPixelsRGBA(int x, int y, int w, int h, unsigned char *pixels)
{
    glFlush();

    int phys_x, phys_y, phys_w, phys_h;

#ifdef RENDERGL_HAS_FBO
    GLint prev_fb = -1;

    if(m_game_texture_fb && m_game_texture)
    {
        phys_x = x;
        phys_y = y;
        phys_w = w;
        phys_h = h;

        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fb);
        glBindFramebuffer(GL_FRAMEBUFFER, m_game_texture_fb);
    }
    else
#endif
    {
        mapFromScreen(x, y, &phys_x, &phys_y);

        phys_w = w * m_phys_w / ScreenW;
        phys_h = h * m_phys_h / ScreenH;
    }

    // allocate buffer for screen-space pixels
    uint8_t* phys_pixels = (uint8_t*)malloc(phys_w * phys_h * 4);
    if(!phys_pixels)
        return;

    // read screen-space pixels
    glReadPixels(phys_x, phys_y, phys_w, phys_h,
        GL_RGBA, GL_UNSIGNED_BYTE, phys_pixels);

#ifdef RENDERGL_HAS_FBO
    if(prev_fb != -1)
        glBindFramebuffer(GL_FRAMEBUFFER, prev_fb);
#endif

    // rescale and move to target
    for(int r = 0; r < h; r++)
    {
        int phys_r_max = phys_h - 1;
        int phys_r_ind = r * phys_h / h;

        // vertical flip from OpenGL to image
        int phys_r = phys_r_max - phys_r_ind;

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
