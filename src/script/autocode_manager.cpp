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

#include <sstream>
#include <fstream>
#include <Utils/files.h>

#include "autocode_manager.h"
#include "globals.h"
#include "lunamisc.h"
#include "lunaspriteman.h"

#define PARSEDEBUG true



AutocodeManager gAutoMan;

// CTOR
AutocodeManager::AutocodeManager()
{
    Clear(true);
    m_Enabled = true;
}

AutocodeManager::~AutocodeManager()
{
    Clear(true);
}

// CLEAR - Delete all autocodes and clear lists (and reset hearts to 2)
void AutocodeManager::Clear(bool clear_global_codes)
{
    if(clear_global_codes)
    {
        while(m_GlobalCodes.empty() == false)
        {
            delete m_GlobalCodes.front();
            m_GlobalCodes.pop_front();
        }
        m_GlobalCodes.clear();
    }
    while(m_Autocodes.empty() == false)
    {
        delete m_Autocodes.front();
        m_Autocodes.pop_front();
    }
    while(m_InitAutocodes.empty() == false)
    {
        delete m_InitAutocodes.front();
        m_InitAutocodes.pop_front();
    }
    while(m_CustomCodes.empty() == false)
    {
        delete m_CustomCodes.front();
        m_CustomCodes.pop_front();
    }
    m_Autocodes.clear();
    m_InitAutocodes.clear();
    m_CustomCodes.clear();

    m_Hearts = 2;
}

// READ FILE - Read the autocode file in the level folder
void AutocodeManager::ReadFile(std::string dir_path)
{
    // Form full path
    //string full_path = RemoveExtension(dir_path);
    std::string full_path = dir_path.append(FileName);
    full_path = full_path.append("/");
    full_path = full_path.append(AUTOCODE_FNAME);

    FILE *code_file = Files::utf8_fopen(full_path.c_str(), "rb");
    if(!code_file)
        return;

    m_Enabled = true;
    Parse(code_file, false);

    std::fclose(code_file);
}

// READ WORLD - Read the world autocode file in the world folder
void AutocodeManager::ReadWorld(std::string dir_path)
{
    std::string full_path = dir_path;

    full_path = full_path.append("/");
    full_path = full_path.append(WORLDCODE_FNAME);

    FILE *code_file = Files::utf8_fopen(full_path.c_str(), "rb");
    if(!code_file)
        return;

    Parse(code_file, false);
    std::fclose(code_file);
}

// READ GLOBALS - Read the global code file
void AutocodeManager::ReadGlobals(std::string dir_path)
{
    std::string full_path = dir_path.append("/");
    full_path = full_path.append(GLOBALCODE_FNAME);

    FILE *code_file = Files::utf8_fopen(full_path.c_str(), "rb");
    if(!code_file)
        return;

    Parse(code_file, true);

    std::fclose(code_file);
}

