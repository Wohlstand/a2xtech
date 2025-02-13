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
#include "sdl_proxy/sdl_atomic.h"
#include "sdl_proxy/sdl_assert.h"
#include "sdl_proxy/sdl_timer.h"
#include "sdl_proxy/mixer.h"

#include "globals.h"
#include "config.h"
#include "global_dirs.h"
#include "frame_timer.h"

#include "load_gfx.h"
#include "core/msgbox.h"
#include "main/screen_progress.h"

#include "sound.h"
#include "sound_thread.h"

#ifdef THEXTECH_ENABLE_AUDIO_FX
#include "sound/fx/reverb.h"
#include "sound/fx/spc_echo.h"
#endif

#include "sound/sound_msgsnd.h"

#include <Logger/logger.h>
#include <IniProcessor/ini_processing.h>
#include <Utils/files.h>
#include <Utils/files_ini.h>
#include <Utils/strings.h>
#include <unordered_map>
#include <fmt_format_ne.h>

enum class SoundScope
{
    global,
    episode,
    custom
};

// Public musicPlaying As Boolean
bool musicPlaying = false;
// Public musicLoop As Integer
int musicLoop = 0;
// Public musicName As String
std::string musicName;

int playerHammerSFX = SFX_Fireball;

const AudioDefaults_t g_audioDefaults =
#if defined(__WII__) /* Defaults for Nintendo Wii */
{
    32000,
    2,
    1536,
    (int)AUDIO_S16SYS
};
#elif defined(__WIIU__)
{
    44100,
    2,
    1024,
    (int)AUDIO_F32SYS
};
#elif defined(__3DS__)
{
    44100,
    2,
    2048,
    (int)AUDIO_S16SYS
};
#elif defined(__SWITCH__) /* Defaults for Nintendo Switch */
{
    48000,
    2,
    1024,
    (int)AUDIO_S16SYS
};
#else /* Defaults for all other platforms */
{
    44100,
    2,
    512,
    (int)AUDIO_F32SYS
};
#endif

static Mix_Music *g_curMusic = nullptr;
bool g_mixerLoaded = false;

//! most recent argument to StartMusic. Could be a world map music ID or a section index.
static constexpr int s_null_music = -5;
static int s_recentMusicA = s_null_music;

static int g_customLvlMusicId = 24;
static int g_customWldMusicId = 17;
static int g_reservedChannels = 0;

//! Total count of loaded default sounds
static unsigned int g_totalSounds = 0;
//! Are custom sounds was loaded from the level/world data folder?
static bool g_customSoundsInDataFolder = false;
//! Are custom music files was loaded from the level/world data folder?
static bool g_customMusicInDataFolder = false;
//! Total count of level music
static unsigned int g_totalMusicLevel = 0;
//! Total count of world map music
static unsigned int g_totalMusicWorld = 0;
//! Total count of special music
static unsigned int g_totalMusicSpecial = 0;
//! Enable using the unique iceball SFX when available
static bool s_useIceBallSfx = false;
//! Enable using of the new ice SFX: NPC freeze and breaking of the frozen NPC
static bool s_useNewIceSfx = false;

static int g_errorsSfx = 0;
// static int g_errorsMusic = 0; // Unued yet

static bool s_musicHasYoshiMode = false;
static int  s_musicYoshiTrackNumber = -1;
#ifdef THEXTECH_ENABLE_AUDIO_FX
static bool s_musicDisableSpcEcho = false;
#endif
static int s_musicDefaultVolume = 64;

static std::string MusicRoot;
static std::string SfxRoot;

static std::string musicIni; // = "music.ini";
static std::string sfxIni; // = "sounds.ini";

#ifdef THEXTECH_ENABLE_AUDIO_FX
struct SectionEffect_t
{
    enum FX
    {
        FX_None = 0,
        FX_Echo,
        FX_Reverb
    };
    //! Reverb effect settings
    SoundFXReverb rev;
    //! Echo effect settings
    SoundFXEchoSetup echo;
    //! Selected effect
    int fx = FX_None;
    //! Disable echo of playing SPC files
    bool disableSpcEcho = false;
};

static SectionEffect_t s_sectionEffect[maxSections + 1];
static std::unordered_map<std::string, SectionEffect_t> s_effectsList;
#endif

struct Music_t
{
    std::string path;
    int volume = 52;
    int yoshiModeTrack = -1;
};

struct SFX_t
{
    std::string path;
    std::string customPath;
    Mix_Chunk *chunk = nullptr;
    Mix_Music *music = nullptr;
    Mix_Chunk *chunkOrig = nullptr;
    Mix_Music *musicOrig = nullptr;
    bool isCustom = false;
    bool isSilent = false;
    bool isSilentOrig = false;
    int volume = 128;
    int channel = -1;
};

static std::unordered_map<std::string, Music_t> music;
static std::unordered_map<int, SFX_t>           sound;

//! Sounds played by scripts
static SDL_atomic_t                                extSfxBusy;
static std::unordered_map<std::string, Mix_Chunk*> extSfx;
static std::unordered_map<int, std::string>        extSfxPlaying;
static void extSfxStopCallback(int channel);

static const int maxSfxChannels = 91;

#ifdef LOW_MEM
static const double c_max_chunk_duration = 1.25; // max length of an in-memory chunk in seconds
#else
static const double c_max_chunk_duration = 5.0;  // max length of an in-memory chunk in seconds
#endif

static const char *audio_format_to_string(SDL_AudioFormat f)
{
    switch(f)
    {
    default:
        return "<unknown>";
    case AUDIO_U8:
        return "U8";
    case AUDIO_S8:
        return "S8";
    case AUDIO_S16LSB:
        return "S16-LE";
    case AUDIO_S16MSB:
        return "S16-BE";
    case AUDIO_U16LSB:
        return "U16-LE";
    case AUDIO_U16MSB:
        return "U16-BE";
    case AUDIO_S32LSB:
        return "S32-LE";
    case AUDIO_S32MSB:
        return "S32-BE";
    case AUDIO_F32LSB:
        return "F32-LE";
    case AUDIO_F32MSB:
        return "F32-BE";
    }
}


int CustomWorldMusicId()
{
    return g_customWldMusicId;
}

