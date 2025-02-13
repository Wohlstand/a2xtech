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

#if defined(_X86_) || defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) || defined(_M_IX86)
#   define PGE_CPU_x86
#   define PGE_CPU_x86_32
#elif defined(__x86_64__) || defined(__amd64__) || defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64)
#   define PGE_CPU_x86
#   define PGE_CPU_x86_64
#elif defined(__arm__) || defined(__TARGET_ARCH_ARM) || defined(__ARM_ARCH)
#   if defined(__ARM_ARCH_8__) \
     || defined(__ARM_ARCH_8A) \
     || defined(__ARM_ARCH_8A__) \
     || defined(__ARM_ARCH_8R__) \
     || defined(__ARM_ARCH_8M__) \
     || (defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM >= 8) \
     || (defined(__ARM_ARCH) && __ARM_ARCH >= 8) || defined(_M_ARM64) || defined(_M_ARM64EC)
#       define PGE_CPU_ARM64
#       define PGE_CPU_ARMv8
#   elif defined(__ARM_ARCH_7__) \
       || defined(__ARM_ARCH_7A__) \
       || defined(__ARM_ARCH_7R__) \
       || defined(__ARM_ARCH_7M__) \
       || (defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM >= 7) \
       || (defined(__ARM_ARCH) && __ARM_ARCH >= 7)
#       define PGE_CPU_ARM32
#       define PGE_CPU_ARMv7
#   elif defined(__ARM_ARCH_6__) \
       || defined(__ARM_ARCH_6J__) \
       || defined(__ARM_ARCH_6T2__) \
       || defined(__ARM_ARCH_6Z__) \
       || defined(__ARM_ARCH_6K__) \
       || defined(__ARM_ARCH_6ZK__) \
       || defined(__ARM_ARCH_6M__) \
       || (defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM >= 6) \
       || (defined(__ARM_ARCH) && __ARM_ARCH >= 6)
#       define PGE_CPU_ARM32
#       define PGE_CPU_ARMv6
#   elif defined(__ARM_ARCH_5TEJ__) \
      || (defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM >= 5)
#       define PGE_CPU_ARM32
#       define PGE_CPU_ARMv5
#   else
#       define PGE_CPU_ARM32
#   endif
#elif defined(__mips__) || defined(__mips) || defined(__MIPS__)
#       define PGE_CPU_MIPS
#elif defined(__ppc__) || defined(__ppc) || defined(__powerpc__) \
   || defined(_ARCH_COM) || defined(_ARCH_PWR) || defined(_ARCH_PPC)  \
   || defined(_M_MPPC) || defined(_M_PPC)
#   if defined(__ppc64__) || defined(__powerpc64__) || defined(__64BIT__)
#       define PGE_CPU_PPC
#       define PGE_CPU_PPC64
#   else
#       define PGE_CPU_PPC
#       define PGE_CPU_PPC32
#   endif
#elif defined(__riscv) // FIXME: Adjust if any extra macros needed
#   if defined(__riscv64)
#       define PGE_CPU_RISCV
#       define PGE_CPU_RISCV64
#   elif defined(__riscv32)
#       define PGE_CPU_RISCV
#       define PGE_CPU_RISCV32
#   endif
#elif defined(__loongarch64)
#       define PGE_CPU_LOONGARCH
#       define PGE_CPU_LOONGARCH64
#elif defined(__loongarch)
#       define PGE_CPU_LOONGARCH
#       define PGE_CPU_LOONGARCH32
#else
#       define PGE_CPU_UNKNOWN
#       warning CPU is unknown! Please add missing architecture!
#endif
