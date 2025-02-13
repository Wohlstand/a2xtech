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

#include <SDL2/SDL_version.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_hints.h>

#include <FreeImageLite.h>
#include <Logger/logger.h>
#include <Utils/maths.h>

#include "render_sdl.h"
#include "config.h"

#include "core/window.h"
#include "core/render.h"

#include "main/cheat_code.h"

#include "sdl_proxy/sdl_stdinc.h"
#include <fmt_format_ne.h>

#include "graphics.h"
#include "controls.h"
#include "sound.h"

#ifndef UNUSED
#define UNUSED(x) (void)x
#endif

// Workaround for older SDL versions that lacks the floating-point based rects and points
#if SDL_COMPILEDVERSION < SDL_VERSIONNUM(2, 0, 10)
#define XTECH_SDL_NO_RECTF_SUPPORT
#define SDL_RenderCopyF SDL_RenderCopy
#define SDL_RenderCopyExF SDL_RenderCopyEx
#endif

static inline uint32_t pow2roundup(uint32_t x)
{
    if(x == 0)
        return 0;

    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

static inline int32_t pow2roundup(int32_t x)
{
    if(x < 0)
        return 0;

    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

RenderSDL::RenderSDL() :
    AbstractRender_t()
{}

RenderSDL::~RenderSDL()
{
    if(m_window)
        RenderSDL::close();
}

unsigned int RenderSDL::SDL_InitFlags()
{
    return 0;
}

bool RenderSDL::isWorking()
{
    return m_gRenderer && (m_tBuffer || m_tBufferDisabled);
}

bool RenderSDL::hasFrameBuffer()
{
    return m_tBuffer && !m_tBufferDisabled;
}

bool RenderSDL::initRender(SDL_Window *window)
{
    pLogDebug("Render SDL: Init...");

    if(!AbstractRender_t::init())
        return false;

    m_window = window;

    Uint32 renderFlags = 0;

    int numRenders = SDL_GetNumRenderDrivers();
    SDL_RendererInfo info;
    for(int i = 0; i < numRenders; ++i)
    {
        SDL_GetRenderDriverInfo(i, &info);
        pLogDebug("Render SDL: Render device: %s, flags: %u, max-w: %u, max-h: %u",
                  info.name,
                  info.flags,
                  info.max_texture_width,
                  info.max_texture_height);
    }

    switch(g_config.render_mode)
    {
    case Config_t::RENDER_ACCELERATED_SDL:
    default:
        if(g_config.render_vsync)
        {
            renderFlags = SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC;
            g_config.render_mode.obtained = Config_t::RENDER_ACCELERATED_SDL;
            pLogDebug("Render SDL: Using accelerated rendering with a vertical synchronization");
            m_gRenderer = SDL_CreateRenderer(window, -1, renderFlags | SDL_RENDERER_TARGETTEXTURE); // Try to make renderer
            if(m_gRenderer)
                break; // All okay
            pLogWarning("Failed to initialize V-Synced renderer, trying to create accelerated renderer...");
        }

        // continue

        renderFlags = SDL_RENDERER_ACCELERATED;
        g_config.render_mode.obtained = Config_t::RENDER_ACCELERATED_SDL;
        pLogDebug("Render SDL: Using accelerated rendering");
        m_gRenderer = SDL_CreateRenderer(window, -1, renderFlags | SDL_RENDERER_TARGETTEXTURE); // Try to make renderer
        if(m_gRenderer)
            break; // All okay
        pLogWarning("Failed to initialize accelerated renderer, trying to create a software renderer...");

        // fallthrough
    case Config_t::RENDER_SOFTWARE:
        renderFlags = SDL_RENDERER_SOFTWARE;
        g_config.render_mode.obtained = Config_t::RENDER_SOFTWARE;
        pLogDebug("Render SDL: Using software rendering");
        m_gRenderer = SDL_CreateRenderer(window, -1, renderFlags | SDL_RENDERER_TARGETTEXTURE); // Try to make renderer
        if(m_gRenderer)
            break; // All okay

        pLogCritical("Render SDL: Unable to create renderer!");
        return false;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    SDL_RendererInfo ri;
    SDL_GetRendererInfo(m_gRenderer, &ri);
    m_maxTextureWidth = ri.max_texture_width;
    m_maxTextureHeight = ri.max_texture_height;

    m_tBuffer = SDL_CreateTexture(m_gRenderer,
                                  SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_TARGET,
                                  ScaleWidth, ScaleHeight);

    if(!m_tBuffer)
    {
        pLogWarning("Render SDL: Failed to create the normal texture render buffer: %s, trying to create a power-2 texture...", SDL_GetError());
        m_pow2 = true;
        m_tBuffer = SDL_CreateTexture(m_gRenderer,
                                      SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_TARGET,
                                      pow2roundup(ScaleWidth), pow2roundup(ScaleHeight));
    }

    if(!m_tBuffer)
    {
        m_pow2 = false;
        pLogWarning("Render SDL: Unable to create texture render buffer: %s", SDL_GetError());
        pLogDebug("Render SDL: Continue without of render to texture. The ability to resize the window will be disabled.");
        SDL_SetWindowResizable(window, SDL_FALSE);
        m_tBufferDisabled = true;
    }

    // Clean-up from a possible start-up junk
    clearBuffer();

    setTargetTexture();
    SDL_SetRenderDrawBlendMode(m_gRenderer, SDL_BLENDMODE_BLEND);

    updateViewport();

    // Clean-up the texture buffer from the same start-up junk
    clearBuffer();

    setTargetScreen();

    repaint();

    return true;
}

void RenderSDL::close()
{
    RenderSDL::clearAllTextures();
    AbstractRender_t::close();

    if(m_tBuffer)
        SDL_DestroyTexture(m_tBuffer);
    m_tBuffer = nullptr;

    if(m_gRenderer)
        SDL_DestroyRenderer(m_gRenderer);
    m_gRenderer = nullptr;
}

void RenderSDL::repaint()
{
#ifdef USE_RENDER_BLOCKING
    if(m_blockRender)
        return;
#endif

    if(XRender::g_BitmaskTexturePresent)
        SuperPrintScreenCenter("Bitmasks using GIFs2PNG in SDL", 5, 2, XTColorF(1.0f, 0.7f, 0.5f));
    else if(g_ForceBitmaskMerge)
        SuperPrintScreenCenter("GIFs2PNG always simulated in SDL", 5, 2, XTColorF(1.0f, 0.7f, 0.5f));

    int w, h, off_x, off_y, wDst, hDst;

#ifdef USE_SCREENSHOTS_AND_RECS
    if(TakeScreen)
    {
        makeShot();
        PlaySoundMenu(SFX_GotItem);
        TakeScreen = false;
    }
#endif

    setTargetScreen();

#ifdef PGE_ENABLE_VIDEO_REC
    processRecorder();
#endif

    // Get the size of surface where to draw the scene
    SDL_GetRendererOutputSize(m_gRenderer, &w, &h);

    if(m_tBufferDisabled) /* Render-to-texture is not supported, draw the scene on the screen */
    {
        Controls::RenderTouchControls();

        flushRenderQueue();

        SDL_RenderPresent(m_gRenderer);
        return;
    }

    flushRenderQueue();

    // Calculate the size difference factor
    wDst = int(m_scale_x * ScaleWidth);
    hDst = int(m_scale_y * ScaleHeight);

    // Align the rendering scene to the center of screen
    off_x = (w - wDst) / 2;
    off_y = (h - hDst) / 2;

    SDL_SetRenderDrawColor(m_gRenderer, 0, 0, 0, 255);
    SDL_RenderClear(m_gRenderer);

    SDL_Rect destRect = {off_x, off_y, wDst, hDst};
    SDL_Rect sourceRect = {0, 0, ScaleWidth, ScaleHeight};

    SDL_SetTextureColorMod(m_tBuffer, 255, 255, 255);
    SDL_SetTextureAlphaMod(m_tBuffer, 255);
    SDL_RenderCopyEx(m_gRenderer, m_tBuffer, &sourceRect, &destRect, 0.0, nullptr, SDL_FLIP_NONE);

    Controls::RenderTouchControls();

    flushRenderQueue();

    SDL_RenderPresent(m_gRenderer);

    m_recent_draw_plane = 0;
}

void RenderSDL::updateViewport()
{
    flushRenderQueue();

    int   render_w, render_h;

    getRenderSize(&render_w, &render_h);

    D_pLogDebug("Updated render size: %d x %d", render_w, render_h);

    // quickly update the HiDPI scaling factor
    int window_w, window_h;
    XWindow::getWindowSize(&window_w, &window_h);
    m_hidpi_x = (float)render_w / (float)window_w;
    m_hidpi_y = (float)render_h / (float)window_h;

    float scale_x = (float)render_w / XRender::TargetW;
    float scale_y = (float)render_h / XRender::TargetH;

    float scale = SDL_min(scale_x, scale_y);

    if(g_config.scale_mode == Config_t::SCALE_FIXED_05X && scale > 0.5f)
        scale = 0.5f;
    if(g_config.scale_mode == Config_t::SCALE_DYNAMIC_INTEGER && scale > 1.f)
        scale = std::floor(scale);
    if(g_config.scale_mode == Config_t::SCALE_FIXED_1X && scale > 1.f)
        scale = 1.f;
    if(g_config.scale_mode == Config_t::SCALE_FIXED_2X && scale > 2.f)
        scale = 2.f;

    int game_w = scale * XRender::TargetW;
    int game_h = scale * XRender::TargetH;

    m_scale_x = scale;
    m_scale_y = scale;
    m_viewport_scale_x = scale;
    m_viewport_scale_y = scale;

    m_viewport_offset_x = 0;
    m_viewport_offset_y = 0;
    m_viewport_offset_x_cur = 0;
    m_viewport_offset_y_cur = 0;
    m_viewport_offset_ignore = false;

    m_offset_x = (render_w - game_w) / 2;
    m_offset_y = (render_h - game_h) / 2;

    m_viewport_x = 0;
    m_viewport_y = 0;
    m_viewport_w = XRender::TargetW;
    m_viewport_h = XRender::TargetH;

    // update render targets
    if(ScaleWidth != XRender::TargetW || ScaleHeight != XRender::TargetH || m_current_scale_mode != g_config.scale_mode)
    {
#ifdef PGE_ENABLE_VIDEO_REC
        // invalidates GIF recorder handle
        if(recordInProcess())
            toggleGifRecorder();
#endif

        // update video settings
        if(g_config.scale_mode == Config_t::SCALE_DYNAMIC_LINEAR || scale < 0.5f)
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        else
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

        if(!m_tBufferDisabled)
        {
            SDL_DestroyTexture(m_tBuffer);

            m_tBuffer = SDL_CreateTexture(m_gRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                                          m_pow2 ? pow2roundup(XRender::TargetW) : XRender::TargetW,
                                          m_pow2 ? pow2roundup(XRender::TargetH) : XRender::TargetH);
            SDL_SetRenderTarget(m_gRenderer, m_tBuffer);
        }

        // reset scaling setting for images loaded later
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

        ScaleWidth = XRender::TargetW;
        ScaleHeight = XRender::TargetH;
        m_current_scale_mode = g_config.scale_mode;
    }

#if SDL_COMPILEDVERSION >= SDL_VERSIONNUM(2, 0, 18)
    SDL_RenderSetVSync(m_gRenderer, g_config.render_vsync);
#endif
}

void RenderSDL::resetViewport()
{
    if(m_viewport_x == 0 && m_viewport_y == 0 && m_viewport_w == XRender::TargetW && m_viewport_h == XRender::TargetH)
        return;

    flushRenderQueue();

    // FIXME: Clarify the version of SDL2 with the buggy viewport
//#if SDL_COMPILEDVERSION < SDL_VERSIONNUM(2, 0, 22)
    // set to an alt viewport as a workaround for SDL bug (doesn't allow resizing viewport without changing position)
    SDL_Rect altViewport = {m_viewport_x + 1, m_viewport_y + 1, 1, 1};
    SDL_RenderSetViewport(m_gRenderer, &altViewport);
//#endif

    SDL_RenderSetViewport(m_gRenderer, nullptr);

    m_viewport_x = 0;
    m_viewport_y = 0;
    m_viewport_w = XRender::TargetW;
    m_viewport_h = XRender::TargetH;
}

void RenderSDL::setViewport(int x, int y, int w, int h)
{
    if(m_viewport_x == x && m_viewport_y == y && m_viewport_w == w && m_viewport_h == h)
        return;

    flushRenderQueue();

    SDL_Rect topLeftViewport;

    // FIXME: Clarify the version of SDL2 with the buggy viewport
//#if SDL_COMPILEDVERSION < SDL_VERSIONNUM(2, 0, 22)
    // set to an alt viewport as a workaround for SDL bug (doesn't allow resizing viewport without changing position)
    topLeftViewport = {m_viewport_x + 1, m_viewport_y + 1, 1, 1};
    SDL_RenderSetViewport(m_gRenderer, &topLeftViewport);
//#endif

    topLeftViewport = {x, y, w, h};
    SDL_RenderSetViewport(m_gRenderer, &topLeftViewport);

    m_viewport_x = x;
    m_viewport_y = y;
    m_viewport_w = w;
    m_viewport_h = h;
}

void RenderSDL::offsetViewport(int x, int y)
{
    if(m_viewport_offset_x != x || m_viewport_offset_y != y)
    {
        m_viewport_offset_x_cur = x;
        m_viewport_offset_y_cur = y;
        m_viewport_offset_x = m_viewport_offset_ignore ? 0 : m_viewport_offset_x_cur;
        m_viewport_offset_y = m_viewport_offset_ignore ? 0 : m_viewport_offset_y_cur;
    }
}

void RenderSDL::offsetViewportIgnore(bool en)
{
    if(m_viewport_offset_ignore != en)
    {
        m_viewport_offset_x = en ? 0 : m_viewport_offset_x_cur;
        m_viewport_offset_y = en ? 0 : m_viewport_offset_y_cur;
    }
    m_viewport_offset_ignore = en;
}

void RenderSDL::getRenderSize(int* w, int* h)
{
    // make sure we're operating on default target (same as SDL)
    SDL_Texture* saved_target = SDL_GetRenderTarget(m_gRenderer);
    if(saved_target)
        SDL_SetRenderTarget(m_gRenderer, NULL);

    SDL_GetRendererOutputSize(m_gRenderer, w, h);

    if (saved_target)
        SDL_SetRenderTarget(m_gRenderer, saved_target);
}

void RenderSDL::mapToScreen(int x, int y, int *dx, int *dy)
{
    *dx = static_cast<int>((static_cast<float>(x) * m_hidpi_x - m_offset_x) / m_viewport_scale_x);
    *dy = static_cast<int>((static_cast<float>(y) * m_hidpi_y - m_offset_y) / m_viewport_scale_y);
}

void RenderSDL::mapFromScreen(int scr_x, int scr_y, int *window_x, int *window_y)
{
    *window_x = ((float)scr_x * m_viewport_scale_x + m_offset_x) / m_hidpi_x;
    *window_y = ((float)scr_y * m_viewport_scale_y + m_offset_y) / m_hidpi_y;
}

void RenderSDL::setTargetTexture()
{
    if(m_tBufferDisabled || m_recentTarget == m_tBuffer)
        return;

    flushRenderQueue();

    SDL_SetRenderTarget(m_gRenderer, m_tBuffer);
    m_recentTarget = m_tBuffer;
}

void RenderSDL::setTargetScreen()
{
    if(m_tBufferDisabled || m_recentTarget == nullptr)
        return;

    flushRenderQueue();

    SDL_SetRenderTarget(m_gRenderer, nullptr);
    m_recentTarget = nullptr;
}

void RenderSDL::setDrawPlane(uint8_t plane)
{
    m_recent_draw_plane = plane;
}

void RenderSDL::loadTextureInternal(StdPicture &target, uint32_t width, uint32_t height, uint8_t *RGBApixels, uint32_t pitch, uint32_t mask_width, uint32_t mask_height)
{
    UNUSED(mask_width);
    UNUSED(mask_height);

    SDL_Surface *surface;
    SDL_Texture *texture = nullptr;

    surface = SDL_CreateRGBSurfaceFrom(RGBApixels,
                                       static_cast<int>(width),
                                       static_cast<int>(height),
                                       32,
                                       static_cast<int>(pitch),
                                       FI_RGBA_RED_MASK,
                                       FI_RGBA_GREEN_MASK,
                                       FI_RGBA_BLUE_MASK,
                                       FI_RGBA_ALPHA_MASK);

    if(surface)
    {
textureTryAgain:
        if(m_pow2)
        {
            uint32_t newW = pow2roundup(width);
            uint32_t newH = pow2roundup(height);
            pLogDebug("Render SDL: Converting surface into Power-2...");
            SDL_Surface *newSurface = SDL_CreateRGBSurfaceWithFormat(0, newW, newH, 32, SDL_PIXELFORMAT_ARGB8888);
            if(newSurface)
            {
                SDL_Rect rect = {0, 0, (int)width, (int)height};
                SDL_LowerBlit(surface, &rect, newSurface, &rect);
                SDL_FreeSurface(surface);
                surface = newSurface;
            }
        }

        texture = SDL_CreateTextureFromSurface(m_gRenderer, surface);

        if(!m_pow2 && !texture) // Try to re-make texture again
        {
            pLogWarning("Render SDL: Failed to load texture (%s), trying to turn on the Power-2 mode...", SDL_GetError());
            m_pow2 = true;
            goto textureTryAgain;
        }
    }

    SDL_FreeSurface(surface);

    if(!texture)
    {
        pLogWarning("Render SDL: Failed to load texture! (%s)", SDL_GetError());
        target.inited = false;
        return;
    }

    target.d.texture = texture;

    target.d.nOfColors = GL_RGBA;
    target.d.format = GL_BGRA;

    target.d.w_scale = static_cast<float>(width) / target.w;
    target.d.h_scale = static_cast<float>(height) / target.h;

    m_loadedPictures.insert(&target);
    D_pLogDebug("RenderSDL: loading texture at %p, new texture count %d...", &target, (int)m_loadedPictures.size());

    target.inited = true;

#if defined(__APPLE__) && defined(USE_APPLE_X11)
    SDL_GL_UnbindTexture(texture); // Unbind texture after it got been loaded (otherwise a white screen will happen)
#endif
}

void RenderSDL::unloadTexture(StdPicture &tx)
{
    auto corpseIt = m_loadedPictures.find(&tx);
    if(corpseIt != m_loadedPictures.end())
        m_loadedPictures.erase(corpseIt);

    D_pLogDebug("RenderSDL: unloading texture at %p, new texture count %d...", &tx, (int)m_loadedPictures.size());

    if(tx.d.hasTexture())
        SDL_DestroyTexture(tx.d.texture);

    tx.d = StdPictureData();

    if(!tx.l.canLoad())
        static_cast<StdPicture_Sub&>(tx) = StdPicture_Sub();

    return;
}

void RenderSDL::clearAllTextures()
{
    for(StdPicture *tx : m_loadedPictures)
    {
        D_pLogDebug("RenderSDL: unloading texture at %p on clearAllTextures()", tx);

        if(tx->d.hasTexture())
            SDL_DestroyTexture(tx->d.texture);

        tx->d = StdPictureData();

        if(!tx->l.canLoad())
            static_cast<StdPicture_Sub&>(*tx) = StdPicture_Sub();
    }

    m_loadedPictures.clear();
}

void RenderSDL::clearBuffer()
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif
    SDL_SetRenderDrawColor(m_gRenderer, 0, 0, 0, 255);
    SDL_RenderClear(m_gRenderer);

    m_render_queue.clear();
}

void RenderSDL::flushRenderQueue()
{
    if(!m_render_queue.size)
        return;

    m_render_queue.sort();

    for(uint32_t i : m_render_queue.indices)
        execute(m_render_queue.ops[i & 0xFFFF]);

    m_render_queue.clear();
}

void RenderSDL::execute(const RenderOp& op)
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif

    switch(op.type)
    {
    case RenderOp::Type::rect:
    {
        SDL_Rect aRect = {op.xDst, op.yDst, op.wDst, op.hDst};
        SDL_SetRenderDrawColor(m_gRenderer, op.color.r, op.color.g, op.color.b, op.color.a);

        if(op.traits & RenderOp::Traits::filled)
            SDL_RenderFillRect(m_gRenderer, &aRect);
        else
            SDL_RenderDrawRect(m_gRenderer, &aRect);

        break;
    }

    case RenderOp::Type::circle:
    {
        int radius = op.radius();

        SDL_SetRenderDrawColor(m_gRenderer, op.color.r, op.color.g, op.color.b, op.color.a);

        int dy = 1;
        do //for(double dy = 1; dy <= radius; dy += 1.0)
        {
            int dx = std::floor(std::sqrt((2 * radius * dy) - (dy * dy)));
            SDL_RenderDrawLine(m_gRenderer,
                               int(op.xDst - dx),
                               int(op.yDst + dy - radius),
                               int(op.xDst + dx),
                               int(op.yDst + dy - radius));

            if(dy < radius) // Don't cross lines
            {
                SDL_RenderDrawLine(m_gRenderer,
                                   int(op.xDst - dx),
                                   int(op.yDst - dy + radius),
                                   int(op.xDst + dx),
                                   int(op.yDst - dy + radius));
            }

            dy += 1;
        } while(dy <= radius);

        break;
    }

    case RenderOp::Type::circle_hole:
    {
        int radius = op.radius();

        SDL_SetRenderDrawColor(m_gRenderer, op.color.r, op.color.g, op.color.b, op.color.a);

        int dy = 1;
        do //for(double dy = 1; dy <= radius; dy += 1.0)
        {
            int dx = std::floor(std::sqrt((2 * radius * dy) - (dy * dy)));

            SDL_RenderDrawLine(m_gRenderer,
                               int(op.xDst - radius),
                               int(op.yDst + dy - radius),
                               int(op.xDst - dx),
                               int(op.yDst + dy - radius));

            SDL_RenderDrawLine(m_gRenderer,
                               int(op.xDst + dx),
                               int(op.yDst + dy - radius),
                               int(op.xDst + radius),
                               int(op.yDst + dy - radius));


            if(dy < radius) // Don't cross lines
            {
                SDL_RenderDrawLine(m_gRenderer,
                                   int(op.xDst - radius),
                                   int(op.yDst - dy + radius),
                                   int(op.xDst - dx),
                                   int(op.yDst - dy + radius));

                SDL_RenderDrawLine(m_gRenderer,
                                   int(op.xDst + dx),
                                   int(op.yDst - dy + radius),
                                   int(op.xDst + radius),
                                   int(op.yDst - dy + radius));
            }

            dy += 1;
        } while(dy <= radius);

        break;
    }


    case RenderOp::Type::texture:
    {
        if(!op.texture || !op.texture->inited)
            break;

        auto& tx = *op.texture;

        if(!tx.d.texture)
        {
            D_pLogWarningNA("Attempt to render an empty texture!");
            break;
        }

        SDL_assert_release(tx.d.texture);

        SDL_Rect destRect = {op.xDst, op.yDst, op.wDst, op.hDst};

        SDL_Rect sourceRect;
        SDL_Rect* sourceRectPtr = nullptr;

        if(op.traits & RenderOp::Traits::src_rect)
        {
            sourceRect = {op.xSrc, op.ySrc, op.wSrc, op.hSrc};
            sourceRectPtr = &sourceRect;
        }

        txColorMod(tx.d, op.color);

        if(op.traits & RenderOp::Traits::rotoflip)
        {
            const SDL_RendererFlip flip = (SDL_RendererFlip)(op.traits & 3);

            double angle = (op.traits & RenderOp::Traits::rotation)
                ? double(op.angle) * (360.0 / 65536.0)
                : 0.;

            SDL_RenderCopyEx(m_gRenderer, tx.d.texture, sourceRectPtr, &destRect,
                            angle, nullptr, flip);
        }
        else
        {
            SDL_RenderCopy(m_gRenderer, tx.d.texture, sourceRectPtr, &destRect);
        }

        break;
    }

    default:
        SDL_assert_release(false); // illegal render op type!
        break;
    }
}

void RenderSDL::renderRect(int x, int y, int w, int h, XTColor color, bool filled)
{
    RenderOp& op = m_render_queue.push(m_recent_draw_plane);

    op.type = RenderOp::Type::rect;
    op.xDst = x + m_viewport_offset_x;
    op.yDst = y + m_viewport_offset_y;
    op.wDst = w;
    op.hDst = h;

    op.color = color;

    if(filled)
        op.traits = RenderOp::Traits::filled;
    else
        op.traits = 0;
}

void RenderSDL::renderRectBR(int _left, int _top, int _right, int _bottom, XTColor color)
{
    RenderOp& op = m_render_queue.push(m_recent_draw_plane);

    op.type = RenderOp::Type::rect;
    op.xDst = _left + m_viewport_offset_x;
    op.yDst = _top + m_viewport_offset_y;
    op.wDst = _right - _left;
    op.hDst = _bottom - _top;

    op.color = color;

    op.traits = RenderOp::Traits::filled;
}

void RenderSDL::renderCircle(int cx, int cy, int radius, XTColor color, bool filled)
{
    if(radius <= 0)
        return; // Nothing to draw

    RenderOp& op = m_render_queue.push(m_recent_draw_plane);

    op.type = RenderOp::Type::circle;
    op.xDst = cx + m_viewport_offset_x;
    op.yDst = cy + m_viewport_offset_y;
    op.radius() = radius;

    op.color = color;

    if(filled)
        op.traits = RenderOp::Traits::filled;
    else
        op.traits = 0;
}

void RenderSDL::renderCircleHole(int cx, int cy, int radius, XTColor color)
{
    if(radius <= 0)
        return; // Nothing to draw

    RenderOp& op = m_render_queue.push(m_recent_draw_plane);

    op.type = RenderOp::Type::circle_hole;
    op.xDst = cx + m_viewport_offset_x;
    op.yDst = cy + m_viewport_offset_y;
    op.radius() = radius;

    op.color = color;
}



void RenderSDL::txColorMod(StdPictureData &tx, XTColor color)
{
    uint8_t modColor[4] = {color.r, color.g, color.b, color.a};

    if(SDL_memcmp(tx.modColor, modColor, 3) != 0)
    {
        SDL_SetTextureColorMod(tx.texture, modColor[0], modColor[1], modColor[2]);
        tx.modColor[0] = modColor[0];
        tx.modColor[1] = modColor[1];
        tx.modColor[2] = modColor[2];
    }

    if(tx.modColor[3] != modColor[3])
    {
        SDL_SetTextureAlphaMod(tx.texture, modColor[3]);
        tx.modColor[3] = modColor[3];
    }
}

void RenderSDL::renderTextureScaleEx(double xDstD, double yDstD, double wDstD, double hDstD,
                                       StdPicture &tx,
                                       int xSrc, int ySrc,
                                       int wSrc, int hSrc,
                                       double rotateAngle, FPoint_t *center, unsigned int flip,
                                       XTColor color)
{
    if(!tx.inited)
        return;

    if(!tx.d.texture && tx.l.lazyLoaded)
        lazyLoad(tx);

    if(!tx.d.texture)
    {
        D_pLogWarningNA("Attempt to render an empty texture!");
        return;
    }

    // Don't go more than size of texture
    if(xSrc + wSrc > tx.w)
    {
        wSrc = tx.w - xSrc;
        if(wSrc < 0)
            return;
    }
    if(ySrc + hSrc > tx.h)
    {
        hSrc = tx.h - ySrc;
        if(hSrc < 0)
            return;
    }


    RenderOp& op = m_render_queue.push(m_recent_draw_plane);

    op.type = RenderOp::Type::texture;
    op.texture = &tx;

    op.xDst = Maths::iRound(xDstD) + m_viewport_offset_x;
    op.yDst = Maths::iRound(yDstD) + m_viewport_offset_y;
    op.wDst = Maths::iRound(wDstD);
    op.hDst = Maths::iRound(hDstD);

    op.xSrc = tx.d.w_scale * xSrc;
    op.ySrc = tx.d.h_scale * ySrc;
    op.wSrc = tx.d.w_scale * wSrc;
    op.hSrc = tx.d.h_scale * hSrc;

    op.color = color;

    op.traits = (flip & 3) | RenderOp::Traits::src_rect;
    op.angle = 0;

    if(rotateAngle != 0.)
    {
        op.traits |= RenderOp::Traits::rotation;
        op.angle = (uint16_t)(std::fmod(rotateAngle, 360.) * (65536. / 360.));

        // calculate new offset now
        if(center)
        {
            double orig_offsetX = wDstD / 2 - center->x;
            double orig_offsetY = hDstD / 2 - center->y;
            double sin_theta = -sin(rotateAngle * (M_PI / 180.));
            double cos_theta = cos(rotateAngle * (M_PI / 180.));

            double rot_offsetX = orig_offsetX * cos_theta - orig_offsetY * sin_theta;
            double rot_offsetY = orig_offsetX * sin_theta + orig_offsetY * cos_theta;

            double shiftX = rot_offsetX - orig_offsetX;
            double shiftY = rot_offsetY - orig_offsetY;

            op.xDst = Maths::iRound(xDstD + shiftX) + m_viewport_offset_x;
            op.yDst = Maths::iRound(yDstD + shiftY) + m_viewport_offset_y;
        }
    }
}

void RenderSDL::renderTextureScale(double xDst, double yDst, double wDst, double hDst,
                                     StdPicture &tx,
                                     XTColor color)
{
    if(!tx.inited)
        return;

    if(!tx.d.texture && tx.l.lazyLoaded)
        lazyLoad(tx);

    if(!tx.d.texture)
    {
        D_pLogWarningNA("Attempt to render an empty texture!");
        return;
    }

    RenderOp& op = m_render_queue.push(m_recent_draw_plane);

    op.type = RenderOp::Type::texture;
    op.traits = m_pow2 ? RenderOp::Traits::src_rect : 0;

    op.texture = &tx;

    op.xDst = Maths::iRound(xDst) + m_viewport_offset_x;
    op.yDst = Maths::iRound(yDst) + m_viewport_offset_y;
    op.wDst = Maths::iRound(wDst);
    op.hDst = Maths::iRound(hDst);

    if(m_pow2)
    {
        op.xSrc = 0;
        op.ySrc = 0;
        op.wSrc = tx.d.w_scale * tx.w;
        op.hSrc = tx.d.h_scale * tx.h;
    }

    op.color = color;
}

void RenderSDL::renderTexture(double xDstD, double yDstD, double wDstD, double hDstD,
                                StdPicture &tx,
                                int xSrc, int ySrc,
                                XTColor color)
{
    if(!tx.inited)
        return;

    if(!tx.d.texture && tx.l.lazyLoaded)
        lazyLoad(tx);

    if(!tx.d.texture)
    {
        D_pLogWarningNA("Attempt to render an empty texture!");
        return;
    }

    int wDst = Maths::iRound(wDstD);
    int hDst = Maths::iRound(hDstD);

    // Don't go more than size of texture
    if(xSrc + wDst > tx.w)
    {
        wDst = tx.w - xSrc;
        if(wDst < 0)
            return;
    }
    if(ySrc + hDst > tx.h)
    {
        hDst = tx.h - ySrc;
        if(hDst < 0)
            return;
    }


    RenderOp& op = m_render_queue.push(m_recent_draw_plane);

    op.type = RenderOp::Type::texture;
    op.traits = RenderOp::Traits::src_rect;

    op.texture = &tx;

    op.xDst = Maths::iRound(xDstD) + m_viewport_offset_x;
    op.yDst = Maths::iRound(yDstD) + m_viewport_offset_y;
    op.wDst = wDst;
    op.hDst = hDst;

    op.xSrc = tx.d.w_scale * xSrc;
    op.ySrc = tx.d.h_scale * ySrc;
    op.wSrc = tx.d.w_scale * wDst;
    op.hSrc = tx.d.h_scale * hDst;

    op.color = color;
}

void RenderSDL::renderTextureFL(double xDstD, double yDstD, double wDstD, double hDstD,
                                  StdPicture &tx,
                                  int xSrc, int ySrc,
                                  double rotateAngle, FPoint_t *center, unsigned int flip,
                                  XTColor color)
{
    renderTextureScaleEx(xDstD, yDstD, wDstD, hDstD,
                         tx,
                         xSrc, ySrc,
                         Maths::iRound(wDstD), Maths::iRound(hDstD),
                         rotateAngle, center, flip,
                         color);
}

void RenderSDL::renderTexture(float xDst, float yDst,
                                StdPicture &tx,
                                XTColor color)
{
#ifdef USE_RENDER_BLOCKING
    SDL_assert(!m_blockRender);
#endif

    if(!tx.inited)
        return;

    if(!tx.d.texture && tx.l.lazyLoaded)
        lazyLoad(tx);

    if(!tx.d.texture)
    {
        D_pLogWarningNA("Attempt to render an empty texture!");
        return;
    }

    RenderOp& op = m_render_queue.push(m_recent_draw_plane);

    op.type = RenderOp::Type::texture;
    op.traits = m_pow2 ? RenderOp::Traits::src_rect : 0;

    op.texture = &tx;

    op.xDst = Maths::iRound(xDst) + m_viewport_offset_x;
    op.yDst = Maths::iRound(yDst) + m_viewport_offset_y;
    op.wDst = tx.w;
    op.hDst = tx.h;

    if(m_pow2)
    {
        op.xSrc = 0;
        op.ySrc = 0;
        op.wSrc = tx.d.w_scale * tx.w;
        op.hSrc = tx.d.h_scale * tx.h;
    }

    op.color = color;
}

void RenderSDL::getScreenPixels(int x, int y, int w, int h, unsigned char *pixels)
{
    flushRenderQueue();

    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;

#ifndef XTECH_SDL_NO_RECTF_SUPPORT
    SDL_RenderFlush(m_gRenderer);
#endif
    SDL_RenderReadPixels(m_gRenderer,
                         &rect,
                         SDL_PIXELFORMAT_BGR24,
                         pixels,
                         w * 3 + (w % 4));
}

void RenderSDL::getScreenPixelsRGBA(int x, int y, int w, int h, unsigned char *pixels)
{
    flushRenderQueue();

    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;

#ifndef XTECH_SDL_NO_RECTF_SUPPORT
    SDL_RenderFlush(m_gRenderer);
#endif
    SDL_RenderReadPixels(m_gRenderer,
                         &rect,
                         SDL_PIXELFORMAT_ABGR8888,
                         pixels,
                         w * 4);
}

int RenderSDL::getPixelDataSize(const StdPicture &tx)
{
    if(!tx.d.texture)
        return 0;
    return (tx.w * tx.h * 4);
}

void RenderSDL::getPixelData(const StdPicture &tx, unsigned char *pixelData)
{
    int pitch, w, h, a;
    void *pixels;

    if(!tx.d.texture)
        return;

    SDL_SetTextureBlendMode(tx.d.texture, SDL_BLENDMODE_BLEND);
    SDL_QueryTexture(tx.d.texture, nullptr, &a, &w, &h);
    SDL_LockTexture(tx.d.texture, nullptr, &pixels, &pitch);
    std::memcpy(pixelData, pixels, static_cast<size_t>(pitch) * h);
    SDL_UnlockTexture(tx.d.texture);
}
