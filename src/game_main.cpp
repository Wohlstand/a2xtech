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

#include <SDL2/SDL_timer.h>

#include <Logger/logger.h>
#include <Utils/files.h>
#include <AppPath/app_path.h>
#include <InterProcess/intproc.h>
#include <pge_delay.h>
#include <fmt_format_ne.h>

#ifdef ENABLE_XTECH_LUA
#include "xtech_lua_main.h"
#endif

#include "globals.h"
#include "game_main.h"

#include "frame_timer.h"
#include "compat.h"
#include "blocks.h"
#include "change_res.h"
#include "collision.h"
#include "effect.h"
#include "graphics.h"
#include "control/joystick.h"
#include "layers.h"
#include "load_gfx.h"
#include "player.h"
#include "sound.h"
#include "video.h"
#include "editor.h"
#include "custom.h"
#include "main/level_file.h"
#include "main/speedrunner.h"
#include "main/menu_main.h"
#include "main/game_info.h"

#include "pseudo_vb.h"

void CheckActive();
// set up sizable blocks
void SizableBlocks();

// game_main_setupphysics.cpp

static int loadingThread(void *waiter_ptr)
{
#ifndef PGE_NO_THREADING
    SDL_atomic_t *waiter = (SDL_atomic_t *)waiter_ptr;
#else
    UNUSED(waiter_ptr);
#endif
    SetupPhysics(); // Setup Physics
    SetupGraphics(); // setup graphics
//    Load GFX 'load the graphics form
//    GFX.load(); // load the graphics form // Moved to before sound load
    SizableBlocks();
    LoadGFX(); // load the graphics from file
    SetupVars(); //Setup Variables

    InitSound(); // Setup sound effects

#ifndef PGE_NO_THREADING
    if(waiter)
        SDL_AtomicSet(waiter, 0);
#endif

    return 0;
}