void InitMixerX()
{
#ifndef THEXTECH_NO_SDL_BUILD
    if(!g_config.audio_enable)
        return;

    int ret;
    const int initFlags = MIX_INIT_MID | MIX_INIT_MOD | MIX_INIT_FLAC | MIX_INIT_OGG | MIX_INIT_OPUS | MIX_INIT_MP3;

    MusicRoot = AppPath + "music/";
    SfxRoot = AppPath + "sound/";

    Mix_SetRWFromFile(Files::open_file);

    SDL_AtomicSet(&extSfxBusy, 0);

    if(g_mixerLoaded)
        return;

#ifdef __3DS__
    // Create the blank "dspfirm.cdc" if not exists (it's required to exist for the 3DS audio work on Citra HLE)
    if(!Files::fileExists("/3ds/dspfirm.cdc"))
    {
        FILE *x = Files::utf8_fopen("/3ds/dspfirm.cdc", "wb");
        fclose(x);
    }
#endif

    pLogDebug("Opening sound (wanted: rate=%d hz, format=%s, channels=%d, buffer=%d frames)...",
              (int)g_config.audio_sample_rate,
              audio_format_to_string((int)g_config.audio_format),
              (int)g_config.audio_channels,
              (int)g_config.audio_buffer_size);

    ret = Mix_Init(initFlags);

    if(ret != initFlags)
    {
        pLogWarning("MixerX: Some modules aren't properly initialized");
        if((ret & MIX_INIT_MID) != MIX_INIT_MID)
            pLogWarning("MixerX: Failed to initialize MIDI module");
        if((ret & MIX_INIT_MOD) != MIX_INIT_MOD)
            pLogWarning("MixerX: Failed to initialize Tracker music module");
        if((ret & MIX_INIT_FLAC) != MIX_INIT_FLAC)
            pLogWarning("MixerX: Failed to initialize FLAC module");
        if((ret & MIX_INIT_OGG) != MIX_INIT_OGG)
            pLogWarning("MixerX: Failed to initialize OGG Vorbis module");
        if((ret & MIX_INIT_OPUS) != MIX_INIT_OPUS)
            pLogWarning("MixerX: Failed to initialize Opus module");
        if((ret & MIX_INIT_MP3) != MIX_INIT_MP3)
            pLogWarning("MixerX: Failed to initialize MP3 module");
    }

    ret = Mix_OpenAudio(g_config.audio_sample_rate,
                        g_config.audio_format,
                        g_config.audio_channels,
                        g_config.audio_buffer_size);

    if(ret < 0)
    {
        std::string msg = fmt::format_ne("Can't open audio stream, continuing without audio: ({0})", Mix_GetError());
        pLogCritical(msg.c_str());
        XMsgBox::simpleMsgBox(XMsgBox::MESSAGEBOX_ERROR, "Sound opening error", msg);
    }
    else
    {
        SDL_AudioSpec ob;
        ret = Mix_QuerySpecEx(&ob);

        if(ret == 0)
        {
            pLogCritical("Failed to call the Mix_QuerySpec!");
            g_config.audio_sample_rate.obtained = g_config.audio_sample_rate;
            g_config.audio_format.obtained = g_config.audio_format;
            g_config.audio_channels.obtained = g_config.audio_channels;
            g_config.audio_buffer_size.obtained = g_config.audio_buffer_size;
        }
        else
        {
            g_config.audio_sample_rate.obtained = ob.freq;
            g_config.audio_format.obtained = ob.format;
            g_config.audio_channels.obtained = ob.channels;
            g_config.audio_buffer_size.obtained = ob.samples;

            pLogDebug("Sound opened (obtained: rate=%d hz, format=%s, channels=%d, buffer=%d frames)...",
                      ob.freq,
                      audio_format_to_string(ob.format),
                      ob.channels,
                      ob.samples);
        }

#if defined(__3DS__)
        // Set fastest emulators to be default
        Mix_OPNMIDI_setEmulator(OPNMIDI_OPN2_EMU_GENS);
        Mix_OPNMIDI_setChannelAllocMode(MIX_CHIP_CHANALLOC_SameInst);
        Mix_OPNMIDI_setChipsCount(2);
        Mix_ADLMIDI_setEmulator(ADLMIDI_OPL3_EMU_DOSBOX);
        Mix_ADLMIDI_setChannelAllocMode(MIX_CHIP_CHANALLOC_SameInst);
        Mix_ADLMIDI_setChipsCount(2);
#elif defined(__WII__)
        // Set fastest emulators to be default
        Mix_OPNMIDI_setEmulator(OPNMIDI_OPN2_EMU_MAME_OPN2);
        Mix_OPNMIDI_setChipsCount(2);
        Mix_ADLMIDI_setEmulator(ADLMIDI_OPL3_EMU_DOSBOX);
        Mix_ADLMIDI_setChipsCount(2);
#elif defined(__vita__)
        Mix_OPNMIDI_setEmulator(OPNMIDI_OPN2_EMU_MAME_OPN2);
        Mix_OPNMIDI_setChipsCount(2);
        Mix_ADLMIDI_setEmulator(ADLMIDI_OPL3_EMU_DOSBOX);
        Mix_ADLMIDI_setChipsCount(2);
#endif

        Mix_VolumeMusic(MIX_MAX_VOLUME);
        Mix_AllocateChannels(maxSfxChannels);

        // Set channel finished callback to handle finished custom SFX
        Mix_ChannelFinished(&extSfxStopCallback);

        StartSfxThread();

        g_mixerLoaded = true;
    }
#endif // #ifndef THEXTECH_NO_SDL_BUILD
}

void RestartMixerX()
{
    int recent_music = s_recentMusicA;

    if(g_mixerLoaded && !musicPlaying)
        recent_music = s_null_music;

    UnloadSound();
    QuitMixerX();

    InitMixerX();
    InitSound();
    LoadCustomSound();

    if(recent_music != s_null_music)
        StartMusic(recent_music);
}

void QuitMixerX()
{
    if(!g_mixerLoaded)
        return;

    EndSfxThread();

    UnloadExtSounds();

    if(g_curMusic)
        Mix_FreeMusic(g_curMusic);

    g_curMusic = nullptr;

    for(auto & it : sound)
    {
        auto &s = it.second;
        if(s.chunk)
            Mix_FreeChunk(s.chunk);

        if(s.chunkOrig)
            Mix_FreeChunk(s.chunkOrig);

        if(s.music)
        {
            Mix_HaltMusicStream(s.music);
            Mix_FreeMusic(s.music);
        }

        if(s.musicOrig)
            Mix_FreeMusic(s.musicOrig);
    }

    sound.clear();
    music.clear();

    Mix_CloseAudio();
    Mix_Quit();

    g_mixerLoaded = false;
}

static void AddMusic(SoundScope root,
                     IniProcessing &ini,
                     const std::string &alias,
                     const std::string &group,
                     int volume)
{
    std::string f;
    ini.beginGroup(group);
    ini.read("file", f, std::string());

    if(!f.empty())
    {
        Music_t m;
        if(root == SoundScope::global)
            m.path = MusicRoot + f;
        else if(root == SoundScope::episode)
            m.path = g_dirEpisode.resolveFileCaseAbs(f);
        else if(root == SoundScope::custom)
            m.path = g_dirCustom.resolveFileCaseAbs(f);

        ini.read("yoshi-mode-track", m.yoshiModeTrack, -1);
        m.volume = volume;
        pLogDebug("Adding music [%s] '%s'", alias.c_str(), m.path.c_str());

        auto a = music.find(alias);

        if(a == music.end())
            music.insert({alias, m});
        else
            a->second = m;
    }

    ini.endGroup();
}

static void RestoreSfx(SFX_t &u)
{
    if(u.isCustom)
    {
        if((u.chunk || u.music || u.isSilent) && (u.chunkOrig || u.musicOrig || u.isSilentOrig))
        {
            if(u.chunk)
                Mix_FreeChunk(u.chunk);

            if(u.music)
                Mix_FreeMusic(u.music);

            u.chunk = u.chunkOrig;
            u.music = u.musicOrig;
            u.isSilent = u.isSilentOrig;
            u.chunkOrig = nullptr;
            u.musicOrig = nullptr;
        }
        u.isCustom = false;
    }
}

