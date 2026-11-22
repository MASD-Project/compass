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
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
#include "compass.codec/types/dependency_record.hpp"
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
 *
 * evidence is true for a codec that also reads the evidence documents of
 * its scope. For every other codec an evidence document does not exist:
 * it is not visible, no lookup finds it and no listing holds its path.
 */
struct inputs {
    input_scope scope = input_scope::listed;
    std::span<const std::string_view> types;
    bool evidence = false;

    [[nodiscard]] static constexpr inputs of_types(
        std::span<const std::string_view> names) noexcept {
        inputs r;
        r.types = names;
        return r;
    }

    [[nodiscard]] static constexpr inputs every_typed_document() noexcept {
        inputs r;
        r.scope = input_scope::typed;
        return r;
    }

    [[nodiscard]] static constexpr inputs every_document() noexcept {
        inputs r;
        r.scope = input_scope::all;
        return r;
    }

    /**
     * @brief These inputs and the evidence documents of their scope.
     */
    [[nodiscard]] constexpr inputs with_evidence() const noexcept {
        auto r = *this;
        r.evidence = true;
        return r;
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
 * @brief How compass blame traces a finding to the commit that likely
 * caused it.
 *
 * line blames the line the finding is on. added finds the commit that
 * added the finding's subject, the thing whose addition created the
 * requirement, or the file the finding is on when the codec names no
 * subject; with a subject text, the last commit that changed how often
 * the subject holds it. drift, for a generated file that differs from
 * its regeneration, names the last commit to touch the differing lines
 * and the commits to the codec's inputs since the file last changed.
 * bisect checks main's merges, running only the reporting codec, for
 * the first that has the finding; it is the default, and the fallback
 * of the others when they find nothing.
 */
enum class blame_method : std::uint8_t { bisect, line, added, drift };

/**
 * @brief One rule a diagnostics codec enforces. name is unique within the
 * codec; with the codec's name it identifies the rule everywhere. summary
 * states in one line what the rule enforces. enforces holds the :ID: of
 * each memory or document that states the rule in prose, when one does,
 * as an uppercase UUID. blame is how compass blame traces a finding of
 * the rule to the commit that likely caused it.
 */
struct rule {
    std::string_view name;
    std::string_view summary;
    std::span<const std::string_view> enforces = {};
    blame_method blame = blame_method::bisect;
};

/**
 * @brief What one call of a codec of files emits: the files, and a
 * finding for each output it refuses to produce, which says why. A codec
 * that reports a refusal keeps on disk every file it wrote before, so a
 * refusal after a good run never removes the file that run wrote.
 */
struct file_output {
    file_table& files;
    diagnostic_table& refusals;
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
 *
 * A scan of documents sets map_document instead of map: it is called with
 * the document of the corpus at each path, its tables parsed, whatever
 * its type, and the codec's options, and reads nothing else either. A
 * listed path that is no document gets an empty fact and no findings.
 */
struct scan_function {
    std::vector<std::string_view> (*files)(const corpus_view&) = nullptr;
    void (*map)(std::string_view path, std::string_view bytes,
        const option_values& options, file_scan& out) = nullptr;
    void (*reduce)(std::span<const scanned_file> files,
        const corpus_view& view, diagnostic_table& out) = nullptr;
    void (*map_document)(const document& d, const option_values& options,
        file_scan& out) = nullptr;
};

/**
 * @brief What the prepare step of a staged codec published: the key of
 * each part, each once, in the order their files are joined, and the
 * facts the parts may read, each a name and the hash of its value.
 */
struct stage {
    std::vector<std::string> parts;
    std::vector<std::pair<std::string, compass::store::types::blob_id>>
        facts;
};

/**
 * @brief An input of a unit whose answer is no longer the one its last
 * call recorded.
 */
struct changed_input {
    input_kind kind;
    std::string_view key;
};

/**
 * @brief Where the run step of a staged codec puts what each part emits
 * and says what it read besides the view.
 *
 * begin starts a part, which must be the next of those the step was
 * given: until the next begin, what is read through the view and through
 * read is recorded as the inputs of that part, and add and keep emit its
 * files. read records that the part depends on the fact @p name, whether
 * or not the prepare step published one.
 *
 * changed lists the inputs of the part whose answers differ from those
 * its last call recorded, or nothing when no call of it is on record
 * under this version and these options. keep emits the file its last call
 * emitted at @p path, unchanged, and returns false when there is none. A
 * part that keeps a file inherits every input its last call recorded, so
 * it may skip the work that file took, but only when no changed input
 * could alter the file or what producing it reads.
 */
class part_sink {
public:
    virtual ~part_sink() = default;
    virtual void begin(std::string_view part) = 0;
    virtual void read(std::string_view fact) = 0;
    virtual void add(std::string_view path, std::string_view bytes) = 0;
    [[nodiscard]] virtual std::optional<std::span<const changed_input>>
    changed() const = 0;
    virtual bool keep(std::string_view path) = 0;
};

/**
 * @brief A corpus codec of files split into a prepare step and one part
 * per key, so that a run calls again only the parts whose inputs changed.
 *
 * prepare reads through the view what every part shares, lists the parts
 * and publishes the facts it derived, and returns the state the parts
 * run from. It is a unit of its own, with an empty document, that emits
 * nothing. run is called with that state and a run of parts; each part is
 * a unit named by its key, whose inputs are what it read through the view
 * and the facts it read. A part is called again when one of those has
 * another answer, so an edit that changes one fact runs only the parts
 * that read it. The files of the codec are those of its parts, in the
 * order prepare listed them; a path two parts emit is the later part's,
 * as the later write wins on disk.
 *
 * The state is shared by every thread that runs parts and is not changed
 * by them. prepare is called again, for its state alone, when a part must
 * run and the record or the cache stood in for prepare.
 */
struct staged_function {
    std::shared_ptr<const void> (*prepare)(const corpus_view& view,
        stage& out) = nullptr;
    void (*run)(const void* state, std::span<const std::string_view> parts,
        const corpus_view& view, part_sink& out) = nullptr;
};

/**
 * @brief The function of a codec. Its type says what one call works on, a
 * document or the corpus, and what it emits, files or diagnostics. A
 * codec of files may also refuse an output, as a finding. A scan is a
 * corpus function of diagnostics, and a staged function a corpus function
 * of files.
 *
 * A function reads only through its arguments and writes only to its
 * output table, so calls may run on any thread, in any order.
 */
using codec_function = std::variant<document_function<file_output>,
    document_function<diagnostic_table>, corpus_function<file_output>,
    corpus_function<diagnostic_table>, scan_function, staged_function>;

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
 * @brief The input that a file of a codec was rendered from, such as its
 * template, as a path relative to the checkout, from the codec's option
 * values and the bytes of the file; empty when the bytes do not say. A
 * unit may render many files from many inputs, and the record keeps only
 * what the unit read, so this names the one input behind one file. Hosts
 * call it to report what produced a file; a run never does.
 */
using origin_function = std::string (*)(const option_values& options,
    std::string_view content);

/**
 * @brief The bytes of what a codec reads from outside the checkout, such
 * as a library installed with compass, from its option values. A run
 * cannot record such reads as inputs, so they key the codec's
 * configuration instead: a change to them runs every unit of the codec
 * again. It must not throw; what it cannot read it leaves out, and the
 * codec reports the problem when it runs.
 */
using environment_function = std::string (*)(const option_values& options);

/**
 * @brief A codec: what it is, what it reads and the function that runs it.
 *
 * version is bumped by hand to discard every result the codec has stored.
 * code is the hash of the code that computes the codec, which the build
 * of its component sets, so a build that changes that code never replays
 * a result an older build stored. A result is kept under the name, the
 * version, the code, the option values and the content IDs of the
 * documents read, so a change to any of them runs the codec again.
 * rules holds, for a codec of files, the reasons it may refuse an output,
 * and is empty for one that never refuses. formatters is null for a codec
 * whose files are written as it emits them, origin for one whose
 * files name no input they were rendered from, and environment for one
 * that reads nothing outside the checkout.
 *
 * A formatting check is a codec of files with formatters whose files are
 * files of the checkout, emitted as they are. They are formatted and never
 * written: each whose formatted form differs from it is a finding of the
 * codec's first rule.
 *
 * level is the severity of every finding of the codec when it is a
 * warning: a warning is reported and fails no command, so a check can
 * show what it finds without gating on it.
 *
 * A descriptor is constant data: every span and string points at storage
 * that outlives the registry it is added to.
 */
struct descriptor {
    std::string_view name;
    std::uint32_t version = 0;
    std::string_view code = {};
    std::string_view summary;
    inputs reads;
    std::span<const option> options;
    std::span<const rule> rules;
    bool enabled_by_default = true;
    codec_function run;
    formatters_function formatters = nullptr;
    bool formatting_check = false;
    severity level = severity::error;
    origin_function origin = nullptr;
    environment_function environment = nullptr;
};

enum class unit : std::uint8_t { document, corpus, staged };
enum class output_kind : std::uint8_t { files, diagnostics };

/**
 * @brief What one call of the codec works on.
 */
[[nodiscard]] constexpr unit unit_of(const descriptor& d) noexcept {
    if (std::holds_alternative<staged_function>(d.run))
        return unit::staged;
    return std::holds_alternative<document_function<file_output>>(d.run) ||
        std::holds_alternative<document_function<diagnostic_table>>(d.run)
        ? unit::document : unit::corpus;
}

/**
 * @brief What the codec emits.
 */
[[nodiscard]] constexpr output_kind output_of(const descriptor& d) noexcept {
    return std::holds_alternative<document_function<file_output>>(d.run) ||
        std::holds_alternative<corpus_function<file_output>>(d.run) ||
        std::holds_alternative<staged_function>(d.run)
        ? output_kind::files : output_kind::diagnostics;
}

}

#endif
