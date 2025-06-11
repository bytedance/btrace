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
//  reflection.h
//  Pods
//
//  Created by ByteDance on 2024/1/22.
//

#ifndef BTRACE_REFLECTION_H_
#define BTRACE_REFLECTION_H_

#ifdef __cplusplus

#include <tuple>
#include <string_view>

namespace btrace
{

    template <typename T>
    constexpr std::string_view type_name()
    {
        constexpr std::string_view pretty_name = __PRETTY_FUNCTION__;
        constexpr std::size_t begin = pretty_name.find("=") + 2;
        constexpr std::size_t end = pretty_name.find_last_of("]");
        auto name = pretty_name.substr(begin, end - begin);
        return name;
    }

    template <typename Class, typename Member>
    struct Field
    {
        const std::string_view type_name_;
        const std::string_view name_;
        Member Class::*ptr_;

        constexpr Field(std::string_view name, Member Class::*ptr) : name_(name), ptr_(ptr), type_name_(type_name<Member>()) {}

        std::string_view GetTypeName() const
        {
            return type_name_;
        }

        std::string_view GetName() const
        {
            return name_;
        }

        Member GetValue(Class &object) const
        {
            return object.*ptr_;
        }
        
        void SetValue(Class &object, Member val) {
            object.*ptr_ = val;
        }

        using type = Member;
    };

    template <typename... Fields>
    struct FieldList
    {
        const size_t size_;
        const std::tuple<Fields...> fields_;

        constexpr FieldList(Fields... fields) : size_(std::tuple_size<decltype(fields_)>::value), fields_{fields...} {}

        constexpr size_t size() const
        {
            return std::tuple_size<decltype(fields_)>::value;
        }

        //        template <class F>
        //        constexpr auto ForEach(F &&func) const
        //        {
        //            std::apply(func, fields_);
        //        }

        template <std::size_t idx = 0, typename F>
        constexpr auto ForEach(F &&func) const
        {
            constexpr auto sz = std::tuple_size<decltype(fields_)>::value;

            if constexpr (idx < sz)
            {
                func(idx, std::get<idx>(fields_));
                ForEach<idx + 1>(func);
            }
        }
    };

    template <typename Class, typename... Fields>
    struct ReflectionMeta
    {
        using type = Class;

        const std::string_view name_;
        const FieldList<Fields...> field_list_;

        constexpr ReflectionMeta(Fields... fields) : name_(Class::TableName()), field_list_{fields...} {}

        constexpr auto GetName() const
        {
            return name_;
        }

        constexpr auto GetFields() const
        {
            return field_list_;
        }
    };

    template <typename Class>
    struct Reflection
    {
        using type = Class;

        template <typename... Fields>
        static auto Register(const Fields &...fields)
        {
            return ReflectionMeta<Class, Fields...>(fields...);
        }
    };
}

#endif // __cplusplus

#endif /* BTRACE_REFLECTION_H_ */
