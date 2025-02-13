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

#include "sdl_proxy/sdl_stdinc.h"
#include "sdl_proxy/sdl_timer.h"

#include <fmt_format_ne.h>
#include <Logger/logger.h>
#include "pge_delay.h"

#include "frame_timer.h"
#include "globals.h"
#include "config.h"
#include "graphics.h"
#include "core/render.h"
#include "core/events.h"

#include "npc/npc_queues.h"

// heap info for min console ports
#if defined(__16M__) || defined(__3DS__) // || defined(__WII__)
    #define STATS_SHOW_RAM
    #include <malloc.h>
#endif // #if defined(__16M__) || defined(__3DS__) || defined(__WII__)

// VRAM info for 16M
#ifdef __16M__
    namespace XRender
    {
        extern uint32_t s_loadedVRAM;
    }
#endif

// VRAM info for 3DS
#ifdef __3DS__
    #include <3ds.h>
    extern u32 __ctru_linear_heap_size;
#endif

// heap size for Wii
#if defined(STATS_SHOW_RAM) && defined(__WII__)
    extern u8 __Arena1Lo[], __Arena1Hi[];
    extern u8 __Arena2Lo[], __Arena2Hi[];
    static const uint32_t s_heap_size = (__Arena2Hi - __Arena2Lo) + (__Arena1Hi - __Arena1Lo);
// heap size for other homebrew
#elif defined(STATS_SHOW_RAM)
    extern u8 *fake_heap_start;
    extern u8 *fake_heap_end;
    static const uint32_t s_heap_size = (fake_heap_end - fake_heap_start);
#endif

MicroStats g_microStats;
PerformanceStats_t g_stats;

void MicroStats::reset()
{
    for(uint8_t i = 0; i < TASK_END; i++)
    {
        level_timer[i] = 0;
        view_timer[i] = 0;
        m_cur_timer[i] = 0;
    }

    view_total = 0;

    m_slow_frame_time = 0;
    m_frame_time = 0;

    m_cur_task = TASK_END;
    m_cur_frame = 0;
}

void MicroStats::start_task(Task task)
{
    uint64_t next_time = SDL_GetMicroTicks();

    if(m_cur_task < TASK_END)
    {
        m_cur_timer[m_cur_task] += next_time - m_cur_time;
        level_timer[m_cur_task] += next_time - m_cur_time;
        m_frame_time += next_time - m_cur_time;
    }

    m_cur_time = next_time;
    m_cur_task = task;
}

void MicroStats::start_sleep()
{
    start_task(TASK_END);
}

void MicroStats::end_frame()
{
    start_task(TASK_END);

    m_cur_frame++;
    m_level_frame++;

    if(m_frame_time > m_slow_frame_time)
        m_slow_frame_time = m_frame_time;

    m_frame_time = 0;

    if(m_cur_frame == 66)
    {
        m_cur_frame = 0;
        view_total = 0;

        for(uint8_t i = 0; i < TASK_END; i++)
        {
            view_timer[i] = (m_cur_timer[i] + 500) / 1000;
            view_total += (m_cur_timer[i] + 500) / 1000;
            m_cur_timer[i] = 0;
        }

        view_slow_frame_time = (m_slow_frame_time * 66 + 500) / 1000;
        m_slow_frame_time = 0;
    }
}

void PerformanceStats_t::next_page()
{
    if((XRender::TargetW >= 720 && XRender::TargetH >= 360) || (LevelSelect && !GameMenu))
        page = !page;
    else
        page = (page + 1) % (GameMenu ? 4 : 5);

#ifndef STATS_SHOW_RAM
    if(page == 3)
        page++;
#endif
}

void PerformanceStats_t::reset()
{
   renderedBlocks = 0;
   renderedBGOs = 0;
   renderedNPCs = 0;
   renderedEffects = 0;

   checkedBlocks = 0;
   checkedBGOs = 0;

   renderedTiles = 0;
   renderedScenes = 0;
   renderedPaths = 0;
   renderedLevels = 0;

   checkedTiles = 0;
   checkedScenes = 0;
   checkedPaths = 0;
   checkedLevels = 0;
}

#define YLINE (y + 2 + (row++ * 18))

