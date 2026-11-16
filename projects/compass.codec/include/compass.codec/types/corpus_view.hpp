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
#ifndef COMPASS_CODEC_TYPES_CORPUS_VIEW_HPP
#define COMPASS_CODEC_TYPES_CORPUS_VIEW_HPP

#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include "compass.codec/types/corpus.hpp"
#include "compass.codec/types/descriptor.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.org/types/type_registry.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::codec::types {

/**
 * @brief One document as a codec sees it.
 *
 * row is the document's row in the corpus, which a diagnostic names. Every
 * range of tables is read against source. type is none for a document whose
 * type the registry does not hold.
 */
struct document {
    compass::org::types::document_id row;
    std::string_view path;
    compass::store::types::blob_id content;
    compass::org::types::type_id type;
    std::string_view source;
    const compass::org::types::tables_view& tables;
};

/**
 * @brief What one codec may read of a corpus: the documents of the types
 * it declared, the type registry and its own options.
 *
 * A codec has no other way to a document. The visible documents are
 * numbered from zero, in corpus order, and a document of an undeclared type
 * has no number, so a codec cannot name one. Every read of a document goes
 * through operator[], which is where a run records what a codec read.
 *
 * The view points into the corpus, the type registry and the options it
 * was given and is stale once any of them changes.
 */
class corpus_view final {
public:
    /**
     * @pre @p rows names rows of @p c, and @p values has one entry per
     * entry of @p options, in the same order.
     */
    corpus_view(const corpus& c,
        const compass::org::types::type_registry& registry,
        std::span<const compass::org::types::document_id> rows,
        std::span<const types::option> options,
        std::span<const std::string> values) noexcept
        : corpus_(&c), types_(&registry), rows_(rows), options_(options),
          values_(values) {}

    /**
     * @brief How many documents the codec may read.
     */
    [[nodiscard]] std::size_t size() const noexcept { return rows_.size(); }

    /**
     * @brief The @p i th visible document.
     */
    [[nodiscard]] document operator[](std::size_t i) const noexcept {
        const auto row = rows_[i];
        const auto& c = *corpus_;
        return {row, c.paths[row.value], c.contents[row.value],
            c.types[row.value], c.sources[row.value], c.tables[row.value]};
    }

    [[nodiscard]] const compass::org::types::type_registry& types()
        const noexcept {
        return *types_;
    }

    /**
     * @brief The value of the option @p key: what the configuration set,
     * or the default the descriptor declares.
     *
     * @throws std::out_of_range when the descriptor declares no such
     * option, which is a defect of the codec.
     */
    [[nodiscard]] std::string_view option(std::string_view key) const {
        for (std::size_t i = 0; i < options_.size(); ++i) {
            if (options_[i].key == key)
                return values_[i];
        }
        throw std::out_of_range("codec option not declared: " +
            std::string(key));
    }

private:
    const corpus* corpus_;
    const compass::org::types::type_registry* types_;
    std::span<const compass::org::types::document_id> rows_;
    std::span<const types::option> options_;
    std::span<const std::string> values_;
};

}

#endif