static void AddSfx(SoundScope root,
                   IniProcessing &ini,
                   int alias,
                   const std::string &group,
                   bool isCustom = false)
{
    std::string f;
    bool isSilent;
    ini.beginGroup(group);
    ini.read("file", f, std::string());
    ini.read("silent", isSilent, false);

    if(!f.empty() || isSilent)
    {
        if(LoadingInProcess)
            LoaderUpdateDebugString(fmt::format_ne("sound {0}", f));

        if(isCustom)
        {
            auto s = sound.find(alias);

            if(s != sound.end())
            {
                auto &m = s->second;

                std::string newPath;
                if(root == SoundScope::global)
                    newPath = SfxRoot + f;
                else if(root == SoundScope::episode)
                    newPath = g_dirEpisode.resolveFileCaseAbs(f);
                else if(root == SoundScope::custom)
                    newPath = g_dirCustom.resolveFileCaseAbs(f);

                if(!isSilent && m.isCustom && newPath == m.customPath)
                {
                    ini.endGroup();
                    return;  // Don't load the same file twice!
                }

                Mix_Chunk *backup_chunk = m.chunk;
                Mix_Music *backup_music = m.music;
                bool backup_isSilent = m.isSilent;
                m.customPath = newPath;

                m.chunk = nullptr;
                m.music = nullptr;
                m.isSilent = false;

                if(!isSilent)
                {
                    m.music = Mix_LoadMUS((newPath).c_str());
                    if(m.music)
                    {
                        // check, if short enough, load it as a chunk instead
                        double duration = Mix_MusicDuration(m.music);
                        if(duration >= 0 && duration < c_max_chunk_duration)
                        {
                            Mix_FreeMusic(m.music);
                            m.music = nullptr;
                        }
                        else
                            pLogInfo("Will load SFX %s as a multi-music", newPath.c_str());
                    }

                    if(!m.music)
                        m.chunk = Mix_LoadWAV((newPath).c_str());
                }

                if(m.chunk || m.music || isSilent)
                {
                    if(!m.isCustom && !m.chunkOrig && !m.musicOrig)
                    {
                        m.chunkOrig = backup_chunk;
                        m.musicOrig = backup_music;
                        m.isSilentOrig = backup_isSilent;
                    }
                    else
                    {
                        if(backup_chunk)
                            Mix_FreeChunk(backup_chunk);
                        if(backup_music)
                            Mix_FreeMusic(backup_music);
                    }

                    m.isCustom = true;
                    m.isSilent = isSilent;
                }
                else
                {
                    m.chunk = backup_chunk;
                    m.music = backup_music;
                    m.isSilent = backup_isSilent;
                    pLogWarning("ERROR: SFX '%s' loading error: %s", m.path.c_str(), Mix_GetError());
                }
            }
        }
        else
        {
            SFX_t m;

            if(root == SoundScope::global)
                m.path = SfxRoot + f;
            else if(root == SoundScope::episode)
                m.path = g_dirEpisode.resolveFileCaseAbs(f);
            else if(root == SoundScope::custom)
                m.path = g_dirCustom.resolveFileCaseAbs(f);

            m.isSilent = isSilent;
            pLogDebug("Adding SFX [sound%d] '%s'", alias, isSilent ? "<silence>" : m.path.c_str());
            if(!isSilent)
            {
                m.music = Mix_LoadMUS((m.path).c_str());
                if(m.music)
                {
                    // check, if short enough, load it as a chunk instead
                    double duration = Mix_MusicDuration(m.music);
                    if(duration >= 0 && duration < c_max_chunk_duration)
                    {
                        Mix_FreeMusic(m.music);
                        m.music = nullptr;
                    }
                    else
                        pLogInfo("Will load SFX %s as a multi-music", m.path.c_str());
                }

                if(!m.music)
                    m.chunk = Mix_LoadWAV((m.path).c_str());
            }

            if(m.chunk || m.music || isSilent)
            {
                bool isSingleChannel = false;
                ini.read("single-channel", isSingleChannel, false);
                if(isSingleChannel && m.chunk)
                    m.channel = g_reservedChannels++;
                sound.insert({alias, m});
            }
            else
            {
                pLogWarning("ERROR: SFX '%s' loading error: %s", m.path.c_str(), Mix_GetError());
                g_errorsSfx++;
            }
        }
    }
    ini.endGroup();
}

void SetMusicVolume(const std::string &Alias, long Volume)
{
    auto mus = music.find(Alias);
    if(mus != music.end())
    {
        mus->second.volume = int(Volume);
    }
}

void SoundPauseAll()
{
    if(!g_mixerLoaded)
        return;

    pLogDebug("Pause all sound");
    Mix_PauseAudio(1);
}

void SoundResumeAll()
{
    if(!g_mixerLoaded)
        return;

    pLogDebug("Resume all sound");
    Mix_PauseAudio(0);
}

void SoundPauseEngine(int paused)
{
    if(!g_mixerLoaded)
        return;

    Mix_PauseAudio(paused);
}

static void processPathArgs(std::string &path,
                            const std::string &episodeRoot,
                            const std::string &dataDirName,
                            int *yoshiModeTrack = nullptr)
{
    if(path.find('|') == std::string::npos)
        return; // Nothing to do
    Strings::List p;
    Strings::split(p, path, '|');

    if(yoshiModeTrack)
    {
        *yoshiModeTrack = -1;
        Strings::List args;
        Strings::split(args, p[1], ';');
        for(auto &arg : args)
        {
            if(arg.compare(0, 3, "ym=") != 0)
                continue;
            *yoshiModeTrack = SDL_atoi(arg.substr(3).c_str());
        }
    }

    Strings::replaceInAll(p[1], "{e}", episodeRoot);
    Strings::replaceInAll(p[1], "{d}", episodeRoot + dataDirName);
    Strings::replaceInAll(p[1], "{r}", MusicRoot);
    path = p[0] + "|" + p[1];
}

void PlayMusic(const std::string &Alias, int fadeInMs)
{
    if(!g_mixerLoaded)
        return;

    if(g_curMusic)
    {
        Mix_HaltMusicStream(g_curMusic);
        Mix_FreeMusic(g_curMusic);
        g_curMusic = nullptr;
        g_stats.currentMusic.clear();
        g_stats.currentMusicFile.clear();
    }

    auto mus = music.find(Alias);
    if(mus != music.end())
    {
        auto &m = mus->second;
        std::string p = m.path;
        processPathArgs(p, FileNamePath + "/", FileName + "/");
        g_curMusic = Mix_LoadMUS(p.c_str());

        if(!g_curMusic)
            pLogWarning("Music '%s' opening error: %s", m.path.c_str(), Mix_GetError());
        else
        {
            int ret;

            Mix_VolumeMusicStream(g_curMusic, m.volume * g_config.audio_mus_volume / 100);
            s_musicYoshiTrackNumber = m.yoshiModeTrack;
            s_musicHasYoshiMode = (s_musicYoshiTrackNumber >= 0 && (Mix_GetMusicTracks(g_curMusic) > s_musicYoshiTrackNumber));
            s_musicDefaultVolume = m.volume;
            UpdateYoshiMusic();

#ifdef THEXTECH_ENABLE_AUDIO_FX
            if(s_musicDisableSpcEcho)
                Mix_GME_SetSpcEchoDisabled(g_curMusic, s_musicDisableSpcEcho);
#endif

            if(fadeInMs > 0)
                ret = Mix_FadeInMusic(g_curMusic, -1, fadeInMs);
            else
                ret = Mix_PlayMusic(g_curMusic, -1);

            if(ret >= 0)
            {
                g_stats.currentMusic = Mix_GetMusicTitle(g_curMusic);
                g_stats.currentMusicFile = Files::basename(m.path);
            }
            else
                pLogWarning("Music '%s' playing error: %s", m.path.c_str(), Mix_GetError());
        }
    }
    else
        pLogWarning("Unknown music alias '%s'", Alias.c_str());
}

