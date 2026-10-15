/*
    SP0256A - Integer Types.

    Created by Michel Bernard (michel_bernard@hotmail.com)
    - <http://www.github.com/GmEsoft/SP0256_CTS256A-AL2>
    Copyright (c) 2023 Michel Bernard.
    All rights reserved.


    This file is part of SP0256_CTS256A-AL2.

    SP0256_CTS256A-AL2 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SP0256_CTS256A-AL2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SP0256_CTS256A-AL2.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

typedef signed char schar;
typedef unsigned char uchar;
typedef signed short sshort;
typedef unsigned short ushort;

typedef signed int sint;
typedef unsigned int uint;
typedef signed long slong;
typedef unsigned long ulong;

#if __cplusplus >= 201103L

#include <cstdint>

/* LambdaSpeak 4 port fix: on a 32-bit target (RP2350, and any 32-bit host)
   `unsigned long` is 32 bits, so the fallback typedefs below would silently
   make uint64_t 32 bits wide and corrupt ivoice_t::now / sound_current.
   Any C99-or-later compiler has the real fixed-width types; use them. */
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L

#include <stdint.h>

#else

typedef uchar uint8_t;
typedef ushort uint16_t;
typedef uint uint32_t;
typedef ulong uint64_t;

typedef schar int8_t;
typedef sshort int16_t;
typedef sint int32_t;
typedef slong int64_t;

#endif
