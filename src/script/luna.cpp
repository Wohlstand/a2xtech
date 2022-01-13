#include "luna.h"
#include "autocode.h"
#include "autocode_manager.h"
#include "lunainput.h"
#include "lunarender.h"
#include "lunaspriteman.h"
#include "lunacell.h"
#include "lunaplayer.h"
#include "lunacounter.h"
#include "compat.h"

#include "globals.h"

#include <unordered_map>
#include <functional>

bool gLunaEnabledGlobally = true;
bool gLunaEnabled = true;
bool gShowDemoCounter = true;
bool gEnableDemoCounter = true;
std::string gDemoCounterTitle = "DEMOS";

static void (*levelCodeRun)() = nullptr;


#include "levels/Docopoper-Calleoca.h"
#include "levels/Docopoper-AbstractAssault.h"
#include "levels/Docopoper-TheFloorisLava.h"
#include "levels/SAJewers-QraestoliaCaverns.h"
#include "levels/SAJewers-Snowboardin.h"
#include "levels/Talkhaus-Science_Final_Battle.h"
#include "levels/KilArmoryCode.h"

static const std::unordered_map<std::string, std::function<void()>> s_levelInit =
{

// Abtract Assault init block
{
    "Docopoper-AbstractAssault.lvl",
    []()->void
    {
        levelCodeRun = AbstractAssaultCode;
    }
},
// Calleoca init block
{
    "Docopoper-Calleoca.lvl",
    []()->void
    {
        CalleocaInitCode();
        levelCodeRun = CalleocaCode;
    }
},
// The Floor is Lava init block
{
    "Docopoper-TheFloorisLava.lvl",
    []()->void
    {
        levelCodeRun = TheFloorisLavaCode;
        Player_t* demo = PlayerF::Get(1);
        if(demo)
            demo->Character = 1;
    }
},

// Qraestolia Caverns init block
{
    "SAJewers-QraestoliaCaverns.lvl",
    []()->void
    {
        levelCodeRun = QraestoliaCavernsCode;
    }
},

// Snowbordin init block
{
    "SAJewers-Snowboardin.lvl",
    []()->void
    {
        levelCodeRun = SAJSnowbordin::SnowbordinCode;
        SAJSnowbordin::SnowbordinInitCode();
    }
},

// Science init block
{
    "Talkhaus-Science_Final_Battle.lvl",
    []()->void
    {
        levelCodeRun = ScienceBattle::ScienceCode;
        ScienceBattle::ScienceInitCode();
    }
},

{
    "LUNA12-thou_starts_a_new_video.lvl",
    []()->void
    {
        levelCodeRun = KilArmoryCode;
        auto *demo = PlayerF::Get(1);
        if(demo)
        {
            PlayerF::FilterToBig(demo);
            PlayerF::FilterMount(demo);
            PlayerF::FilterReservePowerup(demo);
            demo->Character = 1;
        }
    }
},

};

static void InitLevel()
{
    levelCodeRun = nullptr;

    auto l = s_levelInit.find(FileNameFull);
    if(l != s_levelInit.end())
        l->second();
}

void lunaReset()
{
    levelCodeRun = nullptr;

    gFrames = 0;

    gLastDownPress = 0;
    gDownTapped = 0;
    gLastUpPress = 0;
    gUpTapped = 0;
    gLastLeftPress = 0;
    gLeftTapped = 0;
    gLastRightPress = 0;
    gRightTapped = 0;

    gLastJumpPress = 0;
    gJumpTapped = 0;
    gLastRunPress = 0;
    gRunTapped = 0;

    Renderer::Get().ClearAllDebugMessages();
    gSpriteMan.ResetSpriteManager();
    gCellMan.Reset();
//    gSavedVarBank.ClearBank();
    Input::ResetAll();

    gDeathCounter.quit();
}

void lunaLoad()
{
    lunaReset();

    bool isGame = !GameMenu && !GameOutro && !BattleMode && !LevelEditor && !TestLevel;
    bool dcAllow = (gEnableDemoCounter || g_compatibility.demos_counter_enable);

    if(dcAllow && isGame)
        gDeathCounter.init();

    if(gLunaEnabledGlobally && gLunaEnabled)
    {
        // Load autocode
        gAutoMan.LoadFiles();

        // Init some stuff
        InitLevel();
        gAutoMan.m_Hearts = 2;
    }

    if(dcAllow && isGame)
        gDeathCounter.Recount();
}

void lunaLoop()
{
    bool dcAllow = (gEnableDemoCounter || g_compatibility.demos_counter_enable);

    if(gLunaEnabledGlobally)
    {
        // Clean up
        gAutoMan.ClearExpired();

        // Update inputs
        Input::CheckSpecialCheats();
        Input::UpdateInputTasks();
    }

    if(dcAllow)
        gDeathCounter.UpdateDeaths(true);

    if(gLunaEnabledGlobally && gLunaEnabled)
    {
#if COMPILE_PLAYGROUND
        Playground::doPlaygroundStuff();
#endif
//        g_EventHandler.hookLevelLoop();

        // Run autocode
        gAutoMan.DoEvents(false);

        // Update some stuff
        gFrames++;
//        gSavedVarBank.SaveIfNeeded();

        // Run any framecode
//        TestFrameCode();
        if(levelCodeRun)
            levelCodeRun();
    }
}


void lunaRenderHud()
{
    bool dcAllow = (gEnableDemoCounter || g_compatibility.demos_counter_enable);
    if(dcAllow && gShowDemoCounter)
        gDeathCounter.Draw();
}

void lunaRender(int screenZ)
{
    if(gLunaEnabled && gLunaEnabledGlobally)
    {
        Renderer::Get().StartCameraRender(screenZ);
        Renderer::Get().RenderBelowPriority(5);
    }
}

void lunaRenderStart()
{
    if(gLunaEnabled && gLunaEnabledGlobally)
        Renderer::Get().StartFrameRender();
}

void lunaRenderEnd()
{
    if(gLunaEnabled && gLunaEnabledGlobally)
        Renderer::Get().EndFrameRender();
}
