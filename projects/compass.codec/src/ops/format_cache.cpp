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
#include "compass.codec/ops/format_cache.hpp"

#include "compass.store/ops/blob_hash.hpp"

namespace compass::codec::ops {

namespace {

using compass::store::types::blob_id;

void put(std::string& out, std::uint64_t v) {
    for (int shift = 0; shift < 64; shift += 8)
        out.push_back(static_cast<char>((v >> shift) & 0xFF));
}

void put_text(std::string& out, std::string_view text) {
    put(out, text.size());
    out += text;
}

void put(std::string& out, const blob_id& id) {
    out.append(reinterpret_cast<const char*>(id.bytes.data()),
        id.bytes.size());
}

blob_id sha1(std::string_view bytes) {
    return {compass::store::ops::sha1_of(bytes)};
}

std::string_view suffix_of(std::string_view path) {
    const auto slash = path.rfind('/');
    const auto name = slash == std::string_view::npos ? path
                                                      : path.substr(slash + 1);
    const auto dot = name.rfind('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 == name.size())
        return {};
    return name.substr(dot);
}

}

std::optional<std::size_t> formatter_of(
    std::span<const types::formatter> formatters, std::string_view path) {
    const auto suffix = suffix_of(path);
    if (suffix.empty())
        return std::nullopt;
    for (std::size_t i = 0; i < formatters.size(); ++i) {
        for (const auto& e : formatters[i].extensions) {
            if (e == suffix)
                return i;
        }
    }
    return std::nullopt;
}

blob_id formatter_key(const types::formatter& f, std::string_view version,
    const blob_id& style) {
    std::string bytes;
    put_text(bytes, "compass.format.formatter");
    put(bytes, format_cache_version);
    put_text(bytes, f.name);
    put_text(bytes, version);
    put(bytes, f.arguments.size());
    for (const auto& a : f.arguments)
        put_text(bytes, a);
    put(bytes, f.passes);
    put(bytes, style);
    return sha1(bytes);
}

blob_id formatted_key(const blob_id& formatter, std::string_view path,
    const blob_id& raw) {
    std::string bytes;
    put_text(bytes, "compass.format.output");
    put(bytes, formatter);
    put_text(bytes, path);
    put(bytes, raw);
    return sha1(bytes);
}

blob_id program_key(std::string_view path, std::uint64_t size,
    std::int64_t mtime_ns) {
    std::string bytes;
    put_text(bytes, "compass.format.program");
    put(bytes, format_cache_version);
    put_text(bytes, path);
    put(bytes, size);
    put(bytes, static_cast<std::uint64_t>(mtime_ns));
    return sha1(bytes);
}

std::vector<std::string> format_command(const types::formatter& f,
    std::span<const std::string> files) {
    std::vector<std::string> command;
    command.reserve(1 + f.arguments.size() + files.size());
    command.push_back(f.program.string());
    const auto style = f.style.string();
    for (auto a : f.arguments) {
        for (auto at = a.find(types::style_placeholder);
             at != std::string::npos;
             at = a.find(types::style_placeholder, at + style.size()))
            a.replace(at, types::style_placeholder.size(), style);
        command.push_back(std::move(a));
    }
    command.insert(command.end(), files.begin(), files.end());
    return command;
}

}