#ifdef STATS_SHOW_RAM
static void s_print_ram(int x, int y)
{
    int row = 0;

    int items = 1; // RAM

#   ifndef __WII__
    items++; // VRAM
#   endif

    XRender::renderRect(x, y, 340, 12 + (18 * items), XTColorF(0.0f, 0.0f, 0.0f, 0.3f), true);

    if(x < 200)
        y += 6;

    auto m = mallinfo();

#   ifdef __16M__
    SuperPrint(fmt::sprintf_ne(" RAM: %4d/%4dkb", m.uordblks/1024, s_heap_size/1024),
               3, x + 4, YLINE);
    SuperPrint(fmt::sprintf_ne("VRAM: %4d/%4dkb", XRender::s_loadedVRAM/1024, 512),
               3, x + 4, YLINE, XTColorF(0.5f, 1.0f, 0.5f));
#   else
    SuperPrint(fmt::sprintf_ne(" RAM: %5d/%5dkb", m.uordblks/1024, s_heap_size/1024),
               3, x + 4, YLINE);
#   endif

#   ifdef __3DS__
    SuperPrint(fmt::sprintf_ne("VRAM: %5d/%5dkb", (__ctru_linear_heap_size - linearSpaceFree())/1024, __ctru_linear_heap_size / 1024),
               3, x + 4, YLINE, XTColorF(0.5f, 1.0f, 0.5f));
#   endif
}
#else
static void s_print_ram(int, int) {}
#endif // #ifdef STATS_SHOW_RAM

void PerformanceStats_t::print_filenames(int x, int y)
{
    int items = (GameMenu) ? 4 : 3;
    int row = 0;

    XRender::renderRect(x, y, 745, 6 + (18 * items), XTColorF(0.0f, 0.0f, 0.0f, 0.3f), true);

    SuperPrint(fmt::sprintf_ne("FILE: %s", FileNameFull.empty() ? "<none>" : FileNameFull.c_str()),
               3, x + 4, YLINE, XTColorF(1.f, 0.5f, 1.f));
    SuperPrint(fmt::sprintf_ne("MUSK: %s", currentMusic.empty() ? "<none>" : currentMusic.c_str()),
               3, x + 4, YLINE, XTColorF(1.f, 0.5f, 1.f));
    SuperPrint(fmt::sprintf_ne("MUSF: %s", currentMusicFile.empty() ? "<none>" : currentMusicFile.c_str()),
               3, x + 4, YLINE, XTColorF(1.f, 0.5f, 1.f));

    if(GameMenu)
    {
        SuperPrint(fmt::sprintf_ne("MENU-MODE: %d", MenuMode),
                   3, x + 4, YLINE, XTColorF(0.5f, 1.f, 1.f));
    }
}

void PerformanceStats_t::print_obj_stats(int x, int y)
{
    int items = 5;
    int row = 0;

    XRender::renderRect(x, y, 340, 6 + (18 * items), XTColorF(0.0f, 0.0f, 0.0f, 0.3f), true);

    SuperPrint(fmt::sprintf_ne("   DRAW/ACTV/TOTAL"),
        3, x + 4, YLINE, XTColorF(1.f, 1.f, 1.f));

    SuperPrint(fmt::sprintf_ne("B: %04d/%04d/%05d", renderedBlocks, checkedBlocks, numBlock),
        3, x + 4, YLINE, XTColorF(0.5f, 1.f, 1.f));
    SuperPrint(fmt::sprintf_ne("G: %04d/%04d/%05d", renderedBGOs, checkedBGOs, numBackground),
        3, x + 4, YLINE, XTColorF(0.5f, 1.f, 1.f));
    SuperPrint(fmt::sprintf_ne("N: %04d/%04d/%05d", renderedNPCs, NPCQueues::Active.no_change.size(), numNPCs),
        3, x + 4, YLINE, XTColorF(0.5f, 1.f, 1.f));
    SuperPrint(fmt::sprintf_ne("E: %04d/%04d", renderedEffects, numEffects),
        3, x + 4, YLINE, XTColorF(0.5f, 1.f, 1.f));
}

void PerformanceStats_t::print_cpu_stats(int x, int y)
{
    int items = 6;

    XRender::renderRect(x, y, 340, 6 + (18 * items), XTColorF(0.0f, 0.0f, 0.0f, 0.3f), true);

    SuperPrint(fmt::sprintf_ne("CPU: %05dms/s",
                               g_microStats.view_total),
               3, x + 24, y + 2);

    SuperPrint(fmt::sprintf_ne("%s %04d\n%s %04d\n%s %04d\n%s %04d\n%s %04d",
                               g_microStats.task_names[0], g_microStats.view_timer[0],
                               g_microStats.task_names[1], g_microStats.view_timer[1],
                               g_microStats.task_names[2], g_microStats.view_timer[2],
                               g_microStats.task_names[3], g_microStats.view_timer[3],
                               g_microStats.task_names[4], g_microStats.view_timer[4]),
               3, x + 4, y + 2 + 18, XTColorF(1.f, 1.f, 0.5f));

    SuperPrint(fmt::sprintf_ne("%s %04d\n%s %04d\n%s %04d\n%s %04d\n%s %04d",
                               g_microStats.task_names[5], g_microStats.view_timer[5],
                               g_microStats.task_names[6], g_microStats.view_timer[6],
                               g_microStats.task_names[7], g_microStats.view_timer[7],
                               g_microStats.task_names[8], g_microStats.view_timer[8],
                               g_microStats.task_names[9], g_microStats.view_timer[9]),
               3, x + 164, y + 2 + 18, XTColorF(1.f, 1.f, 0.5f));
}

