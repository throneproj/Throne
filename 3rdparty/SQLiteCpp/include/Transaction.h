/**
 * @file    Transaction.h
 * @ingroup SQLiteCpp
 * @brief   A Transaction is way to group multiple SQL statements into an atomic secured operation.
 *
 * Copyright (c) 2012-2025 Sebastien Rombauts (sebastien.rombauts@gmail.com)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */
#pragma once

#include <3rdparty/SQLiteCpp/include/SQLiteCppExport.h>
#include <3rdparty/SQLiteCpp/include/Exception.h>

namespace SQLite
{

// Forward declaration
class Database;

/**
 * @brief Transaction behaviors when opening an SQLite transaction.
 * Names correspond directly to the behavior.
 */
enum class TransactionBehavior {
    DEFERRED,
    IMMEDIATE,
    EXCLUSIVE,
};

/**
 * @brief RAII encapsulation of a SQLite Transaction.
 *
 * A Transaction is a way to group multiple SQL statements into an atomic secured operation;
 * either it succeeds, with all the changes committed to the database file,
 * or if it fails, all the changes are rolled back to the initial state.
 *
 * Resource Acquisition Is Initialization (RAII) means that the Transaction
 * begins in the constructor and is rolled back in the destructor (unless committed before), so that there is
 * no need to worry about memory management or the validity of the underlying SQLite Connection.
 *
 * This method also offers big performances improvements compared to individually executed statements.
 *
 * Thread-safety: a Transaction object shall not be shared by multiple threads, because :
 * 1) in the SQLite "Thread Safe" mode, "SQLite can be safely used by multiple threads
 *    provided that no single database connection is used simultaneously in two or more threads."
 * 2) the SQLite "Serialized" mode is not supported by SQLiteC++,
 *    because of the way it shares the underling SQLite precompiled statement
 *    in a custom shared pointer (See the inner class "Statement::Ptr").
 */
class SQLITECPP_API Transaction
{
public:
    /**
     * @brief Begins the SQLite transaction using the default transaction behavior.
     *
     * @param[in] aDatabase the SQLite Database Connection
     *
     * Exception is thrown in case of error, then the Transaction is NOT initiated.
     */
    explicit Transaction(Database& aDatabase);

    /**
     * @brief Begins the SQLite transaction with the specified behavior.
     *
     * @param[in] aDatabase the SQLite Database Connection
     * @param[in] behavior the requested transaction behavior
     *
     * Exception is thrown in case of error, then the Transaction is NOT initiated.
     */
    explicit Transaction(Database& aDatabase, TransactionBehavior behavior);

    // Transaction is non-copyable
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    /**
     * @brief Safely rollback the transaction if it has not been committed.
     */
    ~Transaction();

    /**
     * @brief Commit the transaction.
     */
    void commit();

    /**
     * @brief Rollback the transaction
     */
    void rollback();

private:
    Database&   mDatabase;              ///< Reference to the SQLite Database Connection
    bool        mbCommited = false;     ///< True when commit has been called
};

}  // namespace SQLite
