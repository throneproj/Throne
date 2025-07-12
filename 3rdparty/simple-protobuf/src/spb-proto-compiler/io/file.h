/***************************************************************************\
* Name              : file io                                               *
* Description       : basic file io                                         *
* Author            : antonin.kriz@gmail.com                                *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

/**
 * @brief loads file from file_path
 *
 * @param file_path files path
 * @return file's content
 */
auto load_file( const std::filesystem::path & file_path ) -> std::string;

/**
 * @brief save file_content to a file_path
 *
 * @param file_path files path
 * @param file_content files content
 */
void save_file( const std::filesystem::path & file_path, std::string_view file_content );
