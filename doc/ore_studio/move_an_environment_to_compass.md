# Move an ORE Studio environment to compass

This guide is for an agent working inside one ORE Studio environment (an
`ores_dev_*` worktree). It describes how to run ORE Studio's code generation and
build-free gates through compass, the C++ tool that replaces `ores.codegen`'s
regeneration and most of `local_checks.py`. The first environment to do this is
a pilot. It runs compass next to the existing tooling, and nothing is removed
until the two have been seen to agree.

compass is developed in `~/Vault/Development/masd/compass` (GitHub
`MASD-Project/compass`). Its rule table and its gate classification, both
referenced below, are in that repository. Read them there; don't copy them
into ORE Studio.

## What compass gives you

Measured on a copy of ORE Studio at `545cafca45` (8 cores; load given where it
mattered):

- **Code generation.** `compass-ng generate` writes all 26,212 files `ores.codegen`
  generates, byte-identical to its formatted output.
  - A warm run with no template change takes under 3 s and starts no formatter.
  - A cold run takes 23 to 31 s, most of it clang-format and prettier.
  - Generation is incremental: a model edit re-renders only the models that
    read it, and a template edit only that template's outputs.
- **Gates.** `compass-ng check` runs 76 of ORE Studio's gates in one pass, in
  about 3.5 s warm at load 4. As scripts, those gates take about half an hour.
  - Each ported gate gives its original's verdict on the same commit, both as
    the tree is and with planted violations.
  - Generator gates (tangles, inventories, seeds, the cmake source lists and
    others) are codec outputs, so their drift is a `compass-ng check` finding and
    `compass-ng generate` replaces the script.
- **Ratchet.** `compass-ng check --ratchet [<base>]` fails only on findings that
  are new since the merge base. The base defaults to the upstream, else
  `origin/main`.
- **Past commits.** `compass-ng check --commit <rev>` checks any commit straight
  from git's object store, without a checkout, with that commit's
  `.compass.org`. A commit that has none is checked with the working tree's,
  and compass says so on standard error
  (`compass: warning: the commit <rev> has no .compass.org; using the working
  tree's`); it never falls back to the defaults.
- **Inline exceptions.** A comment excuses one finding:
  `compass:except(<codec>/<rule>) expires=<YYYY-MM-DD> approver=<name> reason=<text>`.
  Expired, incomplete, malformed and stale exceptions are findings themselves.
- **Edit hook.** `compass-ng hook` checks the one file Claude Code just edited, in
  well under a second for most edits on a quiet machine.

compass does not export HTML yet. Keep the Emacs site export and the
`site-page` gate as they are.

## The binary

There is one compass, shared by every environment on the machine. Scripts run
it as `compass-ng`, from the PATH. It is not called `compass`, because ORE
Studio's Python tool (`./compass.sh`, `ores.compass`) answers to that name
until it is retired; both tools work side by side. ORE Studio runs its
Python tool as `./compass.sh`, so wherever ORE Studio's docs say
`compass <verb>` they mean `./compass.sh <verb>`; no bare `compass` is on the
PATH. `~/.local/bin/compass-ng` is a link to the
installed binary, `~/.local/opt/compass/compass`, which has its build manifest,
`compass.manifest`, beside it.

- **Which build is installed.** `compass-ng --version` prints one fact per line:

  ```
  compass 0.0.1
  commit: 6d1c0f2b9a1e4c55a0e3f1b7d2c8e9a4b5f6c7d8 (6d1c0f2)
  dirty: no
  branch: main
  committed: 2026-11-17T09:31:48Z
  built: 2026-11-17T09:46:18Z
  installed: 2026-11-17T10:02:11Z
  store formats: store layout 2, packs 1, dependency records 6, actions 5, written outputs 1, format cache 1, worktree index 1
  ```

  These are the release, the full and short commit the build was made from,
  whether that tree had uncommitted changes, its branch, the commit's time,
  when the binary was built and installed, and the versions of the records
  compass keeps. Paste the whole output with any result you report to the
  compass repository. A line reading `unknown` means the manifest beside the
  binary is missing or damaged; reinstall.
- **Updating.** Do not add compass as a submodule, and do not build it in the
  environment. The compass repository builds a commit and runs
  `build/scripts/install_compass.sh`, which replaces the binary and its
  manifest, each through an atomic rename, and keeps the link pointing at it.
  Every environment runs the new build from its next `compass-ng` command. The
  script refuses a build of uncommitted changes unless it is given
  `--allow-dirty`.
- The binary depends only on the system C and C++ runtimes.
- To see which codecs the installed build knows, run `compass-ng config`: any unknown codec
  named in `.compass.org` is an error that lists the known ones.
- compass keeps its store under the repository's git common directory
  (`<common dir>/compass/packs/`), shared by every `ores_dev_*` worktree.
  Per-worktree records live under each worktree's git directory.
