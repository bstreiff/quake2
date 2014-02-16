/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#pragma once

#if defined(__linux__)
#define OS_NAME "Linux"
#define LIBRARY_EXTENSION "so"
#elif defined(_WIN32)
#define OS_NAME "Windows"
#define LIBRARY_EXTENSION "dll"
#elif defined(__APPLE__)
#define OS_NAME "Mac OS X"
#define LIBRARY_EXTENSION "dylib"
#else
#error Unknown platform!
#endif

#if defined(_M_IX86) || defined(__i386__)
#define ARCH_NAME "x86"
#elif defined(_M_ARM) || defined(__arm__)
#define ARCH_NAME "arm"
#elif defined(_M_X64) || defined(_M_AMD64) || defined(__amd64__) || defined(__x86_64__)
#define ARCH_NAME "x64"
#elif defined(_M_ALPHA) || defined(__alpha__)
#define ARCH_NAME "axp"
#elif defined(_M_PPC) || defined(__ppc__)
#define ARCH_NAME "ppc"
#else
#error Unknown architecture!
#endif

#if _DEBUG && NDEBUG
#error Both _DEBUG and NDEBUG are set, what is doing on?
#endif

#if defined(_DEBUG)
#define RELEASE_TYPE "debug"
#else
#define RELEASE_TYPE "release"
#endif

#define BUILDSTRING OS_NAME " " ARCH_NAME " " RELEASE_TYPE
