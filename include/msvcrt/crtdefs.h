/*
 * CRT definitions
 *
 * Copyright 2000 Francois Gouget.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINE_USE_MSVCRT
#define __WINE_USE_MSVCRT
#endif

#if defined(__x86_64__) && !defined(_WIN64)
#define _WIN64
#endif

#if !defined(_MSC_VER) && !defined(__int64)
# if defined(_WIN64) && !defined(__MINGW64__)
#   define __int64 long
# else
#   define __int64 long long
# endif
#endif

#ifndef __stdcall
# ifdef __i386__
#  ifdef __GNUC__
#   ifdef __APPLE__ /* Mac OS X uses a 16-byte aligned stack and not a 4-byte one */
#    define __stdcall __attribute__((__stdcall__)) __attribute__((__force_align_arg_pointer__))
#   else
#    define __stdcall __attribute__((__stdcall__))
#   endif
#  elif defined(_MSC_VER)
    /* Nothing needs to be done. __stdcall already exists */
#  else
#   error You need to define __stdcall for your compiler
#  endif
# elif defined(__x86_64__) && defined (__GNUC__)
#  define __stdcall __attribute__((ms_abi))
# else
#  define __stdcall
# endif
#endif /* __stdcall */

#ifndef __cdecl
# if defined(__i386__) && defined(__GNUC__)
#  ifdef __APPLE__ /* Mac OS X uses 16-byte aligned stack and not a 4-byte one */
#   define __cdecl __attribute__((__cdecl__)) __attribute__((__force_align_arg_pointer__))
#  else
#   define __cdecl __attribute__((__cdecl__))
#  endif
# elif defined(__x86_64__) && defined (__GNUC__)
#  define __cdecl __attribute__((ms_abi))
# elif !defined(_MSC_VER)
#  define __cdecl
# endif
#endif /* __cdecl */

#ifndef _INTPTR_T_DEFINED
#ifdef  _WIN64
typedef __int64 intptr_t;
#else
typedef int intptr_t;
#endif
#define _INTPTR_T_DEFINED
#endif

#ifndef _UINTPTR_T_DEFINED
#ifdef  _WIN64
typedef unsigned __int64 uintptr_t;
#else
typedef unsigned int uintptr_t;
#endif
#define _UINTPTR_T_DEFINED
#endif

#ifndef _PTRDIFF_T_DEFINED
#ifdef _WIN64
typedef __int64 ptrdiff_t;
#else
typedef int ptrdiff_t;
#endif
#define _PTRDIFF_T_DEFINED
#endif

#ifndef _SIZE_T_DEFINED
#ifdef _WIN64
typedef unsigned __int64 size_t;
#else
typedef unsigned int size_t;
#endif
#define _SIZE_T_DEFINED
#endif

#ifndef _TIME_T_DEFINED
typedef long time_t;
#define _TIME_T_DEFINED
#endif

#ifndef _TIME32_T_DEFINED
typedef long __time32_t;
#define _TIME32_T_DEFINED
#endif

#ifndef _TIME64_T_DEFINED
typedef __int64 __time64_t;
#define _TIME64_T_DEFINED
#endif

#ifndef _WCHAR_T_DEFINED
#ifndef __cplusplus
typedef unsigned short wchar_t;
#endif
#define _WCHAR_T_DEFINED
#endif

#ifndef _WCTYPE_T_DEFINED
typedef unsigned short  wint_t;
typedef unsigned short  wctype_t;
#define _WCTYPE_T_DEFINED
#endif