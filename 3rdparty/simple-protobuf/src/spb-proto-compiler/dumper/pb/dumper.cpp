/***************************************************************************\
* Name        : pb dumper                                                   *
* Description : generate C++ src files for pb de/serialization              *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#include "dumper.h"
#include "ast/ast-types.h"
#include "ast/proto-field.h"
#include "ast/proto-file.h"
#include "template-h.h"
#include <iterator>
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
    stream << replace( file_pb_header_template, "$", type );
}

void dump_prototypes( std::ostream & stream, const proto_message & message,
                      std::string_view parent )
{
    const auto message_with_parent = std::string( parent ) + "::" + std::string( message.name );
    dump_prototypes( stream, message_with_parent );
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
}

void dump_prototypes( std::ostream & stream, const proto_file & file )
{
    const auto package_name = replace( file.package.name, ".", "::" );
    dump_prototypes( stream, file.package.messages, package_name );
}

void dump_cpp_includes( std::ostream & stream, std::string_view header_file_path )
{
    stream << "#include \"" << header_file_path << "\"\n"
           << "#include <spb/pb.hpp>\n"
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

auto encoder_type_str( const proto_file & file, const proto_field & field ) -> std::string
{
    switch( field.type )
    {
    case proto_field::Type::NONE:
    case proto_field::Type::BYTES:
    case proto_field::Type::MESSAGE:
    case proto_field::Type::STRING:
        return { };

    case proto_field::Type::BOOL:
    case proto_field::Type::ENUM:
    case proto_field::Type::INT32:
    case proto_field::Type::UINT32:
    case proto_field::Type::INT64:
    case proto_field::Type::UINT64:
        return is_packed_array( file, field ) ? "scalar_encoder::varint | scalar_encoder::packed"
                                              : "scalar_encoder::varint";

    case proto_field::Type::SINT32:
    case proto_field::Type::SINT64:
        return is_packed_array( file, field ) ? "scalar_encoder::svarint | scalar_encoder::packed"
                                              : "scalar_encoder::svarint";

    case proto_field::Type::FLOAT:
    case proto_field::Type::FIXED32:
    case proto_field::Type::SFIXED32:
        return is_packed_array( file, field ) ? "scalar_encoder::i32 | scalar_encoder::packed"
                                              : "scalar_encoder::i32";

    case proto_field::Type::DOUBLE:
    case proto_field::Type::FIXED64:
    case proto_field::Type::SFIXED64:
        return is_packed_array( file, field ) ? "scalar_encoder::i64 | scalar_encoder::packed"
                                              : "scalar_encoder::i64";
    }
    return { };
}

auto encoder_type( const proto_file & file, const proto_field & field ) -> std::string
{
    const auto encoder = encoder_type_str( file, field );
    if( encoder.empty( ) )
        return { };

    return "_as<" + encoder + ">";
}

auto map_encoder_type( const proto_file & file, const proto_field & key, const proto_field & value )
    -> std::string
{
    const auto key_encoder   = encoder_type_str( file, key );
    const auto value_encoder = encoder_type_str( file, value );

    return "_as< scalar_encoder_combine( " +
        ( key_encoder.empty( ) ? std::string( "{}" ) : key_encoder ) + ", " +
        ( value_encoder.empty( ) ? std::string( "{}" ) : value_encoder ) + " ) >";
}

auto bitfield_encoder_type( const proto_file & file, const proto_field & field ) -> std::string
{
    const auto encoder = encoder_type_str( file, field );
    return "_as< " + encoder + ", decltype( value." + std::string( field.name ) + ") >";
}

void dump_cpp_serialize_value( std::ostream & stream, const proto_file & file,
                               const proto_oneof & oneof )
{
    stream << "\t{\n\t\tconst auto index = value." << oneof.name << ".index( );\n";
    stream << "\t\tswitch( index )\n\t\t{\n";
    for( size_t i = 0; i < oneof.fields.size( ); ++i )
    {
        stream << "\t\t\tcase " << i << ":\n\t\t\t\treturn stream.serialize"
               << encoder_type( file, oneof.fields[ i ] ) << "( " << oneof.fields[ i ].number
               << ", std::get< " << i << " >( value." << oneof.name << ") );\n";
    }
    stream << "\t\t}\n\t}\n\n";
}

void dump_cpp_serialize_value( std::ostream & stream, const proto_file & file,
                               const proto_message & message, std::string_view full_name )
{
    if( message.fields.empty( ) && message.maps.empty( ) && message.oneofs.empty( ) )
    {
        stream << "void serialize( detail::ostream & , const " << full_name << " & )\n{\n}\n\n";
        return;
    }

    stream << "void serialize( detail::ostream & stream, const " << full_name << " & value )\n{\n";
    for( const auto & field : message.fields )
    {
        stream << "\tstream.serialize" << encoder_type( file, field ) << "( " << field.number
               << ", value." << field.name << " );\n";
    }
    for( const auto & map : message.maps )
    {
        stream << "\tstream.serialize" << map_encoder_type( file, map.key, map.value ) << "( "
               << map.number << ", value." << map.name << " );\n";
    }
    for( const auto & oneof : message.oneofs )
    {
        dump_cpp_serialize_value( stream, file, oneof );
    }
    stream << "}\n";
}

void dump_cpp_deserialize_value( std::ostream & stream, const proto_file & file,
                                 const proto_message & message, std::string_view full_name )
{
    if( message.fields.empty( ) && message.maps.empty( ) && message.oneofs.empty( ) )
    {
        stream << "void deserialize_value( detail::istream & stream, " << full_name
               << " &, uint32_t tag )\n{\n";
        stream << "\tstream.skip( tag );\n}\n\n";
        return;
    }

    stream << "void deserialize_value( detail::istream & stream, " << full_name
           << " & value, uint32_t tag )\n{\n";
    stream << "\tswitch( field_from_tag( tag ) )\n\t{\n";

    for( const auto & field : message.fields )
    {

        stream << "\t\tcase " << field.number << ":\n\t\t\t";
        if( !field.bit_field.empty( ) )
        {
            stream << "value." << field.name << " = stream.deserialize_bitfield"
                   << bitfield_encoder_type( file, field ) << "( " << field.bit_field
                   << ", tag );\n";
            stream << "\t\t\treturn;\n";
        }
        else
        {
            stream << "return stream.deserialize" << encoder_type( file, field ) << "( value."
                   << field.name << ", tag );\n";
        }
    }
    for( const auto & map : message.maps )
    {
        stream << "\t\tcase " << map.number << ":\n\t\t\treturn ";
        stream << "\tstream.deserialize" << map_encoder_type( file, map.key, map.value )
               << "( value." << map.name << ", tag );\n";
    }
    for( const auto & oneof : message.oneofs )
    {
        for( size_t i = 0; i < oneof.fields.size( ); ++i )
        {
            stream << "\t\tcase " << oneof.fields[ i ].number << ":\n\t\t\treturn ";
            auto type = encoder_type( file, oneof.fields[ i ] );
            if( type.empty( ) )
            {
                stream << "\tstream.deserialize_variant< " << i << ">( value." << oneof.name
                       << ", tag );\n";
            }
            else
            {
                type.erase( 0, 4 );
                stream << "\tstream.deserialize_variant_as< " << i << ", " << type << "( value."
                       << oneof.name << ", tag );\n";
            }
        }
    }

    stream << "\t\tdefault:\n\t\t\treturn stream.skip( tag );\t\n}\n}\n\n";
}

void dump_cpp_messages( std::ostream & stream, const proto_file & file,
                        const proto_messages & messages, std::string_view parent );

void dump_cpp_message( std::ostream & stream, const proto_file & file,
                       const proto_message & message, std::string_view parent )
{
    const auto full_name = std::string( parent ) + "::" + std::string( message.name );

    dump_cpp_open_namespace( stream, "detail" );
    dump_cpp_serialize_value( stream, file, message, full_name );
    dump_cpp_deserialize_value( stream, file, message, full_name );
    dump_cpp_close_namespace( stream, "detail" );

    dump_cpp_messages( stream, file, message.messages, full_name );
}

void dump_cpp_messages( std::ostream & stream, const proto_file & file,
                        const proto_messages & messages, std::string_view parent )
{
    for( const auto & message : messages )
    {
        dump_cpp_message( stream, file, message, parent );
    }
}

void dump_cpp( std::ostream & stream, const proto_file & file )
{
    const auto str_namespace = "::" + replace( file.package.name, ".", "::" );
    dump_cpp_messages( stream, file, file.package.messages, str_namespace );
}

}// namespace

void dump_pb_header( const proto_file & file, std::ostream & stream )
{
    dump_cpp_open_namespace( stream, "spb::pb::detail" );
    stream << "struct ostream;\nstruct istream;\n";
    dump_prototypes( stream, file );
    dump_cpp_close_namespace( stream, "spb::pb::detail" );
}

void dump_pb_cpp( const proto_file & file, const std::filesystem::path & header_file,
                  std::ostream & stream )
{
    dump_cpp_includes( stream, header_file.string( ) );
    dump_cpp_open_namespace( stream, "spb::pb" );
    dump_cpp( stream, file );
    dump_cpp_close_namespace( stream, "spb::pb" );
}