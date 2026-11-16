/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Marco Craveiro <marco.craveiro@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#ifndef COMPASS_MODEL_OPS_BUILDER_HPP
#define COMPASS_MODEL_OPS_BUILDER_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include "compass.model/types/model_tables.hpp"

namespace compass::model::ops {

/**
 * @brief Writes values into model tables with the semantics of a Python
 * dict and list, which is what ores.codegen's loader builds.
 *
 * Setting a key an object already holds replaces its value in place. Texts
 * are interned, so a string or key stored twice is one row.
 */
class builder final {
public:
    explicit builder(types::model_tables& tables) : t_(&tables) {}

    [[nodiscard]] types::model_tables& tables() noexcept { return *t_; }
    [[nodiscard]] const types::model_tables& tables() const noexcept {
        return *t_;
    }

    [[nodiscard]] types::text_id intern(std::string_view s);

    /**
     * @brief A new model of type @p t read at @p path, whose root is an
     * empty object.
     */
    types::value_id add_model(std::string_view path, types::model_type t);

    /**
     * @brief The child of object @p parent at @p key, or none.
     */
    [[nodiscard]] types::value_id find(types::value_id parent,
        std::string_view key) const;

    /**
     * @brief The object at @p key of @p parent, made empty.
     */
    types::value_id object(types::value_id parent, std::string_view key);
    types::value_id list(types::value_id parent, std::string_view key);
    void set(types::value_id parent, std::string_view key,
        std::string_view value);
    void set(types::value_id parent, std::string_view key, const char* value) {
        set(parent, key, std::string_view(value));
    }
    void set(types::value_id parent, std::string_view key, bool value);
    void set_integer(types::value_id parent, std::string_view key,
        std::int64_t value);
    void set_null(types::value_id parent, std::string_view key);

    /**
     * @brief Copies @p value, with all it holds, to @p key of @p parent.
     */
    void set_copy(types::value_id parent, std::string_view key,
        types::value_id value);

    /**
     * @brief Removes @p key from @p parent; nothing when it is not there.
     */
    void remove(types::value_id parent, std::string_view key);

    /**
     * @brief A new object or list that no value holds, for a value built
     * before it is known where, or whether, it goes.
     */
    types::value_id detached_object();
    types::value_id detached_list();

    types::value_id append_object(types::value_id list);
    types::value_id append_list(types::value_id list);
    void append(types::value_id list, std::string_view value);
    void append(types::value_id list, bool value);
    void append_integer(types::value_id list, std::int64_t value);
    void append_null(types::value_id list);
    types::value_id append_copy(types::value_id list, types::value_id value);

    /**
     * @brief The object or list at @p key, created empty when absent.
     */
    types::value_id ensure_object(types::value_id parent,
        std::string_view key);
    types::value_id ensure_list(types::value_id parent, std::string_view key);

private:
    types::value_id add_row(types::value_kind kind, types::text_id key,
        std::int64_t scalar);
    void link(types::value_id parent, types::value_id child);
    types::value_id slot(types::value_id parent, std::string_view key,
        types::value_kind kind, std::int64_t scalar);
    types::value_id copy(types::value_id value, types::text_id key);

    struct hash {
        using is_transparent = void;
        std::size_t operator()(std::string_view s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };

    types::model_tables* t_;
    std::unordered_map<std::string, types::text_id, hash, std::equal_to<>>
        interned_;
};

}

#endif
