/**
 * @file    Column.cpp
 * @ingroup SQLiteCpp
 * @brief   Encapsulation of a Column in a row of the result pointed by the prepared SQLite::Statement.
 *
 * Copyright (c) 2012-2025 Sebastien Rombauts (sebastien.rombauts@gmail.com)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */
#include <3rdparty/SQLiteCpp/include/Column.h>

#include <3rdparty/SQLiteCpp/include/sqlite3.h>

#include <iostream>

namespace SQLite
{

const int INTEGER   = SQLITE_INTEGER;
const int FLOAT     = SQLITE_FLOAT;
const int TEXT      = SQLITE_TEXT;
const int BLOB      = SQLITE_BLOB;
const int Null      = SQLITE_NULL;


// Encapsulation of a Column in a row of the result pointed by the prepared Statement.
Column::Column(const Statement::TStatementPtr& aStmtPtr, int aIndex) :
    mStmtPtr(aStmtPtr),
    mIndex(aIndex)
{
    if (!aStmtPtr)
    {
        throw SQLite::Exception("Statement was destroyed");
    }
}

// Return the named assigned to this result column (potentially aliased)
const char* Column::getName() const noexcept
{
    return sqlite3_column_name(mStmtPtr.get(), mIndex);
}

#ifdef SQLITE_ENABLE_COLUMN_METADATA
// Return the name of the table column that is the origin of this result column
const char* Column::getOriginName() const noexcept
{
    return sqlite3_column_origin_name(mStmtPtr.get(), mIndex);
}
#endif

// Return the integer value of the column specified by its index starting at 0
int32_t Column::getInt() const noexcept
{
    return sqlite3_column_int(mStmtPtr.get(), mIndex);
}

// Return the unsigned integer value of the column specified by its index starting at 0
uint32_t Column::getUInt() const noexcept
{
    return static_cast<unsigned>(getInt64());
}

// Return the 64bits integer value of the column specified by its index starting at 0
int64_t Column::getInt64() const noexcept
{
    return sqlite3_column_int64(mStmtPtr.get(), mIndex);
}

// Return the double value of the column specified by its index starting at 0
double Column::getDouble() const noexcept
{
    return sqlite3_column_double(mStmtPtr.get(), mIndex);
}

// Return a pointer to the text value (NULL terminated string) of the column specified by its index starting at 0
const char* Column::getText(const char* apDefaultValue /* = "" */) const noexcept
{
    auto pText = reinterpret_cast<const char*>(sqlite3_column_text(mStmtPtr.get(), mIndex));
    return (pText ? pText : apDefaultValue);
}

// Return a pointer to the blob value (*not* NULL terminated) of the column specified by its index starting at 0
const void* Column::getBlob() const noexcept
{
    return sqlite3_column_blob(mStmtPtr.get(), mIndex);
}

// Return a std::string to a TEXT or BLOB column
std::string Column::getString() const
{
    // Note: using sqlite3_column_blob and not sqlite3_column_text
    // - no need for sqlite3_column_text to add a \0 on the end, as we're getting the bytes length directly
    //   however, we need to call sqlite3_column_bytes() to ensure correct format. It's a noop on a BLOB
    //   or a TEXT value with the correct encoding (UTF-8). Otherwise it'll do a conversion to TEXT (UTF-8).
    (void)sqlite3_column_bytes(mStmtPtr.get(), mIndex);
    auto data = static_cast<const char *>(sqlite3_column_blob(mStmtPtr.get(), mIndex));

    // SQLite docs: "The safest policy is to invoke… sqlite3_column_blob() followed by sqlite3_column_bytes()"
    // Note: std::string is ok to pass nullptr as first arg, if length is 0
    return std::string(data, sqlite3_column_bytes(mStmtPtr.get(), mIndex));
}

// Return the type of the value of the column
int Column::getType() const noexcept
{
    return sqlite3_column_type(mStmtPtr.get(), mIndex);
}

// Return the number of bytes used by the text value of the column
int Column::getBytes() const noexcept
{
    return sqlite3_column_bytes(mStmtPtr.get(), mIndex);
}

// Standard std::ostream inserter
std::ostream& operator<<(std::ostream& aStream, const Column& aColumn)
{
    aStream.write(aColumn.getText(), aColumn.getBytes());
    return aStream;
}

}  // namespace SQLite