void PerformanceStats_t::print()
{
    if(page == 0)
        return;

    if(LevelSelect && !GameMenu)
    {
        int items = 5;
        XRender::renderRect(6 + XRender::TargetOverscanX, 6, 745, 6 + (18 * items), XTColorF(0.0f, 0.0f, 0.0f, 0.3f), true);

        int y = 6;
        int row = 0;

        SuperPrint(fmt::sprintf_ne("FILE: %s", FileNameFull.empty() ? "<none>" : FileNameFull.c_str()),
                   3, 10 + XRender::TargetOverscanX, YLINE, XTColorF(0.5f, 1.f, 1.f));
        SuperPrint(fmt::sprintf_ne("MUSK: %s", currentMusic.empty() ? "<none>" : currentMusic.c_str()),
                   3, 10 + XRender::TargetOverscanX, YLINE, XTColorF(0.5f, 1.f, 1.f));
        SuperPrint(fmt::sprintf_ne("MUSF: %s", currentMusicFile.empty() ? "<none>" : currentMusicFile.c_str()),
                   3, 10 + XRender::TargetOverscanX, YLINE, XTColorF(0.5f, 1.f, 1.f));

        SuperPrint(fmt::sprintf_ne("DRAW: T=%03d S=%03d P=%03d L=%03d, SUM=%03d",
                                   renderedTiles, renderedScenes, renderedPaths, renderedLevels,
                                   (renderedTiles + renderedScenes + renderedPaths + renderedLevels)),
                   3, 10 + XRender::TargetOverscanX, YLINE);
        SuperPrint(fmt::sprintf_ne("CHEK: T=%03d S=%03d P=%03d L=%03d, SUM=%03d",
                                   checkedTiles, checkedScenes, checkedPaths, checkedLevels,
                                   (checkedTiles + checkedScenes + checkedPaths + checkedLevels)),
                   3, 10 + XRender::TargetOverscanX, YLINE);

        s_print_ram(10 + XRender::TargetOverscanX, YLINE);
    }
    else if(XRender::TargetW >= 720 && XRender::TargetH >= 360)
    {
        // threshold of 720
        int next_y = 6;
        print_filenames(6 + XRender::TargetOverscanX, next_y);

        next_y += (GameMenu) ? 6 + 18 * 4 : 6 + 18 * 3;

        if(!GameMenu)
        {
            print_cpu_stats(6 + XRender::TargetOverscanX, next_y);
            print_obj_stats(6 + 340 + XRender::TargetOverscanX, next_y);
            s_print_ram(6 + XRender::TargetOverscanX, next_y + 6 + 18 * 6);
        }
        else
        {
            print_obj_stats(6 + XRender::TargetOverscanX, next_y);
            s_print_ram(6 + XRender::TargetOverscanX, next_y + 6 + 18 * 5);
        }
    }
    else if(page == 1)
        print_filenames(6 + XRender::TargetOverscanX, 6);
    else if(page == 3)
        s_print_ram(6 + XRender::TargetOverscanX, 6);
    else if(page == 4 && !GameMenu)
        print_cpu_stats(6 + XRender::TargetOverscanX, 6);
    else
        print_obj_stats(6 + XRender::TargetOverscanX, 6);
}

#undef YLINE

//#if !defined(__EMSCRIPTEN__)
#define USE_NEW_TIMER
#define USE_NEW_FRAMESKIP
//#endif

#ifdef USE_NEW_TIMER
#define COMPUTE_FRAME_TIME_1_REAL computeFrameTime1Real_2
#define COMPUTE_FRAME_TIME_2_REAL computeFrameTime2Real_2
#else
#define COMPUTE_FRAME_TIME_1_REAL computeFrameTime1Real
#define COMPUTE_FRAME_TIME_2_REAL computeFrameTime2Real
#endif


#ifdef USE_NEW_TIMER

#define ONE_MILLIARD 1000000000

typedef int64_t nanotime_t;

