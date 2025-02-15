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

#pragma once
#ifndef RENDER_HHHHHH
#define RENDER_HHHHHH

#include <cstdint>
#include <string>

#include "sdl_proxy/sdl_stdinc.h"

#include "std_picture.h"
#include "base/render_types.h"

#ifndef RENDER_CUSTOM
#   include "base/render_base.h"
#   define E_INLINE SDL_FORCE_INLINE
#   define TAIL
#else
#   define E_INLINE    extern
#   define TAIL ;
#endif

namespace PGE_FileFormats_misc
{
    class TextInput;
}

struct UniformValue_t;


namespace XRender
{

#ifdef __3DS__

constexpr int MAX_3D_OFFSET = 20;
constexpr int TargetCameraOverscanX = 20;
constexpr int TargetOverscanX = 28;

#else

constexpr int TargetCameraOverscanX = 0;
constexpr int TargetOverscanX = 0;

#endif

// the current width / height of the primary render target
extern int TargetW;
extern int TargetH;

#ifndef RENDER_CUSTOM
// reset bitmask warning flag for SDL platforms
extern bool g_BitmaskTexturePresent;
#endif

#ifdef __16M__
// signals that the GPU is ready for new commands (will not block at frame end)
bool ready_for_frame();
#endif


#ifdef RENDER_CUSTOM

extern bool init();
extern void quit();

#endif

#ifdef RENDER_CUSTOM

constexpr bool is_nullptr()
{
    return false;
}

#else

SDL_FORCE_INLINE bool is_nullptr()
{
    return !g_render;
}

#endif


/*!
 * \brief Identify does render engine works or not
 * \return true if render initialized and works
 */
E_INLINE bool isWorking() TAIL
#ifndef RENDER_CUSTOM
{
    return g_render->isWorking();
}
#endif

/*!
 * \brief Tells is frame buffer is currently available or not
 * \return true if framebuffer is available
 */
E_INLINE  bool hasFrameBuffer() TAIL
#ifndef RENDER_CUSTOM
{
    return g_render->hasFrameBuffer();
}
#endif

/*!
 * \brief Call the repaint
 */
E_INLINE void repaint() TAIL
#ifndef RENDER_CUSTOM
{
    g_render->repaint();
}
#endif

/*!
 * \brief Update viewport (mainly after screen resize)
 */
E_INLINE void updateViewport() TAIL
#ifndef RENDER_CUSTOM
{
    g_render->updateViewport();
}
#endif

/*!
 * \brief Reset viewport into default state
 *
 * Note: WILL invoke splitFrame
 */
E_INLINE void resetViewport() TAIL
#ifndef RENDER_CUSTOM
{
    g_render->resetViewport();
}
#endif

/*!
 * \brief Set the viewport area
 * \param x X position
 * \param y Y position
 * \param w Viewport Width
 * \param h Viewport Height
 *
 * Note: WILL invoke splitFrame
 */
E_INLINE void setViewport(int x, int y, int w, int h) TAIL
#ifndef RENDER_CUSTOM
{
    g_render->setViewport(x, y, w, h);
}
#endif

/*!
 * \brief Set the render offset
 * \param x X offset
 * \param y Y offset
 *
 * All drawing objects will be drawn with a small offset
 * MAY invoke splitFrame
 */
E_INLINE void offsetViewport(int x, int y) TAIL // for screen-shaking
#ifndef RENDER_CUSTOM
{
    g_render->offsetViewport(x, y);
}
#endif

/*!
 * \brief Set temporary ignore of render offset
 * \param en Enable viewport offset ignore
 *
 * Use this to draw certain objects with ignorign of the GFX offset
 * MAY invoke splitFrame
 */
E_INLINE void offsetViewportIgnore(bool en) TAIL
#ifndef RENDER_CUSTOM
{
    g_render->offsetViewportIgnore(en);
}
#endif

/*!
 * \brief Make any subsequent draws invisible to any previous draws (important for reflections, transparency, etc)
 *
 * Only has an effect for OpenGL renderer and other batched renderers.
 *
 * Note: may result in subsequent transparent draw being inaccurately drawn above previous transparent draw.
 */
E_INLINE void splitFrame() TAIL
#ifndef RENDER_CUSTOM
{
    g_render->splitFrame();
}
#endif


/*!
 * \brief Get the current size of the window in render pixels
 * \param w Width
 * \param h Height
 */
E_INLINE void getRenderSize(int *w, int *h) TAIL
#ifndef RENDER_CUSTOM
{
    g_render->getRenderSize(w, h);
}
#endif

/*!
 * \brief Map absolute point coordinate into screen relative
 * \param x Window X position
 * \param y Window Y position
 * \param dx Destinition on-screen X position
 * \param dy Destinition on-screen Y position
 */
E_INLINE void mapToScreen(int x, int y, int *dx, int *dy) TAIL
#ifndef RENDER_CUSTOM
{
    g_render->mapToScreen(x, y, dx, dy);
}
#endif


/*!
 * \brief Map screen relative coordinate into physical canvas
 * \param x On-screen X position
 * \param y On-screen Y position
 * \param dx Destinition window X position
 * \param dy Destinition window Y position
 */
E_INLINE void mapFromScreen(int x, int y, int *dx, int *dy) TAIL
#ifndef RENDER_CUSTOM
{
    g_render->mapFromScreen(x, y, dx, dy);
}
#endif

/*!
 * \brief Set render target into the E_INLINE in-game screen (use to render in-game world)
 */
E_INLINE void setTargetTexture() TAIL
#ifndef RENDER_CUSTOM
{
    g_render->setTargetTexture();
}
#endif

/*!
 * \brief Set render target into the real window or screen (use to render on-screen buttons and other meta-info)
 */
E_INLINE void setTargetScreen() TAIL
#ifndef RENDER_CUSTOM
{
    g_render->setTargetScreen();
}
#endif

/*!
 * \brief Sets draw plane for subsequent draws.
 *
 * \param plane Which draw plane should be used.
 *
 * The multiples of 8 are "primary planes" and will correctly order 1824 draws. There are 32 of these.
 * Other planes are "offset" planes, with each primary_plane + offset (-4 - +3) having the same 5 most significant bits as the primary plane.
 * Offset planes -4 (252), -3, -2, and -1 (255) are offset planes of primary plane 0.
 * Each offset plane will correctly order 32 draws per frame.
 *
 * NOTE: Switching between 3D planes will disrupt the 3DS render pipeline. Avoid doing so more than strictly necessary.
 */
E_INLINE void setDrawPlane(uint8_t plane) TAIL
#ifndef RENDER_CUSTOM
{
    return g_render->setDrawPlane(plane);
}
#endif

/*!
 * \brief Reports whether the *currently-active* renderer supports loading GLSL ES shaders
 *
 * Should not be used to prevent loading (to ensure consistency when renderer is hotswapped)
 */
E_INLINE bool userShadersSupported() TAIL
#ifndef RENDER_CUSTOM
{
    return g_render->userShadersSupported();
}
#endif

/*!
 * \brief Unload all currently loaded GIF textures
 */
E_INLINE void unloadGifTextures() TAIL
#ifndef RENDER_CUSTOM
{
    g_render->unloadGifTextures();
}
#endif

#ifdef __16M__
/*!
 * \brief Clear all currently loaded textures
 */
E_INLINE void clearAllTextures() TAIL
#endif

#ifdef __3DS__
/*!
 * \brief Set render target to a certain layer of the in-game screen
 */
E_INLINE void setTargetLayer(int layer) TAIL

/*!
 * \brief Multiply the alpha of the current screen layer by rate.
 *
 * Exclusively used in fade from asset pack select screen to main menu.
 */
E_INLINE void targetFade(uint8_t rate) TAIL

/*!
 * \brief Set render target to directly render to the 3DS top screen
 */
E_INLINE void setTargetMainScreen() TAIL

/*!
 * \brief Set render target to directly render to the 3DS subscreen
 */
E_INLINE void setTargetSubScreen() TAIL
#endif


E_INLINE void lazyLoadPicture(StdPicture_Sub &target,
                              const std::string &path,
                              int scaleFactor,
                              const std::string &maskPath = std::string(),
                              const std::string &maskFallbackPath = std::string()) TAIL
#ifndef RENDER_CUSTOM
{
    AbstractRender_t::lazyLoadPicture(target, path, scaleFactor, maskPath, maskFallbackPath);
}
#endif

SDL_FORCE_INLINE void lazyLoadPicture(StdPicture_Sub &target,
                                      const std::string &path,
                                      const std::string &maskPath = std::string(),
                                      const std::string &maskFallbackPath = std::string())
{
    lazyLoadPicture(target, path, 1, maskPath, maskFallbackPath);
}

E_INLINE void lazyLoad(StdPicture &target) TAIL
#ifndef RENDER_CUSTOM
{
    AbstractRender_t::lazyLoad(target);
}
#endif

// load a shader-only picture (must succeed if file exists and shaders are supported at COMPILE time)
E_INLINE void LoadPictureShader(StdPicture& target, const std::string &path) TAIL
#ifndef RENDER_CUSTOM
{
    AbstractRender_t::LoadPictureShader(target, path);
}
#endif

// load a particle system picture (must succeed if vertex file exists, all other provided files exist, and shaders are supported at COMPILE time)
E_INLINE void LoadPictureParticleSystem(StdPicture& target, const std::string &vertexPath, const std::string& fragPath, const std::string& imagePath) TAIL
#ifndef RENDER_CUSTOM
{
    AbstractRender_t::LoadPictureParticleSystem(target, vertexPath, fragPath, imagePath);
}
#endif

// load a picture whose logical size is some factor times its texture size (the texture has 1x1 pixels)
SDL_FORCE_INLINE void LoadPicture(StdPicture &target,
                                  const std::string &path,
                                  int scaleFactor,
                                  const std::string &maskPath = std::string(),
                                  const std::string &maskFallbackPath = std::string())
{
    lazyLoadPicture(target, path, scaleFactor, maskPath, maskFallbackPath);
#ifndef PGE_MIN_PORT
    lazyLoad(target);
#endif
}

// load a picture whose logical size is the same as its texture size (the texture normally would have 2x2 pixels)
SDL_FORCE_INLINE void LoadPicture(StdPicture &target,
                                  const std::string &path,
                                  const std::string &maskPath = std::string(),
                                  const std::string &maskFallbackPath = std::string())
{
    lazyLoadPicture(target, path, 1, maskPath, maskFallbackPath);
#ifndef PGE_MIN_PORT
    lazyLoad(target);
#endif
}


#if defined(PGE_MIN_PORT) || defined(THEXTECH_CLI_BUILD)
E_INLINE void lazyLoadPictureFromList(StdPicture_Sub& target, PGE_FileFormats_misc::TextInput& t, std::string& line_buf, const std::string& dir);
#endif

E_INLINE void setTransparentColor(StdPicture &target, uint32_t rgb) TAIL
#ifndef RENDER_CUSTOM
{
    AbstractRender_t::setTransparentColor(target, rgb);
}
#endif

E_INLINE void lazyPreLoad(StdPicture &target) TAIL
#ifndef RENDER_CUSTOM
{
    AbstractRender_t::lazyPreLoad(target);
}
#endif

E_INLINE size_t lazyLoadedBytes() TAIL
#ifndef RENDER_CUSTOM
{
    return AbstractRender_t::lazyLoadedBytes();
}
#endif

E_INLINE void lazyLoadedBytesReset() TAIL
#ifndef RENDER_CUSTOM
{
    AbstractRender_t::lazyLoadedBytesReset();
}
#endif

/*!
 * \brief Load a texture's backing image data (image data may differ from texture's logical size)
 * \param target Destination texture entry
 * \param width Width of the input texture in pixels
 * \param height Height of the input texture in pixels
 * \param RGBApixels Pointer to the RGBA pixel data
 * \param pitch Width of the line in bytes
 *
 * Important note: internal size data of the target texture
 * (such as .w, .h) must be filled externally BEFORE loading
 * the texture.
 */
E_INLINE void loadTexture(StdPicture &target,
                          uint32_t width,
                          uint32_t height,
                          uint8_t *RGBApixels,
                          uint32_t pitch) TAIL
#ifndef RENDER_CUSTOM
{
    g_render->loadTexture(target, width, height, RGBApixels, pitch);
}
#endif

/*!
 * \brief Unload all renderer-specific state of a texture
 *
 * Important note: if the texture does not have loading information, fully de-inits it.
 */
E_INLINE void unloadTexture(StdPicture &tx) TAIL
#ifndef RENDER_CUSTOM
{
    g_render->unloadTexture(tx);
}
#endif

E_INLINE void clearBuffer() TAIL
#ifndef RENDER_CUSTOM
{
    g_render->clearBuffer();
}
#endif


// Support for initializing and setting shader uniforms

/*!
 * \brief Registers a custom uniform variable in the next available index
 * \param target Destination texture entry
 * \param name name of the uniform variable, passed to `glGetUniformLocation`
 * \returns The internal index for the uniform, -1 on failure (including missing GL support at compile time)
 *
 * Returned indexes are maintained for the full lifespan of the StdPicture, including reloads
 */
E_INLINE int registerUniform(StdPicture &target,
                             const char* name) TAIL
#ifndef RENDER_CUSTOM
{
    return g_render->registerUniform(target, name);
}
#endif

/*!
 * \brief Assigns a custom uniform variable to a specific value
 * \param target Destination texture entry
 * \param index index of the uniform variable, returned from `registerUniform`
 * \param value value to store in the uniform variable
 *
 * Assignments to active indexes are maintained for the full lifespan of the StdPicture, including reloads
 * Invalid assignments (wrong type or size) to active indexes will reset the uniform variable at the index
 */
E_INLINE void assignUniform(StdPicture &target,
                            int index,
                            const UniformValue_t& value) TAIL
#ifndef RENDER_CUSTOM
{
    return g_render->assignUniform(target, index, value);
}
#endif

/*!
 * \brief Adds a particle to a particle system at a certain world location
 * \param target Destination texture entry
 * \param worldX world X coordinate at which to draw the particle
 * \param worldY world Y coordinate at which to draw the particle
 * \param attrs  system-specific attributes with which to draw the particle
 *
 * Particles are only stored or remembered if the CURRENT renderer supports them.
 */
E_INLINE void spawnParticle(StdPicture &target,
                            double worldX,
                            double worldY,
                            ParticleVertexAttrs_t attrs) TAIL
#ifndef RENDER_CUSTOM
{
    return g_render->spawnParticle(target, worldX, worldY, attrs);
}
#endif

#ifdef THEXTECH_BUILD_GL_MODERN

/*!
 * \brief Adds a light to the current scene (used by section effect shaders, expires at the next viewport change, splitFrame, or finish draw)
 * \param light Light specified in screen coordinates
 *
 * Lights are only stored or remembered if the CURRENT renderer supports them.
 */
E_INLINE void addLight(const GLLight &light) TAIL
{
    return g_render->addLight(light);
}

/*!
 * \brief Sets the lighting system parameters for the current scene (used by section effect shaders, expires at the next viewport change, splitFrame, or finish draw)
 * \param system Light system parameters
 *
 * Lights are only stored or remembered if the CURRENT renderer supports them.
 */
E_INLINE void setupLighting(const GLLightSystem &system) TAIL
{
    return g_render->setupLighting(system);
}

/*!
 * \brief A render call over the full viewport whose fragment shader tints the screen with information from the lighting system
 */
E_INLINE void renderLighting() TAIL
{
    return g_render->renderLighting();
}

#endif

// Draw primitives

E_INLINE void renderRect(int x, int y, int w, int h,
                        XTColor color = XTColor(),
                        bool filled = true) TAIL
#ifndef RENDER_CUSTOM
{
    g_render->renderRect(x, y, w, h,
                         color,
                         filled);
}
#endif

E_INLINE void renderRectBR(int _left, int _top, int _right, int _bottom,
                           XTColor color) TAIL
#ifndef RENDER_CUSTOM
{
    g_render->renderRectBR(_left, _top, _right, _bottom,
                           color);
}
#endif

E_INLINE void renderCircle(int cx, int cy,
                          int radius,
                          XTColor color = XTColor(),
                          bool filled = true) TAIL
#ifndef RENDER_CUSTOM
{
    g_render->renderCircle(cx, cy,
                           radius,
                           color,
                           filled);
}
#endif

E_INLINE void renderCircleHole(int cx, int cy, int radius,
                               XTColor color = XTColor()) TAIL
#ifndef RENDER_CUSTOM
{
    g_render->renderCircleHole(cx, cy, radius, color);
}
#endif


// Draw texture

E_INLINE void renderTextureScaleEx(double xDst, double yDst, double wDst, double hDst,
                          StdPicture &tx,
                          int xSrc, int ySrc,
                          int wSrc, int hSrc,
                          double rotateAngle = 0.0, FPoint_t *center = nullptr, unsigned int flip = X_FLIP_NONE,
                          XTColor color = XTColor()) TAIL
#ifndef RENDER_CUSTOM
{
    g_render->renderTextureScaleEx(xDst, yDst, wDst, hDst,
                                   tx,
                                   xSrc, ySrc,
                                   wSrc, hSrc,
                                   rotateAngle, center, flip,
                                   color);
}
#endif

E_INLINE void renderTextureScale(double xDst, double yDst, double wDst, double hDst,
                        StdPicture &tx,
                        XTColor color = XTColor()) TAIL
#ifndef RENDER_CUSTOM
{
    g_render->renderTextureScale(xDst, yDst, wDst, hDst,
                                 tx,
                                 color);
}
#endif

E_INLINE void renderTextureBasic(int xDst, int yDst, int wDst, int hDst,
                           StdPicture &tx,
                           int xSrc, int ySrc,
                           XTColor color = XTColor()) TAIL
#ifndef RENDER_CUSTOM
{
    // (this will use the double signature on desktop renderers)
    g_render->renderTexture(xDst, yDst, wDst, hDst,
                            tx,
                            xSrc, ySrc,
                            color);
}
#endif

void renderTextureBasic(double xDst, double yDst, double wDst, double hDst,
                       StdPicture &tx,
                       double xSrc, double ySrc,
                       XTColor color = XTColor()) = delete;

E_INLINE void renderTexture(double xDst, double yDst, double wDst, double hDst,
                           StdPicture &tx,
                           int xSrc, int ySrc,
                           XTColor color = XTColor()) TAIL
#ifndef RENDER_CUSTOM
{
    g_render->renderTexture(xDst, yDst, wDst, hDst,
                            tx,
                            xSrc, ySrc,
                            color);
}
#endif

E_INLINE void renderTextureFL(double xDst, double yDst, double wDst, double hDst,
                             StdPicture &tx,
                             int xSrc, int ySrc,
                             double rotateAngle = 0.0, FPoint_t *center = nullptr, unsigned int flip = X_FLIP_NONE,
                             XTColor color = XTColor()) TAIL
#ifndef RENDER_CUSTOM
{
    g_render->renderTextureFL(xDst, yDst, wDst, hDst,
                              tx,
                              xSrc, ySrc,
                              rotateAngle, center, flip,
                              color);
}
#endif

E_INLINE void renderTexture(float xDst, float yDst, StdPicture &tx,
                           XTColor color = XTColor()) TAIL
#ifndef RENDER_CUSTOM
{
    g_render->renderTexture(xDst, yDst, tx,
                            color);
}
#endif

E_INLINE void renderTextureBasic(int xDst, int yDst, StdPicture &tx,
                           XTColor color = XTColor()) TAIL
#ifndef RENDER_CUSTOM
{
    // (this will use the float signature on desktop renderers)
    g_render->renderTexture(xDst, yDst, tx, color);
}
#endif

void renderTextureBasic(float xDst, float yDst, StdPicture &tx,
                           XTColor color = XTColor()) = delete;

/*!
 * \brief Draws the particle system at a particular camera offset
 * \param tx Source particle system
 * \param camX current camera X position (same as vScreenX)
 * \param camY current camera Y position (same as vScreenY)
 *
 * No-op if particle effects are not supported.
 */
E_INLINE void renderParticleSystem(StdPicture &tx,
                                   double camX,
                                   double camY) TAIL
#ifndef RENDER_CUSTOM
{
    return g_render->renderParticleSystem(tx, camX, camY);
}
#endif


#ifdef USE_RENDER_BLOCKING
E_INLINE bool renderBlocked() TAIL
#   ifndef RENDER_CUSTOM
{
    return AbstractRender_t::renderBlocked();
}
#   endif

E_INLINE void setBlockRender(bool b) TAIL
#   ifndef RENDER_CUSTOM
{
    AbstractRender_t::setBlockRender(b);
}
#   endif
#endif // USE_RENDER_BLOCKING


#ifdef USE_SCREENSHOTS_AND_RECS

E_INLINE void makeShot() TAIL
#   ifndef RENDER_CUSTOM
{
    AbstractRender_t::makeShot();
}
#   endif

#endif

#ifdef PGE_ENABLE_VIDEO_REC

E_INLINE void toggleGifRecorder() TAIL
#   ifndef RENDER_CUSTOM
{
    AbstractRender_t::toggleGifRecorder();
}
#   endif

E_INLINE void processRecorder() TAIL
#   ifndef RENDER_CUSTOM
{
    AbstractRender_t::processRecorder();
}
#   endif

#endif // USE_SCREENSHOTS_AND_RECS


} // XRender

#ifndef RENDER_CUSTOM
#   undef E_INLINE
#   undef TAIL
#endif


#endif // RENDER_HHHHHH
