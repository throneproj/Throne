/**
 * @file    Database.h
 * @ingroup SQLiteCpp
 * @brief   Management of a SQLite Database Connection.
 *
 * Copyright (c) 2012-2025 Sebastien Rombauts (sebastien.rombauts@gmail.com)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */
#pragma once

#include <3rdparty/SQLiteCpp/include/SQLiteCppExport.h>
#include <3rdparty/SQLiteCpp/include/Column.h>

// c++17: MinGW GCC version > 8
// c++17: Visual Studio 2017 version 15.7
// c++17: macOS unless targetting compatibility with macOS < 10.15
#ifndef SQLITECPP_HAVE_STD_EXPERIMENTAL_FILESYSTEM
#if __cplusplus >= 201703L
    #if defined(__MINGW32__) || defined(__MINGW64__)
        #if __GNUC__ > 8 // MinGW requires GCC version > 8 for std::filesystem
            #define SQLITECPP_HAVE_STD_FILESYSTEM
        #endif
    #elif defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101500
        // macOS clang won't let us touch std::filesystem if we're targetting earlier than 10.15
    #elif defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && defined(__IPHONE_13_0) && \
__IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_13_0
        // build for iOS clang won't let us touch std::filesystem if we're targetting earlier than iOS 13
    #else
        #define SQLITECPP_HAVE_STD_FILESYSTEM
    #endif
#elif defined(_MSVC_LANG) && _MSVC_LANG >= 201703L
    #define SQLITECPP_HAVE_STD_FILESYSTEM
#endif

// disable the support if the required header is not available
#ifdef __has_include
    #if !__has_include(<filesystem>)
        #undef SQLITECPP_HAVE_STD_FILESYSTEM
    #endif
    #if !__has_include(<experimental/filesystem>)
        #undef SQLITECPP_HAVE_EXPERIMENTAL_FILESYSTEM
    #endif
#endif

// C++17 allow to disable std::filesystem support
#ifdef SQLITECPP_DISABLE_STD_FILESYSTEM
    #undef SQLITECPP_HAVE_STD_FILESYSTEM
    #undef SQLITECPP_HAVE_STD_EXPERIMENTAL_FILESYSTEM
#endif

#ifdef SQLITECPP_HAVE_STD_FILESYSTEM
#include  <filesystem>
#endif // c++17 and a suitable compiler

#else // SQLITECPP_HAVE_STD_EXPERIMENTAL_FILESYSTEM

#define SQLITECPP_HAVE_STD_FILESYSTEM
#include  <experimental/filesystem>
namespace std {
namespace filesystem = experimental::filesystem;
}

#endif // SQLITECPP_HAVE_STD_EXPERIMENTAL_FILESYSTEM

#include <memory>
#include <string.h>

// Forward declarations to avoid inclusion of <sqlite3.h> in a header
struct sqlite3;
struct sqlite3_context;

#ifndef SQLITE_USE_LEGACY_STRUCT // Since SQLITE 3.19 (used by default since SQLiteCpp 2.1.0)
typedef struct sqlite3_value sqlite3_value;
#else // Before SQLite 3.19 (legacy struct forward declaration can be activated with CMake SQLITECPP_LEGACY_STRUCT var)
struct Mem;
typedef struct Mem sqlite3_value;
#endif


