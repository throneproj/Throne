/**
 * @file    Savepoint.cpp
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

#include <3rdparty/SQLiteCpp/include/Assertion.h>
#include <3rdparty/SQLiteCpp/include/Database.h>
#include <3rdparty/SQLiteCpp/include/Savepoint.h>
#include <3rdparty/SQLiteCpp/include/Statement.h>

namespace SQLite
{

// Begins the SQLite savepoint
Savepoint::Savepoint(Database& aDatabase, const std::string& aName)
    : mDatabase(aDatabase), msName(aName)
{
    // workaround because you cannot bind to SAVEPOINT
    // escape name for use in query
    Statement stmt(mDatabase, "SELECT quote(?)");
    stmt.bind(1, msName);
    stmt.executeStep();
    msName = stmt.getColumn(0).getText();

    mDatabase.exec(std::string("SAVEPOINT ") + msName);
}

// Safely rollback the savepoint if it has not been committed.
Savepoint::~Savepoint()
{
    if (!mbReleased)
    {
        try
        {
            rollback();
            release();
        }
        catch (SQLite::Exception&)
        {
            // Never throw an exception in a destructor: error if already released,
            // but no harm is caused by this.
        }
    }
}

// Release the savepoint and commit
void Savepoint::release()
{
    if (!mbReleased)
    {
        mDatabase.exec(std::string("RELEASE SAVEPOINT ") + msName);
        mbReleased = true;
    }
    else
    {
        throw SQLite::Exception("Savepoint already released.");
    }
}

// Rollback to the savepoint, but don't release it
void Savepoint::rollbackTo()
{
    if (!mbReleased)
    {
        mDatabase.exec(std::string("ROLLBACK TO SAVEPOINT ") + msName);
    }
    else
    {
        throw SQLite::Exception("Savepoint already released.");
    }
}

}  // namespace SQLite