- The store is a handful of pack files, not a file per object and result.
  Every run that computes something adds one pack, and small packs are
  merged into bigger ones as they are written, so the count stays near the
  logarithm of the store's size. A cold check of ORE Studio leaves one pack
  of about 620 MB beside four records. Any number of worktrees, checks and
  hooks may use the store at once: each writer publishes its own pack with
  an atomic rename, and only merging and `gc` take a lock.
- When a new binary changes the record format, the first run after it runs
  everything once (cold). That is expected. Compare the `store formats` line
  of the two builds' `--version` to see whether it will happen.
- **Store layout 2.** A build whose `store formats` line starts `store
  layout 2` does not read the old layout of one file per object, manifest
  and result (`compass/objects`, `compass/manifests`, `compass/results`).
  Its first check is cold, which every new build is anyway, since a new
  build changes the key of every stored result. Run `compass-ng gc` once
  after the install: it removes the old directories, about 189,000 files
  in a store a day old.
- Run `compass-ng gc` now and then. It keeps every entry a worktree's index
  or record names and everything written in the last hour, and rewrites
  the rest of the store into one pack without the entries nothing names.
  Formatted files are among those: a `gc` more than an hour after the last
  check drops them, and the next check runs clang-format and prettier again
  (about 50 s on ORE Studio).

## Step 1: add `.compass.org`

Create `.compass.org` at the repository root. It is an org page following the
ORE Studio document contract, of type `meta`, with these keywords and a codec
table:

```org
#+title: Compass configuration
#+description: The configuration compass reads at the root of ORE Studio.
#+type: meta
#+output: .
#+meta: doc/meta
```

Then a `* Codecs` section with one row per codec: name, `yes` or `no`, and
options as `key=value` pairs. Use these rows:

| name | enabled | options |
|---|---|---|
| document_index | no | |
| required_sections | no | |
| tangle | yes | `directory=projects/ores.codegen/library/templates` |
| tangle_check | yes | `directory=projects/ores.codegen/library/templates` |
| codegen | yes | `library=projects/ores.codegen/library convention_component=refdata year=2026 clang_format=clang-format clang_format_style=.clang-format prettier=projects/ores.web/node_modules/.bin/prettier,prettier prettier_style=.prettierrc.yaml` |
| component_files | yes | `year=2026` |
| ore_output_catalogue | yes | `clang_format=clang-format` |
| rule_table | no | |

`year` is the copyright year of the licence blocks that `codegen` and
`component_files` write. Both codecs require it: compass never takes the
year from the clock, so a run in January writes the same bytes as one in
December. 2026 is the year in the files ORE Studio has committed. To move to
a new year, change `year` in `.compass.org` and run `compass-ng generate`; the
change of year is then one commit.

`rule_table` writes a page listing every rule and the memories none enforces;
turn it on with `path=` naming a knowledge page once ORE Studio wants that
page. Also add every other codec `compass-ng config` lists, with `yes` and no options:
the C++, SQL, org, cross-artefact and generator gates. Keep
`document_index` and `required_sections` off; ORE Studio has its own index
and contract checks.

Check it with `compass-ng config`. Then run `compass-ng diff | head` to see what
`generate` would change, before running `generate` itself.

## Step 2: compare with the existing tooling (the pilot)

Run both sides on the same tree and record every disagreement. Nothing is
replaced in this step.

1. Run `compass-ng check`. Expect findings in two groups.
   - **Drift that is real in ORE Studio.** This list was taken at
     `545cafca45`; the tree moves on, so re-establish it at your commit. At
     `67803175ce` the pilot found 26 codegen files and 52 shell scripts (40
     new files for four families never tangled, and 12 changed), and a new
     hand edit: `market_fixing_repository.cpp` has a hand-written
     `read_latest_for_series` that `compass-ng generate` deletes, as
     `ores.codegen` would. Move such code into a template or a hand-written
     file before regenerating. `compass-ng generate` now warns when it
     deletes three or more lines its output lacks from a file compass did
     not write last (`compass: warning: <path>:<line>: generate deleted <n>
     lines its output lacks, from "<first line>"`); it still writes, so
     restore the lines from git and move them.
     - 24 codegen outputs. 22 were not regenerated after the system-tenant
       shared-read change (the 7 script entities' `*_create.sql`,
       `eventing/*_event.hpp` and `repository/*_repository.cpp`, and
       `tenor_convention_resolution_repository.cpp`). 2 are hand edits:
       `ores.marketdata.service/config/options.hpp` (`= 0` initialisers) and
       `ores.ore/core/tests/CMakeLists.txt` (`pugixml`).
     - 9 shell recipe scripts (`barrier_types`, `commodity_instruments`,
       `leg_types`; `-add`, `-set`, `-put-many`). The recipes gained a `-`
       argument and were never re-tangled. Emacs 30.2 writes the same 9 files
       compass does.
     - `backlog_indexes`: a capture from 2026-11-16 missing from the inbox
       index.
     - `route_inventory`: 19 rows with stale "Where" line numbers. The script's
       own `--check` ignores that column.
     - `er_diagram`: `ores_schema.dq_datasets.puml` predates a reworded
       table description in `dq_dataset_bundles_create.sql`.
     - Two orphan headers,
       `refdata.api/generators/curve_volatility_parametric_smile{,_parameter}_generator.hpp`,
       left by a model rename. compass does not write them, so it does not
       report them, but they should be deleted.
   - **Old baseline entries** that compass expects as inline exceptions,
     because it no longer reads the baseline files:
     - `entity_writes`: 136, from `entity_table_writes_baseline.json`.
     - `core_link_edges`: 37 live edges from `core_link_edges_baseline.json`.
       Its 2 entries for links that are gone are why the script fails today.
     - `rls_widenings`: 13 policies.
     - `template_sections`: 8 lines.
     - `money_decimal_columns`: 7 columns.
     - `forward_declarable_includes`: 1.