namespace SQLite
{

// Those public constants enable most usages of SQLiteCpp without including <sqlite3.h> in the client application.

/// The database is opened in read-only mode. If the database does not already exist, an error is returned.
SQLITECPP_API extern const int OPEN_READONLY;     // SQLITE_OPEN_READONLY
/// The database is opened for reading and writing if possible, or reading only if the file is write protected
/// by the operating system. In either case the database must already exist, otherwise an error is returned.
SQLITECPP_API extern const int OPEN_READWRITE;    // SQLITE_OPEN_READWRITE
/// With OPEN_READWRITE: The database is opened for reading and writing, and is created if it does not already exist.
SQLITECPP_API extern const int OPEN_CREATE;       // SQLITE_OPEN_CREATE
/// Enable URI filename interpretation, parsed according to RFC 3986 (ex. "file:data.db?mode=ro&cache=private")
SQLITECPP_API extern const int OPEN_URI;          // SQLITE_OPEN_URI
/// Open in memory database
SQLITECPP_API extern const int OPEN_MEMORY;       // SQLITE_OPEN_MEMORY
/// Open database in multi-thread threading mode
SQLITECPP_API extern const int OPEN_NOMUTEX;      // SQLITE_OPEN_NOMUTEX
/// Open database with thread-safety in serialized threading mode
SQLITECPP_API extern const int OPEN_FULLMUTEX;    // SQLITE_OPEN_FULLMUTEX
/// Open database with shared cache enabled
SQLITECPP_API extern const int OPEN_SHAREDCACHE;  // SQLITE_OPEN_SHAREDCACHE
/// Open database with shared cache disabled
SQLITECPP_API extern const int OPEN_PRIVATECACHE; // SQLITE_OPEN_PRIVATECACHE
/// Database filename is not allowed to be a symbolic link (Note: only since SQlite 3.31.0 from 2020-01-22)
SQLITECPP_API extern const int OPEN_NOFOLLOW;     // SQLITE_OPEN_NOFOLLOW


SQLITECPP_API extern const int OK;                ///< SQLITE_OK (used by check() bellow)

SQLITECPP_API extern const char* const VERSION;        ///< SQLITE_VERSION string from sqlite3.h used at compile time
SQLITECPP_API extern const int         VERSION_NUMBER; ///< SQLITE_VERSION_NUMBER from sqlite3.h used at compile time

/// Return SQLite version string using runtime call to the compiled library
SQLITECPP_API const char* getLibVersion() noexcept;
/// Return SQLite version number using runtime call to the compiled library
SQLITECPP_API int   getLibVersionNumber() noexcept;

// Public structure for representing all fields contained within the SQLite header.
// Official documentation for fields: https://www.sqlite.org/fileformat.html#the_database_header
struct Header {
    unsigned char headerStr[16];
    unsigned int pageSizeBytes;
    unsigned char fileFormatWriteVersion;
    unsigned char fileFormatReadVersion;
    unsigned char reservedSpaceBytes;
    unsigned char maxEmbeddedPayloadFrac;
    unsigned char minEmbeddedPayloadFrac;
    unsigned char leafPayloadFrac;
    unsigned long fileChangeCounter;
    unsigned long  databaseSizePages;
    unsigned long firstFreelistTrunkPage;
    unsigned long totalFreelistPages;
    unsigned long schemaCookie;
    unsigned long schemaFormatNumber;
    unsigned long defaultPageCacheSizeBytes;
    unsigned long largestBTreePageNumber;
    unsigned long databaseTextEncoding;
    unsigned long userVersion;
    unsigned long incrementalVaccumMode;
    unsigned long applicationId;
    unsigned long versionValidFor;
    unsigned long sqliteVersion;
};

/**
 * @brief RAII management of a SQLite Database Connection.
 *
 * A Database object manage a list of all SQLite Statements associated with the
 * underlying SQLite 3 database connection.
 *
 * Resource Acquisition Is Initialization (RAII) means that the Database Connection
 * is opened in the constructor and closed in the destructor, so that there is
 * no need to worry about memory management or the validity of the underlying SQLite Connection.
 *
 * Thread-safety: a Database object shall not be shared by multiple threads, because :
 * 1) in the SQLite "Thread Safe" mode, "SQLite can be safely used by multiple threads
 *    provided that no single database connection is used simultaneously in two or more threads."
 * 2) the SQLite "Serialized" mode is not supported by SQLiteC++,
 *    because of the way it shares the underling SQLite precompiled statement
 *    in a custom shared pointer (See the inner class "Statement::Ptr").
 */
class SQLITECPP_API Database
{
    friend class Statement; // Give Statement constructor access to the mSQLitePtr Connection Handle

public:
    /**
     * @brief Open the provided database UTF-8 filename.
     *
     * Uses sqlite3_open_v2() with readonly default flag, which is the opposite behavior
     * of the old sqlite3_open() function (READWRITE+CREATE).
     * This makes sense if you want to use it on a readonly filesystem
     * or to prevent creation of a void file when a required file is missing.
     *
     * Exception is thrown in case of error, then the Database object is NOT constructed.
     *
     * @param[in] apFilename        UTF-8 path/uri to the database file ("filename" sqlite3 parameter)
     * @param[in] aFlags            SQLite::OPEN_READONLY/SQLite::OPEN_READWRITE/SQLite::OPEN_CREATE...
     * @param[in] aBusyTimeoutMs    Amount of milliseconds to wait before returning SQLITE_BUSY (see setBusyTimeout())
     * @param[in] apVfs             UTF-8 name of custom VFS to use, or nullptr for sqlite3 default
     *
     * @throw SQLite::Exception in case of error
     */
    Database(const char* apFilename,
             const int   aFlags         = SQLite::OPEN_READONLY,
             const int   aBusyTimeoutMs = 0,
             const char* apVfs          = nullptr);

