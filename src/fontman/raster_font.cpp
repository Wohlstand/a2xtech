/*
 * Moondust, a free game engine for platform game making
 * Copyright (c) 2014-2023 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This software is licensed under a dual license system (MIT or GPL version 3 or later).
 * This means you are free to choose with which of both licenses (MIT or GPL version 3 or later)
 * you want to use this software.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You can see text of MIT license in the LICENSE.mit file you can see in Engine folder,
 * or see https://mit-license.org/.
 *
 * You can see text of GPLv3 license in the LICENSE.gpl3 file you can see in Engine folder,
 * or see <http://www.gnu.org/licenses/>.
 */

#include "raster_font.h"

#include "../core/render.h"
#include <fmt_format_ne.h>
#include <Logger/logger.h>
#include <IniProcessor/ini_processing.h>
#include <Utils/files.h>
#include <DirManager/dirman.h>

#ifdef THEXTECH_ENABLE_TTF_SUPPORT
#include "ttf_font.h"
#endif
#include "font_manager_private.h"

RasterFont::RasterFont() : BaseFontEngine()
{
    static uint64_t fontNumberCount = 0;//This number will be used as default font name
    m_letterWidth    = 0;
    m_letterHeight   = 0;
    m_spaceWidth     = 0;
    m_interLetterSpace = 0;
    m_newlineOffset  = 0;
    m_glyphOffsetX   = 0;
    m_glyphOffsetY   = 0;
    m_matrixWidth    = 0;
    m_matrixHeight   = 0;
    m_isReady        = false;
    m_ttfBorders     = false;
    m_fontName       = fmt::format_ne("font{0}", fontNumberCount++);
}

RasterFont::~RasterFont()
{
    for(StdPicture &t : m_texturesBank)
        XRender::deleteTexture(t);
    m_texturesBank.clear();
}

void RasterFont::loadFont(std::string font_ini)
{
    if(!Files::fileExists(font_ini))
    {
        pLogWarning("Can't load font %s: file not exist", font_ini.c_str());
        return;
    }

    std::string root = DirMan(Files::dirname(font_ini)).absolutePath() + "/";
    IniProcessing font(font_ini);

    size_t tables = 0;
    font.beginGroup("font");
    font.read("tables", tables, 0);
    font.read("name", m_fontName, m_fontName);
    font.read("ttf-borders", m_ttfBorders, false);
    font.read("space-width", m_spaceWidth, 0);
    font.read("interletter-space", m_interLetterSpace, 0);
    font.read("newline-offset", m_newlineOffset, 0);
    font.read("glyph-offset-x", m_glyphOffsetX, 0);
    font.read("glyph-offset-y", m_glyphOffsetY, 0);
    font.endGroup();
    std::vector<std::string> tables_list;
    tables_list.reserve(tables);

    font.beginGroup("tables");

    for(size_t i = 1; i <= tables; i++)
    {
        std::string table;
        font.read(fmt::format_ne("table{0}", i).c_str(), table, "");
        if(!table.empty())
            tables_list.push_back(table);
    }

    font.endGroup();

    for(std::string &tbl : tables_list)
        loadFontMap(root + tbl);
}