2. Run `./compass.sh check` (the Python gates) on the same tree. For every
   ported gate, compare its verdict with compass's findings for the matching
   codec. Codec names follow the gate ids with `-` as `_`. Exceptions: the
   drift gates (`component-drift`, `drift-sweep`) are `codegen/drift`;
   `reachability` is `test_case_reachability`; the tangle gates are `tangle`
   and `shell_recipe_library` and `http_recipe_library`; `cmake-sources` is
   `component_files`; `er-diagram` is two codecs, `er_diagram` for the
   diagrams and `rls_coverage` for the row-level security checks its
   `--strict` fails on (RLS_001 and RLS_002, read with the same
   `validation_ignore.txt`). Some gates and rules have no codec because
   another check covers them; see "Gates compass dropped" below. Compare
   their verdict with the check that covers them.
3. Time both. Record wall time, the load average and whether compass was warm.
4. Write the result into an ORE Studio task: each disagreement, its cause, and
   whether compass or the script is right. Send compass bugs back to the
   compass repository. Don't work around them in ORE Studio.

## Step 3: fix the drift and move baselines to exceptions

This is ordinary ORE Studio work, done on a branch and merged through a PR.

- **Regenerate.** Run `compass-ng generate`, then review and commit the drift
  files that are real: the 22 codegen files, the 9 shell scripts, the backlog
  index and the route inventory. The 2 hand edits need a decision: either move
  the change into the template or model, or drop it.
- **Delete** the 2 orphan generator headers and their `component_files.cmake`
  entries.
- **Move baselines to exceptions.** Turn each baseline entry into an inline
  exception on the line it covers. Use `compass:except-file(...)` for
  whole-file entries such as entity writers in C++ files. Give each one a
  real reason, an approver and an expiry. Then delete the baseline file once
  the script that reads it is no longer run.
- After this step, `compass-ng check` should report nothing on that branch.

## Step 4: switch the environment over

Only after step 2 shows agreement:

- **Gates.** Make `./compass.sh check` run `compass-ng check` for the ported
  gates, and keep running the Python script only for the gates compass does
  not cover:
  - not ported: `boost-deps` (reads the vcpkg submodule),
    `xml-benchmark-recorded` (needs files at commits named in `history.tsv`),
    `format`, `no-committed-credentials` (reads the git-ignored `.env`), and
    the `--self-test` steps;
  - kept in Python for now: `xml-bindings-drift`, `file-types-seed` and
    `ore-parameters-seed` (see "Fixes to make in ORE Studio" below);
  - `ore-element-map`, `ore-parameter-value-sets` and
    `parameter-value-sets-only` are one compass model: the `ore_value_sets`
    codec writes their three seeds and `parameter_value_sets` reports their
    rules. They need the `r.id` fix below before either side passes;
  - always kept: build, database and test gates (31 of them), and
    `site-page`;
  - retired with no replacement: the gates in "Gates compass dropped".

  The compass repository's gate classification
  (`doc/agile/versions/v0/sprint_01/run_the_gates_as_codecs/evidence/gate_classification.org`)
  lists every gate and its class.
- **Ratchet.** Both sides of the ratchet are checked with the working
  tree's `.compass.org`, so the base is judged by the rules HEAD is judged
  by, whether or not the base has a `.compass.org` of its own. For pull
  requests,
  `compass-ng check --ratchet origin/main` is the
  gate: it fails only on new findings.
- **Regeneration.** Replace `regenerate --all` with `compass-ng generate`. Keep
  `regenerate_cmake_component_files.py` and the PlantUML script until
  `component_files` and `er_diagram` are confirmed; `component_files` already
  covers the former.
