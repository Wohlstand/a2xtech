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
#ifndef WINDOW_HHHH
#define WINDOW_HHHH

#include "base/window_types.h"

#ifndef WINDOW_CUSTOM
#   include "sdl_proxy/sdl_stdinc.h"
#   include "base/window_base.h"
#   define E_INLINE SDL_FORCE_INLINE
#   define TAIL
#else
#   define E_INLINE    extern
#   define TAIL ;
#endif


/*!
 *  Window interface
 */
namespace XWindow
{

#ifdef WINDOW_CUSTOM

extern bool init();
extern void quit();

#endif


#ifdef WINDOW_CUSTOM

constexpr bool is_nullptr()
{
    return false;
}

#else

SDL_FORCE_INLINE bool is_nullptr()
{
    return !g_window;
}

#endif


/*!
 * \brief Show the window
 */
E_INLINE void show() TAIL
#ifndef WINDOW_CUSTOM
{
    g_window->show();
}
#endif

/*!
 * \brief Hide the window
 */
E_INLINE void hide() TAIL
#ifndef WINDOW_CUSTOM
{
    g_window->hide();
}
#endif


/*!
 * \brief Updates the window icon based on AppPath
 */
E_INLINE void updateWindowIcon() TAIL
#ifndef WINDOW_CUSTOM
{
    g_window->updateWindowIcon();
}
#endif

/**
 *  \brief Toggle whether or not the cursor is shown.
 *
 *  \param toggle 1 to show the cursor, 0 to hide it, -1 to query the current
 *                state.
 *
 *  \return 1 if the cursor is shown, or 0 if the cursor is hidden.
 */
E_INLINE int showCursor(int show) TAIL
#ifndef WINDOW_CUSTOM
{
    return g_window->showCursor(show);
}
#endif

/*!
 * \brief Change the displayable cursor type
 * \param cursor cursor type
 */
E_INLINE void setCursor(WindowCursor_t cursor) TAIL
#ifndef WINDOW_CUSTOM
{
    g_window->setCursor(cursor);
}
#endif

/*!
 * \brief Get the current cursor type
 * \return Cursor type
 */
E_INLINE WindowCursor_t getCursor() TAIL
#ifndef WINDOW_CUSTOM
{
    return g_window->getCursor();
}
#endif

/*!
 * \brief Place cursor at desired window position
 * \param x and y positions in physical window coordinates
 */
E_INLINE void placeCursor(int window_x, int window_y) TAIL
#ifndef WINDOW_CUSTOM
{
    return g_window->placeCursor(window_x, window_y);
}
#endif

/*!
 * \brief Is full-screen mode active?
 * \return True if the full-screen mode works right now
 */
E_INLINE bool isFullScreen() TAIL
#ifndef WINDOW_CUSTOM
{
    return g_window->isFullScreen();
}
#endif

/*!
 * \brief Change between fullscreen and windowed modes
 * \param fs Fullscreen state
 * \return 1 when full-screen mode toggled, 0 when windowed mode toggled, -1 on any errors
 */
E_INLINE int setFullScreen(bool fs) TAIL
#ifndef WINDOW_CUSTOM
{
    return g_window->setFullScreen(fs);
}
#endif


#ifdef RENDER_FULLSCREEN_TYPES_SUPPORTED
/*!
 * \brief Sets the type of fullscreen (desktop or real)
 * \param type Fullscreen type: 0 auto, 1 desktop, 2 real
 * \return 1 when full-screen mode toggled, 0 when windowed mode toggled, -1 on any errors
 */
E_INLINE int setFullScreenType(int type) TAIL
#ifndef WINDOW_CUSTOM
{
    return g_window->setFullScreenType(type);
}
#endif

/*!
 * \brief Get a type of full-screen (desktop or real)
 * \return type of fullscreen
 */
E_INLINE int getFullScreenType() TAIL
#ifndef WINDOW_CUSTOM
{
    return g_window->getFullScreenType();
}
#endif

/*!
 * \brief Only real full-screen mode: syncs the real resolution with the canvas
 * \return 0 on success, -1 on any errors
 */
E_INLINE int syncFullScreenRes() TAIL
#ifndef WINDOW_CUSTOM
{
    return g_window->syncFullScreenRes();
}
#endif

#endif // RENDER_FULLSCREEN_TYPES_SUPPORTED


/*!
 * \brief Restore the size and position of a minimized or maximized window.
 */
E_INLINE void restoreWindow() TAIL
#ifndef WINDOW_CUSTOM
{
    g_window->restoreWindow();
}
#endif

/**
 * @brief Change window size
 * @param w Width
 * @param h Height
 */
E_INLINE void setWindowSize(int w, int h) TAIL
#ifndef WINDOW_CUSTOM
{
    g_window->setWindowSize(w, h);
}
#endif

/*!
 * \brief Get the current size of the window
 * \param w Width
 * \param h Height
 */
E_INLINE void getWindowSize(int *w, int *h) TAIL
#ifndef WINDOW_CUSTOM
{
    g_window->getWindowSize(w, h);
}
#endif

/*!
 * \brief Does window has an input focus?
 * \return true if window active
 */
E_INLINE bool hasWindowInputFocus() TAIL
#ifndef WINDOW_CUSTOM
{
    return g_window->hasWindowInputFocus();
}
#endif

/*!
 * \brief Does window has a mouse focus?
 * \return true if window has a mouse focus
 */
E_INLINE bool hasWindowMouseFocus() TAIL
#ifndef WINDOW_CUSTOM
{
    return g_window->hasWindowMouseFocus();
}
#endif

/*!
 * \brief Is window maximized (resized to fill desktop)?
 * \return true if window is maximized
 */
E_INLINE bool isMaximized() TAIL
#ifndef WINDOW_CUSTOM
{
    return g_window->isMaximized();
}
#endif

/*!
 * \brief Set the title of the window
 * \param title Title to set, as C string
 */
E_INLINE void setTitle(const char* title) TAIL
#ifndef WINDOW_CUSTOM
{
    return g_window->setTitle(title);
}
#endif

}

#ifndef WINDOW_CUSTOM
#   undef E_INLINE
#   undef TAIL
#endif


#endif // WINDOW_HHHH
