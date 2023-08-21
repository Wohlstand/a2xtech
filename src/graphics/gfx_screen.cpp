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

#include <sdl_proxy/sdl_stdinc.h>

#include "../globals.h"
#include "../graphics.h"
#include "../player.h"
#include "../sound.h"
#include "../change_res.h"
#include "../load_gfx.h"
#include "../core/window.h"
#include "../compat.h"
#include "config.h"

#include "../compat.h"
#include "config.h"
#include "core/render.h"

// Copies all Screen and vScreen info, except size, from source to dest. Centers screens.
void s_CopyScreen(Screen_t& dest, const Screen_t& source)
{
    for(int v = 1; v <= maxLocalPlayers; v++)
    {
        vScreen_t& d_vscreen = dest.vScreen(v);
        vScreen_t& s_vscreen = source.vScreen(v);

        // copy all "ordinary" fields
        static_cast<qScreen_t&>(d_vscreen) = static_cast<qScreen_t&>(s_vscreen);

        d_vscreen.Visible   = s_vscreen.Visible;
        d_vscreen.tempX     = s_vscreen.tempX;
        d_vscreen.TempY     = s_vscreen.TempY;
        d_vscreen.TempDelay = s_vscreen.TempDelay;
    }

    dest.Type  = source.Type;
    dest.DType = source.DType;
}

void SetScreenType(Screen_t& screen)
{
    // moved this code from game_main.cpp, but it occured elsewhere also
    //   it was always called before setup screens, now it is a part of setup screens.
    //   better to have it in one place so it can be updated
    if(numPlayers == 1)
        screen.Type = 0; // Follow 1 player
    else if(numPlayers == 2)
        screen.Type = 5; // Dynamic screen
    else
    {
        // screen.Type = 3 'Average, no one leaves the screen
        screen.Type = 2; // Average
    }

    // special cases
    if(SingleCoop > 0)
        screen.Type = 6;
    if(GameMenu)
        screen.Type = 2;
    if(GameOutro)
        screen.Type = 7;
    if(LevelEditor)
        screen.Type = 0;
}

// Sets up the split lines
void SetupScreens(Screen_t& screen, bool reset)
{
    // keep canonical screen in sync
    if(!screen.is_canonical())
    {
        Screen_t& c_screen = screen.canonical_screen();
        SetupScreens(c_screen, reset);

        // if needed, defer to canonical screen logic
        if(!g_compatibility.free_level_res)
        {
            s_CopyScreen(screen, c_screen);
            return;
        }
    }

    SetScreenType(screen);

    vScreen_t& vscreen1 = screen.vScreen(1);
    vScreen_t& vscreen2 = screen.vScreen(2);

    switch(ScreenType)
    {
    case 0: // Follows Player 1
        vscreen1.Height = screen.H;
        vscreen1.Width = screen.W;
        vscreen1.Left = 0;
        vscreen1.Top = 0;
        vscreen2.Visible = false;
        break;
    case 1: // Split Screen vertical
        vscreen1.Height = screen.H / 2;
        vscreen1.Width = screen.W;
        vscreen1.Left = 0;
        vscreen1.Top = 0;
        vscreen2.Height = screen.H / 2;
        vscreen2.Width = screen.W;
        vscreen2.Left = 0;
        vscreen2.Top = screen.H / 2;
        break;
    case 2: // Follows all players
        vscreen1.Height = screen.H;
        vscreen1.Width = screen.W;
        vscreen1.Left = 0;
        vscreen1.Top = 0;
        vscreen2.Visible = false;
        break;
    case 3: // Follows all players. Noone leaves the screen
        vscreen1.Height = screen.H;
        vscreen1.Width = screen.W;
        vscreen1.Left = 0;
        vscreen1.Top = 0;
        vscreen2.Visible = false;
        break;
    case 4: // Split Screen horizontal
        vscreen1.Height = screen.H;
        vscreen1.Width = screen.W / 2;
        vscreen1.Left = 0;
        vscreen1.Top = 0;
        vscreen2.Height = screen.H;
        vscreen2.Width = screen.W / 2;
        vscreen2.Left = screen.W / 2;
        vscreen2.Top = 0;
        break;
    case 5: // Dynamic screen detection
        vscreen1.Height = screen.H;
        vscreen1.Width = screen.W;
        vscreen1.Left = 0;
        vscreen1.Top = 0;

        if(reset)
            vscreen2.Visible = false;
        break;
    case 6: // VScreen Coop
        vscreen1.Height = screen.H;
        vscreen1.Width = screen.W;
        vscreen1.Left = 0;
        vscreen1.Top = 0;
        vscreen2.Height = screen.H;
        vscreen2.Width = screen.W;
        vscreen2.Left = 0;
        vscreen2.Top = 0;
        break;
    case 7: // Credits
    case 8: // netplay
        vscreen1.Left = 0;
        vscreen1.Height = screen.H;
        vscreen1.Top = 0;
        vscreen1.Width = screen.W;
        vscreen2.Visible = false;
        break;
#if 0 // Merged with the branch above because they both are same
    case 8: // netplay
        vscreen1.Left = 0;
        vscreen1.Width = 800;
        vscreen1.Top = 0;
        vscreen1.Height = 600;
        vscreen2.Visible = false;
        break;
#endif
//    End If
    }
}

