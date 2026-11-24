#!/usr/bin/env python3
#
# Copyright (C) 2026 Marco Craveiro <marco.craveiro@gmail.com>
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
"""Writes a C++ program that prints, for every generated message and row,
its JSON Schema, its body schema when it has one, and the JSON of samples 0
and 1 and of the default value, one JSON object per line.

usage: json_schema_probe.py <checkout> <probe.cpp>

Compile it against the build's compass.serve, compass.db, compass.store,
compass.platform, yyjson and zlib libraries, run it into a file, and check
that file with json_schema_validate.py.
"""

import pathlib
import re
import sys

if len(sys.argv) != 3:
    raise SystemExit(__doc__)
root = pathlib.Path(sys.argv[1]) / "projects"
includes, types = [], []
for h in sorted((root / "compass.serve/include/compass.serve/messages").glob("*_protocol.hpp")):
    includes.append(f"compass.serve/messages/{h.name}")
    for name in re.findall(r"^struct (\w+) \{", h.read_text(), re.M):
        types.append(f"compass::serve::messages::{name}")
for h in sorted((root / "compass.db/include/compass.db/types").glob("*_row_codec.hpp")):
    includes.append(f"compass.db/types/{h.name}")
    types.append(re.search(r"struct wire_traits<([\w:]+)>", h.read_text()).group(1))

out = ["#include <iostream>", "#include <string>",
       '#include "compass.db/codec/json_schema.hpp"',
       '#include "compass.db/codec/sample.hpp"',
       '#include "compass.db/codec/struct_codec.hpp"']
out += [f'#include "{i}"' for i in includes]
out.append('''
namespace codec = compass::db::codec;

template <typename T>
void dump(const char* name) {
    using traits = codec::wire_traits<T>;
    std::cout << "{\\"type\\":\\"" << name << "\\",\\"schema\\":" << codec::json_schema<T>();
    if constexpr (requires { traits::body_json_schema(); })
        std::cout << ",\\"body_schema\\":" << traits::body_json_schema();
    std::cout << ",\\"docs\\":[" << traits::write_json(codec::sample<T>(0)) << ","
              << traits::write_json(codec::sample<T>(1)) << "," << traits::write_json(T{}) << "]}\\n";
}

int main() {''')
out += [f'    dump<{t}>("{t}");' for t in types]
out.append("}")
pathlib.Path(sys.argv[2]).write_text("\n".join(out) + "\n")
print(len(types), "types")
