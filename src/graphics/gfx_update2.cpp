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

#include "../globals.h"
#include "../frame_timer.h"
#include "../graphics.h"
#include "../collision.h"
#include "../player.h"
#include "../main/speedrunner.h"
#include "../main/trees.h"
#include "../compat.h"

#include <fmt_format_ne.h>


// draws GFX to screen when on the world map/world map editor
void UpdateGraphics2(bool skipRepaint)
{
    if(!GameIsActive)
        return;

#ifdef __ANDROID__
    if(frmMain.renderBlocked())
        return;
#endif

    float c = ShadowMode ? 0.f : 1.f;
    cycleNextInc();

    if(FrameSkip && frameSkipNeeded())
        return;

    frmMain.setTargetTexture();

    frameNextInc();
    frameRenderStart();

    g_stats.reset();

    // Keep them static to don't re-alloc them for every iteration
    static TilePtrArr  tarr;
    static ScenePtrArr sarr;
    static WorldPathPtrArr parr;
    static WorldLevelPtrArr larr;

    // Reserve 400 elements per every array
    if(tarr.capacity() < 400)
        tarr.reserve(400);
    if(sarr.capacity() < 400)
        sarr.reserve(400);
    if(parr.capacity() < 400)
        parr.reserve(400);
    if(larr.capacity() < 400)
        larr.reserve(400);

    int A = 0;
    int B = 0;
    int Z = 0;
    int WPHeight = 0;
//    Location_t tempLocation;
    Z = 1;

    vScreen[Z].Left = 0;
    vScreen[Z].Top = 0;
    vScreen[Z].Width = ScreenW;
    vScreen[Z].Height = ScreenH;
    SpecialFrames();
    SceneFrame2[1] = SceneFrame2[1] + 1;
    if(SceneFrame2[1] >= 12)
    {
        SceneFrame2[1] = 0;
        SceneFrame[1] = SceneFrame[1] + 1;
        if(SceneFrame[1] >= 4)
            SceneFrame[1] = 0;
        SceneFrame[4] = SceneFrame[1];
        SceneFrame[5] = SceneFrame[1];
        SceneFrame[6] = SceneFrame[1];
        SceneFrame[9] = SceneFrame[1];
        SceneFrame[10] = SceneFrame[1];
        SceneFrame[12] = SceneFrame[1];
        SceneFrame[51] = SceneFrame[1];
        SceneFrame[52] = SceneFrame[1];
        SceneFrame[53] = SceneFrame[1];
        SceneFrame[54] = SceneFrame[1];
        SceneFrame[55] = SceneFrame[1];
    }
    SceneFrame2[27] = SceneFrame2[27] + 1;
    if(SceneFrame2[27] >= 8)
    {
        SceneFrame2[27] = 0;
        SceneFrame[27] = SceneFrame[27] + 1;
        if(SceneFrame[27] >= 12)
            SceneFrame[27] = 0;
        SceneFrame[28] = SceneFrame[27];
        SceneFrame[29] = SceneFrame[27];
        SceneFrame[30] = SceneFrame[27];
    }
    SceneFrame2[33] = SceneFrame2[33] + 1;
    if(SceneFrame2[33] >= 4)
    {
        SceneFrame2[33] = 0;
        SceneFrame[33] = SceneFrame[27] + 1;
        if(SceneFrame[33] >= 14)
            SceneFrame[33] = 0;
        SceneFrame[34] = SceneFrame[33];
    }
    SceneFrame2[62] = SceneFrame2[62] + 1;
    if(SceneFrame2[62] >= 8)
    {
        SceneFrame2[62] = 0;
        SceneFrame[62] = SceneFrame[62] + 1;
        if(SceneFrame[62] >= 8)
            SceneFrame[62] = 0;
        SceneFrame[63] = SceneFrame[62];
    }
    LevelFrame2[2] = LevelFrame2[2] + 1;
    if(LevelFrame2[2] >= 6)
    {
        LevelFrame2[2] = 0;
        LevelFrame[2] = LevelFrame[2] + 1;
        if(LevelFrame[2] >= 6)
            LevelFrame[2] = 0;
        LevelFrame[9] = LevelFrame[2];
        LevelFrame[13] = LevelFrame[2];
        LevelFrame[14] = LevelFrame[2];
        LevelFrame[15] = LevelFrame[2];
        LevelFrame[31] = LevelFrame[2];
        LevelFrame[32] = LevelFrame[2];
    }
    LevelFrame2[8] = LevelFrame2[8] + 1;
    if(LevelFrame2[8] >= 12)
    {
        LevelFrame2[8] = 0;
        LevelFrame[8] = LevelFrame[8] + 1;
        if(LevelFrame[8] >= 4)
            LevelFrame[8] = 0;
    }
    LevelFrame2[12] = LevelFrame2[12] + 1;
    if(LevelFrame2[12] >= 8)
    {
        LevelFrame2[12] = 0;
        LevelFrame[12] = LevelFrame[12] + 1;
        if(LevelFrame[12] >= 2)
            LevelFrame[12] = 0;
    }
    LevelFrame2[25] = LevelFrame2[25] + 1;
    if(LevelFrame2[25] >= 8)
    {
        LevelFrame2[25] = 0;
        LevelFrame[25] = LevelFrame[25] + 1;
        if(LevelFrame[25] >= 4)
            LevelFrame[25] = 0;
        LevelFrame[26] = LevelFrame[25];
    }
    TileFrame2[14] = TileFrame2[14] + 1;
    if(TileFrame2[14] >= 14)
    {
        TileFrame2[14] = 0;
        TileFrame[14] = TileFrame[14] + 1;
        if(TileFrame[14] >= 4)
            TileFrame[14] = 0;
        TileFrame[27] = TileFrame[14];
        TileFrame[241] = TileFrame[14];
    }

//    if(WorldEditor == true)
//    {
//        frmMain.renderTexture(0, 0, ScreenW, ScreenH, 0, 0, 0);
//    }
//    else
//    {
//        frmMain.renderTexture(0, 0, ScreenW, ScreenH, 0, 0, 0);
//    }
    frmMain.clearBuffer();

//    if(TakeScreen == true)
//    {
//        if(LevelEditor == true || MagicHand == true)
//            frmLevelWindow::vScreen[1].AutoRedraw = true;
//        else
//            frmMain.AutoRedraw = true;
//    }

//    If LevelEditor = True Then
//        For A = 1 To numTiles
//            With Tile(A)
//                If vScreenCollision(1, .Location) = True Then
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXTile(.Type), 0, TileHeight(.Type) * TileFrame(.Type), vbSrcCopy
//                End If
//            End With
//        Next A
//        For A = 1 To numScenes
//            With Scene(A)
//                If vScreenCollision(1, .Location) = True Then
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXSceneMask(.Type), 0, SceneHeight(.Type) * SceneFrame(.Type), vbSrcAnd
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXScene(.Type), 0, SceneHeight(.Type) * SceneFrame(.Type), vbSrcPaint
//                End If
//            End With
//        Next A
//        For A = 1 To numWorldPaths
//            With WorldPath(A)
//                If vScreenCollision(1, .Location) = True Then
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXPathMask(.Type), 0, 0, vbSrcAnd
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXPath(.Type), 0, 0, vbSrcPaint
//                End If
//            End With
//        Next A
//        For A = 1 To numWorldLevels
//            With WorldLevel(A)
//                If vScreenCollision(1, .Location) = True Then
//                    If .Path = True Then
//                        BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXLevelMask(0), 0, 0, vbSrcAnd
//                        BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXLevel(0), 0, 0, vbSrcPaint
//                    End If
//                    If .Path2 = True Then
//                        BitBlt myBackBuffer, vScreenX(Z) + .Location.X - 16, vScreenY(Z) + 8 + .Location.Y, 64, 32, GFXLevelMask(29), 0, 0, vbSrcAnd
//                        BitBlt myBackBuffer, vScreenX(Z) + .Location.X - 16, vScreenY(Z) + 8 + .Location.Y, 64, 32, GFXLevel(29), 0, 0, vbSrcPaint
//                    End If
//                    If GFXLevelBig(.Type) = True Then
//                        BitBlt myBackBuffer, vScreenX(Z) + .Location.X - (GFXLevelWidth(.Type) - 32) / 2, vScreenY(Z) + .Location.Y - GFXLevelHeight(.Type) + 32, GFXLevelWidth(.Type), GFXLevelHeight(.Type), GFXLevelMask(.Type), 0, 32 * LevelFrame(.Type), vbSrcAnd
//                        BitBlt myBackBuffer, vScreenX(Z) + .Location.X - (GFXLevelWidth(.Type) - 32) / 2, vScreenY(Z) + .Location.Y - GFXLevelHeight(.Type) + 32, GFXLevelWidth(.Type), GFXLevelHeight(.Type), GFXLevel(.Type), 0, 32 * LevelFrame(.Type), vbSrcPaint
//                    Else
//                        BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXLevelMask(.Type), 0, 32 * LevelFrame(.Type), vbSrcAnd
//                        BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXLevel(.Type), 0, 32 * LevelFrame(.Type), vbSrcPaint
//                    End If
//                End If
//            End With
//        Next A
//    Else
    {
        double sLeft = -vScreenX[1] + 64;
        double sTop = -vScreenY[1] + 96;
        double sRight = -vScreenX[1] + vScreen[1].Width - 64;
        double sBottom = -vScreenY[1] + vScreen[1].Height - 64;

        treeWorldTileQuery(sLeft, sTop, sRight, sBottom, tarr, true);
        //for(A = 1; A <= numTiles; A++)
        for(auto *t : tarr)
        {
            Tile_t &tile = *t;
            SDL_assert(IF_INRANGE(tile.Type, 1, maxTileType));

            g_stats.checkedTiles++;
            if(vScreenCollision2(1, tile.Location))
            {
                g_stats.renderedTiles++;
//                frmMain.renderTexture(vScreenX[Z] + Tile[A].Location.X, vScreenY[Z] + Tile[A].Location.Y, Tile[A].Location.Width, Tile[A].Location.Height, GFXTile[Tile[A].Type], 0, TileHeight[Tile[A].Type] * TileFrame[Tile[A].Type]);
                frmMain.renderTexture(vScreenX[Z] + tile.Location.X,
                                      vScreenY[Z] + tile.Location.Y,
                                      tile.Location.Width,
                                      tile.Location.Height,
                                      GFXTileBMP[tile.Type], 0, TileHeight[tile.Type] * TileFrame[tile.Type]);
            }
        }

        treeWorldSceneQuery(sLeft, sTop, sRight, sBottom, sarr, true);
        //for(A = 1; A <= numScenes; A++)
        for(auto *t : sarr)
        {
            Scene_t &scene = *t;
            SDL_assert(IF_INRANGE(scene.Type, 1, maxSceneType));

            g_stats.checkedScenes++;
            if(vScreenCollision2(1, scene.Location) && scene.Active)
            {
                g_stats.renderedScenes++;
//                frmMain.renderTexture(vScreenX[Z] + scene.Location.X, vScreenY[Z] + scene.Location.Y, scene.Location.Width, scene.Location.Height, GFXSceneMask[scene.Type], 0, SceneHeight[scene.Type] * SceneFrame[scene.Type]);
//                frmMain.renderTexture(vScreenX[Z] + scene.Location.X, vScreenY[Z] + scene.Location.Y, scene.Location.Width, scene.Location.Height, GFXScene[scene.Type], 0, SceneHeight[scene.Type] * SceneFrame[scene.Type]);
                frmMain.renderTexture(vScreenX[Z] + scene.Location.X,
                                      vScreenY[Z] + scene.Location.Y,
                                      scene.Location.Width, scene.Location.Height,
                                      GFXSceneBMP[scene.Type], 0, SceneHeight[scene.Type] * SceneFrame[scene.Type]);
            }
        }

        treeWorldPathQuery(sLeft, sTop, sRight, sBottom, parr, true);
        //for(A = 1; A <= numWorldPaths; A++)
        for(auto *t : parr)
        {
            WorldPath_t &path = *t;
            SDL_assert(IF_INRANGE(path.Type, 1, maxPathType));

            g_stats.checkedPaths++;
            if(vScreenCollision2(1, path.Location) && path.Active)
            {
                g_stats.renderedPaths++;
//                frmMain.renderTexture(vScreenX[Z] + path.Location.X, vScreenY[Z] + path.Location.Y, path.Location.Width, path.Location.Height, GFXPathMask[path.Type], 0, 0);
//                frmMain.renderTexture(vScreenX[Z] + path.Location.X, vScreenY[Z] + path.Location.Y, path.Location.Width, path.Location.Height, GFXPath[path.Type], 0, 0);
                frmMain.renderTexture(vScreenX[Z] + path.Location.X,
                                      vScreenY[Z] + path.Location.Y,
                                      path.Location.Width, path.Location.Height,
                                      GFXPathBMP[path.Type], 0, 0);
            }
        }

        treeWorldLevelQuery(sLeft, sTop, sRight, sBottom, larr, true);
        //for(A = 1; A <= numWorldLevels; A++)
        for(auto *t : larr)
        {
            WorldLevel_t &level = *t;
            SDL_assert(IF_INRANGE(level.Type, 0, maxLevelType));

            g_stats.checkedLevels++;
            if(vScreenCollision2(1, level.Location) && level.Active)
            {
                g_stats.renderedLevels++;
                if(level.Path)
                {
                    frmMain.renderTexture(vScreenX[Z] + level.Location.X,
                                          vScreenY[Z] + level.Location.Y,
                                          level.Location.Width,
                                          level.Location.Height,
                                          GFXLevelBMP[0], 0, 0);
                }
                if(level.Path2)
                {
                    frmMain.renderTexture(vScreenX[Z] + level.Location.X - 16,
                                          vScreenY[Z] + 8 + level.Location.Y,
                                          64, 32,
                                          GFXLevelBMP[29], 0, 0);
                }
                if(GFXLevelBig[level.Type])
                {
                    frmMain.renderTexture(vScreenX[Z] + level.Location.X - (GFXLevelWidth[level.Type] - 32) / 2.0,
                                          vScreenY[Z] + level.Location.Y - GFXLevelHeight[level.Type] + 32,
                                          GFXLevelWidth[level.Type], GFXLevelHeight[level.Type],
                                          GFXLevelBMP[level.Type], 0, 32 * LevelFrame[level.Type]);
                }
                else
                {
                    frmMain.renderTexture(vScreenX[Z] + level.Location.X,
                                          vScreenY[Z] + level.Location.Y,
                                          level.Location.Width, level.Location.Height,
                                          GFXLevelBMP[level.Type], 0, 32 * LevelFrame[level.Type]);
                }
            }
        }
    }

//    If WorldEditor = True Then
//        For A = 1 To numEffects
//            With Effect(A)
//                If vScreenCollision(Z, .Location) Then
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXEffectMask(.Type), 0, .Frame * EffectHeight(.Type), vbSrcAnd
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXEffect(.Type), 0, .Frame * EffectHeight(.Type), vbSrcPaint
//                End If
//            End With
//        Next A
//        For A = 1 To numWorldMusic
//            With WorldMusic(A).Location
//                BitBlt myBackBuffer, vScreenX(Z) + .X, vScreenY(Z) + .Y, .Width, .Height, GFX.WarpMask(1).hdc, 0, 0, vbSrcAnd
//                BitBlt myBackBuffer, vScreenX(Z) + .X, vScreenY(Z) + .Y, .Width, .Height, GFX.Warp(1).hdc, 0, 0, vbSrcPaint
//                SuperPrint Str(WorldMusic(A).Type), 1, Int(.X + 2 + vScreenX(Z)), Int(.Y + 2 + vScreenY(Z))
//            End With
//        Next A
//        If EditorCursor.Mode = 7 Then
//            With EditorCursor.Tile
//                BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXTile(.Type), 0, TileHeight(.Type) * TileFrame(.Type), vbSrcCopy
//            End With
//        End If
//        If EditorCursor.Mode = 8 Then
//            With EditorCursor.Scene
//                BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXSceneMask(.Type), 0, SceneHeight(.Type) * SceneFrame(.Type), vbSrcAnd
//                BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXScene(.Type), 0, SceneHeight(.Type) * SceneFrame(.Type), vbSrcPaint
//            End With
//        End If
//        If EditorCursor.Mode = 9 Then
//            With EditorCursor.WorldLevel
//                If .Path = True Then
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXLevelMask(0), 0, 0, vbSrcAnd
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXLevel(0), 0, 0, vbSrcPaint
//                End If
//                If .Path2 = True Then
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X - 16, vScreenY(Z) + 8 + .Location.Y, 64, 32, GFXLevelMask(29), 0, 0, vbSrcAnd
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X - 16, vScreenY(Z) + 8 + .Location.Y, 64, 32, GFXLevel(29), 0, 0, vbSrcPaint
//                End If
//                If GFXLevelBig(.Type) = True Then
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X - (GFXLevelWidth(.Type) - 32) / 2, vScreenY(Z) + .Location.Y - GFXLevelHeight(.Type) + 32, GFXLevelWidth(.Type), GFXLevelHeight(.Type), GFXLevelMask(.Type), 0, 32 * LevelFrame(.Type), vbSrcAnd
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X - (GFXLevelWidth(.Type) - 32) / 2, vScreenY(Z) + .Location.Y - GFXLevelHeight(.Type) + 32, GFXLevelWidth(.Type), GFXLevelHeight(.Type), GFXLevel(.Type), 0, 32 * LevelFrame(.Type), vbSrcPaint
//                Else
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXLevelMask(.Type), 0, 32 * LevelFrame(.Type), vbSrcAnd
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXLevel(.Type), 0, 32 * LevelFrame(.Type), vbSrcPaint
//                End If
//            End With
//        End If
//        If EditorCursor.Mode = 10 Then
//            With EditorCursor.WorldPath
//                BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXPathMask(.Type), 0, 0, vbSrcAnd
//                BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFXPath(.Type), 0, 0, vbSrcPaint
//            End With
//        End If
//        With EditorCursor
//                If .Mode = 6 Then
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X - 2, vScreenY(Z) + .Location.Y, 22, 30, GFX.ECursorMask(3).hdc, 0, 0, vbSrcAnd
//                    BitBlt myBackBuffer, vScreenX(Z) + .Location.X - 2, vScreenY(Z) + .Location.Y, 22, 30, GFX.ECursor(3).hdc, 0, 0, vbSrcPaint
//                Else
//                    If .Mode = 11 Then

//                BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFX.WarpMask(1).hdc, 0, 0, vbSrcAnd
//                BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, .Location.Height, GFX.Warp(1).hdc, 0, 0, vbSrcPaint
//                SuperPrint Str(.WorldMusic.Type), 1, Int(.Location.X + 2 + vScreenX(Z)), Int(.Location.Y + 2 + vScreenY(Z))

//                        'BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, .Location.Width, 2, GFX.Split(1).hdc, 0, 0, vbSrcCopy
//                        'BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y + .Location.Height - 2, .Location.Width, 2, GFX.Split(1).hdc, 0, 0, vbSrcCopy
//                        'BitBlt myBackBuffer, vScreenX(Z) + .Location.X, vScreenY(Z) + .Location.Y, 2, .Location.Height, GFX.Split(1).hdc, 0, 0, vbSrcCopy
//                        'BitBlt myBackBuffer, vScreenX(Z) + .Location.X + .Location.Width - 2, vScreenY(Z) + .Location.Y, 2, .Location.Height, GFX.Split(1).hdc, 0, 0, vbSrcCopy
//                    End If
//                    BitBlt myBackBuffer, .X, .Y + 8, 32, 32, GFX.ECursorMask(2).hdc, 0, 0, vbSrcAnd
//                    BitBlt myBackBuffer, .X, .Y + 8, 32, 32, GFX.ECursor(2).hdc, 0, 0, vbSrcPaint
//                End If
//        End With
//        'BitBlt frmLevelWindow.vScreen(Z).hdc, 0, 0, frmLevelWindow.vScreen(Z).ScaleWidth, frmLevelWindow.vScreen(Z).ScaleHeight, myBackBuffer, 0, 0, vbSrcCopy
//        StretchBlt frmLevelWindow.vScreen(Z).hdc, 0, 0, frmLevelWindow.vScreen(Z).ScaleWidth, frmLevelWindow.vScreen(Z).ScaleHeight, myBackBuffer, 0, 0, 800, 600, vbSrcCopy
//    Else
    { // NOT AN EDITOR!!!
        if(WorldPlayer[1].Type == 0)
            WorldPlayer[1].Type = 1;
        if(Player[1].Character == 1)
            WorldPlayer[1].Type = 1;
        if(Player[1].Character == 2)
            WorldPlayer[1].Type = 2;
        if(Player[1].Character == 3)
            WorldPlayer[1].Type = 3;
        if(Player[1].Character == 4)
            WorldPlayer[1].Type = 4;
        if(Player[1].Character == 5)
            WorldPlayer[1].Type = 5;
        if(WorldPlayer[1].Type == 3)
            WPHeight = 44;
        else if(WorldPlayer[1].Type == 4)
            WPHeight = 40;
        else
            WPHeight = 32;
//        frmMain.renderTexture(vScreenX[Z] + WorldPlayer[1].Location.X, vScreenY[Z] + WorldPlayer[1].Location.Y - 10 + WorldPlayer[1].Location.Height - WPHeight, WorldPlayer[1].Location.Width, WPHeight, GFXPlayerMask[WorldPlayer[1].Type], 0, WPHeight * WorldPlayer[1].Frame);
//        frmMain.renderTexture(vScreenX[Z] + WorldPlayer[1].Location.X, vScreenY[Z] + WorldPlayer[1].Location.Y - 10 + WorldPlayer[1].Location.Height - WPHeight, WorldPlayer[1].Location.Width, WPHeight, GFXPlayer[WorldPlayer[1].Type], 0, WPHeight * WorldPlayer[1].Frame);
        frmMain.renderTexture(vScreenX[Z] + WorldPlayer[1].Location.X,
                              vScreenY[Z] + WorldPlayer[1].Location.Y - 10 + WorldPlayer[1].Location.Height - WPHeight,
                              WorldPlayer[1].Location.Width, WPHeight,
                              GFXPlayerBMP[WorldPlayer[1].Type], 0, WPHeight * WorldPlayer[1].Frame);

//        frmMain.renderTexture(0, 0, 800, 130, GFX.Interface[4], 0, 0);

        frmMain.renderTexture(0, 0, 800, 130, GFX.Interface[4], 0, 0);
        frmMain.renderTexture(0, 534, 800, 66, GFX.Interface[4], 0, 534);
        frmMain.renderTexture(0, 130, 66, 404, GFX.Interface[4], 0, 130);
        frmMain.renderTexture(734, 130, 66, 404, GFX.Interface[4], 734, 130);

        for(A = 1; A <= numPlayers; A++)
        {
            Player[A].Direction = -1;
            Player[A].Location.SpeedY = 0;
            Player[A].Location.SpeedX = -1;
            Player[A].Controls.Left = false;
            Player[A].Controls.Right = false;
            if(Player[A].Duck)
                UnDuck(A);
            PlayerFrame(A);

            if(Player[A].Mount == 3)
            {
                if(Player[A].MountType == 0)
                    Player[A].MountType = 1;
                B = Player[A].MountType;
                // Yoshi's Body
                frmMain.renderTexture(32 + (48 * A) + Player[A].YoshiBX, 124 - Player[A].Location.Height + Player[A].YoshiBY,
                                      32, 32, GFXYoshiBBMP[B], 0, 32 * Player[A].YoshiBFrame, c, c, c);

                // Yoshi's Head
                frmMain.renderTexture(32 + (48 * A) + Player[A].YoshiTX,
                                      124 - Player[A].Location.Height + Player[A].YoshiTY,
                                      32, 32, GFXYoshiTBMP[B], 0, 32 * Player[A].YoshiTFrame, c, c, c);
            }

            if(Player[A].Character == 1)
            {
                if(Player[A].Mount == 0 || Player[A].Mount == 3)
                {
                    frmMain.renderTexture(32 + (48 * A) + MarioFrameX[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)],
                                          124 - Player[A].Location.Height + MarioFrameY[(Player[A].State * 100) +
                                            (Player[A].Frame * Player[A].Direction)] + Player[A].MountOffsetY,
                                          99, 99, GFXMarioBMP[Player[A].State], pfrX(100 + Player[A].Frame * Player[A].Direction),
                                          pfrY(100 + Player[A].Frame * Player[A].Direction), c, c, c);
                }
                else if(Player[A].Mount == 1)
                {
                    int height;
                    if(Player[A].State == 1)
                        height = Player[A].Location.Height - MarioFrameY[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)];
                    else
                        height = Player[A].Location.Height - MarioFrameY[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)] - 30;
                    frmMain.renderTexture(32 + (48 * A) + MarioFrameX[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)],
                                          124 - Player[A].Location.Height + MarioFrameY[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)],
                                          99,
                                          height,
                                          GFXMarioBMP[Player[A].State],
                                          pfrX(100 + Player[A].Frame * Player[A].Direction),
                                          pfrY(100 + Player[A].Frame * Player[A].Direction), c, c, c);

                    frmMain.renderTexture(32 + (48 * A) + Player[A].Location.Width / 2.0 - 16, 124 - 30, 32, 32,
                                          GFX.Boot[Player[A].MountType], 0, 32 * Player[A].MountFrame, c, c, c);

                    if(Player[A].MountType == 3)
                    {
                        Player[A].YoshiWingsFrameCount = Player[A].YoshiWingsFrameCount + 1;
                        Player[A].YoshiWingsFrame = 0;
                        if(Player[A].YoshiWingsFrameCount <= 12)
                            Player[A].YoshiWingsFrame = 1;
                        else if(Player[A].YoshiWingsFrameCount >= 24)
                            Player[A].YoshiWingsFrameCount = 0;

                        frmMain.renderTexture(32 + (48 * A) + Player[A].Location.Width / 2.0 - 16 + 20, 124 - 30 - 10, 32, 32,
                                              GFX.YoshiWings, 0, 0 + 32 * Player[A].YoshiWingsFrame);
                    }
                }
            }
            else if(Player[A].Character == 2)
            {
                if(Player[A].Mount == 0 || Player[A].Mount == 3)
                {
                    frmMain.renderTexture(32 + (48 * A) + LuigiFrameX[(Player[A].State * 100) +
                                            (Player[A].Frame * Player[A].Direction)],
                                          124 - Player[A].Location.Height + LuigiFrameY[(Player[A].State * 100) +
                                            (Player[A].Frame * Player[A].Direction)] + Player[A].MountOffsetY, 99, 99,
                                          GFXLuigiBMP[Player[A].State], pfrX(100 + Player[A].Frame * Player[A].Direction),
                                          pfrY(100 + Player[A].Frame * Player[A].Direction), c, c, c);
                }
                else if(Player[A].Mount == 1)
                {
                    int height;
                    if(Player[A].State == 1)
                        height = Player[A].Location.Height - LuigiFrameY[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)];
                    else
                        height = Player[A].Location.Height - LuigiFrameY[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)] - 30;
                    frmMain.renderTexture(32 + (48 * A) + LuigiFrameX[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)],
                                          124 - Player[A].Location.Height + LuigiFrameY[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)],
                                          99,
                                          height,
                                          GFXLuigiBMP[Player[A].State],
                                          pfrX(100 + Player[A].Frame * Player[A].Direction),
                                          pfrY(100 + Player[A].Frame * Player[A].Direction), c, c, c);

                    frmMain.renderTexture(32 + (48 * A) + Player[A].Location.Width / 2.0 - 16, 124 - 30, 32, 32, GFX.Boot[Player[A].MountType], 0, 32 * Player[A].MountFrame, c, c, c);

                    if(Player[A].MountType == 3)
                    {
                        Player[A].YoshiWingsFrameCount = Player[A].YoshiWingsFrameCount + 1;
                        Player[A].YoshiWingsFrame = 0;
                        if(Player[A].YoshiWingsFrameCount <= 12)
                            Player[A].YoshiWingsFrame = 1;
                        else if(Player[A].YoshiWingsFrameCount >= 24)
                            Player[A].YoshiWingsFrameCount = 0;

                        frmMain.renderTexture(32 + (48 * A) + Player[A].Location.Width / 2.0 - 16 + 20, 124 - 30 - 10, 32, 32, GFX.YoshiWings, 0, 0 + 32 * Player[A].YoshiWingsFrame, c, c, c);
                    }
                }
            }
            else if(Player[A].Character == 3)
            {
                if(Player[A].Mount == 0 || Player[A].Mount == 3)
                {
                    frmMain.renderTexture(32 + (48 * A) + PeachFrameX[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)],
                                          124 - Player[A].Location.Height + PeachFrameY[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)] + Player[A].MountOffsetY,
                                          99, 99,
                                          GFXPeachBMP[Player[A].State],
                                          pfrX(100 + Player[A].Frame * Player[A].Direction),
                                          pfrY(100 + Player[A].Frame * Player[A].Direction), c, c, c);
                }
                else if(Player[A].Mount == 1)
                {
                    int height;
                    if(Player[A].State == 1)
                        height = Player[A].Location.Height - PeachFrameY[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)];
                    else
                        height = Player[A].Location.Height - PeachFrameY[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)] - 30;
                    frmMain.renderTexture(32 + (48 * A) + PeachFrameX[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)],
                                          124 - Player[A].Location.Height + PeachFrameY[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)],
                                          99,
                                          height,
                                          GFXPeachBMP[Player[A].State],
                                          pfrX(100 + Player[A].Frame * Player[A].Direction),
                                          pfrY(100 + Player[A].Frame * Player[A].Direction), c, c, c);

                    frmMain.renderTexture(32 + (48 * A) + Player[A].Location.Width / 2.0 - 16, 124 - 30, 32, 32, GFX.Boot[Player[A].MountType], 0, 32 * Player[A].MountFrame, c, c, c);

                    if(Player[A].MountType == 3)
                    {
                        Player[A].YoshiWingsFrameCount = Player[A].YoshiWingsFrameCount + 1;
                        Player[A].YoshiWingsFrame = 0;
                        if(Player[A].YoshiWingsFrameCount <= 12)
                            Player[A].YoshiWingsFrame = 1;
                        else if(Player[A].YoshiWingsFrameCount >= 24)
                            Player[A].YoshiWingsFrameCount = 0;

                        frmMain.renderTexture(32 + (48 * A) + Player[A].Location.Width / 2.0 - 16 + 20, 124 - 30 - 10, 32, 32, GFX.YoshiWings, 0, 0 + 32 * Player[A].YoshiWingsFrame, c, c, c);
                    }
                }
            }
            else if(Player[A].Character == 4)
            {
                if(Player[A].Mount == 0 || Player[A].Mount == 3)
                {
                    frmMain.renderTexture(32 + (48 * A) + ToadFrameX[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)], 124 - Player[A].Location.Height + ToadFrameY[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)] + Player[A].MountOffsetY, 99, 99, GFXToadBMP[Player[A].State], pfrX(100 + Player[A].Frame * Player[A].Direction), pfrY(100 + Player[A].Frame * Player[A].Direction), c, c, c);
                }
                else if(Player[A].Mount == 1)
                {
                    if(Player[A].State == 1)
                    {
                        frmMain.renderTexture(32 + (48 * A) + ToadFrameX[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)], 6 + 124 - Player[A].Location.Height + ToadFrameY[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)], 99, Player[A].Location.Height - 24, GFXToadBMP[Player[A].State], pfrX(100 + Player[A].Frame * Player[A].Direction), pfrY(100 + Player[A].Frame * Player[A].Direction), c, c, c);
                    }
                    else
                    {
                        int height;
                        if(Player[A].State == 1)
                            height = Player[A].Location.Height - ToadFrameY[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)];
                        else
                            height = Player[A].Location.Height - ToadFrameY[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)] - 26;
                        frmMain.renderTexture(32 + (48 * A) + ToadFrameX[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)],
                                              124 - Player[A].Location.Height + ToadFrameY[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)],
                                              99,
                                              height,
                                              GFXToadBMP[Player[A].State], pfrX(100 + Player[A].Frame * Player[A].Direction), pfrY(100 + Player[A].Frame * Player[A].Direction), c, c, c);
                    }
                    frmMain.renderTexture(32 + (48 * A) + Player[A].Location.Width / 2.0 - 16, 124 - 30, 32, 32, GFX.Boot[Player[A].MountType], 0, 32 * Player[A].MountFrame, c, c, c);

                    if(Player[A].MountType == 3)
                    {
                        Player[A].YoshiWingsFrameCount = Player[A].YoshiWingsFrameCount + 1;
                        Player[A].YoshiWingsFrame = 0;
                        if(Player[A].YoshiWingsFrameCount <= 12)
                            Player[A].YoshiWingsFrame = 1;
                        else if(Player[A].YoshiWingsFrameCount >= 24)
                            Player[A].YoshiWingsFrameCount = 0;
                        frmMain.renderTexture(32 + (48 * A) + Player[A].Location.Width / 2.0 - 16 + 20, 124 - 30 - 10, 32, 32, GFX.YoshiWings, 0, 0 + 32 * Player[A].YoshiWingsFrame, c, c, c);
                    }
                }
            }
            else if(Player[A].Character == 5)
            {
                frmMain.renderTexture(32 + (48 * A) + LinkFrameX[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)], 124 - Player[A].Location.Height + LinkFrameY[(Player[A].State * 100) + (Player[A].Frame * Player[A].Direction)], 99, 99, GFXLinkBMP[Player[A].State], pfrX(100 + Player[A].Frame * Player[A].Direction), pfrY(100 + Player[A].Frame * Player[A].Direction), c, c, c);
            }
        }
        A = numPlayers + 1;

        // Print lives on the screen
        frmMain.renderTexture(32 + (48 * A), 126 - GFX.Interface[3].h, GFX.Interface[3].w, GFX.Interface[3].h, GFX.Interface[3], 0, 0);
        frmMain.renderTexture(32 + (48 * A) + 40, 128 - GFX.Interface[3].h, GFX.Interface[1].w, GFX.Interface[1].h, GFX.Interface[1], 0, 0);
        SuperPrint(std::to_string(int(Lives)), 1, 32 + (48 * A) + 62, 112);

        // Print coins on the screen
        if(Player[1].Character == 5)
            frmMain.renderTexture(32 + (48 * A) + 16, 88, GFX.Interface[2].w, GFX.Interface[2].h, GFX.Interface[6], 0, 0);
        else
            frmMain.renderTexture(32 + (48 * A) + 16, 88, GFX.Interface[2].w, GFX.Interface[2].h, GFX.Interface[2], 0, 0);
        frmMain.renderTexture(32 + (48 * A) + 40, 90, GFX.Interface[1].w, GFX.Interface[1].h, GFX.Interface[1], 0, 0);

        SuperPrint(std::to_string(Coins), 1, 32 + (48 * A) + 62, 90);

        // Print stars on the screen
        if(numStars > 0)
        {
            frmMain.renderTexture(32 + (48 * A) + 16, 66, GFX.Interface[5].w, GFX.Interface[5].h, GFX.Interface[5], 0, 0);
            frmMain.renderTexture(32 + (48 * A) + 40, 68, GFX.Interface[1].w, GFX.Interface[1].h, GFX.Interface[1], 0, 0);
            SuperPrint(std::to_string(numStars), 1, 32 + (48 * A) + 62, 68);
        }

        // Print the level's name
        if(!WorldPlayer[1].LevelName.empty())
        {
            int c = WorldPlayer[1].levelStarsCur;
            int m = WorldPlayer[1].levelStarsMax;
            int lnl = SuperTextPixLen(WorldPlayer[1].LevelName, 2);
            int lnlx = 32 + (48 * A) + 116;
            SuperPrint(WorldPlayer[1].LevelName, 2, lnlx, 109);

            if(m > 0 && g_compatibility.enable_world_map_level_stars)
            {
                std::string label = fmt::format_ne("{0}/{1}", c, m);
                int len = SuperTextPixLen(label, 3);
                int totalLen = len + GFX.Interface[1].w + GFX.Interface[5].w + 8 + 4;
                int x = 734;
                int y = (lnl + lnlx >= x - totalLen ? 90 : 109); // Print stars count above the title if it gets too long
                frmMain.renderTexture(x - len - (GFX.Interface[1].w + 4), y,
                                      GFX.Interface[1].w, GFX.Interface[1].h, GFX.Interface[1], 0, 0);
                frmMain.renderTexture(x - len - (GFX.Interface[1].w + GFX.Interface[5].w + 8), y,
                                      GFX.Interface[5].w, GFX.Interface[5].h, GFX.Interface[5], 0, 0);
                SuperPrintRightAlign(label, 3, x, y);
            }
        }

        if(GamePaused)
        {
            frmMain.renderRect(210, 200, 380, 200, 0.f, 0.f, 0.f);
            if(!Cheater)
            {
                SuperPrint("CONTINUE", 3, 272, 257);
                SuperPrint("SAVE & CONTINUE", 3, 272, 292);
                SuperPrint("SAVE & QUIT", 3, 272, 327);
                frmMain.renderTexture(252, 257 + (MenuCursor * 35), 16, 16, GFX.MCursor[0], 0, 0);
            }
            else
            {
                SuperPrint("CONTINUE", 3, 272 + 56, 275);
                SuperPrint("QUIT", 3, 272 + 56, 310);
                frmMain.renderTexture(252 + 56, 275 + (MenuCursor * 35), 16, 16, GFX.MCursor[0], 0, 0);
            }
        }

        if(PrintFPS > 0)
            SuperPrint(std::to_string(int(PrintFPS)), 1, 8, 8, 0.f, 1.f, 0.f);

        g_stats.print();

        speedRun_renderControls(1, -1);
        speedRun_renderTimer();

        if(!skipRepaint)
            frmMain.repaint();

        frmMain.setTargetScreen();
    }

    if(TakeScreen)
        ScreenShot();

    frameRenderEnd();

//    if(frmMain.lazyLoadedBytes() > 200000) // Reset timer while loading many pictures at the same time
//        resetFrameTimer();
    frmMain.lazyLoadedBytesReset();
}
