/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2024 Vitaly Novichkov <admin@wohlnet.ru>
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

#include "sdl_proxy/sdl_timer.h"
#include "sdl_proxy/sdl_atomic.h"

#ifndef PGE_NO_THREADING
#include <SDL2/SDL_thread.h>
#endif

#include <fmt_format_ne.h>
#include <array>

#include <AppPath/app_path.h>
#include <DirManager/dirman.h>
#include <Utils/files.h>
#include <PGE_File_Formats/file_formats.h>
#include <Integrator/integrator.h>

#include "menu_main.h"
#include "game_info.h"
#include "../version.h"
#include "../gfx.h"
#include "screen_connect.h"
#include "menu_controls.h"
#include "translate_episode.h"

#include "speedrunner.h"
#include "main/gameplay_timer.h"
#include "../game_main.h"
#include "../sound.h"
#include "../player.h"
#include "../collision.h"
#include "../graphics.h"
#include "../core/render.h"
#include "../core/window.h"
#include "../core/events.h"
#include "../controls.h"
#include "../config.h"
#include "../compat.h"
#include "level_file.h"
#include "world_file.h"
#include "pge_delay.h"
#include "video.h"
#include "change_res.h"
#include "game_globals.h"
#include "core/language.h"
#include "main/translate.h"
#include "fontman/font_manager.h"

#include "video.h"
#include "editor.h"
#include "frm_main.h"

#include "screen_textentry.h"
#include "editor/new_editor.h"
#include "editor/write_level.h"
#include "editor/write_world.h"
#include "editor/editor_custom.h"
#include "script/luna/luna.h"

MainMenuContent g_mainMenu;

#ifndef PGE_NO_THREADING
static bool                 s_atomicsInited = false;
static SDL_atomic_t         loading = {};
static SDL_atomic_t         loadingProgrss = {};
static SDL_atomic_t         loadingProgrssMax = {};

static SDL_Thread*          loadingThread = nullptr;
#endif

static constexpr int c_menuSavesPerPage     = 3;
static constexpr int c_menuSavesPageLength  = c_menuSavesPerPage + 2;
static constexpr int c_menuSavesPageCount   = (maxSaveSlots + c_menuSavesPerPage - 1) / c_menuSavesPerPage;
static constexpr int c_menuSavesFooterHint  = (c_menuSavesPageLength * 30) - 30;
static constexpr int c_menuItemSavesEndList = c_menuSavesPageCount * c_menuSavesPageLength - 1;
static constexpr int c_menuItemSavesCopy    = -2;
static constexpr int c_menuItemSavesDelete  = -1;

// returns the offset in MenuCursor items for the current page
static inline int s_GetSavesPageMenuOffset()
{
    return (MenuCursor / c_menuSavesPageLength) * c_menuSavesPageLength;
}

// returns the offset in Save Slots for the current page
static inline int s_GetSavesPageSaveOffset()
{
    return (MenuCursor / c_menuSavesPageLength) * c_menuSavesPerPage;
}

// returns the number of save pages that actually exist currently
static inline int s_GetCurrentSavesPageCount()
{
    int max_all = -1;
    int max_any = -1;

    for(int page_i = 0; page_i < c_menuSavesPageCount; page_i++)
    {
        bool any = false;
        bool all = true;

        int page_base = c_menuSavesPerPage * page_i;
        for(int slot_i = 1; slot_i <= c_menuSavesPerPage && page_base + slot_i <= maxSaveSlots; slot_i++)
        {
            if(SaveSlotInfo[page_base + slot_i].Progress >= 0)
                any = true;
            else
                all = false;
        }

        if(any)
            max_any = page_i;

        if(all)
            max_all = page_i;
    }

    return SDL_min(SDL_max(SDL_max(max_all + 1, max_any), MenuCursor / c_menuSavesPageLength) + 1, c_menuSavesPageCount);
}

// returns c_menuItemSavesCopy, c_menuItemSavesDelete, or an actual save slot
static inline int s_GetSavesActionIdx(bool check_bounds = true)
{
    int item = ((MenuCursor + 2) % c_menuSavesPageLength) - 2;
    if(item >= 0)
        item += (MenuCursor / c_menuSavesPageLength) * c_menuSavesPerPage + 1;

    if(check_bounds && item > maxSaveSlots)
        item = maxSaveSlots;

    return item;
}

int NumSelectWorld = 0;
int NumSelectWorldEditable = 0;
int NumSelectBattle = 0;
std::vector<SelectWorld_t> SelectWorld;
std::vector<SelectWorld_t> SelectWorldEditable;
std::vector<SelectWorld_t> SelectBattle;


void initMainMenu()
{
#ifndef PGE_NO_THREADING
    if(!s_atomicsInited)
    {
        SDL_AtomicSet(&loading, 0);
        SDL_AtomicSet(&loadingProgrss, 0);
        SDL_AtomicSet(&loadingProgrssMax, 0);
        s_atomicsInited = true;
    }
#endif

    g_mainMenu.introPressStart = "Press Start";

    g_mainMenu.mainStartGame = "Start Game";
    g_mainMenu.main1PlayerGame = "1 Player Game";
    g_mainMenu.mainMultiplayerGame = "2 Player Game";
    g_mainMenu.mainBattleGame = "Battle Game";
    g_mainMenu.mainEditor = "Editor";
    g_mainMenu.mainOptions = "Options";
    g_mainMenu.mainExit = "Exit";

    g_mainMenu.loading = "Loading...";

    g_mainMenu.languageName = "English";
    g_mainMenu.pluralRules = "one-is-singular";

    g_mainMenu.selectCharacter = "{0} game";

    g_mainMenu.editorBattles = "<Battle Levels>";
    g_mainMenu.editorNewWorld = "<New World>";
    g_mainMenu.editorErrorResolution = "Sorry! The in-game editor is not supported at your current resolution.";
    g_mainMenu.editorErrorMissingResources = "Sorry! You are missing {0}, required for the in-game editor.";
    g_mainMenu.editorPromptNewWorldName = "New world name";

    g_mainMenu.gameNoEpisodesToPlay = "<No episodes to play>";
    g_mainMenu.gameNoBattleLevels = "<No battle levels>";
    g_mainMenu.gameBattleRandom = "Random Level";

    g_mainMenu.gameSlotContinue = "SLOT {0} ... {1}%";
    g_mainMenu.gameSlotNew = "SLOT {0} ... NEW GAME";
    g_mainMenu.gameCopySave = "Copy save";
    g_mainMenu.gameEraseSave = "Erase save";
    g_mainMenu.gameSourceSlot = "Select the source slot";
    g_mainMenu.gameTargetSlot = "Now select the target";
    g_mainMenu.gameEraseSlot = "Select the slot to erase";

    g_mainMenu.phraseScore = "Score: {0}";
    g_mainMenu.phraseTime = "Time: {0}";

    g_mainMenu.errorBattleNoLevels = "Can't start battle because of no levels available";

    g_mainMenu.optionsModeFullScreen = "Fullscreen mode";
    g_mainMenu.optionsModeWindowed = "Windowed mode";
    g_mainMenu.optionsViewCredits = "View credits";
    g_mainMenu.optionsRestartEngine = "Restart engine for changes to take effect.";
    g_mainMenu.optionsRender = "Render: {0}";
    g_mainMenu.optionsRenderAuto = "Render: Auto ({0})";
    g_mainMenu.optionsRenderX = "Render: {0} (X)";
    g_mainMenu.optionsScaleMode = "Scale";
    g_mainMenu.optionsScaleInteger = "Integer";
    g_mainMenu.optionsScaleNearest = "Nearest";
    g_mainMenu.optionsScaleLinear = "Linear";

    g_mainMenu.connectCharSelTitle = "Character Select";
    g_mainMenu.connectStartGame = "Start Game";

    g_mainMenu.wordPlayer = "Player";
    g_mainMenu.wordProfile = "Profile";
    g_mainMenu.wordBack = "Back";
    g_mainMenu.wordResume = "Resume";
    g_mainMenu.wordWaiting = "Waiting";

    g_mainMenu.controlsTitle = "Controls";
    g_mainMenu.controlsConnected = "Connected:";
    g_mainMenu.controlsDeleteKey = "(Alt Jump to Delete)";
    g_mainMenu.controlsDeviceTypes = "Device Types";
    g_mainMenu.controlsInUse = "(In Use)";
    g_mainMenu.controlsNotInUse = "(Not In Use)";

    g_mainMenu.controlsActivateProfile = "Activate profile";
    g_mainMenu.controlsRenameProfile = "Rename profile";
    g_mainMenu.controlsDeleteProfile = "Delete profile";
    g_mainMenu.controlsPlayerControls = "Player controls";
    g_mainMenu.controlsCursorControls = "Cursor controls";
    g_mainMenu.controlsEditorControls = "Editor controls";
    g_mainMenu.controlsHotkeys = "Hotkeys";

    g_mainMenu.controlsOptionRumble = "Rumble";
    // g_mainMenu.controlsOptionGroundPoundButton = "Ground Pound Button";
    g_mainMenu.controlsOptionBatteryStatus = "Battery Status";

    g_mainMenu.wordProfiles = "Profiles";
    g_mainMenu.wordButtons  = "Buttons";

    g_mainMenu.controlsReallyDeleteProfile = "Really delete profile?";
    g_mainMenu.controlsNewProfile = "<New Profile>";

    g_mainMenu.wordNo   = "No";
    g_mainMenu.wordYes  = "Yes";
    g_mainMenu.wordOkay = "Okay";
    g_mainMenu.caseNone = "<None>";
    g_mainMenu.wordOn   = "On";
    g_mainMenu.wordOff  = "Off";
    g_mainMenu.wordShow = "Show";
    g_mainMenu.wordHide = "Hide";
    g_mainMenu.wordLanguage = "Language";
    g_mainMenu.abbrevMilliseconds = "MS";

    g_mainMenu.promptDeprecatedSetting = "This file uses a deprecated compatibility flag that will be removed in version 1.3.7.\n\nOld flag: \"{0}\"\nNew flag: \"{1}\"\n\n\nReplace it with the updated flag for version 1.3.6 and newer?";
    g_mainMenu.promptDeprecatedSettingUnwritable = "An unwritable file ({0}) uses a deprecated compatibility flag that will be removed in version 1.3.7.\n\nSection: [{1}]\nOld flag: \"{2}\"\nNew flag: \"{3}\"\n\n\nPlease update it manually and copy to your device.";
}


static int ScrollDelay = 0;
static int menuPlayersNum = 0;
static int menuBattleMode = false;

static int menuCopySaveSrc = 0;
static int menuCopySaveDst = 0;
static int menuRecentEpisode = -1;

static int listMenuLastScroll = 0;
static int listMenuLastCursor = 0;

static const int TinyScreenH = 400;
static const int SmallScreenH = 500;
static const int TinyScreenW = 600;

void GetMenuPos(int* MenuX, int* MenuY)
{
    if(MenuX)
        *MenuX = ScreenW / 2 - 100;

    if(MenuY)
        *MenuY = ScreenH - 250;

    // tweaks for MenuX
    if(MenuX && ScreenW < TinyScreenW)
    {
        *MenuX = ScreenW / 2 - 240;
        if(*MenuX < 24)
            *MenuX = 24;
    }

    // the rest is tweaks for MenuY
    if(!MenuY)
        return;

    if(ScreenH < TinyScreenH)
        *MenuY = 100;
    else if(ScreenH < SmallScreenH)
    {
        if(MenuMode == MENU_OPTIONS)
            *MenuY = ScreenH - 250;
        else
            *MenuY = ScreenH - 220;
    }
}

static void s_findRecentEpisode()
{
    menuRecentEpisode = -1;

    std::vector<SelectWorld_t>& SelectorList
        = (MenuMode == MENU_EDITOR)
        ? SelectWorldEditable
        : SelectWorld;

    for(size_t i = 1; i < SelectorList.size(); ++i)
    {
        // special editor entry for battle levels
        bool is_battle = (MenuMode == MENU_EDITOR && i == SelectorList.size() - 2);

        auto &w = SelectorList[i];
        const std::string wPath = (!is_battle) ? w.WorldPath + w.WorldFile : "battle";

        if((MenuMode == MENU_1PLAYER_GAME && wPath == g_recentWorld1p) ||
           (MenuMode == MENU_2PLAYER_GAME && wPath == g_recentWorld2p) ||
           (MenuMode == MENU_EDITOR && wPath == g_recentWorldEditor))
        {
            menuRecentEpisode = (int)i - 1;
            w.highlight = true;
        }
        else
        {
            w.highlight = false;
        }
    }

    if(menuRecentEpisode >= 0)
        worldCurs = menuRecentEpisode - 3;

    MenuCursor = (menuRecentEpisode < 0) ? 0 : menuRecentEpisode;
}

