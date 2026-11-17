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
#ifndef COMPASS_CODEC_TYPES_DESCRIPTOR_HPP
#define COMPASS_CODEC_TYPES_DESCRIPTOR_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "compass.codec/types/diagnostic_table.hpp"
#include "compass.codec/types/file_table.hpp"
#include "compass.codec/types/formatter.hpp"

namespace compass::codec::types {

struct document;
class corpus_view;

/**
 * @brief Which documents a codec reads: those of the types it lists, those
 * of any registered type, or every document, typed or not.
 */
enum class input_scope : std::uint8_t { listed, typed, all };

/**
 * @brief The documents a codec declares it reads.
 *
 * types holds #+type: values and is empty unless the scope is listed. They
 * are resolved against the type registry when the codec registry is built.
 */
struct inputs {
    input_scope scope = input_scope::listed;
    std::span<const std::string_view> types;

    [[nodiscard]] static constexpr inputs of_types(
        std::span<const std::string_view> names) noexcept {
        return {input_scope::listed, names};
    }

    [[nodiscard]] static constexpr inputs every_typed_document() noexcept {
        return {input_scope::typed, {}};
    }

    [[nodiscard]] static constexpr inputs every_document() noexcept {
        return {input_scope::all, {}};
    }
};

/**
 * @brief One setting of a codec, with the value it has when the
 * configuration does not set it.
 */
struct option {
    std::string_view key;
    std::string_view default_value;
    std::string_view summary;
};

/**
 * @brief One rule a diagnostics codec enforces. name is unique within the
 * codec; with the codec's name it identifies the rule everywhere. summary
 * states in one line what the rule enforces. enforces holds the :ID: of
 * each memory or document that states the rule in prose, when one does,
 * as an uppercase UUID.
 */
struct rule {
    std::string_view name;
    std::string_view summary;
    std::span<const std::string_view> enforces = {};
};

/**
 * @brief A codec that is run once per visible document.
 *
 * The first argument is the document to work on. The view gives the codec
 * its options and the type registry, and the other documents it declared
 * when it needs them.
 */
template <typename Output>
using document_function = void (*)(const document&, const corpus_view&,
    Output&);

/**
 * @brief A codec that is run once over every visible document.
 */
template <typename Output>
using corpus_function = void (*)(const corpus_view&, Output&);

/**
 * @brief The value of each option of a codec, in the order its descriptor
 * declares them.
 */
struct option_values {
    std::span<const option> options;
    std::span<const std::string> values;

    /**
     * @brief The value of the option @p key.
     * @throws std::out_of_range when no option has that key, which is a
     * defect of the codec.
     */
    [[nodiscard]] std::string_view operator[](std::string_view key) const {
        for (std::size_t i = 0; i < options.size(); ++i) {
            if (options[i].key == key)
                return values[i];
        }
        throw std::out_of_range("codec option not declared: " +
            std::string(key));
    }
};

/**
 * @brief What the map step of a scan made of one file: the findings on
 * that file, and the fact the reduce step reads, in bytes the codec
 * chooses.
 */
struct file_scan {
    diagnostic_table findings;
    std::string fact;
};

/**
 * @brief One file a scan mapped, as its reduce step sees it.
 */
struct scanned_file {
    std::string_view path;
    std::string_view fact;
};

/**
 * @brief A corpus codec of diagnostics split into a map over files and a
 * reduce over what the map made of them, so that a run maps again only
 * the files whose content changed.
 *
 * files lists, through the view, the paths to map, each once. map is
 * called with each path, the file's bytes (empty when it cannot be read)
 * and the codec's options; it reads nothing else, so its result depends on
 * those alone, and its findings are on that file. reduce, when not null,
 * is called once with every file in the order files gave them and may
 * read more through the view. The findings of the call are those of the
 * map steps, file by file, then those of reduce.
 */
struct scan_function {
    std::vector<std::string_view> (*files)(const corpus_view&) = nullptr;
    void (*map)(std::string_view path, std::string_view bytes,
        const option_values& options, file_scan& out) = nullptr;
    void (*reduce)(std::span<const scanned_file> files,
        const corpus_view& view, diagnostic_table& out) = nullptr;
};

/**
 * @brief The function of a codec. Its type says what one call works on, a
 * document or the corpus, and what it emits, files or diagnostics. A scan
 * is a corpus function of diagnostics.
 *
 * A function reads only through its arguments and writes only to its
 * output table, so calls may run on any thread, in any order.
 */
using codec_function = std::variant<document_function<file_table>,
    document_function<diagnostic_table>, corpus_function<file_table>,
    corpus_function<diagnostic_table>, scan_function>;

/**
 * @brief The formatters the files of a codec go through before they are
 * written, from the codec's option values and the root of the checkout.
 * The formatted forms are not part of what the codec records: a unit
 * keeps the bytes the function emitted, and the host formats them, through
 * the result cache, in every run.
 * @throws std::runtime_error when the options name a program that cannot
 * be found.
 */
using formatters_function = std::vector<formatter> (*)(
    const option_values& options, const std::filesystem::path& checkout);

/**
 * @brief A codec: what it is, what it reads and the function that runs it.
 *
 * version is bumped by hand whenever some input would now give an output
 * that differs from the one the previous version gave it. A result is kept
 * under the name, the version, the option values and the content IDs of
 * the documents read, so a change to any of them runs the codec again.
 * rules is empty for a codec that emits files. formatters is null for a
 * codec whose files are written as it emits them.
 *
 * A descriptor is constant data: every span and string points at storage
 * that outlives the registry it is added to.
 */
struct descriptor {
    std::string_view name;
    std::uint32_t version = 0;
    std::string_view summary;
    inputs reads;
    std::span<const option> options;
    std::span<const rule> rules;
    bool enabled_by_default = true;
    codec_function run;
    formatters_function formatters = nullptr;
};

enum class unit : std::uint8_t { document, corpus };
enum class output_kind : std::uint8_t { files, diagnostics };

/**
 * @brief What one call of the codec works on.
 */
[[nodiscard]] constexpr unit unit_of(const descriptor& d) noexcept {
    return std::holds_alternative<document_function<file_table>>(d.run) ||
        std::holds_alternative<document_function<diagnostic_table>>(d.run)
        ? unit::document : unit::corpus;
}

/**
 * @brief What the codec emits.
 */
[[nodiscard]] constexpr output_kind output_of(const descriptor& d) noexcept {
    return std::holds_alternative<document_function<file_table>>(d.run) ||
        std::holds_alternative<corpus_function<file_table>>(d.run)
        ? output_kind::files : output_kind::diagnostics;
}

}

#endif
