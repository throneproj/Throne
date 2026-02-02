/**
 * @file    VariadicBind.h
 * @ingroup SQLiteCpp
 * @brief   Convenience function for Statement::bind(...)
 *
 * Copyright (c) 2016 Paul Dreik (github@pauldreik.se)
 * Copyright (c) 2016-2025 Sebastien Rombauts (sebastien.rombauts@gmail.com)
 * Copyright (c) 2019 Maximilian Bachmann (contact@maxbachmann.de)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */
#pragma once

#include <3rdparty/SQLiteCpp/include/Statement.h>

#if (__cplusplus >= 201402L) || ( defined(_MSC_VER) && (_MSC_VER >= 1900) ) // c++14: Visual Studio 2015
#include <tuple>
#endif // c++14

/// @cond
#include <utility>
#include <initializer_list>

namespace SQLite
{
/// @endcond

/**
 * \brief Convenience function for calling Statement::bind(...) once for each argument given.
 *
 * This takes care of incrementing the index between each calls to bind.
 *
 * This feature requires a c++11 capable compiler.
 *
 * \code{.cpp}
 * SQLite::Statement stm("SELECT * FROM MyTable WHERE colA>? && colB=? && colC<?");
 * SQLite::bind(stm,a,b,c);
 * //...is equivalent to
 * stm.bind(1,a);
 * stm.bind(2,b);
 * stm.bind(3,c);
 * \endcode
 * @param query     statement
 * @param args      zero or more args to bind.
 */
template<class ...Args>
void bind(SQLite::Statement& query, const Args& ... args)
{
    int pos = 0;
    (void)std::initializer_list<int>{
        ((void)query.bind(++pos, std::forward<decltype(args)>(args)), 0)...
    };
}

#if (__cplusplus >= 201402L) || ( defined(_MSC_VER) && (_MSC_VER >= 1900) ) // c++14: Visual Studio 2015

/**
 * \brief Convenience function for calling Statement::bind(...) once for each parameter of a tuple,
 * by forwarding them to the variadic template
 *
 * This feature requires a c++14 capable compiler.
 *
 * \code{.cpp}
 * SQLite::Statement stm("SELECT * FROM MyTable WHERE colA>? && colB=? && colC<?");
 * SQLite::bind(stm, std::make_tuple(a, b, c));
 * //...is equivalent to
 * stm.bind(1,a);
 * stm.bind(2,b);
 * stm.bind(3,c);
 * \endcode
 * @param query     statement
 * @param tuple     tuple with values to bind
 */
template <typename ... Types>
void bind(SQLite::Statement& query, const std::tuple<Types...> &tuple)
{
    bind(query, tuple, std::index_sequence_for<Types...>());
}

/**
 * \brief Convenience function for calling Statement::bind(...) once for each parameter of a tuple,
 * by forwarding them to the variadic template. This function is just needed to convert the tuples
 * to parameter packs
 *
 * This feature requires a c++14 capable compiler.
 * 
 * @param query     statement
 * @param tuple     tuple with values to bind
 */
template <typename ... Types, std::size_t ... Indices>
void bind(SQLite::Statement& query, const std::tuple<Types...> &tuple, std::index_sequence<Indices...>)
{
    bind(query, std::get<Indices>(tuple)...);
}
#endif // c++14

} // namespace SQLite
