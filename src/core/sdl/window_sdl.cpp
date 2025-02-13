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

#ifdef _WIN32
#   include <winternl.h>
#endif
#include <SDL2/SDL.h>
#include <SDL2/SDL_cpuinfo.h>

#include <Logger/logger.h>
#include <Graphics/graphics_funcs.h>

#include "globals.h"
#include "main/game_info.h"
#include "window_sdl.h"
#include "../render.h"
#include "config.h"
#include "../version.h"


#ifdef RENDER_FULLSCREEN_TYPES_SUPPORTED
#   include "pge_cpu_arch.h"

static bool s_hasFrameBuffer = false;

static int s_fsTypeToSDL(int type)
{
    switch(type)
    {
    case 0:
    {
#if defined(PGE_CPU_x86_32) || defined(PGE_CPU_ARM32) || defined(PGE_CPU_PPC32)
        // On 32-bit architecures
        return SDL_WINDOW_FULLSCREEN;
#endif
    }
    case 1:
    default:
        return SDL_WINDOW_FULLSCREEN_DESKTOP;
    case 2:
        return SDL_WINDOW_FULLSCREEN;
    }
}
#endif


#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>

void s_emscriptenFillBrowser()
{
    EmscriptenFullscreenStrategy strategy;
    strategy.scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH;
    strategy.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_HIDEF;
    strategy.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;

    strategy.canvasResizedCallback = nullptr;

    pLogDebug("Activating canvas resize code");

    emscripten_enter_soft_fullscreen("canvas", &strategy);
    EM_ASM(currentFullscreenStrategy_bak = currentFullscreenStrategy;);
}

EM_BOOL s_restoreSoftFullscreen(int eventType, const void *reserved, void *userData)
{
    (void)(eventType);
    (void)(reserved);
    (void)(userData);

    pLogDebug("Restoring canvas resize code after entering fullscreen...");

    EM_ASM(
        currentFullscreenStrategy = currentFullscreenStrategy_bak;
    );
    return false;
}

void s_emscriptenRealFullscreen()
{
    EmscriptenFullscreenStrategy strategy;

    strategy.scaleMode = 0;
    strategy.canvasResolutionScaleMode = 0;
    strategy.filteringMode = 0;
    strategy.canvasResizedCallback = s_restoreSoftFullscreen;

    pLogDebug("Requesting HTML5 fullscreen mode...");

    emscripten_request_fullscreen_strategy("#thextech-document", true, &strategy);
}

void s_emscriptenLeaveRealFullscreen()
{
    EM_ASM(
        if(document.fullscreenElement || document.webkitFullscreenElement)
        {
            if(document.webkitExitFullscreen)
                document.webkitExitFullscreen();
            else
                document.exitFullscreen();

            setTimeout(() => {
                console.log("Restoring canvas on return from fullscreen");
                softFullscreenResizeWebGLRenderTarget();
                window.dispatchEvent(new Event('resize'));
            }, 500);
        }
    );
}

#endif

//! Path to game resources assets (by default it's ~/.PGE_Project/thextech/)
extern std::string AppPath;


WindowSDL::WindowSDL() :
    AbstractWindow_t()
{}

WindowSDL::~WindowSDL()
{}

