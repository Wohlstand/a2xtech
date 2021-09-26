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

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <string>
#include "location.h"

enum
{
    SHAKE_RANDOM = 0,
    SHAKE_SEQUENTIAL
};

void doShakeScreen(int force, int type);
void doShakeScreen(int forceX, int forceY, int type, int duration, double decay);
void doShakeScreenClear();

// Public Sub UpdateGraphics2() 'draws GFX to screen when on the world map/world map editor
// draws GFX to screen when on the world map/world map editor
void UpdateGraphics2(bool skipRepaint = false);
// Public Sub UpdateGraphics() 'This draws the graphic to the screen when in a level/game menu/outro/level editor
// This draws the graphic to the screen when in a level/game menu/outro/level editor
void UpdateGraphics(bool skipRepaint = false);
// Public Sub GetvScreen(A As Integer) ' Get the screen position
//  Get the screen position
void GetvScreen(int A);
// Public Sub GetvScreenAverage() ' Get the average screen position for all players
//  Get the average screen position for all players
void GetvScreenAverage();
// Public Sub GetvScreenAverage2() ' Get the average screen position for all players with no level edge detection
//  Get the average screen position for all players with no level edge detection
void GetvScreenAverage2();
// Public Sub SetupGraphics()
//! DUMMY AND USELESS
void SetupGraphics();
// Public Sub SetupEditorGraphics()
//! DUMMY AND USELESS
void SetupEditorGraphics();
// Public Sub SetupScreens()
void SetupScreens();
// Public Sub DynamicScreen() 'for the split screen stuff
// for the split screen stuff
void DynamicScreen();
// Public Sub SuperPrint(SuperWords As String, Font As Integer, X As Single, Y As Single) 'prints text to the screen
// prints text to the screen
int SuperTextPixLen(const std::string &SuperWords, int Font);
void SuperPrint(const std::string &SuperWords, int Font, float X, float Y, float r = 1.f, float g = 1.f, float b = 1.f, float a = 1.f);
void SuperPrintRightAlign(const std::string &SuperWords, int Font, float X, float Y, float r = 1.f, float g = 1.f, float b = 1.f, float a = 1.f);
void SuperPrintScreenCenter(const std::string &SuperWords, int Font, float Y, float r = 1.f, float g = 1.f, float b = 1.f, float a = 1.f);
// Public Sub SetRes()
void SetRes();
// Public Function CheckKey(newStrizzle As String) As String
//std::string CheckKey(std::string newStrizzle); // USELESS
// Private Sub SpecialFrames() 'update frames for special things such as coins and kuribo's shoe
extern void SpecialFrames();//PRIVATE
// update frames for special things such as coins and kuribo's shoe

// Public Sub DrawBackground(S As Integer, Z As Integer) 'draws the background to the screen
// draws the background to the screen
void DrawBackground(int S, int Z);
// Public Sub PlayerWarpGFX(A As Integer, tempLocation As Location, X2 As Single, Y2 As Single)
void PlayerWarpGFX(int A, Location_t &tempLocation, float &X2, float &Y2);
// Public Sub NPCWarpGFX(A As Integer, tempLocation As Location, X2 As Single, Y2 As Single)
void NPCWarpGFX(int A, Location_t &tempLocation, float &X2, float &Y2);
// Public Sub ChangeScreen() 'change from fullscreen to windowed mode
// change from fullscreen to windowed mode
void ChangeScreen();
// Public Sub GetvScreenCredits() ' Get the average screen position for all players for the games outro
//  Get the average screen position for all players for the games outro
void GetvScreenCredits();
// Public Sub DoCredits() 'print credits
// print credits
void DoCredits();
void DrawCredits();
// Public Sub DrawInterface(Z As Integer, numScreens) 'draws the games interface
// draws the games interface
void DrawInterface(int Z, int numScreens);
// Public Function pfrX(plrFrame As Integer) As Integer
int pfrX(int plrFrame);
// Public Function pfrY(plrFrame As Integer) As Integer
int pfrY(int plrFrame);
// Public Sub GameThing()
void GameThing();
// Public Sub DrawPlayer(A As Integer, Z As Integer)
void DrawPlayer(int A, int Z);
// Public Sub ScreenShot()
void ScreenShot();
// Public Sub DrawFrozenNPC(Z As Integer, A As Integer)
void DrawFrozenNPC(int Z, int A);



#endif // GRAPHICS_H