void SetupScreens(bool reset)
{
    SetupScreens(Screens[0], reset);
}

void DynamicScreen(Screen_t& screen, bool mute)
{
    // if needed, defer to canonical screen logic
    if(!g_compatibility.free_level_res && !screen.is_canonical())
    {
        Screen_t& c_screen = screen.canonical_screen();
        DynamicScreen(c_screen, mute);

        for(int v = 1; v <= 2; v++)
        {
            vScreen_t& vscreen = screen.vScreen(v);
            vScreen_t& c_vscreen = c_screen.vScreen(v);

            // copy all "ordinary" fields
            static_cast<qScreen_t&>(vscreen) = static_cast<qScreen_t&>(c_vscreen);

            vscreen.Visible = c_vscreen.Visible;
            vscreen.tempX = c_vscreen.tempX;
            vscreen.TempY = c_vscreen.TempY;
            vscreen.TempDelay = c_vscreen.TempDelay;
        }

        screen.DType = c_screen.DType;
        return;
    }
    // keep canonical screen in sync
    else if(!screen.is_canonical())
    {
        DynamicScreen(screen.canonical_screen(), true);
    }

    int A = 0;

    vScreen_t& vscreen1 = screen.vScreen(1);
    vScreen_t& vscreen2 = screen.vScreen(2);

    GetvScreenAverage(vscreen1);

    for(A = 1; A <= numPlayers; A++)
    {
        if(Player[A].Effect == 6)
            return;
    }

    for(A = 1; A <= numPlayers; A++)
    {
        if(Player[A].Mount == 2)
            Player[A].Location.Height = 0;
    }

    Player_t& p1 = Player[screen.players[0]];
    Player_t& p2 = Player[screen.players[1]];

    if(CheckDead() == 0)
    {
        if(p1.Section == p2.Section)
        {
            if(level[p1.Section].Width - level[p1.Section].X > screen.W && (((!vscreen2.Visible && p2.Location.X + vscreen1.X >= screen.W * 0.75 - p2.Location.Width / 2.0) || (vscreen2.Visible && p2.Location.X + vscreen1.X >= screen.W * 0.75 - p2.Location.Width / 2.0)) && (p1.Location.X < level[p1.Section].Width - screen.W * 0.75 - p1.Location.Width / 2.0)))
            {
                vscreen2.Height = screen.H;
                vscreen2.Width = screen.W / 2;
                vscreen2.Left = screen.W / 2.0;
                vscreen2.Top = 0;
                vscreen1.Height = screen.H;
                vscreen1.Width = screen.W / 2;
                vscreen1.Left = 0;
                vscreen1.Top = 0;
                GetvScreenAverage2(vscreen1);
                if(screen.DType != 1 && !mute)
                    PlaySound(SFX_Camera);
                for(A = 1; A <= 2; A++)
                {
                    vScreen_t& vscreena = screen.vScreen(A);
                    Player_t& p = Player[screen.players[A - 1]];

                    vscreena.TempDelay = 200;
                    vscreena.tempX = 0;
                    vscreena.TempY = -vscreen1.Y + screen.H * 0.5 - p.Location.Y - vScreenYOffset - p.Location.Height;
                }
                vscreen2.Visible = true;
                screen.DType = 1;
            }
            else if(level[p1.Section].Width - level[p1.Section].X > screen.W && (((!vscreen2.Visible && p1.Location.X + vscreen1.X >= screen.W * 0.75 - p1.Location.Width / 2.0) || (vscreen2.Visible && p1.Location.X + vscreen2.X >= screen.W * 0.75 - p1.Location.Width / 2.0)) && (p2.Location.X < level[p1.Section].Width - screen.W * 0.75 - p2.Location.Width / 2.0)))
            {
                vscreen1.Height = screen.H;
                vscreen1.Width = screen.W / 2;
                vscreen1.Left = screen.W / 2.0;
                vscreen1.Top = 0;
                vscreen2.Height = screen.H;
                vscreen2.Width = screen.W / 2;
                vscreen2.Left = 0;
                vscreen2.Top = 0;
                GetvScreenAverage2(vscreen1);
                if(screen.DType != 2 && !mute)
                    PlaySound(SFX_Camera);
                for(A = 1; A <= 2; A++)
                {
                    vScreen_t& vscreena = screen.vScreen(A);
                    Player_t& p = Player[screen.players[A - 1]];

                    vscreena.TempDelay = 200;
                    vscreena.tempX = 0;
                    vscreena.TempY = -vscreen1.Y + screen.H * 0.5 - p.Location.Y - vScreenYOffset - p.Location.Height;
                }
                screen.DType = 2;
                vscreen2.Visible = true;
            }
            else if(level[p1.Section].Height - level[p1.Section].Y > screen.H && ((!vscreen2.Visible && p1.Location.Y + vscreen1.Y >= screen.H * 0.75 - vScreenYOffset - p1.Location.Height) || (vscreen2.Visible && p1.Location.Y + vscreen2.Y >= screen.H * 0.75 - vScreenYOffset - p1.Location.Height)) && (p2.Location.Y < level[p1.Section].Height - screen.H * 0.75 - vScreenYOffset - p2.Location.Height))
            {
                vscreen1.Height = screen.H / 2;
                vscreen1.Width = screen.W;
                vscreen1.Left = 0;
                vscreen1.Top = screen.H / 2.0;
                vscreen2.Height = screen.H / 2;
                vscreen2.Width = screen.W;
                vscreen2.Left = 0;
                vscreen2.Top = 0;
                GetvScreenAverage2(vscreen1);
                if(screen.DType != 3 && !mute)
                    PlaySound(SFX_Camera);
                for(A = 1; A <= 2; A++)
                {
                    vScreen_t& vscreena = screen.vScreen(A);
                    Player_t& p = Player[screen.players[A - 1]];

                    vscreena.TempDelay = 200;
                    vscreena.TempY = 0;
                    vscreena.tempX = -vscreen1.X + screen.W * 0.5 - p.Location.X - p.Location.Width * 0.5;
                }
                vscreen2.Visible = true;
                screen.DType = 3;
            }
            else if(level[p1.Section].Height - level[p1.Section].Y > screen.H && ((!vscreen2.Visible && p2.Location.Y + vscreen1.Y >= screen.H * 0.75 - vScreenYOffset - p2.Location.Height) || (vscreen2.Visible && p2.Location.Y + vscreen1.Y >= screen.H * 0.75 - vScreenYOffset - p2.Location.Height)) && (p1.Location.Y < level[p1.Section].Height - screen.H * 0.75 - vScreenYOffset - p1.Location.Height))
            {
                vscreen1.Height = screen.H / 2;
                vscreen1.Width = screen.W;
                vscreen1.Left = 0;
                vscreen1.Top = 0;
                vscreen2.Height = screen.H / 2;
                vscreen2.Width = screen.W;
                vscreen2.Left = 0;
                vscreen2.Top = screen.H / 2.0;
                GetvScreenAverage2(vscreen1);
                if(screen.DType != 4 && !mute)
                    PlaySound(SFX_Camera);
                for(A = 1; A <= 2; A++)
                {
                    vScreen_t& vscreena = screen.vScreen(A);
                    Player_t& p = Player[screen.players[A - 1]];

                    vscreena.TempDelay = 200;
                    vscreena.TempY = 0;
                    vscreena.tempX = -vscreen1.X + screen.W * 0.5 - p.Location.X - p.Location.Width * 0.5;
                }
                vscreen2.Visible = true;
                screen.DType = 4;
            }
            else
            {
                if(vscreen2.Visible)
                {
                    if(screen.DType != 5 && !mute)
                        PlaySound(SFX_Camera);
                    vscreen2.Visible = false;
                    vscreen1.Height = screen.H;
                    vscreen1.Width = screen.W;
                    vscreen1.Left = 0;
                    vscreen1.Top = 0;
                    vscreen1.tempX = 0;
                    vscreen1.TempY = 0;
                    vscreen2.tempX = 0;
                    vscreen2.TempY = 0;
                }
                screen.DType = 5;
            }
            for(A = 1; A <= 2; A++)
            {
                vScreen_t& vscreena = screen.vScreen(A);

                if(vscreena.TempY > (vscreena.Height * 0.25))
                    vscreena.TempY = (vscreena.Height * 0.25);
                if(vscreena.TempY < -(vscreena.Height * 0.25))
                    vscreena.TempY = -(vscreena.Height * 0.25);
                if(vscreena.tempX > (vscreena.Width * 0.25))
                    vscreena.tempX = (vscreena.Width * 0.25);
                if(vscreena.tempX < -(vscreena.Width * 0.25))
                    vscreena.tempX = -(vscreena.Width * 0.25);
            }
        }
        else
        {
            vscreen1.Height = screen.H / 2;
            vscreen1.Width = screen.W;
            vscreen1.Left = 0;
            vscreen1.Top = 0;
            vscreen2.Height = screen.H / 2;
            vscreen2.Width = screen.W;
            vscreen2.Left = 0;
            vscreen2.Top = screen.H / 2.0;
            vscreen1.tempX = 0;
            vscreen1.TempY = 0;
            vscreen2.tempX = 0;
            vscreen2.TempY = 0;
            GetvScreenAverage2(vscreen1);
            if(screen.DType != 6 && !mute)
                PlaySound(SFX_Camera);
            screen.DType = 6;
            vscreen2.Visible = true;
        }
    }
    else
    {
        if(vscreen2.Visible)
        {
            vscreen2.Visible = false;
            // vscreen1.Visible = false; // Useless, because code below sets it as TRUE back
            vscreen1.Height = screen.H;
            vscreen1.Width = screen.W;
            vscreen1.Left = 0;
            vscreen1.Top = 0;
            vscreen1.Visible = true;
            vscreen1.tempX = 0;
            vscreen1.TempY = 0;
            vscreen2.tempX = 0;
            vscreen2.TempY = 0;
        }
    }
    for(A = 1; A <= numPlayers; A++)
    {
        if(Player[A].Mount == 2)
            Player[A].Location.Height = 128;
    }
}