bool WindowSDL::initSDL(uint32_t windowInitFlags)
{
    SDL_ClearError();

    bool res = true;

    // SDL_GL_ResetAttributes();

#if defined(__SWITCH__) /* On Switch, expect the initial size 1920x1080 */
    const int initWindowW = 1920;
    const int initWindowH = 1080;
#else
    const auto initWindowW = XRender::TargetW;
    const auto initWindowH = XRender::TargetH;
#endif

#if defined(RENDER_FULLSCREEN_ALWAYS)
    windowInitFlags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN;
#else
    windowInitFlags |= SDL_WINDOW_HIDDEN;
#endif

#ifdef RENDER_FULLSCREEN_TYPES_SUPPORTED
    m_fullscreen_type = g_config.fullscreen_type;
#endif

    // restore fullscreen state
    if(m_fullscreen)
        windowInitFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

#ifdef __EMSCRIPTEN__
    // don't use SDL fullscreen API on Emscripten
    windowInitFlags &= ~SDL_WINDOW_FULLSCREEN;
#endif

    std::string window_name = "TheXTech Engine - (TheXTech v" V_LATEST_STABLE ", #" V_BUILD_VER ")";
    if(!g_gameInfo.title.empty())
        window_name = g_gameInfo.titleWindow();

    D_pLogDebug("SDL2: Extra WindowFlags: %u", windowInitFlags);

    m_window = SDL_CreateWindow(window_name.c_str(),
                                SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,
                                initWindowW, initWindowH,
                                SDL_WINDOW_RESIZABLE |
                                SDL_WINDOW_ALLOW_HIGHDPI |
                                windowInitFlags);

    if(m_window == nullptr)
    {
        const char *error = SDL_GetError();
        if(*error != '\0')
            pLogFatal("Unable to create window: %s", error);
        else
            pLogFatal("Unable to create window!");

        SDL_ClearError();
        return false;
    }
    else if(isSdlError())
    {
        const char *error = SDL_GetError();
        pLogCritical("SDL error on window creation: %s", error);
        SDL_ClearError();
    }

    SDL_SetWindowMinimumSize(m_window, 240, 160);

#ifdef __EMSCRIPTEN__ // Set canvas be 1/2 size for a faster rendering
    SDL_SetWindowSize(m_window, XRender::TargetW / 2, XRender::TargetH / 2);
#elif defined(__ANDROID__) || defined(__SWITCH__) // Set as small as possible
    SDL_SetWindowMinimumSize(m_window, 200, 150);
#elif defined(VITA)
    SDL_SetWindowSize(m_window, 960, 544);
#else
    if(g_config.scale_mode == Config_t::SCALE_FIXED_05X)
        SDL_SetWindowSize(m_window, XRender::TargetW / 2, XRender::TargetH / 2);
    else if(g_config.scale_mode == Config_t::SCALE_FIXED_2X)
        SDL_SetWindowSize(m_window, XRender::TargetW * 2, XRender::TargetH * 2);
    else
        SDL_SetWindowSize(m_window, XRender::TargetW, XRender::TargetH);
#endif //__EMSCRIPTEN__

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

#ifdef RENDER_FULLSCREEN_TYPES_SUPPORTED
    if(m_screen_orig_w == 0 || m_screen_orig_h == 0)
    {
        int display = SDL_GetWindowDisplayIndex(m_window);
        SDL_DisplayMode mode;

        if(SDL_GetCurrentDisplayMode(display, &mode) < 0)
        {
            pLogWarning("Getting current display mode failed: %s", SDL_GetError());
            return -1;
        }

        m_screen_orig_w = mode.w;
        m_screen_orig_h = mode.h;
    }
#endif // RENDER_FULLSCREEN_TYPES_SUPPORTED

#ifdef __EMSCRIPTEN__
    EM_ASM(
        document.documentElement.id = "thextech-document";
        document.body.style.overflow = "hidden";
    );
    s_emscriptenFillBrowser();
#endif

#ifdef RENDER_FULLSCREEN_ALWAYS // Use a full-screen on Android & PS Vita mode by default
    setFullScreen(true);
    show();
#else
#   ifdef RENDER_FULLSCREEN_TYPES_SUPPORTED
    setFullScreenType(g_config.fullscreen_type);
    setFullScreen(g_config.fullscreen);
#   endif
#   ifdef _WIN32
    show();
#   endif
#endif

    return res;
}

