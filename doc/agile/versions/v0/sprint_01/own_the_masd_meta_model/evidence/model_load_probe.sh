#!/bin/bash
# Loads compass.db's entity models with compass's codegen codec on a scratch
# copy of ORE Studio, the only checkout with a template library, in four
# stages, each a step closer to what ORE Studio's entity loader accepts:
#   as_written  the models unchanged (#+type: masd.codegen.entity);
#   ores_type   the type renamed to ores.codegen.entity;
#   no_profile  also without :profile:, which names profiles ORE lacks;
#   no_subcomp  also with * C++ ** Flags :no_subcomponent: true.
# Each stage diffs every model, one at a time, and prints its first errors.
# Usage: model_load_probe.sh <scratch-dir> <ores.git> <models-dir> [<rev>]
set -uo pipefail
scratch=${1:?scratch directory}
ores_git=${2:?ORE Studio git directory}
models=$(readlink -f "${3:?models directory}")
rev=${4:-main}

mkdir -p "$scratch/ore" "$scratch/models"
git --git-dir="$ores_git" archive "$rev" | tar -x -C "$scratch/ore"
cd "$scratch/ore"
git init -q
git add -A
git -c user.name=probe -c user.email=probe@example.invalid commit -qm base
cp "$models"/compass.db.*.org "$scratch/models/"

target=projects/ores.geo/modeling

stage() {
    for f in "$scratch"/models/*.org; do
        b=$(basename "$f")
        cp "$f" "$target/$b"
        compass-ng -C "$scratch/ore" diff --skip-stale --files \
            --scope "$target/$b" > /dev/null 2> "$scratch/$1.$b.err"
        rc=$?
        echo "$1 $b exit=$rc codegen_writes=$(grep -c '^  write .*(codegen)' \
            "$scratch/$1.$b.err") $(grep -v '^  write \|^already stale\|^scope:' \
            "$scratch/$1.$b.err" | head -3 | cut -c1-200 | tr '\n' ' ')"
        rm "$target/$b"
    done
}

stage as_written
sed -i 's/^#+type: masd.codegen.entity$/#+type: ores.codegen.entity/' \
    "$scratch"/models/*.org
stage ores_type
sed -i '/^:profile: /d' "$scratch"/models/*.org
stage no_profile
sed -i 's/^\* Notes$/** Flags\n:PROPERTIES:\n:no_subcomponent: true\n:END:\n\n* Notes/' \
    "$scratch"/models/*.org
stage no_subcomp