static inline nanotime_t getNanoTime()
{
    return static_cast<nanotime_t>(SDL_GetTicks()) * 1000000;
}

static inline nanotime_t getElapsedTime(nanotime_t oldTime)
{
    return getNanoTime() - oldTime;
}

static inline nanotime_t getSleepTime(nanotime_t oldTime, nanotime_t target)
{
    return target - getElapsedTime(oldTime);
}

static inline void xtech_nanosleep(nanotime_t sleepTime)
{
    if(sleepTime <= 0)
        return;
    PGE_Delay((uint32_t)SDL_ceil(sleepTime / 1000000.0));
}


struct TimeStore
{
    const size_t size = 4;
    size_t       pos = 0;
    nanotime_t   sum = 0;
    nanotime_t   items[4] = {0, 0, 0, 0};

    void add(nanotime_t item)
    {
        sum -= items[pos];
        sum += item;
        items[pos] = item;
        pos = (pos + 1) % size;
    }

    nanotime_t average()
    {
        return sum / size;
    }
};

static TimeStore         s_overheadTimes;
static const  nanotime_t c_frameRateNano = 1000000000.0 / 64.1025;
static nanotime_t        s_oldTime = 0,
                         s_overhead = 0;
#ifdef USE_NEW_FRAMESKIP
static nanotime_t        s_startProcessing = 0;
static nanotime_t        s_stopProcessing = 0;
static nanotime_t        s_doUpdate = 0;
#endif

#endif
// ----------------------------------------------------

//Public Const frameRate As Double = 15 'for controlling game speed
//const int frameRate = 15;
static const  double c_frameRate = 15.0;

static double s_overTime = 0;
static double s_goalTime = 0;
static double s_fpsCount = 0.0;
static double s_fpsTime = 0.0;
static int    s_cycleCount = 0;
static double s_gameTime = 0.0;
static double s_currentTicks = 0.0;


void resetFrameTimer()
{
    s_overTime = 0;
    s_fpsCount = 0;
    s_fpsTime = 0;
    s_cycleCount = 0;
    s_gameTime = 0;
#ifdef USE_NEW_FRAMESKIP
    s_doUpdate = 0;
    s_goalTime = 0;
#else
    s_goalTime = SDL_GetTicks() + 1000;
#endif
    // D_pLogDebugNA("Time counter reset was called");
}

void resetTimeBuffer()
{
    s_currentTicks = 0;
}

#ifdef USE_NEW_FRAMESKIP

bool frameSkipNeeded()
{
    return s_doUpdate > 0;
}

#else

bool frameSkipNeeded() // Old and buggy Redigit's
{
    return SDL_GetTicks() + SDL_floor(1000 * (1 - (s_cycleCount / 63.0))) > s_goalTime;
}

#endif

void frameNextInc()
{
    s_fpsCount += 1;
}

void cycleNextInc()
{
    s_cycleCount++;
}

extern void CheckActive(); // game_main.cpp

static inline bool canProcessFrameCond()
{
    bool ret = s_currentTicks >= s_gameTime + c_frameRate || s_currentTicks < s_gameTime || g_config.unlimited_framerate;
#ifdef USE_NEW_FRAMESKIP
    if(ret && s_doUpdate <= 0)
        s_startProcessing = getNanoTime();
#endif
    return ret;
}

bool canProceedFrame()
{
    s_currentTicks = SDL_GetTicks();
    return canProcessFrameCond();
}

#ifndef USE_NEW_TIMER
static inline void computeFrameTime1Real()
{
    if(s_fpsCount >= 32000)
        s_fpsCount = 0; // Fixes Overflow bug

    if(s_cycleCount >= 32000)
        s_cycleCount = 0; // Fixes Overflow bug

    s_overTime += (s_currentTicks - (s_gameTime + c_frameRate));

    if(s_gameTime == 0.0)
        s_overTime = 0;

    if(s_overTime <= 1)
        s_overTime = 0;
    else if(s_overTime > 1000)
        s_overTime = 1000;

    s_gameTime = s_currentTicks - s_overTime;
    s_overTime -= (s_currentTicks - s_gameTime);
}

static inline void computeFrameTime2Real()
{
    if(SDL_GetTicks() > s_fpsTime)
    {
        if(s_cycleCount >= 65)
        {
            s_overTime = 0;
            s_gameTime = s_currentTicks;
        }
        s_cycleCount = 0;
        s_fpsTime = SDL_GetTicks() + 1000;
        s_goalTime = s_fpsTime;
        //      if(Debugger == true)
        //          frmLevelDebugger.lblFPS = fpsCount;

        // if(g_config.show_fps)
        PrintFPS = s_fpsCount;

        s_fpsCount = 0;
    }
}
#endif


