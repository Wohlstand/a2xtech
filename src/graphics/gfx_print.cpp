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

#include <algorithm>

#include "../globals.h"
#include "../graphics.h"

#if defined(_MSC_VER) && _MSC_VER <= 1900 // Workaround for MSVC 2015
namespace std
{
    using ::toupper;
}
#endif

int SuperTextPixLen(const std::string &SuperWords, int Font)
{
    int len = 0;

    switch(Font)
    {
    default:
    case 1:
    case 4:
        len = SuperWords.length() * 18;
        break;
    case 2:
        len = SuperWords.length() * 16;
        break;
    case 3:
    {
        int B = 0;
        for(auto c : SuperWords)
        {
            c = std::toupper(c);
            if(c >= 33 && c <= 126)
            {
                B += 18;
                if(c == 'M')
                    B += 2;
            } else {
                B += 16;
            }
        }
        len = B;
        break;
    }
    }

    return len;
}

void SuperPrintRightAlign(const std::string &SuperWords, int Font, float X, float Y, float r, float g, float b, float a)
{
    X -= SuperTextPixLen(SuperWords, Font);
    SuperPrint(SuperWords, Font, X, Y, r, g, b, a);
}

void SuperPrintScreenCenter(const std::string &SuperWords, int Font, float Y, float r, float g, float b, float a)
{
    float X = (ScreenW / 2) - (SuperTextPixLen(SuperWords, Font) / 2);
    SuperPrint(SuperWords, Font, X, Y, r, g, b, a);
}

void SuperPrint(const std::string &SuperWords, int Font, float X, float Y,
                float r, float g, float b, float a)
{
//    int A = 0;
    int B = 0;
    int C = 0;

    if(Font == 1)
    {
        for(auto c : SuperWords)
        {
            if(c >= '0' && c <= '9')
                frmMain.renderTexture(int(X + B), int(Y), 16, 14, GFX.Font1[c - '0'], 0, 0, r, g, b, a);
            B += 18;
        }
    }
    else if(Font == 2)
    {
        for(auto c : SuperWords)
        {
            if(c >= 48 && c <= 57) {
                C = (c - 48) * 16;
                frmMain.renderTexture(int(X + B), int(Y), 15, 17, GFX.Font2[1], C, 0, r, g, b, a);
                B += 16;
            } else if(c >= 65 && c <= 90) {
                C = (c - 55) * 16;
                frmMain.renderTexture(int(X + B), int(Y), 15, 17, GFX.Font2[1], C, 0, r, g, b, a);
                B += 16;
            } else if(c >= 97 && c <= 122) {
                C = (c - 61) * 16;
                frmMain.renderTexture(int(X + B), int(Y), 15, 17, GFX.Font2[1], C, 0, r, g, b, a);
                B += 16;
            } else if(c >= 33 && c <= 47) {
                C = (c - 33) * 16;
                frmMain.renderTexture(int(X + B), int(Y), 15, 17, GFX.Font2S, C, 0, r, g, b, a);
                B += 16;
            } else if(c >= 58 && c <= 64) {
                C = (c - 58 + 15) * 16;
                frmMain.renderTexture(int(X + B), int(Y), 15, 17, GFX.Font2S, C, 0, r, g, b, a);
                B += 16;
            } else if(c >= 91 && c <= 96) {
                C = (c - 91 + 22) * 16;
                frmMain.renderTexture(int(X + B), int(Y), 15, 17, GFX.Font2S, C, 0, r, g, b, a);
                B += 16;
            } else if(c >= 123 && c <= 125) {
                C = (c - 123 + 28) * 16;
                frmMain.renderTexture(int(X + B), int(Y), 15, 17, GFX.Font2S, C, 0, r, g, b, a);
                B += 16;
            } else {
                B += 16;
            }
        }

    }
    else if (Font == 3)
    {
//        Do While Len(Words) > 0
        for(auto c : SuperWords)
        {
            c = std::toupper(c);
//            If Asc(Left(Words, 1)) >= 33 And Asc(Left(Words, 1)) <= 126 Then
            if(c >= 33 && c <= 126)
            {
//                C = (Asc(Left(Words, 1)) - 33) * 32
                C = (c - 33) * 32;
//                BitBlt myBackBuffer, X + B, Y, 18, 16, GFX.Font2Mask(2).hdc, 2, C, vbSrcAnd
//                BitBlt myBackBuffer, X + B, Y, 18, 16, GFX.Font2(2).hdc, 2, C, vbSrcPaint
                frmMain.renderTexture(int(X + B), int(Y), 18, 16, GFX.Font2[2], 2, C, r, g, b, a);
//                B = B + 18
                B += 18;
//                If Left(Words, 1) = "M" Then B = B + 2
                if(c == 'M')
                    B += 2;
//            Else
            } else {
//                B = B + 16
                B += 16;
            }
//            End If
//            Words = Right(Words, Len(Words) - 1)
//        Loop
        }
//    ElseIf Font = 4 Then
    }
    else if(Font == 4)
    {
//        Do While Len(Words) > 0
        for(auto c : SuperWords)
        {
//            If Asc(Left(Words, 1)) >= 33 And Asc(Left(Words, 1)) <= 126 Then
            if(c >= 33 && c <= 126)
            {
//                C = (Asc(Left(Words, 1)) - 33) * 16
                C = (c - 33) * 16;
//                BitBlt myBackBuffer, X + B, Y, 18, 16, GFX.Font2(3).hdc, 2, C, vbSrcPaint
                frmMain.renderTexture(int(X + B), int(Y), 18, 16, GFX.Font2[3], 2, C, r, g, b, a);
//                B = B + 18
                B += 18;
//            Else
            } else {
//                B = B + 18
                B += 18;
//            End If
            }
//            Words = Right(Words, Len(Words) - 1)
//        Loop
        }
//    End If
    }
}
