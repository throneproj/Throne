/***************************************************************************\
* Name        : ast render                                                  *
* Description : resolve types in ast tree                                   *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#include "ast-types.h"
#include "dumper/header.h"
#include "proto-field.h"
#include "proto-file.h"
#include "proto-message.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <parser/char_stream.h>
#include <algorithm>
#include <string_view>

namespace
{
[[nodiscard]] auto is_imported( std::string_view full_type, const proto_files & imports ) -> bool
{
    return std::any_of( imports.begin( ), imports.end( ),
                        [ full_type ]( const auto & import ) -> bool
                        {
                            return full_type.size( ) > import.package.name.size( ) &&
                                full_type[ import.package.name.size( ) ] == '.' &&
                                full_type.starts_with( import.package.name );
                        } );
}

[[nodiscard]] auto is_enum( std::string_view type, const proto_enums & enums ) -> bool
{
    return std::any_of( enums.begin( ), enums.end( ), [ type ]( const auto & enum_field ) -> bool
                        { return type == enum_field.name; } );
}

[[nodiscard]] auto is_sub_message( std::string_view type, const proto_messages & messages ) -> bool
{
    return std::any_of( messages.begin( ), messages.end( ),
                        [ type ]( const auto & message ) -> bool { return type == message.name; } );
}

[[nodiscard]] auto is_resolved_sub_message( std::string_view type, const proto_messages & messages )
    -> bool
{
    return std::any_of( messages.begin( ), messages.end( ), [ type ]( const auto & message ) -> bool
                        { return type == message.name && message.resolved > 0; } );
}

[[nodiscard]] auto is_sub_oneof( std::string_view type, const proto_oneofs & oneofs ) -> bool
{
    return std::any_of( oneofs.begin( ), oneofs.end( ),
                        [ type ]( const auto & oneof ) -> bool { return type == oneof.name; } );
}

struct search_state
{
    enum resolve_mode
    {
        //- only check if type is already defined
        dependencies_only,
        //- for optional fields create an std::unique_ptr< T > type and forward declare T
        //- to break cyclic dependency
        optional_pointers,
    };

    resolve_mode mode        = dependencies_only;
    size_t resolved_messages = 0;
    const proto_files & imports;
    const proto_file & file;
};

/**
 * @brief search ctx to hold relation 'message -> parent_message'
 *        the relation is not stored in proto_message because its not needed until now
 *        and the messages get sorted (moved) later so the parent pointers will be invalid anyways
 *        the parent relation is used for type dependency check and for proper order of structs
 * definition in the generated *.pb.h header file, because C++ needs proper order of type
 * dependencies. The proper order is defined by the value of `.resolved` for every message
 *
 */
struct search_ctx
{
    proto_message & message;
    //- parent message (can be null for top level)
    search_ctx * p_parent;
    search_state & state;
};

/**
 * @brief if the field is self-referencing and can be forward declared
 *        field label must be LABEL_OPTIONAL or LABEL_PTR or LABEL_REPEATED
 *
 * @param field filed
 * @param ctx search context
 * @return true if field can be forward declared
 */
[[nodiscard]] auto is_self( proto_field & field, const search_ctx & ctx ) -> bool
{
    if( field.type == proto_field::Type::MESSAGE && field.type_name == ctx.message.name )
    {
        switch( field.label )
        {
        case proto_field::Label::NONE:
            throw_parse_error( ctx.state.file, field.name,
                               "Field '" + std::string( field.name ) +
                                   "' cannot be self-referencing (make it optional)" );
        case proto_field::Label::OPTIONAL:
            field.label = proto_field::Label::PTR;
            return true;
        case proto_field::Label::REPEATED:
        case proto_field::Label::PTR:
            return true;
        }
    }
    return false;
}

/**
 * @brief if this dependency can be forward declared, do it
 *        field label must be LABEL_REPEATED or LABEL_PTR
 *
 * @param field field
 * @param ctx search context
 * @return true if field can be forward declared
 */
