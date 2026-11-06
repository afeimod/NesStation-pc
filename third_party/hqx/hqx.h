/*
 * Copyright (C) 2003 Maxim Stepin ( maxst@hiend3d.com )
 *
 * Copyright (C) 2010 Cameron Zemek ( grom@zeminvaders.net)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __HQX_H_
#define __HQX_H_

#include <stdint.h>

#if defined( __GNUC__ )
    #ifdef __MINGW32__
        #define HQX_CALLCONV __stdcall
    #else
        #define HQX_CALLCONV
    #endif
#else
    #define HQX_CALLCONV
#endif

/* NesStation-pc: 静态编入（third_party/hqx，随主目标编译）。
 * 原 DLL 导入/导出宏在静态构建下会产生 __declspec(dllimport) 链接错误，
 * 仅在显式声明 HQX_SHARED_BUILD 时保留动态链接语义。 */
#if defined(_WIN32) && defined(HQX_SHARED_BUILD)
    #ifdef DLL_EXPORT
        #define HQX_API __declspec(dllexport)
    #else
        #define HQX_API __declspec(dllimport)
    #endif
#else
    #define HQX_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

HQX_API void HQX_CALLCONV hqxInit(void);
HQX_API void HQX_CALLCONV hq2x_32( const uint32_t * src, uint32_t * dest, int width, int height );
HQX_API void HQX_CALLCONV hq3x_32( const uint32_t * src, uint32_t * dest, int width, int height );
HQX_API void HQX_CALLCONV hq4x_32( const uint32_t * src, uint32_t * dest, int width, int height );

HQX_API void HQX_CALLCONV hq2x_32_rb( const uint32_t * src, uint32_t src_rowBytes, uint32_t * dest, uint32_t dest_rowBytes, int width, int height );
HQX_API void HQX_CALLCONV hq3x_32_rb( const uint32_t * src, uint32_t src_rowBytes, uint32_t * dest, uint32_t dest_rowBytes, int width, int height );
HQX_API void HQX_CALLCONV hq4x_32_rb( const uint32_t * src, uint32_t src_rowBytes, uint32_t * dest, uint32_t dest_rowBytes, int width, int height );

#ifdef __cplusplus
}
#endif

#endif
