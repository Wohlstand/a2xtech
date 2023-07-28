/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2023 Vitaly Novichkov <admin@wohlnet.ru>
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

#include <set>

#include "sdl_proxy/sdl_stdinc.h"
#include "globals.h"
#include "layers.h"
#include "effect.h"
#include "collision.h"
#include "npc.h"
#include "sound.h"
#include "graphics.h"
#include "game_main.h"
#include "compat.h"
#include "frame_timer.h"
#include "main/speedrunner.h"
#include "compat.h"
#include "editor.h"
#include "blocks.h"
#include "main/trees.h"
#include "main/block_table.h"

#include "npc/npc_queues.h"
#include "graphics/gfx_update.h"

int numLayers = 0;
RangeArr<Layer_t, 0, maxLayers> Layer;

int numEvents = 0;
RangeArr<Events_t, 0, maxEvents> Events;

RangeArrI<eventindex_t, 1, maxEvents, EVENT_NONE> NewEvent;
RangeArrI<int, 1, maxEvents, 0> newEventDelay;
int newEventNum = 0;

layerindex_t LAYER_USED_P_SWITCH = LAYER_NONE;
static std::set<eventindex_t> recentlyTriggeredEvents;

static SDL_INLINE bool equalCase(const std::string &x, const std::string &y)
{
    return (SDL_strcasecmp(x.c_str(), y.c_str()) == 0);
}

static SDL_INLINE bool equalCase(const char *x, const char *y)
{
    return (SDL_strcasecmp(x, y) == 0);
}



// utilities for layerindex_t and eventindex_t

layerindex_t FindLayer(const std::string& LayerName)
{
    if(LayerName.empty())
        return LAYER_NONE;

    for(layerindex_t A = 0; A <= maxLayers; A++)
    {
        if(equalCase(Layer[A].Name, LayerName))
            return A;
    }

    return LAYER_NONE;
}

eventindex_t FindEvent(const std::string& EventName)
{
    if(EventName.empty())
        return EVENT_NONE;

    for(eventindex_t A = 0; A <= maxEvents; A++)
    {
        if(equalCase(Events[A].Name, EventName))
            return A;
    }

    return EVENT_NONE;
}


// Functions for layers


// New functions:


bool SwapLayers(layerindex_t index_1, layerindex_t index_2)
{
    if(index_1 == LAYER_NONE || index_2 == LAYER_NONE)
        return false;

    std::swap(Layer[index_1], Layer[index_2]);

    // repoint all of Layer 1's objects to index 2
    for(int A : Layer[index_1].NPCs)
        NPC[A].Layer = index_2;

    for(int A : Layer[index_1].blocks)
        Block[A].Layer = index_2;

    for(int A : Layer[index_1].BGOs)
        Background[A].Layer = index_2;

    for(int A : Layer[index_1].warps)
        Warp[A].Layer = index_2;

    for(int A : Layer[index_1].waters)
        Water[A].Layer = index_2;

    // repoint all of Layer 2's objects to index 1
    for(int A : Layer[index_2].NPCs)
        NPC[A].Layer = index_1;

    for(int A : Layer[index_2].blocks)
        Block[A].Layer = index_1;

    for(int A : Layer[index_2].BGOs)
        Background[A].Layer = index_1;

    for(int A : Layer[index_2].warps)
        Warp[A].Layer = index_1;

    for(int A : Layer[index_2].waters)
        Water[A].Layer = index_1;

    // swap AttLayers
    for(int A = 1; A <= numNPCs; A++)
    {
        if(NPC[A].AttLayer == index_1)
            NPC[A].AttLayer = index_2;
        else if(NPC[A].AttLayer == index_2)
            NPC[A].AttLayer = index_1;
    }

    // swap event layer refs
    for(int A = 0; A < numEvents; A++)
    {
        for(layerindex_t& l : Events[A].HideLayer)
        {
            if(l == index_1)
                l = index_2;
            else if(l == index_2)
                l = index_1;
        }

        for(layerindex_t& l : Events[A].ShowLayer)
        {
            if(l == index_1)
                l = index_2;
            else if(l == index_2)
                l = index_1;
        }

        for(layerindex_t& l : Events[A].ToggleLayer)
        {
            if(l == index_1)
                l = index_2;
            else if(l == index_2)
                l = index_1;
        }

        if(Events[A].MoveLayer == index_1)
            Events[A].MoveLayer = index_2;
        else if(Events[A].MoveLayer == index_2)
            Events[A].MoveLayer = index_1;
    }

    // swap EditorCursor layer
    if(EditorCursor.Layer == index_1)
        EditorCursor.Layer = index_2;
    else if(EditorCursor.Layer == index_2)
        EditorCursor.Layer = index_1;

    LAYER_USED_P_SWITCH = FindLayer(LAYER_USED_P_SWITCH_TITLE);

    return true;
}

bool RenameLayer(layerindex_t L, const std::string& NewName)
{
    if(L == LAYER_NONE)
        return false;
    if(NewName.empty())
        return false;

    int A = 0;

    for(A = 0; A <= maxLayers; A++)
    {
        if(equalCase(Layer[A].Name, NewName))
            return false;
    }

    Layer[L].Name = NewName;

    LAYER_USED_P_SWITCH = FindLayer(LAYER_USED_P_SWITCH_TITLE);

    return true;
}