int GameMain(const CmdLineSetup_t &setup)
{
    Player_t blankPlayer;
//    int A = 0;
//    int B = 0;
//    int C = 0;
    bool tempBool = false;
    int lastWarpEntered = 0;

    LB = "\n";
    EoT = "";

    FrameSkip = setup.frameSkip;
    noSound = setup.noSound;
    neverPause = setup.neverPause;

    CompatSetEnforcedLevel(setup.compatibilityLevel);

    g_speedRunnerMode = setup.speedRunnerMode;
    g_drawController |= setup.showControllerState;
    speedRun_setSemitransparentRender(setup.speedRunnerSemiTransparent);

    ResetCompat();

    // [ !Here was a starting dialog! ]

    //    frmLoader.Show 'show the Splash screen
    //    Do
    //        DoEvents
    //    Loop While StartMenu = False 'wait until the player clicks a button

    initMainMenu();
    StartMenu = true;

    initAll();

//    Unload frmLoader
    gfxLoaderTestMode = setup.testLevelMode;

    if(!GFX.load()) // Load UI graphics
        return 1;

//    If LevelEditor = False Then
//        frmMain.Show // Show window a bit later
//    frmMain.show();
//        GameMenu = True
    GameMenu = true;
//    Else
//        frmSplash.Show
//        BlocksSorted = True
//    End If

    LoadingInProcess = true;

    ShowFPS = setup.testShowFPS;
    MaxFPS = setup.testMaxFPS; // || (g_videoSettings.renderModeObtained == RENDER_ACCELERATED_VSYNC);

    InitControls(); // init player's controls
    DoEvents();

#ifdef __EMSCRIPTEN__ // Workaround for a recent Chrome's policy to avoid sudden sound without user's interaction
    frmMain.show(); // Don't show window until playing an initial sound

    while(!MenuMouseDown)
    {
        frmMain.setTargetTexture();
        frmMain.clearBuffer();
        SuperPrint("Click to start a game", 3, 230, 280);
        frmMain.repaint();
        frmMain.setTargetScreen();
        DoEvents();
        PGE_Delay(10);
    }
#endif

    if(!noSound)
        InitMixerX();

#ifndef PGE_NO_THREADING
    gfxLoaderThreadingMode = true;
#endif
    frmMain.show(); // Don't show window until playing an initial sound

    if(!noSound)
    {
        if(!setup.testLevelMode)
            PlayInitSound();
    }

#ifndef PGE_NO_THREADING
    {
        SDL_Thread*     loadThread;
        int             threadReturnValue;
        SDL_atomic_t    loadWaiter;
        int             loadWaiterState = 1;

        SDL_AtomicSet(&loadWaiter, loadWaiterState);
        loadThread = SDL_CreateThread(loadingThread, "Loader", &loadWaiter);

        if(!loadThread)
        {
            gfxLoaderThreadingMode = false;
            pLogCritical("Failed to create the loading thread! Do running the load directly");
            loadingThread(NULL);
        }
        else
        {
            do
            {
                UpdateLoadREAL();
                PGE_Delay(15);
                loadWaiterState = SDL_AtomicGet(&loadWaiter);
            } while(loadWaiterState);

            SDL_WaitThread(loadThread, &threadReturnValue);
            pLogDebug("Loading thread was exited with %d code.", threadReturnValue);
        }
    }
#else
    loadingThread(NULL);
#endif

    LevelSelect = true; // world map is to be shown

    if(setup.interprocess)
        IntProc::init();

    LoadingInProcess = false;

    // Clear the screen
    frmMain.setTargetTexture();
    frmMain.clearBuffer();
    frmMain.repaint();
    DoEvents();

    if(!neverPause && !frmMain.isWindowActive())
        SoundPauseEngine(1);

    if(!setup.testLevel.empty() || setup.interprocess) // Start level testing immediately!
    {
        GameMenu = false;
        LevelSelect = false;
        FullFileName = setup.testLevel;
        if(setup.testBattleMode)
        {
            numPlayers = 2;
            BattleMode = true;
            BattleIntro = 150;
        }
        else
        {
            numPlayers = setup.testNumPlayers;
            BattleMode = false;
            BattleIntro = 0;
        }
        GodMode = setup.testGodMode;
        GrabAll = setup.testGrabAll;
        zTestLevel(setup.testMagicHand, setup.interprocess);
    }

    do
    {
        if(GameMenu || MagicHand || LevelEditor)
        {
            frmMain.MousePointer = 99;
            showCursor(0);
        }
        else if(!resChanged)
        {
            frmMain.MousePointer = 0;
            showCursor(1);
        }

//        If LevelEditor = True Then 'Load the level editor
//            [USELESS!]

        // TheXTech Credits
        if(GameOutro)
        {
            ShadowMode = false;
            GodMode = false;
            GrabAll= false;
            CaptainN = false;
            FlameThrower = false;
            FreezeNPCs = false;
            WalkAnywhere = false;
            MultiHop = false;
            SuperSpeed = false;
            FlyForever = false;
            GoToLevelNoGameThing = false;

            for(int A = 1; A <= maxPlayers; A++)
            {
                Player[A] = blankPlayer;
            }

            numPlayers = 5;
            GameMenu = false;
            StopMusic();

            auto outroPath = AppPath + "outro.lvlx";
            if(!Files::fileExists(outroPath))
                outroPath = AppPath + "outro.lvl";
            OpenLevel(outroPath);

            ScreenType = 7;
            SetupScreens();
            ClearBuffer = true;

            for(int A = 1; A <= numPlayers; ++A)
            {
                Player_t &p = Player[A];

                if(A == 1)
                    p.State = 4;
                else if(A == 2)
                    p.State = 7;
                else if(A == 3)
                    p.State = 5;
                else if(A == 4)
                    p.State = 3;
                else
                    p.State = 6;

                if(A == 4)
                {
                    p.Mount = 1;
                    p.MountType = int(iRand() % 3) + 1;
                }

                p.Character = A;
                if(A == 2)
                {
                    p.Mount = 3;
                    p.MountType = int(iRand() % 8) + 1;
                }

                p.HeldBonus = 0;
                p.Section = 0;
                p.Location.Height = Physics.PlayerHeight[p.Character][p.State];
                p.Location.Width = Physics.PlayerWidth[p.Character][p.State];
            }

            SetupPlayers();
            CreditChop = 300; // 100
            EndCredits = 0;
            GameOutroDoQuit = false;
            SetupCredits();

            resetFrameTimer();

            // Update graphics before loop begin (to process an initial lazy-unpacking of used sprites)
            UpdateGraphics(true);
            resetFrameTimer();

            // Run the frame-loop
            runFrameLoop(&OutroLoop,
                         nullptr,
                        []()->bool{ return GameOutro;}, nullptr,
                        nullptr,
                        []()->void
                        {
                            ScreenType = 0;
                            SetupScreens();
                        });
        }

        // The Game Menu
        else if(GameMenu)
        {
            BattleIntro = 0;
            BattleOutro = 0;
            AllCharBlock = 0;
            Cheater = false;

            // in a main menu, reset this into initial state
            GoToLevelNoGameThing = false;

            for(int A = 1; A <= maxPlayers; ++A)
            {
                OwedMount[A] = 0;
                OwedMountType[A] = 0;
            }

            MenuMouseMove = false;
            MenuMouseRelease = false;
            MenuMouseClick = false;
            MenuMouseBack = false;
            BattleMode = false;

            if(MenuMode != MENU_BATTLE_MODE)
            {
                PlayerCharacter = 0;
                PlayerCharacter2 = 0;
            }

            pLogDebug("Clear check-points at Game Menu start");
            Checkpoint.clear();
            CheckpointsList.clear();
            WorldPlayer[1].Frame = 0;
            CheatString = "";
            LevelBeatCode = 0;
            curWorldLevel = 0;

            ClearWorld();

            ReturnWarp = 0;
            ReturnWarpSaved = 0;
            ShadowMode = false;
            GodMode = false;
            GrabAll = false;
            CaptainN = false;
            FlameThrower = false;
            FreezeNPCs = false;
            WalkAnywhere = false;
            MultiHop = false;
            SuperSpeed = false;
            FlyForever = false;
            BeatTheGame = false;
            ScreenType = 2;

            SetupScreens();

            BattleOutro = 0;
            BattleIntro = 0;

            for(int A = 1; A <= maxPlayers; ++A)
            {
                Player[A] = blankPlayer;
            }

            numPlayers = g_gameInfo.introMaxPlayersCount;
            if(!g_gameInfo.introEnableActivity || g_gameInfo.introMaxPlayersCount < 1)
                numPlayers = 1;// one deadman should be

            auto introPath = AppPath + "intro.lvlx";
            if(!Files::fileExists(introPath))
                introPath = AppPath + "intro.lvl";
            OpenLevel(introPath);
            vScreenX[1] = -level[0].X;

            StartMusic(0);
            SetupPlayers();

            For(A, 1, numPlayers)
            {
                Player_t &p = Player[A];
                p.State = (iRand() % 6) + 2;
                p.Character = (iRand() % 5) + 1;

                if(A >= 1 && A <= 5)
                    p.Character = A;

                p.HeldBonus = 0;
                p.Section = 0;
                p.Location.Height = Physics.PlayerHeight[p.Character][p.State];
                p.Location.Width = Physics.PlayerWidth[p.Character][p.State];
                p.Location.X = level[p.Section].X + ((128 + dRand() * 64) * A);
                p.Location.Y = level[p.Section].Height - p.Location.Height - 65;

                do
                {
                    tempBool = true;
                    for(int B = 1; B <= numBlock; ++B)
                    {
                        if(CheckCollision(p.Location, Block[B].Location))
                        {
                            p.Location.Y = Block[B].Location.Y - p.Location.Height - 0.1;
                            tempBool = false;
                        }
                    }
                } while(!tempBool);
                p.Dead = true;
            }

            ProcEvent("Level - Start", true);
            For(A, 2, maxEvents)
            {
                if(Events[A].AutoStart)
                    ProcEvent(Events[A].Name, true);
            }

            resetFrameTimer();

            // Update graphics before loop begin (to process inital lazy-unpacking of used sprites)
            UpdateGraphics(true);
            resetFrameTimer();
            // Clear the speed-runner timer
            speedRun_resetTotal();

            // Main menu loop
            runFrameLoop(&MenuLoop, nullptr, []()->bool{ return GameMenu;});
            if(!GameIsActive)
            {
                speedRun_saveStats();
                return 0;// Break on quit
            }
        }

        // World Map
        else if(LevelSelect)
        {
            CheatString.clear();
            For(A, 1, numPlayers)
            {
                if(Player[A].Mount == 0 || Player[A].Mount == 2)
                {
                    if(OwedMount[A] > 0)
                    {
                        Player[A].Mount = OwedMount[A];
                        if(OwedMountType[A] > 0)
                            Player[A].MountType = OwedMountType[A];
                        else
                            Player[A].MountType = 1;
                    }
                }
                OwedMount[A] = 0;
                OwedMountType[A] = 0;
            }

            if(!NoMap)
            {
                // Restore the previously preserved world map paths
                FileNameFull = FileNameFullWorld;
                FileName = FileNameWorld;
            }

            LoadCustomCompat();
            FindCustomPlayers();
            LoadCustomGFX();
            LoadCustomSound();
            SetupPlayers();

            if(!NoMap)
                FindWldStars();

            if((!StartLevel.empty() && NoMap) || !GoToLevel.empty())
            {
                if(NoMap)
                    SaveGame();

                Player[1].Vine = 0;
                Player[2].Vine = 0;

                if(!GoToLevelNoGameThing)
                    PlaySound(SFX_LevelSelect);
                SoundPause[26] = 2000;

                LevelSelect = false;

                if(!GoToLevelNoGameThing)
                    GameThing();
                else
                {
                    frmMain.setTargetTexture();
                    frmMain.clearBuffer();
                    frmMain.repaint();
                }
                ClearLevel();
                PGE_Delay(1000);

                std::string levelPath;
                if(GoToLevel.empty())
                    levelPath = SelectWorld[selWorld].WorldPath + StartLevel;
                else
                {
                    levelPath = SelectWorld[selWorld].WorldPath + GoToLevel;
                    GoToLevel.clear();
                }

                if(!OpenLevel(levelPath))
                {
                    MessageText = fmt::format_ne("ERROR: Can't open \"{0}\": file doesn't exist or corrupted.", levelPath);
                    PauseGame(1);
                    ErrorQuit = true;
                }
            }
            else
            {
                if(curWorldMusic > 0)
                    StartMusic(curWorldMusic);

                resetFrameTimer();
                speedRun_resetCurrent();

                // On a world map, reset this into default state
                GoToLevelNoGameThing = false;

                // Update graphics before loop begin (to process inital lazy-unpacking of used sprites)
                UpdateGraphics2(true);
                resetFrameTimer();

                // 'level select loop
                runFrameLoop(nullptr, &WorldLoop,
                             []()->bool{return LevelSelect;},
                             nullptr,
                             []()->void{FreezeNPCs = false;});
                if(!GameIsActive)
                {
                    speedRun_saveStats();
                    return 0;// Break on quit
                }
            }
        }

        // MAIN GAME
        else
        {
            CheatString.clear();
            EndLevel = false;

            if(numPlayers == 1)
                ScreenType = 0; // Follow 1 player
            else if(numPlayers == 2)
                ScreenType = 5; // Dynamic screen
            else
            {
                // ScreenType = 3 'Average, no one leaves the screen
                ScreenType = 2; // Average
            }

            if(SingleCoop > 0)
                ScreenType = 6;
//            If nPlay.Online = True Then ScreenType = 8 'Online

            for(int A = 1; A <= numPlayers; ++A)
            {
                if(Player[A].Mount == 2)
                    Player[A].Mount = 0; // take players off the clown car
            }

            SetupPlayers(); // Setup Players for the level

            if(LevelRestartRequested && Checkpoint.empty())
                StartWarp = lastWarpEntered; // When restarting a level (after death), don't restore an entered warp on checkpoints

            qScreen = false;
            LevelRestartRequested = false;

// for warp entrances
            if((ReturnWarp > 0 && IsEpisodeIntro/*FileName == StartLevel*/) || (StartWarp > 0))
            {
                for(int numPlayersMax = numPlayers, A = 1; A <= numPlayersMax; ++A)
                {
                    Player_t &p = Player[A];

                    if(StartWarp > 0)
                        p.Warp = StartWarp;
                    else
                        p.Warp = ReturnWarp;

                    p.WarpBackward = false;
                    auto &warp = Warp[p.Warp];

                    if(warp.Effect == 1)
                    {
                        if(warp.Direction2 == 1) // DOWN
                        {
//                                .Location.X = Warp(.Warp).Exit.X + Warp(.Warp).Exit.Width / 2 - .Location.Width / 2
                            p.Location.X = warp.Exit.X + (warp.Exit.Width / 2) - (p.Location.Width / 2);
//                                .Location.Y = Warp(.Warp).Exit.Y - .Location.Height - 8
                            p.Location.Y = warp.Exit.Y - p.Location.Height - 8;
                        }
//                            ElseIf Warp(.Warp).Direction2 = 3 Then
                        if(warp.Direction2 == 3) // UP
                        {
//                                .Location.X = Warp(.Warp).Exit.X + Warp(.Warp).Exit.Width / 2 - .Location.Width / 2
                            p.Location.X = warp.Exit.X + (warp.Exit.Width / 2) - (p.Location.Width / 2);
//                                .Location.Y = Warp(.Warp).Exit.Y + Warp(.Warp).Exit.Height + 8
                            p.Location.Y = warp.Exit.Y + warp.Exit.Height + 8;
                        }
//                            ElseIf Warp(.Warp).Direction2 = 2 Then
                        if(warp.Direction2 == 2) // RIGHT
                        {
//                                If .Mount = 3 Then .Duck = True
                            if(p.Mount == 3) p.Duck = true;
//                                .Location.X = Warp(.Warp).Exit.X - .Location.Width - 8
                            p.Location.X = warp.Exit.X - p.Location.Width - 8;
//                                .Location.Y = Warp(.Warp).Exit.Y + Warp(.Warp).Exit.Height - .Location.Height - 2
                            p.Location.Y = warp.Exit.Y + warp.Exit.Height - p.Location.Height - 2;
                        }
//                            ElseIf Warp(.Warp).Direction2 = 4 Then
                        if(warp.Direction2 == 4) // LEFT
                        {
//                                If .Mount = 3 Then .Duck = True
                            if(p.Mount == 3) p.Duck = true;
//                                .Location.X = Warp(.Warp).Exit.X + Warp(.Warp).Exit.Width + 8
                            p.Location.X = warp.Exit.X + warp.Exit.Width + 8;
//                                .Location.Y = Warp(.Warp).Exit.Y + Warp(.Warp).Exit.Height - .Location.Height - 2
                            p.Location.Y = warp.Exit.Y + warp.Exit.Height - p.Location.Height - 2;
//                            End If
                        }

                        PlayerFrame(A);
                        CheckSection(A);
                        SoundPause[17] = 0;
                        p.Effect = 8;
                        p.Effect2 = 950;
                    }
                    else if(warp.Effect == 2)
                    {
//                            .Location.X = Warp(.Warp).Exit.X + Warp(.Warp).Exit.Width / 2 - .Location.Width / 2
                        p.Location.X = warp.Exit.X + warp.Exit.Width / 2 - p.Location.Width / 2;
//                            .Location.Y = Warp(.Warp).Exit.Y + Warp(.Warp).Exit.Height - .Location.Height
                        p.Location.Y = warp.Exit.Y + warp.Exit.Height - p.Location.Height;

                        CheckSection(A);
                        p.Effect = 8;
                        p.Effect2 = 2000;
                    }
                    else if(warp.Effect == 3) // Portal warp
                    {
                        p.Location.X = warp.Exit.X + warp.Exit.Width / 2 - p.Location.Width / 2;
                        p.Location.Y = warp.Exit.Y + warp.Exit.Height - p.Location.Height;
                        CheckSection(A);
                        p.WarpCD = 50;
                    }
                }

                if(StartWarp > 0)
                {
                    lastWarpEntered = StartWarp; // Re-use it when player re-enters a level after death (when option is toggled on)
                    StartWarp = 0;
                }
                else
                {
                    lastWarpEntered = 0;
                    ReturnWarp = 0;
                }
            }

            speedRun_resetCurrent();
//'--------------------------------------------
            ProcEvent("Level - Start", true);

            for(int A = 2; A <= maxEvents; ++A)
            {
                if(Events[A].AutoStart)
                    ProcEvent(Events[A].Name, true);
            }

            resetFrameTimer();

            // Update graphics before loop begin (to process inital lazy-unpacking of used sprites)
            UpdateGraphics(true);
            resetFrameTimer();

            speedRun_triggerEnter();

            // MAIN GAME LOOP
            runFrameLoop(nullptr, &GameLoop,
            []()->bool{return !LevelSelect && !GameMenu;},
            []()->bool
            {
                if(!LivingPlayers())
                {
                    EveryonesDead();
                    return true;
                }
                return false;
            });

            if(!GameIsActive)
            {
                speedRun_saveStats();
                return 0;// Break on quit
            }

            // TODO: Utilize this and any TestLevel/MagicHand related code to allow PGE Editor integration
            // (do any code without interaction of no more existnig Editor VB forms, keep IPS with PGE Editor instead)

//            If TestLevel = True Then
            if(TestLevel)
            {
//                TestLevel = False
//                TestLevel = false;
//                LevelEditor = True
//                LevelEditor = true;
//                LevelEditor = true; //FIXME: Restart level testing or quit a game instead of THIS

                if(LevelBeatCode != 0)
                    GameIsActive = false;
                else
                {
                    GameThing();
                    PGE_Delay(500);
                    zTestLevel(setup.testMagicHand, setup.interprocess); // Restart level
                }

                LevelBeatCode = 0;

//                If nPlay.Online = False Then
//                    OpenLevel FullFileName
//                OpenLevel(FullFileName);
//                Else
//                    If nPlay.Mode = 1 Then
//                        Netplay.sendData "H0" & LB
//                        If Len(FullFileName) > 4 Then
//                            If LCase(Right(FullFileName, 4)) = ".lvl" Then
//                                OpenLevel FullFileName
//                            Else
//                                For A = 1 To 15
//                                    If nPlay.ClientCon(A) = True Then Netplay.InitSync A
//                                Next A
//                            End If
//                        Else
//                            For A = 1 To 15
//                                If nPlay.ClientCon(A) = True Then Netplay.InitSync A
//                            Next A
//                        End If
//                    End If
//                End If

//                LevelSelect = False
                LevelSelect = false;
            }
//            Else
            else if(!LevelRestartRequested)
            {
                ClearLevel();
//            End If
            } // TestLevel
        }

    } while(GameIsActive);

    return 0;
}

