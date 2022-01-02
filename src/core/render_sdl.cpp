/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2021 Vitaly Novichkov <admin@wohlnet.ru>
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

#ifdef __ANDROID__
#include <SDL2/SDL_assert.h>
#endif

#include <FreeImageLite.h>
#include <Logger/logger.h>
#include <Utils/maths.h>

#include "render_sdl.h"
#include "video.h"

#include "control/joystick.h"

#ifndef UNUSED
#define UNUSED(x) (void)x
#endif

// Workaround for older SDL versions that lacks the floating-point based rects and points
#ifdef XTECH_SDL_NO_RECTF_SUPPORT
#define SDL_RenderCopyF SDL_RenderCopy
#define SDL_RenderCopyExF SDL_RenderCopyEx
#endif



RenderSDL_t::RenderSDL_t()
{
    AbstractRender_t::init();
}

unsigned int RenderSDL_t::SDL_InitFlags()
{
    return 0;
}

bool RenderSDL_t::isWorking()
{
    return m_gRenderer && m_tBuffer;
}

bool RenderSDL_t::initRender(const CmdLineSetup_t &setup, SDL_Window *window)
{
    pLogDebug("Init renderer settings...");

    if(!AbstractRender_t::init())
        return false;

    m_window = window;

    Uint32 renderFlags = 0;

    switch(setup.renderType)
    {
    case RENDER_ACCELERATED_VSYNC:
        renderFlags = SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC;
        g_videoSettings.renderModeObtained = RENDER_ACCELERATED_VSYNC;
        pLogDebug("Using accelerated rendering with a vertical synchronization");
        m_gRenderer = SDL_CreateRenderer(window, -1, renderFlags | SDL_RENDERER_TARGETTEXTURE); // Try to make renderer
        if(m_gRenderer)
            break; // All okay
        pLogWarning("Failed to initialize V-Synced renderer, trying to create accelerated renderer...");

        // fallthrough
    case RENDER_ACCELERATED:
        renderFlags = SDL_RENDERER_ACCELERATED;
        g_videoSettings.renderModeObtained = RENDER_ACCELERATED;
        pLogDebug("Using accelerated rendering");
        m_gRenderer = SDL_CreateRenderer(window, -1, renderFlags | SDL_RENDERER_TARGETTEXTURE); // Try to make renderer
        if(m_gRenderer)
            break; // All okay
        pLogWarning("Failed to initialize accelerated renderer, trying to create a software renderer...");

        // fallthrough
    case RENDER_SOFTWARE:
        renderFlags = SDL_RENDERER_SOFTWARE;
        g_videoSettings.renderModeObtained = RENDER_SOFTWARE;
        pLogDebug("Using software rendering");
        m_gRenderer = SDL_CreateRenderer(window, -1, renderFlags | SDL_RENDERER_TARGETTEXTURE); // Try to make renderer
        if(m_gRenderer)
            break; // All okay

        pLogCritical("Unable to create renderer!");
        return false;
    }

    SDL_GetRendererInfo(m_gRenderer, &m_ri);
    m_maxTextureWidth = m_ri.max_texture_width;
    m_maxTextureHeight = m_ri.max_texture_height;

    m_tBuffer = SDL_CreateTexture(m_gRenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, ScaleWidth, ScaleHeight);
    if(!m_tBuffer)
    {
        pLogCritical("Unable to create texture render buffer!");
        return false;
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

void RenderSDL_t::close()
{
    clearAllTextures();
    AbstractRender_t::close();

    if(m_tBuffer)
        SDL_DestroyTexture(m_tBuffer);
    m_tBuffer = nullptr;

    if(m_gRenderer)
        SDL_DestroyRenderer(m_gRenderer);
    m_gRenderer = nullptr;
}

void RenderSDL_t::repaint()
{
#ifdef __ANDROID__
    if(m_blockRender)
        return;
#endif

    int w, h, off_x, off_y, wDst, hDst;
    float scale_x, scale_y;

    setTargetScreen();

#ifndef __EMSCRIPTEN__
    processRecorder();
    drawBatteryStatus();
#endif

    // Get the size of surface where to draw the scene
    SDL_GetRendererOutputSize(m_gRenderer, &w, &h);

    // Calculate the size difference factor
    scale_x = float(w) / ScaleWidth;
    scale_y = float(h) / ScaleHeight;

    wDst = w;
    hDst = h;

    // Keep aspect ratio
    if(scale_x > scale_y) // Width more than height
    {
        wDst = int(scale_y * ScaleWidth);
        hDst = int(scale_y * ScaleHeight);
    }
    else if(scale_x < scale_y) // Height more than width
    {
        hDst = int(scale_x * ScaleHeight);
        wDst = int(scale_x * ScaleWidth);
    }

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

#ifdef USE_TOUCHSCREEN_CONTROLLER
    RenderTouchControls();
#endif

    SDL_RenderPresent(m_gRenderer);
}

void RenderSDL_t::updateViewport()
{
    float w, w1, h, h1;
    int   wi, hi;

#ifndef __EMSCRIPTEN__
    SDL_GetWindowSize(m_window, &wi, &hi);
#else
    if(IsFullScreen(m_window))
    {
        SDL_GetWindowSize(m_window, &wi, &hi);
    }
    else
    {
        wi = ScreenW;
        hi = ScreenH;
    }
#endif

    w = wi;
    h = hi;
    w1 = w;
    h1 = h;

    m_scale_x = w / ScaleWidth;
    m_scale_y = h / ScaleHeight;
    m_viewport_scale_x = m_scale_x;
    m_viewport_scale_y = m_scale_y;

    m_viewport_offset_x = 0;
    m_viewport_offset_y = 0;
    m_viewport_offset_x_cur = 0;
    m_viewport_offset_y_cur = 0;
    m_viewport_offset_ignore = false;

    if(m_scale_x > m_scale_y)
    {
        w1 = m_scale_y * ScaleWidth;
        m_viewport_scale_x = w1 / ScaleWidth;
    }
    else if(m_scale_x < m_scale_y)
    {
        h1 = m_scale_x * ScaleHeight;
        m_viewport_scale_y = h1 / ScaleHeight;
    }

    m_offset_x = (w - w1) / 2;
    m_offset_y = (h - h1) / 2;

    m_viewport_x = 0;
    m_viewport_y = 0;
    m_viewport_w = static_cast<int>(w1);
    m_viewport_h = static_cast<int>(h1);
}

void RenderSDL_t::resetViewport()
{
    updateViewport();
    SDL_RenderSetViewport(m_gRenderer, nullptr);
}

void RenderSDL_t::setViewport(int x, int y, int w, int h)
{
    SDL_Rect topLeftViewport = {x, y, w, h};
    SDL_RenderSetViewport(m_gRenderer, &topLeftViewport);

    m_viewport_x = x;
    m_viewport_y = y;
    m_viewport_w = w;
    m_viewport_h = h;
}

void RenderSDL_t::offsetViewport(int x, int y)
{
    if(m_viewport_offset_x != x || m_viewport_offset_y != y)
    {
        m_viewport_offset_x_cur = x;
        m_viewport_offset_y_cur = y;
        m_viewport_offset_x = m_viewport_offset_ignore ? 0 : m_viewport_offset_x_cur;
        m_viewport_offset_y = m_viewport_offset_ignore ? 0 : m_viewport_offset_y_cur;
    }
}

void RenderSDL_t::offsetViewportIgnore(bool en)
{
    if(m_viewport_offset_ignore != en)
    {
        m_viewport_offset_x = en ? 0 : m_viewport_offset_x_cur;
        m_viewport_offset_y = en ? 0 : m_viewport_offset_y_cur;
    }
    m_viewport_offset_ignore = en;
}

void RenderSDL_t::mapToScreen(int x, int y, int *dx, int *dy)
{
    *dx = static_cast<int>((static_cast<float>(x) - m_offset_x) / m_viewport_scale_x);
    *dy = static_cast<int>((static_cast<float>(y) - m_offset_y) / m_viewport_scale_y);
}

void RenderSDL_t::setTargetTexture()
{
    if(m_recentTarget == m_tBuffer)
        return;
    SDL_SetRenderTarget(m_gRenderer, m_tBuffer);
    m_recentTarget = m_tBuffer;
}

void RenderSDL_t::setTargetScreen()
{
    if(m_recentTarget == nullptr)
        return;
    SDL_SetRenderTarget(m_gRenderer, nullptr);
    m_recentTarget = nullptr;
}

void RenderSDL_t::loadTexture(StdPicture &target, uint32_t width, uint32_t height, uint8_t *RGBApixels, uint32_t pitch)
{
    SDL_Surface *surface;
    SDL_Texture *texture;
    surface = SDL_CreateRGBSurfaceFrom(RGBApixels,
                                       static_cast<int>(width),
                                       static_cast<int>(height),
                                       32,
                                       static_cast<int>(pitch),
                                       FI_RGBA_RED_MASK,
                                       FI_RGBA_GREEN_MASK,
                                       FI_RGBA_BLUE_MASK,
                                       FI_RGBA_ALPHA_MASK);
    texture = SDL_CreateTextureFromSurface(m_gRenderer, surface);
    if(!texture)
    {
        pLogWarning("Render SDL: Failed to load texture! (%s)", SDL_GetError());
        SDL_FreeSurface(surface);
        target.texture = nullptr;
        target.inited = false;
        return;
    }

    SDL_FreeSurface(surface);

    target.texture = texture;
    m_textureBank.insert(texture);

    target.inited = true;
}

void RenderSDL_t::deleteTexture(StdPicture &tx, bool lazyUnload)
{
    if(!tx.inited || !tx.texture)
    {
        if(!lazyUnload)
            tx.inited = false;
        return;
    }

    auto corpseIt = m_textureBank.find(tx.texture);
    if(corpseIt == m_textureBank.end())
    {
        SDL_DestroyTexture(tx.texture);
        tx.texture = nullptr;
        if(!lazyUnload)
            tx.inited = false;
        return;
    }

    SDL_Texture *corpse = *corpseIt;
    if(corpse)
        SDL_DestroyTexture(corpse);
    m_textureBank.erase(corpse);

    tx.texture = nullptr;

    if(!lazyUnload)
    {
        tx.inited = false;
        tx.raw.clear();
        tx.rawMask.clear();
        tx.lazyLoaded = false;
        tx.isMaskPng = false;
        tx.w = 0;
        tx.h = 0;
        tx.frame_w = 0;
        tx.frame_h = 0;
    }

    tx.format = 0;
    tx.nOfColors = 0;
    tx.ColorUpper.r = 0;
    tx.ColorUpper.g = 0;
    tx.ColorUpper.b = 0;
    tx.ColorLower.r = 0;
    tx.ColorLower.g = 0;
    tx.ColorLower.b = 0;
}

void RenderSDL_t::clearAllTextures()
{
    for(SDL_Texture *tx : m_textureBank)
        SDL_DestroyTexture(tx);
    m_textureBank.clear();
}

void RenderSDL_t::clearBuffer()
{
#ifdef __ANDROID__
    SDL_assert(!m_blockRender);
#endif
    SDL_SetRenderDrawColor(m_gRenderer, 0, 0, 0, 255);
    SDL_RenderClear(m_gRenderer);
}

void RenderSDL_t::renderRect(int x, int y, int w, int h, float red, float green, float blue, float alpha, bool filled)
{
#ifdef __ANDROID__
    SDL_assert(!m_blockRender);
#endif
    SDL_Rect aRect = {x + m_viewport_offset_x,
                      y + m_viewport_offset_y,
                      w, h};
    SDL_SetRenderDrawColor(m_gRenderer,
                           static_cast<unsigned char>(255.f * red),
                           static_cast<unsigned char>(255.f * green),
                           static_cast<unsigned char>(255.f * blue),
                           static_cast<unsigned char>(255.f * alpha)
                          );

    if(filled)
        SDL_RenderFillRect(m_gRenderer, &aRect);
    else
        SDL_RenderDrawRect(m_gRenderer, &aRect);
}

void RenderSDL_t::renderRectBR(int _left, int _top, int _right, int _bottom, float red, float green, float blue, float alpha)
{
#ifdef __ANDROID__
    SDL_assert(!m_blockRender);
#endif
    SDL_Rect aRect = {_left + m_viewport_offset_x,
                      _top + m_viewport_offset_y,
                      _right - _left, _bottom - _top};
    SDL_SetRenderDrawColor(m_gRenderer,
                           static_cast<unsigned char>(255.f * red),
                           static_cast<unsigned char>(255.f * green),
                           static_cast<unsigned char>(255.f * blue),
                           static_cast<unsigned char>(255.f * alpha)
                          );
    SDL_RenderFillRect(m_gRenderer, &aRect);
}

void RenderSDL_t::renderCircle(int cx, int cy, int radius, float red, float green, float blue, float alpha, bool filled)
{
#ifdef __ANDROID__
    SDL_assert(!m_blockRender);
#endif
    UNUSED(filled);

    if(radius <= 0)
        return; // Nothing to draw

    SDL_SetRenderDrawColor(m_gRenderer,
                               static_cast<unsigned char>(255.f * red),
                               static_cast<unsigned char>(255.f * green),
                               static_cast<unsigned char>(255.f * blue),
                               static_cast<unsigned char>(255.f * alpha)
                          );

    cx += m_viewport_offset_x;
    cy += m_viewport_offset_y;

    double dy = 1;
    do //for(double dy = 1; dy <= radius; dy += 1.0)
    {
        double dx = std::floor(std::sqrt((2.0 * radius * dy) - (dy * dy)));
        SDL_RenderDrawLine(m_gRenderer,
                           int(cx - dx),
                           int(cy + dy - radius),
                           int(cx + dx),
                           int(cy + dy - radius));

        if(dy < radius) // Don't cross lines
        {
            SDL_RenderDrawLine(m_gRenderer,
                               int(cx - dx),
                               int(cy - dy + radius),
                               int(cx + dx),
                               int(cy - dy + radius));
        }

        dy += 1.0;
    } while(dy <= radius);
}

void RenderSDL_t::renderCircleHole(int cx, int cy, int radius, float red, float green, float blue, float alpha)
{
#ifdef __ANDROID__
    SDL_assert(!m_blockRender);
#endif

    if(radius <= 0)
        return; // Nothing to draw

    SDL_SetRenderDrawColor(m_gRenderer,
                               static_cast<unsigned char>(255.f * red),
                               static_cast<unsigned char>(255.f * green),
                               static_cast<unsigned char>(255.f * blue),
                               static_cast<unsigned char>(255.f * alpha)
                          );

    cx += m_viewport_offset_x;
    cy += m_viewport_offset_y;

    double dy = 1;
    do //for(double dy = 1; dy <= radius; dy += 1.0)
    {
        double dx = std::floor(std::sqrt((2.0 * radius * dy) - (dy * dy)));

        SDL_RenderDrawLine(m_gRenderer,
                           int(cx - radius),
                           int(cy + dy - radius),
                           int(cx - dx),
                           int(cy + dy - radius));

        SDL_RenderDrawLine(m_gRenderer,
                           int(cx + dx),
                           int(cy + dy - radius),
                           int(cx + radius),
                           int(cy + dy - radius));


        if(dy < radius) // Don't cross lines
        {
            SDL_RenderDrawLine(m_gRenderer,
                               int(cx - radius),
                               int(cy - dy + radius),
                               int(cx - dx),
                               int(cy - dy + radius));

            SDL_RenderDrawLine(m_gRenderer,
                               int(cx + dx),
                               int(cy - dy + radius),
                               int(cx + radius),
                               int(cy - dy + radius));
        }

        dy += 1.0;
    } while(dy <= radius);
}



static SDL_INLINE void txColorMod(StdPicture &tx, float red, float green, float blue, float alpha)
{
    uint8_t modColor[4] = {static_cast<unsigned char>(255.f * red),
                           static_cast<unsigned char>(255.f * green),
                           static_cast<unsigned char>(255.f * blue),
                           static_cast<unsigned char>(255.f * alpha)};

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

void RenderSDL_t::renderTextureScaleEx(double xDstD, double yDstD, double wDstD, double hDstD,
                                       StdPicture &tx,
                                       int xSrc, int ySrc,
                                       int wSrc, int hSrc,
                                       double rotateAngle, FPoint_t *center, unsigned int flip,
                                       float red, float green, float blue, float alpha)
{
#ifdef __ANDROID__
    SDL_assert(!m_blockRender);
#endif
    if(!tx.inited)
        return;

    if(!tx.texture && tx.lazyLoaded)
        lazyLoad(tx);

    if(!tx.texture)
    {
        D_pLogWarningNA("Attempt to render an empty texture!");
        return;
    }

    SDL_assert_release(tx.texture);

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

#ifndef XTECH_SDL_NO_RECTF_SUPPORT
    SDL_FRect destRect = {(float)xDst + m_viewport_offset_x,
                          (float)yDst + m_viewport_offset_y,
                          (float)wDst,
                          (float)hDst};
    SDL_FPoint *centerD = (SDL_FPoint*)center;
#else
    SDL_Rect destRect = {(int)xDst + m_viewport_offset_x,
                         (int)yDst + m_viewport_offset_y,
                         (int)wDst,
                         (int)hDst};
    SDL_Point centerI = {center ? Maths::iRound(center->x) : 0,
                         center ? Maths::iRound(center->y) : 0};
    SDL_Point *centerD = center ? &centerI : nullptr;
#endif

    SDL_Rect sourceRect;
    if(tx.w_orig == 0 && tx.h_orig == 0)
        sourceRect = {xSrc, ySrc, wSrc, hSrc};
    else
        sourceRect = {int(tx.w_scale * xSrc), int(tx.h_scale * ySrc), int(tx.w_scale * wSrc), int(tx.h_scale * hSrc)};

    txColorMod(tx, red, green, blue, alpha);
    SDL_RenderCopyExF(m_gRenderer, tx.texture, &sourceRect, &destRect,
                      rotateAngle, centerD, static_cast<SDL_RendererFlip>(flip));
}

void RenderSDL_t::renderTextureScale(double xDst, double yDst, double wDst, double hDst,
                                     StdPicture &tx,
                                     float red, float green, float blue, float alpha)
{
#ifdef __ANDROID__
    SDL_assert(!m_blockRender);
#endif
    const unsigned int flip = SDL_FLIP_NONE;

    if(!tx.inited)
        return;

    if(!tx.texture && tx.lazyLoaded)
        lazyLoad(tx);

    if(!tx.texture)
    {
        D_pLogWarningNA("Attempt to render an empty texture!");
        return;
    }

#ifndef XTECH_SDL_NO_RECTF_SUPPORT
    SDL_FRect destRect = {Maths::fRound(xDst) + m_viewport_offset_x,
                          Maths::fRound(yDst) + m_viewport_offset_y,
                          (float)wDst,
                          (float)hDst};
#else
    SDL_Rect destRect = {Maths::iRound(xDst) + m_viewport_offset_x,
                         Maths::iRound(yDst) + m_viewport_offset_y,
                         (int)wDst,
                         (int)hDst};
#endif

    SDL_Rect sourceRect;
    if(tx.w_orig == 0 && tx.h_orig == 0)
        sourceRect = {0, 0, tx.w, tx.h};
    else
        sourceRect = {0, 0, tx.w_orig, tx.h_orig};

    txColorMod(tx, red, green, blue, alpha);
    SDL_RenderCopyExF(m_gRenderer, tx.texture, &sourceRect, &destRect,
                      0.0, nullptr, static_cast<SDL_RendererFlip>(flip));
}

void RenderSDL_t::renderTexture(double xDstD, double yDstD, double wDstD, double hDstD,
                                StdPicture &tx,
                                int xSrc, int ySrc,
                                float red, float green, float blue, float alpha)
{
#ifdef __ANDROID__
    SDL_assert(!m_blockRender);
#endif
    if(!tx.inited)
        return;

    if(!tx.texture && tx.lazyLoaded)
        lazyLoad(tx);

    if(!tx.texture)
    {
        D_pLogWarningNA("Attempt to render an empty texture!");
        return;
    }

    SDL_assert_release(tx.texture);

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

#ifndef XTECH_SDL_NO_RECTF_SUPPORT
    SDL_FRect destRect = {(float)xDst + m_viewport_offset_x,
                          (float)yDst + m_viewport_offset_y,
                          (float)wDst,
                          (float)hDst};
#else
    SDL_Rect destRect = {(int)xDst + m_viewport_offset_x,
                         (int)yDst + m_viewport_offset_y,
                         (int)wDst,
                         (int)hDst};
#endif

    SDL_Rect sourceRect;

    if(tx.w_orig == 0 && tx.h_orig == 0)
        sourceRect = {xSrc, ySrc, (int)wDst, (int)hDst};
    else
        sourceRect = {int(tx.w_scale * xSrc), int(tx.h_scale * ySrc), int(tx.w_scale * wDst), int(tx.h_scale * hDst)};

    txColorMod(tx, red, green, blue, alpha);
    SDL_RenderCopyF(m_gRenderer, tx.texture, &sourceRect, &destRect);
}

void RenderSDL_t::renderTextureFL(double xDstD, double yDstD, double wDstD, double hDstD,
                                  StdPicture &tx,
                                  int xSrc, int ySrc,
                                  double rotateAngle, FPoint_t *center, unsigned int flip,
                                  float red, float green, float blue, float alpha)
{
#ifdef __ANDROID__
    SDL_assert(!m_blockRender);
#endif
    if(!tx.inited)
        return;

    if(!tx.texture && tx.lazyLoaded)
        lazyLoad(tx);

    if(!tx.texture)
    {
        D_pLogWarningNA("Attempt to render an empty texture!");
        return;
    }

    SDL_assert_release(tx.texture);

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

#ifndef XTECH_SDL_NO_RECTF_SUPPORT
    SDL_FRect destRect = {(float)xDst + m_viewport_offset_x,
                          (float)yDst + m_viewport_offset_y,
                          (float)wDst,
                          (float)hDst};
    SDL_FPoint *centerD = (SDL_FPoint*)center;
#else
    SDL_Rect destRect = {(int)xDst + m_viewport_offset_x,
                         (int)yDst + m_viewport_offset_y,
                         (int)wDst,
                         (int)hDst};
    SDL_Point centerI = {center ? Maths::iRound(center->x) : 0,
                         center ? Maths::iRound(center->y) : 0};
    SDL_Point *centerD = center ? &centerI : nullptr;
#endif

    SDL_Rect sourceRect;

    if(tx.w_orig == 0 && tx.h_orig == 0)
        sourceRect = {xSrc, ySrc, (int)wDst, (int)hDst};
    else
        sourceRect = {int(tx.w_scale * xSrc), int(tx.h_scale * ySrc), int(tx.w_scale * wDst), int(tx.h_scale * hDst)};

    txColorMod(tx, red, green, blue, alpha);
    SDL_RenderCopyExF(m_gRenderer, tx.texture, &sourceRect, &destRect,
                      rotateAngle, centerD, static_cast<SDL_RendererFlip>(flip));
}

void RenderSDL_t::renderTexture(float xDst, float yDst,
                                StdPicture &tx,
                                float red, float green, float blue, float alpha)
{
#ifdef __ANDROID__
    SDL_assert(!m_blockRender);
#endif
    const unsigned int flip = SDL_FLIP_NONE;

    if(!tx.inited)
        return;

    if(!tx.texture && tx.lazyLoaded)
        lazyLoad(tx);

    if(!tx.texture)
    {
        D_pLogWarningNA("Attempt to render an empty texture!");
        return;
    }

#ifndef XTECH_SDL_NO_RECTF_SUPPORT
    SDL_FRect destRect = {Maths::fRound(xDst), Maths::fRound(yDst), (float)tx.w, (float)tx.h};
#else
    SDL_Rect destRect = {Maths::iRound(xDst), Maths::iRound(yDst), tx.w, tx.h};
#endif

    SDL_Rect sourceRect;
    if(tx.w_orig == 0 && tx.h_orig == 0)
        sourceRect = {0, 0, tx.w, tx.h};
    else
        sourceRect = {0, 0, tx.w_orig, tx.h_orig};

    txColorMod(tx, red, green, blue, alpha);
    SDL_RenderCopyExF(m_gRenderer, tx.texture, &sourceRect, &destRect,
                      0.0, nullptr, static_cast<SDL_RendererFlip>(flip));
}

void RenderSDL_t::getScreenPixels(int x, int y, int w, int h, unsigned char *pixels)
{
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_RenderReadPixels(m_gRenderer,
                         &rect,
                         SDL_PIXELFORMAT_BGR24,
                         pixels,
                         w * 3 + (w % 4));
}

void RenderSDL_t::getScreenPixelsRGBA(int x, int y, int w, int h, unsigned char *pixels)
{
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_RenderReadPixels(m_gRenderer,
                         &rect,
                         SDL_PIXELFORMAT_ABGR8888,
                         pixels,
                         w * 4);
}

int RenderSDL_t::getPixelDataSize(const StdPicture &tx)
{
    if(!tx.texture)
        return 0;
    return (tx.w * tx.h * 4);
}

void RenderSDL_t::getPixelData(const StdPicture &tx, unsigned char *pixelData)
{
    int pitch, w, h, a;
    void *pixels;

    if(!tx.texture)
        return;

    SDL_SetTextureBlendMode(tx.texture, SDL_BLENDMODE_BLEND);
    SDL_QueryTexture(tx.texture, nullptr, &a, &w, &h);
    SDL_LockTexture(tx.texture, nullptr, &pixels, &pitch);
    std::memcpy(pixelData, pixels, static_cast<size_t>(pitch) * h);
    SDL_UnlockTexture(tx.texture);
}
