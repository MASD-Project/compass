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
#include "compass.bench/ops/render_stage.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <fstream>
#include <map>
#include <stdexcept>
#include "compass.bench/ops/model_corpus.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codegen/codecs/codegen.hpp"
#include "compass.codegen/ops/generate.hpp"
#include "compass.codegen/ops/output.hpp"
#include "compass.model/io/json.hpp"
#include "compass.model/ops/builder.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::bench::ops {

namespace {

namespace cops = compass::codegen::ops;
using compass::model::types::model_tables;
using compass::model::types::text_id;
using compass::model::types::value_id;
using compass::model::types::value_kind;
using clock_type = std::chrono::steady_clock;

double elapsed_ms(clock_type::time_point start) {
    return std::chrono::duration<double, std::milli>(
        clock_type::now() - start).count();
}

value_id member(const model_tables& t, value_id object, std::string_view key) {
    for (auto c = t.values.firsts[object]; !c.is_none();
         c = t.values.nexts[c]) {
        if (compass::model::types::text_of(t.texts, t.values.keys[c]) == key)
            return c;
    }
    return value_id::none();
}

std::string_view string_member(const model_tables& t, value_id object,
    std::string_view key) {
    const auto v = member(t, object, key);
    if (v.is_none() || t.values.kinds[v] != value_kind::string)
        throw std::runtime_error("no string " + std::string(key));
    return compass::model::types::text_of(t.texts,
        text_id{static_cast<std::uint32_t>(t.values.scalars[v])});
}

void write_file(const std::filesystem::path& target, std::string_view text) {
    std::filesystem::create_directories(target.parent_path());
    std::ofstream os(target, std::ios::binary);
    os << text;
    if (!os)
        throw std::runtime_error("cannot write " + target.string());
}

/**
 * @brief Where the rendered outputs go: written under <out>/renders when
 * out is set, and kept in rendered when they are to be formatted.
 */
struct render_sink {
    std::filesystem::path out;
    bool keep = false;
    compass::codec::types::file_table rendered;

    void add(std::string_view path, std::string_view text) {
        if (!out.empty())
            write_file(out / "renders" / std::filesystem::path(path), text);
        if (keep)
            rendered.add(path, text);
    }
};

/**
 * @brief The files of the checkout as a codec's view sees them, read from
 * disk, and the parsed documents among them.
 */
class disk_checkout final {
public:
    disk_checkout(const std::filesystem::path& checkout,
        const types::file_table& table)
        : checkout_(checkout), documents_(checkout, table.paths) {
        for (const auto& p : table.paths)
            files_.paths.push_back(p);
        std::ranges::sort(files_.paths);
        files_.read = [this](std::string_view path)
            -> std::optional<std::string> {
            try {
                const compass::platform::filesystem::mapped_file f(
                    checkout_ / std::filesystem::path(path));
                return std::string(f.text());
            } catch (const std::exception&) {
                return std::nullopt;
            }
        };
        files_.content_of = [this](std::string_view path) {
            return files_.read(path).transform([](const std::string& s) {
                return compass::store::ops::blob_id_of(s);
            });
        };
    }
    disk_checkout(const disk_checkout&) = delete;
    disk_checkout& operator=(const disk_checkout&) = delete;