    /**
     * @brief Open the provided database UTF-8 filename.
     *
     * Uses sqlite3_open_v2() with readonly default flag, which is the opposite behavior
     * of the old sqlite3_open() function (READWRITE+CREATE).
     * This makes sense if you want to use it on a readonly filesystem
     * or to prevent creation of a void file when a required file is missing.
     *
     * Exception is thrown in case of error, then the Database object is NOT constructed.
     *
     * @param[in] aFilename         UTF-8 path/uri to the database file ("filename" sqlite3 parameter)
     * @param[in] aFlags            SQLite::OPEN_READONLY/SQLite::OPEN_READWRITE/SQLite::OPEN_CREATE...
     * @param[in] aBusyTimeoutMs    Amount of milliseconds to wait before returning SQLITE_BUSY (see setBusyTimeout())
     * @param[in] aVfs              UTF-8 name of custom VFS to use, or empty string for sqlite3 default
     *
     * @throw SQLite::Exception in case of error
     */
    Database(const std::string& aFilename,
             const int          aFlags          = SQLite::OPEN_READONLY,
             const int          aBusyTimeoutMs  = 0,
             const std::string& aVfs            = "") :
        Database(aFilename.c_str(), aFlags, aBusyTimeoutMs, aVfs.empty() ? nullptr : aVfs.c_str())
    {
    }

    #ifdef SQLITECPP_HAVE_STD_FILESYSTEM

    /**
     * @brief Open the provided database std::filesystem::path.
     *
     * @note This feature requires std=C++17
     *
     * Uses sqlite3_open_v2() with readonly default flag, which is the opposite behavior
     * of the old sqlite3_open() function (READWRITE+CREATE).
     * This makes sense if you want to use it on a readonly filesystem
     * or to prevent creation of a void file when a required file is missing.
     *
     * Exception is thrown in case of error, then the Database object is NOT constructed.
     *
     * @param[in] apFilename        Path/uri to the database file ("filename" sqlite3 parameter)
     * @param[in] aFlags            SQLite::OPEN_READONLY/SQLite::OPEN_READWRITE/SQLite::OPEN_CREATE...
     * @param[in] aBusyTimeoutMs    Amount of milliseconds to wait before returning SQLITE_BUSY (see setBusyTimeout())
     * @param[in] apVfs             UTF-8 name of custom VFS to use, or nullptr for sqlite3 default
     *
     * @throw SQLite::Exception in case of error
     */
    Database(const std::filesystem::path& apFilename,
             const int   aFlags         = SQLite::OPEN_READONLY,
             const int   aBusyTimeoutMs = 0,
             const std::string& aVfs            = "") :
        Database(reinterpret_cast<const char*>(apFilename.u8string().c_str()),
                 aFlags, aBusyTimeoutMs, aVfs.empty() ? nullptr : aVfs.c_str())
    {
    }

    #endif // have std::filesystem

    // Database is non-copyable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Database is movable
    Database(Database&& aDatabase) = default;
    Database& operator=(Database&& aDatabase) = default;

    /**
     * @brief Close the SQLite database connection.
     *
     * All SQLite statements must have been finalized before,
     * so all Statement objects must have been unregistered.
     *
     * @warning assert in case of error
     */
    ~Database() = default;

    // Deleter functor to use with smart pointers to close the SQLite database connection in an RAII fashion.
    struct Deleter
    {
        SQLITECPP_API void operator()(sqlite3* apSQLite);
    };

    /**
     * @brief Set a busy handler that sleeps for a specified amount of time when a table is locked.
     *
     *  This is useful in multithreaded program to handle case where a table is locked for writing by a thread.
     *  Any other thread cannot access the table and will receive a SQLITE_BUSY error:
     *  setting a timeout will wait and retry up to the time specified before returning this SQLITE_BUSY error.
     *  Reading the value of timeout for current connection can be done with SQL query "PRAGMA busy_timeout;".
     *  Default busy timeout is 0ms.
     *
     * @param[in] aBusyTimeoutMs    Amount of milliseconds to wait before returning SQLITE_BUSY
     *
     * @throw SQLite::Exception in case of error
     */
    void setBusyTimeout(const int aBusyTimeoutMs);

