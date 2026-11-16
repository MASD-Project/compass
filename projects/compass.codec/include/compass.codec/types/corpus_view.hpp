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
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus.hpp"
#include "compass.codec/types/corpus_index.hpp"
#include "compass.codec/types/dependency_record.hpp"
#include "compass.codec/types/descriptor.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.org/types/type_registry.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::codec::types {

/**
 * @brief One document as a codec sees it.
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
 * @brief What one unit read through its view, each input once, in the
 * order of the first read.
 * Every document handed out is a path input, so the path a document is read
 * at is part of what the unit read. A lookup adds an input whether or not a
 * document answered it. by_document is false for a corpus unit: it depends
 * on the whole set of documents it may read, so the documents it opens are
 * not listed one by one. registry is true once the unit read the type
 * registry.
 */
struct read_log {
    bool by_document = true;
    bool registry = false;
    std::vector<input_kind> kinds;
    std::vector<std::string> keys;
    std::vector<std::uint8_t> found;
    std::vector<compass::store::types::blob_id> values;

    [[nodiscard]] std::size_t size() const noexcept { return kinds.size(); }

    void add(input_kind kind, std::string_view key,
        const std::optional<compass::store::types::blob_id>& value) {
        for (std::size_t i = 0; i < kinds.size(); ++i) {
            if (kinds[i] == kind && keys[i] == key)
                return;
        }
        kinds.push_back(kind);
        keys.emplace_back(key);
        found.push_back(value ? 1 : 0);
        values.push_back(value.value_or(compass::store::types::blob_id{}));
    }
};

/**
 * @brief True when a codec whose type flags are @p reads may read a
 * document of type @p type: the flag of its row, or the last flag for none.
 */
[[nodiscard]] inline bool may_read(std::span<const std::uint8_t> reads,
    compass::org::types::type_id type) noexcept {
    return reads[type.is_none() ? reads.size() - 1 : type.value] != 0;
}

/**
 * @brief The document a path or ID lookup finds among those a codec whose
 * type flags are @p reads may read, or none.
 * @pre @p kind is input_kind::path or input_kind::id.
 */
[[nodiscard]] inline compass::org::types::document_id locate(
    const corpus& c, const corpus_index& index,
    std::span<const std::uint8_t> reads, input_kind kind,
    std::string_view key) {
    const auto& map = kind == input_kind::path ? index.paths : index.ids;
    const auto it = map.find(key);
    if (it == map.end() || !may_read(reads, c.types[it->second.value]))
        return compass::org::types::document_id::none();
    return it->second;
}

/**
 * @brief The value a path or ID input records for document @p row: its
 * content ID for a path lookup, the blob ID of its path for an ID lookup.
 */
[[nodiscard]] inline compass::store::types::blob_id answer_of(
    const corpus& c, input_kind kind, compass::org::types::document_id row) {
    return kind == input_kind::path ? c.contents[row.value]
        : compass::store::ops::blob_id_of(c.paths[row.value]);
}

/**
 * @brief What one codec may read of a corpus: the documents of the types
 * it declared, the type registry and its own options.
 * A codec has no other way to a document. The visible documents are
 * numbered from zero, in corpus order, and a document of an undeclared type
 * has no number and is not found by a lookup, so a codec cannot name one.
 * When a run gives the view a read_log, every document handed out, every
 * lookup and its answer, found or not, and every read of the type registry
 * is written to it. Options are not recorded: their values are part of the
 * key of every unit.
 * The view points into the corpus, its index, the type registry and the
 * options it was given and is stale once any of them changes.
 */
class corpus_view final {
public:
    /**
     * @pre @p rows names rows of @p c in ascending order, @p index was
     * built from @p c, @p reads holds one flag per row of @p registry and a
     * last one for documents with no registered type, and @p values has
     * one entry per entry of @p options, in the same order.
     */
    corpus_view(const corpus& c,
        const compass::org::types::type_registry& registry,
        const corpus_index& index,
        std::span<const compass::org::types::document_id> rows,
        std::span<const std::uint8_t> reads,
        std::span<const types::option> options,
        std::span<const std::string> values) noexcept
        : corpus_(&c), types_(&registry), index_(&index), rows_(rows),
          reads_(reads), options_(options), values_(values) {}

    /**
     * @brief Where the reads of the calls that follow are recorded; null
     * records nothing.
     */
    void record_to(read_log* log) noexcept { log_ = log; }

    /**
     * @brief How many documents the codec may read.
     */
    [[nodiscard]] std::size_t size() const noexcept { return rows_.size(); }

    /**
     * @brief The @p i th visible document.
     */
    [[nodiscard]] document operator[](std::size_t i) const {
        return open(rows_[i]);
    }

    /**
     * @brief The visible document whose ID property is @p id, if any.
     */
    [[nodiscard]] std::optional<document> find_id(std::string_view id) const {
        return find(input_kind::id, id);
    }

    /**
     * @brief The visible document at @p path, relative to the root of the
     * checkout, if any.
     */
    [[nodiscard]] std::optional<document> find_path(
        std::string_view path) const {
        return find(input_kind::path, path);
    }

    [[nodiscard]] const compass::org::types::type_registry& types() const
        noexcept {
        if (log_ != nullptr)
            log_->registry = true;
        return *types_;
    }

    /**
     * @brief The value of the option @p key: what the configuration set,
     * or the default the descriptor declares.
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
    [[nodiscard]] bool recording() const noexcept {
        return log_ != nullptr && log_->by_document;
    }

    [[nodiscard]] document open(compass::org::types::document_id row) const {
        const auto& c = *corpus_;
        if (recording())
            log_->add(input_kind::path, c.paths[row.value],
                c.contents[row.value]);
        return {row, c.paths[row.value], c.contents[row.value],
            c.types[row.value], c.sources[row.value], c.tables[row.value]};
    }

    [[nodiscard]] std::optional<document> find(input_kind kind,
        std::string_view key) const {
        const auto row = locate(*corpus_, *index_, reads_, kind, key);
        if (recording()) {
            std::optional<compass::store::types::blob_id> answer;
            if (!row.is_none())
                answer = answer_of(*corpus_, kind, row);
            log_->add(kind, key, answer);
        }
        if (row.is_none())
            return std::nullopt;
        return open(row);
    }

    const corpus* corpus_;
    const compass::org::types::type_registry* types_;
    const corpus_index* index_;
    std::span<const compass::org::types::document_id> rows_;
    std::span<const std::uint8_t> reads_;
    std::span<const types::option> options_;
    std::span<const std::string> values_;
    read_log* log_ = nullptr;
};

}

#endif
