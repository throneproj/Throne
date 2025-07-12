/***************************************************************************\
* Name        : .proto parser                                               *
* Description : parse proto file and constructs an ast tree                 *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "ast/proto-file.h"
#include <filesystem>
#include <span>
#include <string_view>

/**
 * @brief generate C++ file name from a .proto file name
 *        example: "foo.proto" -> "foo.pb.h"
 *
 * @param proto_file_path proto file name
 * @param extension CPP file extension (example: ".pb.h")
 * @return CPP file name
 */
[[nodiscard]] auto cpp_file_name_from_proto( const std::filesystem::path & proto_file_path,
                                             std::string_view extension ) -> std::filesystem::path;

/**
 * @brief parse proto file
 *
 * @param file_path file path
 * @param import_paths proto import paths (searched in order)
 * @param base_dir working directory
 * @return proto_file parsed proto
 */
auto parse_proto_file(
    const std::filesystem::path & file_path, std::span< const std::filesystem::path > import_paths,
    const std::filesystem::path & base_dir = std::filesystem::current_path( ) ) -> proto_file;

/**
 * @brief parse proto file content, used for fuzzing
 *
 * @param file proto file
 */
void parse_proto_file_content( proto_file & file );
