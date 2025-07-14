/***************************************************************************\
* Name        : .proto parser                                               *
* Description : parse proto file and constructs an ast tree                 *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#include "parser.h"
#include "ast/proto-field.h"
#include "ast/proto-file.h"
#include "dumper/header.h"
#include "options.h"
#include <array>
#include <ast/ast.h>
#include <ast/ast-types.h>
#include <cctype>
#include <cerrno>
#include <concepts>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <io/file.h>
#include <parser/char_stream.h>
#include <spb/to_from_chars.h>
#include <stdexcept>
#include <string_view>

namespace
{
namespace fs       = std::filesystem;
using parsed_files = std::set< std::string >;

[[nodiscard]] auto parse_proto_file( fs::path file, parsed_files &,
                                     std::span< const fs::path > import_paths,
                                     const fs::path & base_dir ) -> proto_file;

auto find_file_in_paths( const fs::path & file_name, std::span< const fs::path > import_paths,
                         const fs::path & base_dir ) -> fs::path
{
    if( file_name.has_root_path( ) )
    {
        if( fs::exists( file_name ) )
        {
            return file_name;
        }
    }
    else
    {
        if( fs::exists( base_dir / file_name ) )
        {
            return base_dir / file_name;
        }

        for( const auto & import_path : import_paths )
        {
            auto file_path = import_path.has_root_path( ) ? import_path / file_name
                                                          : base_dir / import_path / file_name;
            if( fs::exists( file_path ) )
            {
                return file_path;
            }
        }
    }

    throw std::runtime_error( strerror( ENOENT ) );
}

[[nodiscard]] auto parse_all_imports( const proto_file & file, parsed_files & already_parsed,
                                      std::span< const fs::path > import_paths,
                                      const fs::path & base_dir ) -> proto_files
{
    proto_files result;
    result.reserve( file.imports.size( ) );
    for( const auto & import : file.imports )
    {
        if( !already_parsed.contains( std::string( import.file_name ) ) )
        {
            try
            {
                result.emplace_back(
                    parse_proto_file( import.file_name, already_parsed, import_paths, base_dir ) );
            }
            catch( const std::runtime_error & error )
            {
                throw_parse_error( file, import.file_name, error.what( ) );
            }
        }
    }
    return result;
}

void parse_or_throw( bool parsed, spb::char_stream & stream, std::string_view message )
{
    if( !parsed )
    {
        stream.throw_parse_error( message );
    }
}

void consume_or_fail( spb::char_stream & stream, char c )
{
    if( !stream.consume( c ) )
    {
        return stream.throw_parse_error( "(expecting '" + std::string( 1, c ) + "')" );
    }
}

void consume_or_fail( spb::char_stream & stream, std::string_view token )
{
    if( !stream.consume( token ) )
    {
        return stream.throw_parse_error( "(expecting '" + std::string( token ) + "')" );
    }
}

void skip_white_space_until_new_line( spb::char_stream & stream )
{
    while( ( isspace( stream.current_char( ) ) != 0 ) && stream.current_char( ) != '\n' )
    {
        stream.consume_current_char( false );
    }
}

template < typename T >
concept int_or_float = std::integral< T > || std::floating_point< T >;

auto consume_number( spb::char_stream & stream, std::integral auto & number ) -> bool
{
    number    = { };
    auto base = 10;
    if( stream.consume( '0' ) )
    {
        base = 8;
        if( stream.consume( 'x' ) || stream.consume( 'X' ) )
        {
            base = 16;
        }
        if( !::isdigit( stream.current_char( ) ) )
        {
            return true;
        }
    }
    auto result = spb_std_emu::from_chars( stream.begin( ), stream.end( ), number, base );
    if( result.ec == std::errc{ } ) [[likely]]
    {
        stream.skip_to( result.ptr );
        return true;
    }
    return false;
}

auto consume_number( spb::char_stream & stream, std::floating_point auto & number ) -> bool
{
    auto result = spb_std_emu::from_chars( stream.begin( ), stream.end( ), number );
    if( result.ec == std::errc{ } ) [[likely]]
    {
        stream.skip_to( result.ptr );
        return true;
    }
    return false;
}

auto consume_int( spb::char_stream & stream, std::integral auto & number ) -> bool
{
    return consume_number( stream, number );
}

auto consume_float( spb::char_stream & stream, std::floating_point auto & number ) -> bool
{
    return consume_number( stream, number );
}

template < int_or_float T >
auto parse_number( spb::char_stream & stream ) -> T
{
    auto result = T{ };
    if( consume_number( stream, result ) )
    {
        return result;
    }
    stream.throw_parse_error( "expecting number" );
}

auto parse_int_or_float( spb::char_stream & stream ) -> std::string_view
{
    const auto * start = stream.begin( );
    auto number        = double( );
    auto result        = spb_std_emu::from_chars( stream.begin( ), stream.end( ), number );
    if( result.ec == std::errc{ } ) [[likely]]
    {
        stream.skip_to( result.ptr );
        return { start, static_cast< size_t >( result.ptr - start ) };
    }
    stream.throw_parse_error( "expecting number" );
}

//- parse single line comment // \n
void parse_comment_line( spb::char_stream & stream, proto_comment & comment )
{
    const auto * start = stream.begin( );
    const auto end     = stream.content( ).find( '\n' );
    if( end == std::string_view::npos )
    {
        stream.throw_parse_error( "expecting \\n" );
    }

    comment.comments.emplace_back( start - 2, end + 2 );

    stream.skip_to( start + end + 1 );
}

//- parse multiline comment /* */
void parse_comment_multiline( spb::char_stream & stream, proto_comment & comment )
{
    const auto * start = stream.begin( );
    const auto end     = stream.content( ).find( "*/" );
    if( end == std::string_view::npos )
    {
        stream.throw_parse_error( "expecting */" );
    }

    comment.comments.emplace_back( start - 2, end + 4 );
    stream.skip_to( start + end + 2 );
}

