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

#include <bitset>
#include <array>
#include <algorithm>

#include <PGE_File_Formats/file_formats.h>

#include <Logger/logger.h>

#include "npc_id.h"

#include "main/level_medals.h"
#include "main/level_save_info.h"

#include "logic/object_graph.h"

CurLevelMedals_t g_curLevelMedals;

//! find the LevelSaveInfo for the current level
static inline LevelSaveInfo_t* s_findSaveInfo()
{
    for(int i = 1; i <= numWorldLevels; ++i)
    {
        if(WorldLevel[i].FileName == FileNameFull)
            return &WorldLevel[i].save_info;
    }

    for(auto& e : LevelWarpSaveEntries)
    {
        if(e.levelPath == FileNameFull)
            return &e.save_info;
    }

    return nullptr;
}

//! find the next matching LevelSaveInfo for the current level
static inline LevelSaveInfo_t* s_nextSaveInfo(LevelSaveInfo_t* prev)
{
    for(int i = 1; i <= numWorldLevels; ++i)
    {
        if(WorldLevel[i].FileName == FileNameFull)
        {
            LevelSaveInfo_t* info = &WorldLevel[i].save_info;

            if(info > prev)
                return info;
        }
    }

    return nullptr;
}

void CurLevelMedals_t::get(uint8_t idx)
{
    if(InHub())
        return;

    if(idx < 8 && idx < max)
    {
        got |= (1 << idx);
        life |= (1 << idx);
    }
}

void CurLevelMedals_t::on_any_death()
{
    life = 0;
}

void CurLevelMedals_t::on_all_dead()
{
    life = 0;
    got = m_checkpoint;
}

void CurLevelMedals_t::on_checkpoint()
{
    if(InHub())
        return;

    m_checkpoint = got;
}

void CurLevelMedals_t::reset_checkpoint()
{
    got = 0;
    life = 0;
    m_checkpoint = 0;
}

void CurLevelMedals_t::reset_lvl()
{
    max = 0;
    prev = 0;
}

void CurLevelMedals_t::prepare_lvl(const LevelData& loadedLevel)
{
    LevelSaveInfo_t* info = s_findSaveInfo();

    if(!info && LevelWarpSaveEntries.size() != 0xFFFF)
    {
        LevelSaveInfo_t info_init = InitLevelSaveInfo(loadedLevel);

        if(info_init.inited())
        {
            LevelWarpSaveEntries.push_back({FileNameFull, info_init});
            info = &LevelWarpSaveEntries[LevelWarpSaveEntries.size() - 1].save_info;
        }
    }

    if(!info)
    {
        reset_lvl();
        return;
    }

    max = info->max_medals;
    prev = info->medals_got;
}

void CurLevelMedals_t::commit()
{
    LevelSaveInfo_t* info = s_findSaveInfo();

    if(!info)
        return;

    info->medals_got |= got;

    int old_best_count = 0;
    int new_best_count = 0;

    for(int i = 0; i < 8 && i < info->max_medals; ++i)
    {
        if(info->medals_best & (1 << i))
            old_best_count++;

        if(life & (1 << i))
            new_best_count++;
    }

    if(new_best_count > old_best_count)
        info->medals_best = life;

    // copy this to any other world levels with the same filename
    while(LevelSaveInfo_t* next = s_nextSaveInfo(info))
    {
        *next = *info;
        info = next;
    }
}

void OrderMedals()
{
    using dist_and_hit_index = std::pair<uint16_t, int16_t>;

    // used to track medals without creator-specified indexes; first short is distance from start, second short is NPC array index
    std::array<dist_and_hit_index, 8> auto_medals;
    int auto_medal_count = 0;

    // used to track medals with creator indexes
    std::bitset<8> used_indexes;

    // look for medals
    for(int i = 1; i <= numNPCs; ++i)
    {
        NPC_t& n = NPC[i];

        bool is_container = (n.Type == NPCID_ITEM_BURIED || n.Type == NPCID_ITEM_POD ||
                             n.Type == NPCID_ITEM_BUBBLE || n.Type == NPCID_ITEM_THROWER);

        bool contains_medal = is_container && n.Special == NPCID_MEDAL;

        // allow medals only
        if(n.Type != NPCID_MEDAL && !contains_medal)
            continue;

        // don't count friendly medals (except, thrown medals can be collected)
        if(n.Inert && n.Type != NPCID_ITEM_THROWER)
            continue;

        // medal won't be counted (at all) if out-of-range
        if(n.Variant > 8)
            continue;

        // record that the medal index has been used
        if(n.Variant > 0)
        {
            used_indexes[n.Variant - 1] = true;
            continue;
        }

        // if not specified, then try to fill into the array to auto-assign an index
        if(auto_medal_count < 8)
        {
            auto_medals[auto_medal_count] = {0, static_cast<int16_t>(i)};
            auto_medal_count++;
        }

        // otherwise, Variant will be left as 0 and medal won't be counted
    }

    // find the distance of unspecified medals from level start, and sort
    if(auto_medal_count > 1)
    {
        // need to order the auto_hits; get an idea of the layout of the level
        ObjectGraph::Graph graph;
        ObjectGraph::FillGraph(graph);

        for(int auto_i = 0; auto_i < auto_medal_count; ++auto_i)
        {
            const NPC_t& n = NPC[auto_medals[auto_i].second];

            double coord = graph.distance_from_start({n.Location.X, n.Location.Y});

            if(graph.furthest_dist)
                coord /= graph.furthest_dist;

            D_pLogDebug("Medal with NPC ID %d at %f from level start to level end", auto_medals[auto_i].second, coord);

            // 1.0 is the furthest exit, and it's never negative
            if(coord > 2.0)
                coord = 2.0;

            auto_medals[auto_i].first = static_cast<uint16_t>(coord / 2.0 * 0xFFFF);
        }

        // sort by distance
        std::sort(auto_medals.begin(), auto_medals.begin() + auto_medal_count);
    }

    // auto-assign indexes to unspecified medals
    for(int auto_i = 0; auto_i < auto_medal_count; ++auto_i)
    {
        NPC_t& n = NPC[auto_medals[auto_i].second];

        // try to find an index
        for(int i = 0; i < 8; ++i)
        {
            if(!used_indexes[i])
            {
                D_pLogDebug("Medal with NPC ID %d gets index %d", auto_medals[auto_i].second, i + 1);

                n.Variant = i + 1;
                used_indexes[i] = true;
                break;
            }
        }

        // if failed, Variant will be left as 0 and medal won't be counted
    }
}
