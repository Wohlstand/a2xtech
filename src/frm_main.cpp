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

#include <Logger/logger.h>

#if defined(__WII__) || defined(__3DS__) || !defined(RENDER_CUSTOM)
#   include <Graphics/graphics_funcs.h>
#endif

#include "../version.h"

#include "gfx.h"
#include "config.h"

#include "core/render.h"
#include "core/window.h"
#include "core/msgbox.h"
#include "core/events.h"

#ifndef THEXTECH_NO_SDL_CORE
#   include "core/sdl/sdl_core.h"
#endif

#ifdef CORE_EVERYTHING_SDL

#   include "core/sdl/render_sdl.h"
#   include "core/opengl/render_gl.h"

#   define USE_CORE_RENDER_SDL

#   include "core/sdl/window_sdl.h"
typedef WindowSDL WindowUsed;
#   define USE_CORE_WINDOW_SDL

#   ifdef __WIIU__
#       include "core/wiiu/msgbox_wiiu.h"
typedef MsgBoxWiiU MsgBoxUsed;
#       define SDL_CORE_MSGBOX_WIIU
#   else
#       include "core/sdl/msgbox_sdl.h"
typedef MsgBoxSDL MsgBoxUsed;
#       define SDL_CORE_MSGBOX_SDL
#   endif

#   include "core/sdl/events_sdl.h"
typedef EventsSDL EventsUsed;
#   define USE_CORE_EVENTS_SDL
#endif

#include "fontman/font_manager.h"

#include "frm_main.h"

FrmMain g_frmMain;

bool FrmMain::initSystem(const CmdLineSetup_t &setup)
{
    bool res = false;

    LoadLogSettings(setup.interprocess, setup.verboseLogging);
    //Write into log the application start event
    pLogInfo("<Application started>");
    pLogInfo("TheXTech %s (branch %s, commit %s)", V_LATEST_STABLE, V_BUILD_BRANCH, V_BUILD_VER);

#ifndef THEXTECH_NO_SDL_CORE
    res = CoreSDL::init(setup);
    if(!res)
        return false;
#endif

#if defined(__WII__) || defined(__3DS__) || !defined(RENDER_CUSTOM)
    //Initialize FreeImage
    D_pLogDebugNA("FrmMain: Loading FreeImage...");
    GraphicsHelps::initFreeImage();
#endif

    // Build interfaces
#ifndef WINDOW_CUSTOM
    WindowUsed *window = new WindowUsed();
    m_window.reset(window);
    g_window = m_window.get();
#endif

#ifdef USE_CORE_RENDER_SDL


#   ifdef RENDERGL_SUPPORTED
    bool try_gl = false;

    if(    g_config.render_mode == Config_t::RENDER_ACCELERATED_AUTO
        || g_config.render_mode == Config_t::RENDER_ACCELERATED_OPENGL
        || g_config.render_mode == Config_t::RENDER_ACCELERATED_OPENGL_ES
        || g_config.render_mode == Config_t::RENDER_ACCELERATED_OPENGL_LEGACY
        || g_config.render_mode == Config_t::RENDER_ACCELERATED_OPENGL_ES_LEGACY)
    {
        RenderGL *render = new RenderGL();
        m_render.reset(render);
        g_render = m_render.get();
        try_gl = true;
    }
    else
#   endif // #ifdef RENDERGL_SUPPORTED

    {
        RenderSDL *render = new RenderSDL();
        m_render.reset(render);
        g_render = m_render.get();
    }

#endif

#ifndef MSGBOX_CUSTOM
    MsgBoxUsed *msgbox = new MsgBoxUsed();
    m_msgbox.reset(msgbox);
    g_msgBox = m_msgbox.get();
#endif

#ifndef EVENTS_CUSTOM
    EventsUsed *events = new EventsUsed();
    m_events.reset(events);
    g_events = m_events.get();
#endif

    // Initializing window

#ifdef WINDOW_CUSTOM
    D_pLogDebugNA("FrmMain: Loading XWindow...");
    res = XWindow::init();
#elif defined(USE_CORE_WINDOW_SDL)
    res = window->initSDL(g_render->SDL_InitFlags());
#else
#   error "FIXME: Implement supported window initialization here"
#endif

    if(!res)
    {
#ifndef THEXTECH_NO_SDL_CORE
        CoreSDL::quit();
#endif
        return true;
    }


    // Initializing message box

#ifdef MSGBOX_CUSTOM
    D_pLogDebugNA("FrmMain: Loading XMsgBox...");
    res &= XMsgBox::init();
#elif defined(USE_CORE_WINDOW_SDL) && defined(SDL_CORE_MSGBOX_SDL)
    msgbox->init(window->getWindow());
#elif defined(SDL_CORE_MSGBOX_WIIU)
    // Nothing to do for Wii U
#else
#   error "FIXME: Implement supported message boxes initialization here"
#endif



    // Initializing events

#ifdef EVENTS_CUSTOM
    D_pLogDebugNA("FrmMain: Loading XEvents...");
    res &= XEvents::init();
#else
    events->init(this);
#endif


    // Initializing render
    pLogDebug("Init renderer settings...");

#ifdef RENDER_CUSTOM
    D_pLogDebugNA("FrmMain: Loading XRender...");
    res &= XRender::init();
#elif defined(USE_CORE_WINDOW_SDL) && defined(USE_CORE_RENDER_SDL)
    res = g_render->initRender(window->getWindow());

#   ifdef RENDERGL_SUPPORTED
    if(try_gl && !res)
    {
        pLogDebug("FrmMain: closing Render GL");
        m_render->clearAllTextures();
        m_render->close();

        msgbox->close();
        window->close();

        m_render.reset();
        g_render = nullptr;

        pLogDebug("FrmMain: retrying with Render SDL layer...");

        RenderSDL *render = new RenderSDL();
        m_render.reset(render);
        g_render = m_render.get();

        // make new window
        res = window->initSDL(g_render->SDL_InitFlags());

        if(res)
        {
            msgbox->init(window->getWindow());
            res = g_render->initRender(window->getWindow());
        }
    }
#   endif // #ifdef RENDERGL_SUPPORTED

#else
#   error "FIXME: Implement supported render initialization here"
#endif

    if(!res)
    {
        D_pLogCriticalNA("FrmMain: Error has occured, exiting...");
        freeSystem();
        return true;
    }

#ifdef RENDER_FULLSCREEN_ALWAYS // Use a full-screen on Android & PS Vita mode by default
    XWindow::setFullScreen(true);
    XWindow::show();
#else
#   ifdef RENDER_FULLSCREEN_TYPES_SUPPORTED
    WindowSDL::setHasFrameBuffer(m_render->hasFrameBuffer());
    XWindow::setFullScreenType(g_config.fullscreen_type);
    XWindow::setFullScreen(g_config.fullscreen);
#   endif
#   ifdef _WIN32
    XWindow::show();
#   endif
#endif

    XEvents::doEvents();

    return !res;
}

