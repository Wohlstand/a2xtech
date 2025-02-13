/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2025 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */


#ifndef GLOBAL_PGE_VERSION
#define GLOBAL_PGE_VERSION

#ifdef PGE_TOOLCHAIN_VERSION_1
#   define V_VP1 PGE_TOOLCHAIN_VERSION_1
#else
#   define V_VP1 0
#endif

#ifdef PGE_TOOLCHAIN_VERSION_2
#   define V_VP2 PGE_TOOLCHAIN_VERSION_2
#else
#   define V_VP2 0
#endif

#ifdef PGE_TOOLCHAIN_VERSION_3
#   define V_VP3 PGE_TOOLCHAIN_VERSION_3
#else
#   define V_VP3 0
#endif

#ifdef PGE_TOOLCHAIN_VERSION_4
#   define V_VP4 PGE_TOOLCHAIN_VERSION_4
#else
#   define V_VP4 0
#endif

#ifdef PGE_CONFIG_PACK_API_VERSION
#   define V_CP_API PGE_CONFIG_PACK_API_VERSION
#else
#   define V_CP_API 0
#endif

#ifdef PGE_TOOLCHAIN_VERSION_REL
#   define V_TOOLCHAIN_RELEASE PGE_TOOLCHAIN_VERSION_REL
#else
#   define V_TOOLCHAIN_RELEASE "-unk" //"-alpha","-beta","-dev", or "" aka "release"
#endif

#define GEN_VERSION_NUMBER_2(v1,v2)     v1 "." v2
#define GEN_VERSION_NUMBER_3(v1,v2,v3)  v1 "." v2 "." v3
#define GEN_VERSION_NUMBER_4(v1,v2,v3,v4)  v1 "." v2 "." v3 "." v4

#define STR_VALUE_REAL(arg)  #arg
#define STR_VALUE(verf)      STR_VALUE_REAL(verf)
#define V_VP1_s STR_VALUE(V_VP1)
#define V_VP2_s STR_VALUE(V_VP2)
#define V_VP3_s STR_VALUE(V_VP3)
#define V_VP4_s STR_VALUE(V_VP4)
#if V_VP4 == 0
#   if V_VP3 == 0
#       define V_PROJECT_VERSION_NUM GEN_VERSION_NUMBER_2(V_VP1_s, V_VP2_s)
#   else
#       define V_PROJECT_VERSION_NUM GEN_VERSION_NUMBER_3(V_VP1_s, V_VP2_s, V_VP3_s)
#   endif
#else
#   define V_PROJECT_VERSION_NUM GEN_VERSION_NUMBER_4(V_VP1_s, V_VP2_s, V_VP3_s, V_VP4_s)
#endif



// --------------- Version of whole project ---------------

#define V_VERSION V_PROJECT_VERSION_NUM
#define V_RELEASE "" //Developing state (for release this field must be empty)

#define V_COPYRIGHT "2020-2025 by Wohlstand"

#define V_COMPANY "Moondust Team"

#define V_PGE_URL "wohlsoft.ru"

#define V_PRODUCT_NAME "Moondust"



// --------------- OPERATING SYSTEM ---------------

#if defined(_WIN64)
#   if defined(_M_ARM64)
#       define OPERATION_SYSTEM "Windows ARM64"
#   else
#       define OPERATION_SYSTEM "Windows x64"
#   endif
#elif defined(_WIN32)
#   define OPERATION_SYSTEM "Windows"
#elif defined __APPLE__
#   include <TargetConditionals.h>
#   if TARGET_IPHONE_SIMULATOR
#       define OPERATION_SYSTEM "iOS Simulator"
#   elif TARGET_OS_IPHONE
#       define OPERATION_SYSTEM "iOS"
#   else
#       define OPERATION_SYSTEM "macOS"
#   endif
#elif defined __FreeBSD__
#   define OPERATION_SYSTEM "FreeBSD"
#elif defined __gnu_linux__
#   define OPERATION_SYSTEM "GNU/Linux"
#elif defined __HAIKU__
#   define OPERATION_SYSTEM "Haiku"
#elif defined __EMSCRIPTEN__
#   define OPERATION_SYSTEM "Emscripten"
#elif defined(__ANDROID__)
#   define OPERATION_SYSTEM "Android"
#elif defined __WII__
#   define OPERATION_SYSTEM "Wii System"
#elif defined __16M__
#   define OPERATION_SYSTEM "DSi System"
#elif defined __3DS__
#   define OPERATION_SYSTEM "3DS System"
#elif defined __SWITCH__
#   define OPERATION_SYSTEM "Nintendo Switch"
#elif defined __vita__
#   define OPERATION_SYSTEM "Live Area (PS Vita)"
#else
#   define OPERATION_SYSTEM "Other"
#endif




