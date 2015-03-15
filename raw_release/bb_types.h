/*
 *
 * Copyright (C) 2008 yajin <yajin@vm-kernel.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef _BB_TYPES_H_
#define _BB_TYPES_H_

#include <stddef.h>

/*From qemu*/
#ifdef __OpenBSD__
#include <sys/types.h>
#else
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
// Linux/Sparc64 defines uint64_t
#if !(defined (__sparc_v9__) && defined(__linux__)) && !(defined(__APPLE__) && defined(__x86_64__))
/* XXX may be done for all 64 bits targets ? */
#if defined (__x86_64__) || defined(__ia64) || defined(__s390x__) || defined(__alpha__) || defined(__powerpc64__)
typedef unsigned long uint64_t;
#else
typedef unsigned long long uint64_t;
#endif
#endif

/* if Solaris/__sun__, don't typedef int8_t, as it will be typedef'd
   prior to this and will cause an error in compliation, conflicting
   with /usr/include/sys/int_types.h, line 75 */
#ifndef __sun__
typedef signed char int8_t;
#endif
typedef signed short int16_t;
typedef signed int int32_t;
// Linux/Sparc64 defines int64_t
#if !(defined (__sparc_v9__) && defined(__linux__)) && !(defined(__APPLE__) && defined(__x86_64__))
#if defined (__x86_64__) || defined(__ia64) || defined(__s390x__) || defined(__alpha__) || defined(__powerpc64__)
typedef signed long int64_t;
#else
typedef signed long long int64_t;
#endif
#endif
#endif


#endif
