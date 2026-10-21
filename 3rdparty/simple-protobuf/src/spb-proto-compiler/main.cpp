/***************************************************************************\
* Name        : spb-protoc                                                  *
* Description : proto file compiler                                         *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#include "dumper/dumper.h"
#include "parser/parser.h"
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
using namespace std::literals;
namespace fs = std::filesystem;

constexpr auto opt_version = "--version"sv;
constexpr auto opt_v = "-v"sv;

constexpr auto opt_help = "--help"sv;
constexpr auto opt_h = "-h"sv;

constexpr auto opt_cpp_out_prefix = "--cpp_out="sv;
constexpr auto opt_cpp_out = "--cpp_out"sv;

constexpr auto opt_proto_path_prefix = "--proto_path="sv;
constexpr auto opt_i = "-I"sv;

void print_usage()
{
    std::cout << "Usage: spb-protoc [OPTION] PROTO_FILES\n"
              << "Parse PROTO_FILES and generate C++ source files.\n"
              << "  -IPATH, --proto_path=PATH  Specify the directory in which to search for imports.\n"
              << "May be specified multiple times. directories will be searched in order.\n"
              << "  -v, --version               Show version info and exit.\n"
              << "  -h, --help                  Show this text and exit.\n"
              << "  --cpp_out=OUT_DIR           Generate C++ header and source.\n\n";
}

auto construct_path(fs::path::iterator begin, fs::path::iterator end) -> fs::path
{
    fs::path result;
    for (auto it = begin; it != end; ++it)
    {
        result /= *it;
    }

    return result.parent_path();
}

auto get_relative_output_dir(const fs::path &input_file, std::span<const fs::path> import_paths) -> fs::path
{
    for (const auto &import : import_paths)
    {
        const auto miss = std::mismatch(import.begin(), import.end(), input_file.begin(), input_file.end());

        if (miss.first == import.end())
            return construct_path(miss.second, input_file.end());
    }
    return {};
}

void process_file(const fs::path &input_file, std::span<const fs::path> import_paths,
                  const fs::path &output_dir)
{
    const auto parsed_file = parse_proto_file(input_file, import_paths);
    const auto output_cpp_header = cpp_file_name_from_proto(input_file, ".pb.h");
    const auto output_cpp = cpp_file_name_from_proto(input_file, ".pb.cc");
    const auto rel_output_dir = get_relative_output_dir(input_file, import_paths);

    fs::create_directories(output_dir / rel_output_dir);
    auto cpp_header_stream = std::ofstream(output_dir / rel_output_dir / output_cpp_header);
    dump_cpp_header(parsed_file, cpp_header_stream);

    auto cpp_stream = std::ofstream(output_dir / rel_output_dir / output_cpp);
    dump_cpp(parsed_file, rel_output_dir / output_cpp_header, cpp_stream);
}

} // namespace

auto main(int argc, char *argv[]) -> int
{
    if (argc < 2)
    {
        print_usage();
        return 1;
    }

    auto output_dir = fs::path();
    auto import_paths = std::vector<fs::path>();

    for (; argc > 1 && argv[1][0] == '-'; argc--, argv++)
    {
        const auto opt = std::string_view(argv[1]);

        if (opt_help == opt || opt_h == opt)
        {
            print_usage();
            return 0;
        }

        if (opt_version == opt || opt_v == opt)
        {
            std::cout << "spb-protoc version 0.1.0\n";
            return 0;
        }

        if (opt_cpp_out == opt)
        {
            if (argc < 3)
            {
                std::cerr << "Missing value for option: " << opt << "\n";
                return 1;
            }
            output_dir = fs::absolute(argv[2]);
            argc--;
            argv++;
            continue;
        }

        if (opt_i == opt)
        {
            if (argc < 3)
            {
                std::cerr << "Missing value for option: " << opt << "\n";
                return 1;
            }
            import_paths.emplace_back(fs::absolute(argv[2]));
            argc--;
            argv++;
            continue;
        }

        if (opt.starts_with(opt_proto_path_prefix))
        {
            import_paths.emplace_back(fs::absolute(opt.substr(opt_proto_path_prefix.size())));
        }
        else if (opt.starts_with(opt_i) && opt.size() > opt_i.size())
        {
            import_paths.emplace_back(fs::absolute(opt.substr(opt_i.size())));
        }
        else if (opt.starts_with(opt_cpp_out_prefix))
        {
            output_dir = fs::absolute(opt.substr(opt_cpp_out_prefix.size()));
        }
        else
        {
            std::cerr << "Unknown option: " << opt << ", use -h or --help\n";
            return 1;
        }
    }

    auto input_files = std::vector<fs::path>();
    for (auto i = 1; i < argc; i++)
    {
        input_files.emplace_back(fs::absolute(argv[i]));
    }

    if (output_dir.empty())
    {
        std::cerr << "Missing output directory, use --cpp_out=OUT_DIR:\n";
        return 1;
    }

    if (input_files.empty())
    {
        std::cerr << "Missing input files, use PROTO_FILES:\n";
        return 1;
    }

    try
    {
        for (const auto &input_file : input_files)
        {
            process_file(input_file, import_paths, output_dir);
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}