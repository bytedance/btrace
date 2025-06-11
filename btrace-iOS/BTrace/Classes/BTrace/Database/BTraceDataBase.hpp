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
 */

//
//  BTraceDataBase.hpp
//  BTrace
//
//  Created by Bytedance.
//

#ifndef DATABASE_H
#define DATABASE_H


#ifdef __cplusplus

#include <array>
#include <vector>
#include <type_traits>

#include "database.hpp"
#include "reflection.hpp"


template<typename Test, template<typename...> class Ref>
struct is_specialization : std::false_type {};

template<template<typename...> class Ref, typename... Args>
struct is_specialization<Ref<Args...>, Ref>: std::true_type {};

template<typename T>
struct is_std_array : std::false_type {};

template<typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <typename T>
static int cpp_2_sqlite_type() {
    
    if (std::is_integral<T>::value) {
        return SQLITE_INTEGER;
    } else if (std::is_floating_point<T>::value) {
        return SQLITE_FLOAT;
    } else if (std::is_same<T, char*>::value || std::is_same<T, const char*>::value) {
        return SQLITE_TEXT;
    } else if (is_specialization<T, std::vector>::value) {
        return SQLITE_BLOB;
    } else if (is_std_array<T>::value) {
        return SQLITE_BLOB;
    }
    
    return SQLITE_NULL;
}

static inline std::string_view sqlite_type_2_string(int type) {
    
    switch (type) {
        case SQLITE_INTEGER:
            return "INTEGER";
        case SQLITE_FLOAT:
            return "REAL";
        case SQLITE_TEXT:
            return "TEXT";
        case SQLITE_BLOB:
            return "BLOB";
        default:
            ASSERT(false);
    }
    
    return "BLOB";
}

class BTraceDataBase
{
public:
    
    BTraceDataBase(const char *path);
    
    ~BTraceDataBase();

    btrace::DataBase *getHandle()
    {
        return db_;
    }

    bool exec(const char *query)
    {
        return db_->tryExec(query);
    }

    template <class... Args>
    bool bind_exec(const char *query, const Args &...args)
    {
        btrace::Statement stmt(db_, query);
        stmt.bindMultiple(args...);
        return stmt.executeStep();
    }
    
    template <typename Model>
    bool insert(Model& model) {
        auto reflection = model.MetaInfo();
        auto fields = reflection.GetFields();
        std::string sql = insert_bind_sql(model);
        btrace::Statement stmt(db_, sql.c_str());
        
        fields.ForEach([&model, &stmt](size_t idx, auto field) {
            auto value = field.GetValue(model);
            stmt.bind((int)idx+1, std::move(value));
        });
        
        return stmt.executeStep();
    }
    template <typename T>
    bool delete_table() {
        auto reflection = T::MetaInfo();
        auto table_name = reflection.GetName();
        std::string sql = "DELETE FROM " + std::string(table_name) + ";";
        return exec(sql.c_str());
    }

    template <typename T>
    bool create_table() {
        std::string sql = create_table_sql<T>();
        return exec(sql.c_str());
    }
    
    template <typename T>
    bool drop_table() {
        auto reflection = T::MetaInfo();
        auto table_name = reflection.GetName();
        std::string sql = "DROP TABLE IF EXISTS " + std::string(table_name) + ";";
        return exec(sql.c_str());
    }

private:

    BTraceDataBase(const BTraceDataBase &) = delete;
    BTraceDataBase &operator=(const BTraceDataBase &) = delete;
    
    template <typename Model>
    std::string insert_bind_sql(Model& model)
    {
        int count = 0;
        std::string sql = "insert into ";
        
        auto reflection = model.MetaInfo();
        std::string_view table_name = reflection.GetName();
        
        sql += std::string(table_name) + "(";
        
        auto fields = reflection.GetFields();
        fields.ForEach([&model, &sql, &count](size_t idx, auto field) {
            sql += field.GetName();
            sql += ",";
            count += 1;
        });
        
        sql.pop_back();
        sql += ") values(";
        
        for (int i=0; i<count; ++i) {
            sql += "?,";
        }
        
        sql.pop_back();
        sql += ");";
        return sql;
    }
    
    template <typename T>
    std::string create_table_sql()
    {
        auto reflection = T::MetaInfo();
        auto fields = reflection.GetFields();
        auto table_name = reflection.GetName();
        
        std::string sql = std::string("create table if not exists ") + std::string(table_name) + "(";
        
        fields.ForEach([&sql](size_t idx, auto field) {
            std::string_view type_name = field.GetTypeName();
            std::string_view name = field.GetName();
            
            using Type = typename decltype(field)::type;
            
            sql += name.data();
            sql += " ";
            int sql_type = cpp_2_sqlite_type<Type>();
            sql += sqlite_type_2_string(sql_type);
            sql += ",";
        });
        
        sql.pop_back();
        
        sql += ");";
        return sql;
    }

    btrace::DataBase *db_;
};

#endif // __cplusplus
#endif // DATABASE_H
