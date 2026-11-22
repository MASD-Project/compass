#!/usr/bin/env bash
# Syncs a scratch copy of ORE Studio into a private PostgreSQL cluster and
# prints the counts, times and sizes the task asks for.
# Usage: corpus_sync_ore_studio.sh <compass binary> <scratch dir holding ore/>
set -euo pipefail
compass="$1"
S="$2"
bin=/usr/lib/postgresql/18/bin
pg="$S/pg"
ore="$S/ore"
ore2="$S/ore2"

rm -rf "${pg:?}"
sock=$(mktemp -d /tmp/cpg.XXXXXX)
"$bin/initdb" -D "$pg/data" -U compass -A trust -E UTF8 --locale=C -N > /dev/null
cat >> "$pg/data/postgresql.conf" <<EOF
listen_addresses = ''
unix_socket_directories = '$sock'
EOF
"$bin/pg_ctl" -D "$pg/data" -l "$pg/server.log" -w -s start
trap '"$bin/pg_ctl" -D "$pg/data" -m fast -w -s stop || true; rm -rf "${sock:?}"' EXIT
"$bin/createdb" -h "$sock" -U compass compass
conn="host=$sock user=compass dbname=compass"
psql=("$bin/psql")
[[ -x "${psql[0]}" ]] || psql=(psql)
q() { "${psql[@]}" -h "$sock" -U compass -d compass -At -c "$1"; }

ms() { local s=$1 e=$2; echo $(( (e - s) / 1000000 )); }

echo "== db init"
"$compass" db init -C "$ore" --database "$conn"

echo "== first sync"
s=$(date +%s%N); "$compass" sync -C "$ore" --timings; e=$(date +%s%N)
echo "wall: $(ms "$s" "$e") ms"

echo "== no-change sync (3 runs)"
for _ in 1 2 3; do
    s=$(date +%s%N); "$compass" sync -C "$ore"; e=$(date +%s%N)
    echo "wall: $(ms "$s" "$e") ms"
done

echo "== counts after the first environment"
q "SELECT 'documents', count(*) FROM corpus.document UNION ALL
   SELECT 'headings', count(*) FROM corpus.heading UNION ALL
   SELECT 'sections', count(*) FROM corpus.section UNION ALL
   SELECT 'links', count(*) FROM corpus.link UNION ALL
   SELECT 'id links', count(*) FROM corpus.link WHERE target_id IS NOT NULL UNION ALL
   SELECT 'scenarios', count(*) FROM test.scenario UNION ALL
   SELECT 'steps', count(*) FROM test.step UNION ALL
   SELECT 'placements', count(*) FROM corpus.placement"
q "SELECT 'database size', pg_size_pretty(pg_database_size('compass'))"
q "SELECT 'corpus.section size', pg_size_pretty(pg_total_relation_size('corpus.section'))"
q "SELECT 'corpus.document size', pg_size_pretty(pg_total_relation_size('corpus.document'))"

echo "== one-file sync through the edit hook"
target=doc/agile/versions/v0/sprint_30/see_the_market_as_vendor_quotes/scenario_watch_the_vendors_quotes.org
printf '\nA line the evidence adds.\n' >> "$ore/$target"
payload="{\"session_id\": \"s\", \"transcript_path\": \"/t.jsonl\", \"cwd\": \"$ore\", \"permission_mode\": \"default\", \"hook_event_name\": \"PostToolUse\", \"tool_name\": \"Edit\", \"tool_input\": {\"file_path\": \"$ore/$target\", \"old_string\": \"a\", \"new_string\": \"b\"}, \"tool_response\": {\"filePath\": \"$ore/$target\", \"type\": \"update\"}, \"tool_use_id\": \"toolu_1\"}"
for _ in 1 2 3; do
    printf 'x' >> "$ore/$target"
    echo "$payload" | "$compass" hook --timings 2>&1 >/dev/null | grep -E 'synced|step (load|write)' || true
done
q "SELECT 'documents after three edits', count(*) FROM corpus.document"

echo "== a second environment: a worktree with one file edited"
git -C "$ore" checkout -q -- "$target"
git -C "$ore" worktree add -q "$ore2" HEAD
printf '\nEdited in the second environment.\n' >> "$ore2/doc/agile/versions/v0/sprint_30/see_the_market_as_vendor_quotes/story.org"
before_docs=$(q "SELECT count(*) FROM corpus.document")
before_sections=$(q "SELECT count(*) FROM corpus.section")
s=$(date +%s%N); "$compass" sync -C "$ore2"; e=$(date +%s%N)
echo "wall: $(ms "$s" "$e") ms"
echo "documents: $before_docs -> $(q "SELECT count(*) FROM corpus.document")"
echo "sections: $before_sections -> $(q "SELECT count(*) FROM corpus.section")"
q "SELECT e.name, count(*) FROM corpus.placement p JOIN corpus.environment e ON e.id = p.environment GROUP BY e.name ORDER BY e.name"
q "SELECT 'database size', pg_size_pretty(pg_database_size('compass'))"

echo "== collection after the second worktree is gone"
git -C "$ore" worktree remove --force "$ore2"
"$compass" db prune -C "$ore"
q "SELECT 'documents', count(*) FROM corpus.document UNION ALL SELECT 'sections', count(*) FROM corpus.section UNION ALL SELECT 'environments', count(*) FROM corpus.environment"
"$compass" db status -C "$ore"