    [[nodiscard]] cops::checkout_reader reader() {
        namespace ct = compass::codec::types;
        return {.find = documents_.finder(),
            .list = [this](std::string_view prefix, std::string_view suffix) {
                return ct::matching_files(&files_, prefix, suffix);
            },
            .read = files_.read,
            .exists = [this](std::string_view path) {
                return ct::holds_file(&files_, path);
            }};
    }

private:
    std::filesystem::path checkout_;
    model_corpus documents_;
    compass::codec::types::checkout_files files_;
};

struct unit {
    std::string output;
    std::string template_name;
};

/**
 * @brief The dump's outputs, grouped by the context they render from, so
 * one context at a time is read and rendered.
 */
std::map<std::string, std::vector<unit>> oracle_units(
    const std::filesystem::path& oracle) {
    const compass::platform::filesystem::mapped_file file(
        oracle / "index.json");
    model_tables t;
    compass::model::ops::builder b(t);
    const auto index = compass::model::io::read_json(b, file.text());
    std::map<std::string, std::vector<unit>> by_context;
    const auto outputs = member(t, index, "outputs_by_path");
    for (auto e = t.values.firsts[outputs]; !e.is_none();
         e = t.values.nexts[e]) {
        by_context[std::string(string_member(t, e, "context"))].push_back(
            {std::string(string_member(t, e, "output")),
             std::string(string_member(t, e, "template"))});
    }
    return by_context;
}

void render_oracle(render_result& r, const cops::template_map& templates,
    const std::filesystem::path& oracle, unsigned repetitions,
    render_sink& sink) {
    const auto start = clock_type::now();
    const auto by_context = oracle_units(oracle);
    r.setup_ms += elapsed_ms(start);
    r.render_ms.assign(repetitions, 0.0);
    std::string text;
    for (const auto& [digest, units] : by_context) {
        const auto read_start = clock_type::now();
        const compass::platform::filesystem::mapped_file file(
            oracle / "contexts" / (digest + ".json"));
        model_tables t;
        compass::model::ops::builder b(t);
        const auto context = compass::model::io::read_json(b, file.text());
        r.setup_ms += elapsed_ms(read_start);
        ++r.contexts;
        for (const auto& u : units) {
            const auto it = templates.find(u.template_name);
            if (it == templates.end()) {
                r.messages.push_back(u.output + ": no template " +
                    u.template_name);
                continue;
            }
            for (unsigned rep = 0; rep < repetitions; ++rep) {
                text.clear();
                const auto render_start = clock_type::now();
                cops::render_output(text, u.template_name, it->second, t,
                    context);
                r.render_ms[rep] += elapsed_ms(render_start);
            }
            ++r.outputs;
            r.bytes += text.size();
            sink.add(u.output, text);
        }
    }
}

/**
 * @brief Writes each distinct context of @p plan once, as
 * <out>/contexts/<n>.json, and <out>/context_index.json, which names for
 * every output its context and its model type, for
 * build/scripts/codegen_render_diff.py.
 */
void write_contexts(const std::filesystem::path& out,
    const cops::generation_plan& plan) {
    std::map<std::uint32_t, std::size_t> numbers;
    std::string index = "[\n";
    std::string text;
    const auto& outputs = plan.outputs;
    for (std::size_t i = 0; i < outputs.size(); ++i) {
        const auto& p = outputs[i];
        const auto [it, added] = numbers.try_emplace(p.context.value,
            numbers.size());
        const auto number = std::to_string(it->second);
        if (added) {
            text.clear();
            compass::model::io::write_json(text, plan.tables, p.context);
            write_file(out / "contexts" / (number + ".json"), text);
        }
        index += "{\"output\": \"" + p.output + "\", \"context\": \"" +
            number + "\", \"type\": \"" + std::string(p.type) + "\"}" +
            (i + 1 < outputs.size() ? ",\n" : "\n");
    }
    write_file(out / "context_index.json", index + "]\n");
}

void render_models(render_result& r, const cops::template_map& templates,
    const cops::checkout_reader& reader, const cops::generation_settings& s,
    unsigned repetitions, render_sink& sink) {
    const auto start = clock_type::now();
    cops::generation_plan plan;
    cops::plan_generation(plan, reader, s);
    r.setup_ms = elapsed_ms(start);
    r.contexts = plan.contexts;
    r.messages = plan.messages;
    if (!sink.out.empty())
        write_contexts(sink.out, plan);

    r.render_ms.assign(repetitions, 0.0);
    std::string text;
    for (const auto& o : plan.outputs) {
        for (unsigned rep = 0; rep < repetitions; ++rep) {
            text.clear();
            const auto render_start = clock_type::now();
            cops::render_planned(text, templates, plan, o);
            r.render_ms[rep] += elapsed_ms(render_start);
        }
        ++r.outputs;
        r.bytes += text.size();
        sink.add(o.output, text);
    }
}

}

render_result run_render_stage(const std::filesystem::path& checkout,
    const types::file_table& table,
    const compass::codec::types::option_values& options,
    const std::filesystem::path& oracle, unsigned repetitions,
    const std::filesystem::path& out,
    const std::filesystem::path& format_store, unsigned threads) {
    render_result r;
    const auto settings = compass::codegen::codecs::settings_of(options);
    disk_checkout files(checkout, table);
    const auto reader = files.reader();
    const auto start = clock_type::now();
    const auto templates = cops::compile_templates(reader, settings);
    r.compile_ms = elapsed_ms(start);
    r.templates = templates.size();
    render_sink sink{out, !format_store.empty(), {}};
    if (oracle.empty())
        render_models(r, templates, reader, settings, repetitions, sink);
    else
        render_oracle(r, templates, oracle, repetitions, sink);
    if (format_store.empty())
        return r;

    const auto formatters =
        compass::codegen::codecs::formatters_of(options, checkout);
    const auto formatted = compass::codec::io::format_files(format_store,
        formatters, sink.rendered, threads);
    r.format = formatted.stats;
    if (!out.empty()) {
        const auto& files_out = formatted.files;
        for (std::uint32_t i = 0; i < files_out.size(); ++i) {
            const compass::codec::types::file_id id{i};
            write_file(out / "formatted" /
                std::filesystem::path(files_out.paths[id]),
                files_out.contents[id]);
        }
    }
    return r;
}

void write_render_report(std::ostream& os, const render_result& r) {
    os << "templates: " << r.templates << " compiled in " << r.compile_ms
       << " ms\n";
    os << "contexts: " << r.contexts << " in " << r.setup_ms << " ms\n";
    os << "outputs: " << r.outputs << ", bytes: " << r.bytes << "\n";
    auto sorted = r.render_ms;
    std::ranges::sort(sorted);
    if (!sorted.empty()) {
        os << "render ms: min " << sorted.front() << ", median "
           << sorted[sorted.size() / 2] << ", max " << sorted.back()
           << " over " << sorted.size() << " runs\n";
    }
    if (r.format) {
        const auto& f = *r.format;
        os << "format: " << f.hits << " stored, " << f.formatted
           << " formatted, " << f.unformatted << " unformatted, " << f.runs
           << " formatter processes, " << f.format_ms << " ms\n";
    }
    for (const auto& m : r.messages)
        os << "  " << m << "\n";
}

}
