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
#ifndef CONFIG_HOOKS_H
#define CONFIG_HOOKS_H

void config_res_set();
void config_asset_pack_set();
void config_language_set();
void config_rendermode_set();
void config_fullscreen_set();
#ifdef RENDER_FULLSCREEN_TYPES_SUPPORTED
void config_fullscreen_type_set();
#endif
void config_mountdrums_set();
void config_screenmode_set();
void config_audiofx_set();
void config_audio_set();
void config_music_volume_set();
void config_log_level_set();
void config_integrations_set();
void config_3ds_inaccurate_gifs_set();
void config_compat_changed();

#endif
