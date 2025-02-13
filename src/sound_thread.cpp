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

#include <vector>
#include <cstdint>
#include <exception>

#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_mutex.h>

#include "sound.h"
#include "sound_thread.h"

struct EnqueuedSfx_t
{
    int Alias;
    uint8_t loops;
    uint8_t volume;
    uint8_t left;
    uint8_t right;
};

static std::vector<EnqueuedSfx_t> s_sfx_queue;

static SDL_Thread* s_sound_thread = nullptr;
static SDL_mutex*  s_sound_thread_mutex = nullptr;
static SDL_cond*   s_sound_thread_cond = nullptr;

static bool        s_sound_thread_quit = false;

static int s_sound_thread_main(void*)
{
    static std::vector<EnqueuedSfx_t> s_sfx_queue_pop;
    s_sfx_queue_pop.clear();

    while(true)
    {
        // load state from the main thread
        SDL_LockMutex(s_sound_thread_mutex);

        if(!s_sound_thread_quit && s_sfx_queue.empty())
            SDL_CondWait(s_sound_thread_cond, s_sound_thread_mutex);

        bool quit = s_sound_thread_quit;
        std::swap(s_sfx_queue, s_sfx_queue_pop);

        SDL_UnlockMutex(s_sound_thread_mutex);

        if(quit)
            break;

        // execute SFX queue
        for(const EnqueuedSfx_t& sfx : s_sfx_queue_pop)
            PlaySfx_Blocking(sfx.Alias, sfx.loops, sfx.volume, sfx.left, sfx.right);

        s_sfx_queue_pop.clear();
    }

    return 0;
}

void PlaySfx(int Alias, int loops, int volume, uint8_t left, uint8_t right)
{
    if(!s_sound_thread)
    {
        PlaySfx_Blocking(Alias, loops, volume, left, right);
        return;
    }

    SDL_LockMutex(s_sound_thread_mutex);

    try
    {
        s_sfx_queue.push_back(EnqueuedSfx_t{Alias, (uint8_t)loops, (uint8_t)volume, left, right});
    }
    catch(...)
    {
        SDL_UnlockMutex(s_sound_thread_mutex);

        std::exception_ptr e = std::current_exception();

        if(e)
            std::rethrow_exception(e);
    }

    SDL_UnlockMutex(s_sound_thread_mutex);
    SDL_CondSignal(s_sound_thread_cond);
}

void StartSfxThread()
{
    EndSfxThread();

    s_sound_thread_mutex = SDL_CreateMutex();
    if(!s_sound_thread_mutex)
        return;

    s_sound_thread_cond = SDL_CreateCond();
    if(!s_sound_thread_cond)
        return;

    s_sound_thread = SDL_CreateThread(s_sound_thread_main, "SFX thread", nullptr);
}

void EndSfxThread()
{
    if(s_sound_thread)
    {
        SDL_LockMutex(s_sound_thread_mutex);
        s_sound_thread_quit = true;
        SDL_UnlockMutex(s_sound_thread_mutex);
        SDL_CondSignal(s_sound_thread_cond);

        SDL_WaitThread(s_sound_thread, nullptr);
        s_sound_thread = nullptr;
        s_sound_thread_quit = false;
    }

    if(s_sound_thread_cond)
    {
        SDL_DestroyCond(s_sound_thread_cond);
        s_sound_thread_cond = nullptr;
    }

    if(s_sound_thread_mutex)
    {
        SDL_DestroyMutex(s_sound_thread_mutex);
        s_sound_thread_mutex = nullptr;
    }
}