// NEW: limit vScreens to playable section area and center them on the real screen
void CenterScreens(Screen_t& screen)
{
    // keep canonical screen in sync
    if(!screen.is_canonical())
    {
        Screen_t& c_screen = screen.canonical_screen();

        // still needed in case of small sections
        CenterScreens(c_screen);

        // if needed, defer to canonical screen logic
        if(!g_compatibility.free_level_res)
        {
            s_CopyScreen(screen, c_screen);

            // center c_screen on screen
            int offX = (screen.W - c_screen.W) / 2;
            int offY = (screen.H - c_screen.H) / 2;

            for(int v = 1; v <= maxLocalPlayers; v++)
            {
                screen.vScreen(v).ScreenLeft += offX;
                screen.vScreen(v).ScreenTop  += offY;
            }

            return;
        }
    }

    for(int v = 1; v <= maxLocalPlayers; v++)
    {
        vScreen_t& vscreen = screen.vScreen(v);
        const Player_t& p = Player[screen.players[v - 1]];
        const Location_t& section = level[p.Section];

        vscreen.ScreenLeft = vscreen.Left;
        vscreen.ScreenTop  = vscreen.Top;

        bool in_map = (LevelSelect && !GameMenu);

        // skip centering in all these places (world map sections handled elsewhere)
        if(GameOutro || LevelEditor || WorldEditor || in_map)
            continue;

        // restrict the vScreen to the level if the level is smaller than the screen
        double MaxWidth = section.Width - section.X;
        double MaxHeight = section.Height - section.Y;

        if(MaxWidth < vscreen.Width)
        {
            int left_from_center = vscreen.ScreenLeft - (screen.W / 2);
            int right_from_center = (screen.W / 2) - (vscreen.ScreenLeft + vscreen.Width);
            int total_from_center = (left_from_center + right_from_center);
            int size_diff = vscreen.Width - MaxWidth;

            // Move towards center of screen. If left is on center, don't need to move left side of screen.
            // If right is on center, need to fully move left so right can stay. Yields following formula:
            vscreen.ScreenLeft += size_diff * left_from_center / total_from_center;

            vscreen.Width = MaxWidth;
        }

        if(MaxHeight < vscreen.Height)
        {
            int top_from_center = vscreen.ScreenTop - (screen.H / 2);
            int bottom_from_center = (screen.H / 2) - (vscreen.ScreenTop + vscreen.Height);
            int total_from_center = (top_from_center + bottom_from_center);
            int size_diff = vscreen.Height - MaxHeight;

            // Move towards center of screen. If top is on center, don't need to move top side of screen.
            // If bottom is on center, need to fully move top so bottom can stay. Yields following formula:
            vscreen.ScreenTop += size_diff * top_from_center / total_from_center;

            vscreen.Height = MaxHeight;
        }
    }
}

