/*
 * Definitions for Wine main program
 *
 * Copyright 2004 Mike McCormack for CodeWeavers
 * Copyright 2004 Alexandre Julliard
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

#ifndef __WINE_LOADER_MAIN_H
#define __WINE_LOADER_MAIN_H

#include "wine/pthread.h"

struct wine_preload_info
{
    void  *addr;
    size_t size;
};

#if defined(__GNUC__) && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 3)))
extern void init_pthread_functions(void) __attribute__((visibility ("hidden")));
#else
extern void init_pthread_functions(void);
#endif

#endif /* __WINE_LOADER_MAIN_H */
