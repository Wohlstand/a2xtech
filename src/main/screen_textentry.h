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
#ifndef SCREEN_TEXTENTRY_H
#define SCREEN_TEXTENTRY_H

#include <string>

namespace TextEntryScreen
{

extern std::string Text;

// Initialize TextEntryScreen. [Temporarily returns whether a PauseGame(PauseCode::None) should be triggered.]
bool Init(const std::string& Prompt, void (*callback)(), const std::string Value = std::string());

// Initialize and run TextEntryScreen, waiting for the pause loop to complete, and returning the entered text. Deprecated.
const std::string& Run(const std::string& Prompt, const std::string Value = std::string());

void Render();
bool Logic();

void CursorLeft();
void CursorRight();
void Insert(const char* text);
void Backspace();
void Commit();

} // namespace TextEntryScreen

#endif // #ifndef SCREEN_TEXTENTRY_H
