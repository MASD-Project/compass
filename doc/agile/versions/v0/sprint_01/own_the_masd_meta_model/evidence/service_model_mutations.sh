#!/bin/bash
# Proves the two service model checks catch what they claim. Each mutation
# edits a scratch copy of the models and compass.db's headers; it must change
# the copy and make at least one check exit 1. The mutations are the ten the
# review of PR #193 ran against the first struct check, adapted to the models
# without tester, and the binding rules of the component overview.
# Usage: service_model_mutations.sh <repository root> <scratch dir>
# Exits 1 when a mutation changes nothing or no check fails on it.
set -uo pipefail
root=$(readlink -f "${1:?repository root}")
mkdir -p "${2:?scratch directory}"
scratch=$(readlink -f "$2")
evidence=$root/doc/agile/versions/v0/sprint_01/own_the_masd_meta_model/evidence
models=projects/compass.serve/modeling
types=projects/compass.db/include/compass.db/types
query=projects/compass.db/include/compass.db/query
cw=$models/compass.serve.campaign_writes.org
dq=$models/compass.serve.document_queries.org
tq=$models/compass.serve.testing_queries.org

copy() {
    rm -rf "$1"
    mkdir -p "$1/$types" "$1/$query" "$1/$models"
    cp "$root/$types"/*.hpp "$1/$types/"
    cp "$root/$query"/*.hpp "$1/$query/"
    cp "$root/$models"/*.org "$1/$models/"
}

checks() {
    python3 -I "$evidence/service_model_struct_check.py" "$1" > "$1.struct"
    s=$?
    python3 -I "$evidence/service_model_binding_check.py" "$1" > "$1.binding"
    b=$?
    echo "struct=$s binding=$b"
}

copy "$scratch/base"
echo "unmutated: $(checks "$scratch/base")"

survivors=0
mut() {
    copy "$scratch/m"
    (cd "$scratch/m" && eval "$2")
    if diff -rq "$scratch/base" "$scratch/m" > /dev/null; then
        echo "NOOP     $1"
        survivors=$((survivors + 1))
        return
    fi
    result=$(checks "$scratch/m")
    if [ "$result" = "struct=0 binding=0" ]; then
        echo "SURVIVES $1: $result"
        survivors=$((survivors + 1))
    else
        echo "caught   $1: $result"
        grep -h '^FAIL' "$scratch/m.struct" "$scratch/m.binding" |
            head -2 | cut -c1-150 | sed 's/^/           /'
    fi
}

mut "model: a request's std::optional<std::int64_t> becomes std::int64_t" \
    "sed -i '0,/std::optional<std::int64_t>/s//std::int64_t/' $cw"
mut "model: search's limit default 20 becomes 25" \
    "sed -i 's/^:default:  20\$/:default:  25/' $dq"
mut "model: search_hit renamed search_hitt" \
    "sed -i 's/^\*\* search_hit\$/** search_hitt/; s/vector<search_hit>/vector<search_hitt>/' $dq"
mut "model: run_step deleted" \
    "sed -i '/^\*\* run_step\$/,/^\*\* start_run_response\$/{/^\*\* start_run_response\$/!d}' $cw"
mut "c++: a new struct export_request with no message" \
    "sed -i 's/^struct finish_run_request {/struct export_request {\n    std::int64_t campaign = 0;\n};\n\nstruct finish_run_request {/' $types/campaign.hpp"
mut "model: :values: of step_outcome loses two enumerators" \
    "sed -i 's/^:values:   pass fail blocked skipped/:values:   pass fail/' $cw"
mut "c++: enum step_outcome gains an enumerator" \
    "sed -i 's/enum class step_outcome { pass, fail, blocked, skipped }/enum class step_outcome { pass, fail, blocked, skipped, waived }/' $types/*.hpp"
mut "model: start_run_request reads tester from the body" \
    "sed -i '0,/^The scenario.s =:ID:=.\$/s//&\n\n*** tester\n:PROPERTIES:\n:cpp_type: std::string\n:http_in:  body\n:END:/' $cw"
mut "model: finish_run loses its :response:" \
    "sed -i '/^:response:    finish_run_response/d' $cw"
mut "c++: members of metadata_match swap order" \
    "sed -i '/^struct metadata_match/,/^};/{s/name;/TMP;/; s/value;/name;/; s/TMP;/value;/}' $types/search.hpp"
mut "c++: the structs drop tester ahead of the pins" \
    "sed -i '/^struct start_run_request {/,/^};/{/std::string tester;/d}' $types/campaign.hpp"
mut "model: a read tool named without a read prefix" \
    "sed -i 's/^:tool:        list_documents\$/:tool:        documents/' $dq"
mut "model: a write tool named as a read" \
    "sed -i 's/^:tool:        start_run\$/:tool:        get_started_run/' $cw"
mut "model: an area compass does not publish" \
    "sed -i 's/^:area:        agile\$/:area:        sprints/' $tq"
mut "model: a GET reads a body field" \
    "sed -i '0,/^:http_in:  query\$/s//:http_in:  body/' $dq"
mut "model: a path placeholder with no path field" \
    "sed -i 's|^:http_path:   /v1/{environment}/notes/{note}\$|:http_path:   /v1/{environment}/notes/{id}|' $cw"
mut "model: a write asks only for a view" \
    "sed -i '0,/^:caller:      subject\$/s//:caller:      view/' $cw"
mut "model: a write states :destructive: false, which the loader refuses" \
    "sed -i '0,/^:writes:      true\$/s//&\n:destructive: false/' $cw"
mut "model: a PUT loses :idempotent:" \
    "sed -i '0,/^:idempotent:  true\$/{//d}' $cw"
mut "model: a read states :idempotent:" \
    "sed -i '0,/^:writes:      false\$/s//&\n:idempotent:  true/' $dq"
mut "model: the screenshot bytes lose :encoding:" \
    "sed -i '/^:encoding: base64\$/d' $cw"
mut "model: a route takes a path outside {environment}" \
    "sed -i 's|^:http_path:   /v1/{environment}/runs\$|:http_path:   /v1/runs|' $cw"
mut "model: two routes share a tool" \
    "sed -i 's/^:tool:        list_notes\$/:tool:        get_run/' $tq"
mut "model: error_response loses a code" \
    "sed -i 's/ too_large internal\$/ internal/' $models/compass.serve.errors.org"

echo "survivors=$survivors"
[ "$survivors" -eq 0 ]
