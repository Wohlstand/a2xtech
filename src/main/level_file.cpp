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
#include "../game_main.h"
#include "../npc.h"
#include "../load_gfx.h"
#include "../custom.h"
#include "../sound.h"
#include "../sorting.h"
#include "../layers.h"
#include "../compat.h"
#include "../graphics.h"
#include "../npc_id.h"
#include "level_file.h"

#include <DirManager/dirman.h>
#include <Utils/files.h>
#include <Utils/strings.h>
#include <Utils/dir_list_ci.h>
#include <Logger/logger.h>
#include <PGE_File_Formats/file_formats.h>


void bgoApplyZMode(Background_t *bgo, int smbx64sp)
{
    if(bgo->zMode == LevelBGO::ZDefault)
        bgo->SortPriority = smbx64sp;
    else
    {
        switch(bgo->zMode)
        {
        case LevelBGO::Background2:
            bgo->SortPriority = 10;
            break;
        case LevelBGO::Background1:
            bgo->SortPriority = 30;
            break;
        case LevelBGO::Foreground1:
            bgo->SortPriority = 125;
            break;
        case LevelBGO::Foreground2:
            bgo->SortPriority = 200;
            break;
        default:
            break;
        }
    }
}

void addMissingLvlSuffix(std::string &fileName)
{
    if(!fileName.empty() && !Files::hasSuffix(fileName, ".lvl") && !Files::hasSuffix(fileName, ".lvlx"))
    {
        bool isAbsolute = Files::isAbsolute(fileName);
        bool lvlxExists;

        if(isAbsolute)
            lvlxExists = Files::fileExists(fileName + ".lvlx");
        else
            lvlxExists = Files::fileExists(FileNamePath + fileName + ".lvlx");

        if(lvlxExists)
            fileName += ".lvlx";
        else
            fileName += ".lvl";
    }
}


bool OpenLevel(std::string FilePath)
{
    addMissingLvlSuffix(FilePath);
//    if(!Files::hasSuffix(FilePath, ".lvl") && !Files::hasSuffix(FilePath, ".lvlx"))
//    {
//        if(Files::fileExists(FilePath + ".lvlx"))
//            FilePath += ".lvlx";
//        else
//            FilePath += ".lvl";
//    }

    LevelData lvl;
    if(!FileFormats::OpenLevelFile(FilePath, lvl))
    {
        pLogWarning("Error of level \"%s\" file loading: %s (line %d).",
                    FilePath.c_str(),
                    lvl.meta.ERROR_info.c_str(),
                    lvl.meta.ERROR_linenum);
        return false;
    }

    return OpenLevelData(lvl, FilePath);
}

