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
#ifndef SOUND_THREAD_H
#define SOUND_THREAD_H

#include <stdint.h>

#ifndef THEXTECH_NO_SDL_BUILD

/**
 * @brief Enqueues the SFX but does not block
 * @param Alias The alias of the sound
 * @param loops Number loops to play (n-1 value. When -1 - loop forever)
 * @param volume The volume level between 0 and 128
 * @param left The panning volume of left channel between 0 and 255
 * @param right The panning volume of right channel between 0 and 255
 */
void PlaySfx(int Alias, int loops = 0, int volume = 128, uint8_t left = 255, uint8_t right = 255);

/**
 * @brief Starts sound thread
 */
void StartSfxThread();

/**
 * @brief ends sound thread
 */
void EndSfxThread();

#else

// fallback: just call directly
#define PlaySfx PlaySfx_Blocking

static inline void StartSfxThread() {}
static inline void EndSfxThread() {}

#endif

#endif // #ifndef SOUND_THREAD_H
