/***************************************************************************\
* Name        : C++ header dumper                                           *
* Description : generate C++ header with all structs and enums              *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#include "header.h"
#include "../parser/char_stream.h"
#include "ast/ast.h"
#include "ast/proto-common.h"
#include "ast/proto-field.h"
#include "ast/proto-file.h"
#include "ast/proto-import.h"
#include "ast/proto-message.h"
#include "io/file.h"
#include "parser/options.h"
#include "parser/parser.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <initializer_list>
#include <spb/json/deserialize.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace std::literals;

namespace
{

using cpp_includes = std::set< std::string >;

void dump_comment( std::ostream & stream, const proto_comment & comment )
{
    for( const auto & comm : comment.comments )
    {
        if( comm.starts_with( "//[[" ) )
        {
            //- ignore options in comments
            continue;
        }

        stream << comm;
        if( comm.back( ) != '\n' )
        {
            stream << '\n';
        }
    }
}

auto trim_include( std::string_view str ) -> std::string
{
    auto p_begin = str.data( );
    auto p_end   = str.data( ) + str.size( );
    while( p_begin < p_end && isspace( *p_begin ) )
    {
        p_begin++;
    }

    while( p_begin < p_end && isspace( p_end[ -1 ] ) )
    {
        p_end--;
    }

    if( p_begin == p_end )
    {
        return { };
    }

    auto add_prefix  = *p_begin != '"' && *p_begin != '<';
    auto add_postfix = p_end[ -1 ] != '"' && p_end[ -1 ] != '>';

    if( add_prefix || add_postfix )
    {
        return '"' + std::string( str ) + '"';
    }
    return std::string( str );
}

void dump_includes( std::ostream & stream, const cpp_includes & includes )
{
    for( auto & include : includes )
    {
        auto file = trim_include( include );
        if( !file.empty( ) )
        {
            stream << "#include " << file << "\n";
        }
    }
    stream << "\n";
}

auto contains_map( const proto_messages & messages ) -> bool
{
    for( const auto & message : messages )
    {
        if( !message.maps.empty( ) )
        {
            return true;
        }

        if( contains_map( message.messages ) )
        {
            return true;
        }
    }
    return false;
}

auto contains_oneof( const proto_messages & messages ) -> bool
{
    for( const auto & message : messages )
    {
        if( !message.oneofs.empty( ) )
        {
            return true;
        }

        if( contains_oneof( message.messages ) )
        {
            return true;
        }
    }
    return false;
}

void get_std_includes( cpp_includes & includes, const proto_file & file )
{
    includes.insert( "<spb/json.hpp>" );
    includes.insert( "<spb/pb.hpp>" );
    includes.insert( "<cstdint>" );
    includes.insert( "<cstddef>" );

    if( contains_map( file.package.messages ) )
    {
        includes.insert( "<map>" );
    }
    if( contains_oneof( file.package.messages ) )
    {
        includes.insert( "<variant>" );
    }
}

void get_include_from_options( cpp_includes & includes, const proto_options & options,
                               const proto_options & message_options,
                               const proto_options & file_options, std::string_view option_include )
{
    if( auto include = options.find( option_include ); include != options.end( ) )
    {
        includes.insert( std::string( include->second ) );
        return;
    }
    if( auto include = message_options.find( option_include ); include != message_options.end( ) )
    {
        includes.insert( std::string( include->second ) );
        return;
    }
    if( auto include = file_options.find( option_include ); include != file_options.end( ) )
    {
        includes.insert( std::string( include->second ) );
        return;
    }
}

void get_includes_from_field( cpp_includes & includes, const proto_field & field,
                              const proto_message & message, const proto_file & file )
{
    if( field.label == proto_field::Label::OPTIONAL )
    {
        get_include_from_options( includes, field.options, message.options, file.options,
                                  option_optional_include );
    }
    if( field.label == proto_field::Label::REPEATED )
    {
        get_include_from_options( includes, field.options, message.options, file.options,
                                  option_repeated_include );
    }
    if( field.label == proto_field::Label::PTR )
    {
        get_include_from_options( includes, field.options, message.options, file.options,
                                  option_pointer_include );
    }
    if( field.type == proto_field::Type::STRING )
    {
        get_include_from_options( includes, field.options, message.options, file.options,
                                  option_string_include );
    }
    if( field.type == proto_field::Type::BYTES )
    {
        get_include_from_options( includes, field.options, message.options, file.options,
                                  option_bytes_include );
    }
}

void get_message_includes( cpp_includes & includes, const proto_message & message,
                           const proto_file & file )
{
    for( const auto & field : message.fields )
    {
        get_includes_from_field( includes, field, message, file );
    }

    for( const auto & oneof : message.oneofs )
    {
        for( const auto & field : oneof.fields )
        {
            get_includes_from_field( includes, field, message, file );
        }
    }

    for( const auto & map : message.maps )
    {
        get_includes_from_field( includes, map.key, message, file );
        get_includes_from_field( includes, map.value, message, file );
    }

    for( const auto & m : message.messages )
    {
        get_message_includes( includes, m, file );
    }
}

void get_user_includes( cpp_includes & includes, const proto_file & file )
{
    get_message_includes( includes, file.package, file );
}

void get_imports( cpp_includes & includes, const proto_file & file )
{
    for( const auto & import : file.file_imports )
    {
        includes.insert( "\"" + cpp_file_name_from_proto( import.path, ".pb.h" ).string( ) + "\"" );
    }
}

void dump_pragma( std::ostream & stream, const proto_file & )
{
    stream << "#pragma once\n\n";
}

void dump_syntax( std::ostream & stream, const proto_file & file )
{
    dump_comment( stream, file.syntax.comments );
}

auto type_literal_suffix( proto_field::Type type ) -> std::string_view
{
    static constexpr auto type_map =
        std::array< std::pair< proto_field::Type, std::string_view >, 12 >{ {
            { proto_field::Type::INT64, "LL" },
            { proto_field::Type::UINT32, "U" },
            { proto_field::Type::UINT64, "ULL" },
            { proto_field::Type::SINT64, "LL" },
            { proto_field::Type::FIXED32, "U" },
            { proto_field::Type::FIXED64, "ULL" },
            { proto_field::Type::SFIXED64, "LL" },
            { proto_field::Type::FLOAT, "F" },
        } };

    for( auto [ proto_type, suffix ] : type_map )
    {
        if( type == proto_type )
        {
            return suffix;
        }
    }

    return { };
}

auto get_field_bits( const proto_field & field ) -> std::string_view
{
    if( auto p_name = field.options.find( option_field_type ); p_name != field.options.end( ) )
    {
        auto bitfield = p_name->second;
        if( auto index = bitfield.find( ':' ); index != std::string_view::npos )
        {
            const_cast< proto_field & >( field ).bit_field = bitfield.substr( index + 1 );
            return bitfield.substr( index );
        }
    }
    return { };
}

auto get_container_type( const proto_options & options, const proto_options & message_options,
                         const proto_options & file_options, std::string_view option,
                         std::string_view ctype, std::string_view default_type = { } )
    -> std::string
{
    if( auto p_name = options.find( option ); p_name != options.end( ) )
    {
        return replace( p_name->second, "$", ctype );
    }

    if( auto p_name = message_options.find( option ); p_name != message_options.end( ) )
    {
        return replace( p_name->second, "$", ctype );
    }

    if( auto p_name = file_options.find( option ); p_name != file_options.end( ) )
    {
        return replace( p_name->second, "$", ctype );
    }

    return replace( default_type, "$", ctype );
}

auto get_enum_type( const proto_file & file, const proto_options & options,
                    const proto_options & message_options, const proto_options & file_options,
                    std::string_view default_type ) -> std::string_view
{
    static constexpr auto type_map =
        std::array< std::pair< std::string_view, std::string_view >, 6 >{ {
            { "int8"sv, "int8_t"sv },
            { "uint8"sv, "uint8_t"sv },
            { "int16"sv, "int16_t"sv },
            { "uint16"sv, "uint16_t"sv },
            { "int32"sv, "int32_t"sv },
        } };

    auto ctype_from_pb = [ & ]( std::string_view type )
    {
        for( auto [ proto_type, c_type ] : type_map )
        {
            if( type == proto_type )
            {
                return c_type;
            }
        }
        throw_parse_error( file, type, "invalid enum type: " + std::string( type ) );
    };

    if( auto p_name = options.find( option_enum_type ); p_name != options.end( ) )
    {
        return ctype_from_pb( p_name->second );
    }

    if( auto p_name = message_options.find( option_enum_type ); p_name != message_options.end( ) )
    {
        return ctype_from_pb( p_name->second );
    }

    if( auto p_name = file_options.find( option_enum_type ); p_name != file_options.end( ) )
    {
        return ctype_from_pb( p_name->second );
    }

    return default_type;
}

auto convert_to_ctype( const proto_file & file, const proto_field & field,
                       const proto_message & message = { } ) -> std::string
{
    if( field.bit_type != proto_field::BitType::NONE )
    {
        switch( field.bit_type )
        {
        case proto_field::BitType::NONE:
            throw_parse_error( file, field.type_name, "invalid type" );
        case proto_field::BitType::INT8:
            return "int8_t";
        case proto_field::BitType::INT16:
            return "int16_t";
        case proto_field::BitType::INT32:
            return "int32_t";
        case proto_field::BitType::INT64:
            return "int64_t";
        case proto_field::BitType::UINT8:
            return "uint8_t";
        case proto_field::BitType::UINT16:
            return "uint16_t";
        case proto_field::BitType::UINT32:
            return "uint32_t";
        case proto_field::BitType::UINT64:
            return "uint64_t";
        }
    }

    switch( field.type )
    {
    case proto_field::Type::NONE:
        throw_parse_error( file, field.type_name, "invalid type" );

    case proto_field::Type::STRING:
        return get_container_type( field.options, message.options, file.options, option_string_type,
                                   "char", "std::string" );
    case proto_field::Type::BYTES:
        return get_container_type( field.options, message.options, file.options, option_bytes_type,
                                   "std::byte", "std::vector<$>" );
    case proto_field::Type::ENUM:
    case proto_field::Type::MESSAGE:
        return replace( field.type_name, ".", "::" );

    case proto_field::Type::FLOAT:
        return "float";
    case proto_field::Type::DOUBLE:
        return "double";
    case proto_field::Type::BOOL:
        return "bool";
    case proto_field::Type::SFIXED32:
    case proto_field::Type::INT32:
    case proto_field::Type::SINT32:
        return "int32_t";
    case proto_field::Type::FIXED32:
    case proto_field::Type::UINT32:
        return "uint32_t";
    case proto_field::Type::SFIXED64:
    case proto_field::Type::INT64:
    case proto_field::Type::SINT64:
        return "int64_t";
    case proto_field::Type::UINT64:
    case proto_field::Type::FIXED64:
        return "uint64_t";
    }

    throw_parse_error( file, field.type_name, "invalid type" );
}

void dump_field_type_and_name( std::ostream & stream, const proto_field & field,
                               const proto_message & message, const proto_file & file )
{
    const auto ctype = convert_to_ctype( file, field, message );

    switch( field.label )
    {
    case proto_field::Label::NONE:
        stream << ctype << ' ' << field.name << get_field_bits( field );
        return;
    case proto_field::Label::OPTIONAL:
        stream << get_container_type( field.options, message.options, file.options,
                                      option_optional_type, ctype, "std::optional<$>" );
        break;
    case proto_field::Label::REPEATED:
        stream << get_container_type( field.options, message.options, file.options,
                                      option_repeated_type, ctype, "std::vector<$>" );
        break;
    case proto_field::Label::PTR:
        stream << get_container_type( field.options, message.options, file.options,
                                      option_pointer_type, ctype, "std::unique_ptr<$>" );
        break;
    }
    if( auto bitfield = get_field_bits( field ); !bitfield.empty( ) )
    {
        throw_parse_error( file, bitfield, "bitfield can be used only with `required` label" );
    }
    stream << ' ' << field.name;
}

void dump_enum_field( std::ostream & stream, const proto_base & field )
{
    dump_comment( stream, field.comment );

    stream << field.name << " = " << field.number << ",\n";
}

void dump_enum( std::ostream & stream, const proto_enum & my_enum, const proto_message & message,
                const proto_file & file )
{
    dump_comment( stream, my_enum.comment );

    stream << "enum class " << my_enum.name << " : "
           << get_enum_type( file, my_enum.options, message.options, file.options, "int32_t" )
           << "\n{\n";
    for( const auto & field : my_enum.fields )
    {
        dump_enum_field( stream, field );
    }
    stream << "};\n";
}

void dump_message_oneof( std::ostream & stream, const proto_oneof & oneof, const proto_file & file )
{
    dump_comment( stream, oneof.comment );

    auto put_comma = false;
    stream << "std::variant< ";
    for( const auto & field : oneof.fields )
    {
        if( put_comma )
        {
            stream << ", ";
        }

        stream << convert_to_ctype( file, field );
        put_comma = true;
    }
    stream << " > " << oneof.name << ";\n";
}

void dump_message_map( std::ostream & stream, const proto_map & map, const proto_file & file )
{
    dump_comment( stream, map.comment );

    stream << "std::map< " << convert_to_ctype( file, map.key ) << ", "
           << convert_to_ctype( file, map.value ) << " > ";
    stream << map.name << ";\n";
}

void dump_default_value( std::ostream & stream, const proto_field & field )
{
    if( auto p_index = field.options.find( "default" ); p_index != field.options.end( ) )
    {
        if( field.type == proto_field::Type::ENUM )
        {
            stream << " = " << replace( field.type_name, ".", "::" ) << "::" << p_index->second;
        }
        else if( field.type == proto_field::Type::STRING &&
                 ( p_index->second.size( ) < 2 || p_index->second.front( ) != '"' ||
                   p_index->second.back( ) != '"' ) )
        {
            stream << " = \"" << p_index->second << "\"";
        }
        else
        {
            stream << " = " << p_index->second << type_literal_suffix( field.type );
        }
    }
}

void dump_deprecated_attribute( std::ostream & stream, const proto_field & field )
{
    if( auto p_index = field.options.find( "deprecated" );
        p_index != field.options.end( ) && p_index->second == "true" )
    {
        stream << "[[deprecated]] ";
    }
}

void dump_message_field( std::ostream & stream, const proto_field & field,
                         const proto_message & message, const proto_file & file )
{
    dump_comment( stream, field.comment );
    dump_deprecated_attribute( stream, field );
    dump_field_type_and_name( stream, field, message, file );
    dump_default_value( stream, field );
    stream << ";\n";
}

void dump_forwards( std::ostream & stream, const forwarded_declarations & forwards )
{
    for( const auto & forward : forwards )
    {
        stream << "struct " << forward << ";\n";
    }
    if( !forwards.empty( ) )
    {
        stream << '\n';
    }
}

void dump_message( std::ostream & stream, const proto_message & message, const proto_file & file )
{
    dump_comment( stream, message.comment );

    stream << "struct " << message.name << "\n{\n";

    dump_forwards( stream, message.forwards );
    for( const auto & sub_enum : message.enums )
    {
        dump_enum( stream, sub_enum, message, file );
    }

    for( const auto & sub_message : message.messages )
    {
        dump_message( stream, sub_message, file );
    }

    for( const auto & field : message.fields )
    {
        dump_message_field( stream, field, message, file );
    }

    for( const auto & map : message.maps )
    {
        dump_message_map( stream, map, file );
    }

    for( const auto & oneof : message.oneofs )
    {
        dump_message_oneof( stream, oneof, file );
    }

    //- TODO: is this used in any way?
    // stream << "auto operator == ( const " << message.name << " & ) const noexcept ->
    // bool = default;\n"; stream << "auto operator != ( const " << message.name << " &
    // ) const noexcept -> bool = default;\n";
    stream << "};\n";
}

void dump_messages( std::ostream & stream, const proto_file & file )
{
    dump_forwards( stream, file.package.forwards );
    for( const auto & message : file.package.messages )
    {
        dump_message( stream, message, file );
    }
}

void dump_enums( std::ostream & stream, const proto_file & file )
{
    for( const auto & my_enum : file.package.enums )
    {
        dump_enum( stream, my_enum, file.package, file );
    }
}

void dump_package_begin( std::ostream & stream, const proto_file & file )
{
    dump_comment( stream, file.package.comment );
    if( !file.package.name.empty( ) )
    {
        stream << "namespace " << replace( file.package.name, ".", "::" ) << "\n{\n";
    }
}

void dump_package_end( std::ostream & stream, const proto_file & file )
{
    if( !file.package.name.empty( ) )
    {
        stream << "}// namespace " << replace( file.package.name, ".", "::" ) << "\n\n";
    }
}
}// namespace

void throw_parse_error( const proto_file & file, std::string_view at, std::string_view message )
{
    auto stream = spb::char_stream( file.content );
    stream.skip_to( at.data( ) );
    stream.throw_parse_error( message );
}

void dump_cpp_definitions( const proto_file & file, std::ostream & stream )
{
    auto includes = cpp_includes( );
    dump_pragma( stream, file );
    get_imports( includes, file );
    get_std_includes( includes, file );
    get_user_includes( includes, file );
    dump_includes( stream, includes );
    dump_syntax( stream, file );
    dump_package_begin( stream, file );
    dump_enums( stream, file );
    dump_messages( stream, file );
    dump_package_end( stream, file );
}

auto replace( std::string_view input, std::string_view what, std::string_view with ) -> std::string
{
    auto result = std::string( input );
    auto pos    = size_t{ };

    while( ( pos = result.find( what, pos ) ) != std::string::npos )
    {
        result.replace( pos, what.size( ), with );
        pos += with.size( );
    }

    return result;
}