#ifdef USE_NEW_TIMER
static inline void computeFrameTime1Real_2()
{
    if(s_fpsCount >= 32000)
        s_fpsCount = 0; // Fixes Overflow bug

    if(s_cycleCount >= 32000)
        s_cycleCount = 0; // Fixes Overflow bug
}

static inline void computeFrameTime2Real_2()
{
#ifdef USE_NEW_FRAMESKIP
    if(s_doUpdate > 0)
        s_doUpdate -= c_frameRateNano;

    s_startProcessing = 0;
    s_stopProcessing = 0;
#endif

    if(SDL_GetTicks() > s_fpsTime)
    {
        if(s_cycleCount >= 65)
        {
            s_overTime = 0;
            s_gameTime = s_currentTicks;
        }

        s_cycleCount = 0;
        s_fpsTime = SDL_GetTicks() + 1000;
        s_goalTime = s_fpsTime;

        // if(g_config.show_fps)
        PrintFPS = s_fpsCount;

        s_fpsCount = 0;
    }

    if(!g_config.unlimited_framerate)
    {
        nanotime_t start = getNanoTime();
        nanotime_t sleepTime = getSleepTime(s_oldTime, c_frameRateNano);
        s_overhead = s_overheadTimes.average();

        // D_pLogDebug("ST=%lld, OH=%lld", sleepTime, s_overhead);
        if(sleepTime > s_overhead)
        {
            nanotime_t adjustedSleepTime = sleepTime - s_overhead;
            if(adjustedSleepTime > 500000000)
            {
                pLogWarning("frame_timer: Adjusted sleep time got a too big value: %ld", (long)adjustedSleepTime);
                adjustedSleepTime = 500000000;
            }

            xtech_nanosleep(adjustedSleepTime);
            auto e = getElapsedTime(start);
            nanotime_t overslept = e - adjustedSleepTime;
            // SDL_assert(overslept >= 0);
            if(overslept < 0)
                s_overheadTimes.add(0);
            else if(overslept < c_frameRateNano)
                s_overheadTimes.add(overslept);
        }
    }

    s_oldTime = getNanoTime();
}
#endif


void computeFrameTime1()
{
    COMPUTE_FRAME_TIME_1_REAL();
}

void computeFrameTime2()
{
    COMPUTE_FRAME_TIME_2_REAL();
}

void runFrameLoop(LoopCall_t doLoopCallbackPre,
                  LoopCall_t doLoopCallbackPost,
                  std::function<bool(void)> condition,
                  std::function<bool ()> subCondition,
                  std::function<void ()> preTimerExtraPre,
                  std::function<void ()> preTimerExtraPost)
{
    do
    {
        if(preTimerExtraPre)
            preTimerExtraPre();

        XEvents::doEvents();
        s_currentTicks = SDL_GetTicks();

        if(preTimerExtraPost)
            preTimerExtraPost();

        if(canProcessFrameCond())
        {
            CheckActive();
            if(doLoopCallbackPre)
                doLoopCallbackPre();

            COMPUTE_FRAME_TIME_1_REAL();

            if(doLoopCallbackPost)
                doLoopCallbackPost(); // Run the loop callback
            XEvents::doEvents();

            COMPUTE_FRAME_TIME_2_REAL();

            if(subCondition && subCondition())
                break;
        }

        if(!g_config.unlimited_framerate)
            PGE_Delay(1);

        if(!GameIsActive)
            break;// Break on quit
    }
    while(condition());
}

void frameRenderStart()
{
#ifdef USE_NEW_FRAMESKIP
    if(s_doUpdate <= 0)
        s_startProcessing = getNanoTime();
#endif
}

void frameRenderEnd()
{
#ifdef USE_NEW_FRAMESKIP
    if(s_doUpdate <= 0)
    {
        s_stopProcessing = getNanoTime();
        nanotime_t newTime = g_config.enable_frameskip ? (s_stopProcessing - s_startProcessing): 0;
        // D_pLogDebug("newTime/nano=%lld (%lld)", newTime/c_frameRateNano, newTime / 1000000);
        if(newTime > c_frameRateNano * 25) // Limit 25 frames being skipped maximum
        {
            pLogDebug("frame_timer: Overloading detected: %lld frames to skip (%lld milliseconds delay)", newTime / c_frameRateNano, newTime / 1000000);
            newTime = c_frameRateNano * 25;
        }

        s_doUpdate += newTime * 300 / 166;
        s_goalTime = double(SDL_GetTicks() + (newTime / 1000000));
    }
#endif
}
