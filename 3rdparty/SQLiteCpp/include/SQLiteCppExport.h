/**
 * @file    SQLiteCppExport.h
 * @ingroup SQLiteCpp
 * @brief   File with macros needed in the generation of Windows DLLs
 *
 *
 * Copyright (c) 2012-2023 Sebastien Rombauts (sebastien.rombauts@gmail.com)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#pragma once

/*
* #define SQLITECPP_COMPILE_DLL to compile a DLL under Windows
* #define SQLITECPP_EXPORT to export symbols when creating the DLL, otherwise it defaults to importing symbols
*/

/* Windows DLL export/import */
#if defined(_WIN32)&& !defined(__GNUC__) && defined(SQLITECPP_COMPILE_DLL)
    #if SQLITECPP_DLL_EXPORT
        #define SQLITECPP_API __declspec(dllexport)
    #else
        #define SQLITECPP_API __declspec(dllimport)
    #endif    
#else    
    #if __GNUC__ >= 4
        #define SQLITECPP_API __attribute__ ((visibility ("default")))
    #else
        #define SQLITECPP_API
    #endif
#endif

#if defined(WIN32) && defined(SQLITECPP_COMPILE_DLL)
    #pragma warning( disable : 4251 )
    #pragma warning( disable : 4275 )
#endif