    /**
     * @brief Shortcut to execute one or multiple statements without results. Return the number of changes.
     *
     *  This is useful for any kind of statements other than the Data Query Language (DQL) "SELECT" :
     *  - Data Manipulation Language (DML) statements "INSERT", "UPDATE" and "DELETE"
     *  - Data Definition Language (DDL) statements "CREATE", "ALTER" and "DROP"
     *  - Data Control Language (DCL) statements "GRANT", "REVOKE", "COMMIT" and "ROLLBACK"
     *
     * @see Database::tryExec() to execute, returning the sqlite result code
     * @see Statement::exec() to handle precompiled statements (for better performances) without results
     * @see Statement::executeStep() to handle "SELECT" queries with results
     *
     * @param[in] apQueries  one or multiple UTF-8 encoded, semicolon-separate SQL statements
     *
     * @return number of rows modified by the *last* INSERT, UPDATE or DELETE statement (beware of multiple statements)
     * @warning undefined for CREATE or DROP table: returns the value of a previous INSERT, UPDATE or DELETE statement.
     *
     * @throw SQLite::Exception in case of error
     */
    int exec(const char* apQueries);

    /**
     * @brief Shortcut to execute one or multiple statements without results.
     *
     *  This is useful for any kind of statements other than the Data Query Language (DQL) "SELECT" :
     *  - Data Manipulation Language (DML) statements "INSERT", "UPDATE" and "DELETE"
     *  - Data Definition Language (DDL) statements "CREATE", "ALTER" and "DROP"
     *  - Data Control Language (DCL) statements "GRANT", "REVOKE", "COMMIT" and "ROLLBACK"
     *
     * @see Database::tryExec() to execute, returning the sqlite result code
     * @see Statement::exec() to handle precompiled statements (for better performances) without results
     * @see Statement::executeStep() to handle "SELECT" queries with results
     *
     * @param[in] aQueries  one or multiple UTF-8 encoded, semicolon-separate SQL statements
     *
     * @return number of rows modified by the *last* INSERT, UPDATE or DELETE statement (beware of multiple statements)
     * @warning undefined for CREATE or DROP table: returns the value of a previous INSERT, UPDATE or DELETE statement.
     *
     * @throw SQLite::Exception in case of error
     */
    int exec(const std::string& aQueries)
    {
        return exec(aQueries.c_str());
    }

    /**
     * @brief Try to execute one or multiple statements, returning the sqlite result code.
     *
     *  This is useful for any kind of statements other than the Data Query Language (DQL) "SELECT" :
     *  - Data Manipulation Language (DML) statements "INSERT", "UPDATE" and "DELETE"
     *  - Data Definition Language (DDL) statements "CREATE", "ALTER" and "DROP"
     *  - Data Control Language (DCL) statements "GRANT", "REVOKE", "COMMIT" and "ROLLBACK"
     *
     * @see exec() to execute, returning number of rows modified
     *
     * @param[in] apQueries  one or multiple UTF-8 encoded, semicolon-separate SQL statements
     *
     * @return the sqlite result code.
     */
    int tryExec(const char* apQueries) noexcept;

    /**
     * @brief Try to execute one or multiple statements, returning the sqlite result code.
     *
     *  This is useful for any kind of statements other than the Data Query Language (DQL) "SELECT" :
     *  - Data Manipulation Language (DML) statements "INSERT", "UPDATE" and "DELETE"
     *  - Data Definition Language (DDL) statements "CREATE", "ALTER" and "DROP"
     *  - Data Control Language (DCL) statements "GRANT", "REVOKE", "COMMIT" and "ROLLBACK"
     *
     * @see exec() to execute, returning number of rows modified
     *
     * @param[in] aQueries  one or multiple UTF-8 encoded, semicolon-separate SQL statements
     *
     * @return the sqlite result code.
     */
    int tryExec(const std::string& aQueries) noexcept
    {
        return tryExec(aQueries.c_str());
    }