// game_main_setupvars.cpp

// game_loop.cpp

// menu_loop.cpp

void EditorLoop()
{
    // DUMMY
}

void KillIt()
{
    GameIsActive = false;
#ifndef __ANDROID__
    frmMain.hide();
    if(resChanged)
        SetOrigRes();
#else
    frmMain.clearBuffer();
    frmMain.repaint();
#endif
    QuitMixerX();
    UnloadGFX();
    showCursor(1);
}


void NextLevel()
{
    int A = 0;

    for(A = 1; A <= numPlayers; A++)
        Player[A].HoldingNPC = 0;

    LevelMacro = LEVELMACRO_OFF;
    LevelMacroCounter = 0;
    StopMusic();
    ClearLevel();
    frmMain.setTargetTexture();
    frmMain.clearBuffer();
    frmMain.repaint();
    DoEvents();

    if(!TestLevel && GoToLevel.empty() && !NoMap)
        PGE_Delay(500);

    if(BattleMode && !LevelEditor && !TestLevel)
    {
        EndLevel = false;
        GameMenu = true;
        MenuMode = MENU_BATTLE_MODE;
        MenuCursor = selWorld - 1;
        PlayerCharacter = Player[1].Character;
        PlayerCharacter2 = Player[2].Character;
    }
    else
    {
        LevelSelect = true;
        EndLevel = false;
        if(TestLevel && BattleMode)
        {
            BattleIntro = 150;
            GameIsActive = false; // Quit game
        }
    }
}

