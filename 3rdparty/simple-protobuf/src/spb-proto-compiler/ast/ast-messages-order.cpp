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
#include <string_view>

namespace
{

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
    const proto_file &file;
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
    proto_message &message;
    //- parent message (is null for top level)
    search_ctx *p_parent;
    search_state &state;
};

[[nodiscard]]
auto type_parts(std::string_view type_name) -> size_t
{
    return size_t(std::count(type_name.cbegin(), type_name.cend(), '.'));
}

[[nodiscard]]
auto is_last_part(const proto_field &field, size_t type_part) -> bool
{
    return type_part == type_parts(field.type_name.proto_name);
}

[[nodiscard]]
auto get_type_part(const proto_field &field, size_t type_part) -> std::string_view
{
    auto type_name = field.type_name.proto_name;
    for (; type_part > 0; type_part--)
    {
        const auto dot_index = type_name.find('.');
        if (dot_index == type_name.npos)
            return {};

        type_name.remove_prefix(dot_index + 1);
    }

    return type_name.substr(0, type_name.find('.'));
}

/**
 * @brief if the field is self-referencing and can be forward declared
 *        field label must be LABEL_OPTIONAL or LABEL_PTR or LABEL_REPEATED
 *
 * @param field filed
 * @param ctx search context
 * @return true if field can be forward declared
 */
[[nodiscard]]
auto is_self(const search_ctx &ctx, proto_field &field, size_t type_part) -> bool
{
    if (field.type != proto_field::Type::MESSAGE)
        return false;

    if (!is_last_part(field, type_part))
        return false;

    if (get_type_part(field, type_part) != ctx.message.name.proto_name)
        return false;

    switch (field.label)
    {
    case proto_field::Label::NONE:
        throw_parse_error(ctx.state.file, field.name.proto_name,
                          "Field '" + std::string(field.name.proto_name) +
                              "' cannot be self-referencing (make it optional)");
    case proto_field::Label::OPTIONAL:
        field.label = proto_field::Label::PTR;
        return true;
    case proto_field::Label::REPEATED:
    case proto_field::Label::PTR:
        return true;
    }

    return false;
}

[[nodiscard]]
auto is_self(const search_ctx &ctx, const proto_field &field, size_t type_part) -> bool
{
    if (field.type != proto_field::Type::MESSAGE)
        return false;

    if (!is_last_part(field, type_part))
        return false;

    if (get_type_part(field, type_part) != ctx.message.name.proto_name)
        return false;

    switch (field.label)
    {
    case proto_field::Label::REPEATED:
    case proto_field::Label::PTR:
        return true;
    default:
        return false;
    }
}

/**
 * @brief if this dependency can be forward declared, do it
 *        field label must be LABEL_REPEATED or LABEL_PTR
 *
 * @param ctx search context
 * @param field field
 * @return true if field can be forward declared
 */
[[nodiscard]]
auto is_forwarded(search_ctx &ctx, proto_field &field, size_t type_part) -> bool
{
    if (ctx.p_parent == nullptr)
        return false;

    if (!is_last_part(field, type_part))
        return false;

    for (const auto &message : ctx.p_parent->message.messages)
    {
        if (get_type_part(field, type_part) != message.name.proto_name)
            continue;

        switch (field.label)
        {
        case proto_field::Label::NONE:
            return false;
        case proto_field::Label::OPTIONAL:
            switch (ctx.state.mode)
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
            ctx.p_parent->message.forwards.insert(message.name.proto_name);
            return true;
        }
    }
    return false;
}

[[nodiscard]]
auto get_sub_message(const proto_message &message, const proto_field &field, size_t type_part)
    -> const proto_message *
{
    const auto type_name = get_type_part(field, type_part);
    const auto index     = std::find_if(
        message.messages.begin(), message.messages.end(), [type_name](const auto &sub_message) -> bool
        { return type_name == sub_message.name.proto_name && sub_message.resolved > 0; });
    return (index != message.messages.end()) ? &*index : nullptr;
}

[[nodiscard]]
auto all_types_are_resolved(const proto_messages &messages) -> bool
{
    return std::all_of(messages.begin(), messages.end(),
                       [](const auto &message) { return message.resolved > 0; });
}

void mark_message_as_resolved(search_ctx &ctx)
{
    assert(ctx.message.resolved == 0);

    ctx.message.resolved = ++ctx.state.resolved_messages;
}

[[nodiscard]]
auto is_enum(const proto_message &message, const proto_field &field, size_t type_part) -> bool
{
    if (!is_last_part(field, type_part))
        return false;

    const auto type_name = get_type_part(field, type_part);

    return std::any_of(message.enums.begin(), message.enums.end(),
                       [type_name](const auto &enum_) -> bool { return type_name == enum_.name.proto_name; });
}

[[nodiscard]]
auto resolve_from_message(const proto_message &message, const proto_field &field, size_t type_part) -> bool
{
    if (const auto type = is_enum(message, field, type_part); type)
        return type;

    if (const auto *sub_message = get_sub_message(message, field, type_part); sub_message)
    {
        if (is_last_part(field, type_part))
            return true;

        return resolve_from_message(*sub_message, field, type_part + 1);
    }

    return false;
}