void WindowSDL::close()
{
    if(m_window)
    {
#ifdef RENDER_FULLSCREEN_TYPES_SUPPORTED
        // Turn the windowed mode before quit, otherwise, on some old systems screen resolution won't restore back
        if((SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN)
            SDL_SetWindowFullscreen(m_window, SDL_FALSE);
#endif
        SDL_DestroyWindow(m_window);
    }
    m_window = nullptr;
}

void WindowSDL::updateWindowIcon()
{
#ifdef _WIN32
    FIBITMAP *img[2];
    img[0] = GraphicsHelps::loadImage(AppPath + "/graphics/ui/icon/thextech_16.png");
    img[1] = GraphicsHelps::loadImage(AppPath + "/graphics/ui/icon/thextech_32.png");

    if(img[0] && !GraphicsHelps::setWindowIcon(m_window, img[0], 16))
    {
        pLogWarning("Unable to setup window icon!");
        SDL_ClearError();
    }

    if(img[1] && !GraphicsHelps::setWindowIcon(m_window, img[1], 32))
    {
        pLogWarning("Unable to setup window icon!");
        SDL_ClearError();
    }

    GraphicsHelps::closeImage(img[0]);
    GraphicsHelps::closeImage(img[1]);
#else//IF _WIN32

    FIBITMAP *img;
#   ifdef __APPLE__
    img = GraphicsHelps::loadImage(AppPath + "/graphics/ui/icon/thextech_256.png");
#   else
    img = GraphicsHelps::loadImage(AppPath + "/graphics/ui/icon/thextech_32.png");
#   endif //__APPLE__

    if(img)
    {
        SDL_Surface *sIcon = GraphicsHelps::fi2sdl(img);
        SDL_SetWindowIcon(m_window, sIcon);
        GraphicsHelps::closeImage(img);
        SDL_FreeSurface(sIcon);

        if(isSdlError())
        {
            pLogWarning("Unable to setup window icon!");
            SDL_ClearError();
        }
    }
#endif//IF _WIN32 #else
}

SDL_Window *WindowSDL::getWindow()
{
    return m_window;
}

bool WindowSDL::isSdlError()
{
    const char *error = SDL_GetError();
    return (*error != '\0');
}

void WindowSDL::show()
{
    SDL_ShowWindow(m_window);
}

void WindowSDL::hide()
{
    SDL_HideWindow(m_window);
    showCursor(1);
}

int WindowSDL::showCursor(int show)
{
    return SDL_ShowCursor(show);
}

void WindowSDL::setCursor(WindowCursor_t cursor)
{
    m_cursor = cursor;
    // Do nothing, just remember the last cursor type was set
}

WindowCursor_t WindowSDL::getCursor()
{
    return m_cursor;
}

void WindowSDL::placeCursor(int window_x, int window_y)
{
    if(!this->hasWindowInputFocus())
        return;

    int old_window_x, old_window_y;
    SDL_GetMouseState(&old_window_x, &old_window_y);
    int o_sx, o_sy, n_sx, n_sy;

    XRender::mapToScreen(old_window_x, old_window_y, &o_sx, &o_sy);
    XRender::mapToScreen(window_x, window_y, &n_sx, &n_sy);

    if(n_sx - o_sx < -2 || n_sx - o_sx > 2 || n_sy - o_sy < -2 || n_sy - o_sy > 2)
    {
        int window_w, window_h;
        this->getWindowSize(&window_w, &window_h);
        if(window_x >= 0 && window_x < window_w && window_y >= 0 && window_y < window_h)
            SDL_WarpMouseInWindow(m_window, window_x, window_y);
    }
}

bool WindowSDL::isFullScreen()
{
    return m_fullscreen;
}

int WindowSDL::setFullScreen(bool fs)
{
    if(!m_window)
        return -1;

    if((SDL_GetWindowFlags(m_window) & SDL_WINDOW_RESIZABLE) == 0)
        return -1; // Can't switch fullscreen mode when window is not resizable

    if(fs != m_fullscreen)
    {
        m_fullscreen = fs;

#ifdef __EMSCRIPTEN__
        if(fs)
            s_emscriptenRealFullscreen();
        else
            s_emscriptenLeaveRealFullscreen();

        return m_fullscreen;
#else
        if(fs)
        {
            // Swith to FULLSCREEN mode
#ifdef RENDER_FULLSCREEN_TYPES_SUPPORTED
            if(SDL_SetWindowFullscreen(m_window, m_fullscreen_type_real) < 0)
            {
                pLogWarning("Setting fullscreen failed: %s", SDL_GetError());
                return -1;
            }

            syncFullScreenRes();
#else
            if(SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP) < 0)
            {
                pLogWarning("Setting fullscreen failed: %s", SDL_GetError());
                return -1;
            }
#endif

            // Hide mouse cursor in full screen mdoe
            SDL_ShowCursor(SDL_DISABLE);
            return 1;
        }
        else
        {
            // Swith to WINDOWED mode
            if(SDL_SetWindowFullscreen(m_window, SDL_FALSE) < 0)
            {
                pLogWarning("Setting windowed failed: %s", SDL_GetError());
                return -1;
            }
            return 0;
        }
#endif
    }

    return 0;
}