// macros mainly used for end of level stuffs. takes over the players controls
void UpdateMacro()
{
    int A = 0;
    bool OnScreen = false;

    if(LevelMacro != LEVELMACRO_OFF && LevelMacroCounter == 0 && IntProc::isEnabled())
    {
        for(int i = 0; i < numPlayers; ++i)
        {
            auto &p = Player[i + 1];
            IntProc::sendPlayerSettings(i, p.Character, p.State, p.Mount, p.MountType);
        }
    }

    if(LevelMacro == LEVELMACRO_CARD_ROULETTE_EXIT) // SMB3 Exit
    {
        for(A = 1; A <= numPlayers; A++)
        {
            if(Player[A].Location.X < level[Player[A].Section].Width && !Player[A].Dead)
            {
                OnScreen = true;
                Player[A].Controls.Down = false;
                Player[A].Controls.Drop = false;
                Player[A].Controls.Jump = false;
                Player[A].Controls.Left = false;
                Player[A].Controls.Right = true;
                Player[A].Controls.Run = false;
                Player[A].Controls.Up = false;
                Player[A].Controls.Start = false;
                Player[A].Controls.AltJump = false;
                Player[A].Controls.AltRun = false;
                if(Player[A].Wet > 0 && Player[A].CanJump)
                {
                    if(Player[A].Location.SpeedY > 1)
                        Player[A].Controls.Jump = true;
                }
            }
            else
            {
                Player[A].Location.SpeedY = -Physics.PlayerGravity;
                Player[A].Controls.Down = false;
                Player[A].Controls.Drop = false;
                Player[A].Controls.Jump = false;
                Player[A].Controls.Left = false;
                Player[A].Controls.Right = true;
                Player[A].Controls.Run = false;
                Player[A].Controls.Up = false;
                Player[A].Controls.Start = false;
                Player[A].Controls.AltJump = false;
                Player[A].Controls.AltRun = false;
            }
        }

        if(!OnScreen)
        {
            LevelMacroCounter += 1;
            if(LevelMacroCounter >= 100)
            {
                LevelBeatCode = 1;
                LevelMacro = LEVELMACRO_OFF;
                LevelMacroCounter = 0;
                EndLevel = true;
            }
        }
    }
    else if(LevelMacro == LEVELMACRO_QUESTION_SPHERE_EXIT)
    {
        for(A = 1; A <= numPlayers; A++)
        {
            Player[A].Controls.Down = false;
            Player[A].Controls.Drop = false;
            Player[A].Controls.Jump = false;
            Player[A].Controls.Left = false;
            Player[A].Controls.Right = false;
            Player[A].Controls.Run = false;
            Player[A].Controls.Up = false;
            Player[A].Controls.Start = false;
            Player[A].Controls.AltJump = false;
            Player[A].Controls.AltRun = false;
        }

        LevelMacroCounter += 1;
        if(LevelMacroCounter >= 460)
        {
            LevelBeatCode = 2;
            EndLevel = true;
            LevelMacro = LEVELMACRO_OFF;
            LevelMacroCounter = 0;
            frmMain.clearBuffer();
        }
    }
    else if(LevelMacro == LEVELMACRO_KEYHOLE_EXIT)
    {
        float tempTime = 0;
        float gameTime = 0;
        int keyholeMax = g_compatibility.fix_keyhole_framerate ? 192 : 300;

        do
        {
            // tempTime = Timer - Int(Timer)

            tempTime = (float(SDL_GetTicks()) / 1000.0f) - std::floor(float(SDL_GetTicks()) / 1000.0f);
//            if(tempTime > (float)(gameTime + 0.01f) || tempTime < gameTime)

            if(g_compatibility.fix_keyhole_framerate)
                DoEvents();

            if(g_compatibility.fix_keyhole_framerate ?
               canProceedFrame() :
               (tempTime > (float)(gameTime + 0.01f) || tempTime < gameTime))
            {
                gameTime = tempTime;

                if(g_compatibility.fix_keyhole_framerate)
                    computeFrameTime1();
                else
                    DoEvents();

                speedRun_tick();
                UpdateGraphics();
                UpdateSound();
                BlockFrames();

                if(g_compatibility.fix_keyhole_framerate)
                {
                    DoEvents();
                    computeFrameTime2();
                }

                LevelMacroCounter += 1;
                if(LevelMacroCounter >= keyholeMax) /*300*/
                    break;
            }

            if(!GameIsActive)
            {
                speedRun_saveStats();
                return;
            }

            PGE_Delay(1);
        } while(true);

        LevelBeatCode = 4;
        EndLevel = true;
        LevelMacro = LEVELMACRO_OFF;
        LevelMacroCounter = 0;
        frmMain.clearBuffer();
    }
    else if(LevelMacro == LEVELMACRO_CRYSTAL_BALL_EXIT)
    {
        for(A = 1; A <= numPlayers; A++)
        {
            Player[A].Controls.Down = false;
            Player[A].Controls.Drop = false;
            Player[A].Controls.Jump = false;
            Player[A].Controls.Left = false;
            Player[A].Controls.Right = false;
            Player[A].Controls.Run = false;
            Player[A].Controls.Up = false;
            Player[A].Controls.Start = false;
            Player[A].Controls.AltJump = false;
            Player[A].Controls.AltRun = false;
        }

        LevelMacroCounter += 1;
        if(LevelMacroCounter >= 300)
        {
            LevelBeatCode = 5;
            EndLevel = true;
            LevelMacro = LEVELMACRO_OFF;
            LevelMacroCounter = 0;
            frmMain.clearBuffer();
        }
    }
    else if(LevelMacro == LEVELMACRO_GAME_COMPLETE_EXIT)
    {
        // numNPCs = 0
        for(A = 1; A <= numPlayers; A++)
        {
            Player[A].Controls.Down = false;
            Player[A].Controls.Drop = false;
            Player[A].Controls.Jump = false;
            Player[A].Controls.Left = false;
            Player[A].Controls.Right = false;
            Player[A].Controls.Run = false;
            Player[A].Controls.Up = false;
            Player[A].Controls.Start = false;
            Player[A].Controls.AltJump = false;
            Player[A].Controls.AltRun = false;
        }

        LevelMacroCounter += 1;
        if(LevelMacroCounter == 250)
            PlaySound(SFX_GameBeat);
        if(LevelMacroCounter >= 800)
        {
            EndLevel = true;
            LevelMacro = LEVELMACRO_OFF;
            LevelMacroCounter = 0;
            if(!TestLevel)
            {
                BeatTheGame = true;
                SaveGame();
                GameOutro = true;
                MenuMode = MENU_MAIN;
                MenuCursor = 0;
            }
            frmMain.clearBuffer();
        }
    }
    else if(LevelMacro == LEVELMACRO_STAR_EXIT) // Star Exit
    {
        for(A = 1; A <= numPlayers; A++)
        {
            Player[A].Controls.Down = false;
            Player[A].Controls.Drop = false;
            Player[A].Controls.Jump = false;
            Player[A].Controls.Left = false;
            Player[A].Controls.Right = false;
            Player[A].Controls.Run = false;
            Player[A].Controls.Up = false;
            Player[A].Controls.Start = false;
            Player[A].Controls.AltJump = false;
            Player[A].Controls.AltRun = false;
        }

        LevelMacroCounter += 1;
        if(LevelMacroCounter >= 300)
        {
            LevelBeatCode = 7;
            LevelMacro = LEVELMACRO_OFF;
            LevelMacroCounter = 0;
            EndLevel = true;
        }
    }
    else if(LevelMacro == LEVELMACRO_GOAL_TAPE_EXIT) // SMW Exit
    {
        for(A = 1; A <= numPlayers; A++)
        {
            if(Player[A].Location.X < level[Player[A].Section].Width && Player[A].Dead == false)
            {
                Player[A].Controls.Down = false;
                Player[A].Controls.Drop = false;
                Player[A].Controls.Jump = false;
                Player[A].Controls.Left = false;
                Player[A].Controls.Right = true;
                Player[A].Controls.Run = false;
                Player[A].Controls.Up = false;
                Player[A].Controls.Start = false;
                Player[A].Controls.AltJump = false;
                Player[A].Controls.AltRun = false;
            }
            else
            {
                Player[A].Location.SpeedY = -Physics.PlayerGravity;
                Player[A].Controls.Down = false;
                Player[A].Controls.Drop = false;
                Player[A].Controls.Jump = false;
                Player[A].Controls.Left = false;
                Player[A].Controls.Right = true;
                Player[A].Controls.Run = false;
                Player[A].Controls.Up = false;
                Player[A].Controls.Start = false;
                Player[A].Controls.AltJump = false;
                Player[A].Controls.AltRun = false;
            }
        }

        LevelMacroCounter += 1;
        if(LevelMacroCounter >= 630)
        {
            LevelBeatCode = 8;
            LevelMacro = LEVELMACRO_OFF;
            LevelMacroCounter = 0;
            EndLevel = true;
        }
    }
}

