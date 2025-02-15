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

#include <Logger/logger.h>
#include <Utils/files.h>
#include <SDL2/SDL_rwops.h>
#include <PGE_File_Formats/file_formats.h>

#include "globals.h"
#include "frame_timer.h"
#include "core/render.h"
#include "core/window.h"

#ifdef THEXTECH_CLI_BUILD
#include <Graphics/image_size.h>
#endif

namespace XRender
{

int TargetW = 800;
int TargetH = 600;

bool isWorking()
{
    return true;
}

bool hasFrameBuffer()
{
    return false;
}

bool init()
{
    return true;
}

void quit()
{}

void setTargetTexture()
{}

void setTargetScreen()
{}

void setDrawPlane(uint8_t plane)
{
    (void)plane;
}

void clearBuffer()
{}

void repaint()
{}

void getRenderSize(int* w, int* h)
{
    return XWindow::getWindowSize(w, h);
}

void mapToScreen(int x, int y, int *dx, int *dy)
{
    *dx = x;
    *dy = y;
}

void mapFromScreen(int scr_x, int scr_y, int *window_x, int *window_y)
{
    *window_x = scr_x;
    *window_y = scr_y;
}

void updateViewport()
{
}

void resetViewport()
{
}

void setViewport(int x, int y, int w, int h)
{
    UNUSED(x);
    UNUSED(y);
    UNUSED(w);
    UNUSED(h);
}

void offsetViewport(int x, int y)
{
    UNUSED(x);
    UNUSED(y);
}

void offsetViewportIgnore(bool en)
{
    UNUSED(en);
}

void setTransparentColor(StdPicture &target, uint32_t rgb)
{
    UNUSED(target);
    UNUSED(rgb);
}

void lazyLoadPicture(StdPicture_Sub& target, const std::string& path, int scaleFactor, const std::string& maskPath, const std::string& maskFallbackPath)
{
    (void)maskPath;
    (void)maskFallbackPath;

    if(!GameIsActive)
        return; // do nothing when game is closed

    target.inited = true;

    // We need to figure out the height and width!
    std::string sizePath = path + ".size";
    SDL_RWops* fs = Files::open_file(sizePath, "r");

    // NOT null-terminated: wwww\nhhhh\n
    char contents[10];

    if(fs != nullptr)
    {
        SDL_RWread(fs, &contents[0], 1, 10);
        contents[4] = '\0';
        contents[9] = '\0';
        target.w = atoi(&contents[0]);
        target.h = atoi(&contents[5]);
        if(SDL_RWclose(fs))
            pLogWarning("loadPicture: Couldn't close file.");
    }
    else
    {
#ifdef THEXTECH_CLI_BUILD
        uint32_t w, h;
        if(PGE_ImageInfo::getImageSize(path, &w, &h, nullptr))
        {
            target.w = w * scaleFactor;
            target.h = h * scaleFactor;
        }
        else
        {
            pLogWarning("loadPicture: Couldn't open file.");
            target.inited = false;
            return;
        }
#else
        // this will work if it's a PNG
        SDL_RWops* fpng = Files::open_file(path, "rb");
        if(!fpng)
        {
            pLogWarning("loadPicture: Couldn't open size file.");
            target.inited = false;
            return;
        }

        SDL_RWseek(fpng, 16, RW_SEEK_SET);

        uint32_t w, h;
        if(SDL_RWread(fpng, &w, 4, 1) == 1 && fread(fpng, &h, 4, 1) == 1)
        {
            w = SDL_SwapLE32(w);
            h = SDL_SwapLE32(h);

            w = static_cast<uint32_t>(((w << 24) | ((w << 8) & 0x00FF0000) |
                ((w >> 8) & 0x0000FF00) | (w >> 24)));
            h = static_cast<uint32_t>(((h << 24) | ((h << 8) & 0x00FF0000) |
                ((h >> 8) & 0x0000FF00) | (h >> 24)));

            target.w = w * scaleFactor;
            target.h = h * scaleFactor;
        }
        else
        {
            target.inited = false;
        }

        SDL_RWclose(fpng);
#endif
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

    int w, h;
    t.readLine(line_buf);
    if(sscanf(line_buf.c_str(), "%d", &w) == 1)
    {
        t.readLine(line_buf);

        if(sscanf(line_buf.c_str(), "%d", &h) == 1)
        {
            // t.readLine(line_buf);

            // if(sscanf(line_buf.c_str(), "%d", &flags) == 1)
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

    return;
}

void lazyLoad(StdPicture &target)
{
    UNUSED(target);
    return;
}

void lazyPreLoad(StdPicture &target)
{
    lazyLoad(target);
}


void loadTexture(StdPicture&, uint32_t, uint32_t, uint8_t*, uint32_t)
{
}

void unloadTexture(StdPicture &tx)
{
    if(!tx.l.canLoad())
        static_cast<StdPicture_Sub&>(tx) = StdPicture_Sub();
}

void unloadGifTextures()
{}

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

void renderRect(int, int, int, int, XTColor, bool)
{
}

void renderRectBR(int _left, int _top, int _right, int _bottom, XTColor color)
{
    renderRect(_left, _top, _right-_left, _bottom-_top, color, true);
}

void renderCircle(int, int,
                  int,
                  XTColor,
                  bool)
{
}

void renderCircleHole(int cx, int cy,
                      int radius,
                      XTColor color)
{
    if(radius <= 0)
        return; // Nothing to draw

    double line_size = 4;
    double dy = line_size;

    do
    {
        double dx = std::floor(std::sqrt((2.0 * radius * dy) - (dy * dy)));

        renderRectBR(cx - radius, cy + dy - radius - line_size, cx - dx, cy + dy - radius + line_size,
            color);

        renderRectBR(cx + dx, cy + dy - radius - line_size, cx + radius, cy + dy - radius + line_size,
            color);

        if(dy < radius) // Don't cross lines
        {
            renderRectBR(cx - radius, cy - dy + radius - line_size, cx + radius, cy - dy + radius + line_size,
                color);

            renderRectBR(cx + dx, cy - dy + radius - line_size, cx + radius, cy - dy + radius + line_size,
                color);
        }

        dy += line_size * 2;
    } while(dy + line_size <= radius);
}

inline void i_renderTexturePrivate(float, float, float, float,
                             StdPicture&,
                             float, float, float, float,
                             float, FPoint_t*, unsigned int,
                             XTColor)
{
}

inline void i_renderTexturePrivate_Basic(int16_t, int16_t, int16_t, int16_t,
                             StdPicture &,
                             int16_t, int16_t,
                             XTColor)
{
}

// public draw methods

void renderTextureScale(double xDst, double yDst, double wDst, double hDst,
                            StdPicture &tx,
                            int xSrc, int ySrc, int wSrc, int hSrc,
                            XTColor color)
{
    auto div_x = ROUNDDIV2(xDst), div_y = ROUNDDIV2(yDst);
    i_renderTexturePrivate(
        div_x, div_y, ROUNDDIV2(xDst + wDst) - div_x, ROUNDDIV2(yDst + hDst) - div_y,
        tx,
        xSrc / 2, ySrc / 2, wSrc / 2, hSrc / 2,
        0.0f, nullptr, X_FLIP_NONE,
        color);
}

void renderTexture(double xDst, double yDst, double wDst, double hDst,
                            StdPicture &tx,
                            int xSrc, int ySrc,
                            XTColor color)
{
    auto div_x = ROUNDDIV2(xDst), div_y = ROUNDDIV2(yDst);
    auto div_w = ROUNDDIV2(xDst + wDst) - div_x;
    auto div_h = ROUNDDIV2(yDst + hDst) - div_y;

    i_renderTexturePrivate(
        div_x, div_y, div_w, div_h,
        tx,
        xSrc / 2, ySrc / 2, div_w, div_h,
        0.0f, nullptr, X_FLIP_NONE,
        color);
}

void renderTextureBasic(int xDst, int yDst, int wDst, int hDst,
                            StdPicture &tx,
                            int xSrc, int ySrc,
                            XTColor color)
{
    auto div_x = FLOORDIV2(xDst), div_y = FLOORDIV2(yDst);
    auto div_w = FLOORDIV2(xDst + wDst) - div_x;
    auto div_h = FLOORDIV2(yDst + hDst) - div_y;

    i_renderTexturePrivate_Basic(
        div_x, div_y, div_w, div_h,
        tx,
        FLOORDIV2(xSrc), FLOORDIV2(ySrc),
        color);
}

void renderTexture(float xDst, float yDst, StdPicture &tx,
                   XTColor color)
{
    int w = tx.w / 2;
    int h = tx.h / 2;
    i_renderTexturePrivate(
        ROUNDDIV2(xDst), ROUNDDIV2(yDst), w, h,
        tx,
        0.0f, 0.0f, w, h,
        0.0f, nullptr, X_FLIP_NONE,
        color);
}

void renderTextureBasic(int xDst, int yDst, StdPicture &tx, XTColor color)
{
    int w = tx.w / 2;
    int h = tx.h / 2;
    i_renderTexturePrivate_Basic(
        ROUNDDIV2(xDst), ROUNDDIV2(yDst), w, h,
        tx,
        0.0f, 0.0f,
        color);
}

void renderTextureScale(double xDst, double yDst, double wDst, double hDst, StdPicture &tx, XTColor color)
{
    i_renderTexturePrivate(
        ROUNDDIV2(xDst), ROUNDDIV2(yDst), tx.w / 2, tx.h / 2,
        tx,
        0.0f, 0.0f, wDst / 2, hDst / 2,
        0.0f, nullptr, X_FLIP_NONE,
        color);
}

void renderTextureFL(double xDst, double yDst, double wDst, double hDst,
                          StdPicture &tx,
                          int xSrc, int ySrc,
                          double rotateAngle, FPoint_t *center, unsigned int flip,
                          XTColor color)
{
    auto div_x = ROUNDDIV2(xDst), div_y = ROUNDDIV2(yDst);
    auto div_w = ROUNDDIV2(xDst + wDst) - div_x;
    auto div_h = ROUNDDIV2(yDst + hDst) - div_y;

    i_renderTexturePrivate(
        div_x, div_y, div_w, div_h,
        tx,
        xSrc / 2, ySrc / 2, div_w, div_h,
        rotateAngle, center, flip,
        color);
}

void renderTextureScaleEx(double xDst, double yDst, double wDst, double hDst,
                          StdPicture &tx,
                          int xSrc, int ySrc,
                          int wSrc, int hSrc,
                          double rotateAngle, FPoint_t *center, unsigned int flip,
                          XTColor color)
{
    auto div_x = ROUNDDIV2(xDst), div_y = ROUNDDIV2(yDst);
    auto div_w = ROUNDDIV2(xDst + wDst) - div_x;
    auto div_h = ROUNDDIV2(yDst + hDst) - div_y;

    i_renderTexturePrivate(
        div_x, div_y, div_w, div_h,
        tx,
        xSrc / 2, ySrc / 2, wSrc / 2, hSrc / 2,
        rotateAngle, center, flip,
        color);
}

void splitFrame()
{
}

size_t lazyLoadedBytes()
{
    return 0;
}

void lazyLoadedBytesReset()
{
}

} // namespace XRender