void PlaySfx_Blocking(int Alias, int loops, int volume, uint8_t left, uint8_t right)
{
    if(!g_mixerLoaded || (int)g_config.audio_sfx_volume == 0)
        return;

    auto sfx = sound.find(Alias);
    if(sfx != sound.end())
    {
        auto &s = sfx->second;
        if(s.chunk)
        {
            int channel = Mix_PlayChannelVol(s.channel, s.chunk, loops, volume * g_config.audio_sfx_volume / 100);

            if(channel >= 0)
                Mix_SetPanning(channel, left, right);
        }
        else if(s.music)
        {
            if(Mix_PlayingMusicStream(s.music))
                Mix_RewindMusicStream(s.music);

            Mix_VolumeMusicStream(s.music, volume * g_config.audio_sfx_volume / 100);
            Mix_SetMusicEffectPanning(s.music, left, right);
            Mix_PlayMusicStream(s.music, loops);
        }
    }
}

void StopSfx(int Alias)
{
    auto sfx = sound.find(Alias);
    if(sfx != sound.end())
    {
        auto &s = sfx->second;
        if(s.chunk)
            Mix_HaltChannel(s.channel);
        else if(s.music)
            Mix_HaltMusicStream(s.music);
    }
}


static bool s_delayMusic = false;
static bool s_delayedMusicRequested = false;
static int  s_delayedMusicA = 0;
static int  s_delayedMusicFadeInMs = 0;

void setMusicStartDelay()
{
    s_delayedMusicRequested = false;
    s_delayMusic = true;
}

bool delayMusicIsSet()
{
    return s_delayMusic;
}

void delayedMusicStart()
{
    s_delayMusic = false;
    if(s_delayedMusicRequested)
    {
        D_pLogDebug("Restored delayed music request A=%d", s_delayedMusicA);
        StartMusic(s_delayedMusicA, s_delayedMusicFadeInMs);
        s_delayedMusicRequested = false;
    }
}

void delayedMusicReset()
{
    s_delayMusic = false;
    if(s_delayedMusicRequested)
        D_pLogDebug("Saved music request erase A=%d", s_delayedMusicA);
    s_delayedMusicRequested = false;
}

void StartMusic(int A, int fadeInMs)
{
    if(s_delayMusic)
    {
        s_delayedMusicA = A;
        s_delayedMusicFadeInMs = fadeInMs;
        s_delayedMusicRequested = true;
        D_pLogDebug("Saved start music request A=%d", A);
        return;
    }

    D_pLogDebug("Start music A=%d", A);

    if(!g_mixerLoaded || (int)g_config.audio_mus_volume == 0)
    {
        if(g_mixerLoaded && g_curMusic)
            StopMusic();

        // Keep world map music being remembered when sound disabled
        if((LevelSelect || WorldEditor) && !GameMenu && !GameOutro)
            curWorldMusic = A;

        s_recentMusicA = A;
        musicPlaying = true;

        return;
    }

    if((LevelSelect || WorldEditor) && !GameMenu && !GameOutro) // music on the world map
    {
        StopMusic();
        curWorldMusic = A;
        std::string mus = fmt::format_ne("wmusic{0}", A);
        if(curWorldMusic == g_customWldMusicId)
        {
            pLogDebug("Starting custom music [%s]", curWorldMusicFile.c_str());
            if(g_curMusic)
                Mix_FreeMusic(g_curMusic);
            std::string p = FileNamePath + "/" + curWorldMusicFile;
            processPathArgs(p, FileNamePath + "/", FileName + "/");
            g_curMusic = Mix_LoadMUS(p.c_str());
            s_musicHasYoshiMode = false;
            s_musicYoshiTrackNumber = -1;
            Mix_VolumeMusicStream(g_curMusic, 64 * g_config.audio_mus_volume / 100);
            s_musicDefaultVolume = 64;
            if(fadeInMs > 0)
                Mix_FadeInMusic(g_curMusic, -1, fadeInMs);
            else
                Mix_PlayMusic(g_curMusic, -1);
        }
        else
        {
            pLogDebug("Starting world music [%s]", mus.c_str());
            PlayMusic(mus, fadeInMs);
        }
        musicName = std::move(mus);
    }
    else if(A == -1) // P switch music
    {
        StopMusic();
        if(FreezeNPCs) {
            pLogDebug("Starting special music [stmusic]");
            PlayMusic("stmusic", fadeInMs);
            musicName = "stmusic";
        } else {
            pLogDebug("Starting special music [smusic]");
            PlayMusic("smusic", fadeInMs);
            musicName = "smusic";
        }
        curMusic = -1;
    }
    else if(PSwitchTime == 0 && PSwitchStop == 0) // level music
    {
        StopMusic();
        curMusic = bgMusic[A];
        std::string mus = fmt::format_ne("music{0}", curMusic);
        if(curMusic == g_customLvlMusicId)
        {
            int ret = 0;

            pLogDebug("Starting custom music [%s%s]", FileNamePath.c_str(), CustomMusic[A].c_str());
            if(g_curMusic)
                Mix_FreeMusic(g_curMusic);
            std::string p = FileNamePath + CustomMusic[A];
            s_musicYoshiTrackNumber = -1;
            processPathArgs(p, FileNamePath, FileName + "/", &s_musicYoshiTrackNumber);
            g_curMusic = Mix_LoadMUS(p.c_str());
            if(!g_curMusic)
                pLogWarning("Failed to open the music [%s]: ", p.c_str(), Mix_GetError());
            else
            {
                s_musicHasYoshiMode = (s_musicYoshiTrackNumber >= 0 && (Mix_GetMusicTracks(g_curMusic) > s_musicYoshiTrackNumber));
                UpdateYoshiMusic();
                Mix_VolumeMusicStream(g_curMusic, 52 * g_config.audio_mus_volume / 100);
                s_musicDefaultVolume = 52;
                if(fadeInMs > 0)
                {
                    ret = Mix_FadeInMusic(g_curMusic, -1, fadeInMs);
                    if(ret < 0)
                        pLogWarning("Failed to fade-in the music [%s]: %s", p.c_str(), Mix_GetError());
                }
                else
                {
                    ret = Mix_PlayMusic(g_curMusic, -1);
                    if(ret < 0)
                        pLogWarning("Failed to play the music [%s]: %s", p.c_str(), Mix_GetError());
                }
            }

            if(ret >= 0)
            {
                g_stats.currentMusic = Mix_GetMusicTitle(g_curMusic);
                g_stats.currentMusicFile = CustomMusic[A];
            }
        }
        else
        {
            pLogDebug("Starting level music [%s]", mus.c_str());
            PlayMusic(mus);
        }
        musicName = std::move(mus);
    }

    s_recentMusicA = A;
    musicPlaying = true;
}

void PauseMusic()
{
    if(!musicPlaying || !g_mixerLoaded)
        return;

    if(g_curMusic && Mix_PlayingMusicStream(g_curMusic))
        Mix_PauseMusicStream(g_curMusic);
}

void ResumeMusic()
{
    if(!musicPlaying || !g_mixerLoaded)
        return;

    if(g_curMusic && Mix_PausedMusicStream(g_curMusic))
        Mix_ResumeMusicStream(g_curMusic);
}

void StopMusic()
{
    if(!musicPlaying || !g_mixerLoaded)
        return;

    pLogDebug("Stopping music");

    if(g_curMusic)
    {
        Mix_HaltMusicStream(g_curMusic);
        Mix_FreeMusic(g_curMusic);
    }
    g_curMusic = nullptr;
    musicPlaying = false;
    s_recentMusicA = s_null_music;
    g_stats.currentMusic.clear();
    g_stats.currentMusicFile.clear();
}

void FadeOutMusic(int ms)
{
    if(!musicPlaying || !g_mixerLoaded)
        return;

    pLogDebug("Fading out music");
    if(g_curMusic)
        Mix_FadeOutMusicStream(g_curMusic, ms);
    musicPlaying = false;
    s_recentMusicA = s_null_music;
}

