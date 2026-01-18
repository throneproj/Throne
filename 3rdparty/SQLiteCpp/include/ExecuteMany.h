/**
 * @file    ExecuteMany.h
 * @ingroup SQLiteCpp
 * @brief   Convenience function to execute a Statement with multiple Parameter sets
 *
 * Copyright (c) 2019 Maximilian Bachmann (contact@maxbachmann.de)
 * Copyright (c) 2019-2025 Sebastien Rombauts (sebastien.rombauts@gmail.com)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */
#pragma once

#if (__cplusplus >= 201402L) || ( defined(_MSC_VER) && (_MSC_VER >= 1900) ) // c++14: Visual Studio 2015

#include <3rdparty/SQLiteCpp/include/Statement.h>
#include <3rdparty/SQLiteCpp/include/VariadicBind.h>

/// @cond
#include <tuple>
#include <utility>
#include <initializer_list>

namespace SQLite
{

/// @endcond

/**
 * \brief Convenience function to execute a Statement with multiple Parameter sets once for each parameter set given.
 *
 *
 * This feature requires a c++14 capable compiler.
 *
 * \code{.cpp}
 * execute_many(db, "INSERT INTO test VALUES (?, ?)",
 *   1,
 *   std::make_tuple(2),
 *   std::make_tuple(3, "three")
 * );
 * \endcode
 * @param aDatabase Database to use
 * @param apQuery   Query to use with all parameter sets
 * @param aArg      first tuple with parameters
 * @param aParams   the following tuples with parameters
 */
template <typename Arg, typename... Types>
void execute_many(Database& aDatabase, const char* apQuery, Arg&& aArg, Types&&... aParams)
{
    SQLite::Statement query(aDatabase, apQuery);
    bind_exec(query, std::forward<Arg>(aArg));
    (void)std::initializer_list<int>
    {
        ((void)reset_bind_exec(query, std::forward<Types>(aParams)), 0)...
    };
}

/**
 * \brief Convenience function to reset a statement and call bind_exec to 
 * bind new values to the statement and execute it
 *
 * This feature requires a c++14 capable compiler.
 *
 * @param apQuery   Query to use
 * @param aTuple    Tuple to bind
 */
template <typename TupleT>
void reset_bind_exec(Statement& apQuery, TupleT&& aTuple)
{
    apQuery.reset();
    bind_exec(apQuery, std::forward<TupleT>(aTuple));
}

/**
 * \brief Convenience function to bind values a the statement and execute it
 *
 * This feature requires a c++14 capable compiler.
 *
 * @param apQuery   Query to use
 * @param aTuple    Tuple to bind
 */
template <typename TupleT>
void bind_exec(Statement& apQuery, TupleT&& aTuple)
{
    SQLite::bind(apQuery, std::forward<TupleT>(aTuple));
    while (apQuery.executeStep()) {}
}

}  // namespace SQLite

#endif // c++14