// PARSE    - Parse the autocode file and populate manager with the contained code/settings
//            Doesn't delete codes already in the lists
void AutocodeManager::Parse(FILE *code_file, bool add_to_globals)
{
    char wbuf[2000];
    char combuf[150];
    SDL_memset(wbuf, 0, 2000 * sizeof(char));
    SDL_memset(combuf, 0, 150 * sizeof(char));
    int cur_section = 0;
    AutocodeType ac_type = AT_Invalid;
    double target = 0;
    double param1 = 0;
    double param2 = 0;
    double param3 = 0;
    double length = 0;
    int btarget = 0;
    int bparam1 = 0;
    int bparam2 = 0;
    int bparam3 = 0;
    int blength = 0;
    char wstrbuf[1000];
    char wrefbuf[128];
    SDL_memset(wstrbuf, 0, 1000 * sizeof(char));
    SDL_memset(wrefbuf, 0, 128 * sizeof(char));

    std::fseek(code_file, 0, SEEK_SET);

    //char* dbg = "ParseDbgEOF";
    while(!std::feof(code_file))
    {
        // Get a line and reset buffers
        SDL_memset(wbuf, 0, 2000 * sizeof(char));
        SDL_memset(wstrbuf, 0, 1000 * sizeof(char));
        SDL_memset(wrefbuf, 0, 128 * sizeof(char));
        SDL_memset(combuf, 0, 150 * sizeof(char));
        target = 0;
        param1 = 0;
        param2 = 0;
        param3 = 0;
        length = 0;
        btarget = 0;
        bparam1 = 0;
        bparam2 = 0;
        bparam3 = 0;
        blength = 0;
        std::fgets(wbuf, 2000, code_file);

        // Is it a comment?
        if(SDL_strstr(wbuf, "//") != nullptr)
            continue;

        // Is it a new section header?
        if(wbuf[0] == L'#')
        {
            // Is it the level load header?
            if(wbuf[1] == L'-')
                cur_section = -1;
            else   // Else, parse the section number
                cur_section = SDL_atoi(&wbuf[1]);
        }
        else   // Else, parse as a command
        {

            // Is there a variable reference marker?
            if(wbuf[0] == '$')
            {
                int i = 1;
                for(i = 1; wbuf[i] != L',' && wbuf[i] != '\x0D' && wbuf[i] != '\x0A' && i < 126; i++)
                {}

                wbuf[i] = '\x00'; // Turn the comma into a null terminator
                SDL_strlcpy(wrefbuf, &wbuf[1], 128); // Copy new string into wrefbuf
                SDL_strlcpy(wbuf, &wbuf[i + 1], 2000); // The rest of the line minus the ref is the new wbuf
            }

            ac_type = Autocode::EnumerizeCommand(wbuf);

            // Decimal pass
            int hits = SDL_sscanf(wbuf, PARSE_FMT_STR, combuf, &target, &param1, &param2, &param3, &length, wstrbuf);

            // Integer pass
            int bhits = SDL_sscanf(wbuf, PARSE_FMT_STR_2, combuf, &btarget, &bparam1, &bparam2, &bparam3, &blength, wstrbuf);

            // Check for formatting failure
            if(hits < 3 && bhits < 3)
                continue;

            // Check for hexadecimal inputs
            if(true)
            {
                if(target == 0 && btarget != 0)
                    target = btarget;
                if(param1 == 0 && bparam1 != 0)
                    param1 = bparam1;
                if(param2 == 0 && bparam2 != 0)
                    param2 = bparam2;
                if(param3 == 0 && bparam3 != 0)
                    param3 = bparam3;
                if(length == 0 && blength != 0)
                    length = blength;
            }

            // Register new autocode

            std::string ac_str = std::string(wstrbuf); // Get the string out of strbuf
            ac_str.erase(ac_str.find_last_not_of(" \n\r\t") + 1);

            std::string ref_str = std::string(wrefbuf); // Get var reference string if any

            Autocode *newcode = new Autocode(ac_type, target, param1, param2, param3, ac_str, length, cur_section, ref_str);
            if(!add_to_globals)
            {
                if(newcode->m_Type < 10000 || newcode->MyRef.length() > 0)
                    m_Autocodes.emplace_back(std::move(newcode));
                else   // Sprite components (type 10000+) with no reference go into callable component list
                {
                    gSpriteMan.m_ComponentList.push_back(Autocode::GenerateComponent(newcode));
                    delete newcode;
                }
            }
            else
            {
                if(newcode->m_Type < 10000)
                    m_GlobalCodes.emplace_back(std::move(newcode));
            }
        }
    }//while
}

// DO EVENTS
void AutocodeManager::DoEvents(bool init)
{
    //char* dbg = "DO EVENTS DBG";
    if(m_Enabled)
    {
        // Add any outstanding custom events
        while(!m_CustomCodes.empty())
        {
            m_Autocodes.push_back(m_CustomCodes.back());
            m_CustomCodes.pop_back();
        }

        // Do each code
        for(std::list<Autocode *>::iterator iter = m_Autocodes.begin(), end = m_Autocodes.end(); iter != end; ++iter)
            (*iter)->Do(init);
    }

    if(m_GlobalEnabled)
    {
        // Do each global code
        for(std::list<Autocode *>::iterator iter = m_GlobalCodes.begin(), end = m_GlobalCodes.end(); iter != end; ++iter)
            (*iter)->Do(init);
    }
}

// GET EVENT BY REF
Autocode *AutocodeManager::GetEventByRef(std::string ref_name)
{
    if(ref_name.length() > 0)
    {
        for(std::list<Autocode *>::iterator iter = m_Autocodes.begin(), end = m_Autocodes.end(); iter != end; ++iter)
        {
            if((*iter)->MyRef == ref_name)
                return (*iter);
        }
    }
    return NULL;
}

// DELETE EVENT -- Expires any command that matches the given name
void AutocodeManager::DeleteEvent(std::string ref_name)
{
    if(ref_name.length() > 0)
    {
        for(std::list<Autocode *>::iterator iter = m_Autocodes.begin(), end = m_Autocodes.end(); iter != end; ++iter)
        {
            if((*iter)->MyRef == ref_name)
                (*iter)->Expired = true;
        }
    }
}