//- parse // \n or /**/
auto parse_comment( spb::char_stream & stream ) -> proto_comment
{
    auto result = proto_comment{ };

    while( stream.current_char( ) == '/' )
    {
        stream.consume_current_char( false );
        if( stream.current_char( ) == '/' )
        {
            stream.consume_current_char( false );
            parse_comment_line( stream, result );
        }
        else if( stream.current_char( ) == '*' )
        {
            stream.consume_current_char( false );
            parse_comment_multiline( stream, result );
        }
        else
        {
            stream.throw_parse_error( "expecting // or /*" );
        }
    }
    return result;
}

//- parse ;
[[nodiscard]] auto parse_empty_statement( spb::char_stream & stream ) -> bool
{
    return stream.consume( ';' );
}

//- parse "string" | 'string'
[[nodiscard]] auto parse_string_literal( spb::char_stream & stream ) -> std::string_view
{
    const auto c = stream.current_char( );
    if( c != '"' && c != '\'' )
    {
        stream.throw_parse_error( "expecting \" or '" );
        return { };
    }

    stream.consume_current_char( false );
    const auto * start = stream.begin( );
    auto current       = stream.current_char( );
    while( ( current != 0 ) && current != c )
    {
        stream.consume_current_char( false );
        current = stream.current_char( );
    }

    if( current != c )
    {
        stream.throw_parse_error( "missing string end" );
    }

    auto result = std::string_view( start, static_cast< size_t >( stream.begin( ) - start ) );
    stream.consume_current_char( true );
    return result;
}

[[nodiscard]] auto parse_ident( spb::char_stream & stream, bool skip_last_white_space = true )
    -> std::string_view
{
    const auto * start = stream.begin( );

    if( isalpha( stream.current_char( ) ) == 0 )
    {
        stream.throw_parse_error( "expecting identifier(a-zA-Z)" );
        return { };
    }

    stream.consume_current_char( false );
    auto current = stream.current_char( );
    while( ( current != 0 ) && ( isalnum( current ) != 0 || current == '_' ) )
    {
        stream.consume_current_char( false );
        current = stream.current_char( );
    }

    auto result = std::string_view( start, static_cast< size_t >( stream.begin( ) - start ) );
    if( skip_last_white_space )
    {
        stream.consume_space( );
    }
    return result;
}

[[nodiscard]] auto parse_full_ident( spb::char_stream & stream ) -> std::string_view
{
    const auto * start = stream.begin( );

    for( ;; )
    {
        ( void ) parse_ident( stream, false );
        if( stream.current_char( ) != '.' )
        {
            break;
        }
        stream.consume_current_char( false );
    }
    auto result = std::string_view{ start, static_cast< size_t >( stream.begin( ) - start ) };
    stream.consume_space( );
    return result;
}

