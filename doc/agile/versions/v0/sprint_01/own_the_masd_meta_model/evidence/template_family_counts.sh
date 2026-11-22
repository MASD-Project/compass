#!/bin/bash
# usage: count.sh <templates dir> <tangle.tsv>
cd "$1" || exit 1
printf "family\tn\ttenant_id\trls\tparty\tores_utility\tores::database\tores::utility\tlogging\tnats\tores_any\tgpl\tfiles\n"
awk -F'\t' '{o=$1; sub(/\.org$/,"",o); n=split(o,p,"."); if(p[1]=="ores" && n>=3) fam=p[1]"."p[2]"."p[3]; else fam=o; print fam"\t"$2}' "$2" \
  | sort -u \
  | awk -F'\t' '{a[$1]=a[$1]" "$2} END{for(k in a) print k"\t"a[k]}' \
  | sort \
  | while IFS=$'\t' read -r fam files; do
      set -- $files
      n=$#
      c() { cat $files | grep -ciE "$1"; }
      printf "%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n" "$fam" "$n" \
        "$(c tenant_id)" "$(c 'row level security|create policy|rls')" "$(c 'party')" \
        "$(c 'ores_utility_')" "$(c 'ores::database|ores\.database')" "$(c 'ores::utility|ores\.utility')" \
        "$(c 'ores\.logging|ores::logging')" "$(c 'nats')" "$(c 'ores')" "$(c 'GNU General Public|licen[cs]e')" "$files"
    done