#ifdef RENDER_FULLSCREEN_TYPES_SUPPORTED

int WindowSDL::setFullScreenType(int type)
{
    bool change_needed = m_fullscreen_type != type;
    m_fullscreen_type = type;
    m_fullscreen_type_real = s_fsTypeToSDL(type);

    if(change_needed && m_fullscreen)
    {
        if(m_fullscreen_type_real == SDL_WINDOW_FULLSCREEN_DESKTOP)
        {
            if(SDL_SetWindowFullscreen(m_window, m_fullscreen_type_real) < 0)
            {
                pLogWarning("Setting fullscreen failed: %s", SDL_GetError());
                return -1;
            }
        }
        else if(m_fullscreen_type_real == SDL_WINDOW_FULLSCREEN)
            return syncFullScreenRes();
    }

    return 0;
}

int WindowSDL::getFullScreenType()
{
    int flags = SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN_DESKTOP;

    if(flags == SDL_WINDOW_FULLSCREEN_DESKTOP)
        return FULLSCREEN_TYPE_DESKTOP;

    if(flags == SDL_WINDOW_FULLSCREEN)
        return FULLSCREEN_TYPE_REAL;

    return 0;
}

int WindowSDL::syncFullScreenRes()
{
    if(m_fullscreen_type_real != SDL_WINDOW_FULLSCREEN || !m_window)
        return 0; // Nothing to do

    int dst_w = XRender::TargetW;
    int dst_h = (dst_w * m_screen_orig_h) / m_screen_orig_w;

    SDL_SetWindowSize(m_window, dst_w, dst_h);

    pLogDebug("Toggling screen into %d x %d resolution", dst_w, dst_h);

    if(SDL_SetWindowFullscreen(m_window, m_fullscreen_type_real) < 0)
    {
        pLogWarning("Setting fullscreen failed: %s", SDL_GetError());
        return -1;
    }

    return 0;
}

#endif // RENDER_FULLSCREEN_TYPES_SUPPORTED

void WindowSDL::restoreWindow()
{
    SDL_RestoreWindow(m_window);
}

void WindowSDL::setWindowSize(int w, int h)
{
    // doesn't make sense on Emscripten, actually causes crashes on Wii U
#if !defined(__EMSCRIPTEN__) && !defined(__WIIU__)
    // try to figure out whether requested size is bigger than the screen
    int display = SDL_GetWindowDisplayIndex(m_window);
    if(display >= 0)
    {
        SDL_Rect bounds;
        if(SDL_GetDisplayUsableBounds(display, &bounds) == 0)
        {
            if(w > bounds.w || h > bounds.h)
                return;
        }
    }

    SDL_SetWindowSize(m_window, w, h);
#else
    UNUSED(w); UNUSED(h);
#endif
}

void WindowSDL::getWindowSize(int *w, int *h)
{
    SDL_GetWindowSize(m_window, w, h);
}

bool WindowSDL::hasWindowInputFocus()
{
    if(!m_window)
        return false;
    Uint32 flags = SDL_GetWindowFlags(m_window);
    return (flags & SDL_WINDOW_INPUT_FOCUS) != 0;
}

bool WindowSDL::hasWindowMouseFocus()
{
    if(!m_window)
        return false;
    Uint32 flags = SDL_GetWindowFlags(m_window);
    return (flags & SDL_WINDOW_MOUSE_FOCUS) != 0;
}

bool WindowSDL::isMaximized()
{
    if(!m_window)
        return false;
    Uint32 flags = SDL_GetWindowFlags(m_window);
    return (flags & SDL_WINDOW_MAXIMIZED) != 0;
}

void WindowSDL::setTitle(const char* title)
{
    if(!m_window)
        return;

    SDL_SetWindowTitle(m_window, title);
}