void RasterFont::loadFontMap(std::string fontmap_ini)
{
    std::string root = DirMan(Files::dirname(fontmap_ini)).absolutePath() + "/";

    if(!Files::fileExists(fontmap_ini))
    {
        pLogWarning("Can't load font map %s: file not exist", fontmap_ini.c_str());
        return;
    }

    IniProcessing font(fontmap_ini);
    std::string texFile;
    uint32_t w = m_letterWidth, h = m_letterHeight;
    int texture_scale_factor = 1;
    font.beginGroup("font-map");
    font.read("texture", texFile, "");
    font.read("texture-scale", texture_scale_factor, 1);
    font.read("width", w, 0);
    font.read("height", h, 0);
    m_matrixWidth = w;
    m_matrixHeight = h;

    if((w <= 0) || (h <= 0))
    {
        pLogWarning("Wrong width and height values! %d x %d",  w, h);
        return;
    }

    if(!Files::fileExists(root + texFile))
    {
        pLogWarning("Failed to load font texture! file not exists: %s",
                    (root + texFile).c_str());
        return;
    }

    StdPicture fontTexture = XRender::LoadPicture(root + texFile);

    if(!fontTexture.inited)
        pLogWarning("Failed to load font texture! Invalid image!");

    if(texture_scale_factor > 1)
    {
        pLogDebug("RasterFont::loadFontMap: Force texture upscale with factor %d", texture_scale_factor);
#ifdef PICTURE_LOAD_NORMAL
        fontTexture.l.w_orig = fontTexture.w;
        fontTexture.l.h_orig = fontTexture.h;
        fontTexture.l.w_scale = 1.0f / float(texture_scale_factor);
        fontTexture.l.h_scale = 1.0f / float(texture_scale_factor);
#endif
        fontTexture.w *= texture_scale_factor;
        fontTexture.h *= texture_scale_factor;
        fontTexture.frame_w *= texture_scale_factor;
        fontTexture.frame_h *= texture_scale_factor;
    }

    m_texturesBank.push_back(fontTexture);
    StdPicture *loadedTexture = &m_texturesBank.back();

    if((m_letterWidth == 0) || (m_letterHeight == 0))
    {
        m_letterWidth    = static_cast<uint32_t>(fontTexture.w) / w;
        m_letterHeight   = static_cast<uint32_t>(fontTexture.h) / h;

        if(m_spaceWidth == 0)
            m_spaceWidth = m_letterWidth;

        if(m_newlineOffset == 0)
            m_newlineOffset = m_letterHeight;
    }

    font.endGroup();
    font.beginGroup("entries");
    std::vector<std::string> entries = font.allKeys();

    for(std::string &x : entries)
    {
        std::string charPosX = "0", charPosY = "0";

        std::string::size_type begPos = 0;
        std::string::size_type endPos = x.find('-', begPos);

        //QStringList tmp = x.split('-');
        if(endPos == std::string::npos)
            endPos = x.size();//Use entire string

        charPosX = x.substr(begPos, endPos);

        if(charPosX.empty())
        {
            pLogDebug("=invalid-X=%d=", x.c_str());
            continue;
        }

        if(m_matrixWidth > 1)
        {
            if(endPos == x.size())
            {
                pLogWarning("=invalid-Y=%d= in the raster font map %s", x.c_str(), fontmap_ini.c_str());
                continue;
            }
            begPos = endPos + 1;//+1 to skip '-' divider
            endPos = x.find('-', begPos);
            if(endPos == std::string::npos)
                endPos = x.size();//Use entire string
            charPosY = x.substr(begPos, endPos);
            if(charPosY.empty())
            {
                pLogWarning("=invalid-Y=%d= in the raster font map %s", x.c_str(), fontmap_ini.c_str());
                continue;
            }
        }

        std::string charX;
        font.read(x.c_str(), charX, "");

        /*Format of entry: X23
         * X - UTF-8 Symbol
         * 2 - padding left [for non-mono fonts]
         * 3 - padding right [for non-mono fonts]
        */
        if(charX.empty())
            continue;

        std::u32string ucharX = std_to_utf32(charX);
        char32_t ch = ucharX[0];
        //qDebug()<<"=char=" << ch << "=id="<<charPosX.toInt()<<charPosY.toInt()<<"=";
        RasChar rch;
        float l, t;
        try
        {
            rch.tx              =  loadedTexture;
            l                   =  std::stof(charPosY.c_str()) / m_matrixWidth;
            rch.padding_left    = (ucharX.size() > 1) ? char2int(ucharX[1]) : 0;
            rch.padding_right   = (ucharX.size() > 2) ? char2int(ucharX[2]) : 0;
            t                   =  std::stof(charPosX.c_str()) / m_matrixHeight;
            rch.x               =  static_cast<int32_t>(loadedTexture->w * l);
            rch.y               =  static_cast<int32_t>(loadedTexture->h * t);
            rch.valid = true;
        }
        catch(std::exception &e)
        {
            pLogWarning("Invalid entry of font map: entry: %s, reason: %s, file: %s", x.c_str(), e.what(), fontmap_ini.c_str());
        }

        m_charMap[ch] = rch;
    }

    font.endGroup();

    if(!m_charMap.empty())
        m_isReady = true;
}


