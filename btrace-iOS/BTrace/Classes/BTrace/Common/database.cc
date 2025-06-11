/*
 * Copyright (C) 2021 ByteDance Inc
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

#include <fstream>
#include <cassert>
#include <string.h>
#include <type_traits>

#include "assert.hpp"
#include "database.hpp"
#include "utils.hpp"

namespace btrace
{

    DataBase::DataBase(const char *path,
                       const int flags,
                       const int timeout,
                       const char *vfs) : path_(path), db_lock_()
    {
        sqlite3 *handle;
        const int ret = sqlite3_open_v2(path, &handle, flags, vfs);
        if (SQLITE_OK == ret)
        {
            db_ = handle;
            if (timeout > 0)
            {
                setBusyTimeout(timeout);
            }
        }
        else
        {
            db_ = nullptr;
            FATAL("Failed to open db, path: %s", path);
        }
    }

    void DataBase::setBusyTimeout(const int timeout)
    {
        if (!db_)
            return;
        sqlite3_busy_timeout(db_, timeout);
        check();
    }

    void DataBase::setWalMode()
    {
        exec("PRAGMA journal_mode=WAL;");
    }

    void DataBase::setAutoVacuum()
    {
        exec("PRAGMA auto_vacuum=FULL;");
        vacuum();
    }

    void DataBase::vacuum()
    {
        exec("VACUUM;");
    }

    void DataBase::wal_checkpoint()
    {
        exec("PRAGMA wal_checkpoint(TRUNCATE);");
    }

    int DataBase::exec(const char *query)
    {
        tryExec(query);
        check();

        // Return the number of rows modified by those SQL statements (INSERT, UPDATE or DELETE only)
        return getChanges();
    }

    void DataBase::check() const
    {
        if (SQLITE_OK != getErrorCode()) {
            FATAL("%s", getErrorMsg());
        }
    }

    int DataBase::tryExec(const char *query) noexcept
    {
        if (!db_)
            return 0;

        std::lock_guard<std::recursive_mutex> m(db_lock_);
        return sqlite3_exec(db_, query, nullptr, nullptr, nullptr) == SQLITE_OK;
    }

    // Get number of rows modified by last INSERT, UPDATE or DELETE statement (not DROP table).
    int DataBase::getChanges() const noexcept
    {
        if (!db_)
            return 0;
        return sqlite3_changes(db_);
    }

    // Return the numeric result code for the most recent failed API call (if any).
    int DataBase::getErrorCode() const noexcept
    {
        if (!db_)
            return 0;
        return sqlite3_errcode(db_);
    }

    // Return UTF-8 encoded English language explanation of the most recent failed API call (if any).
    const char *DataBase::getErrorMsg() const noexcept
    {
        if (!db_)
            return nullptr;
        return sqlite3_errmsg(db_);
    }

    Statement::Statement(DataBase *db, const char *query) : query_(query),
                                                            database_(db),
                                                            db_(db->getHandle())
    {
        prepared_stmt_ = prepareStatement();
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ != nullptr)
        {
            database_->db_lock_.lock();
            column_count_ = sqlite3_column_count(prepared_stmt_);
        }
    }

    Statement::~Statement()
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ != nullptr)
        {
            sqlite3_finalize(prepared_stmt_);
            database_->db_lock_.unlock();
        }
    }

    int Statement::getIndex(const char *const apName) const
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return 0;
        }
        return sqlite3_bind_parameter_index(prepared_stmt_, apName);
    }

    // Bind an 32bits int value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
    void Statement::bind(const int index, const int32_t value)
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return;
        }
        sqlite3_bind_int(prepared_stmt_, index, value);
        check();
    }

    // Bind an bool value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
    void Statement::bind(const int index, const bool value)
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return;
        }
        sqlite3_bind_int(prepared_stmt_, index, value);
        check();
    }

    void Statement::bind(const int index, const long value)
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return;
        }
        sqlite3_bind_int64(prepared_stmt_, index, value);
        check();
    }

    // Bind a 32bits unsigned int value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
    void Statement::bind(const int index, const uint32_t value)
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return;
        }
        sqlite3_bind_int64(prepared_stmt_, index, value);
        check();
    }

    // Bind a 64bits int value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
    void Statement::bind(const int index, const int64_t value)
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return;
        }
        sqlite3_bind_int64(prepared_stmt_, index, value);
        check();
    }

    void Statement::bind(const int index, const uint64_t value)
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return;
        }
        sqlite3_bind_int64(prepared_stmt_, index, value);
        check();
    }

    void Statement::bind(const int index, const uintptr_t value)
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return;
        }
        sqlite3_bind_int64(prepared_stmt_, index, value);
        check();
    }

    // Bind a double (64bits float) value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
    void Statement::bind(const int index, const double value)
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return;
        }
        sqlite3_bind_double(prepared_stmt_, index, value);
        check();
    }

    // Bind a text value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
    void Statement::bind(const int index, const char *value, void (*action)(void *))
    {
        if (!action)
        {
            action = SQLITE_STATIC;
        }
        sqlite3_bind_text(prepared_stmt_, index, value, -1, action);
        check();
    }

    // Bind a binary blob value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
    void Statement::bind(const int index, const void *value, const int size, void (*action)(void *))
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
        sqlite3_bind_blob(prepared_stmt_, index, value, size, action);
        check();
    }

    // Bind a NULL value to a parameter "?", "?NNN", ":VVV", "@VVV" or "$VVV" in the SQL prepared statement
    void Statement::bind(const int index)
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return;
        }
        sqlite3_bind_null(prepared_stmt_, index);
        check();
    }

    // Execute a step of the query to fetch one row of results
    bool Statement::executeStep()
    {
        const int ret = tryExecuteStep();
        if ((SQLITE_ROW != ret) && (SQLITE_DONE != ret)) // on row or no (more) row ready, else it's a problem
        {
            if (ret == sqlite3_errcode(db_))
            {
                return false;
            }
            else
            {
//                Utils::Print("Statement needs to be reseted\n");
                abort();
            }
        }

        return has_row_; // true only if one row is accessible by getColumn(N)
    }

    int Statement::tryExecuteStep() noexcept
    {
        if (done_)
        {
            return SQLITE_MISUSE; // Statement needs to be reseted !
        }
        
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return 0;
        }

        const int ret = sqlite3_step(prepared_stmt_);
        if (SQLITE_ROW == ret) // one row is ready : call getColumn(N) to access it
        {
            has_row_ = true;
        }
        else
        {
            has_row_ = false;
            done_ = SQLITE_DONE == ret; // check if the query has finished executing
        }
        return ret;
    }

    void Statement::check() const
    {
        database_->check();
    }

    // Test if the column is NULL
    bool Statement::isColumnNull(const int index) const
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return false;
        }
        checkRow();
        checkIndex(index);
        return (SQLITE_NULL == sqlite3_column_type(prepared_stmt_, index));
    }

    bool Statement::isColumnNull(const char *apName) const
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return false;
        }
        checkRow();
        const int index = getColumnIndex(apName);
        return (SQLITE_NULL == sqlite3_column_type(prepared_stmt_, index));
    }

    // Return the named assigned to the specified result column (potentially aliased)
    const char *Statement::getColumnName(const int index) const
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return nullptr;
        }
        checkIndex(index);
        return sqlite3_column_name(prepared_stmt_, index);
    }

    // Return the index of the specified (potentially aliased) column name
    int Statement::getColumnIndex(const char *apName) const
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return 0;
        }
        // Build the map of column index by name on first call
        if (column_names_.empty())
        {
            for (int i = 0; i < column_count_; ++i)
            {
                const char *pName = sqlite3_column_name(prepared_stmt_, i);
                column_names_[pName] = i;
            }
        }

        const auto iIndex = column_names_.find(apName);
        if (iIndex == column_names_.end())
        {
//            Utils::Print("Unknown column name.\n");
            return 0;
        }

        return iIndex->second;
    }

    const char *Statement::getColumnDeclaredType(const int index) const
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return nullptr;
        }
        checkIndex(index);
        const char *result = sqlite3_column_decltype(prepared_stmt_, index);
        if (!result)
        {
//            Utils::Print("Could not determine declared column type.\n");
        }
        return result;
    }

    int Statement::getColumnType(const int index) const {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return SQLITE_NULL;
        }
        checkIndex(index);
        int result = sqlite3_column_type(prepared_stmt_, index);
        if (result == SQLITE_NULL)
        {
//            Utils::Print("Could not determine column type.\n");
        }
        return result;
    }

    int Statement::getColumnInt(const int index) const {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return 0;
        }
        checkIndex(index);
        return sqlite3_column_int(prepared_stmt_, index);
    }
    
    int64_t Statement::getColumnInt64(const int index) const {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return 0;
        }
        checkIndex(index);
        return sqlite3_column_int64(prepared_stmt_, index);
    }
    
    double Statement::getColumnDouble(const int index) const {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return 0;
        }
        checkIndex(index);
        return sqlite3_column_double(prepared_stmt_, index);
    }
    
    const uint8_t *Statement::getColumnText(const int index) const {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return nullptr;
        }
        checkIndex(index);
        return sqlite3_column_text(prepared_stmt_, index);
    }
    
    const void *Statement::getColumnBlob(const int index) const {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return nullptr;
        }
        
        const void *blob = sqlite3_column_blob(prepared_stmt_, index);
        int blob_size = sqlite3_column_bytes(prepared_stmt_, index);

        return blob;
    }

    // Get number of rows modified by last INSERT, UPDATE or DELETE statement (not DROP table).
    int Statement::getChanges() const noexcept
    {
        return sqlite3_changes(db_);
    }

    int Statement::getBindParameterCount() const noexcept
    {
        ASSERT(prepared_stmt_ != nullptr);
        if (prepared_stmt_ == nullptr)
        {
            return 0;
        }
        return sqlite3_bind_parameter_count(prepared_stmt_);
    }

    // Return the numeric result code for the most recent failed API call (if any).
    int Statement::getErrorCode() const noexcept
    {
        return sqlite3_errcode(db_);
    }

    // Return UTF-8 encoded English language explanation of the most recent failed API call (if any).
    const char *Statement::getErrorMsg() const noexcept
    {
        return sqlite3_errmsg(db_);
    }

    // Prepare btrace statement object and return shared pointer to this object
    sqlite3_stmt *Statement::prepareStatement()
    {
        sqlite3_stmt *statement;

        const int ret = sqlite3_prepare_v2(db_, query_.data(), static_cast<int>(query_.size()), &statement, nullptr);

        check();

        if (SQLITE_OK != ret)
        {
            return nullptr;
        }
        return statement;
    }

    sqlite3_stmt *Statement::getPreparedStatement() const
    {
        sqlite3_stmt *ret = prepared_stmt_;
        if (ret == nullptr)
        {
//            Utils::Print("Statement was not prepared.\n");
        }
        ASSERT(ret != nullptr);
        return ret;
    }

    // Begins the btrace transaction
    Transaction::Transaction(DataBase *db, TransactionBehavior behavior) : db_(db)
    {
        const char *stmt;
        switch (behavior)
        {
        case TransactionBehavior::DEFERRED:
            stmt = "BEGIN DEFERRED;";
            break;
        case TransactionBehavior::IMMEDIATE:
            stmt = "BEGIN IMMEDIATE;";
            break;
        case TransactionBehavior::EXCLUSIVE:
            stmt = "BEGIN EXCLUSIVE;";
            break;
        default:
            stmt = nullptr;
        }
        if (stmt)
        {
            db_->db_lock_.lock();
            db_->exec(stmt);
        }
    }

    // Begins the btrace transaction
    Transaction::Transaction(DataBase *db) : db_(db)
    {
        db->db_lock_.lock();
        db_->exec("BEGIN TRANSACTION;");
    }

    // Safely rollback the transaction if it has not been committed.
    Transaction::~Transaction()
    {
        if (false == commited_)
        {
            db_->exec("ROLLBACK TRANSACTION;");
            db_->db_lock_.unlock();
        }
    }

    // Commit the transaction.
    void Transaction::commit()
    {
        if (false == commited_)
        {
            db_->exec("COMMIT TRANSACTION;");
            commited_ = true;
            db_->db_lock_.unlock();
        }
        else
        {
//            Utils::Print("Transaction already committed.\n");
        }
    }

    // Rollback the transaction
    void Transaction::rollback()
    {
        if (false == commited_)
        {
            db_->exec("ROLLBACK TRANSACTION;");
            db_->db_lock_.unlock();
        }
        else
        {
//            Utils::Print("Transaction already committed.\n");
        }
    }

} // namespace btrace
