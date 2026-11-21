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
#include "compass.cli/app/env.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "compass.cli/app/application.hpp"
#include "compass.cli/config/check_documents.hpp"
#include "compass.codec/ops/data_tables.hpp"
#include "compass.env/ops/format.hpp"
#include "compass.env/ops/generate.hpp"
#include "compass.env/ops/versions.hpp"
#include "compass.platform/filesystem/new_file.hpp"
#include "compass.platform/process/environment.hpp"
#include "compass.platform/random/secure_random.hpp"

namespace compass::cli::app {

namespace {

namespace eo = compass::env::ops;
namespace et = compass::env::types;
namespace fs = std::filesystem;

constexpr std::string_view alphanumeric =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

std::optional<std::string> read_text(const fs::path& p) {
    std::error_code ec;
    if (!fs::is_regular_file(p, ec))
        return std::nullopt;
    std::ifstream is(p, std::ios::binary);
    if (!is)
        return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(is), {});
}

/**
 * @brief Writes @p text to @p path through a file beside it, created
 * readable by its owner alone, that then replaces it in one step.
 */
void write_private(const fs::path& path, std::string_view text) {
    auto temporary = path;
    temporary += ".compass-new";
    std::error_code ec;
    fs::remove(temporary, ec);
    auto file = platform::filesystem::new_file::create(temporary, true);
    if (!file)
        throw std::runtime_error("cannot create " + temporary.string());
    file->write(std::as_bytes(std::span(text.data(), text.size())));
    file->close();
    platform::filesystem::replace_file(temporary, path);
}

/**
 * @brief The format the check documents of @p checkout give, the path of
 * the document, or the reason, printed to @p err, there is none.
 */
struct loaded {
    et::env_format format;
    std::string path;
};

std::optional<loaded> load(const fs::path& checkout, std::ostream& err) {
    const auto checks = config::read_check_documents(checkout);
    if (!checks) {
        err << "compass: " << checks.error() << "\n";
        return std::nullopt;
    }
    const auto it = std::ranges::find_if(checks->data, [](const auto& d) {
        return std::ranges::contains(d.codecs, eo::codec_name);
    });
    if (it == checks->data.end()) {
        err << "compass: " << codec::ops::tables_for(eo::codec_name, "", {})
            .error() << "\n";
        return std::nullopt;
    }
    const auto tables = codec::ops::decode_data_tables(it->text);
    if (!tables) {
        err << "compass: " << it->path << ": " << tables.error() << "\n";
        return std::nullopt;
    }
    auto format = eo::read_format(*tables);
    if (!format) {
        const auto& f = format.error();
        err << "compass: " << it->path << ":" << f.line << ": " << f.message
            << "; fix: " << f.fix << "\n";
        return std::nullopt;
    }
    return loaded{std::move(*format), it->path};
}

std::optional<std::vector<et::service>> services_of(const fs::path& checkout,
    const et::env_format& f, std::ostream& err) {
    if (f.service_registry.empty())
        return std::vector<et::service>{};
    const auto text = read_text(checkout / f.service_registry);
    if (!text) {
        err << "compass: cannot read the service registry " <<
            f.service_registry << "\n";
        return std::nullopt;
    }
    return eo::read_services(*text);
}

int run_generate(const types::invocation& i, const et::env_format& f,
    std::ostream& out, std::ostream& err) {
    const auto services = services_of(i.checkout, f, err);
    if (!services)
        return exit_failure;
    const auto relative = i.env.file.empty() ? f.file : i.env.file;
    const auto path = i.checkout / relative;
    const auto old = read_text(path);
    eo::sources s;
    s.inputs["checkout"] = i.checkout.generic_string();
    s.inputs["home"] = platform::process::home_directory().generic_string();
    s.inputs["now"] = std::format("{:%Y-%m-%d %H:%M:%S} UTC",
        std::chrono::floor<std::chrono::seconds>(
            std::chrono::system_clock::now()));
    for (const auto& [name, value] : i.env.inputs)
        s.inputs[name] = value;
    for (const auto& [name, value] : i.env.variables)
        s.overrides[name] = value;
    if (old)
        s.existing = eo::read_env(*old);
    s.environment = [](const std::string& name) {
        return platform::process::environment_variable(name);
    };
    s.read_file = [&](const std::string& p) {
        const fs::path file(p);
        return read_text(file.is_absolute() ? file : i.checkout / file);
    };
    s.secret = [](std::size_t length) {
        return platform::random::secure_random_string(length, alphanumeric);
    };
    const auto g = eo::generate(f, *services, s);
    if (!g) {
        err << "compass: env generate: " << g.error() << "\n";
        return exit_failure;
    }
    if (old) {
        auto backup = path;
        backup += ".old";
        write_private(backup, *old);
    }
    write_private(path, g->text);
    out << "compass: wrote " << relative << ": version " << f.current()
        << ", " << g->names.size() << " variables\n";
    return exit_success;
}

void write_names(std::ostream& out, std::string_view what,
    const std::vector<std::string>& names) {
    if (names.empty())
        return;
    out << "  " << what << ":";
    for (const auto& n : names)
        out << " " << n;
    out << "\n";
}

int run_status(const types::invocation& i, const et::env_format& f,
    std::ostream& out, std::ostream& err) {
    const auto relative = i.env.file.empty() ? f.file : i.env.file;
    const auto text = read_text(i.checkout / relative);
    const auto regenerate = f.regenerate.empty()
        ? std::string("compass env generate") : f.regenerate;
    if (!text) {
        out << relative << ": none; generate it: " << regenerate << "\n";
        return exit_findings;
    }
    const auto services = services_of(i.checkout, f, err);
    if (!services)
        return exit_failure;
    const auto s = eo::status_of(f, *services, eo::read_env(*text));
    if (!s.stale()) {
        out << relative << ": current, version " << s.current << "\n";
        return exit_success;
    }
    out << relative << ": stale\n  version: ";
    if (!s.version)
        out << "no " << f.version_variable;
    else
        out << *s.version;
    out << " of " << s.current << "\n";
    write_names(out, "missing", s.missing);
    write_names(out, "removed", s.removed);
    write_names(out, "undeclared", s.undeclared);
    out << "  regenerate: " << regenerate << "\n";
    return exit_findings;
}

int run_version_new(const types::invocation& i, const loaded& l,
    std::ostream& out, std::ostream& err) {
    const auto path = i.checkout / l.path;
    const auto text = read_text(path);
    if (!text) {
        err << "compass: cannot read " << l.path << "\n";
        return exit_failure;
    }
    const auto today = i.today.value_or(std::chrono::year_month_day{
        std::chrono::floor<std::chrono::days>(
            std::chrono::system_clock::now())});
    const auto date = std::format("{:%Y-%m-%d}",
        std::chrono::sys_days{today});
    const auto updated = eo::append_version(*text, l.format,
        {.description = i.env.description, .adds = i.env.adds,
            .removes = i.env.removes, .format_only = i.env.format_only,
            .date = date});
    if (!updated) {
        err << "compass: env version new: " << updated.error() << "\n";
        return exit_failure;
    }
    std::ofstream os(path, std::ios::binary | std::ios::trunc);
    os << *updated;
    os.close();
    if (!os) {
        err << "compass: cannot write " << l.path << "\n";
        return exit_failure;
    }
    out << "compass: recorded version " << l.format.current() + 1 << " ("
        << date << ") in " << l.path << ": " << i.env.description << "\n";
    return exit_success;
}

}

int run_env(const types::invocation& i, std::ostream& out,
    std::ostream& err) {
    const auto l = load(i.checkout, err);
    if (!l)
        return exit_failure;
    switch (i.env.action) {
    case types::env_action::generate:
        return run_generate(i, l->format, out, err);
    case types::env_action::version:
        out << l->format.current() << "\n";
        return exit_success;
    case types::env_action::version_new:
        return run_version_new(i, *l, out, err);
    case types::env_action::status:
        return run_status(i, l->format, out, err);
    }
    return exit_failure;
}

}