// --------------- Processor Architecture ---------------

#if defined(_X86_) || defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) || defined(_M_IX86)
#   define FILE_CPU "x86 (32-bit)"
#elif defined(__x86_64__) || defined(__amd64__) || defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64)
#   define FILE_CPU "x86_64 (64-bit)"
#elif defined(__arm__) || defined(__TARGET_ARCH_ARM) || defined(__ARM_ARCH)
#   if defined(__ARM_ARCH_8__) \
     || defined(__ARM_ARCH_8A) \
     || defined(__ARM_ARCH_8A__) \
     || defined(__ARM_ARCH_8R__) \
     || defined(__ARM_ARCH_8M__) \
     || (defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM >= 8) \
     || (defined(__ARM_ARCH) && __ARM_ARCH >= 8) || defined(_M_ARM64) || defined(_M_ARM64EC)
#       define FILE_CPU "ARM64 (64-bit)"
#   elif defined(__ARM_ARCH_7__) \
       || defined(__ARM_ARCH_7A__) \
       || defined(__ARM_ARCH_7R__) \
       || defined(__ARM_ARCH_7M__) \
       || (defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM >= 7) \
       || (defined(__ARM_ARCH) && __ARM_ARCH >= 7)
#       define FILE_CPU "ARMv7 (32-bit)"
#   elif defined(__ARM_ARCH_6__) \
       || defined(__ARM_ARCH_6J__) \
       || defined(__ARM_ARCH_6T2__) \
       || defined(__ARM_ARCH_6Z__) \
       || defined(__ARM_ARCH_6K__) \
       || defined(__ARM_ARCH_6ZK__) \
       || defined(__ARM_ARCH_6M__) \
       || (defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM >= 6) \
       || (defined(__ARM_ARCH) && __ARM_ARCH >= 6)
#       define FILE_CPU "ARMv6 (32-bit)"
#   elif defined(__ARM_ARCH_5TEJ__) \
      || (defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM >= 5)
#       define FILE_CPU "ARMv5 (32-bit)"
#   else
#       define FILE_CPU "ARM (Unknown)"
#   endif
#elif defined(__mips__) || defined(__mips) || defined(__MIPS__)
#   define FILE_CPU "MIPS"
#elif defined(__ppc__) || defined(__ppc) || defined(__powerpc__) \
   || defined(_ARCH_COM) || defined(_ARCH_PWR) || defined(_ARCH_PPC)  \
   || defined(_M_MPPC) || defined(_M_PPC)
#   if defined(__ppc64__) || defined(__powerpc64__) || defined(__64BIT__)
#       define FILE_CPU "PowerPC (64-bit)"
#   else
#       define FILE_CPU "PowerPC (32-bit)"
#   endif
#elif defined(__riscv) // FIXME: Adjust if any extra macros needed
#   if defined(__riscv64)
#       define FILE_CPU "RISC-V (64-bit)"
#   elif defined(__riscv32)
#       define FILE_CPU "RISC-V (32-bit)"
#   endif
#elif defined(__loongarch64)
#       define FILE_CPU "LoongArch (64-bit)"
#elif defined(__loongarch)
#       define FILE_CPU "LoongArch (32-bit)"
#else
#   define FILE_CPU "unknown"
#endif



// --------------- Date and time of the build ---------------
#ifndef DISABLE_XTECH_BUILD_DATE
#   define V_DATE_OF_BUILD __DATE__ " " __TIME__
#else
#   define V_DATE_OF_BUILD "<Build timestamp disabled>"
#endif



#endif // GLOBAL_PGE_VERSION