[[nodiscard]] auto is_forwarded( proto_field & field, search_ctx & ctx ) -> bool
{
    if( ctx.p_parent == nullptr )
        return false;

    for( auto & message : ctx.p_parent->message.messages )
    {
        if( field.type_name == message.name )
        {
            switch( field.label )
            {
            case proto_field::Label::NONE:
                return false;
            case proto_field::Label::OPTIONAL:
                switch( ctx.state.mode )
                {
                case search_state::dependencies_only:
                    return false;

                case search_state::optional_pointers:
                    field.label = proto_field::Label::PTR;
                    //
                    //- revert the mode to dependencies_only and try again
                    //- reason is to create as little pointers as possible
                    //
                    ctx.state.mode = search_state::dependencies_only;
                }
                [[fallthrough]];
            case proto_field::Label::REPEATED:
            case proto_field::Label::PTR:
                ctx.p_parent->message.forwards.insert( message.name );
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief if this dependency references the parent message and can be forward declared
 *        field label must be LABEL_OPTIONAL or LABEL_REPEATED or LABEL_PTR
 *
 * @param field field
 * @param ctx search context
 * @return true if field can be forward declared
 */
[[nodiscard]] auto is_parent( proto_field & field, const search_ctx & ctx ) -> bool
{
    if( ctx.p_parent == nullptr )
        return false;

    if( field.type_name == ctx.p_parent->message.name )
    {
        switch( field.label )
        {
        case proto_field::Label::NONE:
            throw_parse_error( ctx.state.file, field.name,
                               "Field '" + std::string( field.name ) +
                                   "' cannot reference parent (make it optional)" );
        case proto_field::Label::OPTIONAL:
            field.label = proto_field::Label::PTR;
            return true;
        case proto_field::Label::REPEATED:
        case proto_field::Label::PTR:
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto is_defined_in_parents( std::string_view type, const search_ctx & ctx ) -> bool
{
    if( ctx.p_parent == nullptr )
        return false;

    const auto & message = ctx.p_parent->message;
    if( is_enum( type, message.enums ) || is_resolved_sub_message( type, message.messages ) ||
        is_sub_oneof( type, message.oneofs ) )
    {
        return true;
    }

    return is_defined_in_parents( type, *ctx.p_parent );
}

[[nodiscard]] auto all_types_are_resolved( const proto_messages & messages ) -> bool
{
    return std::all_of( messages.begin( ), messages.end( ),
                        []( const auto & message ) { return message.resolved > 0; } );
}

void mark_message_as_resolved( search_ctx & ctx )
{
    assert( ctx.message.resolved == 0 );

    ctx.message.resolved = ++ctx.state.resolved_messages;
}

[[nodiscard]] auto resolve_message_field( search_ctx & ctx, proto_field & field ) -> bool
{
    //- check only the first type (before .) and leave the rest for the C++ compiler to check
    //- TODO: check full type name
    const auto type_name = field.type_name.substr( 0, field.type_name.find( '.' ) );

    return is_scalar( field.type ) || is_self( field, ctx ) ||
        is_enum( field.type_name, ctx.message.enums ) ||
        is_sub_message( type_name, ctx.message.messages ) ||
        is_sub_oneof( type_name, ctx.message.oneofs ) || is_parent( field, ctx ) ||
        is_defined_in_parents( type_name, ctx ) ||
        is_imported( field.type_name, ctx.state.imports ) || is_forwarded( field, ctx );
}

[[nodiscard]] auto resolve_field( search_ctx & ctx, const proto_field & field ) -> bool
{
    const auto type_name = field.type_name.substr( 0, field.type_name.find( '.' ) );

    return is_scalar( field.type ) || is_enum( field.type_name, ctx.message.enums ) ||
        is_sub_message( type_name, ctx.message.messages ) ||
        is_defined_in_parents( type_name, ctx ) ||
        is_imported( field.type_name, ctx.state.imports );
}

void resolve_message_fields( search_ctx & ctx )
{
    if( ctx.message.resolved > 0 )
        return;

    for( auto & field : ctx.message.fields )
    {
        if( !resolve_message_field( ctx, field ) )
            return;
    }

    for( const auto & map : ctx.message.maps )
    {
        if( !resolve_field( ctx, map.value ) )
            return;
    }

    for( const auto & oneof : ctx.message.oneofs )
    {
        for( const auto & field : oneof.fields )
        {
            if( !resolve_field( ctx, field ) )
                return;
        }
    }

    if( all_types_are_resolved( ctx.message.messages ) )
        mark_message_as_resolved( ctx );
}

void resolve_message_dependencies( search_ctx & ctx )
{
    if( ctx.message.resolved > 0 )
        return;

    for( auto & message : ctx.message.messages )
    {
        auto sub_ctx = search_ctx{
            .message  = message,
            .p_parent = &ctx,
            .state    = ctx.state,
        };

        resolve_message_dependencies( sub_ctx );
    }
    resolve_message_fields( ctx );
}

[[noreturn]] void dump_unresolved_message( const proto_messages & messages,
                                           const proto_file & file )
{
    for( const auto & message : messages )
    {
        if( message.resolved == 0 )
        {
            throw_parse_error( file, message.name, "type dependency can't be resolved" );
        }
    }
    throw_parse_error( file, file.content, "type dependency can't be resolved" );
}

void sort_messages( proto_messages & messages )
{
    std::sort( messages.begin( ), messages.end( ),
               []( const auto & a, const auto & b ) { return a.resolved < b.resolved; } );

    for( auto & message : messages )
    {
        sort_messages( message.messages );
    }
}

}// namespace

void resolve_messages_order( proto_file & file )
{
    auto state = search_state{
        .mode              = search_state::dependencies_only,
        .resolved_messages = 0,
        .imports           = file.file_imports,
        .file              = file,
    };

    while( !all_types_are_resolved( file.package.messages ) )
    {
        const auto resolved_messages = state.resolved_messages;

        auto top_level_ctx = search_ctx{
            .message  = file.package,
            .p_parent = nullptr,
            .state    = state,
        };

        resolve_message_dependencies( top_level_ctx );

        if( resolved_messages == state.resolved_messages )
        {
            if( state.mode == search_state::dependencies_only )
            {
                //- no messages were resolved using only dependencies?
                //- try to break cyclic dependency by using forward pointers declaration
                state.mode = search_state::optional_pointers;
                continue;
            }

            dump_unresolved_message( file.package.messages, file );
        }
    }

    sort_messages( file.package.messages );
}
