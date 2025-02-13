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
#ifndef SOUND_H
#define SOUND_H

#include <string>
#include <cstdint>

#include "location.h"

// Public musicPlaying As Boolean
extern bool musicPlaying;
// Public musicLoop As Integer
extern int musicLoop;
// Public musicName As String
extern std::string musicName;

extern int playerHammerSFX;

extern const struct AudioDefaults_t
{
    int sampleRate;
    int channels;
    int bufferSize;
    uint16_t format;
} g_audioDefaults;

struct AudioSetup_t
{
    bool disableSound = false;
    int sampleRate = 44100;
    int channels = 2;
    int bufferSize = 512;
    uint16_t format = 0x8120;
};


enum
{
    SFX_Jump            = 1,
    SFX_Stomp,
    SFX_BlockHit,
    SFX_BlockSmashed,
    SFX_PlayerShrink,
    SFX_PlayerGrow,
    SFX_ItemEmerge,
    SFX_PlayerDied,
    SFX_ShellHit,
    SFX_Skid            = 10,
    SFX_DropItem,
    SFX_GotItem,
    SFX_Camera,
    SFX_Coin,
    SFX_1up,
    SFX_Lava,
    SFX_Warp,
    SFX_Fireball,
    SFX_CardRouletteClear,
    SFX_BossBeat        = 20,
    SFX_DungeonClear,
    SFX_Bullet,
    SFX_Grab,
    SFX_Spring,
    SFX_HeavyToss,
    SFX_Slide,
    SFX_NewPath,
    SFX_LevelSelect,
    SFX_Do,
    SFX_Pause           = 30,
    SFX_Key,
    SFX_PSwitch,
    SFX_Whip,
    SFX_Transform,
    SFX_Boot,
    SFX_Smash,
    SFX_Stone,
    SFX_SpitBossSpit,
    SFX_SpitBossHit,
    SFX_CrystalBallExit = 40,
    SFX_SpitBossBeat,
    SFX_BigFireball,
    SFX_Fireworks,
    SFX_VillainKilled,
    SFX_GameBeat,
    SFX_Door,
    SFX_Message,
    SFX_Pet,
    SFX_PetHurt,
    SFX_PetTongue       = 50,
    SFX_PetBirth,
    SFX_GotStar,
    SFX_HeroKill,
    SFX_PlayerDied2,
    SFX_PetSwallow,
    SFX_RingGet,
    SFX_Skeleton,
    SFX_Checkpoint,
    SFX_MedalGet,
    SFX_TapeExit        = 60,
    SFX_LavaMonster,
    SFX_SickBossSpit,
    SFX_SickBossKilled,
    SFX_SMBlockHit,
    SFX_SMKilled,
    SFX_SMHurt,
    SFX_SMGlass,
    SFX_SMBossHit,
    SFX_SMCry,
    SFX_SMExplosion     = 70,
    SFX_Climbing,
    SFX_Swim,
    SFX_Grab2,
    SFX_Saw,
    SFX_Throw,
    SFX_PlayerHit,
    SFX_HeroStab,
    SFX_HeroHurt,
    SFX_HeroHeart,
    SFX_HeroDied        = 80,
    SFX_HeroRupee,
    SFX_HeroFire,
    SFX_HeroItem,
    SFX_HeroKey,
    SFX_HeroShield,
    SFX_HeroDash,
    SFX_HeroFairy,
    SFX_HeroGrass,
    SFX_HeroHit,
    SFX_HeroSwordBeam   = 90,
    SFX_Bubble,

    // non-SMBX64, Extra
    SFX_CoinSwitchTimeout,
    SFX_BatFlap,
    SFX_Iceball,
    SFX_Freeze,
    SFX_Icebreak,
    SFX_PlayerHeavy, // Reserved
    SFX_SproutVine,
    SFX_MagicBossShell,
    SFX_MagicBossKilled = 100,
    SFX_FireBossKilled,
    SFX_HeroIce,
    SFX_HeroFireRod,
    SFX_FlameThrower
};

int CustomWorldMusicId();

void InitSoundDefaults();