#if !defined(PGE_NO_THREADING)
static int FindWorldsThread(void *)
{
    FindWorlds();
    return 0;
}
#endif

#if (defined(__APPLE__) && defined(USE_BUNDLED_ASSETS)) || defined(FIXED_ASSETS_PATH)
#   define CAN_WRITE_APPPATH_WORLDS false
#else
#   define CAN_WRITE_APPPATH_WORLDS true
#endif

struct WorldRoot_t
{
    std::string path;
    bool editable;
};

// helper functions used by FindWorlds() and LoadSingleWorld()
static void s_LoadSingleWorld(const std::string& epDir, const std::string& fName, WorldData& head, TranslateEpisode& tr, bool compatModern, bool editable);
static void s_FinishFindWorlds();

void FindWorlds()
{
    TranslateEpisode tr;
    WorldData head;
    bool compatModern = (CompatGetLevel() == COMPAT_MODERN);
    NumSelectWorld = 0;

    std::vector<WorldRoot_t> worldRoots =
    {
        {AppPath + "worlds/", CAN_WRITE_APPPATH_WORLDS}
    };

    if(AppPathManager::userDirIsAvailable())
        worldRoots.push_back({AppPathManager::userWorldsRootDir(), true});

#ifdef APP_PATH_HAS_EXTRA_WORLDS
    // can't edit base assets if they're a romfs package (different from the user dir)
    if(AppPathManager::userDirIsAvailable())
        worldRoots[0].editable = false;

    // add worlds from additional romfs packages
    for(const std::string& root : AppPathManager::worldRootDirs())
        worldRoots.push_back({root, false});
#endif

    SelectWorld.clear();
    SelectWorld.emplace_back(SelectWorld_t()); // Dummy entry
    SelectWorldEditable.clear();
    SelectWorldEditable.push_back(SelectWorld_t()); // Dummy entry

#ifndef PGE_NO_THREADING
    SDL_AtomicSet(&loadingProgrss, 0);
    SDL_AtomicSet(&loadingProgrssMax, 0);

    for(const auto &worldsRoot : worldRoots)
    {
        std::vector<std::string> dirs;
        DirMan episodes(worldsRoot.path);
        episodes.getListOfFolders(dirs);
        SDL_AtomicAdd(&loadingProgrssMax, (int)dirs.size());
    }
#endif

    for(const auto &worldsRoot : worldRoots)
    {
        DirMan episodes(worldsRoot.path);

        std::vector<std::string> dirs;
        std::vector<std::string> files;
        episodes.getListOfFolders(dirs);

        for(auto &dir : dirs)
        {
            std::string epDir = worldsRoot.path + dir + "/";
            DirMan episode(epDir);
            episode.getListOfFiles(files, {".wld", ".wldx"});

            for(std::string &fName : files)
                s_LoadSingleWorld(epDir, fName, head, tr, compatModern, worldsRoot.editable);

#ifndef PGE_NO_THREADING
            SDL_AtomicAdd(&loadingProgrss, 1);
#endif
        }
    }

    s_FinishFindWorlds();
}

static void s_LoadSingleWorld(const std::string& epDir, const std::string& fName, WorldData& head, TranslateEpisode& tr, bool compatModern, bool editable)
{
    std::string wPath = epDir + fName;

    if(FileFormats::OpenWorldFileHeader(wPath, head))
    {
        SelectWorld_t w;
        w.WorldName = head.EpisodeTitle;
        head.charactersToS64();
        w.WorldPath = epDir;
        w.WorldFile = fName;
        if(w.WorldName.empty())
            w.WorldName = fName;

        w.blockChar[1] = head.nocharacter1;
        w.blockChar[2] = head.nocharacter2;

        if(head.meta.RecentFormat != LevelData::SMBX64 || head.meta.RecentFormatVersion >= 30 || !compatModern)
        {
            w.blockChar[3] = head.nocharacter3;
            w.blockChar[4] = head.nocharacter4;
            w.blockChar[5] = head.nocharacter5;
        }
        else
        {
            w.blockChar[3] = true;
            w.blockChar[4] = true;
            w.blockChar[5] = true;
        }

        w.editable = editable;

        if(tr.tryTranslateTitle(epDir, fName, w.WorldName))
            pLogDebug("Translated world title: %s", w.WorldName.c_str());

        SelectWorld.push_back(w);
        if(editable)
            SelectWorldEditable.push_back(w);
    }
}

static void s_FinishFindWorlds()
{
    if(SelectWorld.size() <= 1) // No available worlds in the list
    {
        SelectWorld.clear();
        SelectWorld.emplace_back(SelectWorld_t()); // Dummy entry
        SelectWorld.emplace_back(SelectWorld_t()); // "no episodes to play" entry
        SelectWorld[1].WorldName = g_mainMenu.gameNoEpisodesToPlay;
        SelectWorld[1].disabled = true;
    }

    // Sort all worlds by alphabetical order
    std::sort(SelectWorld.begin(), SelectWorld.end(),
              [](const SelectWorld_t& a, const SelectWorld_t& b)
    {
        return a.WorldName < b.WorldName;
    });

    std::sort(SelectWorldEditable.begin(), SelectWorldEditable.end(),
              [](const SelectWorld_t& a, const SelectWorld_t& b)
    {
        return a.WorldName < b.WorldName;
    });

    NumSelectWorld = (int)(SelectWorld.size() - 1);

    SelectWorld_t battles = SelectWorld_t();
    battles.WorldName = g_mainMenu.editorBattles;
    SelectWorldEditable.push_back(battles);

    SelectWorld_t createWorld = SelectWorld_t();
    createWorld.WorldName = g_mainMenu.editorNewWorld;
    SelectWorldEditable.push_back(createWorld);

    NumSelectWorldEditable = ((int)SelectWorldEditable.size() - 1);

    s_findRecentEpisode();

#ifndef PGE_NO_THREADING
    SDL_AtomicSet(&loading, 0);
#endif
}

void LoadSingleWorld(const std::string wPath)
{
    TranslateEpisode tr;
    WorldData head;
    bool compatModern = (CompatGetLevel() == COMPAT_MODERN);
    NumSelectWorld = 0;

    SelectWorld.clear();
    SelectWorld.emplace_back(SelectWorld_t()); // Dummy entry
    SelectWorldEditable.clear();
    SelectWorldEditable.push_back(SelectWorld_t()); // Dummy entry

    std::string fName = Files::basename(wPath);
    std::string epDir = wPath.substr(0, wPath.size() - fName.size());

    if(epDir.empty())
        epDir = "./";

    epDir = DirMan(epDir).absolutePath() + "/";

    s_LoadSingleWorld(epDir, fName, head, tr, compatModern, true);

    s_FinishFindWorlds();
}

#if !defined(THEXTECH_PRELOAD_LEVELS) && !defined(PGE_NO_THREADING)
static int FindLevelsThread(void *)
{
    FindLevels();
    return 0;
}
#endif

void FindLevels()
{
    std::vector<WorldRoot_t> battleRoots =
    {
        {AppPath + "battle/", CAN_WRITE_APPPATH_WORLDS}
    };

    if(AppPathManager::userDirIsAvailable())
        battleRoots.push_back({AppPathManager::userBattleRootDir(), true});

    SelectBattle.clear();
    SelectBattle.emplace_back(SelectWorld_t()); // Dummy entry

    NumSelectBattle = 1;
    SelectBattle.emplace_back(SelectWorld_t()); // "random level" entry
    SelectBattle[1].WorldName = g_mainMenu.gameBattleRandom;
    LevelData head;

#ifndef PGE_NO_THREADING
    SDL_AtomicSet(&loadingProgrss, 0);
    SDL_AtomicSet(&loadingProgrssMax, 0);

    for(const auto &battleRoot : battleRoots)
    {
        std::vector<std::string> files;
        DirMan battleLvls(battleRoot.path);
        battleLvls.getListOfFiles(files, {".lvl", ".lvlx"});
        SDL_AtomicAdd(&loadingProgrssMax, (int)files.size());
    }
#endif

    for(const auto &battleRoot : battleRoots)
    {
        std::vector<std::string> files;
        DirMan battleLvls(battleRoot.path);
        battleLvls.getListOfFiles(files, {".lvl", ".lvlx"});
        for(std::string &fName : files)
        {
            std::string wPath = battleRoot.path + fName;
            if(FileFormats::OpenLevelFileHeader(wPath, head))
            {
                SelectWorld_t w;
                w.WorldPath = battleRoot.path;
                w.WorldFile = fName;
                w.WorldName = head.LevelName;
                if(w.WorldName.empty())
                    w.WorldName = fName;
                w.editable = battleRoot.editable;
                SelectBattle.push_back(w);
            }
#ifndef PGE_NO_THREADING
            SDL_AtomicAdd(&loadingProgrss, 1);
#endif
        }
    }

    NumSelectBattle = ((int)SelectBattle.size() - 1);
#ifndef PGE_NO_THREADING
    SDL_AtomicSet(&loading, 0);
#endif

    if(SelectBattle.size() <= 2) // No available levels in the list
    {
        SelectBattle.clear();
        SelectBattle.emplace_back(SelectWorld_t()); // Dummy entry

        NumSelectBattle = 1;
        SelectBattle.emplace_back(SelectWorld_t()); // "no battle levels" entry
        SelectBattle[1].WorldName = g_mainMenu.gameNoBattleLevels;
        SelectBattle[1].disabled = true;
    }
}


static void s_handleMouseMove(int items, int x, int y, int maxWidth, int itemHeight)
{
    For(A, 0, items)
    {
        if(SharedCursor.Y >= y + A * itemHeight && SharedCursor.Y <= y + 16 + A * itemHeight)
        {
            if(SharedCursor.X >= x && SharedCursor.X <= x + maxWidth)
            {
                if(MenuMouseRelease && SharedCursor.Primary)
                    MenuMouseClick = true;
                if(MenuCursor != A)
                {
                    PlaySoundMenu(SFX_Slide);
                    MenuCursor = A;
                    break;
                }
            }
        }
    }
}



