/**
 * @file    Statement.cpp
 * @ingroup SQLiteCpp
 * @brief   A prepared SQLite Statement is a compiled SQL query ready to be executed, pointing to a row of result.
 *
 * Copyright (c) 2012-2025 Sebastien Rombauts (sebastien.rombauts@gmail.com)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */
#include <3rdparty/SQLiteCpp/include/Statement.h>

#include <3rdparty/SQLiteCpp/include/Database.h>
#include <3rdparty/SQLiteCpp/include/Column.h>
#include <3rdparty/SQLiteCpp/include/Assertion.h>
#include <3rdparty/SQLiteCpp/include/Exception.h>

#include <3rdparty/SQLiteCpp/include/sqlite3.h>

// check for if SQLite3 version >= 3.14.0
#if SQLITE_VERSION_NUMBER < 3014000
    #warning "SQLite3 version is less than 3.14.0, so expanded SQL is not available"
    #warning "To use expanded SQL, please upgrade to SQLite3 version 3.14.0 or later"
    #warning "If you want to disable this warning, define SQLITECPP_DISABLE_SQLITE3_EXPANDED_SQL"
    #warning "or use the specific project option in your build system"
    #warning "disabling expanded SQL support"
    #define SQLITECPP_DISABLE_SQLITE3_EXPANDED_SQL
#endif