void parse_top_level_service_body( spb::char_stream & stream, proto_file &, proto_comment && )
{
    while( !stream.consume( '}' ) )
        stream.consume_current_char( true );
}

void consume_statement_end( spb::char_stream & stream, proto_comment & comment )
{
    if( stream.current_char( ) != ';' )
    {
        return stream.throw_parse_error( R"(expecting ";")" );
    }
    stream.consume_current_char( false );
    skip_white_space_until_new_line( stream );
    if( stream.current_char( ) == '/' )
    {
        stream.consume_current_char( false );
        auto line_comment = proto_comment( );
        parse_comment_line( stream, line_comment );
        comment.comments.insert( comment.comments.end( ), line_comment.comments.begin( ),
                                 line_comment.comments.end( ) );
    }
    stream.consume_space( );
}

void parse_top_level_syntax_body( spb::char_stream & stream, proto_syntax & syntax,
                                  proto_comment && comment )
{
    //- syntax = ( "proto2" | "proto3" );

    consume_or_fail( stream, '=' );

    syntax.comments = std::move( comment );
    if( stream.consume( R"("proto2")" ) )
    {
        syntax.version = 2;
        return consume_statement_end( stream, syntax.comments );
    }

    if( stream.consume( R"("proto3")" ) )
    {
        syntax.version = 3;
        return consume_statement_end( stream, syntax.comments );
    }

    stream.throw_parse_error( "expecting proto2 or proto3" );
}

void parse_top_level_syntax_or_service( spb::char_stream & stream, proto_file & file,
                                        proto_comment && comment )
{
    if( stream.consume( "syntax" ) )
    {
        return parse_top_level_syntax_body( stream, file.syntax, std::move( comment ) );
    }

    if( stream.consume( "service" ) )
    {
        return parse_top_level_service_body( stream, file, std::move( comment ) );
    }

    stream.throw_parse_error( "expecting syntax or service" );
}

void parse_top_level_import( spb::char_stream & stream, proto_imports & imports,
                             proto_comment && comment )
{
    // "import" [ "weak" | "public" ] strLit ";"
    consume_or_fail( stream, "import" );
    stream.consume( "weak" ) || stream.consume( "public" );
    imports.emplace_back( proto_import{ .file_name = parse_string_literal( stream ),
                                        .comments  = std::move( comment ) } );
    consume_statement_end( stream, imports.back( ).comments );
}

void parse_top_level_package( spb::char_stream & stream, proto_base & package,
                              proto_comment && comment )
{
    //- "package" fullIdent ";"
    consume_or_fail( stream, "package" );
    package.name    = parse_full_ident( stream );
    package.comment = std::move( comment );
    consume_statement_end( stream, package.comment );
}

[[nodiscard]] auto parse_option_name( spb::char_stream & stream ) -> std::string_view
{
    auto ident = std::string_view{ };

    //- ( ident | "(" fullIdent ")" ) { "." ident }
    parse_comment( stream );
    if( stream.consume( '(' ) )
    {
        ident = parse_full_ident( stream );
        consume_or_fail( stream, ')' );
    }
    else
    {
        ident = parse_ident( stream );
    }
    auto ident2 = std::string_view{ };

    while( stream.consume( '.' ) )
    {
        ident2 = parse_ident( stream );
    }

    if( ident2.empty( ) )
    {
        return ident;
    }

    return { ident.data( ),
             static_cast< size_t >( ident2.data( ) + ident2.size( ) - ident.data( ) ) };
}

[[nodiscard]] auto parse_constant( spb::char_stream & stream ) -> std::string_view
{
    //- fullIdent | ( [ "-" | "+" ] intLit ) | ( [ "-" | "+" ] floatLit ) | strLit | boolLit |
    // MessageValue
    if( stream.consume( "true" ) )
    {
        return "true";
    }
    if( stream.consume( "false" ) )
    {
        return "false";
    }
    const auto c = stream.current_char( );
    if( c == '"' || c == '\'' )
    {
        return parse_string_literal( stream );
    }
    if( isdigit( c ) || c == '+' || c == '-' )
    {
        return parse_int_or_float( stream );
    }
    return parse_full_ident( stream );
}