void InitControls()
{
    int A = 0;
//    int B = 0;
    bool newJoystick = false;

    int joysticksCount = joyInitJoysticks();

    for(int i = 0; i < joysticksCount; ++i)
    {
        newJoystick = joyStartJoystick(i);
        if(newJoystick) {
            A += 1;
        } else {
            break;
        }
    }
    numJoysticks = A;

    /* // Crazy Redigit's solution, useless
//    If numJoysticks = 0 Then
    if(numJoysticks == 0) {
//        useJoystick(1) = 0
        useJoystick[1] = 0;
//        useJoystick(2) = 0
        useJoystick[2] = 0;
//    ElseIf numJoysticks = 1 Then
    } else if(numJoysticks == 1) {
//        useJoystick(1) = 1
        useJoystick[1] = 1;
//        useJoystick(2) = 0
        useJoystick[2] = 0;
//    Else
    } else {
//        useJoystick(1) = 1
        useJoystick[1] = 1;
//        useJoystick(2) = 2
        useJoystick[2] = 2;
//    End If
    }
    */

    For(A, 1, maxLocalPlayers)
        useJoystick[A] = 0;

    For(A, 1, maxLocalPlayers)
    {
        joyFillDefaults(conKeyboard[A]);
        joyFillDefaults(conJoystick[A]);
    }

    OpenConfig();

    // Automatically set the joystick if keyboard chosen
    for(int i = 1; i <= numJoysticks && i <= maxLocalPlayers; i++)
    {
        if(useJoystick[i] <= 0 && !wantedKeyboard[i])
            useJoystick[i] = i;
    }

    for(int player = 1; player <= maxLocalPlayers; ++player)
    {
        if(useJoystick[player] > numJoysticks)
            useJoystick[player] = 0;
        else
        {
            int jip = useJoystick[player];
            int ji = jip - 1;
            if(ji >= 0)
                joyGetByIndex(player, ji, conJoystick[player]);
        }
    }
}


