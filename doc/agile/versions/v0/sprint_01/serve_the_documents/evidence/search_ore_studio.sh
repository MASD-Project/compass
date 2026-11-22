#!/usr/bin/env bash
# Syncs a scratch copy of ORE Studio into a private PostgreSQL cluster, scores
# search against the recorded questions under each ranking tried, measures
# latency with the server's buffers cold and warm, and logs the plan of every
# search statement through auto_explain.
# Usage: search_ore_studio.sh <compass bin dir> <scratch dir holding ore/>
set -euo pipefail
bin_dir="$1"
S="$2"
here=$(cd "$(dirname "$0")" && pwd)
questions="$here/search_questions.tsv"
pgbin=/usr/lib/postgresql/18/bin
pg="$S/pg"
ore="$S/ore"

rm -rf "${pg:?}"
sock=$(mktemp -d /tmp/cpg.XXXXXX)
"$pgbin/initdb" -D "$pg/data" -U compass -A trust -E UTF8 --locale=C -N > /dev/null
cat >> "$pg/data/postgresql.conf" <<EOF
listen_addresses = ''
unix_socket_directories = '$sock'
session_preload_libraries = 'auto_explain'
EOF
start() { "$pgbin/pg_ctl" -D "$pg/data" -l "$pg/server.log" -w -s start; }
stop() { "$pgbin/pg_ctl" -D "$pg/data" -m fast -w -s stop; }
start
trap 'stop || true; rm -rf "${sock:?}"' EXIT
"$pgbin/createdb" -h "$sock" -U compass compass
conn="host=$sock user=compass dbname=compass"
q() { "$pgbin/psql" -h "$sock" -U compass -d compass -At -c "$1"; }

if [[ ! -d "$ore/.git" ]]; then
    git -C "$ore" init -q
    git -C "$ore" add -A
    git -C "$ore" -c user.name=evidence -c user.email=evidence@localhost \
        commit -q -m snapshot
fi

echo "== sync"
"$bin_dir/compass" db init -C "$ore" --database "$conn"
"$bin_dir/compass" sync -C "$ore"
q "VACUUM ANALYZE"
q "SELECT 'documents', count(*) FROM corpus.document UNION ALL
   SELECT 'sections', count(*) FROM corpus.section UNION ALL
   SELECT 'documents with a state', count(state) FROM corpus.document"
q "SELECT type, count(*) FROM corpus.document GROUP BY type
   ORDER BY 2 DESC LIMIT 12"

bench() { "$bin_dir/compass-bench" search "$conn" --environment ore \
    --questions "$questions" "$@"; }

# Each ranking runs after a restart, so its first run of each query finds the
# server's buffers cold; the files stay in the kernel's page cache.
for ranking in "--normalization 0" "--normalization 1" "--normalization 2"; do
    echo "== ranking: $ranking"
    stop; start
    # shellcheck disable=SC2086
    bench --repeat 5 $ranking
done

explain() { q "EXPLAIN (ANALYZE, BUFFERS, COSTS OFF) $1"; }
echo "== plan of a typical search, from auto_explain"
: > "$pg/server.log"
q "ALTER SYSTEM SET auto_explain.log_min_duration = 0"
q "ALTER SYSTEM SET auto_explain.log_analyze = on"
q "ALTER SYSTEM SET auto_explain.log_buffers = on"
q "SELECT pg_reload_conf()" > /dev/null
printf 'developer\tnats subject naming\tX\n' > "$S/one.tsv"
bench --questions "$S/one.tsv" --repeat 1 > /dev/null
awk '/Query Text: WITH q AS/ {n++} n == 1' "$pg/server.log" |
    sed -n '/Query Parameters/,/ LOG: /p' | grep -v ' LOG: '
sprint=$(q "SELECT org_id FROM corpus.document
    WHERE type = 'sprint' AND title = 'Sprint 30'")
echo "== plan of the sprint filter, Sprint 30"
explain "SELECT count(*) FROM corpus.placement p JOIN corpus.document d
    ON d.hash = p.hash WHERE p.environment = 1
    AND d.hash IN (SELECT corpus.sprint_members(1, '$sprint'))"
echo "== plan of the tag and keyword filters"
explain "SELECT count(*) FROM corpus.placement p JOIN corpus.document d
    ON d.hash = p.hash WHERE p.environment = 1
    AND d.tags @> ARRAY['nats'::text]
    AND d.keywords @> jsonb_build_object('type', jsonb_build_array('knowledge'))"
echo "== plan of the sprint facet over every document"
explain "SELECT s.org_id, count(*) FROM (SELECT DISTINCT d.org_id
    FROM corpus.document d JOIN corpus.placement p ON p.hash = d.hash
    WHERE d.type = 'sprint' AND p.environment = 1) s
    JOIN LATERAL corpus.sprint_members(1, s.org_id) m(hash) ON true
    JOIN corpus.placement p ON p.hash = m.hash AND p.environment = 1
    GROUP BY s.org_id"
