/*
 * Copyright (C) 2025 ByteDance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Includes work from SQLiteCpp (https://github.com/SRombauts/SQLiteCpp)
 * with modifications.
 *
 * Copyright (c) 2012-2024 Sebastien Rombauts (sebastien.rombauts@gmail.com)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef BTRACE_DATABASE_H
#define BTRACE_DATABASE_H

#ifdef __cplusplus

#include <map>
#include <mutex>
#include <array>
#include <vector>
#include <memory>
#include <string>
#include <sqlite3.h>

#include "assert.hpp"

struct sqlite3;
struct sqlite3_stmt;

namespace btrace
{

    class DataBase
    {

    public:
        DataBase(const char *path,
                 const int flags,
                 const int timeout = 0,
                 const char *vfs = nullptr);

        DataBase(const DataBase &) = delete;
        DataBase &operator=(const DataBase &) = delete;

        ~DataBase() = default;

        void setBusyTimeout(const int timeout);

        void setWalMode();

        void setAutoVacuum();
        
        void vacuum();
        
        void wal_checkpoint();

        int exec(const char *query);

        int tryExec(const char *query) noexcept;

        bool tableExists(const char *tableName) const;

        int getChanges() const noexcept;

        int getErrorCode() const noexcept;

        const char *getErrorMsg() const noexcept;

        const std::string &getPath() const noexcept
        {
            return path_;
        }

        sqlite3 *getHandle() const noexcept
        {
            return db_;
        }

        void check() const;

    private:
        sqlite3 *db_;
        std::string path_;
        std::recursive_mutex db_lock_;
        
        friend class Statement;
        friend class Transaction;
    };

    enum class TransactionBehavior
    {
        DEFERRED,
        IMMEDIATE,
        EXCLUSIVE,
    };

    class Statement
    {
    public:
        Statement(DataBase *db, const char *query);

        // Statement is non-copyable
        Statement(const Statement &) = delete;
        Statement &operator=(const Statement &) = delete;

        ~Statement();

        int getIndex(const char *const name) const;

        template <class... Args>
        void bindMultiple(const Args &...args)
        {
            int pos = 0;
            (void)std::initializer_list<int>{
                ((void)bind(++pos, std::forward<decltype(args)>(args)), 0)...};
        }

        template <typename... Types>
        void bind(const int index, const std::tuple<Types...> &tuple)
        {
            std::apply([this, index](const auto &...args)
                       { bind(index, args...); },
                       tuple);
        }

        void bind(const int index, const int32_t value);

        void bind(const int index, const bool value);

        void bind(const int index, const long value);

        void bind(const int index, const uintptr_t value);

        void bind(const int index, const uint32_t value);

        void bind(const int index, const int64_t value);

        void bind(const int index, const uint64_t value);

        void bind(const int index, const double value);

        void bind(const int index, const char *value, void (*action)(void *) = nullptr);

        void bind(const int index, const void *value, const int size, void (*action)(void *) = nullptr);

        template <typename T>
        void bind(const int index, const std::vector<T> &&value, void (*action)(void *) = nullptr)
        {
            ASSERT(prepared_stmt_ != nullptr);
            if (prepared_stmt_ == nullptr)
            {
                return;
            }
            if (!action)
            {
                action = SQLITE_TRANSIENT;
            }

            int blob_size = static_cast<int>(value.size() * sizeof(T));
            sqlite3_bind_blob(prepared_stmt_, index, value.data(), blob_size, action);
            check();
        };
        
        template <typename T, size_t Size>
        void bind(const int index, const std::array<T, Size> &&value, void (*action)(void *) = nullptr)
        {
            ASSERT(prepared_stmt_ != nullptr);
            if (prepared_stmt_ == nullptr)
            {
                return;
            }
            if (!action)
            {
                action = SQLITE_TRANSIENT;
            }

            int blob_size = static_cast<int>(value.size() * sizeof(T));
            sqlite3_bind_blob(prepared_stmt_, index, value.data(), blob_size, action);
            check();
        };

        //        void bind(const int index, const std::vector<uintptr_t> &&value, void (*action)(void *) = nullptr);

        void bind(const int index);

        void bind(const char *name, const int32_t value)
        {
            bind(getIndex(name), value);
        }

        void bind(const char *name, const uint32_t value)
        {
            bind(getIndex(name), value);
        }

        void bind(const char *name, const int64_t value)
        {
            bind(getIndex(name), value);
        }

        void bind(const char *name, const double value)
        {
            bind(getIndex(name), value);
        }

        void bind(const char *name, const char *value, void (*action)(void *) = nullptr)
        {
            bind(getIndex(name), value);
        }

        void bind(const char *name, const void *value, const int size, void (*action)(void *) = nullptr)
        {
            bind(getIndex(name), value, size);
        }

        void bind(const char *name) // bind NULL value
        {
            bind(getIndex(name));
        }

        bool executeStep();

        int tryExecuteStep() noexcept;

    public:
        bool isColumnNull(const int index) const;

        bool isColumnNull(const char *name) const;

        const char *getColumnName(const int index) const;

        int getColumnIndex(const char *name) const;

        const char *getColumnDeclaredType(const int index) const;
        
        int getColumnType(const int index) const;
        
        int getColumnInt(const int index) const;
        
        int64_t getColumnInt64(const int index) const;
        
        double getColumnDouble(const int index) const;
        
        const uint8_t *getColumnText(const int index) const;
        
        const void *getColumnBlob(const int index) const;

        int getChanges() const noexcept;

        const std::string_view &getQuery() const
        {
            return query_;
        }

        int getColumnCount() const
        {
            return column_count_;
        }

        bool hasRow() const
        {
            return has_row_;
        }

        bool isDone() const
        {
            return done_;
        }

        sqlite3_stmt *getPreparedStatement() const;

        int getBindParameterCount() const noexcept;

        int getErrorCode() const noexcept;

        const char *getErrorMsg() const noexcept;

        using TStatementPtr = std::shared_ptr<sqlite3_stmt>;

    private:
        void check() const;

        void checkRow() const
        {
            ASSERT(false == has_row_);
        }

        void checkIndex(const int index) const
        {
            ASSERT((0 <= index) && (index < column_count_));
        }

        sqlite3_stmt *prepareStatement();

        DataBase *database_;
        std::string_view query_;      //!< UTF-8 SQL Query
        sqlite3 *db_;                 //!< Pointer to btrace DataBase Connection Handle
        sqlite3_stmt *prepared_stmt_; //!< Shared Pointer to the prepared btrace Statement Object
        int column_count_ = 0;        //!< Number of columns in the result of the prepared statement
        bool has_row_ = false;        //!< true when a row has been fetched with executeStep()
        bool done_ = false;           //!< true when the last executeStep() had no more row to fetch

        /// Map of columns index by name (mutable so getColumnIndex can be const)
        mutable std::map<std::string, int> column_names_;
    };

    class Transaction
    {
    public:
        explicit Transaction(DataBase *db);

        explicit Transaction(DataBase *db, TransactionBehavior behavior);

        Transaction(const Transaction &) = delete;
        Transaction &operator=(const Transaction &) = delete;

        ~Transaction();

        void commit();

        void rollback();

    private:
        DataBase *db_;
        bool commited_ = false;
    };

} // namespace btrace

#endif // __cplusplus
#endif // BTRACE_DATABASE_H