bool DeleteLayer(layerindex_t L, bool killall)
{
    if(L == LAYER_NONE)
        return false;

    int A = 0;

    // order is important here, thus the unoptimized loops
    for(A = numNPCs; A >= 1; A--)
    {
        if(NPC[A].AttLayer == L)
            NPC[A].AttLayer = LAYER_NONE;
        if(NPC[A].Layer == L)
        {
            if(killall)
                KillNPC(A, 9);
            else
            {
                NPC[A].Layer = LAYER_DEFAULT;
                syncLayers_NPC(A);
            }
        }
    }

    for(A = numBlock; A >= 1; A--)
    {
        if(Block[A].Layer == L)
        {
            if(killall)
                KillBlock(A, false);
            else
            {
                Block[A].Layer = LAYER_DEFAULT;
                syncLayersTrees_Block(A);
            }
        }
    }

    for(A = numWarps; A >= 1; A--)
    {
        if(Warp[A].Layer == L)
        {
            if(killall)
                KillWarp(A);
            else
            {
                Warp[A].Layer = LAYER_DEFAULT;
                syncLayers_Warp(A);
            }
        }
    }

    for(A = numBackground; A >= 1; A--)
    {
        if(Background[A].Layer == L)
        {
            if(killall)
            {
                Background[A] = Background[numBackground];
                numBackground --;
                syncLayers_BGO(A);
                syncLayers_BGO(numWater+1);
            }
            else
            {
                Background[A].Layer = LAYER_DEFAULT;
                syncLayers_BGO(A);
            }
        }
    }

    for(A = numWater; A >= 1; A--)
    {
        if(Water[A].Layer == L)
        {
            if(killall)
            {
                Water[A] = Water[numWater];
                numWater --;
                syncLayers_Water(A);
                syncLayers_Water(numWater+1);
            }
            else
            {
                Water[A].Layer = LAYER_DEFAULT;
                syncLayers_Water(A);
            }
        }
    }

    for(A = 0; A < numEvents; A++)
    {
        for(auto it = Events[A].HideLayer.end(); it != Events[A].HideLayer.begin();)
        {
            it--;
            if(*it == L)
                it = Events[A].HideLayer.erase(it);
        }

        for(auto it = Events[A].ShowLayer.end(); it != Events[A].ShowLayer.begin();)
        {
            it--;
            if(*it == L)
                it = Events[A].ShowLayer.erase(it);
        }

        for(auto it = Events[A].ToggleLayer.end(); it != Events[A].ToggleLayer.begin();)
        {
            it--;
            if(*it == L)
                it = Events[A].ToggleLayer.erase(it);
        }

        if(Events[A].MoveLayer == L)
            Events[A].MoveLayer = LAYER_NONE;
    }

    for(int B = L; B <= numLayers - 1; B++)
        SwapLayers(B, B+1);

    Layer[numLayers] = Layer_t();
    numLayers --;

    if(EditorCursor.Layer == L)
        EditorCursor.Layer = LAYER_DEFAULT;

    LAYER_USED_P_SWITCH = FindLayer(LAYER_USED_P_SWITCH_TITLE);

    return true;
}


// Old functions:

void ShowLayer(layerindex_t L, bool NoEffect)
{
    if(L == LAYER_NONE)
        return;

    int A = 0;
    int B = 0;
    Location_t tempLocation;

    Layer[L].Hidden = false;
    if(L == LAYER_DESTROYED_BLOCKS)
        Layer[L].Hidden = true;

    if(L == LAYER_SPAWNED_NPCS)
        Layer[L].Hidden = false;

    for(int A : Layer[L].NPCs)
    {
        if(NPC[A].Hidden)
        {
            if(!NoEffect && !NPC[A].Generator)
            {
                tempLocation = NPC[A].Location;
                tempLocation.X += tempLocation.Width / 2.0 - EffectWidth[10] / 2.0;
                tempLocation.Y += tempLocation.Height / 2.0 - EffectHeight[10] / 2.0;
                NewEffect(10, tempLocation);
            }

            if(!LevelEditor)
            {
                if(!NPCWontHurt[NPC[A].Type] && !NPCIsABonus[NPC[A].Type] && NPC[A].Active)
                {
                    for(B = 1; B <= numPlayers; B++)
                    {
                        if(CheckCollision(Player[B].Location, NPC[A].Location))
                            Player[B].Immune = 120;
                    }
                }
            }
        }
        NPC[A].Hidden = false;
        NPC[A].GeneratorActive = true;
        NPC[A].Reset[1] = true;
        NPC[A].Reset[2] = true;

        if(!NPC[A].Generator)
        {
            NPC[A].Active = true;
            NPC[A].TimeLeft = 1;

            NPCQueues::Active.insert(A);
        }

        CheckSectionNPC(A);
    }

    if(!Layer[L].blocks.empty())
        invalidateDrawBlocks();

    for(int A : Layer[L].blocks)
    {
        // If Not (Block(A).DefaultType = 0 And Block(A).Layer = "Destroyed Blocks") Then
        if(Block[A].Hidden)
        {
            if(!NoEffect && !Block[A].Invis)
            {
                tempLocation = Block[A].Location;
                tempLocation.X += tempLocation.Width / 2.0 - EffectWidth[10] / 2.0;
                tempLocation.Y += tempLocation.Height / 2.0 - EffectHeight[10] / 2.0;
                NewEffect(10, tempLocation);
            }
        }
        Block[A].Hidden = false;

        // moved code to restore all hit blocks below
    }

    if(!Layer[L].BGOs.empty())
        invalidateDrawBGOs();

    for(int A : Layer[L].BGOs)
    {
        if(Background[A].Hidden)
        {
            if(!NoEffect)
            {
                tempLocation = Background[A].Location;
                tempLocation.X += tempLocation.Width / 2.0 - EffectWidth[10] / 2.0;
                tempLocation.Y += tempLocation.Height / 2.0 - EffectHeight[10] / 2.0;
                NewEffect(10, tempLocation);
            }
        }
        Background[A].Hidden = false;
    }

    for(int A : Layer[L].warps)
        Warp[A].Hidden = false;

    for(int A : Layer[L].waters)
        Water[A].Hidden = false;

    if(L == LAYER_DESTROYED_BLOCKS)
    {
        // restore all hit blocks, even non-destroyed
        for(A = 1; A <= numBlock; A++)
        {
            if(Block[A].DefaultType > 0)
            {
                // could be nice to have an "orig_layer" variable,
                //  would eliminate certain vanilla peculiarities,
                //  especially if done cleverly with the block's
                //  layer offset
                if(Block[A].Layer == L)
                    Block[A].Layer = LAYER_DEFAULT;
                Block[A].Special = Block[A].DefaultSpecial;
                Block[A].Type = Block[A].DefaultType;
                syncLayersTrees_Block(A);
            }
        }
    }
}