// main_config.cpp


void NPCyFix()
{
    int A = 0;
    float XnH = 0;
    float XnHfix = 0;
    for(A = 1; A <= numNPCs; A++)
    {
        XnH = NPC[A].Location.Y + NPC[A].Location.Height;
        if((int(XnH * 100) % 800) / 100 != 0)
        {
            if((int(XnH + std::abs((int(XnH * 100) % 800) / 100)) * 100) % 800 == 0)
                XnHfix = std::abs((int(XnH * 100) % 800) / 100);
            else
                XnHfix = std::abs(8 - ((int(XnH * 100) % 800) / 100));
            NPC[A].Location.Y = NPC[A].Location.Y + XnHfix;
        }
    }
}

void CheckActive()
{
    // It's useless on Emscripten as no way to check activity (or just differently)
    // and on Android as it has built-in application pauser
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
//    bool MusicPaused = false;
    bool focusLost = false;

    if(neverPause)
        return;

    if(!GameIsActive)
        return;

//    If nPlay.Online = True Then Exit Sub
    // If LevelEditor = False And TestLevel = False Then Exit Sub
    // If LevelEditor = False Then Exit Sub
    while(!frmMain.isWindowActive())
    {
        frmMain.waitEvents();
//        If LevelEditor = True Or MagicHand = True Then frmLevelWindow.vScreen(1).MousePointer = 0
        if(LevelEditor || MagicHand)
            showCursor(0);

        resetFrameTimer();

        resetTimeBuffer();
        //keyDownEnter = false;
        //keyDownAlt = false;

        if(!focusLost)
        {
            pLogDebug("Window Focus lost");
            focusLost = true;
        }

//        if(musicPlaying && !MusicPaused)
//        {
//            MusicPaused = true;
//            SoundPauseEngine(1);
//        }

        if(!GameIsActive)
        {
            speedRun_saveStats();
            break;
        }
    }

    if(focusLost)
        pLogDebug("Window Focus got back");

//    if(MusicPaused)
//        SoundPauseEngine(0);

/* // Useless condition
    if(!noSound && MusicPaused)
    {
        if(MusicPaused)
        {
            SoundResumeAll();
            if(GameOutro == true)
                SoundResumeAll();
            else if(LevelSelect == true && GameMenu == false && LevelEditor == false)
                SoundResumeAll();
            else if(curMusic > 0)
                SoundResumeAll();
            else if(curMusic < 0)
            {
                SoundResumeAll();
                if(PSwitchStop > 0)
                {
                    // mciSendString "resume stmusic", 0, 0, 0
                    SoundResumeAll();
                }
                else
                {
                    // mciSendString "resume smusic", 0, 0, 0
                    SoundResumeAll();
                }
            }
        }
    }
    */
//    If LevelEditor = True Or MagicHand = True Then frmLevelWindow.vScreen(1).MousePointer = 99
#endif // not def __EMSCRIPTEN__
}


