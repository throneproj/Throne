/**
 * @file    Savepoint.h
 * @ingroup SQLiteCpp
 * @brief   A Savepoint is a way to group multiple SQL statements into an atomic
 * secured operation. Similar to a transaction while allowing child savepoints.
 *
 * Copyright (c) 2020 Kelvin Hammond (hammond.kelvin@gmail.com)
 * Copyright (c) 2020-2025 Sebastien Rombauts (sebastien.rombauts@gmail.com)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt or
 * copy at http://opensource.org/licenses/MIT)
 */
#pragma once

#include <3rdparty/SQLiteCpp/include/SQLiteCppExport.h>
#include <3rdparty/SQLiteCpp/include/Exception.h>

namespace SQLite
{

// Forward declaration
class Database;

/**
 * @brief RAII encapsulation of a SQLite Savepoint.
 *
 * SAVEPOINTs are a method of creating Transactions, similar to BEGIN and COMMIT,
 * except that the SAVEPOINT and RELEASE commands are named and may be nested..
 *
 * Resource Acquisition Is Initialization (RAII) means that the Savepoint
 * begins in the constructor and is rolled back in the destructor (unless committed before), so that there is
 * no need to worry about memory management or the validity of the underlying SQLite Connection.
 *
 * This method also offers big performances improvements compared to
 * individually executed statements.
 *
 * Caveats:
 *
 * 1) Calling COMMIT or committing a parent transaction or RELEASE on a parent
 * savepoint will cause this savepoint to be released.
 *
 * 2) Calling ROLLBACK TO or rolling back a parent savepoint will cause this
 * savepoint to be rolled back.
 *
 * 3) This savepoint is not saved to the database until this and all savepoints
 * or transaction in the savepoint stack have been released or committed.
 *
 * See also: https://sqlite.org/lang_savepoint.html
 *
 * Thread-safety: a Savepoint object shall not be shared by multiple threads, because:
 * 1) in the SQLite "Thread Safe" mode, "SQLite can be safely used by multiple threads
 *    provided that no single database connection is used simultaneously in two or more threads."
 * 2) the SQLite "Serialized" mode is not supported by SQLiteC++,
 *    because of the way it shares the underling SQLite precompiled statement
 *    in a custom shared pointer (See the inner class "Statement::Ptr").
 */
class SQLITECPP_API Savepoint
{
public:
    /**
     * @brief Begins the SQLite savepoint
     *
     * @param[in] aDatabase the SQLite Database Connection
     * @param[in] aName the name of the Savepoint
     *
     * Exception is thrown in case of error, then the Savepoint is NOT
     * initiated.
     */
    Savepoint(Database& aDatabase, const std::string& aName);

    // Savepoint is non-copyable
    Savepoint(const Savepoint&) = delete;
    Savepoint& operator=(const Savepoint&) = delete;

    /**
     * @brief Safely rollback the savepoint if it has not been committed.
     */
    ~Savepoint();

    /**
     * @brief Commit and release the savepoint.
     */
    void release();

    /**
     * @brief Rollback to the savepoint, but don't release it.
     */
    void rollbackTo();
    // @deprecated same as rollbackTo();
    void rollback() { rollbackTo(); }

private:
    Database&   mDatabase;          ///< Reference to the SQLite Database Connection
    std::string msName;             ///< Name of the Savepoint
    bool        mbReleased = false; ///< True when release has been called
};

}  // namespace SQLite