- **CI.** Leave CI on the Python gates for now. CI cannot use a binary on this
  machine; it needs a published compass release first.

## Gates compass dropped

compass's review of the ported gates found some that repeat another check.
compass no longer has these codecs or rules. Remove the rows of a dropped
codec from `.compass.org`: `compass-ng config` rejects a codec it does not
know. An inline exception that names a dropped rule no longer parses, so
remove it too.

| Gate or rule | What covers it |
|---|---|
| `paste-blocks` (codec `paste_blocks`) | `codegen/drift`, when the model's block and the generated file disagree; the build of the component, when both name a symbol that is gone. |

## Fixes to make in ORE Studio

compass's review of the gates it did not copy found defects in ORE Studio
itself. Fix them in ORE Studio as ordinary work:

- **`r.id` in the value-set code.** At `545cafca45` the value-set generators
  stop on `r.id` in `conventions_fx_projection.cpp`, which breaks four SQL
  generators and the three value-set gates. Adding `("CURRENCY_PAIR", "r",
  "id")` to `NOT_A_COLUMN` makes them pass, and with it compass's
  `ore_value_sets` writes the committed seeds byte for byte. Decide whether
  that exemption or a source fix is right.
- **xml-bindings-drift runs everything twice.** Its second, determinism run
  proves nothing: the output follows the schema's document order and no output
  loop runs over a set. Dropping it halves the gate. Most of what remains is
  clang-format (2.2 s for `portfolio.cpp` alone).
- **file-types-seed depends on the locale and on directory order.** Its
  manifest digest uses the order `sort -z` gives, so under `LC_ALL=C` the gate
  fails on a clean tree; use byte order. Separately,
  `scenarioGeneration.scenarioOutputFile` is a three-way tie broken by
  `os.walk` order; curate it as `stress_scenarios.csv` rather than relying on
  either order. The vendored manifests' `tree_sha256` values change once when
  the digest moves to byte order.
- **ore-parameters-seed keeps its texts in Python.** About 7,800 lines of
  text and logic; it stays a Python gate until those texts move into data
  files compass can read.

The full reasoning for each is in the compass repository's
`doc/agile/versions/v0/sprint_01/run_the_gates_as_codecs/evidence/remaining_generator_gates.org`.

## Step 5: the edit hook (optional, per environment)

Claude Code runs `compass-ng hook` after each edit. Add this to the environment's
Claude Code settings through ORE Studio's own settings mechanism (they are
tangled from an org page there; don't hand-edit the generated JSON):

```json
{
  "hooks": {
    "PostToolUse": [
      {
        "matcher": "Edit|Write|MultiEdit",
        "hooks": [
          { "type": "command", "command": "compass-ng hook", "timeout": 30 }
        ]
      }
    ]
  }
}
```

What the hook does:

- It reads the PostToolUse payload, checks only what the edited file feeds,
  and returns a `block` decision with the findings. That shows them to the
  agent; it does not undo the edit.
- It fails open, with a note, on any internal error.
- It reports only findings that are new since the last `compass-ng check`:
  a finding the record of that check holds, on the edited file or another,
  does not block, even when the edit moved its line. The hook says how many
  old findings it left out, and `compass-ng check` lists them. So the hook
  can go in before the baselines become inline exceptions (step 3). To see
  every finding on the edited file instead, use
  `compass-ng hook --all-findings`.
- It writes nothing, so it trusts the record of the last `compass-ng check`. Run
  a full `compass-ng check` after pulls and rebases.
- Warm times at load 2 to 4: most edits take 0.2 to 0.9 s, and a new model
  takes about 1.1 s. A loaded machine can push model and C++ header edits to
  1.4 to 1.7 s.

## Rolling out to the fleet

`.compass.org`, the regenerated files, the exceptions and the script changes
are all committed, so other environments get them as they rebase onto the
merged branch. Each environment then needs only:

1. `~/.local/bin/compass-ng` present (shared by all environments on this
   machine).
2. One full `compass-ng check` to build its records. The store is shared, so
   this is mostly cache hits after the first environment.
3. The hook, if wanted, in its own Claude Code settings.

## When something is wrong

- **compass disagrees with a script.** Record both outputs and the commit in
  the ORE Studio task, and report it to the compass repository. Keep the
  script as the gate for that rule until the fix lands.
- **The `compass` binary is missing or crashes.** Fall back to
  `./compass.sh check` and `regenerate`, which this migration does not remove
  until the Retire legacy tooling work.
- **A run reports a whole tree of drift after a compass upgrade.** Run
  `compass-ng generate` into a scratch directory with `--out`, and compare it
  before writing anything. Report it if the outputs really changed.
