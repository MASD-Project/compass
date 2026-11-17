# Review of the compass.docs codecs

Scope: the 22 descriptors in `projects/compass.docs/src/codecs/` (17 files) and
`ops/regions.cpp`, `ops/skills.cpp`. Read-only review. Nothing was built or run
in the compass checkout. ORE facts were read with `grep`/`awk` in
`~/Development/OreStudio/ores_dev_prime_origin` at `da9ee3cece` (not
`545cafca45`; counts may differ slightly from the evidence pages).

Evidence labels: **measured** (a command I ran, output quoted), **read** (from
the code or a doc), **guess** (an estimate or a prediction nobody tested).

## Cross-cutting findings

These recur across most gates. Each per-gate section refers back to them.

### X1. The docs gates ignore the parse they already have

Every org document is in the corpus with its `compass.org` tables, parsed the
way org-element parses it (10,909 of 10,909 ORE files identical, per the
`compass.ingest` overview). Of the 22 docs codecs, only `org_links` reads the
tables, and `*_recipe_library` reads them through `ops::tangle`. The rest
re-lex the text with regular expressions. Most of them (`lint`,
`component_docs`, `backlog_indexes`, `recipe_inventories`,
`physical_inventory`, `skill_*`, `trade_type_coverage`, `rule_identifiers`,
`prototypes`) do not even take the document from the corpus: they call
`read_file` on a path the corpus already holds parsed, so they also pay a
second read and a UTF-8 decode. **read**

No codec calls `corpus_view::find_id`. Four codecs (`lint`, `pattern_uses`,
`structure_notes`, and the ID lookups of the generators) build their own ID
index with `^:ID:` regexes. **read**

The component overview justifies the regexes "because the parse would decide
differently on an indented block or a heading inside one". That reason does
not hold:

- org-element recognises indented and upper-case blocks. The regexes are the
  ones that differ: `rule_identifiers` uses case-sensitive, unindented
  `starts_with("#+begin_")`; `heading_numbers` counts a `#+end_src` anywhere on
  a line, prose included; `component_docs` tracks only `src` and `example`.
  **read**
- A heading inside a block is a heading to org. An unescaped `* 2. Foo` inside
  a src block breaks the block in the export, which is why org-src
  comma-escapes such lines. So the parse is right and the regex is wrong.
  **read** (org-mode behaviour)
- Where it matters on ORE, the parse is stricter in the useful direction. 32
  `:ID:` lines sit inside blocks (placeholders such as `{{id}}` and `...`, and
  one real UUID in `paste_blocks_in_codegen.org`). `lint` counts them all as
  link targets, so they can mask a dangling link. Id links also appear inside
  blocks (for example 4 in `doc_feature_org.org`), and `lint` reports those
  as dangling. **measured** with a crude awk block tracker
  (`/tmp/claude-1000/-home-marco-Vault-Development-masd-compass/6fc2e654-6b06-470e-8f36-48cb2b46040a/scratchpad/ids_in_blocks.txt`).
- Zero numbered headings exist anywhere in ORE (`grep -rnE '^\*+
  [0-9]+(\.[0-9]+)*[.)]\s+'` over `doc` and `projects` prints nothing), so the
  regex in `heading_numbers` protects no verdict that the parse would change.
  **measured**

### X2. ORE policy compiled into compass

Several ports copied a script's reviewed table into C++:

| Codec | Table copied |
|---|---|
| `skill_levels`, `skill_level_tags` | `LEVELS`, 76 skill→level rows |
| `skill_recipes` | 16 skill→recipe owner rows |
| `skill_catalogue` | 10 domains with headings and blurbs |
| `skill_type_tables` | 7 model types, 13+10 read-only commands with blurbs |
| `rule_identifiers` | 2 pages, 38 section-title→code rows, 5 unkeyed codes |
| `backlog_indexes` | 4 whole page heads (front matter, prose, note) |
| `recipe_inventories` | the HTTP index head, 4 shell heading renames |
| `trade_type_coverage` | one sprint_30 story path and three task UUIDs |
| `prototypes` | the shared-copy file list |

In Python the table lived in ORE. Now adding a skill, renaming a UX section or
editing the inbox page's prose needs a compass release, or compass reports
drift and reverts the edit. The value-set work avoided this with
`ops::module_table`, which reads the Python literal. The better fix is to move
each table into the org pages it describes: a skill's level is already its
`#+level:` keyword, a section's rule code can be a heading property, and a
page head is the page itself. **read**

### X3. A refusal becomes silence, and maybe a deletion

`skill_catalogue`, `skill_type_tables`, `skill_levels`, `recipe_inventories`
and `trade_type_coverage` return with no output where the script would raise
or exit non-zero. Examples are a skill without a description, the table
missing a skill, a merge conflict in `shell.org`, or a coverage source the
script rejects. The Python gate failed in those cases. The codec reports
nothing. The `compass.codec` overview also says that a recorded file of a
complete codec that no output names "is removed when it still holds what
compass wrote". These generators emit whole hand-written pages, so a refusal
after one good run would plan to remove `claude_code_skills.org`,
`skill_systems_levels.org`, `shell.org` or the coverage note. **read**, from the
sink's documented rule. **Not tested.** It needs a test before anyone relies
on it. The value-set port solved the same problem: its generator writes
nothing on a problem, and a paired diagnostics codec says why. Every
generator here needs the same: a refusal is a finding, and the file is kept.

### X4. Corpus functions that enumerate every document

