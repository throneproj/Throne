/**
 * @file    Exception.h
 * @ingroup SQLiteCpp
 * @brief   Encapsulation of the error message from SQLite3 on a std::runtime_error.
 *
 * Copyright (c) 2012-2025 Sebastien Rombauts (sebastien.rombauts@gmail.com)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */
#pragma once

#include <3rdparty/SQLiteCpp/include/SQLiteCppExport.h>
#include <stdexcept>
#include <string>

// Forward declaration to avoid inclusion of <sqlite3.h> in a header
struct sqlite3;

namespace SQLite
{

/**
 * @brief Encapsulation of the error message from SQLite3, based on std::runtime_error.
 */
class SQLITECPP_API Exception : public std::runtime_error
{
public:
    /**
     * @brief Encapsulation of the error message from SQLite3, based on std::runtime_error.
     *
     * @param[in] aErrorMessage The string message describing the SQLite error
     * @param[in] ret           Return value from function call that failed.
     */
    Exception(const char* aErrorMessage, int ret);

    Exception(const std::string& aErrorMessage, int ret) :
        Exception(aErrorMessage.c_str(), ret)
    {
    }

    /**
     * @brief Encapsulation of the error message from SQLite3, based on std::runtime_error.
     *
     * @param[in] aErrorMessage The string message describing the SQLite error
     */
    explicit Exception(const char* aErrorMessage) :
        Exception(aErrorMessage, -1) // 0 would be SQLITE_OK, which doesn't make sense
    {
    }
    explicit Exception(const std::string& aErrorMessage) :
        Exception(aErrorMessage.c_str(), -1) // 0 would be SQLITE_OK, which doesn't make sense
    {
    }

   /**
     * @brief Encapsulation of the error message from SQLite3, based on std::runtime_error.
     *
     * @param[in] apSQLite The SQLite object, to obtain detailed error messages from.
     */
    explicit Exception(sqlite3* apSQLite);

    /**
     * @brief Encapsulation of the error message from SQLite3, based on std::runtime_error.
     *
     * @param[in] apSQLite  The SQLite object, to obtain detailed error messages from.
     * @param[in] ret       Return value from function call that failed.
     */
    Exception(sqlite3* apSQLite, int ret);

    /// Return the result code (if any, otherwise -1).
    int getErrorCode() const noexcept
    {
        return mErrcode;
    }

    /// Return the extended numeric result code (if any, otherwise -1).
    int getExtendedErrorCode() const noexcept
    {
        return mExtendedErrcode;
    }

    /// Return a string, solely based on the error code
    const char* getErrorStr() const noexcept;

private:
    int mErrcode;         ///< Error code value
    int mExtendedErrcode; ///< Detailed error code if any
};

}  // namespace SQLite