bool OpenLevelData(LevelData &lvl, const std::string FilePath)
{
    std::string newInput;
//    int FileRelease = 0;
    int A = 0;
    int B = 0;
    int checkPointId = 0;
//    int C = 0;
//    bool tempBool = false;
//    int mSections = 0;
//    Location_t tempLocation;

    DirListCI dirEpisode;

    qScreen = false;
    ClearLevel();
    BlockSound();
    FreezeNPCs = false;
    CoinMode = false;

    FileFormats::smbx64LevelPrepare(lvl);
    FileFormats::smbx64LevelSortBlocks(lvl);
    FileFormats::smbx64LevelSortBGOs(lvl);

    dirEpisode.setCurDir(lvl.meta.path);
    FileName = dirEpisode.resolveDirCase(lvl.meta.filename);
    FileNamePath = lvl.meta.path + "/";

    bool compatModern = (CompatGetLevel() == COMPAT_MODERN);
    bool isSmbx64 = (lvl.meta.RecentFormat == LevelData::SMBX64);
    int  fVersion = lvl.meta.RecentFormatVersion;

    if(!FilePath.empty())
    {
        FileNameFull = Files::basename(FilePath);
        FullFileName = FilePath;
    }
    else
    {
        FileNameFull = FileName + ".lvlx";
        FullFileName = FileNamePath + FileName + ".lvlx";
    }

    IsEpisodeIntro = (StartLevel == FileNameFull);

    numBlock = 0;
    numBackground = 0;
    numLocked = 0;
    numNPCs = 0;
    numWarps = 0;

    numLayers = 0;
    numEvents = 0;

// Load Custom Stuff
//    if(DirMan::exists(FileNamePath + FileName))
//        FindCustomNPCs(FileNamePath + FileName);
//    else
    LoadCustomCompat();
    FindCustomPlayers();
    FindCustomNPCs();
    LoadCustomGFX();
    LoadCustomSound();
//    if(DirMan::exists(FileNamePath + FileName)) // Useless now
//        LoadCustomGFX2(FileNamePath + FileName);
// Blah

    if(FilePath == ".lvl" || FilePath == ".lvlx")
        return false;

    maxStars = lvl.stars;
    LevelName = lvl.LevelName;

    numSections = 0;
    B = 0;
    for(auto & s : lvl.sections)
    {
        level[B].X = double(s.size_left);
        level[B].Y = double(s.size_top);
        level[B].Height = double(s.size_bottom);
        level[B].Width = double(s.size_right);
        LevelREAL[B] = level[B];
        bgMusic[B] = int(s.music_id);
        bgMusicREAL[B] = bgMusic[B];
        bgColor[B] = s.bgcolor;
        LevelWrap[B] = s.wrap_h;
        LevelVWrap[B] = s.wrap_v; // EXTRA
        OffScreenExit[B] = s.OffScreenEn;
        Background2[B] = int(s.background);
        Background2REAL[B] = Background2[B];
        NoTurnBack[B] = s.lock_left_scroll;
        UnderWater[B] = s.underwater;
        CustomMusic[B] = dirEpisode.resolveFileCase(s.music_file);
        B++;
        if(B > maxSections)
            break;
        numSections++;
    }

    for(A = 1; A <= 2; A++) // Fill with zeroes
    {
        PlayerStart[A].X = 0;
        PlayerStart[A].Y = 0;
        PlayerStart[A].Width = 0;
        PlayerStart[A].Height = 0;
    }

    A = 1;
    for(auto &p : lvl.players)
    {
        PlayerStart[A].X = double(p.x);
        PlayerStart[A].Y = double(p.y);
        PlayerStart[A].Width = double(p.w);
        PlayerStart[A].Height = double(p.h);
        PlayerStart[A].Direction = p.direction;
        A++;
        if(A > 2)
            break;
    }


    for(auto &b : lvl.blocks)
    {
        numBlock++;
        if(numBlock > maxBlocks)
        {
            numBlock = maxBlocks;
            break;
        }

        auto &block = Block[numBlock];

        block = Block_t();

        block.Location.X = double(b.x);
        block.Location.Y = double(b.y);
        block.Location.Height = double(b.h);
        block.Location.Width = double(b.w);
        block.Type = int(b.id);
        block.DefaultType = block.Type;

        block.Special = int(b.npc_id > 0 ? b.npc_id + 1000 : -1 * b.npc_id);
        if(block.Special == 100)
            block.Special = 1009;
        if(block.Special == 102)
            block.Special = 1014;
        if(block.Special == 103)
            block.Special = 1034;
        if(block.Special == 105)
            block.Special = 1095;
        block.DefaultSpecial = block.Special;

        block.Special2 = 0;
        if(b.id == 90 && lvl.meta.RecentFormat == LevelData::SMBX64 && lvl.meta.RecentFormatVersion < 20)
            block.Special2 = 1; // Restore bricks algorithm for turn blocks for SMBX19 and lower
        block.DefaultSpecial2 = block.Special2;

        block.Invis = b.invisible;
        block.Slippy = b.slippery;
        block.Layer = b.layer;
        block.TriggerDeath = b.event_destroy;
        block.TriggerHit = b.event_hit;
        block.TriggerLast = b.event_emptylayer;

        if(IF_OUTRANGE(block.Type, 0, maxBlockType)) // Drop ID to 1 for blocks of out of range IDs
        {
            pLogWarning("Block-%d ID is out of range (max types %d), reset to Block-1", block.Type, maxBlockType);
            block.Type = 1;
        }
    }

    for(auto &b : lvl.bgo)
    {
        numBackground++;
        if(numBackground > maxBackgrounds)
        {
            numBackground = maxBackgrounds;
            break;
        }

        auto &bgo = Background[numBackground];

        bgo = Background_t();

        bgo.Location.X = double(b.x);
        bgo.Location.Y = double(b.y);
        bgo.Type = int(b.id);

        if(IF_OUTRANGE(bgo.Type, 1, maxBackgroundType)) // Drop ID to 1 for BGOs of out of range IDs
        {
            pLogWarning("BGO-%d ID is out of range (max types %d), reset to BGO-1", bgo.Type, maxBackgroundType);
            bgo.Type = 1;
        }

        bgo.Layer = b.layer;
        bgo.Location.Width = GFXBackgroundWidth[bgo.Type];
        bgo.Location.Height = BackgroundHeight[bgo.Type];

        bgo.uid = int(b.meta.array_id);

        bgo.zMode = b.z_mode;
        bgo.zOffset = b.z_offset;

        bgoApplyZMode(&bgo, int(b.smbx64_sp));
    }


    for(auto &n : lvl.npc)
    {
        numNPCs++;
        if(numNPCs > maxNPCs)
        {
            numNPCs = maxNPCs;
            break;
        }

        auto &npc = NPC[numNPCs];

        npc = NPC_t();

        npc.Location.X = n.x;
        npc.Location.Y = n.y;
        if(!LevelEditor)
            npc.Location.Y -= 0.01;
        npc.Direction = n.direct;
        npc.Type = int(n.id);

        if(IF_OUTRANGE(npc.Type, 0, maxNPCType)) // Drop ID to 1 for NPCs of out of range IDs
        {
            pLogWarning("NPC-%d ID is out of range (max types %d), reset to NPC-1", npc.Type, maxNPCType);
            npc.Type = 1;
        }

        if(npc.Type == NPCID_BURIEDPLANT || npc.Type == NPCID_YOSHIEGG ||
           npc.Type == NPCID_BUBBLE || npc.Type == NPCID_LAKITU_SMW)
        {
            npc.Special = n.contents;
            npc.DefaultSpecial = int(npc.Special);
        }

        if(npc.Type == NPCID_POTION || npc.Type == NPCID_POTIONDOOR ||
          (npc.Type == NPCID_BURIEDPLANT && n.contents == NPCID_POTION))
        {
            npc.Special2 = n.special_data;
            npc.DefaultSpecial2 = int(npc.Special2);
        }

        if(NPCIsAParaTroopa[npc.Type])
        {
            npc.Special = n.special_data;
            npc.DefaultSpecial = int(npc.Special);
        }

        if(NPCIsCheep[npc.Type])
        {
            npc.Special = n.special_data;
            npc.DefaultSpecial = int(npc.Special);
        }

        if(npc.Type == NPCID_FIREBAR)
        {
            npc.Special = n.special_data;
            npc.DefaultSpecial = int(npc.Special);
        }

        if(npc.Type == NPCID_CANNONITEM) // billy gun
        {
            if(compatModern && isSmbx64 && fVersion < 28)
                npc.Special7 = 2.0; // SMBX 1.1.x and 1.0.x behavior
            else if(compatModern && isSmbx64 && fVersion < 51)
                npc.Special7 = 1.0; // SMBX 1.2 behavior
            else
                npc.Special7 = n.special_data; // SMBX 1.2.1 and newer behavior, customizable behavior
        }

        if(npc.Type == NPCID_THWOMP_SMB3)
        {
            if(compatModern && isSmbx64 && fVersion < 9)
                npc.Special7 = 1.0; // Make twomps to fall always
            else
                npc.Special7 = n.special_data;
        }

        if(npc.Type == NPCID_BOWSER_SMB3)
        {
            if(compatModern && isSmbx64 && fVersion < 9)
                npc.Special7 = 1.0; // Keep original behavior of Bowser as in SMBX 1.0
            else
                npc.Special7 = n.special_data;
        }

        if(npc.Type == NPCID_YELBLOCKS)
        {
            if(compatModern && isSmbx64 && fVersion < 9)
                npc.Special7 = 1.0; // Workaround for yellow platform at The Invasion 1
            else
                npc.Special7 = 0.0;
        }

        npc.Generator = n.generator;
        if(npc.Generator)
        {
            npc.GeneratorDirection = n.generator_direct;
            npc.GeneratorEffect = n.generator_type;
            npc.GeneratorTimeMax = n.generator_period;
        }

        npc.Text = n.msg;

        npc.Inert = n.friendly;
        if(npc.Type == NPCID_SIGN)
            npc.Inert = true;
        npc.Stuck = n.nomove;
        npc.DefaultStuck = npc.Stuck;

        npc.Legacy = n.is_boss;

        npc.Layer = n.layer;
        npc.TriggerActivate = n.event_activate;
        npc.TriggerDeath = n.event_die;
        npc.TriggerTalk = n.event_talk;
        npc.TriggerLast = n.event_emptylayer;
        npc.AttLayer = n.attach_layer;

        npc.DefaultType = npc.Type;
        npc.Location.Width = NPCWidth[npc.Type];
        npc.Location.Height = NPCHeight[npc.Type];
        npc.DefaultLocation = npc.Location;
        npc.DefaultDirection = npc.Direction;
        npc.TimeLeft = 1;
        npc.Active = true;
        npc.JustActivated = 1;

        CheckSectionNPC(numNPCs);

        if(npc.Type == NPCID_CHECKPOINT) // Is a checkpoint
        {
            checkPointId++;
            npc.Special = checkPointId;
            npc.DefaultSpecial = int(npc.Special);
        }
        else if(npc.Type == NPCID_STAR_SMB3 || npc.Type == NPCID_STAR_SMW) // Is a star
        {
            bool tempBool = false;
            for(B = 1; B <= numStars; ++B)
            {
                if(Star[B].level == FileNameFull && (Star[B].Section == npc.Section || Star[B].Section == -1))
                    tempBool = true;
            }

            if(tempBool)
            {
                npc.Special = 1;
                npc.DefaultSpecial = 1;
                if(npc.Type == NPCID_STAR_SMW)
                    npc.Killed = 9;
            }
        }
    }


    for(auto &w : lvl.doors)
    {
        numWarps++;
        if(numWarps > maxWarps)
        {
            numWarps = maxWarps;
            break;
        }

        auto &warp = Warp[numWarps];

        warp = Warp_t();

        warp.PlacedEnt = true;
        warp.PlacedExit = true;
        warp.Entrance.X = w.ix;
        warp.Entrance.Y = w.iy;
        warp.Exit.X = w.ox;
        warp.Exit.Y = w.oy;
        warp.Direction = w.idirect;
        warp.Direction2 = w.odirect;
        warp.Effect = w.type;
        warp.twoWay = w.two_way;

        // Work around filenames with no extension suffix and case missmatch
        if(!Strings::endsWith(w.lname, ".lvl") && !Strings::endsWith(w.lname, ".lvlx"))
        {
            std::string lx = dirEpisode.resolveFileCase(w.lname + ".lvlx"),
                        lo = dirEpisode.resolveFileCase(w.lname + ".lvl");
            if(Files::fileExists(FileNamePath + lx))
                warp.level = lx;
            else if(Files::fileExists(FileNamePath + lo))
                warp.level = lo;
            else
                warp.level = dirEpisode.resolveFileCase(w.lname);
        }
        else
            warp.level = dirEpisode.resolveFileCase(w.lname);

        warp.LevelWarp = int(w.warpto);
        warp.LevelEnt = w.lvl_i;

        warp.MapWarp = w.lvl_o;
        warp.MapX = int(w.world_x);
        warp.MapY = int(w.world_y);

        warp.Stars = w.stars;
        warp.Layer = w.layer;
        warp.Hidden = w.unknown;

        warp.NoYoshi = w.novehicles;
        warp.WarpNPC = w.allownpc;
        warp.Locked = w.locked;

        warp.cannonExit = w.cannon_exit;
        warp.cannonExitSpeed = w.cannon_exit_speed;
        warp.eventEnter = w.event_enter;
        warp.StarsMsg = w.stars_msg;
        warp.noPrintStars = w.star_num_hide;
        warp.noEntranceScene = w.hide_entering_scene;

        warp.Entrance.Height = 32;
        warp.Entrance.Width = 32;
        warp.Exit.Height = 32;
        warp.Exit.Width = 32;
    }


    for(auto &w : lvl.physez)
    {
        numWater++;
        if(numWater > maxWater)
        {
            numWater = maxWater;
            break;
        }

        auto &water = Water[numWater];

        water = Water_t();

        water.Location.X = w.x;
        water.Location.Y = w.y;
        water.Location.Width = w.w;
        water.Location.Height = w.h;
        water.Buoy = w.buoy;
        water.Quicksand = w.env_type;
        water.Layer = w.layer;
    }

    A = 0;
    for(auto &l : lvl.layers)
    {
        auto &layer = Layer[A];

        layer = Layer_t();

        layer.Name = l.name;
        layer.Hidden = l.hidden;
        if(layer.Hidden)
        {
            HideLayer(layer.Name, true);
        }
//        if(LevelEditor == true || MagicHand == true)
//        {
//            // Add into listbox
//        }
        A++;
        numLayers++;
        if(numLayers > maxLayers)
        {
            numLayers = maxLayers;
            break;
        }
    }

    A = 0;
    for(auto &e : lvl.events)
    {
        auto &event = Events[A];

        event = Events_t();

        event.Name = e.name;
        event.Text = e.msg;
        event.Sound = int(e.sound_id);
        event.EndGame = int(e.end_game);

//        int hideLayersNum = int(e.layers_hide.size());
//        int showLayersNum = int(e.layers_show.size());
//        int toggleLayersNum = int(e.layers_toggle.size());

        event.HideLayer.clear();
        event.ShowLayer.clear();
        event.ToggleLayer.clear();

//        for(B = 0; B <= maxSections; B++)
//        {
//            event.HideLayer[B].clear();
//            event.ShowLayer[B].clear();
//            event.ToggleLayer[B].clear();
//        }

        event.HideLayer = e.layers_hide;
        event.ShowLayer = e.layers_show;
        event.ToggleLayer = e.layers_toggle;

//        for(B = 0; B <= maxSections; B++)
//        {
//            if(B < hideLayersNum)
//                event.HideLayer[B] = e.layers_hide[size_t(B)];
//            if(B < showLayersNum)
//                event.ShowLayer[B] = e.layers_show[size_t(B)];
//            if(B < toggleLayersNum)
//                event.ToggleLayer[B] = e.layers_toggle[size_t(B)];
//        }

        int maxSets = int(e.sets.size());
        if(maxSets > numSections)
            maxSets = numSections;

        for(B = 0; B <= numSections; B++)
        {
            auto &s = event.section[B];
            s.music_id = LevelEvent_Sets::LESet_Nothing;
            s.background_id = LevelEvent_Sets::LESet_Nothing;
            s.music_file.clear();
            s.position.X = LevelEvent_Sets::LESet_Nothing;
            s.position.Y = 0;
            s.position.Height = 0;
            s.position.Width = 0;
        }

        for(B = 0; B < maxSets; B++)
        {
            auto &ss = event.section[B];
            auto &s = e.sets[size_t(B)];
            ss.music_id = int(s.music_id);
            ss.background_id = int(s.background_id);
            ss.music_file = s.music_file;

            auto &l = ss.position;
            l.X = s.position_left;
            l.Y = s.position_top;
            l.Height = s.position_bottom;
            l.Width = s.position_right;

            ss.autoscroll = s.autoscrol;
            // Simple style is only supported yet
            if(s.autoscroll_style == LevelEvent_Sets::AUTOSCROLL_SIMPLE)
            {
                ss.autoscroll_x = s.autoscrol_x;
                ss.autoscroll_y = s.autoscrol_y;
            }
        }

        event.TriggerEvent = e.trigger;
        event.TriggerDelay = e.trigger_timer;

        event.LayerSmoke = e.nosmoke;

        event.Controls.AltJump = e.ctrl_altjump;
        event.Controls.AltRun = e.ctrl_altrun;
        event.Controls.Down = e.ctrl_down;
        event.Controls.Drop = e.ctrl_drop;
        event.Controls.Jump = e.ctrl_jump;
        event.Controls.Left = e.ctrl_left;
        event.Controls.Right = e.ctrl_right;
        event.Controls.Run = e.ctrl_run;
        event.Controls.Start = e.ctrl_start;
        event.Controls.Up = e.ctrl_up;

        event.AutoStart = e.autostart;
        event.MoveLayer = e.movelayer;
        event.SpeedX = float(e.layer_speed_x);
        event.SpeedY = float(e.layer_speed_y);

        event.AutoX = float(e.move_camera_x);
        event.AutoY = float(e.move_camera_y);
        event.AutoSection = int(e.scroll_section);

        A++;
        numEvents++;
        if(numEvents > maxEvents)
        {
            numEvents = maxEvents;
            break;
        }
    }

    FindBlocks();
    qSortBackgrounds(1, numBackground);
    UpdateBackgrounds();
    FindSBlocks();


//    if(LevelEditor == true || MagicHand == true)
//    {
//        frmEvents::lstEvent.ListIndex = 0;
//        frmLayers::lstLayer.ListIndex = 0;
//        frmEvents::RefreshEvents;
//    }

//    if(LevelEditor == true)
//    {
//        ResetNPC EditorCursor.NPC.Type;
//        curSection = 0;
//        vScreenY[1] = -(level[curSection].Height - 600);
//        vScreenX[1] = -level[curSection].X;
//        numWarps = numWarps + 1;
//        for(A = 0; A < frmLevelSettings::optBackground.Count; A++)
//        {
//            if(Background2[0] == A)
//                frmLevelSettings::optBackground(A).Value = true;
//            else
//                frmLevelSettings::optBackground(A).Value = false;
//        }
//        for(A = 1; A <= frmLevelSettings::optBackgroundColor.Count; A++)
//        {
//            if(bgColor[0] == frmLevelSettings::optBackgroundColor(A).BackColor)
//            {
//                frmLevelSettings::optBackgroundColor(A).Value = true;
//                break;
//            }
//        }
//        frmLevelSettings::optMusic(bgMusic[0]).Value = true;
//        if(LevelWrap[0] == true)
//            frmLevelSettings::cmdWrap.Caption = "On";
//        else
//            frmLevelSettings::cmdWrap.Caption = "Off";
//        if(UnderWater[0] == true)
//            frmLevelSettings::cmdWater.Caption = "On";
//        else
//            frmLevelSettings::cmdWater.Caption = "Off";
//        if(OffScreenExit[0] == true)
//            frmLevelSettings::cmdExit.Caption = "On";
//        else
//            frmLevelSettings::cmdExit.Caption = "Off";
//        frmLevelSettings::txtMusic.Enabled = false;
//        frmLevelSettings::txtMusic.Text = CustomMusic[0];
//        frmLevelSettings::txtMusic.Enabled = true;
//        if(nPlay.Online == true && nPlay.Mode == 1) // sync to server
//        {
//            Netplay::sendData "j" + LB + "d" + LocalNick + " has loaded " + FileName + "." + LB + "w1" + LB + EoT;
//            frmChat.txtChat = frmChat::txtChat + LocalNick + " has loaded " + FileName + "." + LB;
//            frmChat::txtChat.SelStart = frmChat::txtChat.Text.Length;
//            PlaySound(SFX_Message);
//            SoundPause[47] = 2;
//            for(A = 1; A <= 15; A++)
//            {
//                if(nPlay.ClientCon(A) == true)
//                {
//                    Netplay::InitSync A;
//                }
//            }
//        }
//    }
//    else
    {
        FindStars();
        LevelMacro = LEVELMACRO_OFF;
        for(A = 0; A <= numSections; A++) // Automatically correct 608 section height to 600
        {
//            if(int(level[A].Height - level[A].Y) == 608)
//                level[A].Y = level[A].Y + 8;
            int height = int(level[A].Height - level[A].Y);
            if(height > 600 && height < 610)
                level[A].Y = level[A].Height - 600; // Better and cleaner logic
        }

        B = numBackground;
        for(A = 1; A <= numWarps; A++)
        {
            if(Warp[A].Effect == 2 && Warp[A].Stars > numStars)
            {
                B++;
                numLocked++;
                auto &bgo = Background[B];
                bgo = Background_t();
                bgo.Layer = Warp[A].Layer;
                bgo.Hidden = Warp[A].Hidden;
                bgo.Location.Width = 24;
                bgo.Location.Height = 24;
                bgo.Location.Y = Warp[A].Entrance.Y - bgo.Location.Height;
                bgo.Location.X = Warp[A].Entrance.X + Warp[A].Entrance.Width / 2.0 - bgo.Location.Width / 2.0;
                bgo.Type = 160;
            }
            else if(Warp[A].Effect == 2 && Warp[A].Locked) // For locks
            {
                B++;
                numLocked++;
                auto &bgo = Background[B];
                bgo = Background_t();
                bgo.Layer = Warp[A].Layer;
                bgo.Hidden = Warp[A].Hidden;
                bgo.Location = Warp[A].Entrance;
                bgo.Type = 98;
                bgo.Location.Width = 16;
            }
        }
    }

    // If too much locks
    SDL_assert_release(numBackground + numLocked <= (maxBackgrounds + maxWarps));

    SoundPause[13] = 100;
    resetFrameTimer();

    return true;
}