void UpdateMusicVolume()
{
    if(!musicPlaying || !g_mixerLoaded)
        return;

    if(g_curMusic)
    {
        if((int)g_config.audio_mus_volume == 0)
            StartMusic(s_recentMusicA); // will actually STOP it
        else
            Mix_VolumeMusicStream(g_curMusic, s_musicDefaultVolume * g_config.audio_mus_volume / 100);
    }
    else if(s_recentMusicA != s_null_music)
        StartMusic(s_recentMusicA); // will restart it
}

void PlayInitSound()
{
    MusicRoot = AppPath + "music/";
    SfxRoot = AppPath + "sound/";

    // std::string doSound = AppPath + "sound/";
    IniProcessing sounds = Files::load_ini(AppPath + "sounds.ini");
    unsigned int totalSounds;
    sounds.beginGroup("sound-main");
    sounds.read("total", totalSounds, 0);
    sounds.endGroup();

    if(totalSounds >= 29)
    {
        std::string p;
        sounds.beginGroup("sound-29");
        sounds.read("file", p, std::string());
        sounds.endGroup();

        if(!p.empty())
        {
            Mix_Music *loadsfx = Mix_LoadMUS((SfxRoot + p).c_str());
            if(loadsfx)
            {
                if(Mix_PlayMusicStream(loadsfx, 0) < 0)
                    Mix_FreeMusic(loadsfx);
                else
                    Mix_SetFreeOnStop(loadsfx, 1);
            }
        }
    }
}

static void loadMusicIni(SoundScope root, const std::string &path, bool isLoadingCustom)
{
    IniProcessing musicSetup = Files::load_ini(path);
    if(!isLoadingCustom)
    {
        music.clear();
        g_totalMusicLevel = 0;
        g_totalMusicWorld = 0;
        g_totalMusicSpecial = 0;
        musicSetup.beginGroup("music-main");
        musicSetup.read("total-level", g_totalMusicLevel, 0);
        musicSetup.read("total-world", g_totalMusicWorld, 0);
        musicSetup.read("total-special", g_totalMusicSpecial, 0);
        musicSetup.read("level-custom-music-id", g_customLvlMusicId, 0);
        musicSetup.read("world-custom-music-id", g_customWldMusicId, 0);
        musicSetup.endGroup();

        if(LoadingInProcess)
            UpdateLoad();
    }

    for(unsigned int i = 1; i <= g_totalMusicLevel; ++i)
    {
        std::string alias = fmt::format_ne("music{0}", i);
        std::string group = fmt::format_ne("level-music-{0}", i);
        AddMusic(root, musicSetup, alias, group, 52);
    }

    if(!isLoadingCustom && LoadingInProcess)
        UpdateLoad();

    for(unsigned int i = 1; i <= g_totalMusicWorld; ++i)
    {
        std::string alias = fmt::format_ne("wmusic{0}", i);
        std::string group = fmt::format_ne("world-music-{0}", i);
        AddMusic(root, musicSetup, alias, group, 64);
    }

    if(!isLoadingCustom && LoadingInProcess)
        UpdateLoad();

    for(unsigned int i = 1; i <= g_totalMusicSpecial; ++i)
    {
        std::string alias = fmt::format_ne("smusic{0}", i);
        if(i == 1)
            alias = "smusic";
        else if(i == 2)
            alias = "stmusic";
        else if(i == 3)
            alias = "tmusic";
        std::string group = fmt::format_ne("special-music-{0}", i);
        AddMusic(root, musicSetup, alias, group, 64);
    }
}

#ifdef THEXTECH_ENABLE_AUDIO_FX
static void readFx(IniProcessing &sounds, SectionEffect_t &s)
{
    const IniProcessing::StrEnumMap fxType =
    {
        {"none", SectionEffect_t::FX_None},
        {"reverb", SectionEffect_t::FX_Reverb},
        {"echo", SectionEffect_t::FX_Echo}
    };

    sounds.readEnum("fx", s.fx, (int)SectionEffect_t::FX_None, fxType);
    sounds.read("spc-echo-off", s.disableSpcEcho, false);

    switch(s.fx)
    {
    case SectionEffect_t::FX_Reverb:
        sounds.read("mode", s.rev.mode, 0.0f);
        sounds.read("room-size", s.rev.roomSize, 0.7f);
        sounds.read("damping", s.rev.damping, 0.5f);
        sounds.read("wet-level", s.rev.wetLevel, 0.2f);
        sounds.read("dry-level", s.rev.dryLevel, 0.4f);
        sounds.read("width", s.rev.width, 1.0f);
        break;

    case SectionEffect_t::FX_Echo:
        sounds.read("echo-on", s.echo.echoOn, 1);
        sounds.read("delay", s.echo.echoDelay, 6);
        sounds.read("feedback", s.echo.echoFeedBack, 30);
        sounds.read("main-volume-left", s.echo.echoMainVolL, 127);
        sounds.read("main-volume-right", s.echo.echoMainVolR, 127);
        sounds.read("echo-volume-left", s.echo.echoVolL, 28);
        sounds.read("echo-volume-right", s.echo.echoVolR, 28);
        sounds.read("fir-0", s.echo.echoFir[0], 99);
        sounds.read("fir-1", s.echo.echoFir[1], -52);
        sounds.read("fir-2", s.echo.echoFir[2], 32);
        sounds.read("fir-3", s.echo.echoFir[3], 50);
        sounds.read("fir-4", s.echo.echoFir[4], 25);
        sounds.read("fir-5", s.echo.echoFir[5], 51);
        sounds.read("fir-6", s.echo.echoFir[6], -35);
        sounds.read("fir-7", s.echo.echoFir[7], 56);
        break;

    default:
    case SectionEffect_t::FX_None:
        break;
    }
}
#endif // THEXTECH_ENABLE_AUDIO_FX

static void loadCustomSfxIni(SoundScope root, const std::string &path)
{
    IniProcessing sounds = Files::load_ini(path);
    for(unsigned int i = 1; i <= g_totalSounds; ++i)
    {
        int alias = i;
        std::string group = fmt::format_ne("sound-{0}", i);
        AddSfx(root, sounds, alias, group, true);
    }

#ifdef THEXTECH_ENABLE_AUDIO_FX
    auto ch = sounds.childGroups();
    for(const auto &g : ch)
    {
        if(g.find("fx-") == 0)
        {
            sounds.beginGroup(g);
            SectionEffect_t fx;
            auto e = g;
            e.erase(e.begin(), e.begin() + 3);
            readFx(sounds, fx);
            s_effectsList.insert({e, fx});
            sounds.endGroup();
        }
    }

    // FX settings
    for(int i = 0; i <= maxSections; ++i)
    {
        auto &s = s_sectionEffect[i];
        std::string cfx;

        sounds.beginGroup(fmt::format_ne("section-fx-{0}", i));
        sounds.read("custom-fx", cfx, std::string());
        if(!cfx.empty() && s_effectsList.find(cfx) != s_effectsList.end())
            s = s_effectsList[cfx];
        else
            readFx(sounds, s);
        sounds.endGroup();
    }
#endif // THEXTECH_ENABLE_AUDIO_FX
}

static void restoreDefaultSfx()
{
    for(auto &s : sound)
    {
        auto &u = s.second;
        RestoreSfx(u);
    }

#ifdef THEXTECH_ENABLE_AUDIO_FX
    s_effectsList.clear();

    for(int i = 0; i <= maxSections; ++i)
    {
        s_sectionEffect[i].fx = SectionEffect_t::FX_None;
        s_sectionEffect[i].rev = SoundFXReverb();
        s_sectionEffect[i].echo = SoundFXEchoSetup();
    }
    SoundFX_Clear();
#endif
}