void parse_option_body( spb::char_stream & stream, proto_options & options )
{
    const auto option_name = parse_option_name( stream );
    consume_or_fail( stream, '=' );
    options[ option_name ] = parse_constant( stream );
}

void parse_option_from_comment( const spb::char_stream & stream, proto_options & options,
                                std::string_view comment )
{
    for( ;; )
    {
        auto start = comment.find( "[[" );
        if( start == std::string_view::npos )
        {
            return;
        }
        auto end = comment.find( "]]", start + 2 );
        if( end == std::string_view::npos )
        {
            return;
        }
        auto option = comment.substr( start + 2, end - start - 2 );
        comment.remove_prefix( end + 2 );
        auto option_stream = stream;
        option_stream.skip_to( option.data( ) );
        parse_option_body( option_stream, options );
    }
}

void parse_options_from_comments( const spb::char_stream & stream, proto_options & options,
                                  const proto_comment & comment )
{
    for( auto & c : comment.comments )
    {
        parse_option_from_comment( stream, options, c );
    }
}

[[nodiscard]] auto parse_option( spb::char_stream & stream, proto_options & options,
                                 proto_comment && comment ) -> bool
{
    //- "option" optionName  "=" constant ";"
    if( !stream.consume( "option" ) )
    {
        return false;
    }
    parse_option_body( stream, options );
    consume_statement_end( stream, comment );
    parse_options_from_comments( stream, options, comment );
    return true;
}

void parse_reserved_names( spb::char_stream & stream, proto_reserved_name & name,
                           proto_comment && comment )
{
    //- strFieldNames = strFieldName { "," strFieldName }
    //- strFieldName = "'" fieldName "'" | '"' fieldName '"'
    do
    {
        name.insert( parse_string_literal( stream ) );
    } while( stream.consume( ',' ) );

    consume_statement_end( stream, comment );
    comment.comments.clear( );
}

void parse_reserved_ranges( spb::char_stream & stream, proto_reserved_range & range,
                            proto_comment && comment )
{
    //- ranges = range { "," range }
    //- range =  intLit [ "to" ( intLit | "max" ) ]
    do
    {
        const auto number = parse_number< uint32_t >( stream );
        auto number2      = number;

        if( stream.consume( "to" ) )
        {
            if( stream.consume( "max" ) )
            {
                number2 = std::numeric_limits< decltype( number2 ) >::max( );
            }
            else
            {
                number2 = parse_number< uint32_t >( stream );
            }
        }

        range.emplace_back( number, number2 );
    } while( stream.consume( ',' ) );

    consume_statement_end( stream, comment );
    comment.comments.clear( );
}

[[nodiscard]] auto parse_extensions( spb::char_stream & stream, proto_reserved_range & extensions,
                                     proto_comment && comment ) -> bool
{
    //- extensions = "extensions" ranges ";"
    if( !stream.consume( "extensions" ) )
    {
        return false;
    }

    parse_reserved_ranges( stream, extensions, std::move( comment ) );
    return true;
}

[[nodiscard]] auto parse_reserved( spb::char_stream & stream, proto_reserved & reserved,
                                   proto_comment && comment ) -> bool
{
    //- reserved = "reserved" ( ranges | strFieldNames ) ";"
    if( !stream.consume( "reserved" ) )
    {
        return false;
    }

    auto c = stream.current_char( );
    if( c == '\'' || c == '"' )
    {
        parse_reserved_names( stream, reserved.reserved_name, std::move( comment ) );
        return true;
    }
    parse_reserved_ranges( stream, reserved.reserved_range, std::move( comment ) );
    return true;
}

[[nodiscard]] auto parse_field_options( spb::char_stream & stream ) -> proto_options
{
    auto options = proto_options{ };
    if( stream.consume( '[' ) )
    {
        auto first = true;
        while( !stream.consume( ']' ) )
        {
            if( !first )
            {
                consume_or_fail( stream, ',' );
            }
            parse_option_body( stream, options );
            first = false;
        }
    }
    return options;
}

