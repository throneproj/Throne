/***************************************************************************\
* Name        : template concepts                                           *
* Description : general template concepts used by protobuf de/serializer    *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/
#pragma once

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace spb
{
template < class T >
concept resizable_container = requires( T container ) {
    { container.data( ) } -> std::same_as< typename std::decay_t< T >::value_type * >;
    { container.resize( 1 ) };
    typename std::decay_t< T >::value_type;
    { sizeof( typename std::decay_t< T >::value_type ) == sizeof( char ) };
};

template < class T >
concept size_container = requires( T container ) {
    { container.data( ) };
    { container.size( ) } -> std::convertible_to< std::size_t >;
    typename std::decay_t< T >::value_type;
    { sizeof( typename std::decay_t< T >::value_type ) == sizeof( char ) };
};

namespace detail
{
template < class T >
concept proto_enum = std::is_enum_v< T >;

template < class T >
concept proto_field_int_or_float = std::is_integral_v< T > || std::is_floating_point_v< T >;

template < class T >
concept proto_field_number = proto_enum< T > || proto_field_int_or_float< T >;

template < class T >
concept container = requires( T container ) {
    { container.data( ) } -> std::same_as< typename std::decay_t< T >::value_type * >;
    { container.size( ) } -> std::convertible_to< std::size_t >;
    { container.begin( ) };
    { container.end( ) };
    typename std::decay_t< T >::value_type;
};

template < class T >
concept proto_field_bytes =
    container< T > && std::is_same_v< typename std::decay_t< T >::value_type, std::byte >;

template < class T >
concept proto_field_bytes_resizable = proto_field_bytes< T > && requires( T obj ) {
    { obj.resize( 1 ) };
    { obj.clear( ) };
};

template < class T >
concept proto_field_string =
    container< T > && std::is_same_v< typename std::decay_t< T >::value_type, char >;

template < class T >
concept proto_field_string_resizable = proto_field_string< T > && requires( T obj ) {
    { obj.append( "1", 1 ) };
    { obj.clear( ) };
};

template < class T >
concept proto_label_repeated = requires( T container ) {
    { container.emplace_back( ) };
    { container.begin( ) };
    { container.end( ) };
    { container.clear( ) };
    typename T::value_type;
} && !proto_field_string< T > && !proto_field_bytes< T >;

template < class T >
concept proto_label_optional = requires( T container ) {
    { container.has_value( ) } -> std::convertible_to< bool >;
    { container.reset( ) };
    { *container } -> std::same_as< typename T::value_type & >;
    { container.emplace( typename T::value_type( ) ) } -> std::same_as< typename T::value_type & >;
    typename T::value_type;
};

template < class T >
concept proto_message = std::is_class_v< T > && !proto_field_string< T > &&
    !proto_field_bytes< T > && !proto_label_repeated< T > && !proto_label_optional< T >;

}// namespace detail
}// namespace spb