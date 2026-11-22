#!/usr/bin/env bash
# Syncs a scratch copy of ORE Studio into a private PostgreSQL cluster, on a
# Unix socket with no TCP port, and records test campaigns on its present
# sprint through compass.db's writes, played by compass-bench campaign: runs
# that pass, fail, block and pass against an earlier definition, a screenshot
# attached twice, notes, an addressed note, and writes the service refuses.
# It asks what needs testing before and after.
# Usage: campaign_ore_studio.sh <compass bin dir> <scratch dir holding ore/>
set -euo pipefail
bin_dir="$1"
S="$2"
pgbin=/usr/lib/postgresql/18/bin
pg="$S/pg"
ore="$S/ore"
play="$S/play"

rm -rf "${pg:?}" "${play:?}"
mkdir -p "$play"
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
"$bin_dir/compass" sync -C "$ore" 2>&1 | sed 's/in [0-9]* ms/in <n> ms/'

testing() { "$bin_dir/compass-bench" testing "$conn" --environment ore \
    --repeat 1 | sed -n '1,/^sprint state/p' | grep -v '^sprint state'; }
echo "== what needs testing, before"
testing

# The present sprint's scenarios in the order the report lists them, with
# the number of steps of each.
mapfile -t picked < <(q "WITH s AS (
    SELECT d.org_id FROM corpus.document d
    WHERE d.type = 'sprint' AND d.state = 'STARTED'
    ORDER BY d.created DESC LIMIT 1)
  SELECT sc.org_id || ' ' || (SELECT count(*) FROM test.step st
                              WHERE st.hash = sc.hash)
  FROM s, corpus.sprint_members(1, s.org_id) m(hash)
  JOIN corpus.document st ON st.hash = m.hash AND st.type = 'story'
  JOIN corpus.placement sp ON sp.hash = st.hash
  JOIN test.scenario sc ON sc.story_id = st.org_id
  JOIN corpus.placement p ON p.hash = sc.hash
  ORDER BY sp.path, p.path")
read -r a na <<< "${picked[0]}"
read -r b nb <<< "${picked[1]}"
read -r c nc <<< "${picked[2]}"
read -r d nd <<< "${picked[3]}"
echo "scenarios and their steps: $a $na, $b $nb, $c $nc, $d $nd"

# The eight bytes of the PNG signature stand for a screenshot.
printf '\x89PNG\r\n\x1a\n' > "$play/shot.png"

passes() { for ((i = 0; i < $1; ++i)); do echo "step $i pass"; done; }
{
    echo "# A run that passes, with a screenshot attached at a step twice."
    echo "start $a ana"
    passes "$na"
    echo "attach shot.png 0 The login page"
    echo "attach shot.png 0 The login page, captioned again"
    echo "finish"
    echo "finish"
    echo "step 0 fail"
    echo "# A run that fails at its second step, with notes and a screenshot."
    echo "start $b ben"
    echo "step 0 pass"
    echo "step 1 fail The tenant list is empty."
    echo "note defect $b 1 The tenant list is empty after provisioning."
    echo "attach shot.png note"
    echo "note question - - Which tenant should a tester provision?"
    echo "address dev Provision acme."
    echo "address dev Again."
    echo "note rant - - Not a kind."
    echo "note defect $c - Not a scenario of this story."
    echo "finish"
    echo "# A run that is blocked."
    echo "start $c ana"
    echo "step 0 blocked The service does not start."
    echo "finish"
    echo "# Writes refused: no tester, a step past the end, a run not done."
    echo "start $d -"
    echo "start $d ana"
    echo "step $nd pass"
    echo "step 0 pass"
    echo "finish"
    passes "$nd"
    echo "finish"
} > "$play/script.txt"

echo "== playing the script"
"$bin_dir/compass-bench" campaign "$conn" --environment ore \
    --script "$play/script.txt" --note-kinds defect,question \
    | sed -E 's/\[[0-9.]+ ms\]/[<t> ms]/'

echo "== stored"
q "SELECT 'attachments', count(*) FROM activity.attachment UNION ALL
   SELECT 'attached', count(*) FROM activity.attached UNION ALL
   SELECT 'notes open', count(*) FROM activity.note WHERE state = 'open'
   UNION ALL
   SELECT 'notes addressed', count(*) FROM activity.note
   WHERE state = 'addressed'"
q "SELECT id, tester, outcome, encode(definition, 'hex') = (
     SELECT encode(s.definition, 'hex') FROM test.scenario s
     WHERE s.hash = r.scenario_hash) AS current
   FROM activity.run r ORDER BY id"

echo "== what needs testing, after the runs"
testing

echo "== the last scenario's steps change, and the worktree is synced again"
page=$(q "SELECT p.path FROM test.scenario s
          JOIN corpus.placement p ON p.hash = s.hash WHERE s.org_id = '$d'")
echo "page: $page"
trap 'git -C "$ore" checkout -q -- "$page"; "$pgbin/pg_ctl" -D "$pg/data" -m fast -w -s stop || true; rm -rf "${sock:?}"' EXIT
awk '/^\* Steps/ { steps = 1 } steps && /^\*\* / && !done {
    print; print ""; print "- Also check the page title."; done = 1; next }
    { print }' "$ore/$page" > "$play/page.org"
cp "$play/page.org" "$ore/$page"
"$bin_dir/compass" sync -C "$ore" 2>&1 | sed 's/in [0-9]* ms/in <n> ms/'
echo "== what needs testing, after the edit"
testing