// NEW: moves qScreen towards vScreen, now including the screen size
bool Update_qScreen(int Z, int camRate, int resizeRate)
{
    if(Z == 2 && !g_compatibility.modern_section_change)
        return false;

    bool continue_qScreen = true;

    // take the slower option of 2px per second camera (vanilla)
    //   or 2px per second resize, then scale the speed of the faster one to match
    double camRateX = camRate;
    double camRateY = camRate;

    double resizeRateX = resizeRate;
    double resizeRateY = resizeRate;

    double camFramesX_l = std::abs(vScreen[Z].X - qScreenLoc[Z].X) / camRateX;
    double camFramesY_t = std::abs(vScreen[Z].Y - qScreenLoc[Z].Y) / camRateY;

    double camFramesX_r = std::abs(vScreen[Z].X - vScreen[Z].Width - qScreenLoc[Z].X + qScreenLoc[Z].Width) / camRateX;
    double camFramesY_b = std::abs(vScreen[Z].Y - vScreen[Z].Height - qScreenLoc[Z].Y + qScreenLoc[Z].Height) / camRateY;

    double camFramesX = SDL_min(camFramesX_l, camFramesX_r);
    double camFramesY = SDL_min(camFramesY_t, camFramesY_b);

    double resizeFramesX = std::abs(vScreen[Z].ScreenLeft - qScreenLoc[Z].ScreenLeft) / resizeRateX;
    double resizeFramesY = std::abs(vScreen[Z].ScreenTop - qScreenLoc[Z].ScreenTop) / resizeRateY;

    if(!g_compatibility.modern_section_change)
    {
        resizeFramesX = 0;
        resizeFramesY = 0;
    }

    double qFramesX = (camFramesX > resizeFramesX ? camFramesX : resizeFramesX);
    double qFramesY = (camFramesY > resizeFramesY ? camFramesY : resizeFramesY);

    // don't continue after this frame if it would arrive next frame
    // (this is the intent of the <5 condition in the vanilla game)
    if(qFramesX < 2.5 && qFramesY < 2.5)
        continue_qScreen = false;

    // but, the original condition occurred *after* adding/subtracting 2, so actually
    // the original game would not continue if it would arrive the frame after next, too
    if(!g_compatibility.modern_section_change && qFramesX < 3.5 && qFramesY < 3.5)
        continue_qScreen = false;

    if(qFramesX < 1)
        qFramesX = 1;
    if(qFramesY < 1)
        qFramesY = 1;

    camRateX = std::abs(vScreen[Z].X - qScreenLoc[Z].X)/qFramesX;
    camRateY = std::abs(vScreen[Z].Y - qScreenLoc[Z].Y)/qFramesY;

    resizeRateX = std::abs(vScreen[Z].ScreenLeft - qScreenLoc[Z].ScreenLeft)/qFramesX;
    resizeRateY = std::abs(vScreen[Z].ScreenTop - qScreenLoc[Z].ScreenTop)/qFramesY;

    if(vScreen[Z].X < qScreenLoc[Z].X - camRateX)
        qScreenLoc[Z].X -= camRateX;
    else if(vScreen[Z].X > qScreenLoc[Z].X + camRateX)
        qScreenLoc[Z].X += camRateX;
    else
        qScreenLoc[Z].X = vScreen[Z].X;

    if(vScreen[Z].Y < qScreenLoc[Z].Y - camRateY)
        qScreenLoc[Z].Y -= camRateY;
    else if(vScreen[Z].Y > qScreenLoc[Z].Y + camRateY)
        qScreenLoc[Z].Y += camRateY;
    else
        qScreenLoc[Z].Y = vScreen[Z].Y;

    if(vScreen[Z].ScreenLeft < qScreenLoc[Z].ScreenLeft - resizeRateX)
        qScreenLoc[Z].ScreenLeft -= resizeRateX;
    else if(vScreen[Z].ScreenLeft > qScreenLoc[Z].ScreenLeft + resizeRateX)
        qScreenLoc[Z].ScreenLeft += resizeRateX;
    else
        qScreenLoc[Z].ScreenLeft = vScreen[Z].ScreenLeft;

    if(vScreen[Z].ScreenTop < qScreenLoc[Z].ScreenTop - resizeRateY)
        qScreenLoc[Z].ScreenTop -= resizeRateY;
    else if(vScreen[Z].ScreenTop > qScreenLoc[Z].ScreenTop + resizeRateY)
        qScreenLoc[Z].ScreenTop += resizeRateY;
    else
        qScreenLoc[Z].ScreenTop = vScreen[Z].ScreenTop;

    if(vScreen[Z].Width < qScreenLoc[Z].Width - resizeRateX * 2)
        qScreenLoc[Z].Width -= resizeRateX * 2;
    else if(vScreen[Z].Width > qScreenLoc[Z].Width + resizeRateX * 2)
        qScreenLoc[Z].Width += resizeRateX * 2;
    else
        qScreenLoc[Z].Width = vScreen[Z].Width;

    if(vScreen[Z].Height < qScreenLoc[Z].Height - resizeRateY * 2)
        qScreenLoc[Z].Height -= resizeRateY * 2;
    else if(vScreen[Z].Height > qScreenLoc[Z].Height + resizeRateY * 2)
        qScreenLoc[Z].Height += resizeRateY * 2;
    else
        qScreenLoc[Z].Height = vScreen[Z].Height;

    vScreen[Z].X = qScreenLoc[Z].X;
    vScreen[Z].Y = qScreenLoc[Z].Y;

    // update vScreen width / height
    if(g_compatibility.modern_section_change)
    {
        vScreen[Z].Width -= 2*(std::floor(qScreenLoc[Z].ScreenLeft) - vScreen[Z].ScreenLeft);
        vScreen[Z].Height -= 2*(std::floor(qScreenLoc[Z].ScreenTop) - vScreen[Z].ScreenTop);
        vScreen[Z].ScreenLeft = std::floor(qScreenLoc[Z].ScreenLeft);
        vScreen[Z].ScreenTop = std::floor(qScreenLoc[Z].ScreenTop);
    }

    return continue_qScreen;
}

void SetRes()
{
//    GetCurrentRes(); // Dummy, useless
    resChanged = true;
    ChangeRes(800, 600, 16, 60); // 800x600 pixels, 16 bit Color, 60Hz
    XWindow::showCursor(0);
}
