/***************************************************************************\
* Name        : ast render                                                  *
* Description : resolve types in ast tree                                   *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#include "ast/proto-oneof.h"

#include "../dumper/header.h"
#include "../parser/options.h"
#include "proto-field.h"
#include <algorithm>
#include "proto-file.h"
#include "proto-message.h"
#include "spb/pb/wire-types.h"
#include <array>
#include <string_view>
#include <utility>

namespace
{
using namespace std::literals;

struct search_ctx
{
    //- `file` containing `message`
    const proto_file & file;
    //- `message` containing `field`
    const proto_message & message;
    //- parent message (null for top level)
    const search_ctx * p_parent = nullptr;
};

using scalar_encoder = spb::pb::detail::scalar_encoder;

[[nodiscard]] auto type_parts( std::string_view type_name ) -> size_t
{
    return size_t( std::count( type_name.cbegin( ), type_name.cend( ), '.' ) );
}

[[nodiscard]] auto is_last_part( const proto_field & field, size_t type_part ) -> bool
{
    return type_part == type_parts( field.type_name );
}

[[nodiscard]] auto get_type_part( const proto_field & field, size_t type_part ) -> std::string_view
{
    auto type_name = field.type_name;
    for( ; type_part > 0; type_part-- )
    {
        const auto dot_index = type_name.find( '.' );
        if( dot_index == type_name.npos )
            return { };

        type_name.remove_prefix( dot_index + 1 );
    }

    return type_name.substr( 0, type_name.find( '.' ) );
}

[[nodiscard]] auto resolve_type( const search_ctx & self, const proto_field & field,
                                 size_t type_part )
    -> std::pair< proto_field::Type, proto_field::BitType >;

[[nodiscard]] auto remove_bitfield( std::string_view type ) -> std::string_view
{
    return type.substr( 0, type.find( ':' ) );
}

[[nodiscard]] auto convertible_types( proto_field::Type from, proto_field::BitType to ) -> bool
{
    static constexpr auto type_map =
        std::array< std::pair< proto_field::Type, std::array< proto_field::BitType, 4 > >, 15 >{ {
            { proto_field::Type::INT32,
              { { proto_field::BitType::INT8, proto_field::BitType::INT16,
                  proto_field::BitType::INT32 } } },
            { proto_field::Type::INT64,
              { { proto_field::BitType::INT8, proto_field::BitType::INT16,
                  proto_field::BitType::INT32, proto_field::BitType::INT64 } } },
            { proto_field::Type::SINT32,
              { { proto_field::BitType::INT8, proto_field::BitType::INT16,
                  proto_field::BitType::INT32 } } },
            { proto_field::Type::SINT64,
              { { proto_field::BitType::INT8, proto_field::BitType::INT16,
                  proto_field::BitType::INT32, proto_field::BitType::INT64 } } },
            { proto_field::Type::UINT32,
              { { proto_field::BitType::UINT8, proto_field::BitType::UINT16,
                  proto_field::BitType::UINT32 } } },
            { proto_field::Type::UINT64,
              { { proto_field::BitType::UINT8, proto_field::BitType::UINT16,
                  proto_field::BitType::UINT32, proto_field::BitType::UINT64 } } },
            { proto_field::Type::FIXED32,
              { { proto_field::BitType::UINT8, proto_field::BitType::UINT16,
                  proto_field::BitType::UINT32 } } },
            { proto_field::Type::FIXED64,
              { { proto_field::BitType::UINT8, proto_field::BitType::UINT16,
                  proto_field::BitType::UINT32, proto_field::BitType::UINT64 } } },
            { proto_field::Type::SFIXED32,
              { { proto_field::BitType::INT8, proto_field::BitType::INT16,
                  proto_field::BitType::INT32 } } },
            { proto_field::Type::SFIXED64,
              { { proto_field::BitType::INT8, proto_field::BitType::INT16,
                  proto_field::BitType::INT32, proto_field::BitType::INT64 } } },
        } };

    for( auto [ proto_type, types ] : type_map )
    {
        if( from == proto_type )
            return std::find( types.begin( ), types.end( ), to ) != types.end( );
    }
    return false;
}

[[nodiscard]] auto get_scalar_bit_type( std::string_view type_name ) -> proto_field::BitType
{
    struct type_table
    {
        std::string_view name;
        proto_field::BitType type;
    };

    static constexpr auto scalar_types = std::array< type_table, 8 >{ {
        { "int8"sv, proto_field::BitType::INT8 },
        { "int16"sv, proto_field::BitType::INT16 },
        { "int32"sv, proto_field::BitType::INT32 },
        { "int64"sv, proto_field::BitType::INT64 },
        { "uint8"sv, proto_field::BitType::UINT8 },
        { "uint16"sv, proto_field::BitType::UINT16 },
        { "uint32"sv, proto_field::BitType::UINT32 },
        { "uint64"sv, proto_field::BitType::UINT64 },
    } };

    for( const auto item : scalar_types )
    {
        if( item.name == type_name )
            return item.type;
    }

    return proto_field::BitType::NONE;
}

[[nodiscard]] auto get_scalar_proto_type( std::string_view type_name ) -> proto_field::Type
{
    struct type_table
    {
        std::string_view name;
        proto_field::Type type;
    };

    static constexpr auto scalar_types = std::array< type_table, 15 >{ {
        { "bool"sv, proto_field::Type::BOOL },
        { "bytes"sv, proto_field::Type::BYTES },
        { "double"sv, proto_field::Type::DOUBLE },
        { "float"sv, proto_field::Type::FLOAT },
        { "int32"sv, proto_field::Type::INT32 },
        { "int64"sv, proto_field::Type::INT64 },
        { "uint32"sv, proto_field::Type::UINT32 },
        { "uint64"sv, proto_field::Type::UINT64 },
        { "sint32"sv, proto_field::Type::SINT32 },
        { "sint64"sv, proto_field::Type::SINT64 },
        { "fixed32"sv, proto_field::Type::FIXED32 },
        { "fixed64"sv, proto_field::Type::FIXED64 },
        { "sfixed32"sv, proto_field::Type::SFIXED32 },
        { "sfixed64"sv, proto_field::Type::SFIXED64 },
        { "string"sv, proto_field::Type::STRING },
    } };

    for( const auto item : scalar_types )
    {
        if( item.name == type_name )
            return item.type;
    }

    return proto_field::Type::NONE;
}

[[nodiscard]] auto get_field_type( const proto_file & file, const proto_field & field )
    -> std::pair< proto_field::Type, proto_field::BitType >
{
    const auto type = get_scalar_proto_type( field.type_name );
    if( type == proto_field::Type::NONE )
        return { };

    if( const auto p_name = field.options.find( option_field_type );
        p_name != field.options.end( ) )
    {
        const auto field_type = get_scalar_bit_type( remove_bitfield( p_name->second ) );
        if( !convertible_types( type, field_type ) )
        {
            throw_parse_error( file, p_name->second,
                               std::string( "incompatible int type: " ) +
                                   std::string( field.type_name ) + " and " +
                                   std::string( p_name->second ) );
        }
        return { type, field_type };
    }

    return { type, proto_field::BitType::NONE };
}

[[nodiscard]] auto get_scalar_type( const proto_file & file, const proto_field & field,
                                    size_t type_part )
    -> std::pair< proto_field::Type, proto_field::BitType >
{
    if( type_part != 0 )
        return { };

    return get_field_type( file, field );
}

[[nodiscard]] auto get_sub_message( const proto_message & message, const proto_field & field,
                                    size_t type_part ) -> const proto_message *
{
    const auto type_name = get_type_part( field, type_part );
    const auto index     = std::find_if( message.messages.begin( ), message.messages.end( ),
                                         [ type_name ]( const auto & sub_message ) -> bool
                                         { return type_name == sub_message.name; } );
    return ( index != message.messages.end( ) ) ? &*index : nullptr;
}

[[nodiscard]] auto resolve_enum( const proto_message & message, const proto_field & field,
                                 size_t type_part ) -> proto_field::Type
{
    if( !is_last_part( field, type_part ) )
        return proto_field::Type::NONE;

    const auto type_name = get_type_part( field, type_part );

    return std::any_of( message.enums.begin( ), message.enums.end( ),
                        [ type_name ]( const auto & enum_ ) -> bool
                        { return type_name == enum_.name; } )
        ? proto_field::Type::ENUM
        : proto_field::Type::NONE;
}

[[nodiscard]] auto resolve_from_message( const proto_message & message, const proto_field & field,
                                         size_t type_part ) -> proto_field::Type
{
    if( const auto type = resolve_enum( message, field, type_part ); type )
        return type;

    if( const auto * sub_message = get_sub_message( message, field, type_part ); sub_message )
    {
        if( is_last_part( field, type_part ) )
            return proto_field::Type::MESSAGE;

        return resolve_from_message( *sub_message, field, type_part + 1 );
    }

    return proto_field::Type::NONE;
}

[[nodiscard]] auto resolve_from_parent( const search_ctx & self, const proto_field & field,
                                        size_t type_part ) -> proto_field::Type
{
    if( !self.p_parent || type_part > 0 )
        return proto_field::Type::NONE;

    return resolve_type( *self.p_parent, field, type_part ).first;
}

[[nodiscard]] auto resolve_from_import( const proto_file & import, const proto_field & field )
    -> proto_field::Type
{
    if( field.type_name.size( ) > import.package.name.size( ) &&
        field.type_name[ import.package.name.size( ) ] == '.' &&
        field.type_name.starts_with( import.package.name ) )
    {
        return resolve_from_message( import.package, field, type_parts( import.package.name ) + 1 );
    }

    return proto_field::Type::NONE;
}

[[nodiscard]] auto resolve_from_imports( const search_ctx & self, const proto_field & field,
                                         size_t type_part ) -> proto_field::Type
{
    if( type_part > 0 )
        return proto_field::Type::NONE;

    for( const auto & import : self.file.file_imports )
    {
        if( const auto type = resolve_from_import( import, field ); type )
            return type;
    }

    return proto_field::Type::NONE;
}

auto resolve_type( const search_ctx & self, const proto_field & field, size_t type_part )
    -> std::pair< proto_field::Type, proto_field::BitType >
{
    if( const auto type = get_scalar_type( self.file, field, type_part ); type.first )
        return type;

    if( const auto type = resolve_from_message( self.message, field, type_part ); type )
        return { type, proto_field::BitType::NONE };

    if( const auto type = resolve_from_parent( self, field, type_part ); type )
        return { type, proto_field::BitType::NONE };

    if( const auto type = resolve_from_imports( self, field, type_part ); type )
        return { type, proto_field::BitType::NONE };

    throw_parse_error( self.file, field.type_name, "type can't be resolved" );
}

void resolve_types( const search_ctx & self, proto_field & field )
{
    const auto [ type, bit_type ] = resolve_type( self, field, 0 );

    field.type     = type;
    field.bit_type = bit_type;
}

void resolve_types( const search_ctx & self, proto_map & map )
{
    resolve_types( self, map.key );
    resolve_types( self, map.value );
}

void resolve_types( const search_ctx & self, proto_oneof & oneof )
{
    for( auto & field : oneof.fields )
    {
        resolve_types( self, field );
    }
}

void resolve_types( const search_ctx & parent, proto_message & message )
{
    auto ctx = search_ctx{
        .file     = parent.file,
        .message  = message,
        .p_parent = &parent,
    };

    for( auto & field : message.fields )
    {
        resolve_types( ctx, field );
    }

    for( auto & map : message.maps )
    {
        resolve_types( ctx, map );
    }

    for( auto & oneof : message.oneofs )
    {
        resolve_types( ctx, oneof );
    }

    for( auto & sub_message : message.messages )
    {
        resolve_types( ctx, sub_message );
    }
}
}// namespace

auto is_packed_array( const proto_file & file, const proto_field & field ) -> bool
{
    if( field.label != proto_field::Label::REPEATED )
        return { };

    if( file.syntax.version < 3 )
    {
        const auto p_packed = field.options.find( "packed" );
        return p_packed != field.options.end( ) && p_packed->second == "true";
    }
    else
    {
        const auto p_packed = field.options.find( "packed" );
        return p_packed == field.options.end( ) || p_packed->second != "false";
    }
}

auto is_scalar( const proto_field::Type & type ) -> bool
{
    switch( type )
    {
    case proto_field::Type::NONE:
    case proto_field::Type::MESSAGE:
    case proto_field::Type::ENUM:
        return false;

    case proto_field::Type::BYTES:
    case proto_field::Type::STRING:
    case proto_field::Type::BOOL:
    case proto_field::Type::INT32:
    case proto_field::Type::UINT32:
    case proto_field::Type::INT64:
    case proto_field::Type::UINT64:
    case proto_field::Type::SINT32:
    case proto_field::Type::SINT64:
    case proto_field::Type::FLOAT:
    case proto_field::Type::FIXED32:
    case proto_field::Type::SFIXED32:
    case proto_field::Type::DOUBLE:
    case proto_field::Type::FIXED64:
    case proto_field::Type::SFIXED64:
        return true;
    }

    return false;
}

void resolve_types( proto_file & file )
{
    auto ctx = search_ctx{
        .file     = file,
        .message  = file.package,
        .p_parent = nullptr,
    };
    resolve_types( ctx, file.package );
}
