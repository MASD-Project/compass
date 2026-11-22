#!/usr/bin/env bash
# Syncs a scratch copy of ORE Studio into a private PostgreSQL cluster, on a
# Unix socket with no TCP port, and asks what needs testing in its present
# sprint and in an earlier one. Then it writes a few activity rows by hand, as
# the campaign writes of the next task will, and asks again, so the states
# move: a pass, a failure after a pass, a block, a pass against an earlier
# definition, an unfinished run and a failure in another environment after a
# pass, which leave it passed, and open and addressed notes.
# Usage: testing_ore_studio.sh <compass bin dir> <scratch dir holding ore/>
set -euo pipefail
bin_dir="$1"
S="$2"
pgbin=/usr/lib/postgresql/18/bin
pg="$S/pg"
ore="$S/ore"

rm -rf "${pg:?}"
sock=$(mktemp -d /tmp/cpg.XXXXXX)
"$pgbin/initdb" -D "$pg/data" -U compass -A trust -E UTF8 --locale=C -N > /dev/null
cat >> "$pg/data/postgresql.conf" <<EOF
listen_addresses = ''
unix_socket_directories = '$sock'
EOF
"$pgbin/pg_ctl" -D "$pg/data" -l "$pg/server.log" -w -s start
trap '"$pgbin/pg_ctl" -D "$pg/data" -m fast -w -s stop || true; rm -rf "${sock:?}"' EXIT
"$pgbin/createdb" -h "$sock" -U compass compass
conn="host=$sock user=compass dbname=compass"
q() { "$pgbin/psql" -h "$sock" -U compass -d compass -At -v ON_ERROR_STOP=1 -c "$1"; }

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
q "SELECT 'scenarios', count(*) FROM test.scenario UNION ALL
   SELECT 'scenarios with a story', count(story_id) FROM test.scenario"
echo "== sprints whose state is STARTED"
q "SELECT d.title, d.created, d.org_id FROM corpus.document d
   WHERE d.type = 'sprint' AND d.state = 'STARTED' ORDER BY d.created"

testing() { "$bin_dir/compass-bench" testing "$conn" --environment ore "$@"; }

echo "== what needs testing, present sprint, before any run"
testing --repeat 5
echo "== what needs testing, Sprint 28 by title"
testing --sprint "Sprint 28" --repeat 1 | sed -n '1,7p'
echo "== Sprint 30 by directory name"
testing --sprint sprint_30 --repeat 1 | sed -n '1p'

# The scenarios of the present sprint, four in ORE Studio 9ed8b54cd2, in the
# order the report lists them.
mapfile -t picked < <(q "WITH s AS (
    SELECT d.org_id FROM corpus.document d
    WHERE d.type = 'sprint' AND d.state = 'STARTED'
    ORDER BY d.created DESC LIMIT 1)
  SELECT sc.org_id || '|' || sc.story_id
  FROM s, corpus.sprint_members(1, s.org_id) m(hash)
  JOIN corpus.document st ON st.hash = m.hash AND st.type = 'story'
  JOIN corpus.placement sp ON sp.hash = st.hash
  JOIN test.scenario sc ON sc.story_id = st.org_id
  JOIN corpus.placement p ON p.hash = sc.hash
  ORDER BY sp.path, p.path LIMIT 4")

# run <scenario|story> <environment> <outcome or empty> [<definition hex>]
run() {
    local scenario=${1%%|*} story=${1##*|}
    q "INSERT INTO activity.campaign (environment, story_id)
       VALUES ('$2', '$story') ON CONFLICT DO NOTHING" > /dev/null
    q "INSERT INTO activity.run (campaign, scenario_id, definition,
         scenario_hash, tester, finished_at, outcome)
       SELECT c.id, s.org_id,
              coalesce(decode(nullif('${4:-}', ''), 'hex'), s.definition),
              s.hash, 'evidence',
              CASE WHEN '$3' = '' THEN NULL ELSE clock_timestamp() END,
              nullif('$3', '')
       FROM test.scenario s
       JOIN corpus.placement p ON p.hash = s.hash
       JOIN activity.campaign c ON c.environment = '$2'
           AND c.story_id = s.story_id
       WHERE s.org_id = '$scenario'" > /dev/null
}
note() {
    local scenario=${1%%|*} story=${1##*|}
    q "INSERT INTO activity.note (campaign, scenario_id, kind, text, author,
         state)
       SELECT id, '$scenario', 'defect', 'evidence note', 'evidence', '$2'
       FROM activity.campaign WHERE environment = 'ore'
         AND story_id = '$story'" > /dev/null
}

echo "== writing activity rows"
run "${picked[0]}" ore passed
echo "passed:                ${picked[0]}"
run "${picked[1]}" ore passed
run "${picked[1]}" ore failed
echo "passed, then failed:   ${picked[1]}"
run "${picked[2]}" ore blocked
echo "blocked:               ${picked[2]}"
run "${picked[3]}" ore passed 00
echo "passed, earlier def:   ${picked[3]}"
run "${picked[0]}" ore ""
echo "then unfinished:       ${picked[0]}"
run "${picked[0]}" elsewhere failed
echo "then failed elsewhere: ${picked[0]}"
note "${picked[1]}" open
note "${picked[1]}" open
note "${picked[1]}" addressed
echo "notes on ${picked[1]}: 2 open, 1 addressed"

echo "== what needs testing, present sprint, after the runs"
testing --repeat 5