    /**
     * @brief Shortcut to execute a one step query and fetch the first column of the result.
     *
     *  This is a shortcut to execute a simple statement with a single result.
     * This should be used only for non reusable queries (else you should use a Statement with bind()).
     * This should be used only for queries with expected results (else an exception is fired).
     *
     * @warning WARNING: Be very careful with this dangerous method: you have to
     *          make a COPY OF THE result, else it will be destroy before the next line
     *          (when the underlying temporary Statement and Column objects are destroyed)
     *
     * @see also Statement class for handling queries with multiple results
     *
     * @param[in] apQuery  an UTF-8 encoded SQL query
     *
     * @return a temporary Column object with the first column of result.
     *
     * @throw SQLite::Exception in case of error
     */
    Column execAndGet(const char* apQuery);

    /**
     * @brief Shortcut to execute a one step query and fetch the first column of the result.
     *
     *  This is a shortcut to execute a simple statement with a single result.
     * This should be used only for non reusable queries (else you should use a Statement with bind()).
     * This should be used only for queries with expected results (else an exception is fired).
     *
     * @warning WARNING: Be very careful with this dangerous method: you have to
     *          make a COPY OF THE result, else it will be destroy before the next line
     *          (when the underlying temporary Statement and Column objects are destroyed)
     *
     * @see also Statement class for handling queries with multiple results
     *
     * @param[in] aQuery  an UTF-8 encoded SQL query
     *
     * @return a temporary Column object with the first column of result.
     *
     * @throw SQLite::Exception in case of error
     */
    Column execAndGet(const std::string& aQuery)
    {
        return execAndGet(aQuery.c_str());
    }

    /**
     * @brief Shortcut to test if a table exists.
     *
     *  Table names are case sensitive.
     *
     * @param[in] apTableName an UTF-8 encoded case sensitive Table name
     *
     * @return true if the table exists.
     *
     * @throw SQLite::Exception in case of error
     */
    bool tableExists(const char* apTableName) const;

    /**
     * @brief Shortcut to test if a table exists.
     *
     *  Table names are case sensitive.
     *
     * @param[in] aTableName an UTF-8 encoded case sensitive Table name
     *
     * @return true if the table exists.
     *
     * @throw SQLite::Exception in case of error
     */
    bool tableExists(const std::string& aTableName) const
    {
        return tableExists(aTableName.c_str());
    }

    /**
     * @brief Get the rowid of the most recent successful INSERT into the database from the current connection.
     *
     * Each entry in an SQLite table always has a unique 64-bit signed integer key called the rowid.
     * If the table has a column of type INTEGER PRIMARY KEY, then it is an alias for the rowid.
     *
     * @return Rowid of the most recent successful INSERT into the database, or 0 if there was none.
     */
    int64_t getLastInsertRowid() const noexcept;

    /// Get number of rows modified by last INSERT, UPDATE or DELETE statement (not DROP table).
    int getChanges() const noexcept;

    /// Get total number of rows modified by all INSERT, UPDATE or DELETE statement since connection (not DROP table).
    int getTotalChanges() const noexcept;

    /// Return the numeric result code for the most recent failed API call (if any).
    int getErrorCode() const noexcept;
    /// Return the extended numeric result code for the most recent failed API call (if any).
    int getExtendedErrorCode() const noexcept;
    /// Return UTF-8 encoded English language explanation of the most recent failed API call (if any).
    const char* getErrorMsg() const noexcept;

    /// Return the filename used to open the database.
    const std::string& getFilename() const noexcept
    {
        return mFilename;
    }

    /**
     * @brief Return raw pointer to SQLite Database Connection Handle.
     *
     * This is often needed to mix this wrapper with other libraries or for advance usage not supported by SQLiteCpp.
     */
    sqlite3* getHandle() const noexcept
    {
        return mSQLitePtr.get();
    }

    /**
     * @brief Create or redefine a SQL function or aggregate in the sqlite database.
     *
     *  This is the equivalent of the sqlite3_create_function_v2 command.
     * @see http://www.sqlite.org/c3ref/create_function.html
     *
     * @note UTF-8 text encoding assumed.
     *
     * @param[in] apFuncName    Name of the SQL function to be created or redefined
     * @param[in] aNbArg        Number of arguments in the function
     * @param[in] abDeterministic Optimize for deterministic functions (most are). A random number generator is not.
     * @param[in] apApp         Arbitrary pointer of user data, accessible with sqlite3_user_data().
     * @param[in] apFunc        Pointer to a C-function to implement a scalar SQL function (apStep & apFinal nullptr)
     * @param[in] apStep        Pointer to a C-function to implement an aggregate SQL function (apFunc nullptr)
     * @param[in] apFinal       Pointer to a C-function to implement an aggregate SQL function (apFunc nullptr)
     * @param[in] apDestroy     If not nullptr, then it is the destructor for the application data pointer.
     *
     * @throw SQLite::Exception in case of error
     */
    void createFunction(const char* apFuncName,
                        int         aNbArg,
                        bool        abDeterministic,
                        void*       apApp,
                        void      (*apFunc)(sqlite3_context *, int, sqlite3_value **),
                        void      (*apStep)(sqlite3_context *, int, sqlite3_value **) = nullptr,
                        void      (*apFinal)(sqlite3_context *) = nullptr,  // NOLINT(readability/casting)
                        void      (*apDestroy)(void *) = nullptr);

