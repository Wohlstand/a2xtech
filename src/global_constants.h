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

#pragma once
#ifndef GLOBALCONSTANTS_H
#define GLOBALCONSTANTS_H

#include <stdint.h>

// VB6 Integer type had 16-bit precision, but this is not ideal on modern platforms. Decide based on memory constraint.
// TEMPORARY: use 32-bit integers on all platforms instead of 16-bit while tracing some bugs
#if 0 // #ifdef LOW_MEM
    using vbint_t = int16_t;
#else
    using vbint_t = int;
#endif

// OBSERVE: layer and event references are no longer
//   represented by strings but rather indices in the
//   Layers / Events array.
// Every object that had a Layer or Event reference
//   now has a corresponding *String() inline function
//   which looks up and returns the appropriate string.
typedef uint8_t layerindex_t;
typedef uint8_t eventindex_t;
typedef uint16_t stringindex_t;

// note that the NONE indices rely on the fact that
//   there are at most 254 layers/events,
//   leaving the -1 (255) index unused.
// see layers.h for the respective maximums.
constexpr layerindex_t LAYER_NONE = static_cast<layerindex_t>(-1);
constexpr layerindex_t LAYER_DEFAULT = static_cast<layerindex_t>(0);
constexpr layerindex_t LAYER_DESTROYED_BLOCKS = static_cast<layerindex_t>(1);
constexpr layerindex_t LAYER_SPAWNED_NPCS = static_cast<layerindex_t>(2);

constexpr eventindex_t EVENT_NONE = static_cast<eventindex_t>(-1);
constexpr eventindex_t EVENT_LEVEL_START = static_cast<eventindex_t>(0);
constexpr eventindex_t EVENT_PSWITCH_START = static_cast<eventindex_t>(1);
constexpr eventindex_t EVENT_PSWITCH_END = static_cast<eventindex_t>(2);

constexpr stringindex_t STRINGINDEX_NONE = static_cast<stringindex_t>(-1);

const int MaxLevelStrings = 65535;

//Public Const MaxSavedEvents As Integer = 200
const int MaxSavedEvents = 200;

//const int maxSelectWorlds = 100;

const int maxCreditsLines = 200;

#ifdef LOW_MEM
const int maxSaveSlots = 3;
#else
const int maxSaveSlots = 6;
#endif

const int maxWorldCredits = 100;

const int maxYoshiGfx = 10;

// unused since multistars update
// const int maxStarsNum = 1000;

// maximum number of tracked medals and stars per level
// WARNING: c_max_track_medals must not exceed 8 (number of bits per byte) and c_max_track_stars must not exceed 255 (largest uint8_t)
const int c_max_track_medals = 8;
const int c_max_track_stars = 255;

const int maxLocalPlayers = 4;

// was previously changed to 10000; now only used for certain NPC bounds checking.
// returned to 8000.
const int FLBlocks = 8000;

//Public Const vScreenYOffset As Integer = 0     'Players Y on the screen
const int vScreenYOffset = 0;
//Public Const maxBlocks As Integer = 20000  'Max # of blocks
#ifdef __16M__
const int maxBlocks = 10000;
#else
const int maxBlocks = 20000;
#endif
//Public Const maxPlayers As Integer = 200  'Holds the max number of players
#ifdef __16M__
const int maxPlayers = 16;
#else
const int maxPlayers = 200;
#endif

//Public Const maxEffects As Integer = 1000    'Max # of effects
const int maxEffects = 1000;
//Public Const maxNPCs As Integer = 5000    'Max # of NPCs
#ifdef __16M__
const int maxNPCs = 3000;
#else
const int maxNPCs = 5000;
#endif
//Public Const maxBackgrounds As Integer = 8000    'Max # of background objects
#ifdef __16M__
const int maxBackgrounds = 4000;
#else
const int maxBackgrounds = 8000;
#endif
//Public Const maxPlayerFrames As Integer = 750 'Maximum number of player frames
const int maxPlayerFrames = 750;
//Public Const maxBlockType As Integer = 700 'Maximum number of block types
const int maxBlockType = 700;
//Public Const maxBackgroundType As Integer = 200 'Maximum number of background types
const int maxBackgroundType = 200;
//Public Const maxSceneType As Integer = 100 'Maximum number of scenetypes
const int maxSceneType = 100;
//Public Const maxNPCType As Integer = 300 'Maximum number of NPC types
const int maxNPCType = 302;
//Public Const maxEffectType As Integer = 200 'Maximum number of effect types
const int maxEffectType = 200;

//Public Const maxWarps As Integer = 200 'Maximum number of warps
#ifdef LOW_MEM
const int maxWarps = 200; // 200
#else
const int maxWarps = 2000; // 200
#endif

//Public Const numBackground2 As Integer = 100  'Total # of backgrounds
const int numBackground2 = 100;
//Public Const numCharacters As Integer = 5 'Maximum number of player characters
const int numCharacters = 5;
//Public Const numStates As Integer = 7   'Maximum number of player states
const int numStates = 7;
//Public Const maxWater As Integer = 1000
const int maxWater = 1000;
//Public Const maxWorldLevels As Integer = 400   'Maximum number of levels
const int maxWorldLevels = 400;
//Public Const maxWorldPaths As Integer = 2000   'Maximum number of paths
const int maxWorldPaths = 2000;
//Public Const maxWorldMusic As Integer = 1000   'Maximum number of musics
const int maxWorldMusic = 1000;
//! NEW: limit on world map area count
const int maxWorldAreas = 20;
//Public Const numSounds As Integer = 100
const int numSounds = 200;
//Public Const maxSections As Integer = 20
#ifdef LOW_MEM
const int maxSections = 20;
#else
const int maxSections = 200;
#endif
//Public Const maxTileType As Integer = 400
const int maxTileType = 400;
//Public Const maxLevelType As Integer = 100
const int maxLevelType = 100;
//Public Const maxPathType As Integer = 100
const int maxPathType = 100;
//Public Const maxTiles As Integer = 20000
#ifdef __16M__
const int maxTiles = 8000;
#else
const int maxTiles = 20000;
#endif
//Public Const maxScenes As Integer = 5000
const int maxScenes = 5000;

// moved to "screen.h"
//Public Const ScreenW As Integer = 800  'Game Screen Width
// const int ScreenW = 800;
//Public Const ScreenH As Integer = 600  'Game Screen Height
// const int ScreenH = 600;

#endif // GLOBALCONSTANTS_H