Location_t newLoc(double X, double Y, double Width, double Height)
{
    Location_t ret;
    ret.X = X;
    ret.Y = Y;
    ret.Width = Width;
    ret.Height = Height;
    return ret;
}

void MoreScore(int addScore, Location_t Loc)
{
    int mult = 0; // dummy
    MoreScore(addScore, Loc, mult);
}

void MoreScore(int addScore, Location_t Loc, int &Multiplier)
{
    //int oldM = 0;
    int A = 0;

    if(GameMenu || GameOutro || BattleMode)
        return;
    A = addScore + Multiplier;
    if(A == 0)
        return;
    Multiplier++;
    if(A > 13)
        A = 13;
    if(A < addScore)
        A = addScore;
    if(Multiplier > 9)
        Multiplier = 8;
    if(A > 13)
        A = 13;
    if(Points[A] <= 5)
    {
        Lives += Points[A];
        PlaySound(SFX_1up, Points[A] - 1);
    }
    else
        Score += Points[A];
    NewEffect(79, Loc);
    Effect[numEffects].Frame = A - 1;
}

void SizableBlocks()
{
    BlockIsSizable[568] = true;
    BlockIsSizable[579] = true;
    BlockIsSizable[575] = true;
    BlockIsSizable[25] = true;
    BlockIsSizable[26] = true;
    BlockIsSizable[27] = true;
    BlockIsSizable[28] = true;
    BlockIsSizable[38] = true;
    BlockIsSizable[79] = true;
    BlockIsSizable[108] = true;
    BlockIsSizable[130] = true;
    BlockIsSizable[161] = true;
    BlockIsSizable[240] = true;
    BlockIsSizable[241] = true;
    BlockIsSizable[242] = true;
    BlockIsSizable[243] = true;
    BlockIsSizable[244] = true;
    BlockIsSizable[245] = true;
    BlockIsSizable[259] = true;
    BlockIsSizable[260] = true;
    BlockIsSizable[261] = true;
    BlockIsSizable[287] = true;
    BlockIsSizable[288] = true;
    BlockIsSizable[437] = true;
    BlockIsSizable[441] = true;
    BlockIsSizable[442] = true;
    BlockIsSizable[443] = true;
    BlockIsSizable[444] = true;
    BlockIsSizable[438] = true;
    BlockIsSizable[439] = true;
    BlockIsSizable[440] = true;
    BlockIsSizable[445] = true;
}