void ClearLevel()
{
    int A = 0;
    int B = 0;
    NPC_t blankNPC = NPC_t();
    Water_t blankwater = Water_t();
    Warp_t blankWarp = Warp_t();
    Block_t blankBlock = Block_t();
    Background_t BlankBackground = Background_t();
    Location_t BlankLocation = Location_t();
    Events_t blankEvent = Events_t();
    NPCScore[274] = 6;
    LevelName.clear();
    ResetCompat();
    LoadNPCDefaults();
    LoadPlayerDefaults();
    noUpdate = true;
    BlocksSorted = true;
    qScreen = false;
    UnloadCustomGFX();
    doShakeScreenClear();

    AutoUseModern = false;

    numSections = 0;

    for(A = 1; A <= newEventNum; A++)
    {
        NewEvent[A] = "";
        newEventDelay[A] = 0;
    }

    for(A = 0; A <= maxSections; A++)
    {
        AutoX[A] = 0;
        AutoY[A] = 0;
    }

    numEvents = 0;
    newEventNum = 0;
    for(A = 0; A <= maxEvents; A++)
    {
        Events[A] = blankEvent;
        for(B = 0; B <= maxSections; B++)
        {
            auto &ss = Events[A].section[B];
            ss.background_id = EventSection_t::LESet_Nothing;
            ss.music_id = EventSection_t::LESet_Nothing;
            ss.music_file.clear();
            ss.position.X = EventSection_t::LESet_Nothing;
        }
    }

    for(A = 0; A <= maxWater; A++)
        Water[A] = blankwater;

    numWater = 0;
    Events[0].Name = "Level - Start";
    Events[1].Name = "P Switch - Start";
    Events[2].Name = "P Switch - End";
    curMusic = 0;
    curStars = 0;
    maxStars = 0;
    PSwitchTime = 0;
    PSwitchStop = 0;
    BeltDirection = 1;
    StopMusic();
    Layer[0] = Layer_t();
    Layer[0].Name = "Default";
    Layer[0].Hidden = false;
    Layer[1] = Layer_t();
    Layer[1].Name = "Destroyed Blocks";
    Layer[1].Hidden = true;
    Layer[2] = Layer_t();
    Layer[2].Name = "Spawned NPCs";
    Layer[2].Hidden = false;

    numLayers = 0;
    for(A = 0; A <= maxLayers; A++)
    {
        Layer[A] = Layer_t();
        if(A > 2)
        {
            Layer[A].Name = "";
            Layer[A].Hidden = false;
        }
        Layer[A].SpeedX = 0;
        Layer[A].SpeedY = 0;
    }

//    If LevelEditor = True Or MagicHand = True Then
    if(LevelEditor || MagicHand)
    {
//        MessageText = ""
        MessageText.clear();
//        frmNPCs.chkMessage.Value = 0
//        frmBlocks.chkFill.Value = 0
//        frmEvents.txtEvent.Text = ""
//        noUpdate = True
        noUpdate = true;
//        frmEvents.RefreshEvents
//        frmLayers.lstLayer.Clear
//        frmLayers.lstLayer.AddItem "Default"
//        frmLayers.lstLayer.AddItem "Destroyed Blocks"
//        frmLayers.lstLayer.AddItem "Spawned NPCs"
//        frmLayers.lstLayer.Selected(1) = False
//        frmLayers.lstLayer.Selected(2) = True
//        frmLayers.lstLayer.Selected(0) = True
//        frmLayers.cmdDelete.Enabled = False
//        frmLayers.txtLayer.Enabled = False
//        frmLevelEditor.optCursor(13).Value = True
//    End If
    }

    for(A = -128; A <= maxNPCs; A++)
        NPC[A] = blankNPC;
    numNPCs = 0;

    for(A = 1; A <= maxBlocks; A++)
        Block[A] = blankBlock;
    numBlock = 0;

    for(A = 1; A <= maxBackgrounds; A++)
        Background[A] = BlankBackground;

    for(A = 0; A <= maxSections; A++)
    {
        Background2[A] = 0;
        bgColor[A] = 0xF89868;
        bgMusic[A] = 0;
        level[A] = BlankLocation;
        LevelWrap[A] = false;
        LevelVWrap[A] = false;
        LevelChop[A] = 0;
        NoTurnBack[A] = false;
        UnderWater[A] = false;
        OffScreenExit[A] = false;
        CustomMusic[A] = "";
    }

    for(A = 1; A <= numWarps; A++)
        Warp[A] = blankWarp;
    numWarps = 0;

    numEffects = 0;
    numBackground = 0;
    numLocked = 0;
    MidBackground = 1;
    LastBackground = 0;
    PlayerStart[1] = BlankLocation;
    PlayerStart[2] = BlankLocation;

//    If LevelEditor = True Then
//        Unload frmLevelAdv
//        noUpdate = True
//        numPlayers = 0
//        frmLevelSettings.txtMusic.Enabled = False
//        frmLevelSettings.txtMusic.Text = ""
//        frmLevelSettings.txtMusic.Enabled = True
//        frmLevelSettings.optBackgroundColor(1).Value = 1
//        frmLevelSettings.cmdWrap.Caption = "Off"
//        frmLevelSettings.optMusic(bgMusic(0)).Value = True
//        vScreenX(1) = 0
//        vScreenY(1) = 0
//        curSection = 0
//        level(curSection).Height = 20000 * (curSection - maxSections / 2)
//        level(curSection).Height = Int(level(curSection).Height / 32) * 32
//        level(curSection).Y = level(curSection).Height - 600
//        level(curSection).X = 20000 * (curSection - maxSections / 2)
//        level(curSection).X = Int(level(curSection).X / 32) * 32
//        level(curSection).Width = level(curSection).X + 800
//        vScreenY(1) = -(level(curSection).Height - 600)
//        vScreenX(1) = -level(curSection).X
//        numWarps = 1
//        frmLevelSettings.optSection(0).Value = True
//        frmLevelSettings.optBackground(0).Value = True
//        frmLevelSettings.optLevel(0).Value = True
//        For A = 1 To frmLevelSettings.optBackground.Count - 1
//            frmLevelSettings.optBackground(A).Value = False
//        Next A
//    End If

    noUpdate = false;
}

void FindStars()
{
//    int A = 0;
//    int B = 0;
//    std::string newInput;
    LevelData head;

    for(int A = 1; A <= numWarps; A++)
    {
        auto &warp = Warp[A];

        if(!warp.level.empty())
        {
            std::string lFile = FileNamePath + warp.level;
            addMissingLvlSuffix(lFile);

            if(Files::fileExists(lFile))
            {
                if(FileFormats::OpenLevelFileHeader(lFile, head))
                {
                    warp.maxStars = head.stars;
                    warp.curStars = 0;

                    for(int B = 1; B <= numStars; B++)
                    {
                        if(SDL_strcasecmp(Star[B].level.c_str(), warp.level.c_str()) == 0)
                            warp.curStars++;
                    }
                }
            }

        }
    }
}