PGE_Size RasterFont::textSize(const char* text, size_t text_size, uint32_t max_line_lenght, bool cut, uint32_t)
{
    const char* t = text;
    size_t t_size = text_size;
    std::string modText;

    if(!text || text_size == 0)
        return PGE_Size(0, 0);

    size_t lastspace = 0; //!< index of last found space character
    size_t count     = 1; //!< Count of lines
    uint32_t maxWidth     = 0; //!< detected maximal width of message

    uint32_t widthSumm    = 0;
    uint32_t widthSummMax = 0;

    if(cut)
    {
        modText = std::string(text, text_size);
        std::string::size_type i = modText.find('\n');
        if(i != std::string::npos)
            modText.erase(i, modText.size() - i);
        t = modText.c_str();
        t_size = modText.size();
    }

    /****************Word wrap*********************/
    uint32_t x = 0;
    for(size_t i = 0; i < t_size; i++, x++)
    {
        const char &cx = t[i];
        UTF8 uch = static_cast<unsigned char>(cx);

        switch(cx)
        {
        case '\r':
            break;

        case '\t':
        {
            lastspace = i;
            size_t spaceSize = m_spaceWidth + m_interLetterSpace / 2;
            if(spaceSize == 0)
                spaceSize = 1; // Don't divide by zero
            size_t tabMult = 4 - ((widthSumm / spaceSize) % 4);
            widthSumm += static_cast<size_t>(m_spaceWidth + m_interLetterSpace / 2) * tabMult;
            if(widthSumm > widthSummMax)
                widthSummMax = widthSumm;
            break;
        }

        case ' ':
        {
            lastspace = i;
            widthSumm += m_spaceWidth + m_interLetterSpace / 2;
            if(widthSumm > widthSummMax)
                widthSummMax = widthSumm;
            break;
        }

        case '\n':
        {
            lastspace = 0;
            if((maxWidth < x) && (maxWidth < max_line_lenght))
                maxWidth = x;
            x = 0;
            widthSumm = 0;
            count++;
            break;
        }

        default:
        {
            CharMap::iterator rc = m_charMap.find(get_utf8_char(&cx));
            bool need_a_ttf = false;
            if(rc != m_charMap.end())
            {
                RasChar &rch = rc->second;
                need_a_ttf = !rch.valid;
                if(rch.valid)
                    widthSumm += (m_letterWidth - rch.padding_left - rch.padding_right + m_interLetterSpace);
            }
            else
                need_a_ttf = true;

            if(need_a_ttf)
            {
#ifdef THEXTECH_ENABLE_TTF_SUPPORT
                TtfFont *font = dynamic_cast<TtfFont*>(FontManager::getDefaultTtfFont());
                if(font)
                {
                    TtfFont::TheGlyphInfo glyph = font->getGlyphInfo(&cx, m_letterWidth);
                    uint32_t glyph_width = glyph.width > 0 ? uint32_t(glyph.advance >> 6) : (m_letterWidth >> 2);
                    auto lw = m_letterWidth + m_interLetterSpace;
                    // Raster fonts are monospace fonts. TTF glyphs shoudn't break mono-width until they are wider than a cell
                    widthSumm += glyph_width > lw ? glyph_width : lw;
                }
                else
#endif
                {
                    widthSumm += m_letterWidth + m_interLetterSpace;
                }
            }

            if(widthSumm > widthSummMax)
                widthSummMax = widthSumm;

            break;
        }

        }//Switch

        if((max_line_lenght > 0) && (x >= max_line_lenght)) //If lenght more than allowed
        {
            maxWidth = x;
            if(t != modText.c_str())
            {
                modText = text;
                t = modText.c_str();
                t_size = modText.size();
            }

            if(lastspace > 0)
            {
                modText[lastspace] = '\n';
                i = lastspace - 1;
                lastspace = 0;
            }
            else
            {
                modText.insert(i, 1, '\n');
                t = modText.c_str();
                t_size = modText.size();
                x = 0;
                count++;
            }
        }
        i += static_cast<size_t>(trailingBytesForUTF8[uch]);
    }

    // Unused later
//    if(count == 1)
//        maxWidth = static_cast<uint32_t>(t_size);

    /****************Word wrap*end*****************/
    return PGE_Size(static_cast<int32_t>(widthSummMax), static_cast<int32_t>(m_newlineOffset * count));
}