void HideLayer(layerindex_t L, bool NoEffect)
{
    if(L == LAYER_NONE)
        return;

    Location_t tempLocation;

    Layer[L].Hidden = true;

    for(int A : Layer[L].NPCs)
    {
        if(!NPC[A].Hidden)
        {
            if(!NoEffect && !NPC[A].Generator)
            {
                tempLocation = NPC[A].Location;
                tempLocation.X += tempLocation.Width / 2.0 - EffectWidth[10] / 2.0;
                tempLocation.Y += tempLocation.Height / 2.0 - EffectHeight[10] / 2.0;
                NewEffect(10, tempLocation);
            }
        }

        NPC[A].Hidden = true;

        if(!NPC[A].Generator)
        {
            Deactivate(A);
        }
    }

    if(!Layer[L].blocks.empty())
        invalidateDrawBlocks();

    for(int A : Layer[L].blocks)
    {
        if(!Block[A].Hidden)
        {
            if(!NoEffect && !Block[A].Invis)
            {
                tempLocation = Block[A].Location;
                tempLocation.X += tempLocation.Width / 2.0 - EffectWidth[10] / 2.0;
                tempLocation.Y += tempLocation.Height / 2.0 - EffectHeight[10] / 2.0;
                NewEffect(10, tempLocation);
            }
        }
        Block[A].Hidden = true;
    }

    if(!Layer[L].BGOs.empty())
        invalidateDrawBGOs();

    for(int A : Layer[L].BGOs)
    {
        if(!Background[A].Hidden)
        {
            if(!NoEffect)
            {
                tempLocation = Background[A].Location;
                tempLocation.X += tempLocation.Width / 2.0 - EffectWidth[10] / 2.0;
                tempLocation.Y += tempLocation.Height / 2.0 - EffectHeight[10] / 2.0;
                NewEffect(10, tempLocation);
            }
        }
        Background[A].Hidden = true;
    }

    for(int A : Layer[L].warps)
        Warp[A].Hidden = true;

    for(int A : Layer[L].waters)
        Water[A].Hidden = true;
}

void SetLayer(layerindex_t /*LayerName*/)
{
    // Unused
}



// Functions for events


// New functions:

void InitializeEvent(Events_t& event)
{
    event = Events_t();
    for(int i = 0; i <= maxSections; i++)
    {
        event.section[i] = EventSection_t();
        event.section[i].position.X = EventSection_t::LESet_Nothing;
    }
}

bool SwapEvents(eventindex_t index_1, eventindex_t index_2)
{
    if(index_1 == EVENT_NONE || index_2 == EVENT_NONE)
        return false;

    std::swap(Events[index_1], Events[index_2]);

    int A = 0;

    // swap EVERYTHING
    for(A = 1; A <= numNPCs; A++)
    {
        if(NPC[A].TriggerTalk == index_1)
            NPC[A].TriggerTalk = index_2;
        else if(NPC[A].TriggerTalk == index_2)
            NPC[A].TriggerTalk = index_1;

        if(NPC[A].TriggerDeath == index_1)
            NPC[A].TriggerDeath = index_2;
        else if(NPC[A].TriggerDeath == index_2)
            NPC[A].TriggerDeath = index_1;

        if(NPC[A].TriggerLast == index_1)
            NPC[A].TriggerLast = index_2;
        else if(NPC[A].TriggerLast == index_2)
            NPC[A].TriggerLast = index_1;

        if(NPC[A].TriggerActivate == index_1)
            NPC[A].TriggerActivate = index_2;
        else if(NPC[A].TriggerActivate == index_2)
            NPC[A].TriggerActivate = index_1;
    }

    for(A = 1; A <= numBlock; A++)
    {
        if(Block[A].TriggerHit == index_1)
            Block[A].TriggerHit = index_2;
        else if(Block[A].TriggerHit == index_2)
            Block[A].TriggerHit = index_1;

        if(Block[A].TriggerDeath == index_1)
            Block[A].TriggerDeath = index_2;
        else if(Block[A].TriggerDeath == index_2)
            Block[A].TriggerDeath = index_1;

        if(Block[A].TriggerLast == index_1)
            Block[A].TriggerLast = index_2;
        else if(Block[A].TriggerLast == index_2)
            Block[A].TriggerLast = index_1;
    }

    for(A = 1; A <= numWarps; A++)
    {
        if(Warp[A].eventEnter == index_1)
            Warp[A].eventEnter = index_2;
        else if(Warp[A].eventEnter == index_2)
            Warp[A].eventEnter = index_1;
    }

    for(A = 0; A < numEvents; A++)
    {
        if(Events[A].TriggerEvent == index_1)
            Events[A].TriggerEvent = index_2;
        else if(Events[A].TriggerEvent == index_2)
            Events[A].TriggerEvent = index_1;
    }

    return true;
}

bool RenameEvent(eventindex_t index, const std::string& NewName)
{
    if(index == EVENT_NONE || NewName.empty())
        return false;

    Events[index].Name = NewName;

    return true;
}

bool DeleteEvent(eventindex_t index)
{
    if(index == EVENT_NONE)
        return false;

    int A = 0;

    for(A = 1; A <= numNPCs; A++)
    {
        if(NPC[A].TriggerTalk == index)
            NPC[A].TriggerTalk = EVENT_NONE;
        if(NPC[A].TriggerDeath == index)
            NPC[A].TriggerDeath = EVENT_NONE;
        if(NPC[A].TriggerLast == index)
            NPC[A].TriggerLast = EVENT_NONE;
        if(NPC[A].TriggerActivate == index)
            NPC[A].TriggerActivate = EVENT_NONE;
    }

    for(A = 1; A <= numBlock; A++)
    {
        if(Block[A].TriggerHit == index)
            Block[A].TriggerHit = EVENT_NONE;
        if(Block[A].TriggerDeath == index)
            Block[A].TriggerDeath = EVENT_NONE;
        if(Block[A].TriggerLast == index)
            Block[A].TriggerLast = EVENT_NONE;
    }

    for(A = 1; A <= numWarps; A++)
    {
        if(Warp[A].eventEnter == index)
            Warp[A].eventEnter = EVENT_NONE;
    }

    for(A = 0; A < numEvents; A++)
    {
        if(Events[A].TriggerEvent == index)
            Events[A].TriggerEvent = EVENT_NONE;
    }

    for(int B = index; B < numEvents - 1; B++)
        SwapEvents(B, B+1);

    numEvents--;
    Events[numEvents] = Events_t();

    return true;
}


// Old functions:

