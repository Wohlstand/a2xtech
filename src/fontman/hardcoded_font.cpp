/*
 * Moondust, a free game engine for platform game making
 * Copyright (c) 2014-2024 Vitaly Novichkov <admin@wohlnet.ru>
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

#include <vector>

#include "fontman/hardcoded_font.h"

#include "core/render.h"

// The monochromic 1-bit texture of the 8x749 size
const std::array<uint8_t, 749> c_hardcoded_font_bytes =
{
    0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00, 0x00, 0x24, 0x24, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x7E, 0x24,
    0x7E, 0x24, 0x00, 0x00, 0x00, 0x38, 0x6C, 0x3B, 0x6E, 0xC6, 0x7B, 0x00, 0x62, 0x66, 0x0C, 0x18, 0x30, 0x66, 0xC6, 0x00,
    0x00, 0x38, 0x6C, 0x3B, 0x6E, 0xC6, 0x7B, 0x00, 0x00, 0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x18, 0x18,
    0x18, 0x18, 0x0C, 0x00, 0x00, 0x18, 0x0C, 0x0C, 0x0C, 0x0C, 0x18, 0x00, 0x00, 0x6C, 0x7C, 0x38, 0x7C, 0x6C, 0x00, 0x00,
    0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x7E,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00, 0x00,
    0x00, 0x3C, 0x66, 0x6E, 0x76, 0x66, 0x3C, 0x00, 0x00, 0x18, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x3C, 0x66, 0x06,
    0x3C, 0x60, 0x7E, 0x00, 0x00, 0x3C, 0x66, 0x0C, 0x06, 0x66, 0x3C, 0x00, 0x00, 0x60, 0x6C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00,
    0x00, 0x7E, 0x40, 0x7C, 0x06, 0x66, 0x3C, 0x00, 0x00, 0x3C, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00, 0x00, 0x7E, 0x0C, 0x0C,
    0x18, 0x18, 0x18, 0x00, 0x00, 0x3C, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00, 0x00, 0x3C, 0x66, 0x66, 0x3E, 0x06, 0x3C, 0x00,
    0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x10, 0x00, 0x00, 0x0C, 0x18, 0x30,
    0x18, 0x0C, 0x00, 0x00, 0x00, 0x3E, 0x3E, 0x00, 0x3E, 0x3E, 0x00, 0x00, 0x00, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x00, 0x00,
    0x00, 0x3C, 0x66, 0x06, 0x1C, 0x00, 0x18, 0x00, 0x00, 0x3C, 0x66, 0x6E, 0x6C, 0x60, 0x3E, 0x00, 0x00, 0x3C, 0x66, 0x66,
    0x7E, 0x66, 0x66, 0x00, 0x00, 0x7C, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00, 0x00, 0x3C, 0x66, 0x60, 0x60, 0x66, 0x3C, 0x00,
    0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x00, 0x00, 0x7E, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00, 0x00, 0x7E, 0x60, 0x78,
    0x60, 0x60, 0x60, 0x00, 0x00, 0x3C, 0x66, 0x60, 0x6E, 0x66, 0x3C, 0x00, 0x00, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00,
    0x00, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x3E, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00, 0x00, 0x66, 0x6C, 0x78,
    0x78, 0x6C, 0x66, 0x00, 0x00, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7C, 0x00, 0x00, 0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x00,
    0x00, 0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00, 0x00, 0x7C, 0x66, 0x66,
    0x7C, 0x60, 0x60, 0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x6C, 0x3A, 0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x00,
    0x00, 0x3C, 0x62, 0x3C, 0x06, 0x66, 0x3C, 0x00, 0x00, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x3C, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00, 0x00, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00,
    0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x42, 0x00, 0x00, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00, 0x00, 0x7E, 0x0C, 0x18,
    0x30, 0x60, 0x7E, 0x00, 0x00, 0x1C, 0x18, 0x18, 0x18, 0x18, 0x1C, 0x00, 0x00, 0x60, 0x30, 0x18, 0x0C, 0x04, 0x00, 0x00,
    0x00, 0x1C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1C, 0x00, 0x1C, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7E, 0x00, 0x00, 0x30, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x66, 0x66, 0x66, 0x3B, 0x00,
    0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00, 0x00, 0x00, 0x3C, 0x66, 0x60, 0x66, 0x3C, 0x00, 0x06, 0x06, 0x3E, 0x66,
    0x66, 0x66, 0x3E, 0x00, 0x00, 0x00, 0x3C, 0x66, 0x7C, 0x60, 0x3E, 0x00, 0x00, 0x0E, 0x18, 0x3C, 0x18, 0x18, 0x18, 0x00,
    0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C, 0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00, 0x18, 0x00, 0x18, 0x18,
    0x18, 0x18, 0x18, 0x00, 0x0C, 0x00, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x60, 0x60, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00,
    0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1C, 0x00, 0x00, 0x00, 0xEC, 0xD6, 0xD6, 0xD6, 0xD6, 0x00, 0x00, 0x00, 0x7C, 0x66,
    0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00, 0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60,
    0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x07, 0x06, 0x00, 0x00, 0x6C, 0x76, 0x60, 0x60, 0x60, 0x00, 0x00, 0x00, 0x3C, 0x60,
    0x3C, 0x06, 0x7C, 0x00, 0x00, 0x30, 0x7C, 0x30, 0x30, 0x36, 0x1C, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3B, 0x00,
    0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00, 0x00, 0x00, 0x6B, 0x6B, 0x6B, 0x6B, 0x37, 0x00, 0x00, 0x00, 0x66, 0x7E,
    0x3C, 0x7E, 0x66, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x3C, 0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00,
    0x00, 0x0C, 0x18, 0x70, 0x18, 0x18, 0x0C, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x30, 0x18, 0x0E,
    0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x30, 0x5A, 0x0C
};

#ifdef __16M__

void LoadHardcodedFont(StdPicture& tx)
{
    tx.reset();

    tx.inited = true;
    tx.l.path = "!F";
    tx.l.lazyLoaded = true;

    tx.w = c_hardcoded_font_w * 2;
    tx.h = c_hardcoded_font_h * 2;
    tx.l.flags = 0;
}

#else // __16M__

void LoadHardcodedFont(StdPicture& tx)
{
    tx.reset();

    tx.w = c_hardcoded_font_w * 2;
    tx.h = c_hardcoded_font_h * 2;

    std::vector<uint32_t> image_data(c_hardcoded_font_bytes.size() * 8);

    auto i = c_hardcoded_font_bytes.cbegin();
    uint32_t* o = &image_data[0];

    while(i != c_hardcoded_font_bytes.cend())
    {
        uint8_t row = *(i++);
        *(o++) = (row & 0x80) ? 0xFFFFFFFF : 0;
        *(o++) = (row & 0x40) ? 0xFFFFFFFF : 0;
        *(o++) = (row & 0x20) ? 0xFFFFFFFF : 0;
        *(o++) = (row & 0x10) ? 0xFFFFFFFF : 0;
        *(o++) = (row & 0x08) ? 0xFFFFFFFF : 0;
        *(o++) = (row & 0x04) ? 0xFFFFFFFF : 0;
        *(o++) = (row & 0x02) ? 0xFFFFFFFF : 0;
        *(o++) = (row & 0x01) ? 0xFFFFFFFF : 0;
    }

    XRender::loadTexture(tx, c_hardcoded_font_w, c_hardcoded_font_h, (uint8_t*)image_data.data(), c_hardcoded_font_w * 4);
}

#endif // __16M__