void parse_enum_field( spb::char_stream & stream, proto_enum & new_enum, proto_comment && comment )
{
    //- enumField = ident "=" [ "-" ] intLit [ "[" enumValueOption { ","  enumValueOption } "]" ]";"
    //- enumValueOption = optionName "=" constant
    auto field =
        proto_base{ .name    = parse_ident( stream ),
                    .number  = ( consume_or_fail( stream, '=' ),
                                parse_number< decltype( proto_field::number ) >( stream ) ),
                    .options = parse_field_options( stream ),
                    .comment = std::move( comment ) };

    consume_statement_end( stream, field.comment );
    new_enum.fields.push_back( field );
}

[[nodiscard]] auto parse_enum_body( spb::char_stream & stream, proto_comment && enum_comment )
    -> proto_enum
{
    //- enumBody = "{" { option | enumField | emptyStatement | reserved } "}"

    auto new_enum = proto_enum{
        proto_base{
            .name    = parse_ident( stream ),
            .comment = std::move( enum_comment ),
        },
    };
    consume_or_fail( stream, '{' );

    parse_options_from_comments( stream, new_enum.options, new_enum.comment );

    while( !stream.consume( '}' ) )
    {
        auto comment = parse_comment( stream );
        if( stream.consume( '}' ) )
        {
            break;
        }

        if( !parse_option( stream, new_enum.options, std::move( comment ) ) &&
            !parse_reserved( stream, new_enum.reserved, std::move( comment ) ) &&
            !parse_empty_statement( stream ) )
        {
            parse_enum_field( stream, new_enum, std::move( comment ) );
        }
    }
    return new_enum;
}

[[nodiscard]] auto parse_enum( spb::char_stream & stream, proto_enums & enums,
                               proto_comment && comment ) -> bool
{
    //- enum = "enum" enumName enumBody
    if( !stream.consume( "enum" ) )
    {
        return false;
    }
    enums.push_back( parse_enum_body( stream, std::move( comment ) ) );
    return true;
}

[[nodiscard]] auto parse_field_label( spb::char_stream & stream ) -> proto_field::Label
{
    if( stream.consume( "optional" ) )
    {
        return proto_field::Label::OPTIONAL;
    }
    if( stream.consume( "repeated" ) )
    {
        return proto_field::Label::REPEATED;
    }
    if( stream.consume( "required" ) )
    {
        return proto_field::Label::NONE;
    }

    return proto_field::Label::OPTIONAL;
    // stream.throw_parse_error( "expecting label" );
}

void parse_field( spb::char_stream & stream, proto_fields & fields, proto_comment && comment )
{
    //- field = label type fieldName "=" fieldNumber [ "[" fieldOptions "]" ] ";"
    //- fieldOptions = fieldOption { ","  fieldOption }
    //- fieldOption = optionName "=" constant
    auto new_field = proto_field{
        .label     = parse_field_label( stream ),
        .type_name = parse_full_ident( stream ),
    };

    new_field.name    = parse_ident( stream );
    new_field.number  = ( consume_or_fail( stream, '=' ),
                         parse_number< decltype( proto_field::number ) >( stream ) );
    new_field.options = parse_field_options( stream );
    new_field.comment = std::move( comment );
    consume_statement_end( stream, new_field.comment );
    parse_options_from_comments( stream, new_field.options, new_field.comment );
    fields.push_back( new_field );
}

//[[nodiscard]] auto parse_extend( spb::char_stream & stream, proto_ast & ) -> bool;
//[[nodiscard]] auto parse_extensions( spb::char_stream & stream, proto_fields & ) -> bool;
//[[nodiscard]] auto parse_oneof( spb::char_stream & stream, proto_ast & ) -> bool;

auto parse_map_key_type( spb::char_stream & stream ) -> std::string_view
{
    //- keyType = "int32" | "int64" | "uint32" | "uint64" | "sint32" | "sint64" | "fixed32" |
    //"fixed64" | "sfixed32" | "sfixed64" | "bool" | "string"
    constexpr auto key_types =
        std::array< std::string_view, 12 >{ { "int32", "int64", "uint32", "uint64", "sint32",
                                              "sint64", "fixed32", "fixed64", "sfixed32",
                                              "sfixed64", "bool", "string" } };
    for( auto key_type : key_types )
    {
        if( stream.consume( key_type ) )
        {
            return key_type;
        }
    }
    stream.throw_parse_error( "expecting map key type" );
}