void InitSound()
{
    if(!g_mixerLoaded)
        return;

    uint32_t start_time = SDL_GetTicks();

    MusicRoot = AppPath + "music/";
    SfxRoot = AppPath + "sound/";

    musicIni = AppPath + "music.ini";
    sfxIni = AppPath + "sounds.ini";


    if(LoadingInProcess)
    {
        LoaderUpdateDebugString("Sound configs");
        UpdateLoad();
    }

    if(!Files::fileExists(musicIni) && !Files::fileExists(sfxIni))
    {
        pLogWarning("music.ini and sounds.ini are missing");
        XMsgBox::simpleMsgBox(XMsgBox::MESSAGEBOX_ERROR,
                     "music.ini and sounds.ini are missing",
                     "Files music.ini and sounds.ini are not exist, game will work without default music and SFX.");
        g_customLvlMusicId = 24;
        g_customWldMusicId = 17;
        return;
    }
    else if(!Files::fileExists(musicIni))
    {
        pLogWarning("music.ini is missing");
        XMsgBox::simpleMsgBox(XMsgBox::MESSAGEBOX_ERROR,
                     "music.ini is missing",
                     "File music.ini is not exist, game will work without default music.");
    }
    else if(!Files::fileExists(sfxIni))
    {
        pLogWarning("sounds.ini is missing");
        XMsgBox::simpleMsgBox(XMsgBox::MESSAGEBOX_ERROR,
                     "sounds.ini is missing",
                     "File sounds.ini is not exist, game will work without SFX.");
    }

    loadMusicIni(SoundScope::global, musicIni, false);

    if(LoadingInProcess)
        UpdateLoad();
    else
        IndicateProgress(start_time, 0.01, "");

    IniProcessing sounds = Files::load_ini(sfxIni);
    sounds.beginGroup("sound-main");
    sounds.read("total", g_totalSounds, 0);
    sounds.read("use-iceball-sfx", s_useIceBallSfx, false);
    sounds.read("use-new-ice-sfx", s_useNewIceSfx, false);
    bool playerUseNPCHammer;
    bool playerUseOwnHammer;
    sounds.read("player-use-npc-hammer-sfx", playerUseNPCHammer, false);
    sounds.read("player-use-own-hammer-sfx", playerUseOwnHammer, false);
    sounds.endGroup();

    if(playerUseOwnHammer)
        playerHammerSFX = SFX_PlayerHeavy;
    else if(playerUseNPCHammer)
        playerHammerSFX = SFX_Throw;
    else
        playerHammerSFX = SFX_Fireball;

    if(LoadingInProcess)
        UpdateLoad();
    else
        IndicateProgress(start_time, 0.75 / g_totalSounds, "");

    for(unsigned int i = 1; i <= g_totalSounds; ++i)
    {
        int alias = i;
        std::string group = fmt::format_ne("sound-{0}", i);
        AddSfx(SoundScope::global, sounds, alias, group);

        if(!LoadingInProcess)
            IndicateProgress(start_time, (double)i / g_totalSounds, "");
#ifdef PGE_NO_THREADING
        else
            UpdateLoad();
#endif

        if(!GameIsActive)
            return;
    }

    if(LoadingInProcess)
    {
        LoaderUpdateDebugString("All sounds loaded");
        UpdateLoad();
    }

    Mix_ReserveChannels(g_reservedChannels);

    if(g_errorsSfx > 0 && GameIsActive)
    {
        XMsgBox::simpleMsgBox(XMsgBox::MESSAGEBOX_ERROR,
                              "Sounds loading error",
                              fmt::format_ne("Failed to load some SFX assets. Loo a log file to get more details:\n{0}", getLogFilePath()));
        g_errorsSfx = 0;
    }

    // Print the stats of loaded sound files
    int statSfxDump = 0;
    int statSfxAsMusic = 0;

    for(auto & it : sound)
    {
        auto &s = it.second;

        if(s.music)
            statSfxAsMusic++;

        if(s.chunk)
            statSfxDump++;
    }

    pLogInfo("Loaded sound effects: dumped=%d; as music=%d", statSfxDump, statSfxAsMusic);
}

void UnloadSound()
{
    restoreDefaultSfx();

    UnloadExtSounds();

    if(g_curMusic)
        Mix_FreeMusic(g_curMusic);
    g_curMusic = nullptr;
    g_reservedChannels = 0;

    for(auto & it : sound)
    {
        auto &s = it.second;
        if(s.chunk)
            Mix_FreeChunk(s.chunk);
        if(s.chunkOrig)
            Mix_FreeChunk(s.chunkOrig);
        if(s.music)
        {
            Mix_HaltMusicStream(s.music);
            Mix_FreeMusic(s.music);
        }
        if(s.musicOrig)
            Mix_FreeMusic(s.musicOrig);
    }

    sound.clear();
    music.clear();
}

static const std::unordered_map<int, int> s_soundDelays =
{
    {2, 12}, {3, 12},  {4, 12},  {5, 30}, {8, 10},  {9, 4},
    {10, 8}, {12, 10}, {17, 10}, {26, 8}, {31, 20}, {37, 10},
    {42, 16},{50, 8},  {54, 8},  {71, 9}, {74, 8},  {81, 5},
    {86, 8}, {SFX_Icebreak, 4}
};

static void s_resetSoundDelay(int A)
{
    // set the delay before a sound can be played again
    auto i = s_soundDelays.find(A);
    if(i == s_soundDelays.end())
        SoundPause[A] = 4;
    else
        SoundPause[A] = i->second;

#if 0 // Very old code, replaced with a more flexible thing at above
    switch(A)
    {
    case 2: SoundPause[A] = 12; break;
    case 3: SoundPause[A] = 12; break;
    case 4: SoundPause[A] = 12; break;
    case 5: SoundPause[A] = 30; break;
    case 8: SoundPause[A] = 10; break;
    case 9: SoundPause[A] = 4; break;
    case 10: SoundPause[A] = 8; break;
    case 12: SoundPause[A] = 10; break;
    case 17: SoundPause[A] = 10; break;
    case 26: SoundPause[A] = 8; break;
    case 31: SoundPause[A] = 20; break;
    case 37: SoundPause[A] = 10; break;
    case 42: SoundPause[A] = 16; break;
    case 50: SoundPause[A] = 8; break;
    case 54: SoundPause[A] = 8; break;
    case 71: SoundPause[A] = 9; break;
    case 74: SoundPause[A] = 8; break;
    case 81: SoundPause[A] = 5; break;
    case 86: SoundPause[A] = 8; break;
    default: SoundPause[A] = 4; break;
    }
#endif
}

static const std::unordered_map<int, int> s_soundFallback =
{
    {SFX_Iceball, SFX_Fireball},
    {SFX_Freeze, SFX_ShellHit},
    {SFX_Icebreak, SFX_ShellHit},
    {SFX_SproutVine, SFX_ItemEmerge},
    {SFX_FireBossKilled, SFX_SickBossKilled},
    {SFX_HeroIce, SFX_HeroFire},
    {SFX_HeroFireRod, SFX_HeroFire},
    {SFX_FlameThrower, SFX_HeroFire},
    {SFX_PlayerHeavy, SFX_Fireball},
};

static int getFallbackSfx(int A)
{
    auto fb = s_soundFallback.find(A);
    if(fb != s_soundFallback.end())
        A = fb->second;
    return A;
}