    /**
     * @brief Load a module into the current sqlite database instance.
     *
     *  This is the equivalent of the sqlite3_load_extension call, but additionally enables
     *  module loading support prior to loading the requested module.
     *
     * @see http://www.sqlite.org/c3ref/load_extension.html
     *
     * @note UTF-8 text encoding assumed.
     *
     * @param[in] apExtensionName   Name of the shared library containing extension
     * @param[in] apEntryPointName  Name of the entry point (nullptr to let sqlite work it out)
     *
     * @throw SQLite::Exception in case of error
     */
    void loadExtension(const char* apExtensionName, const char* apEntryPointName);

    /**
    * @brief Set the key for the current sqlite database instance.
    *
    *  This is the equivalent of the sqlite3_key call and should thus be called
    *  directly after opening the database.
    *  Open encrypted database -> call db.key("secret") -> database ready
    *
    * @param[in] aKey   Key to decode/encode the database
    *
    * @throw SQLite::Exception in case of error
    */
    void key(const std::string& aKey) const;

    /**
    * @brief Reset the key for the current sqlite database instance.
    *
    *  This is the equivalent of the sqlite3_rekey call and should thus be called
    *  after the database has been opened with a valid key. To decrypt a
    *  database, call this method with an empty string.
    *  Open normal database -> call db.rekey("secret") -> encrypted database, database ready
    *  Open encrypted database -> call db.key("secret") -> call db.rekey("newsecret") -> change key, database ready
    *  Open encrypted database -> call db.key("secret") -> call db.rekey("") -> decrypted database, database ready
    *
    * @param[in] aNewKey   New key to encode the database
    *
    * @throw SQLite::Exception in case of error
    */
    void rekey(const std::string& aNewKey) const;

    /**
    * @brief Test if a file contains an unencrypted database.
    *
    *  This is a simple test that reads the first bytes of a database file and
    *  compares them to the standard header for unencrypted databases. If the
    *  header does not match the standard string, we assume that we have an
    *  encrypted file.
    *
    * @param[in] aFilename path/uri to a file
    *
    * @return true if the database has the standard header.
    *
    * @throw SQLite::Exception in case of error
    */
    static bool isUnencrypted(const std::string& aFilename);

    /**
    * @brief Parse SQLite header data from a database file.
    *
    *  This function reads the first 100 bytes of a SQLite database file
    *  and reconstructs groups of individual bytes into the associated fields
    *  in a Header object.
    *
    * @param[in] aFilename path/uri to a file
    *
    * @return Header object containing file data
    *
    * @throw SQLite::Exception in case of error
    */
    static Header getHeaderInfo(const std::string& aFilename);

    // Parse SQLite header data from a database file.
    Header getHeaderInfo() const
    {
        return getHeaderInfo(mFilename);
    }

    /**
     * @brief BackupType for the backup() method
     */
    enum BackupType { Save, Load };

    /**
     * @brief Load or save the database content.
     *
     * This function is used to load the contents of a database file on disk
     * into the "main" database of open database connection, or to save the current
     * contents of the database into a database file on disk.
     *
     * @throw SQLite::Exception in case of error
     */
    void backup(const char* apFilename, BackupType aType);

    /**
     * @brief Check if aRet equal SQLITE_OK, else throw a SQLite::Exception with the SQLite error message
     */
    void check(const int aRet) const
    {
        if (SQLite::OK != aRet)
        {
            throw SQLite::Exception(getHandle(), aRet);
        }
    }

private:
    // TODO: perhaps switch to having Statement sharing a pointer to the Connexion
    std::unique_ptr<sqlite3, Deleter>   mSQLitePtr; ///< Pointer to SQLite Database Connection Handle
    std::string                         mFilename;  ///< UTF-8 filename used to open the database
};

}  // namespace SQLite
