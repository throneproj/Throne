#pragma once

#include <3rdparty/SQLiteCpp/include/SQLiteCpp.h>
#include <string>
#include <iostream>

namespace Configs {
    class Database {
        SQLite::Database db;

    public:
        Database(const std::string& path)
            : db(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
            // Enable foreign key support
            db.exec("PRAGMA foreign_keys = ON");
        }

        // 1. Recursive template to bind arguments one by one
        template<typename T, typename... Rest>
        void bindArgs(SQLite::Statement& query, int index, T&& first, Rest&&... rest) {
            query.bind(index, std::forward<T>(first));
            bindArgs(query, index + 1, std::forward<Rest>(rest)...);
        }

        // Base case for recursion
        void bindArgs(SQLite::Statement& query, int index) {
            // No more args to bind
        }

        // 2. The "PGX Style" Exec (No return value, e.g., UPDATE/INSERT)
        template<typename... Args>
        void exec(const std::string& sql, Args&&... args) {
            try {
                SQLite::Statement query(db, sql);
                bindArgs(query, 1, std::forward<Args>(args)...);
                query.exec();
            } catch (std::exception& e) {
                std::cerr << "DB Error: " << e.what() << std::endl;
            }
        }

        // 3. Helper for fetching a single row
        // Returns a Statement you can extract data from
        template<typename... Args>
        std::unique_ptr<SQLite::Statement> query(const std::string& sql, Args&&... args) {
            auto query = std::make_unique<SQLite::Statement>(db, sql);
            bindArgs(*query, 1, std::forward<Args>(args)...);
            return query;
        }
    };
}