bool mainMenuUpdate()
{
    int MenuX, MenuY;
    GetMenuPos(&MenuX, &MenuY);

    int B;
    // Location_t tempLocation;
    int menuLen;
    // Player_t blankPlayer;

    bool upPressed = SharedControls.MenuUp;
    bool downPressed = SharedControls.MenuDown;
    bool leftPressed = SharedControls.MenuLeft;
    bool rightPressed = SharedControls.MenuRight;
    bool homePressed = SharedCursor.Tertiary;

    bool menuDoPress = SharedControls.MenuDo || SharedControls.Pause;
    bool menuBackPress = SharedControls.MenuBack;

    for(int i = 0; i < maxLocalPlayers; i++)
    {
        Controls_t &c = Player[i + 1].Controls;

        menuDoPress |= c.Start || c.Jump;
        menuBackPress |= c.Run;

        upPressed |= c.Up;
        downPressed |= c.Down;
        leftPressed |= c.Left;
        rightPressed |= c.Right;

        homePressed |= c.Drop;
    }

    menuBackPress |= SharedCursor.Secondary && MenuMouseRelease;

    if(menuBackPress && menuDoPress)
        menuDoPress = false;

    {
        if(XWindow::getCursor() != CURSOR_NONE)
        {
            XWindow::setCursor(CURSOR_NONE);
            XWindow::showCursor(0);
        }

        {
            bool k = false;
            k |= menuBackPress;
            k |= menuDoPress;
            k |= upPressed;
            k |= downPressed;
            k |= leftPressed;
            k |= rightPressed;
            k |= homePressed;

            if(!k)
                MenuCursorCanMove = true;
        }

        if(!g_pollingInput && (MenuMode != MENU_CHARACTER_SELECT_NEW && MenuMode != MENU_CHARACTER_SELECT_NEW_BM && MenuMode != MENU_INTRO))
        {
            int cursorDelta = 0;
            bool page_flip = false;
            int pre_flip_page_count = 0;

            if(upPressed)
            {
                if(MenuCursorCanMove)
                {
                    MenuCursor -= 1;
                    cursorDelta = -1;
                }

                MenuCursorCanMove = false;
            }
            else if(downPressed)
            {
                if(MenuCursorCanMove)
                {
                    MenuCursor += 1;
                    cursorDelta = +1;
                }

                MenuCursorCanMove = false;
            }
            else if(MenuMode >= MENU_SELECT_SLOT_BASE && MenuMode < MENU_SELECT_SLOT_END)
            {
                if(leftPressed || SharedCursor.ScrollUp)
                {
                    if(MenuCursorCanMove && (MenuCursor % c_menuSavesPageLength < c_menuSavesPerPage) && (pre_flip_page_count = s_GetCurrentSavesPageCount()) > 1)
                    {
                        MenuCursor -= c_menuSavesPageLength;
                        cursorDelta = -1;
                        page_flip = true;
                    }

                    MenuCursorCanMove = false;
                }
                else if(rightPressed || SharedCursor.ScrollDown)
                {
                    if(MenuCursorCanMove && (MenuCursor % c_menuSavesPageLength < c_menuSavesPerPage) && (pre_flip_page_count = s_GetCurrentSavesPageCount()) > 1)
                    {
                        MenuCursor += c_menuSavesPageLength;
                        cursorDelta = -1;
                        page_flip = true;
                    }

                    MenuCursorCanMove = false;
                }
            }

            if(cursorDelta != 0)
            {
                if(MenuMode >= MENU_CHARACTER_SELECT_BASE && MenuMode <= MENU_CHARACTER_SELECT_BASE_END)
                {
                    while((MenuCursor == (PlayerCharacter - 1) &&
                          (MenuMode == MENU_CHARACTER_SELECT_2P_S2 || MenuMode == MENU_CHARACTER_SELECT_BM_S2)) ||
                           blockCharacter[MenuCursor + 1])
                    {
                        MenuCursor += cursorDelta;
                        if(MenuCursor < 0)
                            MenuCursor = numCharacters - 1;
                        else if(MenuCursor >= numCharacters)
                            MenuCursor = 0;
                    }
                }

                if(MenuMode >= MENU_SELECT_SLOT_BASE && MenuMode < MENU_SELECT_SLOT_END)
                {
                    if(page_flip)
                    {
                        // total wrap
                        if(MenuCursor < 0)
                            MenuCursor += pre_flip_page_count * c_menuSavesPageLength;
                        else if(MenuCursor >= pre_flip_page_count * c_menuSavesPageLength)
                            MenuCursor -= pre_flip_page_count * c_menuSavesPageLength;

                        // backup total wrap
                        if(MenuCursor < 0)
                            MenuCursor += c_menuItemSavesEndList + 1;
                        else if(MenuCursor > c_menuItemSavesEndList)
                            MenuCursor -= c_menuItemSavesEndList + 1;
                    }
                    else
                    {
                        // page wrap
                        if(MenuCursor < 0 || (upPressed && (MenuCursor % c_menuSavesPageLength) == c_menuSavesPageLength - 1))
                            MenuCursor += c_menuSavesPageLength;
                        else if(downPressed && (MenuCursor % c_menuSavesPageLength) == 0)
                            MenuCursor -= c_menuSavesPageLength;
                    }

                    // fix the cursor if invalid
                    bool actions_okay = (MenuMode == MENU_SELECT_SLOT_1P || MenuMode == MENU_SELECT_SLOT_2P);
                    if(!actions_okay && s_GetSavesActionIdx() <= 0)
                    {
                        if(downPressed)
                            MenuCursor = s_GetSavesPageMenuOffset();
                        else
                            MenuCursor = s_GetSavesPageMenuOffset() + c_menuSavesPerPage - 1;
                    }

                    if(s_GetSavesActionIdx(false) > maxSaveSlots)
                    {
                        if(downPressed)
                            MenuCursor = s_GetSavesPageMenuOffset() + actions_okay * c_menuSavesPerPage;
                        else
                            MenuCursor = s_GetSavesPageMenuOffset() + (maxSaveSlots % c_menuSavesPerPage) - 1;
                    }
                }

                if(page_flip)
                    PlaySoundMenu(SFX_Saw);
                else
                    PlaySoundMenu(SFX_Slide);
            }

        } // No keyboard/Joystick grabbing active

        if(MenuMode == MENU_INTRO && ScreenH >= TinyScreenH)
            MenuMode = MENU_MAIN;

#ifndef PGE_NO_THREADING
        if(SDL_AtomicGet(&loading))
        {
            if((menuDoPress && MenuCursorCanMove) || MenuMouseClick)
                PlaySoundMenu(SFX_BlockHit);

            if(MenuCursor != 0)
                MenuCursor = 0;
        }
        else
#endif

        // Menu Intro
        if(MenuMode == MENU_INTRO)
        {
            if(MenuMouseRelease && SharedCursor.Primary)
                MenuMouseClick = true;
            if(menuBackPress && MenuCursorCanMove)
            {
                int quitKeyPos = 2;
                if(!g_gameInfo.disableTwoPlayer)
                    quitKeyPos ++;
                if(!g_gameInfo.disableBattleMode)
                    quitKeyPos ++;
                if(g_config.enable_editor)
                    quitKeyPos ++;

                MenuMode = MENU_MAIN;
                MenuCursor = quitKeyPos;
                MenuCursorCanMove = false;
                PlaySoundMenu(SFX_Slide);
            }
            if((menuDoPress && MenuCursorCanMove) || MenuMouseClick)
            {
                MenuCursorCanMove = false;
                MenuMode = MENU_MAIN;
                MenuCursor = 0;
                PlaySoundMenu(SFX_Do);
            }
        }
        // Main Menu
        else if(MenuMode == MENU_MAIN)
        {
            if(SharedCursor.Move)
            {
                For(A, 0, 10)
                {
                    if(SharedCursor.Y >= MenuY + A * 30 && SharedCursor.Y <= MenuY + A * 30 + 16)
                    {
                        int i = 0;
                        if(A == i++)
                            menuLen = 18 * (g_gameInfo.disableTwoPlayer ? (int)g_mainMenu.main1PlayerGame.size() : (int)g_mainMenu.mainStartGame.size()) - 2;
                        else if(!g_gameInfo.disableTwoPlayer && A == i++)
                            menuLen = 18 * (int)g_mainMenu.mainMultiplayerGame.size() - 2;
                        else if(!g_gameInfo.disableBattleMode && A == i++)
                            menuLen = 18 * (int)g_mainMenu.mainBattleGame.size();
                        else if(g_config.enable_editor && A == i++)
                            menuLen = 18 * (int)g_mainMenu.mainEditor.size();
                        else if(A == i++)
                            menuLen = 18 * (int)g_mainMenu.mainOptions.size();
                        else if(A == i++)
                            menuLen = 18 * (int)g_mainMenu.mainExit.size();
                        else
                            break;

                        if(SharedCursor.X >= MenuX && SharedCursor.X <= MenuX + menuLen)
                        {
                            if(MenuMouseRelease && SharedCursor.Primary)
                                MenuMouseClick = true;

                            if(MenuCursor != A)
                            {
                                PlaySoundMenu(SFX_Slide);
                                MenuCursor = A;
                            }
                        }
                    }
                }
            }

            if(menuBackPress && MenuCursorCanMove)
            {
                int quitKeyPos = 2;
                if(!g_gameInfo.disableTwoPlayer)
                    quitKeyPos ++;
                if(!g_gameInfo.disableBattleMode)
                    quitKeyPos ++;
                if(g_config.enable_editor)
                    quitKeyPos ++;

                if(ScreenH < TinyScreenH)
                {
                    MenuCursorCanMove = false;
                    MenuMode = MENU_INTRO;
                    PlaySoundMenu(SFX_Slide);
                }
                else if(MenuCursor != quitKeyPos)
                {
                    MenuCursor = quitKeyPos;
                    PlaySoundMenu(SFX_Slide);
                }
            }
            else if((menuDoPress && MenuCursorCanMove) || MenuMouseClick)
            {
                MenuCursorCanMove = false;
                PlayerCharacter = 0;
                PlayerCharacter2 = 0;

                int i = 0;
                if(MenuCursor == i++)
                {
                    PlaySoundMenu(SFX_Do);
                    MenuMode = MENU_1PLAYER_GAME;
                    menuPlayersNum = 1;
                    menuBattleMode = false;
                    MenuCursor = 0;
#ifdef THEXTECH_PRELOAD_LEVELS
                    s_findRecentEpisode();
#elif defined(PGE_NO_THREADING)
                    FindWorlds();
#else
                    SDL_AtomicSet(&loading, 1);
                    loadingThread = SDL_CreateThread(FindWorldsThread, "FindWorlds", nullptr);
                    SDL_DetachThread(loadingThread);
#endif
                }
                else if(!g_gameInfo.disableTwoPlayer && MenuCursor == i++)
                {
                    PlaySoundMenu(SFX_Do);
                    MenuMode = MENU_2PLAYER_GAME;
                    menuPlayersNum = 2;
                    menuBattleMode = false;
                    MenuCursor = 0;
#ifdef THEXTECH_PRELOAD_LEVELS
                    s_findRecentEpisode();
#elif defined(PGE_NO_THREADING)
                    FindWorlds();
#else
                    SDL_AtomicSet(&loading, 1);
                    loadingThread = SDL_CreateThread(FindWorldsThread, "FindWorlds", nullptr);
                    SDL_DetachThread(loadingThread);
#endif
                }
                else if(!g_gameInfo.disableBattleMode && MenuCursor == i++)
                {
                    PlaySoundMenu(SFX_Do);
                    MenuMode = MENU_BATTLE_MODE;
                    menuPlayersNum = 2;
                    menuBattleMode = true;
#if !defined(THEXTECH_PRELOAD_LEVELS) && defined(PGE_NO_THREADING)
                    FindLevels();
#elif !defined(THEXTECH_PRELOAD_LEVELS)
                    SDL_AtomicSet(&loading, 1);
                    loadingThread = SDL_CreateThread(FindLevelsThread, "FindLevels", nullptr);
                    SDL_DetachThread(loadingThread);
#endif
                    MenuCursor = 0;
                }
                else if(g_config.enable_editor && MenuCursor == i++)
                {
                    if(ScreenW < 640 || ScreenH < 480)
                    {
                        PlaySoundMenu(SFX_BlockHit);
                        MessageText = g_mainMenu.editorErrorResolution;
                        PauseGame(PauseCode::Message);
                    }
                    else if(!GFX.EIcons.inited)
                    {
                        PlaySoundMenu(SFX_BlockHit);
                        MessageText = fmt::format_ne(g_mainMenu.editorErrorMissingResources, "EditorIcons.png");
                        PauseGame(PauseCode::Message);
                    }
                    else if(!EditorCustom::loaded)
                    {
                        PlaySoundMenu(SFX_BlockHit);
                        MessageText = fmt::format_ne(g_mainMenu.editorErrorMissingResources, "editor.ini");
                        PauseGame(PauseCode::Message);
                    }
                    else
                    {
                        PlaySoundMenu(SFX_Do);
                        MenuMode = MENU_EDITOR;
                        MenuCursor = 0;

#ifdef THEXTECH_PRELOAD_LEVELS
                        s_findRecentEpisode();
#elif defined(PGE_NO_THREADING)
                        FindWorlds();
#else
                        SDL_AtomicSet(&loading, 1);
                        loadingThread = SDL_CreateThread(FindWorldsThread, "FindWorlds", nullptr);
                        SDL_DetachThread(loadingThread);
#endif
                    }
                }
                else if(MenuCursor == i++)
                {
                    PlaySoundMenu(SFX_Do);
                    MenuMode = MENU_OPTIONS;
                    MenuCursor = 0;
                }
                else if(MenuCursor == i++)
                {
                    PlaySoundMenu(SFX_Do);
                    XRender::setTargetTexture();
                    XRender::clearBuffer();
                    StopMusic();
                    XRender::repaint();
                    XEvents::doEvents();

                    if(!MaxFPS)
                        PGE_Delay(500);

                    KillIt();
                    return true;
                }

            }


            int quitKeyPos = 2;
            if(!g_gameInfo.disableTwoPlayer)
                quitKeyPos ++;
            if(!g_gameInfo.disableBattleMode)
                quitKeyPos ++;
            if(g_config.enable_editor)
                quitKeyPos ++;

            if(MenuCursor > quitKeyPos)
                MenuCursor = 0;
            if(MenuCursor < 0)
                MenuCursor = quitKeyPos;
        } // Main Menu

        // Character Select
        else if(MenuMode == MENU_CHARACTER_SELECT_NEW ||
                MenuMode == MENU_CHARACTER_SELECT_NEW_BM)
        {
            int ret = ConnectScreen::Logic();
            if(ret == -1)
            {
                if(MenuMode == MENU_CHARACTER_SELECT_NEW_BM)
                {
                    MenuCursor = selWorld - 1;
                    MenuMode = MENU_BATTLE_MODE;
                }
                else
                {
                    int save_offset = selSave - 1;
                    int save_page = save_offset / c_menuSavesPerPage;
                    MenuCursor = save_page * c_menuSavesPageLength + save_offset % c_menuSavesPerPage;

                    if(menuPlayersNum == 1)
                        MenuMode = MENU_SELECT_SLOT_1P;
                    else
                        MenuMode = MENU_SELECT_SLOT_2P;
                }

                MenuCursorCanMove = false;
            }
            else if(ret == 1)
            {
                if(MenuMode == MENU_CHARACTER_SELECT_NEW)
                {
                    MenuCursor = 0;
                    StartEpisode();
                    return true;
                }
                else if(MenuMode == MENU_CHARACTER_SELECT_NEW_BM)
                {
                    MenuCursor = 0;
                    StartBattleMode();
                    return true;
                }
            }
        }
#if 0 // old code, no longer used
        // Character Select
        else if(MenuMode == MENU_CHARACTER_SELECT_1P ||
                MenuMode == MENU_CHARACTER_SELECT_2P_S1 ||
                MenuMode == MENU_CHARACTER_SELECT_2P_S2 ||
                MenuMode == MENU_CHARACTER_SELECT_BM_S1 ||
                MenuMode == MENU_CHARACTER_SELECT_BM_S2)
        {
            if(SharedCursor.Move)
            {
                B = 0;
                For(A, 0, 4)
                {
                    if(blockCharacter[A + 1])
                    {
                        B -= 30;
                    }
                    else
                    {
                        if(SharedCursor.Y >= MenuY + A * 30 + B && SharedCursor.Y <= 366 + A * 30 + B)
                        {
                            if(A >= 0 && A < numCharacters)
                            {
                                menuLen = 18 * g_mainMenu.selectPlayer[A + 1].size();
                                if(A == 0)
                                    menuLen += 2;
                            }
                            else
                            {
                                menuLen = 180;
                            }

                            if(SharedCursor.X >= MenuX && SharedCursor.X <= MenuX + menuLen)
                            {
                                if(MenuMouseRelease && SharedCursor.Primary)
                                    MenuMouseClick = true;
                                if(MenuCursor != A)
                                {
                                    if(
                                        ((MenuMode == MENU_CHARACTER_SELECT_2P_S2 || MenuMode == MENU_CHARACTER_SELECT_BM_S2) && PlayerCharacter - 1 == A) ||
                                        ((blockCharacter[A + 1]) && MenuMouseClick)
                                    )
                                    {
                                        MenuMouseClick = false;
                                    }
                                    else
                                    {
                                        PlaySoundMenu(SFX_Slide);
                                        MenuCursor = A;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if(MenuCursorCanMove || MenuMouseClick)
            {
                if(menuBackPress)
                {
                    if(MenuMode == MENU_CHARACTER_SELECT_2P_S2)
                    {
                        MenuMode = MENU_CHARACTER_SELECT_2P_S1;
                        MenuCursor = PlayerCharacter - 1;
                    }
                    else if(MenuMode == MENU_CHARACTER_SELECT_BM_S2)
                    {
                        MenuMode = MENU_CHARACTER_SELECT_BM_S1;
                        MenuCursor = PlayerCharacter - 1;
                    }
                    else
                    {
                        // MenuCursor = selWorld - 1;
                        MenuMode /= MENU_CHARACTER_SELECT_BASE;
                        // Restore menu state
                        worldCurs = listMenuLastScroll;
                        MenuCursor = listMenuLastCursor;
                    }

                    MenuCursorCanMove = false;
                    PlaySoundMenu(SFX_Slide);
                }
                else if(menuDoPress || MenuMouseClick)
                {
                    PlaySoundMenu(SFX_Do);

                    if(MenuMode == MENU_CHARACTER_SELECT_1P)
                    {
                        PlayerCharacter = MenuCursor + 1;
                        MenuMode = MENU_SELECT_SLOT_1P;
                        MenuCursor = 0;
                    }
                    else if(MenuMode == MENU_CHARACTER_SELECT_2P_S1)
                    {
                        PlayerCharacter = MenuCursor + 1;
                        MenuMode = MENU_CHARACTER_SELECT_2P_S2;
                        MenuCursor = PlayerCharacter2;
                    }
                    else if(MenuMode == MENU_CHARACTER_SELECT_2P_S2)
                    {
                        PlayerCharacter2 = MenuCursor + 1;
                        MenuMode = MENU_SELECT_SLOT_2P;
                        MenuCursor = 0;
                    }
                    else if(MenuMode == MENU_CHARACTER_SELECT_BM_S1)
                    {
                        PlayerCharacter = MenuCursor + 1;
                        MenuMode = MENU_CHARACTER_SELECT_BM_S2;
                        MenuCursor = PlayerCharacter2 - 1;
                        if(MenuCursor < 0)
                            MenuCursor = 0;
                    }
                    else if(MenuMode == MENU_CHARACTER_SELECT_BM_S2)
                    {
                        PlayerCharacter2 = MenuCursor + 1;
                        MenuCursor = 0;
                        StartBattleMode();
                        return true;
                    }

                    MenuCursorCanMove = false;
                }
            }

            bool isListMenu = (MenuMode == MENU_1PLAYER_GAME || MenuMode == MENU_2PLAYER_GAME || MenuMode == MENU_BATTLE_MODE);

            if(MenuMode > MENU_MAIN && !isListMenu)
            {
                if(MenuCursor > numCharacters - 1)
                {
                    MenuCursor = 0;

                    while((MenuCursor == PlayerCharacter - 1 && (MenuMode == MENU_CHARACTER_SELECT_2P_S2 || MenuMode == MENU_CHARACTER_SELECT_BM_S2)) ||
                          blockCharacter[MenuCursor + 1])
                    {
                        MenuCursor += 1;
                    }

                }

                if(MenuCursor < 0)
                {
                    MenuCursor = numCharacters - 1;

                    while((MenuCursor == PlayerCharacter - 1 && (MenuMode == MENU_CHARACTER_SELECT_2P_S2 || MenuMode == MENU_CHARACTER_SELECT_BM_S2)) ||
                          blockCharacter[MenuCursor + 1])
                    {
                        MenuCursor -= 1;
                    }
                }
            }

            if(!isListMenu)
            {
                while(((MenuMode == MENU_CHARACTER_SELECT_2P_S2 || MenuMode == MENU_CHARACTER_SELECT_BM_S2) && MenuCursor == PlayerCharacter - 1) ||
                       blockCharacter[MenuCursor + 1])
                {
                    MenuCursor += 1;
                }
            }

            if(MenuMode >= MENU_CHARACTER_SELECT_BASE && MenuMode <= MENU_CHARACTER_SELECT_BASE_END)
            {
                if(MenuCursor >= numCharacters)
                {
                    MenuCursor = 0;
                }
                else
                {
                    For(A, 1, numPlayers)
                    {
                        Player[A].Character = MenuCursor + 1;
                        SizeCheck(A);
                    }

                    For(A, 1, numNPCs)
                    {
                        if(NPC[A].Type == NPCID_PLR_FIREBALL)
                            NPC[A].Special = MenuCursor + 1;
                    }
                }
            }
        } // Character Select
#endif

        // World Select
        else if(MenuMode == MENU_1PLAYER_GAME || MenuMode == MENU_2PLAYER_GAME
            || MenuMode == MENU_BATTLE_MODE || MenuMode == MENU_EDITOR)
        {
            const std::vector<SelectWorld_t>& SelectorList
                = (MenuMode == MENU_BATTLE_MODE) ? SelectBattle :
                    ((MenuMode == MENU_EDITOR) ? SelectWorldEditable :
                        SelectWorld);

            if(ScrollDelay > 0)
            {
                SharedCursor.Move = true;
                ScrollDelay -= 1;
            }

            if(SharedCursor.Move)
            {
                B = 0;

                For(A, minShow - 1, maxShow - 1)
                {
                    if(SharedCursor.Y >= MenuY + B * 30 && SharedCursor.Y <= MenuY + B * 30 + 16)
                    {
                        menuLen = 19 * static_cast<int>(SelectorList[A + 1].WorldName.size());

                        if(SharedCursor.X >= MenuX && SharedCursor.X <= MenuX + menuLen)
                        {
                            if(MenuMouseRelease && SharedCursor.Primary)
                                MenuMouseClick = true;

                            if(MenuCursor != A && ScrollDelay == 0)
                            {
                                ScrollDelay = 10;
                                PlaySoundMenu(SFX_Slide);
                                MenuCursor = A;
                            }
                        }
                    }

                    B += 1;
                }
            }

            if(MenuCursorCanMove || MenuMouseClick)
            {
                if(menuBackPress)
                {
                    MenuCursor = MenuMode - 1;

                    if(MenuMode == MENU_BATTLE_MODE)
                    {
                        MenuCursor = g_gameInfo.disableTwoPlayer ? 1 : 2;
                    }
                    else if(MenuMode == MENU_EDITOR)
                    {
                        MenuCursor = 3;
                        if(g_gameInfo.disableTwoPlayer)
                            MenuCursor--;
                        if(g_gameInfo.disableBattleMode)
                            MenuCursor--;
                    }

                    MenuMode = MENU_MAIN;
//'world select back

                    PlaySoundMenu(SFX_Slide);
                    MenuCursorCanMove = false;
                }
                else if(menuDoPress || MenuMouseClick)
                {
                    bool disabled = false;
                    // Save menu state
                    listMenuLastScroll = worldCurs;
                    listMenuLastCursor = MenuCursor;

                    selWorld = MenuCursor + 1;

                    if((MenuMode == MENU_BATTLE_MODE && SelectBattle[selWorld].disabled) ||
                       ((MenuMode == MENU_1PLAYER_GAME || MenuMode == MENU_2PLAYER_GAME) && SelectWorld[selWorld].disabled))
                        disabled = true;

                    if(!disabled)
                        PlaySoundMenu(SFX_Do);

                    if(disabled)
                    {
                        PlaySoundMenu(SFX_BlockHit);
                        // Do nothing. stay at menu
                    }
                    // level editor
                    else if(MenuMode == MENU_EDITOR)
                    {
                        if(selWorld == NumSelectWorldEditable)
                        {
                            ClearWorld(true);
                            WorldName = TextEntryScreen::Run(g_mainMenu.editorPromptNewWorldName);
                            if(!WorldName.empty())
                            {
                                std::string fn = WorldName;
                                // eliminate bad characters
                                std::replace(fn.begin(), fn.end(), '/', '_');
                                std::replace(fn.begin(), fn.end(), '\\', '_');
                                std::replace(fn.begin(), fn.end(), '.', '_');
                                std::replace(fn.begin(), fn.end(), ':', '_');
                                std::replace(fn.begin(), fn.end(), '<', '_');
                                std::replace(fn.begin(), fn.end(), '>', '_');
                                std::replace(fn.begin(), fn.end(), '"', '_');
                                std::replace(fn.begin(), fn.end(), '|', '_');
                                std::replace(fn.begin(), fn.end(), '?', '_');
                                std::replace(fn.begin(), fn.end(), '*', '_');
                                // ensure uniqueness (but still case-sensitive for now)
                                while(DirMan::exists(AppPathManager::userWorldsRootDir() + fn))
                                    fn += "2";
                                DirMan::mkAbsPath(AppPathManager::userWorldsRootDir() + fn);

                                std::string wPath = AppPathManager::userWorldsRootDir() + fn + "/world.wld";
                                if(g_config.editor_preferred_file_format != FileFormats::WLD_SMBX64 && g_config.editor_preferred_file_format != FileFormats::WLD_SMBX38A)
                                    wPath += "x";
                                g_recentWorldEditor = wPath;

                                SaveWorld(wPath, g_config.editor_preferred_file_format);

#ifdef PGE_NO_THREADING
                                FindWorlds();
#else
                                SDL_AtomicSet(&loading, 1);
                                loadingThread = SDL_CreateThread(FindWorldsThread, "FindWorlds", NULL);
#endif
                            }
                        }
                        else if(selWorld == NumSelectWorldEditable - 1)
                        {
                            ClearWorld(true);
                            GameMenu = false;
                            LevelSelect = false;
                            BattleMode = true;
                            LevelEditor = true;
                            WorldEditor = false;
                            ClearLevel();
                            ClearGame();
                            SetupPlayers();

                            std::string lPath = AppPathManager::userBattleRootDir();
                            {
                                // find a single battle level to open first
                                std::vector<std::string> files;
                                DirMan battleLvls(lPath);
                                battleLvls.getListOfFiles(files, {".lvl", ".lvlx"});
                                if(files.empty())
                                    lPath += "newbattle.lvl";
                                else
                                    lPath += files[0];
                            }

                            if(!Files::fileExists(lPath))
                            {
                                if(g_config.editor_preferred_file_format != FileFormats::WLD_SMBX64 && g_config.editor_preferred_file_format != FileFormats::WLD_SMBX38A)
                                    lPath += "x";
                                SaveLevel(lPath, g_config.editor_preferred_file_format);
                            }

                            OpenLevel(lPath);
                            EditorBackup(); // EditorRestore() gets called when not in world editor

                            if(g_recentWorldEditor != "battle")
                            {
                                g_recentWorldEditor = "battle";
                                SaveConfig();
                            }

                            Integrator::setEditorFile(FileName);
                            editorScreen.ResetCursor();
                            editorScreen.active = false;
                            MouseRelease = false;
                            return true;
                        }
                        else
                        {
                            GameMenu = false;
                            LevelSelect = false;
                            BattleMode = false;
                            LevelEditor = true;
                            WorldEditor = true;
                            ClearLevel();
                            ClearGame();
                            SetupPlayers();
                            std::string wPath = SelectWorldEditable[selWorld].WorldPath
                                + SelectWorldEditable[selWorld].WorldFile;
                            OpenWorld(wPath);
                            if(g_recentWorldEditor != wPath)
                            {
                                g_recentWorldEditor = wPath;
                                SaveConfig();
                            }
                            Integrator::setEditorFile(FileName);
                            editorScreen.ResetCursor();
                            editorScreen.active = false;
                            MouseRelease = false;
                            return true;
                        }
                    }
                    // game mode
                    else
                    {
                        if(MenuMode != MENU_BATTLE_MODE)
                            FindSaves();

                        For(A, 1, numCharacters)
                        {
                            if(MenuMode == MENU_BATTLE_MODE)
                                blockCharacter[A] = false;
                            else
                                blockCharacter[A] = SelectWorld[selWorld].blockChar[A];
                        }

                        if(MenuMode == MENU_BATTLE_MODE)
                        {
                            MenuMode = MENU_CHARACTER_SELECT_NEW_BM;
                            ConnectScreen::MainMenu_Start(2);
                        }
                        else
                        {
                            MenuMode *= MENU_SELECT_SLOT_BASE;
                            MenuCursor = 0;
                        }
                    }

                    MenuCursorCanMove = false;
                }

            }

            // New world select scroll options!
            // Based on Wohlstand's but somewhat simpler and less keyboard-specific.
            // Left and right are -/+ 3 (repeatable, so they also provide a quick-first/quick-last function).
            // DropItem / Tertiary cursor button is return to last episode.
            bool dontWrap = false;

            if(leftPressed && (MenuCursorCanMove || ScrollDelay == 0))
            {
                PlaySoundMenu(SFX_Saw);
                MenuCursor -= 3;
                MenuCursorCanMove = false;
                ScrollDelay = 15;
                dontWrap = true;
            }
            else if(rightPressed && (MenuCursorCanMove || ScrollDelay == 0))
            {
                PlaySoundMenu(SFX_Saw);
                MenuCursor += 3;
                MenuCursorCanMove = false;
                ScrollDelay = 15;
                dontWrap = true;
            }
            else if((leftPressed || rightPressed) && !SharedCursor.Move)
            {
                ScrollDelay -= 1;
            }
            else if(SharedCursor.ScrollUp)
            {
                PlaySoundMenu(SFX_Saw);
                MenuCursor -= 1;
                dontWrap = true;
            }
            else if(SharedCursor.ScrollDown)
            {
                PlaySoundMenu(SFX_Saw);
                MenuCursor += 1;
                dontWrap = true;
            }

            if(homePressed && MenuCursorCanMove && menuRecentEpisode >= 0)
            {
                PlaySoundMenu(SFX_Camera);
                MenuCursor = menuRecentEpisode;
                MenuCursorCanMove = false;
                dontWrap = true;
            }

            if(MenuMode == MENU_1PLAYER_GAME || MenuMode == MENU_2PLAYER_GAME
                || MenuMode == MENU_BATTLE_MODE || MenuMode == MENU_EDITOR)
            {
                maxShow = (MenuMode == MENU_BATTLE_MODE) ? NumSelectBattle :
                    ((MenuMode == MENU_EDITOR) ? NumSelectWorldEditable :
                        NumSelectWorld);

                if(dontWrap)
                {
                    if(MenuCursor >= maxShow)
                        MenuCursor = maxShow - 1;
                    if(MenuCursor < 0)
                        MenuCursor = 0;
                }
                else
                {
                    if(MenuCursor >= maxShow)
                        MenuCursor = 0;
                    if(MenuCursor < 0)
                        MenuCursor = maxShow - 1;
                }
            }
        } // World select

        // Save Select
        else if(MenuMode == MENU_SELECT_SLOT_1P || MenuMode == MENU_SELECT_SLOT_2P)
        {
            if(SharedCursor.Move)
            {
                int old_cursor = MenuCursor;

                int page_offset = s_GetSavesPageMenuOffset();
                MenuCursor -= page_offset;

                SoundPause[SFX_Slide] += 1;
                s_handleMouseMove(c_menuSavesPageLength - 1, MenuX, MenuY, 300, 30);
                SoundPause[SFX_Slide] -= 1;

                MenuCursor += page_offset;

                if(s_GetSavesActionIdx(false) > maxSaveSlots)
                    MenuCursor = old_cursor;
                else if(MenuCursor != old_cursor)
                    PlaySoundMenu(SFX_Slide);
            }

            if(MenuCursorCanMove || MenuMouseClick)
            {
                if(menuBackPress)
                {
//'save select back
                    MenuMode /= MENU_SELECT_SLOT_BASE;
                    // Restore menu state
                    worldCurs = listMenuLastScroll;
                    MenuCursor = listMenuLastCursor;

                    MenuCursorCanMove = false;
                    PlaySoundMenu(SFX_Slide);
                }
                else if(menuDoPress || MenuMouseClick)
                {
                    PlaySoundMenu(SFX_Do);

                    int action = s_GetSavesActionIdx();

                    if(action > 0) // Select the save slot, but still need to select players
                    {
                        selSave = action;
                        if(MenuMode == MENU_SELECT_SLOT_2P)
                            ConnectScreen::MainMenu_Start(2);
                        else
                            ConnectScreen::MainMenu_Start(1);
                        MenuMode = MENU_CHARACTER_SELECT_NEW;
                        MenuCursorCanMove = false;
                    }
                    else if(action == c_menuItemSavesCopy) // Copy the gamesave
                    {
                        MenuCursor = s_GetSavesPageMenuOffset();
                        MenuMode += MENU_SELECT_SLOT_COPY_S1_ADD;
                        MenuCursorCanMove = false;
                    }
                    else if(action == c_menuItemSavesDelete) // Delete the gamesave
                    {
                        MenuCursor = s_GetSavesPageMenuOffset();
                        MenuMode += MENU_SELECT_SLOT_DELETE_ADD;
                        MenuCursorCanMove = false;
                    }
                }
            }

            if(MenuMode == MENU_SELECT_SLOT_1P || MenuMode == MENU_SELECT_SLOT_2P)
            {
                if(MenuCursor > c_menuItemSavesEndList) MenuCursor = 0;
                if(MenuCursor < 0) MenuCursor = c_menuItemSavesEndList;
            }
        } // Save Slot Select

        // Save Select
        else if(MenuMode == MENU_SELECT_SLOT_1P_COPY_S1 || MenuMode == MENU_SELECT_SLOT_2P_COPY_S1 ||
                MenuMode == MENU_SELECT_SLOT_1P_COPY_S2 || MenuMode == MENU_SELECT_SLOT_2P_COPY_S2)
        {
            if(SharedCursor.Move)
            {
                int page_offset = s_GetSavesPageMenuOffset();
                MenuCursor -= page_offset;

                int item_bound = c_menuSavesPerPage;
                if(page_offset == c_menuSavesPageLength * (c_menuSavesPageCount - 1))
                    item_bound = SDL_min(item_bound, maxSaveSlots % c_menuSavesPerPage);

                s_handleMouseMove(item_bound - 1, MenuX, MenuY, 300, 30);
                MenuCursor += page_offset;
            }

            if(MenuCursorCanMove || MenuMouseClick)
            {
                if(menuBackPress)
                {
//'save select back
                    if(MenuMode == MENU_SELECT_SLOT_1P_COPY_S2 || MenuMode == MENU_SELECT_SLOT_2P_COPY_S2)
                    {
                        MenuMode -= MENU_SELECT_SLOT_COPY_S1_ADD;
                    }
                    else
                    {
                        MenuMode -= MENU_SELECT_SLOT_COPY_S1_ADD;
                        MenuCursor = s_GetSavesPageMenuOffset() + (c_menuSavesPageLength + c_menuItemSavesCopy);
                    }

                    MenuCursorCanMove = false;
                    PlaySoundMenu(SFX_Do);
                }
                else if(menuDoPress || MenuMouseClick)
                {
                    int slot = s_GetSavesActionIdx();

                    SDL_assert_release(IF_INRANGE(slot, 1, maxSaveSlots));

                    if(MenuMode == MENU_SELECT_SLOT_1P_COPY_S1 || MenuMode == MENU_SELECT_SLOT_2P_COPY_S1)
                    {
                        if(SaveSlotInfo[slot].Progress < 0)
                            PlaySoundMenu(SFX_BlockHit);
                        else
                        {
                            PlaySoundMenu(SFX_Do);
                            menuCopySaveSrc = slot;
                            MenuMode += MENU_SELECT_SLOT_COPY_S1_ADD;
                        }

                        MenuCursorCanMove = false;
                    }
                    else if(menuCopySaveSrc == slot)
                    {
                        PlaySoundMenu(SFX_BlockHit);
                        MenuCursorCanMove = false;
                    }
                    else
                    {
                        PlaySoundMenu(SFX_Transform);
                        menuCopySaveDst = slot;
                        CopySave(selWorld, menuCopySaveSrc, menuCopySaveDst);
                        FindSaves();
                        MenuMode -= MENU_SELECT_SLOT_COPY_S2_ADD;
                        MenuCursor = s_GetSavesPageMenuOffset() + (c_menuSavesPageLength + c_menuItemSavesCopy);
                        MenuCursorCanMove = false;
                    }
                }
            }

            if(MenuMode == MENU_SELECT_SLOT_1P_COPY_S1 || MenuMode == MENU_SELECT_SLOT_2P_COPY_S1 ||
               MenuMode == MENU_SELECT_SLOT_1P_COPY_S2 || MenuMode == MENU_SELECT_SLOT_2P_COPY_S2)
            {
                if(MenuCursor > c_menuItemSavesEndList) MenuCursor = 0;
                if(MenuCursor < 0) MenuCursor = c_menuItemSavesEndList;
            }
        }

        // Delete gamesave
        else if(MenuMode == MENU_SELECT_SLOT_1P_DELETE || MenuMode == MENU_SELECT_SLOT_2P_DELETE)
        {
            if(SharedCursor.Move)
            {
                int page_offset = s_GetSavesPageMenuOffset();
                MenuCursor -= page_offset;

                int item_bound = c_menuSavesPerPage;
                if(page_offset == c_menuSavesPageLength * (c_menuSavesPageCount - 1))
                    item_bound = SDL_min(item_bound, maxSaveSlots % c_menuSavesPerPage);

                s_handleMouseMove(item_bound - 1, MenuX, MenuY, 300, 30);
                MenuCursor += page_offset;
            }

            if(MenuCursorCanMove || MenuMouseClick)
            {
                if(menuBackPress)
                {
//'save select back
                    MenuMode -= MENU_SELECT_SLOT_DELETE_ADD;
                    MenuCursor = s_GetSavesPageMenuOffset() + (c_menuSavesPageLength + c_menuItemSavesDelete);

                    PlaySoundMenu(SFX_Do);
                    MenuCursorCanMove = false;
                }
                else if(menuDoPress || MenuMouseClick)
                {
                    int slot = s_GetSavesActionIdx();

                    SDL_assert_release(IF_INRANGE(slot, 1, maxSaveSlots));

                    DeleteSave(selWorld, slot);
                    FindSaves();

                    MenuMode -= MENU_SELECT_SLOT_DELETE_ADD;
                    MenuCursor = s_GetSavesPageMenuOffset() + (c_menuSavesPageLength + c_menuItemSavesDelete);

                    PlaySoundMenu(SFX_LavaMonster);
                    MenuCursorCanMove = false;
                }
            }

            if(MenuMode == MENU_SELECT_SLOT_1P_DELETE || MenuMode == MENU_SELECT_SLOT_2P_DELETE)
            {
                if(MenuCursor > c_menuItemSavesEndList) MenuCursor = 0;
                if(MenuCursor < 0) MenuCursor = c_menuItemSavesEndList;
            }
        }

        // Options
        else if(MenuMode == MENU_OPTIONS)
        {
            int optionsMenuLength = 2; // controls, language, credits
#ifndef RENDER_FULLSCREEN_ALWAYS
            optionsMenuLength++; // FullScreen
#endif
#ifndef RENDER_CUSTOM
            optionsMenuLength++; // Renderer
#endif
            optionsMenuLength ++; // resolution
            optionsMenuLength ++; // ScaleMode

            if(SharedCursor.Move)
            {
                For(A, 0, optionsMenuLength)
                {
                    if(SharedCursor.Y >= MenuY + A * 30 && SharedCursor.Y <= MenuY + A * 30 + 16)
                    {
                        int i = 0;
                        if(A == i++)
                            menuLen = 18 * (int)g_mainMenu.controlsTitle.size();
#ifndef RENDER_FULLSCREEN_ALWAYS
                        else if(A == i++)
                        {
                            if(resChanged)
                                menuLen = 18 * 13; // std::strlen("windowed mode")
                            else
                                menuLen = 18 * 15; // std::strlen("fullscreen mode")
                        }
#endif
#ifndef RENDER_CUSTOM
                        else if(A == i++)
                            menuLen = 18 * 25; // Render Mode: XXXXXXXX
#endif
                        else if(A == i++)
                            menuLen = 18 * (7 + (int)ScaleMode_strings.at(g_videoSettings.scaleMode).length());
                        else if(A == i++)
                            menuLen = 18 * std::strlen("res: WWWxHHH (word)");
                        else if(A == i++)
                            menuLen = 18 * 25; // Language: XXXXX (YY)
                        else
                            menuLen = 18 * 12 - 2; // std::strlen("view credits")

                        if(SharedCursor.X >= MenuX && SharedCursor.X <= MenuX + menuLen)
                        {
                            if(MenuMouseRelease && SharedCursor.Primary)
                                MenuMouseClick = true;

                            if(MenuCursor != A)
                            {
                                PlaySoundMenu(SFX_Slide);
                                MenuCursor = A;
                            }
                        }
                    }
                }
            }

            if(MenuCursorCanMove || MenuMouseClick)
            {
                if(menuBackPress)
                {
                    SaveConfig();

                    int optionsIndex = 1;
                    if(!g_gameInfo.disableTwoPlayer)
                        optionsIndex++;
                    if(!g_gameInfo.disableBattleMode)
                        optionsIndex++;
                    if(g_config.enable_editor)
                        optionsIndex++;
                    MenuMode = MENU_MAIN;
                    MenuCursor = optionsIndex;
                    MenuCursorCanMove = false;
                    PlaySoundMenu(SFX_Slide);
                }
                else if(menuDoPress || MenuMouseClick || leftPressed || rightPressed)
                {
                    MenuCursorCanMove = false;
                    int i = 0;
                    if(MenuCursor == i++ && (menuDoPress || MenuMouseClick))
                    {
                        MenuCursor = 0;
                        MenuMode = MENU_INPUT_SETTINGS;
                        PlaySoundMenu(SFX_Do);
                    }
#ifndef RENDER_FULLSCREEN_ALWAYS // on Android and some other platforms, run the always full-screen
                    else if(MenuCursor == i++)
                    {
                        PlaySoundMenu(SFX_Do);
                        ChangeScreen();
                    }
#endif
#ifndef RENDER_CUSTOM
                    else if(MenuCursor == i++)
                    {
                        int delta = leftPressed ? -1 : 1;

                        bool first = true;

                        // check for and skip unsupported modes
                        while(first
#   ifndef THEXTECH_BUILD_GL_DESKTOP_MODERN
                            || g_videoSettings.renderMode == RENDER_ACCELERATED_OPENGL
#   endif
#   ifndef THEXTECH_BUILD_GL_DESKTOP_LEGACY
                            || g_videoSettings.renderMode == RENDER_ACCELERATED_OPENGL_LEGACY
#   endif
#   ifndef THEXTECH_BUILD_GL_ES_MODERN
                            || g_videoSettings.renderMode == RENDER_ACCELERATED_OPENGL_ES
#   endif
#   ifndef THEXTECH_BUILD_GL_ES_LEGACY
                            || g_videoSettings.renderMode == RENDER_ACCELERATED_OPENGL_ES_LEGACY
#   endif
                        )
                        {
                            g_videoSettings.renderMode += delta;
                            if(g_videoSettings.renderMode < RENDER_SOFTWARE)
                                g_videoSettings.renderMode = RENDER_END - 1;
                            else if(g_videoSettings.renderMode >= RENDER_END)
                                g_videoSettings.renderMode = RENDER_SOFTWARE;

                            first = false;
                        }

                        bool res = g_frmMain.restartRenderer();
                        PlaySoundMenu(SFX_PSwitch);
                        SaveConfig();

                        if(!res)
                        {
                            MessageText = g_mainMenu.optionsRestartEngine;
                            PauseGame(PauseCode::Message);
                        }
                    }
#endif // #ifndef RENDER_CUSTOM
                    else if(MenuCursor == i++)
                    {
                        PlaySoundMenu(SFX_Do);
                        if(!leftPressed)
                            g_videoSettings.scaleMode = g_videoSettings.scaleMode + 1;
                        else
                            g_videoSettings.scaleMode = g_videoSettings.scaleMode - 1;
                        if(g_videoSettings.scaleMode > SCALE_FIXED_2X)
                            g_videoSettings.scaleMode = SCALE_DYNAMIC_INTEGER;
                        if(g_videoSettings.scaleMode < SCALE_DYNAMIC_INTEGER)
                            g_videoSettings.scaleMode = SCALE_FIXED_2X;
                        UpdateWindowRes();
                        UpdateInternalRes();
                    }
                    else if(MenuCursor == i++)
                    {
                        PlaySoundMenu(SFX_Do);
                        if(!leftPressed)
                        {
                            if (g_config.InternalW == 0 && g_config.InternalH == 0)
                                { g_config.InternalW = 480; g_config.InternalH = 320; }
                            else if (g_config.InternalW == 480 && g_config.InternalH == 320)
                                { g_config.InternalW = 512; g_config.InternalH = 384; }
                            else if (g_config.InternalW == 512 && g_config.InternalH == 384)
                                { g_config.InternalW = 512; g_config.InternalH = 448; }
                            else if (g_config.InternalW == 512 && g_config.InternalH == 448)
                                { g_config.InternalW = 768; g_config.InternalH = 432; }
                            else if (g_config.InternalW == 768 && g_config.InternalH == 432)
                                { g_config.InternalW = 640; g_config.InternalH = 480; }
                            else if (g_config.InternalW == 640 && g_config.InternalH == 480)
                                { g_config.InternalW = 800; g_config.InternalH = 480; }
                            else if (g_config.InternalW == 800 && g_config.InternalH == 480)
                                { g_config.InternalW = 800; g_config.InternalH = 600; }
                            else if (g_config.InternalW == 800 && g_config.InternalH == 600)
                                { g_config.InternalW = 1280; g_config.InternalH = 720; }
                            else if (g_config.InternalW == 1280 && g_config.InternalH == 720)
                                { g_config.InternalW = 0; g_config.InternalH = 600; }
                            else if (g_config.InternalW == 0 && g_config.InternalH == 600)
                                { g_config.InternalW = 0; g_config.InternalH = 0; }
                            else
                                { g_config.InternalW = 0; g_config.InternalH = 0; }
                        }
                        else
                        {
                            if (g_config.InternalW == 0 && g_config.InternalH == 0)
                                { g_config.InternalW = 0; g_config.InternalH = 600; }
                            else if (g_config.InternalW == 480 && g_config.InternalH == 320)
                                { g_config.InternalW = 0; g_config.InternalH = 0; }
                            else if (g_config.InternalW == 512 && g_config.InternalH == 384)
                                { g_config.InternalW = 480; g_config.InternalH = 320; }
                            else if (g_config.InternalW == 512 && g_config.InternalH == 448)
                                { g_config.InternalW = 512; g_config.InternalH = 384; }
                            else if (g_config.InternalW == 640 && g_config.InternalH == 480)
                                { g_config.InternalW = 768; g_config.InternalH = 432; }
                            else if (g_config.InternalW == 768 && g_config.InternalH == 432)
                                { g_config.InternalW = 512; g_config.InternalH = 448; }
                            else if (g_config.InternalW == 800 && g_config.InternalH == 480)
                                { g_config.InternalW = 640; g_config.InternalH = 480; }
                            else if (g_config.InternalW == 800 && g_config.InternalH == 600)
                                { g_config.InternalW = 800; g_config.InternalH = 480; }
                            else if (g_config.InternalW == 1280 && g_config.InternalH == 720)
                                { g_config.InternalW = 800; g_config.InternalH = 600; }
                            else if (g_config.InternalW == 0 && g_config.InternalH == 600)
                                { g_config.InternalW = 1280; g_config.InternalH = 720; }
                            else
                                { g_config.InternalW = 0; g_config.InternalH = 0; }
                        }
                        UpdateWindowRes();
                        UpdateInternalRes();
                    }
                    else if(MenuCursor == i++)
                    {
                        if(!FontManager::isInitied() || FontManager::isLegacy())
                            PlaySoundMenu(SFX_BlockHit);
                        else
                        {
                            XLanguage::rotateLanguage(g_config.language, leftPressed ? -1 : 1);
                            ReloadTranslations();
                            PlaySoundMenu(SFX_Climbing);
                        }
                    }
                    else if(MenuCursor == i++ && (menuDoPress || MenuMouseClick))
                    {
                        PlaySoundMenu(SFX_Do);
                        GameMenu = false;
                        GameOutro = true;
                        CreditChop = ScreenH / 2;
                        EndCredits = 0;
                        SetupCredits();
                    }

                }

            }

            if(MenuMode == MENU_OPTIONS)
            {
                if(MenuCursor > optionsMenuLength)
                    MenuCursor = 0;
                if(MenuCursor < 0)
                    MenuCursor = optionsMenuLength;
            }
        }

        // Input Settings
        else if(MenuMode == MENU_INPUT_SETTINGS)
        {
            int ret = menuControls_Logic();
            if(ret == -1)
            {
                SaveConfig();
                MenuCursor = 0; // index of controls within options
                MenuMode = MENU_OPTIONS;
                MenuCursorCanMove = false;
            }
        }
    }

#if 0 // unused now
//'check for all characters blocked
    if(MenuMode == MENU_CHARACTER_SELECT_1P || MenuMode == MENU_CHARACTER_SELECT_2P_S1 || MenuMode == MENU_CHARACTER_SELECT_2P_S2)
    {
        AllCharBlock = 0;

        For(A, 1, numCharacters)
        {
            if(!blockCharacter[A])
            {
                if(AllCharBlock == 0)
                {
                    AllCharBlock = A;
                }
                else
                {
                    AllCharBlock = 0;
                    break;
                }
            }
        }

        if(AllCharBlock > 0)
        {
            PlayerCharacter = AllCharBlock;
            PlayerCharacter2 = AllCharBlock;

            if(MenuMode == MENU_CHARACTER_SELECT_1P)
            {
                MenuMode = MENU_SELECT_SLOT_1P;
                MenuCursor = 0;
            }
            else if(MenuMode == MENU_CHARACTER_SELECT_2P_S1)
            {
                MenuMode = MENU_CHARACTER_SELECT_2P_S2;
                MenuCursor = PlayerCharacter2;
            }
            else
            {
                MenuMode = MENU_SELECT_SLOT_2P;
                MenuCursor = 0;
            }
        }
    }
#endif

    return false;
}

static constexpr int find_in_string(const char* haystack, const char* haystack_start, char needle)
{
    return (*haystack == '\0') ? -1 :
        ((*haystack == needle) ? haystack - haystack_start : find_in_string(haystack + 1, haystack_start, needle));
}

static constexpr bool in_string(const char* haystack, char needle)
{
    return find_in_string(haystack, haystack, needle) != -1;
}

static constexpr bool str_prefix(const char* string, const char* prefix)
{
    return (*prefix == '\0' || (*string == *prefix && str_prefix(string + 1, prefix + 1)));
}

static constexpr int find_in_string(const char* haystack, char needle)
{
    return find_in_string(haystack, haystack, needle);
}

static void s_drawGameVersion()
{
    constexpr bool is_release = !in_string(V_LATEST_STABLE, '-');
    constexpr bool is_main = str_prefix(V_BUILD_BRANCH, "main");
    constexpr bool is_stable = str_prefix(V_BUILD_BRANCH, "stable");
    constexpr bool is_wip = str_prefix(V_BUILD_BRANCH, "wip-");

    constexpr bool is_dirty = in_string(V_BUILD_VER, '-');

    constexpr bool show_branch = (!is_main && (is_release || !is_stable));
    constexpr bool show_commit = (!is_release || (!is_main && !is_stable));

    // show version
    SuperPrintRightAlign("v" V_LATEST_STABLE, 5, ScreenW - 2, 2);

    // show branch
    if(show_branch)
    {
        int y = show_commit ? ScreenH - 36 : ScreenH - 18;

        if(is_wip)
        {
            // strip the "wip-"
            SuperPrintRightAlign(&V_BUILD_BRANCH[find_in_string(V_BUILD_BRANCH, '-') + 1], 5, ScreenW - 2, y);
        }
        else
            SuperPrintRightAlign(V_BUILD_BRANCH, 5, ScreenW - 2, y);
    }

    // show git commit
    if(show_commit)
    {
        if(is_dirty)
        {
            // only show -d, not -dirty
            SuperPrintRightAlign(find_in_string(V_BUILD_VER, '-') + 2 + 1, "#" V_BUILD_VER, 5, ScreenW - 2, ScreenH - 18);
        }
        else
            SuperPrintRightAlign("#" V_BUILD_VER, 5, ScreenW - 2, ScreenH - 18);
    }
}

static void s_drawGameTypeTitle(int x, int y)
{
    if(MenuMode == MENU_EDITOR)
        SuperPrint(g_mainMenu.mainEditor, 3, x, y, XTColorF(0.8f, 0.8f, 0.3f));
    else if(menuBattleMode)
        SuperPrint(g_mainMenu.mainBattleGame, 3, x, y, XTColorF(0.3f, 0.3f, 1.0f));
    else
    {
        float r = menuPlayersNum == 1 ? 1.f : 0.3f;
        float g = menuPlayersNum == 2 ? 1.f : 0.3f;

        if(menuPlayersNum == 1)
            SuperPrint(g_mainMenu.main1PlayerGame, 3, x, y, XTColorF(r, g, 0.3f));
        else
            SuperPrint(g_mainMenu.mainMultiplayerGame, 3, x, y, XTColorF(r, g, 0.3f));
    }
}

static void s_drawGameSaves(int MenuX, int MenuY)
{
    int page_count = s_GetCurrentSavesPageCount();
    int current_page = MenuCursor / c_menuSavesPageLength;

    // draw page indicator
    if(page_count > 1)
    {
        int lX_base = MenuX + 8;
        int cY = MenuY + (c_menuSavesPerPage * 30) - 6;

        for(int i = 0; i < page_count; i++)
        {
            int lX = lX_base + i * 64;
            int h = (current_page == i) ? 8 : 6;
            int w = 56;
            uint8_t alpha = (current_page == i) ? 255 : 191;
            XTColor fill = (current_page == i) ? XTColorF(0.6f, 1.f, 1.f).with_alpha(alpha) : XTColor(255, 255, 255, alpha);
            XRender::renderRect(lX,     cY - h / 2,     w,     h,     {0, 0, 0, alpha});
            XRender::renderRect(lX + 2, cY - h / 2 + 2, w - 4, h - 4, fill);
        }
    }

    // draw saves
    int page_offset = s_GetSavesPageSaveOffset();
    for(int A = 1; A <= c_menuSavesPerPage && page_offset + A <= maxSaveSlots; A++)
    {
        int posY = MenuY - 30 + (A * 30);

        const auto& slot_info = SaveSlotInfo[page_offset + A];

        if(slot_info.Progress >= 0)
        {
            // "SLOT {0} ... {1}%"
            std::string p = fmt::format_ne(g_mainMenu.gameSlotContinue, page_offset + A, slot_info.Progress);
            int len = SuperTextPixLen(p, 3);

            SuperPrint(p, 3, MenuX, posY);

            if(slot_info.Stars > 0)
            {
                len += 4;
                XRender::renderTexture(MenuX + len, posY + 1,
                                      GFX.Interface[5].w, GFX.Interface[5].h,
                                      GFX.Interface[5], 0, 0);

                len += GFX.Interface[5].w + 4;
                XRender::renderTexture(MenuX + len, posY + 2,
                                      GFX.Interface[1].w, GFX.Interface[1].h,
                                      GFX.Interface[1], 0, 0);

                len += GFX.Interface[1].w + 4;
                SuperPrint(fmt::format_ne("{0}", slot_info.Stars), 3, MenuX + len, posY);
            }
        }
        else
        {
            // "SLOT {0} ... NEW GAME"
            SuperPrint(fmt::format_ne(g_mainMenu.gameSlotNew, page_offset + A), 3, MenuX, posY);
        }
    }

    int A = c_menuSavesPerPage + 1;

    if(MenuMode == MENU_SELECT_SLOT_1P || MenuMode == MENU_SELECT_SLOT_2P)
    {
        SuperPrint(g_mainMenu.gameCopySave, 3, MenuX, MenuY - 30 + (A * 30));
        A++;
        SuperPrint(g_mainMenu.gameEraseSave, 3, MenuX, MenuY - 30 + (A * 30));
    }

    int save_slot = s_GetSavesActionIdx();

    if(save_slot < 1 || save_slot > maxSaveSlots || (MenuMode != MENU_SELECT_SLOT_1P && MenuMode != MENU_SELECT_SLOT_2P) || SaveSlotInfo[save_slot].Progress < 0)
        return;

    // draw save slot info
    const auto& info = SaveSlotInfo[save_slot];

    int infobox_x = ScreenW / 2 - 240;
    int infobox_y = MenuY + 145;

    int row_1 = infobox_y + 10;
    int row_2 = infobox_y + 42;
    int row_c = infobox_y + 26;

    bool hasFails = (gEnableDemoCounter || info.FailsEnabled);

    // recenter if single row
    if((info.Time <= 0 || g_speedRunnerMode == SPEEDRUN_MODE_OFF) && !hasFails)
        row_1 = infobox_y + 26;

    XRender::renderRect(infobox_x, infobox_y, 480, 68, {0, 0, 0, 127});

    // Temp string
    std::string t;

    // Score
    bool show_timer = info.Time > 0 && g_speedRunnerMode != SPEEDRUN_MODE_OFF;
    int row_score = show_timer ? row_1 : row_c;
    SuperPrint(t = fmt::format_ne(g_mainMenu.phraseScore, info.Score), 3, infobox_x + 10, row_score);

    // Gameplay Timer
    if(show_timer)
    {
        std::string ts = GameplayTimer::formatTime(info.Time);

        if(ts.size() > 9)
            ts = ts.substr(0, ts.size() - 4);

        SuperPrint(fmt::format_ne(g_mainMenu.phraseTime, ts), 3, infobox_x + 10, row_2);
    }

    // If demos off, put (l)ives and (c)oins on center
    int row_lc = (hasFails) ? row_1 : row_c;

    // Print lives on the screen (from gfx_update2.cpp)
    XRender::renderTexture(infobox_x + 272, row_lc + 2 + 14 - GFX.Interface[3].h, GFX.Interface[3]);
    XRender::renderTexture(infobox_x + 272 + 40, row_lc + 2 + 16 - GFX.Interface[3].h, GFX.Interface[1]);
    SuperPrint(t = std::to_string(info.Lives), 1, infobox_x + 272 + 62, row_lc + 2);

    // Print coins on the screen (from gfx_update2.cpp)
    int coins_x = infobox_x + 480 - 10 - 36 - 62;
    XRender::renderTexture(coins_x + 16, row_lc, GFX.Interface[2]);
    XRender::renderTexture(coins_x + 40, row_lc + 2, GFX.Interface[1]);
    SuperPrintRightAlign(t = std::to_string(info.Coins), 1, coins_x + 62 + 36, row_lc + 2);

    // Fails Counter
    if(hasFails)
        SuperPrintRightAlign(fmt::format_ne("{0}: {1}", gDemoCounterTitle, info.Fails), 3, infobox_x + 480 - 10, row_2);
}

void mainMenuDraw()
{
    int MenuX, MenuY;
    GetMenuPos(&MenuX, &MenuY);

    int B = 0;

    // just don't call this during an offset!
    // XRender::offsetViewportIgnore(true);

    if(MenuMode != MENU_1PLAYER_GAME && MenuMode != MENU_2PLAYER_GAME && MenuMode != MENU_BATTLE_MODE && MenuMode != MENU_EDITOR)
        worldCurs = 0;

#ifdef __3DS__
    XRender::setTargetLayer(2);
#endif

    // Render the permanent menu graphics (curtain, URL, logo)

    // URL
    if(ScreenH >= SmallScreenH)
        XRender::renderTexture(ScreenW / 2 - GFX.MenuGFX[3].w / 2, ScreenH - 24, GFX.MenuGFX[3]);

    // Curtain
    // correction to loop the original asset properly
    int curtain_draw_w = GFX.MenuGFX[1].w;
    if(curtain_draw_w == 800)
        curtain_draw_w = 768;
    int curtain_horiz_reps = ScreenW / curtain_draw_w + 2;

    for(int i = 0; i < curtain_horiz_reps; i++)
        XRender::renderTexture(curtain_draw_w * i, 0, curtain_draw_w, GFX.MenuGFX[1].h, GFX.MenuGFX[1], 0, 0);

    // game logo
    int LogoMode = 0;
    if(ScreenH >= TinyScreenH || MenuMode == MENU_INTRO)
        LogoMode = 1;
    else if(MenuMode == MENU_MAIN || MenuMode == MENU_OPTIONS)
        LogoMode = 2;

    if(LogoMode == 1)
    {
        // show at half opacity if not at main menu on a small screen
        XTColor logo_tint = (ScreenH < SmallScreenH && MenuMode != MENU_INTRO && MenuMode != MENU_MAIN) ? XTAlpha(127) : XTColor();

        int logo_y = ScreenH / 2 - 230;

        // place manually on small screens
        if(ScreenH < SmallScreenH)
            logo_y = 16;
        else if(ScreenH <= 600)
            logo_y = 40;

        XRender::renderTexture(ScreenW / 2 - GFX.MenuGFX[2].w / 2, logo_y, GFX.MenuGFX[2], logo_tint);
    }
    else if(LogoMode == 2)
    {
        SuperPrint(g_gameInfo.title, 3, ScreenW/2 - g_gameInfo.title.length()*9, 30);
    }


    s_drawGameVersion();

#ifndef PGE_NO_THREADING
    if(SDL_AtomicGet(&loading))
    {
        if(SDL_AtomicGet(&loadingProgrssMax) <= 0)
            SuperPrint(g_mainMenu.loading, 3, MenuX, MenuY);
        else
        {
            int progress = (SDL_AtomicGet(&loadingProgrss) * 100) / SDL_AtomicGet(&loadingProgrssMax);
            SuperPrint(fmt::format_ne("{0} {1}%", g_mainMenu.loading, progress), 3, MenuX, MenuY);
        }
    }
    else
#endif

    // Menu Intro
    if(MenuMode == MENU_INTRO)
    {
        if((CommonFrame % 90) < 45)
            SuperPrint(g_mainMenu.introPressStart, 3, ScreenW/2 - g_mainMenu.introPressStart.length()*9, ScreenH - 40);
    }
    // Main menu
    if(MenuMode == MENU_MAIN)
    {
        int i = 0;
        SuperPrint(g_gameInfo.disableTwoPlayer ? g_mainMenu.mainStartGame : g_mainMenu.main1PlayerGame, 3, MenuX, MenuY+30*(i++));
        if(!g_gameInfo.disableTwoPlayer)
            SuperPrint(g_mainMenu.mainMultiplayerGame, 3, MenuX, MenuY+30*(i++));
        if(!g_gameInfo.disableBattleMode)
            SuperPrint(g_mainMenu.mainBattleGame, 3, MenuX, MenuY+30*(i++));
        if(g_config.enable_editor)
            SuperPrint(g_mainMenu.mainEditor, 3, MenuX, MenuY+30*(i++));
        SuperPrint(g_mainMenu.mainOptions, 3, MenuX, MenuY+30*(i++));
        SuperPrint(g_mainMenu.mainExit, 3, MenuX, MenuY+30*(i++));
        XRender::renderTexture(MenuX - 20, MenuY + (MenuCursor * 30), 16, 16, GFX.MCursor[0], 0, 0);
    }

    // Character select
    else if(MenuMode == MENU_CHARACTER_SELECT_NEW ||
            MenuMode == MENU_CHARACTER_SELECT_NEW_BM)
    {
        ConnectScreen::Render();
    }
#if 0 // dead now
    else if(MenuMode == MENU_CHARACTER_SELECT_1P ||
            MenuMode == MENU_CHARACTER_SELECT_2P_S1 ||
            MenuMode == MENU_CHARACTER_SELECT_2P_S2 ||
            MenuMode == MENU_CHARACTER_SELECT_BM_S1 ||
            MenuMode == MENU_CHARACTER_SELECT_BM_S2)
    {
        A = 0;
        B = 0;
        C = 0;

        s_drawGameTypeTitle(MenuX, 280);
        SuperPrint(SelectWorld[selWorld].WorldName, 3, MenuX, 310, XTColorF(0.6f, 1.f, 1.f));

        // TODO: Make a custom playable character names print here
        if(!blockCharacter[1])
            SuperPrint(g_mainMenu.selectPlayer[1], 3, MenuX, MenuY);
        else
        {
            A -= 30;
            if(MenuCursor + 1 >= 1)
                B -= 30;
            if(PlayerCharacter >= 1)
                C -= 30;
        }

        if(!blockCharacter[2])
            SuperPrint(g_mainMenu.selectPlayer[2], 3, MenuX, 380 + A);
        else
        {
            A -= 30;
            if(MenuCursor + 1 >= 2)
                B -= 30;
            if(PlayerCharacter >= 2)
                C -= 30;
        }

        if(!blockCharacter[3])
            SuperPrint(g_mainMenu.selectPlayer[3], 3, MenuX, 410 + A);
        else
        {
            A -= 30;
            if(MenuCursor + 1 >= 3)
                B -= 30;
            if(PlayerCharacter >= 3)
                C -= 30;
        }

        if(!blockCharacter[4])
            SuperPrint(g_mainMenu.selectPlayer[4], 3, MenuX, 440 + A);
        else
        {
            A -= 30;
            if(MenuCursor + 1 >= 4)
                B -= 30;
            if(PlayerCharacter >= 4)
                C -= 30;
        }

        if(!blockCharacter[5])
            SuperPrint(g_mainMenu.selectPlayer[5], 3, MenuX, 470 + A);
        else
        {
            A -= 30;
            if(MenuCursor + 1 >= 5)
                B -= 30;
            if(PlayerCharacter >= 5)
                C -= 30;
        }

        if(MenuMode == MENU_CHARACTER_SELECT_2P_S2 || MenuMode == MENU_CHARACTER_SELECT_BM_S2)
        {
            XRender::renderTexture(MenuX - 20, B + MenuY + (MenuCursor * 30), GFX.MCursor[3]);
            XRender::renderTexture(MenuX - 20, B + MenuY + ((PlayerCharacter - 1) * 30), GFX.MCursor[0]);
        }
        else
        {
            XRender::renderTexture(MenuX - 20, B + MenuY + (MenuCursor * 30), GFX.MCursor[0]);
        }
    }
#endif

    // Episode / Level selection
    else if(MenuMode == MENU_1PLAYER_GAME || MenuMode == MENU_2PLAYER_GAME || MenuMode == MENU_BATTLE_MODE || MenuMode == MENU_EDITOR)
    {
        s_drawGameTypeTitle(MenuX, MenuY - 70);
        // std::string tempStr;

        minShow = 1;
        maxShow = (MenuMode == MENU_BATTLE_MODE) ? NumSelectBattle :
            ((MenuMode == MENU_EDITOR) ? NumSelectWorldEditable :
                NumSelectWorld);
        const std::vector<SelectWorld_t>& SelectorList
            = (MenuMode == MENU_BATTLE_MODE) ? SelectBattle :
                ((MenuMode == MENU_EDITOR) ? SelectWorldEditable :
                    SelectWorld);

        int original_maxShow = maxShow;
        if(maxShow > 5)
        {
            minShow = worldCurs;
            maxShow = minShow + 4;

            if(MenuCursor <= minShow - 1)
                worldCurs -= 1;

            if(MenuCursor >= maxShow - 1)
                worldCurs += 1;

            if(worldCurs < 1)
                worldCurs = 1;

            if(worldCurs > original_maxShow - 4)
                worldCurs = original_maxShow - 4;

            minShow = worldCurs;
            maxShow = minShow + 4;
        }

        for(auto A = minShow; A <= maxShow; A++)
        {
            auto w = SelectorList[A];

            XTColor color;
            color.r = w.highlight ? 0 : 255;

            if(w.disabled)
                color = {127, 127, 127};

            B = A - minShow + 1;

            SuperPrint(w.WorldName, 3, MenuX, MenuY - 30 + (B * 30), color);
        }

        // render the scroll indicators
        if(minShow > 1)
            XRender::renderTexture(ScreenW/2 - 8, MenuY - 20, GFX.MCursor[1]);

        if(maxShow < original_maxShow)
            XRender::renderTexture(ScreenW/2 - 8, MenuY + 140, GFX.MCursor[2]);

        B = MenuCursor - minShow + 1;

        if(B >= 0 && B < 5)
            XRender::renderTexture(MenuX - 20, MenuY + (B * 30), GFX.MCursor[0].w, GFX.MCursor[0].h, GFX.MCursor[0], 0, 0);
    }

    else if(MenuMode == MENU_SELECT_SLOT_1P || MenuMode == MENU_SELECT_SLOT_2P) // Save Select
    {
        s_drawGameTypeTitle(MenuX, MenuY - 70);
        SuperPrint(SelectWorld[selWorld].WorldName, 3, MenuX, MenuY - 40, XTColorF(0.6f, 1.f, 1.f));
        s_drawGameSaves(MenuX, MenuY);
        XRender::renderTexture(MenuX - 20, MenuY + ((MenuCursor - s_GetSavesPageMenuOffset()) * 30), GFX.MCursor[0]);
    }

    else if(MenuMode == MENU_SELECT_SLOT_1P_COPY_S1 || MenuMode == MENU_SELECT_SLOT_2P_COPY_S1 ||
            MenuMode == MENU_SELECT_SLOT_1P_COPY_S2 || MenuMode == MENU_SELECT_SLOT_2P_COPY_S2) // Copy save
    {
        s_drawGameTypeTitle(MenuX, MenuY - 70);
        SuperPrint(SelectWorld[selWorld].WorldName, 3, MenuX, MenuY - 40, XTColorF(0.6f, 1.f, 1.f));
        s_drawGameSaves(MenuX, MenuY);

        if(MenuMode == MENU_SELECT_SLOT_1P_COPY_S1 || MenuMode == MENU_SELECT_SLOT_2P_COPY_S1)
            SuperPrint(g_mainMenu.gameSourceSlot, 3, MenuX, MenuY + c_menuSavesFooterHint, XTColorF(0.7f, 0.7f, 1.0f));
        else if(MenuMode == MENU_SELECT_SLOT_1P_COPY_S2 || MenuMode == MENU_SELECT_SLOT_2P_COPY_S2)
            SuperPrint(g_mainMenu.gameTargetSlot, 3, MenuX, MenuY + c_menuSavesFooterHint, XTColorF(0.7f, 1.0f, 0.7f));

        if(MenuMode == MENU_SELECT_SLOT_1P_COPY_S2 || MenuMode == MENU_SELECT_SLOT_2P_COPY_S2)
        {
            if((menuCopySaveSrc - 1) / c_menuSavesPerPage == MenuCursor / c_menuSavesPageLength)
                XRender::renderTexture(MenuX - 20, MenuY + ((menuCopySaveSrc - 1 - s_GetSavesPageSaveOffset()) * 30), GFX.MCursor[0]);

            XRender::renderTexture(MenuX - 20, MenuY + ((MenuCursor - s_GetSavesPageMenuOffset()) * 30), GFX.MCursor[3]);
        }
        else
            XRender::renderTexture(MenuX - 20, MenuY + ((MenuCursor - s_GetSavesPageMenuOffset()) * 30), GFX.MCursor[0]);
    }

    else if(MenuMode == MENU_SELECT_SLOT_1P_DELETE || MenuMode == MENU_SELECT_SLOT_2P_DELETE) // Delete save
    {
        s_drawGameTypeTitle(MenuX, MenuY - 70);
        SuperPrint(SelectWorld[selWorld].WorldName, 3, MenuX, MenuY - 40, XTColorF(0.6f, 1.f, 1.f));
        s_drawGameSaves(MenuX, MenuY);

        SuperPrint(g_mainMenu.gameEraseSlot, 3, MenuX, MenuY + c_menuSavesFooterHint, XTColorF(1.0f, 0.7f, 0.7f));

        XRender::renderTexture(MenuX - 20, MenuY + ((MenuCursor - s_GetSavesPageMenuOffset()) * 30), GFX.MCursor[0]);
    }

    // Options Menu
    else if(MenuMode == MENU_OPTIONS)
    {
        int i = 0;
        SuperPrint(g_mainMenu.controlsTitle, 3, MenuX, MenuY + 30*i++);
#ifndef RENDER_FULLSCREEN_ALWAYS
        if(resChanged)
            SuperPrint(g_mainMenu.optionsModeWindowed, 3, MenuX, MenuY + (30 * i++));
        else
            SuperPrint(g_mainMenu.optionsModeFullScreen, 3, MenuX, MenuY + (30 * i++));
#endif

#ifndef RENDER_CUSTOM
        const char* const renderers[] = {
            "SW",
            "Auto",
            "SDL2",
            "OpenGL 3+",
            "OpenGL ES 2+",
            "OpenGL 1.1",
            "OpenGL ES 1.1",
        };

        if(g_videoSettings.renderMode == RENDER_ACCELERATED_AUTO)
            SuperPrint(fmt::format_ne(g_mainMenu.optionsRenderAuto, renderers[g_videoSettings.renderModeObtained]), 3, MenuX, MenuY + (30 * i++));
        else if(g_videoSettings.renderMode != g_videoSettings.renderModeObtained)
            SuperPrint(fmt::format_ne(g_mainMenu.optionsRenderX, renderers[g_videoSettings.renderMode]), 3, MenuX, MenuY + (30 * i++));
        else
            SuperPrint(fmt::format_ne(g_mainMenu.optionsRender, renderers[g_videoSettings.renderMode]), 3, MenuX, MenuY + (30 * i++));
#endif

        const std::string* scale_str = &ScaleMode_strings.at(g_videoSettings.scaleMode);
        if(g_videoSettings.scaleMode == SCALE_DYNAMIC_INTEGER)
            scale_str = &g_mainMenu.optionsScaleInteger;
        else if(g_videoSettings.scaleMode == SCALE_DYNAMIC_NEAREST)
            scale_str = &g_mainMenu.optionsScaleNearest;
        else if(g_videoSettings.scaleMode == SCALE_DYNAMIC_LINEAR)
            scale_str = &g_mainMenu.optionsScaleLinear;

        SuperPrint(fmt::format_ne("{0}: {1}", g_mainMenu.optionsScaleMode, *scale_str), 3, MenuX, MenuY + (30 * i++));

        std::string resString = fmt::format_ne("RES: {0}x{1}", g_config.InternalW, g_config.InternalH);

        if (g_config.InternalW == 480 && g_config.InternalH == 320)
            resString += " (GBA)";
        else if (g_config.InternalW == 512 && g_config.InternalH == 384)
            resString += " (NDS)";
        else if (g_config.InternalW == 512 && g_config.InternalH == 448)
            resString += " (SNES)";
        else if (g_config.InternalW == 640 && g_config.InternalH == 480)
            resString += " (VGA)";
        else if (g_config.InternalW == 768 && g_config.InternalH == 432)
            resString += " (HELLO)";
        else if (g_config.InternalW == 800 && g_config.InternalH == 480)
            resString += " (3DS)";
        else if (g_config.InternalW == 800 && g_config.InternalH == 600)
            resString += " (SMBX)";
        else if (g_config.InternalW == 1280 && g_config.InternalH == 720)
            resString += " (HD)";
        else if (g_config.InternalW == 0 && g_config.InternalH == 600)
            resString = "RES: 600P DYNAMIC";
        else if (g_config.InternalW == 0 && g_config.InternalH == 0)
            resString = "RES: DYNAMIC";
        else
            resString += " (CUSTOM)";

        SuperPrint(resString, 3, MenuX, MenuY + (30 * i++));

        if(!FontManager::isInitied() || FontManager::isLegacy())
            SuperPrint("Language: <missing \"fonts\">", 3, MenuX, MenuY + (30 * i++), XTColorF(0.5f, 0.5f, 0.5f, 1.0f));
        else
            SuperPrint(fmt::format_ne("{0}: {1} ({2})", g_mainMenu.wordLanguage, g_mainMenu.languageName, g_config.language), 3, MenuX, MenuY + (30 * i++));

        SuperPrint(g_mainMenu.optionsViewCredits, 3, MenuX, MenuY + (30 * i++));

        XRender::renderTexture(MenuX - 20, MenuY + (MenuCursor * 30),
                               GFX.MCursor[0].w, GFX.MCursor[0].h, GFX.MCursor[0], 0, 0);
    }

    // Player controls setup
    else if(MenuMode == MENU_INPUT_SETTINGS)
    {
        menuControls_Render();
    }

    // Mouse cursor
    XRender::renderTexture(int(SharedCursor.X), int(SharedCursor.Y), GFX.ECursor[2]);
}
