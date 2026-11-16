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
#include "compass.bench/ops/include_resolve.hpp"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <sys/stat.h>

namespace compass::bench::ops {

namespace {

bool is_regular_file(const std::string& path) {
    struct stat info {};
    return ::stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

class resolver final {
public:
    resolver(const types::file_table& table, const types::search_paths& paths)
        : table_(table), paths_(paths) {
        index_.reserve(table.size());
        for (std::uint32_t id = 0; id < table.size(); ++id)
            index_.emplace(table.paths[id], id);
    }

    std::uint32_t resolve(std::uint32_t includer, types::include_kind kind,
        std::string_view name) {
        if (name.starts_with('/'))
            return is_regular_file(std::string(name))
                ? types::external_target
                : types::unresolved_target;

        const auto context = paths_.file_contexts[includer];
        if (kind == types::include_kind::quoted) {
            const std::string_view path = table_.paths[includer];
            const auto slash = path.rfind('/');
            const auto directory = slash == std::string_view::npos
                ? std::string_view{}
                : path.substr(0, slash);
            if (join_normal(scratch_, directory, name)) {
                const auto it = index_.find(scratch_);
                if (it != index_.end())
                    return it->second;
            }
        }

        key_.assign(reinterpret_cast<const char*>(&context), sizeof context);
        key_.push_back(static_cast<char>(kind));
        key_.append(name);
        const auto cached = cache_.find(key_);
        if (cached != cache_.end())
            return cached->second;
        const auto r = search(paths_.contexts[context], kind, name);
        cache_.emplace(key_, r);
        return r;
    }

private:
    std::uint32_t search(const types::search_context& context,
        types::include_kind kind, std::string_view name) {
        const auto first = kind == types::include_kind::quoted
            ? std::size_t{0}
            : context.first_angle;
        for (auto i = first; i < context.directories.size(); ++i) {
            const auto& directory = paths_.directories[context.directories[i]];
            if (!join_normal(scratch_, directory.path, name))
                continue;
            if (directory.internal) {
                const auto it = index_.find(scratch_);
                if (it != index_.end())
                    return it->second;
            } else if (is_regular_file(scratch_)) {
                return types::external_target;
            }
        }
        return types::unresolved_target;
    }

    const types::file_table& table_;
    const types::search_paths& paths_;
    std::unordered_map<std::string_view, std::uint32_t> index_;
    std::unordered_map<std::string, std::uint32_t, types::string_hash,
        std::equal_to<>> cache_;
    std::string scratch_;
    std::string key_;
};

}

bool join_normal(std::string& out, std::string_view directory,
    std::string_view name) {
    out.assign(directory);
    while (!name.empty()) {
        const auto slash = name.find('/');
        const auto segment = name.substr(0, slash);
        name = slash == std::string_view::npos ? std::string_view{}
                                               : name.substr(slash + 1);
        if (segment.empty() || segment == ".")
            continue;
        if (segment == "..") {
            if (out.empty())
                return false;
            const auto last = out.rfind('/');
            if (last == std::string::npos)
                out.clear();
            else
                out.resize(last == 0 ? 1 : last);
            continue;
        }
        if (!out.empty() && out.back() != '/')
            out.push_back('/');
        out.append(segment);
    }
    return true;
}

void resolve_includes(types::include_table& includes,
    const types::file_table& table, const types::search_paths& paths) {
    resolver r(table, paths);
    includes.targets.resize(includes.size());
    for (std::size_t i = 0; i < includes.size(); ++i)
        includes.targets[i] =
            r.resolve(includes.includers[i], includes.kinds[i],
                includes.name(i));
}

}