`pattern_uses` and `structure_notes` are `corpus_function`s over
`every_document()` that call `v[i]` for every row. A corpus unit that
enumerates depends on the whole type set, so **any org edit reruns them in
full**. **read**, from the "Recording" rule. Both also copy the text of every
page they keep: `universal_newlines(d.source)` and `std::string(d.source)`
into a `std::map`. That is up to 70 MB of copies for ORE (70,152,231 bytes of
tracked `.org`, 61,881,249 under `doc/`). **measured** (sizes) / **guess**
(that they dominate the edit-hook time after `component_docs`). A scan over
corpus documents (map: one page's small fact, reduce: the relation) makes an
edit cost one map, and the reduce is skipped when the fact is unchanged.

### X5. Generators that rewrite hand-written pages

`structure_notes` (417 pages), `skill_recipes` (16 skill pages),
`skill_type_tables` (2 skill pages), `skill_catalogue`, `skill_levels`,
`recipe_inventories`, `physical_inventory` and `trade_type_coverage` emit
whole pages that people also edit. The one-producer-per-path rule is what
forced the `skill_levels` split: `apply_skill_levels.py` writes `#+level:`
into all 76 skills, and 29 skill pages sit in structure-note clusters.
**measured** (29 is an upper bound from a grep of hub link targets). Today
no `skill_recipes` owner and neither `skill_type_tables` page is in a
cluster, so there is no live clash. **measured**. The clash is latent:
the first structure note that orders one of those skills makes two codecs
produce one path. A back-link or a missing recipe link is a property of a
page, so it is a finding with a fix, not a second producer.

---

## org_links

1. **Intent.** No bracket link has a raw target starting with a colon. That is
   what a verbatim span closed early leaves behind (`[[:space:]]`), and the
   site export aborts on it.
2. **Does it make sense.** Yes, but it is one case of a wider invariant: the
   export aborts on any fuzzy link it cannot resolve. `component_docs`'
   `backtick_link` checks another case of the same invariant with a regex.
   Both should be one rule: a fuzzy link (no registered type) whose path
   names no heading, target or `#+name` of its page.
3. **Python accidents.** None of substance. It already uses the links table.
   It does hand-roll the raw target by slicing source from `range.begin + 2`
   to the first `]`. The table's path column gives the same thing.
4. **Cost.** A document codec that scans one page's links table. It is cheap
   and incremental. The `is_published` regex runs once per page. A prefix
   test would do.
5. **Shared data.** The links table (format, type, path). The same table is
   wanted by `lint` (id links), `component_docs` (fuzzy links in backticks),
   `pattern_uses` and `structure_notes` (id links by section).
6. **Recommendation.** Merge with `component_docs/backtick_link` into one
   per-page `unresolved_link` rule over the links table. Effort: small.
   Saving: retires `component_docs`' regex scan of 62 MB on a cold run
   (guess: a few hundred ms), and the export aborts on fewer surprises.
   Verdicts would change on ORE only where a fuzzy link outside backticks
   fails to resolve. That is a real defect the site build would hit. Guess:
   few or none.

## heading_numbers

1. **Intent.** No org heading carries a hand-written number. The exporter
   numbers headings, and a written number goes stale.
2. **Does it make sense.** Yes. It is cheap and the defect is real. Its
   second rule, `unreadable_document` (not UTF-8), belongs to the parser or
   ingest. A non-UTF-8 page is a defect for every gate, not just this one.
3. **Python accidents.**
   - It re-lexes the page line by line with three regexes and a block-depth
     counter, which is the Python's own stand-in for a parser. The depth counter counts a `#+end_src`
     anywhere on a line (`find_all`), so prose that mentions it closes a
     block. It ignores `verse`, `center` and custom blocks.
   - The heading regex needs the number straight after the stars, so
     `* TODO 1. Foo` and `* [#A] 2) Foo` pass.
   - The correct behaviour is to read the headings table and test each
     title's first word. That is a heading only where org says it is one.
     This changes no verdict on ORE, which has no numbered heading anywhere.
     **measured**
4. **Cost.** A document codec over every page under `doc/` and `projects/`.
   The line regex costs one cold pass over about 70 MB (Python: 1.2 s). Via
   the headings table, the cost is proportional to the heading count.
5. **Shared data.** The headings table (titles). `required_sections` and
   `component_docs` read the same titles, as do `pattern_uses`,
   `rule_identifiers` and `structure_notes` (sections).
6. **Recommendation.** Simplify to a headings-table scan. Fold it into one
   per-page "org hygiene" document codec with `org_links`, `lint`'s per-page
   rules and the fuzzy-link rule. Move `unreadable_document` into ingest.
   Effort: small. Saving: the cold regex pass (guess: 100 to 300 ms of CPU),
   and one fewer per-document unit per page in the record.

## pattern_uses

1. **Intent.** A service architecture pattern and each of its uses link each
   other: the pattern lists the use under `In ORE Studio`, and the use lists
   the pattern under `Patterns used`, outside plans, external references and
   agile pages.
2. **Does it make sense.** Yes. It is a real bidirectional-link invariant.
   `shared_id` duplicates what a corpus-wide unique-ID check should own. Both
   `lint` (by building an ID map) and the corpus index see duplicates.
3. **Python accidents.**
   - It reads the page ID with `^:ID:` anywhere in the page and the type with
     `^#\+type:` anywhere. Both are in the tables: the document property and
     the keyword. The registered type is already `d.type`.
   - Sections are found by a regex for level-1 headings, `^\* (.+?)\s*$`,
     which also matches inside src blocks and does not strip tags or TODO
     keywords. The headings table gives the section range.
   - Links are matched by `\[\[id:(...{36})\]\[[^\]]*\]\]`, which **requires
     a description**. A bare `[[id:X]]` in `Patterns used`, or a use that links
     back with a bare link, is invisible. Under that regex a bare back-link
     reads as `one_way`, which is a false positive. The links table holds
     every id link, with or without a description.
   - The one-way check for a use searches the use's *whole text*, which is
     the script's stated intent ("or links it in its body"). That is fine.
   - Correct behaviour would accept bare id links. Verdicts on ORE would
     change only where a bare link is used. **guess**: none today, because
     the convention writes descriptions.
4. **Cost.** A corpus function over every document (X4). It copies every
   UTF-8 page under `doc/` and `projects/` with `universal_newlines` into a
   `std::map<std::string, page>`, about 70 MB, and runs two regexes over the
   whole of every page and the heading regex over every non-exempt page.
   **Every org edit reruns it.** Python: 1.05 s. The compass cost is not
   measured. **guess**: 150 to 400 ms per run, warm or cold.
5. **Shared data.** Per page: the document `:ID:`, the type, and the id-link
   targets tagged with the level-1 section they sit in. The same fact,
   minus the section tag, is what `lint` needs for `dangling_link` and
   `durable_link`. `structure_notes` needs the same fact, with the ordering
   cut and the `# aside` marker.
6. **Recommendation.** Replace with a scan whose map emits (id, type, id-links
   with section) from the tables, or merge into the one link-graph scan (see
   Shared passes) as rules `pattern_unknown_use`, `pattern_one_way` and
   `pattern_not_a_pattern`. Drop `shared_id` in favour of one duplicate-ID
   rule. Effort: medium. Saving: an org edit costs one page map instead of a
   70 MB copy and three regex sweeps. **guess**: 150 to 400 ms off the edit
   hook.

## component_docs

1. **Intent.** Every component and every part with `include/` and `src/` has
   a complete overview (ID, type, description, the six sections) and a
   `.puml` diagram. No top-level component name extends another's. No
   bracket link that the site export cannot resolve sits in a backtick span
   under `doc/`.
2. **Does it make sense.** It is three gates in one, and two of them
   duplicate other checks.
   - `missing_section` duplicates `required_sections`. The `component` type's
     contract (`doc/meta/document_type_component.org`) requires exactly these
     six sections, and `required_sections` reads them from the type
     registry. **read**
   - `missing_id`, `missing_type` and `missing_description` are document
     contract checks (every document has an `:ID:`, a `#+type:` and a
     `#+description:`). They belong in the contract codec beside
     `required_sections`, for every type, not only component overviews.
   - `missing_overview`, `missing_puml` and `name_collision` are tree
     checks. They are sound and nothing else covers them.
   - `backtick_link` is a heuristic for "the export aborts on an unresolved
     fuzzy link" (see `org_links`).
3. **Python accidents.**
   - The overview is read as a file with regexes: `:ID:\s+\S+` anywhere (it
     even matches `:ID:` in prose), `#\+type:\s*(?:ores\.codegen\.)?component`
     **unanchored and case-insensitive**, so `#+type: components_index` or a
     mention in a src block passes. The overview is a corpus document of type
     `component`, and the tables answer all four questions exactly.
   - Each of the six section regexes is compiled again for every overview.
   - The backtick lexer is a markdown habit. It finds `` `...[[x]]...` ``,
     blanks `=...=`/`~...~`, skips `src`/`example` blocks by a line regex, and
     drops targets that contain `:` or `.` or start with `*`. Org has no
     backtick syntax, and the link either parses as a fuzzy link or does not.
     The links table says which, and whether the link sits in a verbatim
     object or block. The correct rule is: a fuzzy link that resolves to
     nothing in its page. It would catch the same backtick cases and also the
     same defect outside backticks. **guess**: few extra findings on ORE,
     because the site build already aborts on them.
   - The exceptions file (`docs_exceptions.txt`, 4 `MISSING_PUML` and 1
     `MISSING_SECTION` lines at `da9ee3cece`, **measured**) duplicates the
     runtime's inline exceptions. `MISSING_SECTION` can be an inline
     exception in the overview. `MISSING_PUML` needs a file-keyed exception
     because the overview may not exist.
4. **Cost.** A scan whose map reads each `doc/**/*.org` as a *file* (not the
   corpus document), decodes it, and runs four regexes per line. That is 62
   MB on a cold run. The reduce lists all of `projects/`, reads every
   overview again as a file and compiles 6 regexes per overview. The edit
   hook's 0.4 s for this codec is unexplained by the map of one page.
   **guess**: it is the reduce's `list_files("projects/")` listing input and
   the overview rereads. This needs a profile before acting.
5. **Shared data.** The `projects/` listing (also `skill_namespace`-style
   tree checks, `prototypes` over `doc/prototypes/`). Overview keywords and
   headings (`required_sections`, the contract codec). Links per `doc/` page
   (`org_links`, `lint`).
6. **Recommendation.** Split and shrink. Drop `missing_section` (it is
   `required_sections`). Move `missing_id`, `missing_type` and
   `missing_description` into the contract codec, read from the tables.
   Move `backtick_link` into the per-page link rule. Keep a small tree codec
   (`missing_overview`, `missing_puml`, `name_collision`) that lists
   `projects/*/` once, with `list_files` patterns narrow enough
   (`projects/*/modeling/`, `projects/*/*/include/`) that an org edit outside
   `projects/` does not invalidate it. Effort: medium. Saving: the 62 MB cold
   lex, and most of the 0.4 s edit-hook cost (**guess**). There are 5
   duplicated rules fewer.

## shell_recipe_provenance

1. **Intent.** Every committed shell script under
   `projects/ores.shell/scripts/library/*/` names, in its `GENERATED from`
   line, a recipe document that exists. No orphan script outlives its
   recipe.
2. **Does it make sense.** Only while the library is not generated by
   compass. `shell_recipe_library` emits every script from its recipe. If
   that codec owns the library directory (every file under it that it does
   not emit is drift), then an orphan is drift and this gate is redundant.
   With the sink as documented, a recorded file that is no longer emitted is
   removed, but a file compass never recorded is never touched. So the gap
   is only for orphans that predate the first `compass generate`. **read**
3. **Python accidents.** `names_a_file` re-implements OS path resolution,
   with `..` going up only from an existing directory. That is faithful, but
   the path is always `doc/recipes/shell/...` as the generator writes it.
   An exact path match against the corpus would do.
4. **Cost.** It reads every `.ores` file in full (5,015 scripts) to find one
   line. **guess**: 20 to 50 ms cold, and zero warm unless a script changes.
5. **Shared data.** The library listing and the scripts.
   `shell_recipe_library` produces exactly these bytes.
6. **Recommendation.** Drop once `shell_recipe_library` runs in the check
   and claims its output directory (one "unowned file in a generated
   directory" rule in the runtime covers this and the HTTP library). Until
   then, keep. Effort: small, for the runtime's directory-ownership rule.
   Saving: 5,015 file reads cold.

## skill_namespace

1. **Intent.** Every skill directory under `doc/llm/skills` is named
   `compass-...`, so project skills do not collide with plugin skills.
2. **Does it make sense.** It is a guard left over from a one-time rename.
   The invariant is cheap and still real.
3. **Python accidents.** None. It is a listing glob.
4. **Cost.** One listing. Negligible.
5. **Shared data.** The `doc/llm/skills/*/SKILL.org` listing, which
   `ops::skill_names` also produces for `skill_catalogue`, `skill_levels` and
   `skill_level_tags`.
6. **Recommendation.** Merge into one skills codec (see Shared passes) as a
   rule. Better still, express it as a contract field: the `skill` type
   could declare a name pattern. Effort: small. Saving: one unit and one
   listing.

## no_pr_triggers

1. **Intent.** No GitHub workflow runs on a pull request, because the local
   runner is the gate.
2. **Does it make sense.** Yes as policy, but the rule as written does not
   enforce it.
3. **Python accidents.** It flags only a line that is exactly
   `  pull_request:` with two spaces. It misses `on: pull_request`,
   `on: [push, pull_request]`, `pull_request_target:` (the riskier trigger)
   and four-space indentation. It does not flag `pull_request_review` or
   `pull_request_review_comment`, which `claude.yml` uses (**measured**).
   Those are probably intended, but the rule does not say so. The correct
   behaviour is to read the top-level `on` key (mapping, sequence or scalar)
   and reject `pull_request` and `pull_request_target`, with an explicit
   allow-list for the review events. That changes no verdict on ORE today
   (**measured**: no workflow uses those forms).
4. **Cost.** 32 small YAML files. Negligible.
5. **Shared data.** None with the org gates.
6. **Recommendation.** Keep, and fix the matcher. Effort: small. Saving: none.
   The fix closes evasions.

## prototypes

1. **Intent.** The prototype tree under `doc/prototypes/<group>/<name>/`, the
   group pages and the top page agree. Each review has an index, and each
   row has its review cell. The site deploys what it finds. Shared copies are
   identical. The retired React tree stays gone.
2. **Does it make sense.** The inventory rules (`unlisted`, `missing`,
   `no_group_page`, `unlinked_group`, `missing_group`, `review_*`) are
   sound. Three rules are not:
   - `react_tree` and `no_discovery` are tombstones for the 2026-10-06
     migration. `no_discovery` checks that a Lisp file contains the string
     `ores-deploy-prototypes`, which a comment satisfies.
   - `copy_differs` exists because files are shared by copy. The fix is to
     share one file (a relative `../_shared/common.js`), and then the rule
     disappears.
   - The group pages are inventories derived from the tree. They could be
     generated, and the check would then be drift.
3. **Python accidents.**
   - The pages are read as files, and links are found with
     `\[\[file:([a-z0-9][a-z0-9-]*)/` over the whole text, src blocks and
     comments included. The parsed links table (type `file`, path) is
     exact.
   - A row is "the line that holds the link". The review cell is
     `endswith("| none |")`. The parsed table rows and cells give the last
     cell directly.
   - The changes would not alter ORE verdicts (**guess**).
4. **Cost.** One listing of `doc/prototypes/`, about 10 page reads, and the
   shared files. Small.
5. **Shared data.** Links of the prototype index pages (the links table).
   The `doc/prototypes/` listing.
6. **Recommendation.** Simplify. Read the index pages from the corpus tables.
   Drop `react_tree` and `no_discovery` (tombstones). Replace `copy_differs`
   by de-duplicating the files in ORE. Effort: small. Saving: 3 rules and the
   text scans. Run time is already small.

## rule_identifiers

1. **Intent.** Each rule bullet of a keyed section of the two UX standards
   starts with a unique, permanent identifier (`*RS-LIST-03*`), and the
   screen review checklist cites only identifiers that exist.
2. **Does it make sense.** Yes. It is a real cross-reference invariant.
3. **Python accidents.**
   - **The 38-row section table is compiled into compass (X2).** Renaming a
     section in ORE now needs a compass release. The section code belongs on
     the heading, for example a `:RULE_CODE: LIST` property, which the
     properties table gives.
   - Sections and bullets are found by regex. `starts_with("#+begin_")` is
     case-sensitive and unindented. A bullet is `^- (?!\[ \])` or a bold
     identifier at column 0, so nested items and `+` bullets are wrong. The
     items table gives top-level items of the section with their ranges.
   - Checklist lines are `- [ ]` prefix matches. The items table gives
     checkbox items.
   - The "unkeyed page" test is three substring forms, `(UXL `, `; UXL ` and
     `, UXL `. That is a heuristic for "cites a page".
   - The pages are read as files, not taken from the corpus.
   - **guess**: no ORE verdict changes with the items table, but this is
     untested.
4. **Cost.** Three files. Small.
5. **Shared data.** Headings, items and properties of three pages.
6. **Recommendation.** Simplify. Move the section codes into the pages as
   heading properties and read headings and items from the tables. Effort:
   medium, because it needs an ORE page edit. Saving: removes the X2
   coupling. Run time is unchanged.

## lint

1. **Intent.** Filetags are lowercase. Generated-region markers are closed
   and name a script that writes the page. Id links are UUIDs that resolve.
   Durable pages do not link agile pages (advisory).
2. **Does it make sense.** Mostly. `dangling_link` is the most valuable rule
   in the component. `bad_filetag` is a contract rule (the tag syntax),
   applicable to every typed page. `marker_without_writer` checks that a
   script mentions the page's file name, which is weak, and after the port
   the writers are compass codecs, not scripts. The marker should name the
   codec, and the check should be that the codec is in the catalogue.
3. **Python accidents.**
   - It reads every `.org` file of the checkout as a file and re-lexes it,
     although every one is a corpus document.
   - IDs are every `^:ID:` line of the page, block content included (32 such
     lines in ORE, **measured**). That adds false targets.
   - Id links are matched by `\[\[id:([0-9A-Fa-f-]{36})\]...` per line, so a
     link that wraps a line is missed in the per-line pass. Links inside
     src/example blocks are reported as dangling. The links table has
     neither problem.
   - `#+type:` is the first `^#\+type:` anywhere. `d.type` is the registered
     type.
   - `bad_filetag` reads only the first `#+filetags:` line, as the script
     does. Org merges every one.
   - `malformed_link` (a hyphenated non-UUID after `id:`) is a regex for "the
     id link's path is not a UUID". The links table gives the path.
   - Markers (`# BEGIN generated X`) are comment lines. The elements table
     has comment elements, so a marker inside an example block (a doc that
     shows the syntax) would stop being reported as unclosed.
   - Correct behaviour changes ORE verdicts slightly. Some dangling links in
     blocks go away (**measured** that id links exist inside blocks, for
     example 4 in `doc_feature_org.org`; **not** measured whether they
     dangle), and the one real UUID in a block stops counting as a target.
4. **Cost.** A scan, so warm and edit runs are one map. Cold is about 1 s:
   five regexes per line over 70 MB plus `universal_newlines` and
   `decode_replacing` copies. The reduce re-sorts all facts, rebuilds a
   `std::map` of every ID (strings uppercased one by one), and reads every
   marker's script. From the tables, the map is a walk of the keywords,
   properties and links rows. **guess**: cold under 200 ms.
5. **Shared data.** Per page: top and heading IDs, the type, filetags,
   id links (path and offset), and comment markers. This is the same fact
   `pattern_uses` and `structure_notes` rebuild by regex, and the ID set that
   `corpus_index` already holds.
6. **Recommendation.** Replace the map with a table-driven map, and make it
   the base of the shared link-graph scan. Move `bad_filetag` to the contract
   codec. Re-point `marker_without_writer` at codec names. Effort: medium.
   Saving: about 0.8 s cold (**guess**), and one ID index instead of three.

## shell_recipe_library and http_recipe_library

1. **Intent.** The committed script libraries (5,015 `.ores` files, 414
   `.hurl` files) are what the recipes tangle to, byte for byte as Emacs
   wrote them.
2. **Does it make sense.** The invariant does. Committing 5,429 generated
   files is a choice to revisit: they exist so that `ores.shell` can run a
   recipe without Emacs. With compass generating them in-process and in
   milliseconds, the build or the shell's install step can generate them.
   ORE's committed copies are then not needed, and neither are the drift
   check and `shell_recipe_provenance`. That is ORE's call. It would remove
   the largest drift surface (38 s of Emacs in Python times).
3. **Python accidents.**
   - `block_tangles` and `section_title` find opening lines and headings with
     regexes over the raw text, while `ops::tangle` already walks the parsed
     blocks. The blocks table has the language and parameters, and the
     enclosing heading is a parent walk. Both regexes match lines inside
     other blocks (an example of a src block in a recipe). The `keyword()`
     helper compiles a regex per call per key.
   - `has_block` is a whole-text regex. The blocks table answers it.
   - "Byte for byte as Emacs" ties output to Emacs quirks (the first
     `#+description` even under a heading, `expand-file-name` resolution).
     That is right for parity, and the quirks can be dropped once ORE no
     longer runs Emacs.
4. **Cost.** A document codec per recipe, which is incremental. Each page
   pays several whole-text regexes plus the regex compilations. **guess**:
   small per page. 581 shell and 95 HTTP recipe pages are a cold cost only.
5. **Shared data.** Recipe keywords (title, description, filetags category),
   IDs and blocks. `recipe_inventories` reads the same pages' ID, title and
   filetags as files with its own line parser.
6. **Recommendation.** Keep, and replace the regexes with block rows. Ask ORE
   whether the libraries need committing. Effort: small for the regexes,
   large for build-time generation (ORE side). Saving: small, unless the
   libraries are no longer committed.

## skill_catalogue

1. **Intent.** The catalogue page lists every skill by domain with its ID,
   title and description.
2. **Does it make sense.** The table is a view of the skill pages. The
   harness reads `SKILL.md`, not this page, so it exists for humans and the
   site. It can be generated at export or by a query. If it stays
   committed, it is a region generator.
3. **Python accidents.**
   - The domain table is compiled in (X2).
   - ID and title are `^:ID: ` and `^#\+title: ` regexes. The tables have
     them.
   - The description is the first `^description: ` line anywhere. It is
     meant to be the markdown front matter inside the `#+begin_export
     markdown` block. The blocks table gives that block's contents.
   - On a skill without an ID, title or description it returns with no
     output (X3). The script raised.
4. **Cost.** 76 skill files read as files. Small.
5. **Shared data.** The skill listing and each skill's keywords and export
   block. These are shared with `skill_namespace`, `skill_levels`,
   `skill_level_tags`, `skill_recipes` and `skill_type_tables`.
6. **Recommendation.** Merge into one skills codec with a prepared skill
   model (name, ID, title, description, level, Recipes section links).
   Emit a finding where it now goes silent. Effort: small. Saving: 5 reads
   of each skill page become 1.

## skill_type_tables

1. **Intent.** The `compass-doc-add` skill lists every document type with
   its scaffold command, home and sections to fill. `compass-doc-show`
   lists the read-only commands.
2. **Does it make sense.** Its sources are wrong for compass-ng. It parses
   `compass.py`, ORE's Python CLI, with regexes for two dict literals
   (`_STATIC_PARENT`, `_DEFAULTABLE_PARENT`) and for argparse
   `add_parser(... help=...)` calls. The types and their folders and
   required sections are in the **type registry** that compass already
   builds from `doc/meta`. The command help belongs to the CLI that will
   serve the commands, which is compass-ng.
3. **Python accidents.**
   - It parses Python source with regexes. A reformatted dict (a trailing
     comment, or single quotes) silently drops rows.
   - The sections are every level-1 heading of the `doc_*.org.mustache`
     template minus `:noexport:`, read by a regex that also matches inside
     blocks. The contract's required sections are the authority. The
     template is one rendering of it.
   - 7 model types and 23 command blurbs are compiled in (X2).
   - It returns silently when `compass.py` or a skill is missing (X3).
4. **Cost.** One 100+ KB Python file and about 25 templates. Small.
5. **Shared data.** The type registry (`required_sections`, `component_docs`
   contract checks). Two skill pages.
6. **Recommendation.** Replace with a render of the type registry (the same
   registry `ops::rule_table` uses for the rule page), plus the command
   table from compass-ng's own command descriptors. Effort: medium. Saving:
   a fragile Python-parsing dependency. The verdict would change wherever
   the template and the contract disagree (**guess**: a few rows).

## skill_recipes

1. **Intent.** Each of 16 action skills links, in its Recipes section, the
   recipe that owns its commands.
2. **Does it make sense.** No longer as a generator. `link_skill_recipes.py`
   applied a reviewed mapping once. After that run the links are in the
   pages, and the 16-row table in C++ is a second copy of what the pages
   say (X2). Its only ongoing effect is to re-add a link someone removed,
   by rewriting the whole skill page (X5).
3. **Python accidents.**
   - The Recipes section is `^\* Recipes\s*$(.*?)(?=^\* )`. A Recipes
     section that is the last heading never matches, and then the code
     inserts a second `* Recipes` heading. The headings table gives the
     range.
   - Presence is `body.find("id:" + id)`, a substring test that would also
     match a longer ID or a comment.
   - It skips silently when a recipe is missing (X3).
4. **Cost.** 16 skill pages and 16 recipe pages. Small.
5. **Shared data.** Skill headings and links, recipe IDs and titles.
6. **Recommendation.** **Drop.** The links exist in ORE today, and
   `dangling_link` already guards that they resolve. If ORE wants the
   pairing enforced, declare it in the recipe (for example
   `:OWNER_SKILL:`) and add one rule to the skills codec. Effort: small.
   Saving: one page producer and the table in compass.

## skill_levels and skill_level_tags

1. **Intent.** Each skill carries the cybernetic level of the System it
   serves. The levels page counts what each System sees by default, and S3*
   is limited to `show`/`find` verbs.
2. **Does it make sense.** The inventory does. The 76-row `LEVELS` table
   does not, in compass. In ORE the table was the reviewed source, and the
   script wrote `#+level:` into every page. In compass the table is in C++,
   and `#+level:` in each page is the contract field the type registry
   already knows. Two sources of one fact, one of them in another
   repository's binary. Adding a skill to ORE fails `skill_level_tags` until
   compass is rebuilt. The split into two codecs is a symptom: the writer
   half collided with `structure_notes` (X5).
3. **Python accidents.**
   - The table is the accident: it duplicates the per-page keyword.
   - `#+level:` is read with `^#\+level:\s*(\S+)\s*$` anywhere. The
     keywords table gives the document keyword.
   - The S3* rule derives "read-only" from the third dash-segment of the
     name. That is a stated design ("derived from the verb register"),
     not an accident.
   - `skill_levels` is silent when the table and the tree disagree (X3).
4. **Cost.** Small: 76 skill pages read twice, once per codec.
5. **Shared data.** The skill listing and each skill's `#+level` keyword.
   The same as `skill_catalogue`.
6. **Recommendation.** Replace. The page's `#+level:` is the source. The
   inventory counts from the keywords. `skill_level_tags` becomes "level is
   one of the contract's levels", which the contract codec owns for every
   type. Drop `skill_level_tags`, and drop the table from compass. Review of
   a level change happens in the skill page's diff. Effort: small (ORE
   already carries correct levels). Saving: one codec, a 76-row table and
   the release coupling.

## structure_notes

1. **Intent.** A structure note orders a cluster of pages, and every page it
   orders links back to it in `See also`, so a reader who lands on a member
   finds the order.
2. **Does it make sense.** The invariant is sound. As a generator it is the
   wrong shape. It rewrites up to 417 hand-written pages to append one line,
   it must emit pages it does not change (so the sink does not delete
   them), and it is the reason other skill-page writers had to become
   checks. Back-links are derivable at export time, and the site could
   render them as org-roam does. Committing them is ORE's choice for agents
   that read raw files. If they stay committed, the right shape is a check
   with a fix: `missing_backlink` on the member page.
3. **Python accidents.**
   - Hub detection is `"structure_note"` as a substring of the first
     `#+filetags:` line, so a `structure_notes` tag also matches. The
     filetags keyword split into tags is exact.
   - The ordering cut is a text search for `"\n* What this cluster does not
     cover"` and `"\n* See also"`. Sections are any-level heading regex
     splits. `# aside` is a substring anywhere in a section body, including
     prose and src blocks. A comment element under the heading is the
     intended marker.
   - Links require a description (`\]\[[^\]]*\]\]`), so a bare id link in a
     hub orders nothing.
   - "Already links the note" is `text.find("id:" + hub_id)` (two cases),
     a substring test over the whole page, comments and src blocks
     included.
   - `frontmatter()` compiles a new regex on every call: twice per hub and
     once per page for filetags, so about 9,700 compilations per run.
   - Correct behaviour would change verdicts only where a hub uses bare
     links, a `structure_notes` tag exists, or `# aside` appears in prose.
     **guess**: none in ORE.
4. **Cost.** A corpus function over every document (X4). It copies every
   UTF-8 `doc/` page (about 62 MB) into a map and regex-scans each one.
   **Every org edit reruns it.** Python: 0.45 s. **guess**: 150 to 300 ms in
   compass, on every edit-hook run that includes it.
5. **Shared data.** Per page: ID, filetags, id-link targets with the
   enclosing section, and whether a link to X exists. This is the same
   per-page fact `lint` and `pattern_uses` need.
6. **Recommendation.** Replace with rules in the link-graph scan:
   `structure_note/missing_backlink` on the member, with a fix that the
   editor or `compass fix` applies. Drop the generator. That removes
   417-page ownership and the X5 clash. Effort: medium. Saving: the 62 MB
   copy per edit, and the producer conflict that forced the `skill_levels`
   split.

## recipe_inventories

1. **Intent.** `doc/recipes/shell/shell.org` lists every shell recipe by
   category, keeping hand-written prose and notes, and
   `generated_recipes.org` lists every generated HTTP recipe by category.
2. **Does it make sense.** These are directory listings of recipe pages. The
   site and agents could read them from a query (`compass list` already
   exists in the read-only command set) or from export-time generation.
   Committing them is ORE's choice. If kept, they should be one codec that
   shares its recipe facts with the recipe libraries.
3. **Python accidents.**
   - Two scripts read the same front matter two ways: the shell one keeps
     the *first* keyword and stops at the first heading, and the HTTP one
     keeps the *last* over the whole file. The codec preserves this with a
     `keywords` enum. The correct behaviour is the document keyword from
     the tables (org's own rule). **guess**: no verdict changes on ORE,
     where each recipe has one `#+title:`.
   - Pages are read as files and line-parsed. The ID is the first `:ID:`
     line anywhere.
   - The existing index is parsed back with `^\* ` and `^- \[\[id:` regexes
     to recover prose and notes. The tables give the headings, items and
     links.
   - The HTTP index head is compiled in (X2). The shell index keeps its head
     from the page, so the two halves differ for no reason.
   - Category order is first-seen in pathlib walk order for shell, and map
     order for HTTP. The shell order is a walk-order accident of the same
     kind the generator investigation called out. Sort by heading.
   - On a merge conflict it is silent (X3).
4. **Cost.** 581 + 95 recipe pages read as files and line-scanned, plus two
   index pages. **guess**: tens of ms cold.
5. **Shared data.** Recipe ID, title, filetags and the path category: the
   same pages and keywords `*_recipe_library` reads from the corpus.
6. **Recommendation.** Simplify and merge. One recipes codec (or a shared
   prepare step) reads each recipe page's keywords from the tables once,
   and feeds the libraries, both inventories and the provenance rule. Keep
   heads from the pages. Effort: medium. Saving: 676 file reads and line
   parses, and two compiled-in heads.

## backlog_indexes

1. **Intent.** Each backlog bucket page (`inbox`, `next`, `deferred`,
   `discarded`) has a table of the captures filed in its directory.
2. **Does it make sense.** Weakly. compass already has `inbox`, `next`,
   `deferred` and `discarded` read-only commands (they are listed in
   `skill_type_tables`' own table). The committed table is a stale-prone
   copy, as shown by ORE's stale `inbox.org` at `545cafca45`. For the site,
   export-time generation suffices.
3. **Python accidents.**
   - **The four page heads are compiled into compass** (X2), including
     `#+updated:` dates. Any hand edit to a bucket page's prose is drift that
     compass reverts. The head belongs to the page. Replace only the
     `# BEGIN AUTOGENERATED` region, as the other generators do.
   - Front matter is read by a hand line scanner ("the first line with a
     value; stop once every field is seen"). The tables give the keywords
     and the document property.
   - Titles are sorted with `ascii_lower`, which is Python's `str.lower()`
     only for ASCII.
   - Width padding in characters is faithful, but it is a cosmetic accident
     that org's own table alignment (`org-table-align`) would decide
     differently for wide characters.
4. **Cost.** A few hundred capture pages read as files. Small.
5. **Shared data.** Capture keywords (ID, title, description, filetags):
   the same keywords every other generator reads.
6. **Recommendation.** If ORE keeps the pages, simplify to region-only, with
   heads read from the pages and captures from the tables. Better, drop the
   committed table in favour of the existing commands and generation at
   export. Effort: small. Saving: removes 4 compiled-in pages and a known
   source of drift.

## physical_inventory

1. **Intent.** The physical-space pages of the template library (the `ores`
   root, technical spaces, `ores.*` facets) each have a child table of what
   sits below them (spaces, facets, archetypes) with counts and
   descriptions.
2. **Does it make sense.** These are reverse links: each child declares its
   parent (`#+facet_group`, `#+facet`), and the parent table is derived from
   them. Like the backlog, it could be rendered at export. If committed, a
   region generator is fine.
3. **Python accidents.**
   - Keywords are parsed by a line regex up to the first `* ` line, and the
     ID by `^:ID:` anywhere.
   - "Real" spaces and facets are those whose *title* starts with `ores.`.
     That is a naming heuristic standing in for a type distinction. A
     property or type would say it.
   - The target table is "the first run of lines starting with `|` in the
     section", found by regex. The section is `^\* Heading\n(.*?)(?=^\* |\z)`.
     The tables give the table element of the section exactly.
   - It reads each page twice, once to parse and once to edit.
   - Correct behaviour would not change ORE verdicts (**guess**).
4. **Cost.** The templates directory, about 100 pages. Small.
5. **Shared data.** Template page keywords. The template library is also
   read by `skill_type_tables` (`doc_*.org.mustache`).
6. **Recommendation.** Simplify to read keywords and table ranges from the
   tables. Raise with ORE whether this belongs at export. Effort: small.
   Saving: small. The gain is correctness, not time.

## trade_type_coverage

1. **Intent.** The coverage note's generated block states, per ORE trade
   type, what ORE does with it and what ORE Studio holds, and which task of
   the coverage story closes each gap.
2. **Does it make sense.** Partly. It is pinned to one story directory and
   three task UUIDs of `sprint_30` (still `STARTED` at `da9ee3cece`,
   **measured**). When the story closes, the "Closed by" column is a frozen
   report, and the generator either goes stale or stops. The coverage
   counts are durable. The task column is agile, not knowledge, which is
   the same mixing `lint`'s `durable_link` rule warns about.
3. **Python accidents.**
   - The story path and task IDs are compiled in (X2).
   - Every "the script refuses" path returns silently (X3). That includes a
     disagreement between catalogue and ORE forms, which is exactly the
     defect the note exists to expose.
   - The catalogue table is read by `starts_with("| ")` lines under a
     `* Trade types` heading. A task's State is read by
     `\| State\s+\| (\w+)`. The tables give the table rows and cells.
   - Task matching is `\b<type>\b` over descriptions, a heuristic, but a
     stated one.
4. **Cost.** One TSV, one catalogue page, about 20 task pages, and one
   existence check per type. Small.
5. **Shared data.** Task `* Status` tables (the agile codecs read these),
   and the trade catalogue page.
6. **Recommendation.** Keep the counts and the table. Move the story and
   task IDs into the note, for example as a property on the block heading.
   Turn every refusal into a finding, and drop the "Closed by" column when
   the story closes. Effort: small. Saving: none in time. It removes a
   silent failure.

---

## Shared passes

What each gate reads, and how it tokenises it today. Gates in one group
should share one pass.

### A. Per-page org hygiene, one page at a time (document codec)

| Gate | Reads today | Tokenises by |
|---|---|---|
| `org_links` | links table | parse |
| `heading_numbers` | source lines | 3 regexes + depth counter |
| `component_docs/backtick_link` | file bytes of `doc/` pages | 4 regexes per line |
| `lint/bad_filetag`, `unclosed_marker`, `malformed_link` | file bytes of every `.org` | 5 regexes per line |
| `required_sections` (compass.codec) | headings table | parse |
| contract fields (`missing_id`, `missing_type`, `missing_description` of `component_docs`) | overview file bytes | 3 regexes |

One document codec over the tables: headings (numbered titles, required
sections), keywords (filetags syntax, type, description, level validity),
the document property (ID), links (colon or unresolved fuzzy, malformed
id path) and comment elements (markers). It is incremental per page with no
reduce. This retires the two whole-corpus regex lexers, `lint`'s map and
`component_docs`' map, which are the ~1 s cold costs.

### B. The id-link graph (one scan, one reduce)

| Gate | Fact it needs per page | Tokenises by |
|---|---|---|
| `lint/dangling_link`, `durable_link` | IDs (all `^:ID:` lines), type, id links | regex, per line and whole text |
| `pattern_uses` | top ID, type, id links by level-1 section | regex; links need a description |
| `structure_notes` | top ID, filetags, id links by section, `# aside`, "links X?" | regex; links need a description |
| `skill_recipes` | Recipes-section id links | regex + substring |
| (runtime) `corpus_index` / `find_id` | ID → document | ingest |

One scan over corpus documents. The map emits (path, top ID, heading IDs,
type, filetags, id links each with offset and enclosing level-1 section,
and aside markers) from the tables. The reduce resolves `dangling_link`,
`durable_link`, the pattern rules, `structure_note/missing_backlink` and
duplicate IDs. An edit maps one page, and the reduce is skipped when the
fact is unchanged. This replaces two whole-corpus copies that rerun on every
org edit. It needs either one codec with all these rules, or the runtime's
shared-prepare support that "Combine gates that read the same data" owns.

### C. Skills (one model of `doc/llm/skills`)

`skill_namespace`, `skill_catalogue`, `skill_type_tables` (its two pages),
`skill_recipes`, `skill_levels` and `skill_level_tags` each list the skills
and reread `SKILL.org` files as text with their own regexes for ID, title,
`description:`, `#+level:` and the Recipes section. One skills codec with one
model (name, ID, title, export-block description, level) built from the
tables. It has rules (namespace, level valid) and one region generator (the
catalogue and the levels inventory). `skill_recipes` and `skill_level_tags`
go. `skill_type_tables` moves to a type-registry render.

### D. Recipes (one model of `doc/recipes/{shell,http}`)

`shell_recipe_library` and `http_recipe_library` read the recipes from the
corpus (blocks via `ops::tangle` plus regexes). `recipe_inventories` reads
the same pages as files with a line parser. `shell_recipe_provenance` reads
the 5,015 outputs back. One model per recipe (ID, title, description,
category, blocks and their targets) from the tables. It feeds the libraries,
the two inventories and, until the runtime owns output directories, the
provenance rule.

### E. Front matter of known page sets (generators)

`backlog_indexes` (captures), `physical_inventory` (templates),
`trade_type_coverage` (tasks), `skill_catalogue` (skills),
`recipe_inventories` (recipes) and `skill_type_tables` (templates) all
implement "read `#+title`, `#+description`, `#+filetags` and `:ID:`" with a
private line scanner or regex, each with a different rule for duplicates
(first, last, first-until-heading). These belong to one helper over the
keywords and properties tables, with org's own rule for a repeated keyword.
That is a small shared op, not a merged codec.

### F. Checkout listing (tree gates)

`component_docs` (`projects/`), `prototypes` (`doc/prototypes/`),
`skill_namespace` (`doc/llm/skills/`), `shell_recipe_provenance` (the
library) and `no_pr_triggers` (`.github/workflows/`) read listings. They are
cheap. The one gain is to narrow `component_docs`' `list_files("projects/")`
to patterns that an unrelated edit does not invalidate.

### G. Region rewriting

`ops::replace_region` builds a `.*?` dotall regex per call from the marker
strings. A `find` of the begin and end markers does the same without regex
compilation, and, through the comment elements, without matching a marker
quoted inside a block.