auto parse_map_body( spb::char_stream & stream, proto_comment && comment ) -> proto_map
{
    //- "map" "<" keyType "," type ">" mapName "=" fieldNumber [ "[" fieldOptions "]" ] ";"

    auto new_map = proto_map{
        .key   = proto_field{ .type_name = parse_map_key_type( stream ) },
        .value = proto_field{ .type_name =
                                  ( consume_or_fail( stream, ',' ), parse_full_ident( stream ) ) },
    };
    new_map.name = ( consume_or_fail( stream, '>' ), parse_ident( stream ) );
    new_map.number =
        ( consume_or_fail( stream, '=' ), parse_number< decltype( proto_map::number ) >( stream ) );
    new_map.options = parse_field_options( stream );
    new_map.comment = std::move( comment );
    consume_statement_end( stream, new_map.comment );
    return new_map;
}

[[nodiscard]] auto parse_map_field( spb::char_stream & stream, proto_maps & maps,
                                    proto_comment && comment ) -> bool
{
    //-  "map" "<"
    if( !stream.consume( "map" ) )
    {
        return false;
    }
    consume_or_fail( stream, '<' );
    maps.push_back( parse_map_body( stream, std::move( comment ) ) );
    return true;
}

void parse_oneof_field( spb::char_stream & stream, proto_fields & fields, proto_comment && comment )
{
    //- oneofField = type fieldName "=" fieldNumber [ "[" fieldOptions "]" ] ";"
    auto new_field = proto_field{ .type_name = parse_full_ident( stream ) };

    new_field.name    = parse_ident( stream );
    new_field.number  = ( consume_or_fail( stream, '=' ),
                         parse_number< decltype( proto_field::number ) >( stream ) );
    new_field.options = parse_field_options( stream );
    new_field.comment = std::move( comment );
    consume_statement_end( stream, new_field.comment );
    fields.push_back( new_field );
}

[[nodiscard]] auto parse_oneof_body( spb::char_stream & stream, proto_comment && oneof_comment )
    -> proto_oneof
{
    //- oneof = "oneof" oneofName "{" { option | oneofField } "}"
    auto new_oneof = proto_oneof{ proto_base{
        .name    = parse_ident( stream ),
        .comment = std::move( oneof_comment ),
    } };
    consume_or_fail( stream, '{' );
    while( !stream.consume( '}' ) )
    {
        auto comment = parse_comment( stream );
        if( stream.consume( '}' ) )
        {
            break;
        }

        if( !parse_option( stream, new_oneof.options, std::move( comment ) ) )
        {
            parse_oneof_field( stream, new_oneof.fields, std::move( comment ) );
        }
    }
    return new_oneof;
}

[[nodiscard]] auto parse_oneof( spb::char_stream & stream, proto_oneofs & oneofs,
                                proto_comment && comment ) -> bool
{
    //- oneof = "oneof" oneofName "{" { option | oneofField } "}"
    if( !stream.consume( "oneof" ) )
    {
        return false;
    }
    oneofs.push_back( parse_oneof_body( stream, std::move( comment ) ) );
    return true;
}

[[nodiscard]] auto parse_message( spb::char_stream & stream, proto_messages & messages,
                                  proto_comment && comment ) -> bool;

void parse_message_body( spb::char_stream & stream, proto_messages & messages,
                         proto_comment && message_comment )
{
    //- messageBody = messageName "{" { field | enum | message | extend | extensions | group |
    // option | oneof | mapField | reserved | emptyStatement } "}"
    auto new_message = proto_message{ proto_base{
        .name    = parse_ident( stream ),
        .comment = std::move( message_comment ),
    } };

    consume_or_fail( stream, '{' );
    parse_options_from_comments( stream, new_message.options, new_message.comment );

    while( !stream.consume( '}' ) )
    {
        auto comment = parse_comment( stream );
        if( stream.consume( '}' ) )
        {
            break;
        }

        if( !parse_empty_statement( stream ) &&
            !parse_enum( stream, new_message.enums, std::move( comment ) ) &&
            !parse_message( stream, new_message.messages, std::move( comment ) ) &&
            //! parse_extend( stream, new_message.extends ) &&
            !parse_extensions( stream, new_message.extensions, std::move( comment ) ) &&
            !parse_oneof( stream, new_message.oneofs, std::move( comment ) ) &&
            !parse_map_field( stream, new_message.maps, std::move( comment ) ) &&
            !parse_reserved( stream, new_message.reserved, std::move( comment ) ) &&
            !parse_option( stream, new_message.options, std::move( comment ) ) )
        {
            parse_field( stream, new_message.fields, std::move( comment ) );
        }
    }
    messages.push_back( new_message );
}