void StartBattleMode()
{
    int A = 0;
    Player_t blankPlayer;
    numPlayers = 2;
    for(A = 1; A <= numCharacters; A++)
    {
        SavedChar[A] = blankPlayer;
        SavedChar[A].Character = A;
        SavedChar[A].State = 1;
    }
    Player[1].State = 2;
    Player[1].Mount = 0;
    Player[1].Character = 1;
    Player[1].HeldBonus = 0;
    Player[1].CanFly = false;
    Player[1].CanFly2 = false;
    Player[1].TailCount = 0;
    Player[1].YoshiBlue = false;
    Player[1].YoshiRed = false;
    Player[1].YoshiYellow = false;
    Player[1].Hearts = 2;
    Player[2].State = 2;
    Player[2].Mount = 0;
    Player[2].Character = 2;
    Player[2].HeldBonus = 0;
    Player[2].CanFly = false;
    Player[2].CanFly2 = false;
    Player[2].TailCount = 0;
    Player[2].YoshiBlue = false;
    Player[2].YoshiRed = false;
    Player[2].YoshiYellow = false;
    Player[2].Hearts = 2;
    Player[1].Character = PlayerCharacter;
    Player[2].Character = PlayerCharacter2;
    numStars = 0;
    Coins = 0;
    Score = 0;
    Lives = 99;
    BattleLives[1] = 3;
    BattleLives[2] = 3;
    LevelSelect = false;
    GameMenu = false;
    BattleMode = true;
    frmMain.setTargetTexture();
    frmMain.clearBuffer();
    frmMain.repaint();
    StopMusic();
    DoEvents();
    PGE_Delay(500);
    ClearLevel();

    if(NumSelectWorld <= 1)
    {
        MessageText = "Can't start battle because of no levels available";
        PauseGame(1);
        ErrorQuit = true;
    }
    else
    {
        if(selWorld == 1)
            selWorld = (iRand() % (NumSelectWorld - 1)) + 2;
    }

    std::string levelPath = SelectWorld[selWorld].WorldPath + SelectWorld[selWorld].WorldFile;
    if(!OpenLevel(levelPath))
    {
        MessageText = fmt::format_ne("ERROR: Can't open \"{0}\": file doesn't exist or corrupted.", SelectWorld[selWorld].WorldFile);
        PauseGame(1);
        ErrorQuit = true;
    }
    SetupPlayers();

    BattleIntro = 150;
    BattleWinner = 0;
    BattleOutro = 0;
}

void DeleteSave(int world, int save)
{
    auto &w = SelectWorld[world];
    std::string savePath = makeGameSavePath(w.WorldPath,
                                            w.WorldFile,
                                            fmt::format_ne("save{0}.savx", save));
    std::string savePathOld = w.WorldPath + fmt::format_ne("save{0}.savx", save);
    std::string savePathAncient = w.WorldPath + fmt::format_ne("save{0}.sav", save);

    if(Files::fileExists(savePath))
        Files::deleteFile(savePath);
    if(Files::fileExists(savePathOld))
        Files::deleteFile(savePathOld);
    if(Files::fileExists(savePathAncient))
        Files::deleteFile(savePathAncient);

    std::string timersPath = makeGameSavePath(w.WorldPath,
                                              w.WorldFile,
                                              fmt::format_ne("timers{0}.ini", save));
    if(Files::fileExists(timersPath))
        Files::deleteFile(timersPath);

#ifdef __EMSCRIPTEN__
    AppPathManager::syncFs();
#endif
}

void CopySave(int world, int src, int dst)
{
    auto &w = SelectWorld[world];
    std::string savePathOld = SelectWorld[world].WorldPath + fmt::format_ne("save{0}.savx", src);
    std::string savePathAncient = SelectWorld[world].WorldPath + fmt::format_ne("save{0}.sav", src);

    std::string savePathSrc = makeGameSavePath(w.WorldPath,
                                               w.WorldFile,
                                               fmt::format_ne("save{0}.savx", src));
    std::string savePathDst = makeGameSavePath(w.WorldPath,
                                               w.WorldFile,
                                               fmt::format_ne("save{0}.savx", dst));

    if(!Files::fileExists(savePathSrc)) // Attempt to convert an old game-save from the episode directory
    {
        GamesaveData sav;
        bool succ = false;

        if(Files::fileExists(savePathOld))
            succ = FileFormats::ReadExtendedSaveFileF(savePathOld, sav);
        else if(Files::fileExists(savePathAncient))
            succ = FileFormats::ReadSMBX64SavFileF(savePathAncient, sav);

        if(succ)
            FileFormats::WriteExtendedSaveFileF(savePathSrc, sav);
    }

    Files::copyFile(savePathDst, savePathSrc, true);

    std::string timersPathSrc = makeGameSavePath(w.WorldPath,
                                                 w.WorldFile,
                                                 fmt::format_ne("timers{0}.ini", src));
    std::string timersPathDst = makeGameSavePath(w.WorldPath,
                                                 w.WorldFile,
                                                 fmt::format_ne("timers{0}.ini", dst));
    Files::copyFile(timersPathDst, timersPathSrc, true);

#ifdef __EMSCRIPTEN__
    AppPathManager::syncFs();
#endif
}