void PlaySound(int A, int loops, int volume)
{
    if(!g_mixerLoaded)
        return;

    if(g_totalSounds == 0)
    {
        playFallbackSfx(A, loops, volume);
        return;
    }

    if(GameMenu || GameOutro) // || A == 26 || A == 27 || A == 29)
        return;

    if(A > (int)g_totalSounds || !g_config.sfx_modern) // Play fallback sound for the missing SFX
        A = getFallbackSfx(A);
    else if(!s_useIceBallSfx && A == SFX_Iceball)
        A = SFX_Fireball; // Fell back into fireball when iceball sound isn't preferred
    else if(!s_useIceBallSfx && A == SFX_HeroIce)
        A = SFX_HeroFire;
    else if(!s_useNewIceSfx && (A == SFX_Freeze || A == SFX_Icebreak))
        A = SFX_ShellHit; // Restore the old behavior

    if(g_ClonedPlayerMode)
        SoundPause[SFX_Skid] = 1;

    if(SoundPause[A] == 0) // if the sound wasn't just played
    {
        int alias = A;
        PlaySfx(alias, loops, volume);
        s_resetSoundDelay(A);
    }
}

void PlaySoundSpatial(int A, int l, int t, int r, int b, int loops, int volume)
{
    if(!g_mixerLoaded)
        return;

    if(GameMenu || GameOutro) // || A == 26 || A == 27 || A == 29)
        return;

    if(A > (int)g_totalSounds || !g_config.sfx_modern) // Play fallback sound for the missing SFX
        A = getFallbackSfx(A);
    else if(!s_useIceBallSfx && A == SFX_Iceball)
        A = SFX_Fireball; // Fell back into fireball when iceball sound isn't preferred
    else if(!s_useIceBallSfx && A == SFX_HeroIce)
        A = SFX_HeroFire;
    else if(!s_useNewIceSfx && (A == SFX_Freeze || A == SFX_Icebreak))
        A = SFX_ShellHit; // Restore the old behavior

    if(g_ClonedPlayerMode)
        SoundPause[SFX_Skid] = 1;

    if(SoundPause[A] == 0) // if the sound wasn't just played
    {
        uint8_t left = 255, right = 255;

        if(g_config.sfx_spatial_audio)
            Sound_ResolveSpatialMod(left, right, l, t, r, b);

        int alias = A;
        PlaySfx(alias, loops, volume, left, right);
        s_resetSoundDelay(A);
    }
}

bool HasSound(int A)
{
    return A <= (int)g_totalSounds;
}

void PlaySoundMenu(int A, int loops)
{
    if(SoundPause[A] > 0) // if the sound wasn't just played
        return;

    if(g_totalSounds == 0 || A > (int)g_totalSounds)
    {
        playFallbackSfx(A, loops, 128);
        return;
    }

    int alias = A;
    PlaySfx(alias, loops);
    s_resetSoundDelay(A);
}

#if defined(THEXTECH_ASSERTS_INGAME_MESSAGE) && !defined(THEXTECH_NO_SDL_BUILD)
void PlayErrorSound(int A, int loops)
{
    playFallbackSfx(A, loops, 128);
}
#endif

// stops all sound from being played for 10 cycles
void BlockSound()
{
    For(A, 1, numSounds)
    {
        SoundPause[A] = 10;
    }
}

void UpdateSound()
{
    if(!g_mixerLoaded)
        return;

    For(A, 1, numSounds)
    {
        if(SoundPause[A] > 0)
            SoundPause[A] -= 1;
    }
}

void LoadCustomSound()
{
    if(!g_mixerLoaded)
        return;

    if(GameMenu || GameOutro)
        return; // Don't load custom music in menu mode

    // To avoid bugs like custom local sounds was transferred into another level, it's need to clean-up old one if that was
    if(g_customMusicInDataFolder)
    {
        loadMusicIni(SoundScope::global, musicIni, true);
        g_customMusicInDataFolder = false;
    }

    if(g_customSoundsInDataFolder)
    {
        restoreDefaultSfx();
        g_customSoundsInDataFolder = false;
    }

    if(FileNamePath == AppPath)
        return; // Don't treat default music/sounds ini as custom

    std::string mIni = g_dirEpisode.resolveFileCaseExistsAbs("music.ini");
    if(!mIni.empty()) // Load music.ini from an episode folder
        loadMusicIni(SoundScope::episode, mIni, true);

    std::string mIniC = g_dirCustom.resolveFileCaseExistsAbs("music.ini");
    if(!mIniC.empty()) // Load music.ini from a level/world custom folder
    {
        loadMusicIni(SoundScope::custom, mIniC, true);
        g_customMusicInDataFolder = true;
    }

    std::string sIni = g_dirEpisode.resolveFileCaseExistsAbs("sounds.ini");
    if(!sIni.empty()) // Load sounds.ini from an episode folder
        loadCustomSfxIni(SoundScope::episode, sIni);

    std::string sIniC = g_dirCustom.resolveFileCaseExistsAbs("sounds.ini");
    if(!sIniC.empty()) // Load sounds.ini from a level/world custom folder
    {
        loadCustomSfxIni(SoundScope::custom, sIniC);
        g_customSoundsInDataFolder = true;
    }
}

void UnloadCustomSound()
{
    if(!g_mixerLoaded)
        return;

    loadMusicIni(SoundScope::global, musicIni, true);
    restoreDefaultSfx();
    g_customMusicInDataFolder = false;
    g_customSoundsInDataFolder = false;

    UnloadExtSounds();
}

void UpdateYoshiMusic()
{
    if(!s_musicHasYoshiMode || !g_mixerLoaded)
        return;

    bool hasYoshi = false;

    if(!g_config.sfx_pet_beat)
        hasYoshi = false;
    else
    {
        for(int i = 1; i <= numPlayers; ++i)
            hasYoshi |= (Player[i].Mount == 3);
    }

    Mix_SetMusicTrackMute(g_curMusic, s_musicYoshiTrackNumber, hasYoshi ? 0 : 1);
}

void PreloadExtSound(const std::string& path)
{
    if(!g_mixerLoaded)
        return;

    auto f = extSfx.find(path);
    if(f == extSfx.end())
    {
        auto *ch = Mix_LoadWAV(path.c_str());
        if(!ch)
        {
            pLogWarning("Can't load custom sound [%s]: %s", path.c_str(), Mix_GetError());
            return;
        }
        extSfx.insert({path, ch});
    }
}

void UnloadExtSounds()
{
    if(!g_mixerLoaded)
        return;

    SDL_AtomicSet(&extSfxBusy, 1);

    for(auto &f : extSfx)
        Mix_FreeChunk(f.second);

    extSfx.clear();
    extSfxPlaying.clear();

    SDL_AtomicSet(&extSfxBusy, 0);
}

void PlayExtSound(const std::string &path, int loops, int volume)
{
    int play_ch = -1;

    if(!g_mixerLoaded || (int)g_config.audio_sfx_volume == 0)
        return;

    auto f = extSfx.find(path);
    if(f == extSfx.end())
    {
        auto *ch = Mix_LoadWAV(path.c_str());
        if(!ch)
        {
            pLogWarning("Can't load custom sound: %s", Mix_GetError());
            return;
        }

        extSfx.insert({path, ch});
        play_ch = Mix_PlayChannelVol(-1, ch, loops, volume * g_config.audio_sfx_volume / 100);
    }
    else
        play_ch = Mix_PlayChannelVol(-1, f->second, loops, volume * g_config.audio_sfx_volume / 100);

    if(play_ch >= 0)
    {
        SDL_AtomicSet(&extSfxBusy, 1);
        // Never re-use the same channel!
        SDL_assert_release(extSfxPlaying.find(play_ch) == extSfxPlaying.end());
        extSfxPlaying.insert({play_ch, path});
        SDL_AtomicSet(&extSfxBusy, 0);
    }
    else
        pLogWarning("Can't play custom sound %s: %s", Mix_GetError());
}