[[nodiscard]]
auto resolve_field(search_ctx &ctx, proto_field &field, size_t type_part) -> bool;

[[nodiscard]]
auto resolve_field(const search_ctx &ctx, const proto_field &field, size_t type_part) -> bool;

[[nodiscard]]
auto resolve_from_parent(const search_ctx &self, proto_field &field, size_t type_part) -> bool
{
    if (!self.p_parent || type_part > 0)
        return false;

    return resolve_field(*self.p_parent, field, type_part);
}

[[nodiscard]]
auto resolve_from_parent(const search_ctx &self, const proto_field &field, size_t type_part) -> bool
{
    if (!self.p_parent || type_part > 0)
        return false;

    return resolve_field(*self.p_parent, field, type_part);
}

[[nodiscard]]
auto resolve_from_import(const proto_file &import, const proto_field &field) -> bool
{
    if (import.package.name.proto_name.empty())
        return resolve_from_message(import.package, field, 0);

    if (field.type_name.proto_name.size() > import.package.name.proto_name.size() &&
        field.type_name.proto_name[import.package.name.proto_name.size()] == '.' &&
        field.type_name.proto_name.starts_with(import.package.name.proto_name))
    {
        return resolve_from_message(import.package, field, type_parts(import.package.name.proto_name) + 1);
    }

    return false;
}

[[nodiscard]]
auto resolve_from_imports(const search_ctx &self, const proto_field &field, size_t type_part) -> bool
{
    if (type_part > 0)
        return false;

    for (const auto &import : self.state.file.imports)
    {
        if (resolve_from_import(import, field))
            return true;
    }

    return false;
}

[[nodiscard]]
auto resolve_field(const search_ctx &ctx, const proto_field &field, size_t type_part) -> bool
{
    if (is_scalar(field.type) || is_self(ctx, field, type_part))
        return true;

    if (resolve_from_message(ctx.message, field, type_part))
        return true;

    if (resolve_from_parent(ctx, field, type_part))
        return true;

    return resolve_from_imports(ctx, field, type_part);
}

[[nodiscard]]
auto resolve_field(search_ctx &ctx, proto_field &field, size_t type_part) -> bool
{
    if (is_scalar(field.type) || is_self(ctx, field, type_part))
        return true;

    if (resolve_from_message(ctx.message, field, type_part))
        return true;

    if (resolve_from_parent(ctx, field, type_part))
        return true;

    if (resolve_from_imports(ctx, field, type_part))
        return true;

    return is_forwarded(ctx, field, type_part);
}

void resolve_message_fields(search_ctx &ctx)
{
    if (ctx.message.resolved > 0)
        return;

    for (auto &field : ctx.message.fields)
    {
        if (!resolve_field(ctx, field, 0))
            return;
    }

    for (const auto &map : ctx.message.maps)
    {
        if (!resolve_field(ctx, map.value, 0))
            return;
    }

    for (const auto &oneof : ctx.message.oneofs)
    {
        for (const auto &field : oneof.fields)
        {
            if (!resolve_field(ctx, field, 0))
                return;
        }
    }

    if (all_types_are_resolved(ctx.message.messages))
        mark_message_as_resolved(ctx);
}

void resolve_message_dependencies(search_ctx &ctx)
{
    if (ctx.message.resolved > 0)
        return;

    for (auto &message : ctx.message.messages)
    {
        auto sub_ctx = search_ctx{
            .message  = message,
            .p_parent = &ctx,
            .state    = ctx.state,
        };

        resolve_message_dependencies(sub_ctx);
    }
    resolve_message_fields(ctx);
}

[[noreturn]] void dump_unresolved_message(const proto_messages &messages, const proto_file &file)
{
    for (const auto &message : messages)
    {
        if (message.resolved == 0)
            throw_parse_error(file, message.name.proto_name, "type dependency can't be resolved");
    }
    throw_parse_error(file, file.content, "type dependency can't be resolved");
}

void sort_messages(proto_messages &messages)
{
    std::sort(messages.begin(), messages.end(),
              [](const auto &a, const auto &b) { return a.resolved < b.resolved; });

    for (auto &message : messages)
    {
        sort_messages(message.messages);
    }
}

} // namespace

void resolve_messages_order(proto_file &file)
{
    auto state = search_state{
        .mode              = search_state::dependencies_only,
        .resolved_messages = 0,
        .file              = file,
    };

    while (!all_types_are_resolved(file.package.messages))
    {
        const auto resolved_messages = state.resolved_messages;

        auto top_level_ctx = search_ctx{
            .message  = file.package,
            .p_parent = nullptr,
            .state    = state,
        };

        resolve_message_dependencies(top_level_ctx);

        if (resolved_messages == state.resolved_messages)
        {
            if (state.mode == search_state::dependencies_only)
            {
                //- no messages were resolved using only dependencies?
                //- try to break cyclic dependency by using forward pointers declaration
                state.mode = search_state::optional_pointers;
                continue;
            }

            dump_unresolved_message(file.package.messages, file);
        }
    }

    sort_messages(file.package.messages);
}
