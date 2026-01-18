/**
 * @file    Database.cpp
 * @ingroup SQLiteCpp
 * @brief   Management of a SQLite Database Connection.
 *
 * Copyright (c) 2012-2025 Sebastien Rombauts (sebastien.rombauts@gmail.com)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */
#include <3rdparty/SQLiteCpp/include/Database.h>

#include <3rdparty/SQLiteCpp/include/Assertion.h>
#include <3rdparty/SQLiteCpp/include/Backup.h>
#include <3rdparty/SQLiteCpp/include/Exception.h>
#include <3rdparty/SQLiteCpp/include/Statement.h>

#include <3rdparty/SQLiteCpp/include/sqlite3.h>
#include <fstream>
#include <string.h>

#ifndef SQLITE_DETERMINISTIC
#define SQLITE_DETERMINISTIC 0x800
#endif // SQLITE_DETERMINISTIC

namespace SQLite
{

const int   OK                = SQLITE_OK;
const int   OPEN_READONLY     = SQLITE_OPEN_READONLY;
const int   OPEN_READWRITE    = SQLITE_OPEN_READWRITE;
const int   OPEN_CREATE       = SQLITE_OPEN_CREATE;
const int   OPEN_URI          = SQLITE_OPEN_URI;
const int   OPEN_MEMORY       = SQLITE_OPEN_MEMORY;
const int   OPEN_NOMUTEX      = SQLITE_OPEN_NOMUTEX;
const int   OPEN_FULLMUTEX    = SQLITE_OPEN_FULLMUTEX;
const int   OPEN_SHAREDCACHE  = SQLITE_OPEN_SHAREDCACHE;
const int   OPEN_PRIVATECACHE = SQLITE_OPEN_PRIVATECACHE;
// check if sqlite version is >= 3.31.0 and SQLITE_OPEN_NOFOLLOW is defined
#if SQLITE_VERSION_NUMBER >= 3031000 && defined(SQLITE_OPEN_NOFOLLOW)
const int   OPEN_NOFOLLOW     = SQLITE_OPEN_NOFOLLOW;
#else
const int   OPEN_NOFOLLOW     = 0;
#endif

const char* const VERSION        = SQLITE_VERSION;
const int         VERSION_NUMBER = SQLITE_VERSION_NUMBER;

// Return SQLite version string using runtime call to the compiled library
const char* getLibVersion() noexcept
{
    return sqlite3_libversion();
}

// Return SQLite version number using runtime call to the compiled library
int getLibVersionNumber() noexcept
{
    return sqlite3_libversion_number();
}


// Open the provided database UTF-8 filename with SQLite::OPEN_xxx provided flags.
Database::Database(const char* apFilename,
                   const int   aFlags         /* = SQLite::OPEN_READONLY*/,
                   const int   aBusyTimeoutMs /* = 0 */,
                   const char* apVfs          /* = nullptr*/) :
    mFilename(apFilename)
{
    sqlite3* handle;
    const int ret = sqlite3_open_v2(apFilename, &handle, aFlags, apVfs);
    mSQLitePtr.reset(handle);
    if (SQLITE_OK != ret)
    {
        throw SQLite::Exception(handle, ret);
    }
    if (aBusyTimeoutMs > 0)
    {
        setBusyTimeout(aBusyTimeoutMs);
    }
}

// Deleter functor to use with smart pointers to close the SQLite database connection in an RAII fashion.
void Database::Deleter::operator()(sqlite3* apSQLite)
{
    const int ret = sqlite3_close(apSQLite); // Calling sqlite3_close() with a nullptr argument is a harmless no-op.

    // Avoid unreferenced variable warning when build in release mode
    (void) ret;

    // Only case of error is SQLITE_BUSY: "database is locked" (some statements are not finalized)
    // Never throw an exception in a destructor :
    SQLITECPP_ASSERT(SQLITE_OK == ret, "database is locked");  // See SQLITECPP_ENABLE_ASSERT_HANDLER
}

// Set a busy handler that sleeps for a specified amount of time when a table is locked.
void Database::setBusyTimeout(const int aBusyTimeoutMs)
{
    const int ret = sqlite3_busy_timeout(getHandle(), aBusyTimeoutMs);
    check(ret);
}

// Shortcut to execute one or multiple SQL statements without results (UPDATE, INSERT, ALTER, COMMIT, CREATE...).
// Return the number of changes.
int Database::exec(const char* apQueries)
{
    const int ret = tryExec(apQueries);
    check(ret);

    // Return the number of rows modified by those SQL statements (INSERT, UPDATE or DELETE only)
    return sqlite3_changes(getHandle());
}

int Database::tryExec(const char* apQueries) noexcept
{
    return sqlite3_exec(getHandle(), apQueries, nullptr, nullptr, nullptr);
}

// Shortcut to execute a one step query and fetch the first column of the result.
// WARNING: Be very careful with this dangerous method: you have to
// make a COPY OF THE result, else it will be destroy before the next line
// (when the underlying temporary Statement and Column objects are destroyed)
// this is an issue only for pointer type result (ie. char* and blob)
// (use the Column copy-constructor)
Column Database::execAndGet(const char* apQuery)
{
    Statement query(*this, apQuery);
    (void)query.executeStep(); // Can return false if no result, which will throw next line in getColumn()
    return query.getColumn(0);
}

// Shortcut to test if a table exists.
bool Database::tableExists(const char* apTableName) const
{
    Statement query(*this, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name=?");
    query.bind(1, apTableName);
    (void)query.executeStep(); // Cannot return false, as the above query always return a result
    return (1 == query.getColumn(0).getInt());
}

// Get the rowid of the most recent successful INSERT into the database from the current connection.
int64_t Database::getLastInsertRowid() const noexcept
{
    return sqlite3_last_insert_rowid(getHandle());
}

// Get number of rows modified by last INSERT, UPDATE or DELETE statement (not DROP table).
int Database::getChanges() const noexcept
{
    return sqlite3_changes(getHandle());
}

// Get total number of rows modified by all INSERT, UPDATE or DELETE statement since connection.
int Database::getTotalChanges() const noexcept
{
    return sqlite3_total_changes(getHandle());
}

// Return the numeric result code for the most recent failed API call (if any).
int Database::getErrorCode() const noexcept
{
    return sqlite3_errcode(getHandle());
}

// Return the extended numeric result code for the most recent failed API call (if any).
int Database::getExtendedErrorCode() const noexcept
{
    return sqlite3_extended_errcode(getHandle());
}

// Return UTF-8 encoded English language explanation of the most recent failed API call (if any).
const char* Database::getErrorMsg() const noexcept
{
    return sqlite3_errmsg(getHandle());
}

// Attach a custom function to your sqlite database. Assumes UTF8 text representation.
// Parameter details can be found here: http://www.sqlite.org/c3ref/create_function.html
void Database::createFunction(const char*   apFuncName,
                              int           aNbArg,
                              bool          abDeterministic,
                              void*         apApp,
                              void        (*apFunc)(sqlite3_context *, int, sqlite3_value **),
                              void        (*apStep)(sqlite3_context *, int, sqlite3_value **) /* = nullptr */,
                              void        (*apFinal)(sqlite3_context *) /* = nullptr */, // NOLINT(readability/casting)
                              void        (*apDestroy)(void *) /* = nullptr */)
{
    int textRep = SQLITE_UTF8;
    // optimization if deterministic function (e.g. of nondeterministic function random())
    if (abDeterministic)
    {
        textRep = textRep | SQLITE_DETERMINISTIC;
    }
    const int ret = sqlite3_create_function_v2(getHandle(), apFuncName, aNbArg, textRep,
                                               apApp, apFunc, apStep, apFinal, apDestroy);
    check(ret);
}

// Load an extension into the sqlite database. Only affects the current connection.
// Parameter details can be found here: http://www.sqlite.org/c3ref/load_extension.html
void Database::loadExtension(const char* apExtensionName, const char *apEntryPointName)
{
#ifdef SQLITE_OMIT_LOAD_EXTENSION
    // Unused
    (void)apExtensionName;
    (void)apEntryPointName;

    throw SQLite::Exception("sqlite extensions are disabled");
#else
#ifdef SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION // Since SQLite 3.13 (2016-05-18):
    // Security warning:
    // It is recommended that the SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION method be used to enable only this interface.
    // The use of the sqlite3_enable_load_extension() interface should be avoided to keep the SQL load_extension()
    // disabled and prevent SQL injections from giving attackers access to extension loading capabilities.
    // (NOTE: not using nullptr: cannot pass object of non-POD type 'std::__1::nullptr_t' through variadic function)
    int ret = sqlite3_db_config(getHandle(), SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, 1, NULL); // NOTE: not using nullptr
#else
    int ret = sqlite3_enable_load_extension(getHandle(), 1);
#endif
    check(ret);

    ret = sqlite3_load_extension(getHandle(), apExtensionName, apEntryPointName, 0);
    check(ret);
#endif
}

// Set the key for the current sqlite database instance.
void Database::key(const std::string& aKey) const
{
    int passLen = static_cast<int>(aKey.length());
#ifdef SQLITE_HAS_CODEC
    if (passLen > 0)
    {
        const int ret = sqlite3_key(getHandle(), aKey.c_str(), passLen);
        check(ret);
    }
#else // SQLITE_HAS_CODEC
    if (passLen > 0)
    {
        throw SQLite::Exception("No encryption support, recompile with SQLITE_HAS_CODEC to enable.");
    }
#endif // SQLITE_HAS_CODEC
}

// Reset the key for the current sqlite database instance.
void Database::rekey(const std::string& aNewKey) const
{
#ifdef SQLITE_HAS_CODEC
    int passLen = aNewKey.length();
    if (passLen > 0)
    {
        const int ret = sqlite3_rekey(getHandle(), aNewKey.c_str(), passLen);
        check(ret);
    }
    else
    {
        const int ret = sqlite3_rekey(getHandle(), nullptr, 0);
        check(ret);
    }
#else // SQLITE_HAS_CODEC
    static_cast<void>(aNewKey); // silence unused parameter warning
    throw SQLite::Exception("No encryption support, recompile with SQLITE_HAS_CODEC to enable.");
#endif // SQLITE_HAS_CODEC
}

// Test if a file contains an unencrypted database.
bool Database::isUnencrypted(const std::string& aFilename)
{
    if (aFilename.empty())
    {
        throw SQLite::Exception("Could not open database, the aFilename parameter was empty.");
    }

    std::ifstream fileBuffer(aFilename.c_str(), std::ios::in | std::ios::binary);
    char header[16];
    if (fileBuffer.is_open())
    {
        fileBuffer.seekg(0, std::ios::beg);
        fileBuffer.getline(header, 16);
        fileBuffer.close();
    }
    else
    {
        throw SQLite::Exception("Error opening file: " + aFilename);
    }

    return strncmp(header, "SQLite format 3\000", 16) == 0;
}

// Parse header data from a database.
Header Database::getHeaderInfo(const std::string& aFilename)
{
    Header h;
    unsigned char buf[100];
    char* pBuf = reinterpret_cast<char*>(&buf[0]);
    char* pHeaderStr = reinterpret_cast<char*>(&h.headerStr[0]);

    if (aFilename.empty())
    {
        throw SQLite::Exception("Filename parameter is empty");
    }

    {
        std::ifstream fileBuffer(aFilename.c_str(), std::ios::in | std::ios::binary);
        if (fileBuffer.is_open())
        {
            fileBuffer.seekg(0, std::ios::beg);
            fileBuffer.read(pBuf, 100);
            fileBuffer.close();
            if (fileBuffer.gcount() < 100)
            {
                throw SQLite::Exception("File " + aFilename + " is too short");
            }
        }
        else
        {
            throw SQLite::Exception("Error opening file " + aFilename);
        }
    }

    // If the "magic string" can't be found then header is invalid, corrupt or unreadable
    memcpy(pHeaderStr, pBuf, 16);
    pHeaderStr[15] = '\0';
    if (strncmp(pHeaderStr, "SQLite format 3", 15) != 0)
    {
        throw SQLite::Exception("Invalid or encrypted SQLite header in file " + aFilename);
    }

    h.pageSizeBytes = (buf[16] << 8) | buf[17];
    h.fileFormatWriteVersion = buf[18];
    h.fileFormatReadVersion = buf[19];
    h.reservedSpaceBytes = buf[20];
    h.maxEmbeddedPayloadFrac = buf[21];
    h.minEmbeddedPayloadFrac = buf[22];
    h.leafPayloadFrac = buf[23];

    h.fileChangeCounter =
        (buf[24] << 24) |
        (buf[25] << 16) |
        (buf[26] << 8)  |
        (buf[27] << 0);

    h.databaseSizePages =
        (buf[28] << 24) |
        (buf[29] << 16) |
        (buf[30] << 8)  |
        (buf[31] << 0);

    h.firstFreelistTrunkPage =
        (buf[32] << 24) |
        (buf[33] << 16) |
        (buf[34] << 8)  |
        (buf[35] << 0);

    h.totalFreelistPages =
        (buf[36] << 24) |
        (buf[37] << 16) |
        (buf[38] << 8)  |
        (buf[39] << 0);

    h.schemaCookie =
        (buf[40] << 24) |
        (buf[41] << 16) |
        (buf[42] << 8)  |
        (buf[43] << 0);

    h.schemaFormatNumber =
        (buf[44] << 24) |
        (buf[45] << 16) |
        (buf[46] << 8)  |
        (buf[47] << 0);

    h.defaultPageCacheSizeBytes =
        (buf[48] << 24) |
        (buf[49] << 16) |
        (buf[50] << 8)  |
        (buf[51] << 0);

    h.largestBTreePageNumber =
        (buf[52] << 24) |
        (buf[53] << 16) |
        (buf[54] << 8)  |
        (buf[55] << 0);

    h.databaseTextEncoding =
        (buf[56] << 24) |
        (buf[57] << 16) |
        (buf[58] << 8)  |
        (buf[59] << 0);

    h.userVersion =
        (buf[60] << 24) |
        (buf[61] << 16) |
        (buf[62] << 8)  |
        (buf[63] << 0);

    h.incrementalVaccumMode =
        (buf[64] << 24) |
        (buf[65] << 16) |
        (buf[66] << 8)  |
        (buf[67] << 0);

    h.applicationId =
        (buf[68] << 24) |
        (buf[69] << 16) |
        (buf[70] << 8)  |
        (buf[71] << 0);

    h.versionValidFor =
        (buf[92] << 24) |
        (buf[93] << 16) |
        (buf[94] << 8)  |
        (buf[95] << 0);

    h.sqliteVersion =
        (buf[96] << 24) |
        (buf[97] << 16) |
        (buf[98] << 8)  |
        (buf[99] << 0);

    return h;
}

void Database::backup(const char* apFilename, BackupType aType)
{
    // Open the database file identified by apFilename
    Database otherDatabase(apFilename, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

    // For a 'Save' operation, data is copied from the current Database to the other. A 'Load' is the reverse.
    Database& src = (aType == BackupType::Save ? *this : otherDatabase);
    Database& dest = (aType == BackupType::Save ? otherDatabase : *this);

    // Set up the backup procedure to copy between the "main" databases of each connection
    Backup bkp(dest, src);
    bkp.executeStep(); // Execute all steps at once

    // RAII Finish Backup an Close the other Database
}

}  // namespace SQLite
