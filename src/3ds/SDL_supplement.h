#ifndef SDL_SUPPLEMENT_H
#define SDL_SUPPLEMENT_H

typedef struct _SDL_Point {
    int x;
    int y;
} SDL_Point;

extern uint SDL_GetTicks(void);

const uint SDL_FLIP_NONE = 0;
const uint SDL_FLIP_HORIZONTAL = 1;
const uint SDL_FLIP_VERTICAL = 2; // bitwise combos

const char KEYNAMES[32][32] = {
 "A", "B", "SELECT", "START",
 "D-PAD RIGHT", "D-PAD LEFT", "D-PAD UP", "D-PAD DOWN",
 "R", "L", "X", "Y",
 "", "", "ZL", "ZR",
 "", "", "", "",
 "TOUCH", "", "", "",
 "C-STICK RIGHT", "C-STICK LEFT", "C-STICK UP", "C-STICK DOWN",
 "THUMB RIGHT", "THUMB LEFT", "THUMB UP", "THUMB DOWN"
};

enum KEYCODE {
 KEYCODE_A=1<<0,
 KEYCODE_B=1<<1,
 KEYCODE_SELECT=1<<2,
 KEYCODE_START=1<<3,
 KEYCODE_DRIGHT=1<<4,
 KEYCODE_DLEFT=1<<5,
 KEYCODE_DUP=1<<6,
 KEYCODE_DDOWN=1<<7,
 KEYCODE_R=1<<8,
 KEYCODE_L=1<<9,
 KEYCODE_X=1<<10,
 KEYCODE_Y=1<<11,
 KEYCODE_ZL=1<<14,
 KEYCODE_ZR=1<<15,

 KEYCODE_TOUCH=1<<20,
 KEYCODE_CSTICK_RIGHT=1<<24,
 KEYCODE_CSTICK_LEFT=1<<25,
 KEYCODE_CSTICK_UP=1<<26,
 KEYCODE_CSTICK_DOWN=1<<27,
 KEYCODE_PAD_RIGHT=1<<28,
 KEYCODE_PAD_LEFT=1<<29,
 KEYCODE_PAD_UP=1<<30,
 KEYCODE_PAD_DOWN=1<<31
};

enum KEYID {
 KEYID_A=0,
 KEYID_B=1,
 KEYID_SELECT=2,
 KEYID_START=3,
 KEYID_DRIGHT=4,
 KEYID_DLEFT=5,
 KEYID_DUP=6,
 KEYID_DDOWN=7,
 KEYID_R=8,
 KEYID_L=9,
 KEYID_X=10,
 KEYID_Y=11,
 KEYID_ZL=14,
 KEYID_ZR=15,

 KEYID_TOUCH=20,
 KEYID_CSTICK_RIGHT=24,
 KEYID_CSTICK_LEFT=25,
 KEYID_CSTICK_UP=26,
 KEYID_CSTICK_DOWN=27,
 KEYID_PAD_RIGHT=28,
 KEYID_PAD_LEFT=29,
 KEYID_PAD_UP=30,
 KEYID_PAD_DOWN=31
};

#define SDL_INLINE inline

#define Sint64 sint64_t
#define Uint8 uint8_t
#define SDL_bool bool

#define SDL_memset memset
#define SDL_fabs fabs
#define SDL_strcasecmp strcasecmp
#define SDL_strtol strtol
#define SDL_fmod fmod
#define SDL_floor floor

inline void SDL_assert_release(bool arg) {};
inline void SDL_assert(bool arg) {};

#endif // SDL_SUPPLEMENT_H
