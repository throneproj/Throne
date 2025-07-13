/***************************************************************************\
* Name        : json dumper                                                 *
* Description : generate C++ src files for json de/serialization            *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#include "dumper.h"
#include "ast/ast.h"
#include "ast/proto-field.h"
#include "ast/proto-file.h"
#include "io/file.h"
#include "parser/parser.h"
#include "template-h.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <map>
#include <set>
#include <spb/json/deserialize.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace std::literals;

namespace
{

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

void dump_prototypes( std::ostream & stream, std::string_view type )
{
    stream << replace( file_json_header_template, "$", type );
}

auto json_name_from_options( const proto_options & options ) -> std::string_view
{
    if( auto p_option = options.find( "json_name" ); p_option != options.end( ) )
    {
        return p_option->second;
    }

    return ""sv;
}

auto convert_to_camelCase( std::string_view input ) -> std::string
{
    auto result = std::string( input );
    if( !result.empty( ) )
    {
        result[ 0 ] = char( std::tolower( result[ 0 ] ) );
    }

    if( input.find( '_' ) == std::string::npos )
    {
        return result;
    }

    auto index = 0U;
    auto up    = false;
    for( auto c : input )
    {
        if( c == '_' )
        {
            up = true;
        }
        else
        {
            result[ index ] = char( ( up && index > 0 ) ? std::toupper( c ) : std::tolower( c ) );
            index += 1;
            up = false;
        }
    }
    result.resize( index );
    return result;
}

auto json_field_name( const proto_base & field ) -> std::string
{
    if( const auto result = json_name_from_options( field.options ); !result.empty( ) )
    {
        return std::string( result );
    }

    return std::string( field.name );
}

auto json_field_name_or_camelCase( const proto_base & field ) -> std::string
{
    if( const auto result = json_name_from_options( field.options ); !result.empty( ) )
    {
        return std::string( result );
    }

    return convert_to_camelCase( field.name );
}

void dump_prototypes( std::ostream & stream, const proto_message & message,
                      std::string_view parent )
{
    const auto message_with_parent = std::string( parent ) + "::" + std::string( message.name );
    dump_prototypes( stream, message_with_parent );
}

void dump_prototypes( std::ostream & stream, const proto_enum & my_enum, std::string_view parent )
{
    const auto enum_with_parent = std::string( parent ) + "::" + std::string( my_enum.name );
    dump_prototypes( stream, enum_with_parent );
}

void dump_prototypes( std::ostream & stream, const proto_enums & enums, std::string_view parent )
{
    for( const auto & my_enum : enums )
    {
        dump_prototypes( stream, my_enum, parent );
    }
}

void dump_prototypes( std::ostream & stream, const proto_messages & messages,
                      std::string_view parent )
{
    for( const auto & message : messages )
    {
        dump_prototypes( stream, message, parent );
    }

    for( const auto & message : messages )
    {
        if( message.messages.empty( ) )
        {
            continue;
        }
        const auto message_with_parent = std::string( parent ) + "::" + std::string( message.name );
        dump_prototypes( stream, message.messages, message_with_parent );
    }

    for( const auto & message : messages )
    {
        if( message.enums.empty( ) )
        {
            continue;
        }
        const auto message_with_parent = std::string( parent ) + "::" + std::string( message.name );
        dump_prototypes( stream, message.enums, message_with_parent );
    }
}

void dump_prototypes( std::ostream & stream, const proto_file & file )
{
    const auto package_name = replace( file.package.name, ".", "::" );
    dump_prototypes( stream, file.package.messages, package_name );
    dump_prototypes( stream, file.package.enums, package_name );
}

void dump_cpp_includes( std::ostream & stream, std::string_view header_file_path )
{
    stream << "#include \"" << header_file_path << "\"\n"
           << "#include <spb/json.hpp>\n"
              "#include <system_error>\n"
              "#include <type_traits>\n\n";
}

void dump_cpp_close_namespace( std::ostream & stream, std::string_view name )
{
    stream << "} // namespace " << name << "\n";
}

void dump_cpp_open_namespace( std::ostream & stream, std::string_view name )
{
    stream << "namespace " << name << "\n{\n";
}

void dump_cpp_serialize_value( std::ostream & stream, const proto_oneof & oneof )
{
    stream << "\t{\n\t\tconst auto index = value." << oneof.name << ".index( );\n";
    stream << "\t\tswitch( index )\n\t\t{\n";
    for( size_t i = 0; i < oneof.fields.size( ); ++i )
    {
        stream << "\t\t\tcase " << i << ":\n\t\t\t\treturn stream.serialize( \""
               << json_field_name( oneof.fields[ i ] ) << "\"sv, std::get< " << i << " >( value."
               << oneof.name << ") );\n";
    }
    stream << "\t\t}\n\t}\n\n";
}

void dump_cpp_serialize_value( std::ostream & stream, const proto_enum & my_enum,
                               std::string_view full_name )
{
    if( my_enum.fields.empty( ) )
    {
        stream << "void serialize_value( detail::ostream &, const " << full_name << " & )\n{\n";
        stream << "\treturn ;\n}\n\n";
        return;
    }

    stream << "void serialize_value( detail::ostream & stream, const " << full_name
           << " & value )\n{\n";
    stream << "\tswitch( value )\n\t{\n";

    std::set< int32_t > numbers_taken;
    for( const auto & field : my_enum.fields )
    {
        if( !numbers_taken.insert( field.number ).second )
        {
            continue;
        }

        stream << "\tcase " << full_name << "::" << field.name
               << ":\n\t\treturn stream.serialize( \"" << field.name << "\"sv);\n";
    }
    stream << "\tdefault:\n\t\tthrow std::system_error( std::make_error_code( "
              "std::errc::invalid_argument ) );\n";
    stream << "\t}\n}\n\n";
}

void dump_cpp_deserialize_value( std::ostream & stream, const proto_enum & my_enum,
                                 std::string_view full_name )
{
    if( my_enum.fields.empty( ) )
    {
        stream << "void deserialize_value( detail::istream &, " << full_name << " & )\n{\n";
        stream << "\n}\n\n";
        return;
    }

    size_t key_size_min = UINT32_MAX;
    size_t key_size_max = 0;

    auto name_map = std::multimap< uint32_t, std::string_view >( );
    for( const auto & field : my_enum.fields )
    {
        name_map.emplace( spb::json::detail::djb2_hash( field.name ), field.name );
        key_size_min = std::min( key_size_min, field.name.size( ) );
        key_size_max = std::max( key_size_max, field.name.size( ) );
    }

    stream << "void deserialize_value( detail::istream & stream, " << full_name
           << " & value )\n{\n";
    stream << "\tauto enum_value = stream.deserialize_string_or_int( " << key_size_min << ", "
           << key_size_max << " );\n";
    stream << "\tstd::visit( detail::overloaded{\n\t\t[&]( std::string_view enum_str )\n\t\t{\n";
    stream << "\t\t\tconst auto enum_hash = djb2_hash( enum_str );\n";
    stream << "\t\t\tswitch( enum_hash )\n\t\t\t{\n";
    auto last_hash = name_map.begin( )->first + 1;
    auto put_break = false;
    for( const auto & [ hash, name ] : name_map )
    {
        if( hash != last_hash )
        {
            last_hash = hash;
            if( put_break )
            {
                stream << "\t\t\t\tbreak ;\n";
            }
            put_break = true;
            stream << "\t\t\tcase detail::djb2_hash( \"" << name << "\"sv ):\n";
        }
        stream << "\t\t\t\tif( enum_str == \"" << name << "\"sv ){\n\t\t\t\t\tvalue = " << full_name
               << "::" << name << ";\n\t\t\t\t\treturn ;\t\t\t\t}\n";
    }
    stream << "\t\t\t\tbreak ;\n";
    stream << "\t\t\t}\n\t\t\tthrow std::system_error( std::make_error_code( "
              "std::errc::invalid_argument ) );\n";
    stream << "\t\t},\n\t\t[&]( int32_t enum_int )\n\t\t{\n\t\t\tswitch( " << full_name
           << "( enum_int ) )\n\t\t\t{\n";
    std::set< int32_t > numbers_taken;
    for( const auto & field : my_enum.fields )
    {
        if( !numbers_taken.insert( field.number ).second )
        {
            continue;
        }

        stream << "\t\t\tcase " << full_name << "::" << field.name << ":\n";
    }
    stream << "\t\t\t\tvalue = " << full_name << "( enum_int );\n\t\t\t\treturn ;\n";
    stream << "\t\t\t}\n\t\t\tthrow std::system_error( std::make_error_code( "
              "std::errc::invalid_argument ) );\n";
    stream << "\t\t}\n\t}, enum_value );\n}\n\n";
}

void dump_cpp_serialize_value( std::ostream & stream, const proto_message & message,
                               std::string_view full_name )
{
    if( message.fields.empty( ) && message.maps.empty( ) && message.oneofs.empty( ) )
    {
        stream << "void serialize_value( detail::ostream & , const " << full_name
               << " & )\n{\n}\n\n";
        return;
    }

    stream << "void serialize_value( detail::ostream & stream, const " << full_name
           << " & value )\n{\n";
    for( const auto & field : message.fields )
    {
        stream << "\tstream.serialize( \"" << json_field_name( field ) << "\"sv, value."
               << field.name << " );\n";
    }
    for( const auto & map : message.maps )
    {
        stream << "\tstream.serialize( \"" << map.name << "\"sv, value." << map.name << " );\n";
    }
    for( const auto & oneof : message.oneofs )
    {
        dump_cpp_serialize_value( stream, oneof );
    }
    stream << "}\n";
}

void dump_cpp_deserialize_value( std::ostream & stream, const proto_message & message,
                                 std::string_view full_name )
{
    if( message.fields.empty( ) && message.maps.empty( ) && message.oneofs.empty( ) )
    {
        stream << "void deserialize_value( detail::istream &, " << full_name << " & )\n{\n";
        stream << "\n}\n\n";
        return;
    }

    //- json deserializer needs to accept both camelCase (parsed_name) and the original field name
    struct one_field
    {
        std::string parsed_name;
        std::string_view name;
        size_t oneof_index = SIZE_MAX;
        std::string_view bitfield;
    };

    size_t key_size_min = UINT32_MAX;
    size_t key_size_max = 0;

    auto name_map = std::multimap< uint32_t, one_field >( );
    for( const auto & field : message.fields )
    {
        key_size_min = std::min( key_size_min, field.name.size( ) );
        key_size_max = std::max( key_size_max, field.name.size( ) );

        const auto field_name = json_field_name_or_camelCase( field );
        name_map.emplace( spb::json::detail::djb2_hash( field_name ),
                          one_field{
                              .parsed_name = field_name,
                              .name        = field.name,
                              .bitfield    = field.bit_field,
                          } );
        if( field_name != field.name )
        {
            key_size_min = std::min( key_size_min, field_name.size( ) );
            key_size_max = std::max( key_size_max, field_name.size( ) );

            name_map.emplace( spb::json::detail::djb2_hash( field.name ),
                              one_field{
                                  .parsed_name = std::string( field.name ),
                                  .name        = field.name,
                                  .bitfield    = field.bit_field,
                              } );
        }
    }
    for( const auto & field : message.maps )
    {
        key_size_min = std::min( key_size_min, field.name.size( ) );
        key_size_max = std::max( key_size_max, field.name.size( ) );

        const auto field_name = json_field_name_or_camelCase( field );
        name_map.emplace( spb::json::detail::djb2_hash( field_name ),
                          one_field{
                              .parsed_name = field_name,
                              .name        = field.name,
                          } );
        if( field_name != field.name )
        {
            key_size_min = std::min( key_size_min, field_name.size( ) );
            key_size_max = std::max( key_size_max, field_name.size( ) );

            name_map.emplace(
                spb::json::detail::djb2_hash( field.name ),
                one_field{ .parsed_name = std::string( field.name ), .name = field.name } );
        }
    }
    for( const auto & oneof : message.oneofs )
    {
        for( size_t i = 0; i < oneof.fields.size( ); ++i )
        {
            key_size_min = std::min( key_size_min, oneof.fields[ i ].name.size( ) );
            key_size_max = std::max( key_size_max, oneof.fields[ i ].name.size( ) );

            const auto field_name = json_field_name_or_camelCase( oneof.fields[ i ] );
            name_map.emplace( spb::json::detail::djb2_hash( field_name ),
                              one_field{
                                  .parsed_name = field_name,
                                  .name        = oneof.name,
                                  .oneof_index = i,
                              } );
            if( field_name != oneof.fields[ i ].name )
            {
                key_size_min = std::min( key_size_min, field_name.size( ) );
                key_size_max = std::max( key_size_max, field_name.size( ) );

                name_map.emplace( spb::json::detail::djb2_hash( oneof.fields[ i ].name ),
                                  one_field{
                                      .parsed_name = std::string( oneof.fields[ i ].name ),
                                      .name        = oneof.name,
                                      .oneof_index = i,
                                  } );
            }
        }
    }

    stream << "void deserialize_value( detail::istream & stream, " << full_name
           << " & value )\n{\n";
    stream << "\tauto key = stream.deserialize_key( " << key_size_min << ", " << key_size_max
           << " );\n";
    stream << "\tswitch( djb2_hash( key ) )\n\t{\n";

    auto last_hash = name_map.begin( )->first + 1;
    auto put_break = false;
    for( const auto & [ hash, field ] : name_map )
    {
        if( hash != last_hash )
        {
            if( put_break )
            {
                stream << "\t\t\t\tbreak;\n";
            }
            put_break = true;
            last_hash = hash;
            stream << "\t\tcase detail::djb2_hash( \"" << field.parsed_name << "\"sv ):\n";
        }
        stream << "\t\t\tif( key == \"" << field.parsed_name << "\"sv )\n\t\t\t{\n";
        if( field.oneof_index == SIZE_MAX )
        {
            if( !field.bitfield.empty( ) )
            {

                stream << "\t\t\t\tvalue." << field.name
                       << " = stream.deserialize_bitfield< decltype( value." << field.name
                       << " ) >( " << field.bitfield << " );\n\t\t\t\treturn ;\n";
            }
            else
            {
                stream << "\t\t\t\treturn stream.deserialize( value." << field.name << " );\n";
            }
        }
        else
        {
            stream << "\t\t\t\treturn stream.deserialize_variant<" << field.oneof_index
                   << ">( value." << field.name << " );\n";
        }
        stream << "\t\t\t}\n";
    }
    stream << "\t\t\tbreak;\n\t}\n\treturn stream.skip_value( );\n}\n";
}

void dump_cpp_enum( std::ostream & stream, const proto_enum & my_enum, std::string_view parent )
{
    const auto full_name = std::string( parent ) + "::" + std::string( my_enum.name );
    dump_cpp_open_namespace( stream, "detail" );
    dump_cpp_serialize_value( stream, my_enum, full_name );
    dump_cpp_deserialize_value( stream, my_enum, full_name );
    dump_cpp_close_namespace( stream, "detail" );
}

void dump_cpp_enums( std::ostream & stream, const proto_enums & enums, std::string_view parent )
{
    for( const auto & my_enum : enums )
    {
        dump_cpp_enum( stream, my_enum, parent );
    }
}

void dump_cpp_messages( std::ostream & stream, const proto_messages & messages,
                        std::string_view parent );

void dump_cpp_message( std::ostream & stream, const proto_message & message,
                       std::string_view parent )
{
    const auto full_name = std::string( parent ) + "::" + std::string( message.name );

    dump_cpp_enums( stream, message.enums, full_name );

    dump_cpp_open_namespace( stream, "detail" );
    dump_cpp_serialize_value( stream, message, full_name );
    dump_cpp_deserialize_value( stream, message, full_name );
    dump_cpp_close_namespace( stream, "detail" );

    dump_cpp_messages( stream, message.messages, full_name );
}

void dump_cpp_messages( std::ostream & stream, const proto_messages & messages,
                        std::string_view parent )
{
    for( const auto & message : messages )
    {
        dump_cpp_message( stream, message, parent );
    }
}

void dump_cpp( std::ostream & stream, const proto_file & file )
{
    const auto str_namespace = "::" + replace( file.package.name, ".", "::" );
    dump_cpp_enums( stream, file.package.enums, str_namespace );
    dump_cpp_messages( stream, file.package.messages, str_namespace );
}

}// namespace

void dump_json_header( const proto_file & file, std::ostream & stream )
{
    dump_cpp_open_namespace( stream, "spb::json::detail" );
    stream << "struct ostream;\nstruct istream;\n";
    dump_prototypes( stream, file );
    dump_cpp_close_namespace( stream, "spb::json::detail" );
}

void dump_json_cpp( const proto_file & file, const std::filesystem::path & header_file,
                    std::ostream & stream )
{
    dump_cpp_includes( stream, header_file.string( ) );
    dump_cpp_open_namespace( stream, "spb::json" );
    dump_cpp( stream, file );
    dump_cpp_close_namespace( stream, "spb::json" );
}