void FrmMain::freeSystem()
{
    FontManager::quit();

    GFX.unLoad();

#ifdef RENDER_CUSTOM
    XRender::quit();
#else
    if(m_render)
    {
        m_render->clearAllTextures();
        m_render->close();
    }

    m_render.reset();
    g_render = nullptr;
#endif

#ifdef MSGBOX_CUSTOM
    XMsgBox::quit();
#else
    if(m_msgbox)
        m_msgbox->close();

    m_msgbox.reset();
    g_msgBox = nullptr;
#endif

#ifdef EVENTS_CUSTOM
    XEvents::quit();
#else
    m_events.reset();
    g_events = nullptr;
#endif

#ifdef WINDOW_CUSTOM
    XWindow::quit();
#else
    if(m_window)
        m_window->close();

    m_window.reset();
    g_window = nullptr;
#endif

#if defined(__WII__) || defined(__3DS__) || !defined(RENDER_CUSTOM)
    GraphicsHelps::closeFreeImage();
#endif

    pLogInfo("<Application closed>");
    CloseLog();

#ifndef THEXTECH_NO_SDL_CORE
    CoreSDL::quit();
#endif
}

bool FrmMain::restartRenderer()
{
#ifndef THEXTECH_RESTART_RENDERER_SUPPORTED
    return false;
#else // #ifndef THEXTECH_RESTART_RENDERER_SUPPORTED
    pLogDebug("FrmMain: attempting to restart XRender...");

    bool res;

#    ifdef RENDER_CUSTOM
    // custom renderer
    XRender::quit();

    res = XRender::init();

#    elif defined(RENDERGL_SUPPORTED)

    if(m_render)
    {
        m_render->clearAllTextures();
        m_render->close();
    }

    g_msgBox->close();
    g_window->close();

    m_render.reset();
    g_render = nullptr;

    bool try_gl = false;

    CmdLineSetup_t setup;

    if(g_config.render_mode == Config_t::RENDER_ACCELERATED_AUTO || g_config.render_mode == Config_t::RENDER_ACCELERATED_OPENGL || g_config.render_mode == Config_t::RENDER_ACCELERATED_OPENGL_ES || g_config.render_mode == Config_t::RENDER_ACCELERATED_OPENGL_LEGACY || g_config.render_mode == Config_t::RENDER_ACCELERATED_OPENGL_ES_LEGACY)
    {
        m_render.reset(new RenderGL());
        try_gl = true;
    }
    else
    {
        m_render.reset(new RenderSDL());
    }

    g_render = m_render.get();

    res = reinterpret_cast<WindowUsed*>(g_window)->initSDL(g_render->SDL_InitFlags());

    if(res)
    {
        reinterpret_cast<MsgBoxUsed*>(g_msgBox)->init(reinterpret_cast<WindowUsed*>(g_window)->getWindow());
        res = m_render->initRender(reinterpret_cast<WindowUsed*>(g_window)->getWindow());
    }

    if(try_gl && !res)
    {
        pLogDebug("FrmMain: closing Render GL");
        m_render->clearAllTextures();
        m_render->close();

        g_msgBox->close();
        g_window->close();

        m_render.reset();
        g_render = nullptr;

        pLogDebug("FrmMain: retrying with Render SDL layer...");

        RenderSDL *render = new RenderSDL();
        m_render.reset(render);
        g_render = m_render.get();

        res = reinterpret_cast<WindowUsed*>(g_window)->initSDL(g_render->SDL_InitFlags());

        if(res)
        {
            reinterpret_cast<MsgBoxUsed*>(g_msgBox)->init(reinterpret_cast<WindowUsed*>(g_window)->getWindow());
            res = g_render->initRender(reinterpret_cast<WindowUsed*>(g_window)->getWindow());
        }

        res = false;
    }

#    else
    // SDL, no OpenGL

    if(m_render)
    {
        m_render->clearAllTextures();
        m_render->close();
    }

    m_render.reset();
    g_render = nullptr;

    m_render.reset(new RenderSDL());

    g_render = m_render.get();

    CmdLineSetup_t setup;

    res = m_render->initRender(setup, reinterpret_cast<WindowUsed*>(g_window)->getWindow());
#    endif

    XWindow::show();

    return res;
#endif // #ifndef THEXTECH_RESTART_RENDERER_SUPPORTED
}
