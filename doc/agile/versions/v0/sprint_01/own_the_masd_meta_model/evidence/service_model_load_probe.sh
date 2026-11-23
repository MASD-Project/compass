#!/bin/bash
# Loads compass.serve's operation models with compass's codegen codec on a
# scratch copy of ORE Studio, the only checkout with a template library, in
# stages, each a step closer to what ORE Studio's operation loader accepts:
#   as_written  the models unchanged (#+type: masd.codegen.operation);
#   ores_type   the type renamed to ores.codegen.operation;
#   facets_on   also with ORE Studio's protocol, wire codec and HTTP route
#               facets switched on in the file's drawer, and :http_route: true
#               on each message that states :http_method:.
# Each stage diffs every model, one at a time, prints compass-ng's whole
# stderr and keeps the files it would write under <scratch>/<stage>.<model>.out.
# compass-ng diff exits 0 when nothing would change, 1 when it finds a change
# and 2 on failure. Where a stage writes a protocol header, the struct check
# compares the header's structs with the models' messages.
# The archive is ORE Studio at one commit:
#   git --git-dir=<ores.git> archive -o <ore.tar> <rev>
# Usage: service_model_load_probe.sh <scratch-dir> <ore.tar> <compass root>
set -uo pipefail
mkdir -p "${1:?scratch directory}"
scratch=$(readlink -f "$1")
archive=$(readlink -f "${2:?ORE Studio archive}")
compass=$(readlink -f "${3:?compass repository root}")
models=$compass/projects/compass.serve/modeling
check=$compass/doc/agile/versions/v0/sprint_01/own_the_masd_meta_model/evidence/service_model_struct_check.py

mkdir -p "$scratch/ore" "$scratch/models"
tar -x -C "$scratch/ore" -f "$archive"
cd "$scratch/ore" || exit 2
git init -q
git add -A
git -c user.name=probe -c user.email=probe@example.invalid commit -qm base
cp "$models"/compass.serve.*.org "$scratch/models/"

target=projects/ores.geo/modeling

stage() {
    for f in "$scratch"/models/*.org; do
        b=$(basename "$f")
        cp "$f" "$target/$b"
        out="$scratch/$1.$b.out"
        err="$scratch/$1.$b.err"
        compass-ng -C "$scratch/ore" diff --skip-stale --files \
            --scope "$target/$b" > "$out" 2> "$err"
        rc=$?
        echo "$1 $b exit=$rc" \
            "routes_registered=$(grep -o 'routes registered: " << [0-9]*' \
            "$out" | grep -o '[0-9]*$' | paste -sd+ -)" \
            "v1_paths_in_output=$(grep -c '/v1/' "$out")" \
            "mcp_in_output=$(grep -ci 'mcp' "$out")"
        sed "s|$scratch|<scratch>|g; s/^/    stderr: /; s/ *\$//" "$err"
        header=$(grep -o '^+++ b/.*_protocol\.hpp$' "$out" | sed 's|^+++ b/||')
        if [ -n "$header" ]; then
            hpp="$scratch/$1.$(basename "$header")"
            awk -v h="+++ b/$header" '
                $0 == h { on = 1; next }
                on && /^(diff |--- |\+\+\+ )/ { on = 0 }
                on && /^\+/ { print substr($0, 2) }' "$out" > "$hpp"
            python3 -I "$check" "$compass" --header "$hpp" |
                sed 's/^/    struct check: /'
            echo "    struct check exit=${PIPESTATUS[0]}"
        fi
        rm "$target/$b"
    done
}

stage as_written
sed -i 's/^#+type: masd.codegen.operation$/#+type: ores.codegen.operation/' \
    "$scratch"/models/*.org
stage ores_type
sed -i '0,/^:END:$/s//:ores.cpp.protocol.enabled: true\n:ores.cpp.wire-codec.enabled: true\n:ores.cpp.http-route.enabled: true\n:END:/' \
    "$scratch"/models/*.org
sed -i 's/^:http_method: \(.*\)$/:http_method: \1\n:http_route:  true/' \
    "$scratch"/models/*.org
stage facets_on
