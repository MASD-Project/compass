#!/usr/bin/env bash
# Times each statement of a first sync of the scratch ORE copy into a fresh
# private cluster, from the server's statement log.
set -euo pipefail
compass="$1"
S="$2"
bin=/usr/lib/postgresql/18/bin
pg="$S/pg2"
rm -rf "${pg:?}"
sock=$(mktemp -d /tmp/cpg.XXXXXX)
"$bin/initdb" -D "$pg/data" -U compass -A trust -E UTF8 --locale=C -N > /dev/null
cat >> "$pg/data/postgresql.conf" <<EOF
listen_addresses = ''
unix_socket_directories = '$sock'
log_min_duration_statement = 200
EOF
"$bin/pg_ctl" -D "$pg/data" -l "$pg/server.log" -w -s start
trap '"$bin/pg_ctl" -D "$pg/data" -m fast -w -s stop || true; rm -rf "${sock:?}"' EXIT
"$bin/createdb" -h "$sock" -U compass compass
rm -f "$S/ore/.git/compass/local.org"
"$compass" db init -C "$S/ore" --database "host=$sock user=compass dbname=compass" > /dev/null
"$compass" sync -C "$S/ore" --timings | grep -E 'synced|write'
grep -oE 'duration: [0-9.]+ ms +statement: .{0,70}' "$pg/server.log" | tr '\n' ' ' | sed 's/duration:/\nduration:/g'
echo
