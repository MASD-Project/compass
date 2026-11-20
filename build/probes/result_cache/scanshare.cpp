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
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include "compass.codec/ops/action_cache.hpp"
#include "compass.store/io/pack_store.hpp"
#include "compass.store/ops/blob_hash.hpp"

// Reads units.tsv (worktree, codec, document, result, manifest) and, for
// each corpus unit, decodes its live result: bytes of files, findings and
// scan table; and for scan tables, the entries, the fact bytes and how many
// distinct (path, content) pairs the tables of all worktrees hold.
int main(int argc, char** argv) {
    namespace st = compass::store::types;
    const compass::store::io::pack_reader packs(argv[2]);
    std::ifstream in(argv[1]);
    struct tally { int units = 0; std::uint64_t total = 0, files = 0, diags = 0,
        scan = 0, entries = 0, facts = 0, findings = 0, paths = 0;
        std::set<std::string> distinct; };
    std::map<std::string, tally> t;
    for (std::string line; std::getline(in, line);) {
        std::istringstream s(line);
        std::string wt, codec, doc, res, man;
        std::getline(s, wt, '\t'); std::getline(s, codec, '\t');
        std::getline(s, doc, '\t'); std::getline(s, res, '\t');
        if (!doc.empty()) continue;
        if (res.size() != 40) continue;
        st::blob_id key;
        for (int i = 0; i < 20; ++i)
            key.bytes[i] = static_cast<std::uint8_t>(std::stoi(res.substr(2 * i, 2), nullptr, 16));
        const auto body = packs.read_action(st::action_kind::result, key);
        if (!body) continue;
        const auto r = compass::codec::ops::decode_result(*body);
        if (!r) continue;
        auto& x = t[codec];
        ++x.units; x.total += body->size();
        x.diags += r->diagnostics.size(); x.scan += r->scan.size();
        if (auto table = compass::codec::ops::decode_scan(r->scan)) {
            for (const auto& [path, e] : table->entries) {
                ++x.entries; x.facts += e.fact.size(); x.findings += e.findings.size();
                x.paths += path.size();
                x.distinct.insert(std::string(path) + compass::store::ops::to_hex(e.content));
            }
        }
    }
    std::printf("codec\tunits\tMB/result\tscan%%\tentries/result\tfact KB/result\tfindings KB/result\tpath KB/result\tdistinct entries\n");
    for (const auto& [c, x] : t) {
        if (x.scan == 0) continue;
        std::printf("%s\t%d\t%.2f\t%.0f\t%llu\t%.0f\t%.0f\t%.0f\t%zu\n", c.c_str(), x.units,
            x.total / 1e6 / x.units, 100.0 * x.scan / x.total,
            (unsigned long long)(x.entries / x.units), x.facts / 1e3 / x.units,
            x.findings / 1e3 / x.units, x.paths / 1e3 / x.units, x.distinct.size());
    }
}
