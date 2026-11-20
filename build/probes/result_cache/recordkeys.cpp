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
#include <cstdio>
#include <filesystem>
#include "compass.codec/io/dependency_file.hpp"
#include "compass.store/ops/blob_hash.hpp"

// Prints one row per unit of each record: worktree, codec, document,
// result key, manifest key.
int main(int argc, char** argv) {
    namespace ct = compass::codec::types;
    for (int a = 1; a < argc; ++a) {
        const std::filesystem::path p(argv[a]);
        const auto r = compass::codec::io::read_dependency_file(p);
        if (!r.problem.empty()) {
            std::fprintf(stderr, "%s: %s\n", argv[a], r.problem.c_str());
            continue;
        }
        const auto wt = p.parent_path().parent_path().filename().string();
        for (std::uint32_t u = 0; u < r.record.size(); ++u) {
            const ct::record_unit_id id{u};
            std::printf("%s\t%.*s\t%.*s\t%s\t%s\n", wt.c_str(),
                int(r.record.codecs[id].size()), r.record.codecs[id].data(),
                int(r.record.documents[id].size()), r.record.documents[id].data(),
                compass::store::ops::to_hex(r.record.results[id]).c_str(),
                compass::store::ops::to_hex(r.record.manifests[id]).c_str());
        }
    }
}
