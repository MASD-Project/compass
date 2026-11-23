#!/bin/bash
# Loads compass.serve's operation models with compass's codegen codec on a
# scratch copy of ORE Studio, the only checkout with a template library, in
# stages, each a step closer to what ORE Studio's operation loader accepts:
#   as_written  the models unchanged (#+type: masd.codegen.operation);
#   ores_type   the type renamed to ores.codegen.operation;
#   facets_on   also with ORE Studio's protocol, wire codec and HTTP route
#               facets switched on in the file's drawer, and :http_route: true
#               on each message that states :http_method:.
# Each stage diffs every model, one at a time, prints its first errors and
# keeps the files it would write under <scratch>/<stage>.<model>.out.
# The archive is ORE Studio at one commit:
#   git --git-dir=<ores.git> archive -o <ore.tar> <rev>
# Usage: service_model_load_probe.sh <scratch-dir> <ore.tar> <models-dir>
set -uo pipefail
scratch=${1:?scratch directory}
archive=$(readlink -f "${2:?ORE Studio archive}")
models=$(readlink -f "${3:?models directory}")

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
        compass-ng -C "$scratch/ore" diff --skip-stale --files \
            --scope "$target/$b" > "$scratch/$1.$b.out" 2> "$scratch/$1.$b.err"
        rc=$?
        out="$scratch/$1.$b.out"
        echo "$1 $b exit=$rc codegen_writes=$(grep -c '^  write .*(codegen' \
            "$scratch/$1.$b.err")" \
            "routes_registered=$(grep -o 'routes registered: " << [0-9]*' \
            "$out" | grep -o '[0-9]*$' | paste -sd+ -)" \
            "v1_paths_in_output=$(grep -c '/v1/' "$out")" \
            "mcp_in_output=$(grep -ci 'mcp' "$out")" \
            "$(grep -v '^  write \|^already stale\|^scope:\|^from your' \
            "$scratch/$1.$b.err" | head -3 | cut -c1-60 | tr '\n' ' ')"
        grep '^  write .*(codegen' "$scratch/$1.$b.err" |
            sed 's/ (codegen.*//; s/^/   /'
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