// CLEAN EXPIRED - Don't call this while iterating over codes
void AutocodeManager::ClearExpired()
{
    //char* dbg = "CLEAN EXPIRED DBG";
    std::list<Autocode *>::iterator iter = m_Autocodes.begin();
    std::list<Autocode *>::iterator end  = m_Autocodes.end();

    while(iter != m_Autocodes.end())
    {
        //Autocode* ac = *iter;
        if((*iter)->Expired || (*iter)->m_Type == AT_Invalid)
        {
            delete(*iter);
            iter = m_Autocodes.erase(iter);
        }
        else
            ++iter;
    }

    iter = m_GlobalCodes.begin();
    end  = m_GlobalCodes.end();

    while(iter != m_GlobalCodes.end())
    {
        //Autocode* ac = *iter;
        if((*iter)->Expired || (*iter)->m_Type == AT_Invalid)
        {
            delete(*iter);
            iter = m_GlobalCodes.erase(iter);
        }
        else
            ++iter;
    }
}

// ACTIVATE CUSTOM EVENTS
void AutocodeManager::ActivateCustomEvents(int new_section, int eventcode)
{
    //char* dbg = "ACTIVATE CUSTOM DBG";
    if(m_Enabled)
    {
        for(std::list<Autocode *>::iterator iter = m_Autocodes.begin(), end = m_Autocodes.end(); iter != end; ++iter)
        {

            // Activate copies of events with 'eventcode' and move them to 'new_section'
            if((*iter)->ActiveSection == eventcode && (*iter)->Activated == false && !(*iter)->Expired)
            {
                Autocode *newcode = (*iter)->MakeCopy();
                newcode->Activated = true;
                newcode->ActiveSection = (new_section < 1000 ? (new_section - 1) : new_section);
                newcode->Length = (*iter)->m_OriginalTime;
                m_CustomCodes.push_front(newcode);
            }

        }

        for(std::list<Autocode *>::iterator iter = m_GlobalCodes.begin(), end = m_GlobalCodes.end(); iter != end; ++iter)
        {

            // Activate copies of events with 'eventcode' and move them to 'new_section'
            if((*iter)->ActiveSection == eventcode && (*iter)->Activated == false && !(*iter)->Expired)
            {
                Autocode *newcode = (*iter)->MakeCopy();
                newcode->Activated = true;
                newcode->ActiveSection = (new_section < 1000 ? (new_section - 1) : new_section);
                newcode->Length = (*iter)->m_OriginalTime;
                m_CustomCodes.push_front(newcode);
            }

        }
    }
}

// FORCE EXPIRE -- Expire all codes in section
void AutocodeManager::ForceExpire(int section)
{
    //char* dbg = "FORCE EXPIRE DBG";
    if(m_Enabled)
    {
        for(std::list<Autocode *>::iterator iter = m_Autocodes.begin(), end = m_Autocodes.end(); iter != end; ++iter)
        {
            if((*iter)->ActiveSection == section)
                (*iter)->Expired = true;
        }

        for(std::list<Autocode *>::iterator iter = m_GlobalCodes.begin(), end = m_GlobalCodes.end(); iter != end; ++iter)
        {
            if((*iter)->ActiveSection == section)
                (*iter)->Expired = true;
        }
    }
}

// FIND MATCHING -- Return a reference to the first autocode that matches, or 0
Autocode *AutocodeManager::FindMatching(int section, std::string soughtstr)
{
    //char* dbg = "FIND MATCHING DBG";
    for(std::list<Autocode *>::iterator iter = m_Autocodes.begin(), end = m_Autocodes.end(); iter != end; ++iter)
    {
        if((*iter)->ActiveSection == section && (*iter)->MyString == soughtstr)
            return (*iter);
    }
    return 0;
}

// VAR OPERATION -- Do something to a variable in the user variable bank
bool AutocodeManager::VarOperation(std::string var_name, double value, OPTYPE operation_to_do)
{
    if(var_name.length() > 0)
    {
        // Create var if doesn't exist
        InitIfMissing(&gAutoMan.m_UserVars, var_name, 0);

        double var_val = m_UserVars[var_name];

        // Do the operation
        OPTYPE oper = operation_to_do;
        switch(oper)
        {
        case OP_Assign:
            m_UserVars[var_name] = value;
            return true;
        case OP_Add:
            m_UserVars[var_name] = var_val + value;
            return true;
        case OP_Sub:
            m_UserVars[var_name] = var_val - value;
            return true;
        case OP_Mult:
            m_UserVars[var_name] = var_val * value;
            return true;
        case OP_Div:
            if(value == 0)
                return false;
            m_UserVars[var_name] = var_val / value;
            return true;
        case OP_XOR:
            m_UserVars[var_name] = (int)var_val ^ (int)value;
            return true;
        default:
            return true;
        }
    }
    return false;
}

// VAR EXISTS
bool AutocodeManager::VarExists(std::string var_name)
{
    return m_UserVars.find(var_name) == m_UserVars.end() ? false : true;
}

// GET VAR
double AutocodeManager::GetVar(std::string var_name)
{
    if(!VarExists(var_name))
        return 0;

    return m_UserVars[var_name];
}