static void extSfxStopCallback(int channel)
{
    if(SDL_AtomicGet(&extSfxBusy) == 1)
        return; // Do nothing!

    auto i = extSfxPlaying.find(channel);
    if(i != extSfxPlaying.end())
        extSfxPlaying.erase(i);
}

void StopExtSound(const std::string& path)
{
    if(!g_mixerLoaded)
        return;

    SDL_AtomicSet(&extSfxBusy, 1);

    for(auto i = extSfxPlaying.begin(); i != extSfxPlaying.end();)
    {
        if(i->second == path)
        {
            Mix_HaltChannel(i->first);
            i = extSfxPlaying.erase(i);
        }
        else
            ++i;
    }

    SDL_AtomicSet(&extSfxBusy, 0);
}

void StopAllExtSounds()
{
    if(!g_mixerLoaded)
        return;

    SDL_AtomicSet(&extSfxBusy, 1);

    for(auto i = extSfxPlaying.begin(); i != extSfxPlaying.end(); ++i)
        Mix_HaltChannel(i->first);

    extSfxPlaying.clear();

    SDL_AtomicSet(&extSfxBusy, 0);
}

void StopAllSounds()
{
    if(!g_mixerLoaded)
        return;

    SDL_AtomicSet(&extSfxBusy, 1);
    Mix_HaltChannel(-1);
    extSfxPlaying.clear();
    SDL_AtomicSet(&extSfxBusy, 0);
}

#ifdef THEXTECH_ENABLE_AUDIO_FX

static bool     enableEffectEcho = false;
static SpcEcho *effectEcho = nullptr;

static void echoEffectDone(int, void *context)
{
    SpcEcho *out = reinterpret_cast<SpcEcho *>(context);
    if(out == effectEcho)
    {
        echoEffectFree(effectEcho);
        effectEcho = nullptr;
        enableEffectEcho = false;
    }
}

void SoundFX_SetEcho(const SoundFXEchoSetup& setup)
{
    if(!g_mixerLoaded)
        return;

    bool isNew = false;

    // Clear previously installed effects first
    if(!effectEcho)
    {
        SoundFX_Clear();

        effectEcho = echoEffectInit(g_config.audio_sample_rate.obtained,
                                    g_config.audio_format.obtained,
                                    g_config.audio_channels.obtained);
        isNew = true;
    }

    if(effectEcho)
    {
        echoEffectSetReg(effectEcho, ECHO_EON, setup.echoOn);
        echoEffectSetReg(effectEcho, ECHO_EDL, setup.echoDelay);
        echoEffectSetReg(effectEcho, ECHO_EFB, setup.echoFeedBack);

        echoEffectSetReg(effectEcho, ECHO_MVOLL, setup.echoMainVolL);
        echoEffectSetReg(effectEcho, ECHO_MVOLR, setup.echoMainVolR);
        echoEffectSetReg(effectEcho, ECHO_EVOLL, setup.echoVolL);
        echoEffectSetReg(effectEcho, ECHO_EVOLR, setup.echoVolR);

        echoEffectSetReg(effectEcho, ECHO_FIR0, setup.echoFir[0]);
        echoEffectSetReg(effectEcho, ECHO_FIR1, setup.echoFir[1]);
        echoEffectSetReg(effectEcho, ECHO_FIR2, setup.echoFir[2]);
        echoEffectSetReg(effectEcho, ECHO_FIR3, setup.echoFir[3]);
        echoEffectSetReg(effectEcho, ECHO_FIR4, setup.echoFir[4]);
        echoEffectSetReg(effectEcho, ECHO_FIR5, setup.echoFir[5]);
        echoEffectSetReg(effectEcho, ECHO_FIR6, setup.echoFir[6]);
        echoEffectSetReg(effectEcho, ECHO_FIR7, setup.echoFir[7]);
        if(isNew)
            Mix_RegisterEffect(MIX_CHANNEL_POST, spcEchoEffect, echoEffectDone, effectEcho);
        enableEffectEcho = true;
    }
}

static bool enableEffectReverb = false;
static FxReverb *effectReverb = nullptr;

static void reverbEffectDone(int, void *context)
{
    FxReverb *out = reinterpret_cast<FxReverb *>(context);
    if(out == effectReverb)
    {
        reverbEffectFree(effectReverb);
        effectReverb = nullptr;
        enableEffectReverb = false;
    }
}

void SoundFX_SetReverb(const SoundFXReverb& setup)
{
    if(!g_mixerLoaded)
        return;

    bool isNew = false;

    if(!effectReverb)
    {
        // Clear previously installed effects first
        SoundFX_Clear();

        effectReverb = reverbEffectInit(g_config.audio_sample_rate.obtained,
                                        g_config.audio_format.obtained,
                                        g_config.audio_channels.obtained);
        isNew = true;
    }

    if(effectReverb)
    {
        ReverbSetup set;
        set.mode = setup.mode;
        set.roomSize = setup.roomSize;
        set.damping = setup.damping;
        set.wetLevel = setup.wetLevel;
        set.dryLevel = setup.dryLevel;
        set.width = setup.width;
        reverbUpdateSetup(effectReverb, set);
        if(isNew)
            Mix_RegisterEffect(MIX_CHANNEL_POST, reverbEffect, reverbEffectDone, effectReverb);
        enableEffectReverb = true;
    }
}

void SoundFX_Clear()
{
    if(!g_mixerLoaded)
        return;

    if(effectEcho)
    {
        Mix_UnregisterEffect(MIX_CHANNEL_POST, spcEchoEffect);
        if(effectEcho)
        {
            echoEffectFree(effectEcho);
            effectEcho = nullptr;
        }
        enableEffectEcho = false;
    }

    if(effectReverb)
    {
        Mix_UnregisterEffect(MIX_CHANNEL_POST, reverbEffect);
        if(effectReverb)
        {
            reverbEffectFree(effectReverb);
            effectReverb = nullptr;
        }
        enableEffectReverb = false;
    }
}

#endif // THEXTECH_ENABLE_AUDIO_FX


void ResetSoundFX()
{
#ifdef THEXTECH_ENABLE_AUDIO_FX
    SoundFX_Clear();
    s_musicDisableSpcEcho = false;
#endif
}

void UpdateSoundFX(int recentSection)
{
#ifndef THEXTECH_ENABLE_AUDIO_FX
    UNUSED(recentSection);
#else
    if(!g_mixerLoaded || LevelSelect)
        return;

    SDL_assert_release(recentSection >= 0 && recentSection <= maxSections);
    const auto &s = g_config.sfx_audio_fx ? s_sectionEffect[recentSection] : SectionEffect_t();

    s_musicDisableSpcEcho = s.disableSpcEcho;
    if(g_curMusic)
        Mix_GME_SetSpcEchoDisabled(g_curMusic, s.disableSpcEcho);

    switch(s.fx)
    {
    default:
    case SectionEffect_t::FX_None:
        SoundFX_Clear();
        break;

    case SectionEffect_t::FX_Echo:
        SoundFX_SetEcho(s.echo);
        break;

    case SectionEffect_t::FX_Reverb:
        SoundFX_SetReverb(s.rev);
        break;
    }
#endif // THEXTECH_ENABLE_AUDIO_FX
}