void RasterFont::printText(const char* text, size_t text_size,
                           int32_t x, int32_t y,
                           float Red, float Green, float Blue, float Alpha,
                           uint32_t)
{
    if(m_charMap.empty() || !text || text_size == 0)
        return;

    uint32_t offsetX = 0;
    uint32_t offsetY = 0;
    uint32_t w = m_letterWidth;
    uint32_t h = m_letterHeight;
#ifdef THEXTECH_ENABLE_TTF_SUPPORT
    //! FIXME: Add the config option for TTF fonts here
    bool    doublePixel = false; //ConfigManager::setup_fonts.double_pixled;
#endif

    const char *strIt  = text;
    const char *strEnd = strIt + text_size;
    for(; strIt < strEnd; strIt++)
    {
        const char &cx = *strIt;
        UTF8 ucx = static_cast<unsigned char>(cx);

        switch(cx)
        {
        case '\n':
            offsetX = 0;
            offsetY += m_newlineOffset;
            continue;

        case '\t':
            //Fake tabulation
            offsetX += offsetX + offsetX % w;
            continue;

        case ' ':
            offsetX += m_spaceWidth + m_interLetterSpace / 2;
            continue;
        }

        const auto rch_f = m_charMap.find(get_utf8_char(strIt));
        if(rch_f != m_charMap.end() && rch_f->second.valid)
        {
            const auto &rch = rch_f->second;
            XRender::renderTexture(x + static_cast<int32_t>(offsetX - rch.padding_left + m_glyphOffsetX),
                                   y + static_cast<int32_t>(offsetY + m_glyphOffsetY),
                                   w, h,
                                   *rch.tx,
                                   rch.x, rch.y,
                                   Red, Green, Blue, Alpha);
            offsetX += w - rch.padding_left - rch.padding_right + m_interLetterSpace;
        }
        else
#ifdef THEXTECH_ENABLE_TTF_SUPPORT
        {
            TtfFont *font = reinterpret_cast<TtfFont*>(FontManager::getDefaultTtfFont());
            if(font)
            {
                TtfFont::TheGlyphInfo glyph = font->getGlyphInfo(&cx, m_letterWidth);
                uint32_t glyph_width = glyph.width > 0 ? uint32_t(glyph.advance >> 6) : (m_letterWidth >> 2);

                font->drawGlyph(&cx,
                                x + static_cast<int32_t>(offsetX + m_glyphOffsetX),
                                y + static_cast<int32_t>(offsetY + m_glyphOffsetY) - 2,
                                (doublePixel ? (glyph_width / 2) : glyph_width),
                                (doublePixel ? 2.0 : 1.0),
                                Red, Green, Blue, Alpha);

                auto lw = w + m_interLetterSpace;
                offsetX += glyph_width > lw ? glyph_width : lw;
            } else {
                offsetX += m_interLetterSpace;
            }
        }
#else
        {
            offsetX += m_interLetterSpace;
        }
#endif
        strIt += static_cast<size_t>(trailingBytesForUTF8[ucx]);
    }
}

bool RasterFont::isLoaded()
{
    return m_isReady;
}

std::string RasterFont::getFontName()
{
    return m_fontName;
}