namespace SQLite
{

Statement::Statement(const Database& aDatabase, const char* apQuery) :
    mQuery(apQuery),
    mpSQLite(aDatabase.getHandle()),
    mpPreparedStatement(prepareStatement()) // prepare the SQL query (needs Database friendship)
{
    mColumnCount = sqlite3_column_count(mpPreparedStatement.get());
}

Statement::Statement(Statement&& aStatement) noexcept :
    mQuery(std::move(aStatement.mQuery)),
    mpSQLite(aStatement.mpSQLite),
    mpPreparedStatement(std::move(aStatement.mpPreparedStatement)),
    mColumnCount(aStatement.mColumnCount),
    mbHasRow(aStatement.mbHasRow),
    mbDone(aStatement.mbDone),
    mColumnNames(std::move(aStatement.mColumnNames))
{
    aStatement.mpSQLite = nullptr;
    aStatement.mColumnCount = 0;
    aStatement.mbHasRow = false;
    aStatement.mbDone = false;
}

// Reset the statement to make it ready for a new execution (see also #clearBindings() below)
void Statement::reset()
{
    const int ret = tryReset();
    check(ret);
}

int Statement::tryReset() noexcept
{
    mbHasRow = false;
    mbDone = false;
    return sqlite3_reset(mpPreparedStatement.get());
}

// Clears away all the bindings of a prepared statement (can be associated with #reset() above).
void Statement::clearBindings()
{
    const int ret = sqlite3_clear_bindings(getPreparedStatement());
    check(ret);
}

int Statement::getIndex(const char * const apName) const
{
    return sqlite3_bind_parameter_index(getPreparedStatement(), apName);
}

// Bind an 32bits int value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
void Statement::bind(const int aIndex, const int32_t aValue)
{
    const int ret = sqlite3_bind_int(getPreparedStatement(), aIndex, aValue);
    check(ret);
}

// Bind a 32bits unsigned int value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
void Statement::bind(const int aIndex, const uint32_t aValue)
{
    const int ret = sqlite3_bind_int64(getPreparedStatement(), aIndex, aValue);
    check(ret);
}

// Bind a 64bits int value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
void Statement::bind(const int aIndex, const int64_t aValue)
{
    const int ret = sqlite3_bind_int64(getPreparedStatement(), aIndex, aValue);
    check(ret);
}

// Bind a double (64bits float) value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
void Statement::bind(const int aIndex, const double aValue)
{
    const int ret = sqlite3_bind_double(getPreparedStatement(), aIndex, aValue);
    check(ret);
}

// Bind a string value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
void Statement::bind(const int aIndex, const std::string& aValue)
{
    const int ret = sqlite3_bind_text(getPreparedStatement(), aIndex, aValue.c_str(),
                                      static_cast<int>(aValue.size()), SQLITE_TRANSIENT);
    check(ret);
}

// Bind a text value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
void Statement::bind(const int aIndex, const char* apValue)
{
    const int ret = sqlite3_bind_text(getPreparedStatement(), aIndex, apValue, -1, SQLITE_TRANSIENT);
    check(ret);
}

// Bind a binary blob value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
void Statement::bind(const int aIndex, const void* apValue, const int aSize)
{
    const int ret = sqlite3_bind_blob(getPreparedStatement(), aIndex, apValue, aSize, SQLITE_TRANSIENT);
    check(ret);
}

// Bind a string value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
void Statement::bindNoCopy(const int aIndex, const std::string& aValue)
{
    const int ret = sqlite3_bind_text(getPreparedStatement(), aIndex, aValue.c_str(),
                                      static_cast<int>(aValue.size()), SQLITE_STATIC);
    check(ret);
}

// Bind a text value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
void Statement::bindNoCopy(const int aIndex, const char* apValue)
{
    const int ret = sqlite3_bind_text(getPreparedStatement(), aIndex, apValue, -1, SQLITE_STATIC);
    check(ret);
}

// Bind a binary blob value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
void Statement::bindNoCopy(const int aIndex, const void* apValue, const int aSize)
{
    const int ret = sqlite3_bind_blob(getPreparedStatement(), aIndex, apValue, aSize, SQLITE_STATIC);
    check(ret);
}

// Bind a NULL value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
void Statement::bind(const int aIndex)
{
    const int ret = sqlite3_bind_null(getPreparedStatement(), aIndex);
    check(ret);
}


// Execute a step of the query to fetch one row of results
bool Statement::executeStep()
{
    const int ret = tryExecuteStep();
    if ((SQLITE_ROW != ret) && (SQLITE_DONE != ret)) // on row or no (more) row ready, else it's a problem
    {
        if (ret == sqlite3_errcode(mpSQLite))
        {
            throw SQLite::Exception(mpSQLite, ret);
        }
        else
        {
            throw SQLite::Exception("Statement needs to be reseted", ret);
        }
    }

    return mbHasRow; // true only if one row is accessible by getColumn(N)
}

// Execute a one-step query with no expected result, and return the number of changes.
int Statement::exec()
{
    const int ret = tryExecuteStep();
    if (SQLITE_DONE != ret) // the statement has finished executing successfully
    {
        if (SQLITE_ROW == ret)
        {
            throw SQLite::Exception("exec() does not expect results. Use executeStep.");
        }
        else if (ret == sqlite3_errcode(mpSQLite))
        {
            throw SQLite::Exception(mpSQLite, ret);
        }
        else
        {
            throw SQLite::Exception("Statement needs to be reseted", ret);
        }
    }

    // Return the number of rows modified by those SQL statements (INSERT, UPDATE or DELETE)
    return sqlite3_changes(mpSQLite);
}

int Statement::tryExecuteStep() noexcept
{
    if (mbDone)
    {
        return SQLITE_MISUSE; // Statement needs to be reseted !
    }

    const int ret = sqlite3_step(mpPreparedStatement.get());
    if (SQLITE_ROW == ret) // one row is ready : call getColumn(N) to access it
    {
        mbHasRow = true;
    }
    else
    {
        mbHasRow = false;
        mbDone = SQLITE_DONE == ret; // check if the query has finished executing
    }
    return ret;
}


// Return a copy of the column data specified by its index starting at 0
// (use the Column copy-constructor)
Column Statement::getColumn(const int aIndex) const
{
    checkRow();
    checkIndex(aIndex);

    // Share the Statement Object handle with the new Column created
    return Column(mpPreparedStatement, aIndex);
}

// Return a copy of the column data specified by its column name starting at 0
// (use the Column copy-constructor)
Column Statement::getColumn(const char* apName) const
{
    checkRow();
    const int index = getColumnIndex(apName);

    // Share the Statement Object handle with the new Column created
    return Column(mpPreparedStatement, index);
}

// Test if the column is NULL
bool Statement::isColumnNull(const int aIndex) const
{
    checkRow();
    checkIndex(aIndex);
    return (SQLITE_NULL == sqlite3_column_type(getPreparedStatement(), aIndex));
}

bool Statement::isColumnNull(const char* apName) const
{
    checkRow();
    const int index = getColumnIndex(apName);
    return (SQLITE_NULL == sqlite3_column_type(getPreparedStatement(), index));
}

// Return the named assigned to the specified result column (potentially aliased)
const char* Statement::getColumnName(const int aIndex) const
{
    checkIndex(aIndex);
    return sqlite3_column_name(getPreparedStatement(), aIndex);
}

#ifdef SQLITE_ENABLE_COLUMN_METADATA
// Return the named assigned to the specified result column (potentially aliased)
const char* Statement::getColumnOriginName(const int aIndex) const
{
    checkIndex(aIndex);
    return sqlite3_column_origin_name(getPreparedStatement(), aIndex);
}
#endif

// Return the index of the specified (potentially aliased) column name
int Statement::getColumnIndex(const char* apName) const
{
    // Build the map of column index by name on first call
    if (mColumnNames.empty())
    {
        for (int i = 0; i < mColumnCount; ++i)
        {
            const char* pName = sqlite3_column_name(getPreparedStatement(), i);
            mColumnNames[pName] = i;
        }
    }

    const auto iIndex = mColumnNames.find(apName);
    if (iIndex == mColumnNames.end())
    {
        throw SQLite::Exception("Unknown column name.");
    }

    return iIndex->second;
}

const char* Statement::getColumnDeclaredType(const int aIndex) const
{
    checkIndex(aIndex);
    const char * result = sqlite3_column_decltype(getPreparedStatement(), aIndex);
    if (!result)
    {
        throw SQLite::Exception("Could not determine declared column type.");
    }
    else
    {
        return result;
    }
}

// Get number of rows modified by last INSERT, UPDATE or DELETE statement (not DROP table).
int Statement::getChanges() const noexcept
{
    return sqlite3_changes(mpSQLite);
}

int Statement::getBindParameterCount() const noexcept
{
    return sqlite3_bind_parameter_count(mpPreparedStatement.get());
}

// Return the numeric result code for the most recent failed API call (if any).
int Statement::getErrorCode() const noexcept
{
    return sqlite3_errcode(mpSQLite);
}

// Return the extended numeric result code for the most recent failed API call (if any).
int Statement::getExtendedErrorCode() const noexcept
{
    return sqlite3_extended_errcode(mpSQLite);
}

// Return UTF-8 encoded English language explanation of the most recent failed API call (if any).
const char* Statement::getErrorMsg() const noexcept
{
    return sqlite3_errmsg(mpSQLite);
}


// Return a UTF-8 string containing the SQL text of prepared statement with bound parameters expanded.
std::string Statement::getExpandedSQL() const {
    #ifdef SQLITECPP_DISABLE_SQLITE3_EXPANDED_SQL
    throw SQLite::Exception("this version of SQLiteCpp does not support expanded SQL");
    #else
    char* expanded = sqlite3_expanded_sql(getPreparedStatement());
    std::string expandedString(expanded);
    sqlite3_free(expanded);
    return expandedString;
    #endif
}


// Prepare SQLite statement object and return shared pointer to this object
Statement::TStatementPtr Statement::prepareStatement()
{
    sqlite3_stmt* statement;
    const int ret = sqlite3_prepare_v2(mpSQLite, mQuery.c_str(), static_cast<int>(mQuery.size()), &statement, nullptr);
    if (SQLITE_OK != ret)
    {
        throw SQLite::Exception(mpSQLite, ret);
    }
    return Statement::TStatementPtr(statement, [](sqlite3_stmt* stmt)
        {
            sqlite3_finalize(stmt);
        });
}

// Return prepered statement object or throw
sqlite3_stmt* Statement::getPreparedStatement() const
{
    sqlite3_stmt* ret = mpPreparedStatement.get();
    if (ret)
    {
        return ret;
    }
    throw SQLite::Exception("Statement was not prepared.");
}

}  // namespace SQLite