void ProcEvent(eventindex_t index, bool NoEffect)
{
    if(index == EVENT_NONE || LevelEditor)
        return;

    // this is for events that have just been triggered
    int B = 0;
    // int C = 0;
    int D = 0;
    bool tempBool = false;
    Location_t tempLevel;
    Location_t newLevel;
    vScreen_t screenLoc;

    // Ignore vanilla autoscroll if newer way has been used
    bool autoScrollerChanged = false;

    {
        auto &evt = Events[index];
        {
            recentlyTriggeredEvents.insert(index);

            if(g_compatibility.speedrun_stop_timer_by == Compatibility_t::SPEEDRUN_STOP_EVENT && equalCase(evt.Name.c_str(), g_compatibility.speedrun_stop_timer_at))
                speedRun_bossDeadEvent();

            for(B = 0; B <= numSections; B++)
            {
                /* Music change */
                auto &s = evt.section[B];

                bool musicChanged = false;
                if(s.music_id == EventSection_t::LESet_ResetDefault)
                {
                    bgMusic[B] = bgMusicREAL[B];
                    musicChanged = true;
                }
                else if(s.music_id != EventSection_t::LESet_Nothing)
                {
                    bgMusic[B] = s.music_id;
                    musicChanged = true;
                }

                if(musicChanged && (B == Player[1].Section || (numPlayers == 2 && B == Player[2].Section)))
                    StartMusic(B);

                /* Background change */
                if(s.background_id == EventSection_t::LESet_ResetDefault)
                    Background2[B] = Background2REAL[B];
                else if(s.background_id != EventSection_t::LESet_Nothing)
                    Background2[B] = s.background_id;

                /* Per-Section autoscroll setup */
                if(s.autoscroll)
                {
                    if(!AutoUseModern) // First attemt to use modern autoscrolling will block futher use of the legacy autoscrolling
                        AutoUseModern = true;
                    autoScrollerChanged = true;
                    AutoX[B] = s.autoscroll_x;
                    AutoY[B] = s.autoscroll_y;
                }

                int onscreen_plr = 0;
                int warped_plr = 0;

                bool is_reset = int(s.position.X) == EventSection_t::LESet_ResetDefault;

                /* Resize the section boundaries */
                if(is_reset && !g_compatibility.modern_section_change)
                    level[B] = LevelREAL[B];
                else if(int(s.position.X) != EventSection_t::LESet_Nothing)
                {
                    tempLevel = level[B];
                    newLevel = (is_reset) ? LevelREAL[B] : static_cast<Location_t>(s.position);
                    level[B] = newLevel;

                    // warp other players to resized section, if not a reset
                    if(!evt.AutoStart && !equalCase(evt.Name, "Level - Start"))
                    {
                        for(int C = 1; C <= numPlayers; C++)
                        {
                            // If .Section = B Then
                            // Should set this only if the warp is successful!
                            if(!g_compatibility.modern_section_change)
                                Player[C].Section = B;

                            tempBool = false;
                            if(Player[C].Location.X + Player[C].Location.Width >= level[B].X)
                            {
                                if(Player[C].Location.X <= level[B].Width)
                                {
                                    if(Player[C].Location.Y + Player[C].Location.Height >= level[B].Y)
                                    {
                                        if(Player[C].Location.Y <= level[B].Height)
                                        {
                                            tempBool = true; // Check to see if player is still in section after resizing
                                            onscreen_plr = C;
                                            Player[C].Section = B;
                                        }
                                    }
                                }
                            }

                            // don't warp on reset
                            if(is_reset)
                                continue;

                            if(!tempBool)
                            {
                                for(D = 1; D <= numPlayers; D++)
                                {
                                    if(D != C && Player[D].Section == B)
                                    {
                                        if(Player[D].Location.X + Player[D].Location.Width >= level[B].X)
                                        {
                                            if(Player[D].Location.X <= level[B].Width)
                                            {
                                                if(Player[D].Location.Y + Player[D].Location.Height >= level[B].Y)
                                                {
                                                    if(Player[D].Location.Y <= level[B].Height) // Move to another player who is still in the section
                                                    {
                                                        warped_plr = C;

                                                        Player[C].Section = B;
                                                        Player[C].Location.X = Player[D].Location.X + Player[D].Location.Width / 2.0 - Player[C].Location.Width / 2.0;
                                                        Player[C].Location.Y = Player[D].Location.Y + Player[D].Location.Height - Player[C].Location.Height;
                                                        Player[C].Effect = 9;
                                                        Player[C].Effect2 = D;
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            // End If
                        }
                    }

                    // start the modern qScreen animation
                    if(!equalCase(evt.Name, "Level - Start") && g_compatibility.modern_section_change)
                    {
                        // the onscreen player
                        if(onscreen_plr == 0)
                        {
                            // no qScreen because there were no onscreen players
                        }
                        else if(numPlayers == 2)
                        {
                            double tX = 0.0;
                            double tY = 0.0;

                            if(warped_plr)
                            {
                                tX = vScreen[warped_plr].X - vScreen[onscreen_plr].X;
                                tY = vScreen[warped_plr].Y - vScreen[onscreen_plr].Y;
                            }

                            SoundPause[13] = 10;

                            // need two cycles to fully update the dynamic screens in the new level
                            SetupScreens(false);
                            DynamicScreen();
                            // CenterScreens();
                            if(vScreen[2].Visible)
                            {
                                for(int Z = 1; Z <= 2; Z++)
                                    GetvScreen(Z);
                            }
                            else
                                GetvScreenAverage();

                            // set up the dynamic screens in the new level
                            SetupScreens(false);
                            DynamicScreen();

                            // set the positions (including screen positions) in the old level
                            level[B] = tempLevel;
                            // CenterScreens();

                            if(vScreen[2].Visible)
                            {
                                for(int Z = 1; Z <= 2; Z++)
                                    GetvScreen(Z);
                            }
                            else
                                GetvScreenAverage();

                            // set the qScreen!
                            qScreen = true;
                            qScreenLoc[1] = vScreen[1];

                            // set the second qScreen if possible
                            if(vScreen[2].Visible)
                            {
                                qScreenLoc[2] = vScreen[2];
                            }

                            // special code to indicate the direction the other player was warped from
                            if(warped_plr && !vScreen[2].Visible)
                            {
                                double dSquare = tX * tX + tY * tY;

                                double xProp = tX * tX / dSquare;
                                double yProp = tY * tY / dSquare;

                                if(tX < 0)
                                    xProp *= -1;
                                if(tY < 0)
                                    yProp *= -1;

                                double maxShiftX = vScreen[1].Width / 4;
                                double maxShiftY = vScreen[1].Height / 4;
                                if(maxShiftX > 200)
                                    maxShiftX = 200;
                                if(maxShiftY > 150)
                                    maxShiftY = 150;

                                qScreenLoc[1].X += maxShiftX * xProp;
                                qScreenLoc[1].Y += maxShiftY * yProp;

                                // restrict to old level bounds
                                if(-qScreenLoc[1].X < level[B].X)
                                    qScreenLoc[1].X = -level[B].X;
                                if(-qScreenLoc[1].X + vScreen[1].Width > level[B].Width)
                                    qScreenLoc[1].X = -(level[B].Width - vScreen[1].Width);
                                if(-qScreenLoc[1].Y < level[B].Y)
                                    qScreenLoc[1].Y = -level[B].Y;
                                if(-qScreenLoc[1].Y + vScreen[1].Height > level[B].Height)
                                    qScreenLoc[1].Y = -(level[B].Height - vScreen[1].Height);
                            }

                            // restore the new level
                            level[B] = newLevel;
                        }
                        else
                        {
                            if(!qScreen)
                            {
                                qScreen = true;
                                qScreenLoc[1] = vScreen[1];
                            }
                        }

                        resetFrameTimer();
                    }
                    // legacy qScreen animation
                    else if(!equalCase(evt.Name, "Level - Start"))
                    {
                        if(numPlayers == 2 && DScreenType != 5)
                        {
                            level[B] = tempLevel;
                            screenLoc = vScreen[onscreen_plr];
                            SoundPause[13] = 10;

                            DynamicScreen();

                            // calculate the vScreen at non-splitscreen resolution (as the original game does)
                            GetvScreenAverage();

                            // enable qScreen!
                            qScreen = true;
                            qScreenLoc[1] = vScreen[1];

                            // pan to indicate warped player
                            if(int(screenLoc.Width) == 400)
                            {
                                if(qScreenLoc[1].X < screenLoc.X + screenLoc.Left)
                                    qScreenLoc[1].X += 200;
                                else
                                    qScreenLoc[1].X -= 200;
                            }

                            if(int(screenLoc.Height) == 300)
                            {
                                if(qScreenLoc[1].Y < screenLoc.Y + screenLoc.Top)
                                    qScreenLoc[1].Y += 150;
                                else
                                    qScreenLoc[1].Y -= 150;
                            }

                            // restrict to old level bounds
                            if(-qScreenLoc[1].X < level[B].X)
                                qScreenLoc[1].X = -level[B].X;
                            if(-qScreenLoc[1].X + ScreenW /*FrmMain.ScaleWidth*/ > level[B].Width)
                                qScreenLoc[1].X = -(level[B].Width - ScreenW);
                            if(-qScreenLoc[1].Y < level[B].Y)
                                qScreenLoc[1].Y = -level[B].Y;
                            if(-qScreenLoc[1].Y + ScreenH /*FrmMain.ScaleHeight*/ > level[B].Height)
                                qScreenLoc[1].Y = -(level[B].Height - ScreenH);

                            level[B] = static_cast<Location_t>(s.position);
                        }
                        else
                        {
                            qScreen = true;
                            qScreenLoc[1] = vScreen[1];
                        }

                        resetFrameTimer();
                    }
                }
            }

            for(auto &l : evt.HideLayer)
                HideLayer(l, NoEffect ? true : evt.LayerSmoke);

            for(auto &l : evt.ShowLayer)
                ShowLayer(l, NoEffect ? true : evt.LayerSmoke);

            for(auto &l : evt.ToggleLayer)
            {
                if(Layer[l].Hidden)
                    ShowLayer(l, evt.LayerSmoke);
                else
                    HideLayer(l, evt.LayerSmoke);
            }

#if 0 // Obsolete, replaced with a code above
            for(B = 0; B <= 20; B++)
            {
                if(NoEffect == true)
                {
                    HideLayer(evt.HideLayer[B], NoEffect);
                    ShowLayer(evt.ShowLayer[B], NoEffect);
                }
                else
                {
                    HideLayer(evt.HideLayer[B], evt.LayerSmoke);
                    ShowLayer(evt.ShowLayer[B], evt.LayerSmoke);
                }

                if(!(evt.ToggleLayer[B] == ""))
                {
                    for(C = 0; C <= maxLayers; C++)
                    {
                        if(Layer[C].Name == evt.ToggleLayer[B])
                        {
                            if(Layer[C].Hidden)
                            {
                                ShowLayer(Layer[C].Name, evt.LayerSmoke);
                            }
                            else
                            {
                                HideLayer(Layer[C].Name, evt.LayerSmoke);
                            }
                        }
                    }
                }
            }
#endif

            if(evt.MoveLayer != LAYER_NONE)
            {

                B = evt.MoveLayer;

                Layer[B].EffectStop = true;
                Layer[B].SpeedX = evt.SpeedX;
                Layer[B].SpeedY = evt.SpeedY;

                if(Layer[B].SpeedX == 0.f && Layer[B].SpeedY == 0.f)
                {
                    // stop layer
                    Layer[B].EffectStop = false;
                    for(int C : Layer[B].blocks)
                    {
                        Block[C].Location.SpeedX = double(Layer[B].SpeedX);
                        Block[C].Location.SpeedY = double(Layer[B].SpeedY);
                    }

                    if(g_compatibility.enable_climb_bgo_layer_move)
                    {
                        for(int C : Layer[B].BGOs)
                        {
                            if(BackgroundFence[Background[C].Type])
                            {
                                Background[C].Location.SpeedX = double(Layer[B].SpeedX);
                                Background[C].Location.SpeedY = double(Layer[B].SpeedY);
                            }
                        }
                    }

                    for(int C : Layer[B].NPCs)
                    {
                        if(NPCIsAVine[NPC[C].Type] || NPC[C].Type == 91)
                        {
                            NPC[C].Location.SpeedX = 0;
                            NPC[C].Location.SpeedY = 0;
                        }
                    }

                    // eventually, only re-join tables the first time the event has been triggered in a level
                    treeBlockJoinLayer(B);
                    treeBackgroundJoinLayer(B);
                    treeWaterJoinLayer(B);
                }
                else
                {
                    // these thresholds can be tweaked, but they balance the expense of querying more tables with the expense of updating locations in the main table
                    if(Layer[B].blocks.size() > 2)
                        treeBlockSplitLayer(B);

                    if(Layer[B].BGOs.size() > 2)
                        treeBackgroundSplitLayer(B);

                    if(Layer[B].waters.size() > 2)
                        treeWaterSplitLayer(B);
                }
            }

            if(!AutoUseModern) // Use legacy auto-scrolling when modern autoscrolling was never used here
            {
                if(g_compatibility.fix_autoscroll_speed)
                {
                    if(!autoScrollerChanged)
                    {
                        // Do set the autoscrool when non-zero values only, don't zero by other autoruns
                        if((evt.AutoX != 0.0 || evt.AutoY != 0.0) && IF_INRANGE(evt.AutoSection, 0, maxSections))
                        {
                            AutoX[evt.AutoSection] = evt.AutoX;
                            AutoY[evt.AutoSection] = evt.AutoY;
                        }
                    }
                }
                else if(IF_INRANGE(evt.AutoSection, 0, SDL_min(maxSections, maxEvents)))
                {
                    // Buggy behavior, see https://github.com/Wohlstand/TheXTech/issues/44
                    AutoX[evt.AutoSection] = Events[evt.AutoSection].AutoX;
                    AutoY[evt.AutoSection] = Events[evt.AutoSection].AutoY;
                }
            }

            if(evt.Text != STRINGINDEX_NONE)
            {
                MessageText = GetS(evt.Text);
                PauseGame(PauseCode::Message, 0);
                MessageText = "";
            }

            if(evt.Sound > 0)
            {
                if(SoundPause[evt.Sound] > 4)
                    SoundPause[evt.Sound] = 0;
                PlaySound(evt.Sound);
            }

            if(evt.EndGame == 1)
            {
                for(B = 0; B <= numSections; B++)
                    bgMusic[B] = 0;
                StopMusic();
                speedRun_bossDeadEvent();
                LevelMacroCounter = 0;
                LevelMacro = LEVELMACRO_GAME_COMPLETE_EXIT;
            }

            ForcedControls = (evt.Controls.AltJump ||
                              evt.Controls.AltRun ||
                              evt.Controls.Down ||
                              evt.Controls.Drop ||
                              evt.Controls.Jump ||
                              evt.Controls.Left ||
                              evt.Controls.Right ||
                              evt.Controls.Run ||
                              evt.Controls.Start ||
                              evt.Controls.Up);

            ForcedControl = evt.Controls;

            tempBool = false;
            if(evt.TriggerEvent != EVENT_NONE)
            {
                if(std::round(evt.TriggerDelay) == 0.0)
                {
                    // for(B = 0; B <= maxEvents; B++)
                    // {
                    //     if(Events[B].Name == evt.TriggerEvent)
                    //     {
                    //         if(Events[B].TriggerEvent == evt.Name)
                    //             tempBool = true;
                    //         break;
                    //     }
                    // }

                    // here tempBool prevented any order-2 circles from occurring
                    // if(!tempBool)
                    if(Events[evt.TriggerEvent].TriggerEvent != index)
                        ProcEvent(evt.TriggerEvent);
                }
                else
                {
                    newEventNum++;
                    NewEvent[newEventNum] = evt.TriggerEvent;
                    newEventDelay[newEventNum] = vb6Round(evt.TriggerDelay * 6.5);
                }
            }
        }
    }
}

void UpdateEvents()
{
    // this is for events that have a delay to call other events
    // this sub also updates the screen position for autoscroll levels
    int A = 0;
    int B = 0;
    if(FreezeNPCs)
        return;

    if(!GameMenu)
    {
        for(B = 1; B <= numPlayers; B++)
        {
            if(!(Player[B].Effect == 0 || Player[B].Effect == 3 || Player[B].Effect == 9 || Player[B].Effect == 10))
                return;
        }
    }

    if(newEventNum > 0)
    {
        for(A = 1; A <= newEventNum; A++)
        {
            if(newEventDelay[A] > 0)
                newEventDelay[A]--;
            else
            {
                ProcEvent(NewEvent[A]);
                newEventDelay[A] = newEventDelay[newEventNum];
                NewEvent[A] = NewEvent[newEventNum];
                newEventNum--;
            }
        }
    }

    for(A = 0; A <= numSections; A++)
    {
        if(AutoX[A] != 0.0f || AutoY[A] != 0.0f)
        {
            level[A].X += double(AutoX[A]);
            level[A].Width += double(AutoX[A]);
            level[A].Y += double(AutoY[A]);
            level[A].Height += double(AutoY[A]);
            if(level[A].Width > LevelREAL[A].Width)
            {
                level[A].Width = LevelREAL[A].Width;
                level[A].X = LevelREAL[A].Width - 800;
            }
            if(level[A].X < LevelREAL[A].X)
            {
                level[A].Width = LevelREAL[A].X + 800;
                level[A].X = LevelREAL[A].X;
            }
            if(level[A].Height > LevelREAL[A].Height)
            {
                level[A].Height = LevelREAL[A].Height;
                level[A].Y = LevelREAL[A].Height - 800;
            }
            if(level[A].Y < LevelREAL[A].Y)
            {
                level[A].Height = LevelREAL[A].Y + 800;
                level[A].Y = LevelREAL[A].Y;
            }
        }
    }
}

void CancelNewEvent(eventindex_t index)
{
    if(newEventNum <= 0)
        return; // Nothing to do

    for(int A = 1; A <= newEventNum; ++A)
    {
        if(index == NewEvent[A])
        {
            newEventDelay[A] = newEventDelay[newEventNum];
            NewEvent[A] = NewEvent[newEventNum];
            newEventNum--;
            --A;
        }
    }
}

bool EventWasTriggered(eventindex_t index)
{
    return !recentlyTriggeredEvents.empty() &&
            recentlyTriggeredEvents.find(index) != recentlyTriggeredEvents.end();
}

void ClearTriggeredEvents()
{
    if(!recentlyTriggeredEvents.empty())
        recentlyTriggeredEvents.clear();
}

void UpdateLayers()
{
    // this is mainly for moving layers
    int A = 0;
    int B = 0;
    // int C = 0;

    bool FreezeLayers = false;

    if(!GameMenu)
    {
        for(B = 1; B <= numPlayers; B++)
        {
            if(!(Player[B].Effect == 0 || Player[B].Effect == 3 || Player[B].Effect == 9 || Player[B].Effect == 10))
            {
                // moved this code into the loop over layers instead of repeating it per player
                // it has also been combined with the FreezeNPCs code since they did the same thing
                // in the original game

                // for(A = 0; A <= maxLayers; A++)
                // {
                //     if(Layer[A].Name != "" && (Layer[A].SpeedX != 0.f || Layer[A].SpeedY != 0.f) && Layer[A].EffectStop)
                //     {
                //         for(C = 1; C <= numBlock; C++)
                //         {
                //             if(Block[C].Layer == Layer[A].Name)
                //             {
                //                 Block[C].Location.SpeedX = 0;
                //                 Block[C].Location.SpeedY = 0;
                //             }
                //         }
                //     }
                // }
                FreezeLayers = true;
            }
        }
    }

    // set invalidate rate
    g_drawBlocks_invalidate_rate = 0;
    g_drawBGOs_invalidate_rate = 0;

    for(A = 0; A <= numLayers; A++)
    {
        // only consider non-empty, moving layers
        if(Layer[A].Name.empty() || (Layer[A].SpeedX == 0.f && Layer[A].SpeedY == 0.f))
            continue;

        // the layer does not move
        if(FreezeNPCs || (FreezeLayers && Layer[A].EffectStop))
        {
            {
                // Block-stopping code from earlier. Now together with the compat BGO layer move code.
                for(int B : Layer[A].blocks)
                {
                    Block[B].Location.SpeedX = 0;
                    Block[B].Location.SpeedY = 0;
                }

                if(g_compatibility.enable_climb_bgo_layer_move)
                {
                    for(int B : Layer[A].BGOs)
                    {
                        if(BackgroundFence[Background[B].Type])
                        {
                            Background[B].Location.SpeedX = 0;
                            Background[B].Location.SpeedY = 0;
                        }
                    }
                }

                if(g_compatibility.enable_climb_bgo_layer_move)
                {
                    for(int B : Layer[A].NPCs)
                    {
                        if(NPC[B].Type == 91 || NPC[B].Type == 211 || NPCIsAVine[NPC[B].Type])
                        {
                            NPC[B].Location.SpeedX = 0;
                            NPC[B].Location.SpeedY = 0;
                        }
                    }
                }
            }
        }
        // the layer moves!
        else
        {
            // if(!(FreezeLayers && Layer[A].EffectStop))
            {
                Layer[A].OffsetX += double(Layer[A].SpeedX);
                Layer[A].OffsetY += double(Layer[A].SpeedY);

                // no longer needed thanks to block quadtree, but used to reproduce some buggy behaviors
                // move the sort invalidation out of the loop over blocks
                if(!Layer[A].blocks.empty() && Layer[A].SpeedX != 0.f && g_compatibility.emulate_classic_block_order)
                {
                    if(BlocksSorted)
                        BlocksSorted = false;
                }

                if(!Layer[A].blocks.empty())
                {
                    if(std::abs(Layer[A].SpeedX) > g_drawBlocks_invalidate_rate)
                        g_drawBlocks_invalidate_rate = std::abs(Layer[A].SpeedX);
                    if(std::abs(Layer[A].SpeedY) > g_drawBlocks_invalidate_rate)
                        g_drawBlocks_invalidate_rate = std::abs(Layer[A].SpeedY);
                }

                if(!Layer[A].BGOs.empty())
                {
                    if(std::abs(Layer[A].SpeedX) > g_drawBGOs_invalidate_rate)
                        g_drawBGOs_invalidate_rate = std::abs(Layer[A].SpeedX);
                    if(std::abs(Layer[A].SpeedY) > g_drawBGOs_invalidate_rate)
                        g_drawBGOs_invalidate_rate = std::abs(Layer[A].SpeedY);
                }

                // is the layer currently included in the main block quadtree?
                bool inactive = !treeBlockLayerActive(A);

                for(int B : Layer[A].blocks)
                {
                    // if(Block[B].Layer == Layer[A].Name)
                    //{
                    Block[B].Location.X += double(Layer[A].SpeedX);
                    Block[B].Location.Y += double(Layer[A].SpeedY);
                    Block[B].Location.SpeedX = double(Layer[A].SpeedX);
                    Block[B].Location.SpeedY = double(Layer[A].SpeedY);

                    if(inactive)
                        treeBlockUpdateLayer(A, B);
                    //}
                }

                // is the layer currently included in the main BGO quadtree?
                inactive = !treeBackgroundLayerActive(A);

                // int allBGOs = numBackground + numLocked;
                for(int B : Layer[A].BGOs)
                {
                    // if(Background[B].Layer == Layer[A].Name)
                    //{
                    Background[B].Location.X += double(Layer[A].SpeedX);
                    Background[B].Location.Y += double(Layer[A].SpeedY);
                    if(g_compatibility.enable_climb_bgo_layer_move && BackgroundFence[Background[B].Type])
                    {
                        Background[B].Location.SpeedX = double(Layer[A].SpeedX);
                        Background[B].Location.SpeedY = double(Layer[A].SpeedY);
                    }

                    if(inactive)
                        treeBackgroundUpdateLayer(A, B);
                    //}
                }

                // is the layer currently included in the main water quadtree?
                inactive = !treeWaterLayerActive(A);

                for(int B : Layer[A].waters)
                {
                    // if(Water[B].Layer == Layer[A].Name)
                    //{
                    Water[B].Location.X += double(Layer[A].SpeedX);
                    Water[B].Location.Y += double(Layer[A].SpeedY);

                    if(inactive)
                        treeWaterUpdateLayer(A, B);
                    //}
                }

                for(int B : Layer[A].NPCs)
                {
                    // if(NPC[B].Layer == Layer[A].Name)
                    {
                        NPC[B].DefaultLocation.X += double(Layer[A].SpeedX);
                        NPC[B].DefaultLocation.Y += double(Layer[A].SpeedY);

                        if(!NPC[B].Active || NPC[B].Generator || NPC[B].Effect != 0 ||
                           NPCIsACoin[NPC[B].Type] || NPC[B].Type == 8 || NPC[B].Type == 37 ||
                           NPC[B].Type == 51 || NPC[B].Type == 52 || NPC[B].Type == 46 ||
                           NPC[B].Type == 93 || NPC[B].Type == 74 || NPCIsAVine[NPC[B].Type] ||
                           NPC[B].Type == 192 || NPC[B].Type == 197 || NPC[B].Type == 91 ||
                           NPC[B].Type == 211 || NPC[B].Type == 256 || NPC[B].Type == 257 ||
                           NPC[B].Type == 245)
                        {
                            if(NPC[B].Type == 91 || NPC[B].Type == 211)
                            {
                                NPC[B].Location.SpeedX = double(Layer[A].SpeedX);
                                NPC[B].Location.SpeedY = double(Layer[A].SpeedY);
                            }
                            else if(NPCIsAVine[NPC[B].Type])
                            {
                                NPC[B].Location.SpeedX = double(Layer[A].SpeedX);
                                NPC[B].Location.SpeedY = double(Layer[A].SpeedY);
                            }

                            if(!NPC[B].Active)
                            {
                                NPC[B].Location.X = NPC[B].DefaultLocation.X;
                                NPC[B].Location.Y = NPC[B].DefaultLocation.Y;
                                if(NPC[B].Type == 8 || NPC[B].Type == 74 || NPC[B].Type == 93 ||
                                   NPC[B].Type == 256 || NPC[B].Type == 245)
                                    NPC[B].Location.Y += NPC[B].DefaultLocation.Height;
                                else if(NPC[B].Type == 52 && fiEqual(NPC[B].Direction, -1))
                                    NPC[B].Location.X += NPC[B].DefaultLocation.Width;
                            }
                            else
                            {
                                NPC[B].Location.X += double(Layer[A].SpeedX);
                                NPC[B].Location.Y += double(Layer[A].SpeedY);
                            }

                            if(NPC[B].Effect == 4)
                            {
                                if(NPC[B].Effect3 == 1 || NPC[B].Effect3 == 3)
                                    NPC[B].Effect2 += double(Layer[A].SpeedY);
                                else
                                    NPC[B].Effect2 += double(Layer[A].SpeedX);
                            }

                            if(!NPC[B].Active)
                            {
                                if(NPC[B].AttLayer != LAYER_NONE && NPC[B].AttLayer != LAYER_DEFAULT)
                                {
                                    Layer[NPC[B].AttLayer].SpeedX = Layer[A].SpeedX;
                                    Layer[NPC[B].AttLayer].SpeedY = Layer[A].SpeedY;
                                }
                            }

                            treeNPCUpdate(B);
                        }
                    }
                }

                for(int B : Layer[A].warps)
                {
                    Warp[B].Entrance.X += double(Layer[A].SpeedX);
                    Warp[B].Entrance.Y += double(Layer[A].SpeedY);
                    Warp[B].Exit.X += double(Layer[A].SpeedX);
                    Warp[B].Exit.Y += double(Layer[A].SpeedY);
                }
            }
        }
    }
}



// the code for synchronizing the layers of objects

void syncLayersTrees_AllBlocks()
{
    // treeLevelCleanBlockLayers();
    for(int block = 1; block <= numBlock; block++)
    {
        syncLayersTrees_Block(block);
    }
}

void syncLayersTrees_Block(int block)
{
    invalidateDrawBlocks();

    for(int layer = 0; layer <= numLayers; layer++)
    {
        if(layer != Block[block].Layer)
        {
            Layer[layer].blocks.erase(block);
            treeBlockRemoveLayer(layer, block);
        }
    }

    int layer = Block[block].Layer;
    if(block <= numBlock)
    {
        treeBlockUpdateLayer(layer, block);
        if(layer != LAYER_NONE)
            Layer[layer].blocks.insert(block);
    }
    else
    {
        treeBlockRemoveLayer(layer, block);
        if(layer != LAYER_NONE)
        {
            Layer[layer].blocks.erase(block);
        }
    }
}

void syncLayersTrees_Block_SetHidden(int block) // set block hidden based on layer
{
    syncLayersTrees_Block(block);
    if(Block[block].Layer != LAYER_NONE)
        Block[block].Hidden = Layer[Block[block].Layer].Hidden;
}

void syncLayers_AllNPCs()
{
    NPCQueues::clear();

    for(int npc = 1; npc <= numNPCs; npc++)
    {
        syncLayers_NPC(npc);
    }
}

void syncLayers_NPC(int npc)
{
    for(int layer = 0; layer <= numLayers; layer++)
    {
        if(npc <= numNPCs && NPC[npc].Layer == layer)
            Layer[layer].NPCs.insert(npc);
        else
            Layer[layer].NPCs.erase(npc);
    }

    NPCQueues::update(npc);

    SDL_assert_release(npc > 0);

    if(npc <= numNPCs)
        treeNPCUpdate(npc);
    else
        treeNPCRemove(npc);
}

void syncLayers_AllBGOs()
{
    for(int bgo = 1; bgo <= numBackground + numLocked; bgo++)
    {
        syncLayers_BGO(bgo);
    }
}

void syncLayers_BGO(int bgo)
{
    invalidateDrawBGOs();

    for(int layer = 0; layer <= numLayers; layer++)
    {
        if(layer != Background[bgo].Layer)
        {
            treeBackgroundRemoveLayer(layer, bgo);
            Layer[layer].BGOs.erase(bgo);
        }
    }

    int layer = Background[bgo].Layer;
    if(bgo <= numBackground + numLocked)
    {
        treeBackgroundUpdateLayer(layer, bgo);
        if(layer != LAYER_NONE)
            Layer[layer].BGOs.insert(bgo);
    }
    else
    {
        treeBackgroundRemoveLayer(layer, bgo);
        if(layer != LAYER_NONE)
            Layer[layer].BGOs.erase(bgo);
    }
}

void syncLayers_Warp(int warp)
{
    for(int layer = 0; layer <= numLayers; layer++)
    {
        if(warp <= numWarps && Warp[warp].Layer == layer)
            Layer[layer].warps.insert(warp);
        else
            Layer[layer].warps.erase(warp);
    }
}

void syncLayers_Water(int water)
{
    for(int layer = 0; layer <= numLayers; layer++)
    {
        if(layer != Water[water].Layer)
        {
            treeWaterRemoveLayer(layer, water);
            Layer[layer].waters.erase(water);
        }
    }

    int layer = Water[water].Layer;
    if(water <= numWater)
    {
        treeWaterUpdateLayer(layer, water);
        if(layer != LAYER_NONE)
            Layer[layer].waters.insert(water);
    }
    else
    {
        treeWaterRemoveLayer(layer, water);
        if(layer != LAYER_NONE)
            Layer[layer].waters.erase(water);
    }
}
