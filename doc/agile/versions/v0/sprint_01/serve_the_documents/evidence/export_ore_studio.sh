#!/usr/bin/env bash
# Syncs a scratch copy of ORE Studio into a private PostgreSQL cluster, on a
# Unix socket with no TCP port, records test campaigns on its present sprint
# through compass.db's writes, played by compass-bench campaign, and exports
# them with compass test export: the files written, compass check over them,
# a second export that writes nothing, and a note added and exported again,
# which rewrites only its campaign's page. ORE Studio adopts the
# test_campaign contract by copying compass's contract page into doc/meta,
# as the scratch copy does here.
# Usage: export_ore_studio.sh <compass checkout> <compass bin dir> <scratch dir holding ore/>
set -euo pipefail
compass_root="$1"
bin_dir="$2"
S="$3"
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
g() { git -C "$ore" -c user.name=evidence -c user.email=evidence@localhost "$@"; }

if [[ ! -d "$ore/.git" ]]; then
    cp "$compass_root/doc/meta/document_type_test_campaign.org" "$ore/doc/meta/"
    g init -q
    g add -A
    g commit -q -m snapshot
fi

echo "== sync"
"$bin_dir/compass" db init -C "$ore" --database "$conn"
"$bin_dir/compass" sync -C "$ore" --environment ore 2>&1 \
    | sed 's/in [0-9]* ms/in <n> ms/'

# The present sprint's first two stories that have scenarios, and their
# first scenarios, with the number of steps of each.
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
echo "scenarios and their steps: $a $na, $b $nb"

# A PNG and a JPEG stand for screenshots: their signatures and a few bytes.
printf '\x89PNG\r\n\x1a\nevidence-one' > "$play/login.png"
printf '\x89PNG\r\n\x1a\nevidence-two' > "$play/list.png"
printf '\xff\xd8\xff\xe0evidence-jpeg' > "$play/photo.jpg"

passes() { for ((i = 0; i < $1; ++i)); do echo "step $i pass"; done; }
{
    echo "# A run that passes, with a screenshot at its first step."
    echo "start $a ana"
    passes "$na"
    echo "attach login.png 0 The login page"
    echo "finish"
    echo "# A run that fails at its second step, with notes and screenshots."
    echo "start $b ben"
    echo "step 0 pass"
    echo "step 1 fail The list is empty | no rows."
    echo "attach list.png 1 The empty list"
    echo "note defect $b 1 * The list is empty after provisioning."
    echo "attach photo.jpg note"
    echo "note question - - Which tenant should a tester provision?"
    echo "address dev Provision acme."
    echo "finish"
    echo "# A second run of the first scenario, left in progress."
    echo "start $a ana"
    echo "step 0 pass"
} > "$play/script.txt"

echo "== recording"
"$bin_dir/compass-bench" campaign "$conn" --environment ore \
    --script "$play/script.txt" --note-kinds defect,question \
    | sed -E 's/\[[0-9.]+ ms\]/[<t> ms]/'

echo "== first export"
"$bin_dir/compass" test export -C "$ore" --environment ore
echo "-- the tree: new files only, no scenario page touched"
g status --porcelain --untracked-files=all
q "SELECT id, story_id, exported_at IS NOT NULL FROM activity.campaign ORDER BY id"
pages=$(g status --porcelain --untracked-files=all | awk '/\.org$/ { print $2 }')
for page in $pages; do
    echo "-- $page"
    cat "$ore/$page"
done

echo "== compass check over the exported pages"
# A finding on a page compass test export wrote would be listed here.
"$bin_dir/compass" check -C "$ore" --no-blame \
    --codec required_sections --codec lint --codec org_links \
    --codec evidence_location --codec heading_numbers 2>&1 \
    | grep -F 'test_campaign' || echo "no finding on a test_campaign page"
echo "-- the checks see the pages: lint without the contract page in doc/meta"
mv "$ore/doc/meta/document_type_test_campaign.org" "$play/"
"$bin_dir/compass" check -C "$ore" --no-blame --codec lint 2>&1 \
    | grep -F 'test_campaign' | cut -c1-160 || true
mv "$play/document_type_test_campaign.org" "$ore/doc/meta/"
echo "-- the same check on a copy of a page without * Notes"
first=$(echo "$pages" | head -1)
cp "$ore/$first" "$play/page.org"
grep -v '^\* Notes$' "$play/page.org" > "$ore/$first"
"$bin_dir/compass" check -C "$ore" --no-blame --codec required_sections 2>&1 \
    | grep -F 'test_campaign' || true
cp "$play/page.org" "$ore/$first"

g add -A
g commit -q -m "first export"
# The inode, size and modification time in nanoseconds of each exported file.
stamp() {
    g ls-files | grep -F test_campaign_ | while read -r f; do
        stat -c '%i %s %.9Y %n' "$ore/$f"
    done | sed "s|$ore/||"
}
stamp > "$play/before.txt"

echo "== second export, with nothing recorded since"
"$bin_dir/compass" test export -C "$ore" --environment ore
echo "-- the tree"
g status --porcelain --untracked-files=all
echo "-- the inode, size and modification time of each exported file"
stamp > "$play/after.txt"
diff "$play/before.txt" "$play/after.txt" \
    && echo "unchanged: $(wc -l < "$play/after.txt") files"

echo "== a note added to the second campaign, then a third export"
campaign=$(q "SELECT campaign FROM activity.run WHERE scenario_id = '$b' LIMIT 1")
{
    echo "campaign $campaign carla"
    echo "note question $b - Is #+end_example safe in a note?"
} > "$play/note.txt"
"$bin_dir/compass-bench" campaign "$conn" --environment ore \
    --script "$play/note.txt" --note-kinds defect,question \
    | sed -E 's/\[[0-9.]+ ms\]/[<t> ms]/'
"$bin_dir/compass" test export -C "$ore" --environment ore
echo "-- the tree"
g status --porcelain --untracked-files=all
g diff --stat
g diff