[[nodiscard]] auto parse_message( spb::char_stream & stream, proto_messages & messages,
                                  proto_comment && comment ) -> bool
{
    //- "message" messageName messageBody
    if( !stream.consume( "message" ) )
    {
        return false;
    }

    parse_message_body( stream, messages, std::move( comment ) );
    return true;
}

void parse_top_level_option( spb::char_stream & stream, proto_options & options,
                             proto_comment && comment )
{
    parse_or_throw( parse_option( stream, options, std::move( comment ) ), stream,
                    "expecting option" );
}

void parse_top_level_message( spb::char_stream & stream, proto_messages & messages,
                              proto_comment && comment )
{
    parse_or_throw( parse_message( stream, messages, std::move( comment ) ), stream,
                    "expecting message" );
}

void parse_top_level_enum( spb::char_stream & stream, proto_enums & enums,
                           proto_comment && comment )
{
    parse_or_throw( parse_enum( stream, enums, std::move( comment ) ), stream, "expecting enum" );
}

void parse_top_level( spb::char_stream & stream, proto_file & file, proto_comment && comment )
{
    switch( stream.current_char( ) )
    {
    case '\0':
        return;
    case 's':
        return parse_top_level_syntax_or_service( stream, file, std::move( comment ) );
    case 'i':
        return parse_top_level_import( stream, file.imports, std::move( comment ) );
    case 'p':
        return parse_top_level_package( stream, file.package, std::move( comment ) );
    case 'o':
        return parse_top_level_option( stream, file.options, std::move( comment ) );
    case 'm':
        return parse_top_level_message( stream, file.package.messages, std::move( comment ) );
    case 'e':
        return parse_top_level_enum( stream, file.package.enums, std::move( comment ) );
    case ';':
        return ( void ) parse_empty_statement( stream );

    default:
        return stream.throw_parse_error( "expecting top level definition" );
    }
}

void set_default_options( proto_file & file )
{
    file.options[ option_optional_type ]    = "std::optional<$>";
    file.options[ option_optional_include ] = "<optional>";

    file.options[ option_repeated_type ]    = "std::vector<$>";
    file.options[ option_repeated_include ] = "<vector>";

    file.options[ option_string_type ]    = "std::string";
    file.options[ option_string_include ] = "<string>";

    file.options[ option_bytes_type ]    = "std::vector<$>";
    file.options[ option_bytes_include ] = "<vector>";

    file.options[ option_pointer_type ]    = "std::unique_ptr<$>";
    file.options[ option_pointer_include ] = "<memory>";

    file.options[ option_enum_type ] = "int32";
}

[[nodiscard]] auto parse_proto_file( fs::path file, parsed_files & already_parsed,
                                     std::span< const fs::path > import_paths,
                                     const fs::path & base_dir ) -> proto_file
{
    try
    {
        file = find_file_in_paths( file, import_paths, base_dir );

        auto result = proto_file{
            .path    = file,
            .content = load_file( file ),
        };

        parse_proto_file_content( result );
        already_parsed.insert( file.string( ) );
        result.file_imports =
            parse_all_imports( result, already_parsed, import_paths, file.parent_path( ) );
        resolve_messages( result );
        return result;
    }
    catch( const std::exception & e )
    {
        throw std::runtime_error( file.string( ) + ":" + e.what( ) );
    }
}

}// namespace

void parse_proto_file_content( proto_file & file )
{
    set_default_options( file );

    auto stream = spb::char_stream( file.content );

    while( !stream.empty( ) )
    {
        auto comment = parse_comment( stream );
        parse_options_from_comments( stream, file.options, comment );
        parse_top_level( stream, file, std::move( comment ) );
    }
}

auto parse_proto_file( const fs::path & file, std::span< const fs::path > import_paths,
                       const fs::path & base_dir ) -> proto_file
{
    auto already_parsed = parsed_files( );
    return parse_proto_file( file, already_parsed, import_paths, base_dir );
}

[[nodiscard]] auto cpp_file_name_from_proto( const fs::path & proto_file_path,
                                             std::string_view extension ) -> fs::path
{
    return proto_file_path.stem( ).concat( extension );
}
