#!/bin/bash
# Renders the two probe models with compass's codegen codec on a scratch copy
# of ORE Studio. Usage: render_probe.sh <scratch-dir> <ores.git> [<rev>]
set -euo pipefail
scratch=${1:?scratch directory}
ores_git=${2:?ORE Studio git directory}
rev=${3:-main}
evidence=$(dirname "$(readlink -f "$0")")

mkdir -p "$scratch/ore"
git --git-dir="$ores_git" archive "$rev" | tar -x -C "$scratch/ore"
cd "$scratch/ore"
git init -q
git add -A
git -c user.name=probe -c user.email=probe@example.invalid commit -qm base

for model in document run; do
    cp "$evidence/ores.geo.$model.org.txt" \
        "projects/ores.geo/modeling/ores.geo.$model.org"
    compass-ng -C "$scratch/ore" diff --skip-stale --files \
        --scope "projects/ores.geo/modeling/ores.geo.$model.org" \
        > "$scratch/$model.diff" 2> "$scratch/$model.err" || true
    grep "ores.geo.$model.org" "$scratch/$model.err" | sed -n 's/^  write //p'
done