// Public Sub InitMixerX()
void InitMixerX();
// Public Sub RestartMixerX()
void RestartMixerX();
// Public Sub QuitMixerX()
void QuitMixerX();
// Public Sub SetMusicVolume(Alias As String, Volume As Long)
void SetMusicVolume(const std::string &Alias, long Volume);
// Public Sub SoundPauseAll()
void SoundPauseAll();
// Public Sub SoundResumeAll()
void SoundResumeAll();
void SoundPauseEngine(int paused);
// Public Sub PlayMusic(Alias As String)
void PlayMusic(const std::string &Alias, int fadeInMs = 0);
// Public Sub PlaySfx(Alias As String)
void PlaySfx_Blocking(int Alias, int loops = 0, int volume = 128, uint8_t left = 255, uint8_t right = 255);
// Public Sub StopSfx(Alias As String)
void StopSfx(int Alias);
// Public Sub StartMusic(A As Integer) 'play music
void setMusicStartDelay();
void delayedMusicStart();
void delayedMusicReset();
bool delayMusicIsSet();
// play music
void StartMusic(int A, int fadeInMs = 0);
// Public Sub StopMusic() 'stop playing music
void PauseMusic();
void ResumeMusic();
// stop playing music
void StopMusic();
// Public Sub PlayInitSound()
void PlayInitSound();
// Public Sub InitSound() 'readys sound and music to be played
// readys sound and music to be played
void InitSound();
// unloads all sounds for asset pack switch
void UnloadSound();
// Public Sub PlaySound(A As Integer) 'play a sound
// play a sound
void PlaySound(int A, int loops = 0, int volume = 128);

// NEW: play a sound with spatial awareness
void PlaySoundSpatial(int A, int l, int t, int r, int b, int loops = 0, int volume = 128);

// public signatures for spatial sound feature
inline void PlaySoundSpatial(int A, const Location_t& loc, int loops = 0, int volume = 128)
{
    PlaySoundSpatial(A, loc.X, loc.Y, loc.X + loc.Width, loc.Y + loc.Height, loops, volume);
}

inline void PlaySoundSpatial(int A, const SpeedlessLocation_t& loc, int loops = 0, int volume = 128)
{
    PlaySoundSpatial(A, loc.X, loc.Y, loc.X + loc.Width, loc.Y + loc.Height, loops, volume);
}

inline void PlaySoundSpatial(int A, int x, int y, int loops = 0, int volume = 128)
{
    PlaySoundSpatial(A, x, y, x, y, loops, volume);
}

// NEW: internal function to determine the panning for a sound (defined in sound_spatial.cpp)
void Sound_ResolveSpatialMod(uint8_t& left, uint8_t& right, int l, int t, int r, int b);

// Check does sound is defined at sounds.ini
bool HasSound(int A);
void PlaySoundMenu(int A, int loops = 0);

#if defined(THEXTECH_ASSERTS_INGAME_MESSAGE) && !defined(THEXTECH_NO_SDL_BUILD)
// always uses fallback sound
void PlayErrorSound(int A, int loops = 0);
#endif

// Public Sub BlockSound() 'stops all sound from being played for 10 cycles
// stops all sound from being played for 10 cycles
void BlockSound();
// Public Sub UpdateSound() 'checks to loop music and update the soundpause variable
// checks to loop music and update the soundpause variable
void UpdateSound();

void UpdateYoshiMusic();
// EXTRA: Fade out music
void FadeOutMusic(int ms);
// EXTRA: load custom sounds.ini and music.ini from episode and custom folder!
void LoadCustomSound();
// EXTRA: Unload custom-loaded music and sounds, and restore originals
void UnloadCustomSound();

void UpdateMusicVolume();

void PreloadExtSound(const std::string &path);
void UnloadExtSounds();
void PlayExtSound(const std::string &path, int loops = 0, int volume = 128);
void StopExtSound(const std::string &path);
void StopAllExtSounds();
void StopAllSounds();

#ifdef THEXTECH_ENABLE_AUDIO_FX
struct SoundFXEchoSetup
{
    int echoOn = 0;
    int echoDelay = 0;
    int echoFeedBack = 0;

    int echoMainVolL = 0;
    int echoMainVolR = 0;

    int echoVolL = 0;
    int echoVolR = 0;

    int echoFir[8] = {0, 0, 0, 0, 0, 0, 0, 0};
};
void SoundFX_SetEcho(const SoundFXEchoSetup &setup);

struct SoundFXReverb
{
    float mode         = 0.0f; // Normal (0) or Freeze (>0.5)
    float roomSize     = 0.7f;
    float damping      = 0.5f; // 0.0...1.0
    float wetLevel     = 0.2f;
    float dryLevel     = 0.4f;
    float width        = 1.0f; // 0.0...1.0
};
void SoundFX_SetReverb(const SoundFXReverb &setup);

void SoundFX_Clear();
#endif // THEXTECH_ENABLE_AUDIO_FX

void ResetSoundFX();

void UpdateSoundFX(int recentSection);

#endif // SOUND_H
