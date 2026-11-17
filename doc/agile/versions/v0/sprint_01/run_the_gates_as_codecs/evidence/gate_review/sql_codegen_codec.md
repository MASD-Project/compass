# Review: the SQL, codegen and codec-runtime gates

Scope: every codec in `projects/compass.sql/src/codecs/`,
`projects/compass.codegen/src/codecs/` and `projects/compass.codec/src/codecs/`,
with the ops each uses. The originals were read in
`~/Development/OreStudio/ores_dev_prime_origin` (HEAD `da9ee3cece`).

This was a read-only review. Nothing was built or run in either repository.
Every claim carries one of three labels:

- **measured**: counted on the ORE tree with `grep`, `find` or `du`;
- **read**: read from the source;
- **guess**: inferred, not measured. A guess about time needs a profile
  before anyone acts on it.

ORE sizes this review relies on (measured, at `da9ee3cece`):

| Tree | `.sql` files | Size |
|------+--------------+------|
| `projects/ores.sql/create` | 1,473 | 11 MB |
| `projects/ores.sql/populate` | 449 | 56 MB, of which 10 data files are 1.1 to 10 MB each |
| `projects/ores.sql/drop` | ... | 5.9 MB |
| all of `projects/ores.sql` | 3,475 | 85 MB |

Other counts (measured): `create/` holds 913 `create policy` statements in 439
files and 917 `drop policy if exists`. `populate/` holds 37,391 calls to a
`*_fn`, of which 32,290 are `ores_utility_system_tenant_id_fn`. Every function
the create scripts define ends in `_fn`; none is schema-qualified and none is a
plain `create function`. `\ir` appears 3,375 times and `\i` only in the
top-level scripts. The corpus holds about 11,000 org documents outside
`external/`, of which 226 are the codegen templates.

---

## compass.sql

The six codecs share one data set: the `.sql` files under
`projects/ores.sql`. They read it six ways:

| Codec | Shape | Reads | Tokenisation |
|-------+-------+-------+--------------|
| `sql_hygiene` | scan, no reduce | all 85 MB, `.sql` under `roots` | four hand-written lexers (`unterminated_comment`, `duplicate_indexes`, `split_concatenations`, `mask_non_code`), each a full pass over the file |
| `sql_reachability` | scan with reduce | all `.sql` under `projects/ores.sql` | one regex `^\s*\\(i(?:r)?)\s+(\S+)` on raw text, which matches inside comments and bodies |
| `populate_refs` | corpus function | `create/` (11 MB), then `populate/` (56 MB) | `blank_comments` (a third comment lexer), `universal_newlines`, then regex over the joined text |
| `rls_widenings` | corpus function | `create/` (11 MB) | regex `--[^\n]*` to blank line comments (blind to strings and block comments), then a lazy `(.*?);` regex |
| `handler_permissions` | corpus function | C++ handler files, one populate script | regex |
| `curve_parameter_paths` | corpus function | one 1.6 MB populate artefact, the curve XSDs, one JSON file | regex over XML and JSON |

Outside this scope, six `compass.crosscheck` codecs read the same `create/`
tree with their own regular expressions: `er_diagram` (through
`ops::er_model_of`), `model_sql_types`, `generated_column_refs`,
`entity_binary_types`, `marketdata_identity_columns` and `entity_writes`. All
but `entity_writes` are corpus functions. So an edit to one create script
reruns about seven corpus codecs, and each one rereads and re-tokenises the
whole tree (read). This is the main reason a SQL edit costs 0.7 to 1.2 s.

### sql_hygiene

1. **Intent.** Every SQL file can be run twice, and its text means what it
   seems to. Block comments close. No comment splits two string literals,
   which PostgreSQL joins only across whitespace. The same index is not
   created twice. DDL is idempotent (`if not exists`, `or replace`, or a
   `drop ... if exists` of the same policy, type or domain). ORE's
   `setup_schema.sql` runs psql with `ON_ERROR_STOP`, and the schema is
   recreated in place, so a non-re-runnable statement breaks the recreate.

2. **Does it make sense.** Yes. The database does not catch these until a
   second run, and a split literal or an open comment changes meaning without
   any error. Most of these rules are properties of one statement, so a real
   tokeniser makes them simple. The four rules each re-lex the file
   separately, and three other lexers exist in this component
   (`ops::blank_comments`, the `rls_widenings` regex, and `read_balanced` /
   `split_top`). One lexer that returns token spans (code, line comment,
   block comment with nesting, `'...'`, `E'...'`, `"..."`, dollar body with
   its tag) would serve all four rules and every other SQL gate.

3. **Python accidents copied.**
   - `create  table  if not exists`, with two spaces before `if`, is
     reported as not re-runnable. The regex `\s+(?!if\s+not\s+exists)` gives
     a space back so that the lookahead fails. The component overview keeps
     this on purpose. **Correct behaviour:** compare token sequences. This
     removes only false positives. ORE's gate is green, so it changes no
     verdict at this commit (guess: any such line would already fail and
     have been fixed).
   - `duplicate_indexes` runs on the raw text, not the masked text. An index
     in a commented-out block counts toward a duplicate. **Correct:** run
     on code tokens only. This could only remove findings.
   - The `create index` re-runnability test reads the next 48 bytes of
     masked text, lower-cases them, and checks a prefix. It works, but it is
     a regex stand-in. A token test is simpler.
   - `create policy` is satisfied by a `drop policy if exists` anywhere in
     the file, even after the create. **Correct:** the drop must precede the
     create in the same file. A drop placed after the create would now be
     reported. Verdict change on ORE: unknown, and cheap to count once the
     lexer exists.
   - The lexers do not treat `\` inside `E'...'` as an escape. No ORE file
     has `E'...\'...'` (measured: 0 files), so there is no verdict change.
   - DDL inside dollar bodies (`do $$ ... create table ... $$`) is masked,
     so it is never checked. This is a deliberate scope choice, and it is
     the right one for a text check.
   - Messages drop the script's counts, which is already a fix.

4. **Cost.** A scan with per-file findings, so an edit maps one file (read).
   The cold run lexes 85 MB four times (read), plus `starts_of` once each
   for `create` and `drop`. One lexer would cut the cold cost of this codec
   to roughly a third (guess). The warm cost and the edit-hook cost are
   already minimal.

5. **Shared data.** The bytes of every `.sql` under `projects/ores.sql`,
   lexed for comments, quotes and dollar bodies. These codecs need the same
   tokens: `sql_reachability` (`\i` lines outside comments),
   `populate_refs` (`blank_comments`, balanced parentheses, top-level
   commas), `rls_widenings` (comment blanking, `create policy ... ;`),
   `ops::er_model_of` and the five crosscheck codecs that parse
   `create table`.

6. **Recommendation.** **Keep, and make it the host of the shared SQL
   lexer.** Replace the four passes with one token pass, and move the
   statement-level rules onto tokens. Effort: medium. Saving: about two
   thirds of this codec's cold time (guess), nothing warm. The real saving
   is that every other SQL gate below can be built on the same tokens.

### sql_reachability

1. **Intent.** Every `.sql` under `create/`, `populate/` and `drop/` is
   reached by an `\i` or `\ir` walk from the root scripts and four named
   entry points, and every include names a file that exists. A table whose
   create script no aggregator includes is never created, and the first
   symptom appears hundreds of files later (from the ORE docstring).

2. **Does it make sense.** Yes. The database cannot report a file that
   never ran. The walk, however, uses a stack with `pop_back`, so it visits
   files in reverse include order, not in psql's execution order. That is
   fine for reachability, but it throws away the one fact that two other
   invariants need: what runs before what. `populate_refs` needs it ("define
   the row in a populate script that runs before this one", its own fix
   text, which it never checks). The reachability docstring also names the
   ordering failure ("names its tables in dependency order"). A recursive
   walk in include order gives the psql execution sequence at the same cost.

3. **Python accidents copied.**
   - The include regex matches `\i` inside block comments and dollar bodies,
     and does not know psql's long forms `\include` and `\include_relative`,
     or a quoted path. ORE uses only `\ir` and `\i` (measured), so there is
     no verdict change today. **Correct:** read meta-commands from the code
     tokens of the shared lexer, at line start.
   - Branches of `\if` / `\else` count as reached (psql meta-commands, 51
     `\if` in ORE, measured). That is the right call for "is it ever
     reachable".
   - `decode_ignoring` mirrors Python's `errors="ignore"`. That is harmless.

4. **Cost.** A scan: the fact is the include list per file, and the reduce
   walks the graph (read). An edit maps one file. The reduce is skipped
   when that file's include fact did not change, which is almost every edit.
   This is already the right shape.

5. **Shared data.** The include lines of every `.sql`. `populate_refs` would
   use the execution order this walk can produce. `ops::er_model_of` takes
   "the create scripts, in the order given", which is path order today. The
   include order is the order psql actually runs them in.

6. **Recommendation.** **Keep. Switch to an in-order recursive walk and
   publish the execution order** as part of the shared SQL facts, so that
   `populate_refs` can check definition-before-use. Read includes from code
   tokens. Effort: small. Saving: none by itself. It enables the
   `populate_refs` correctness fix below.

### populate_refs

1. **Intent.** Every name a populate call looks up (a methodology, a
   dataset, a tag) is a name some populate call defines. Each upsert
   function raises "not found" on a miss, and psql runs with
   `ON_ERROR_STOP`, so one unresolved name aborts the recreate and skips
   the GRANT block (ORE docstring). The contract is inferred from the
   function bodies: a `raise ... not found` after a `select ... from T
   where col = p_x` makes `(T, col)` a mandatory lookup, and an
   `insert into T (col) values (p_y)` in another function defines it.

2. **Does it make sense.** The invariant is real, and inferring it from the
   function bodies is a good idea. As written, though, the gate checks less
   than its own fix text says:
   - **It ignores order.** A row defined by a populate script that runs
     *after* the lookup still satisfies the check, but psql aborts on it.
     This is the actual failure the gate exists for.
   - **Plain `insert into T` rows in populate scripts, and rows seeded by
     create scripts, are not definitions.** Only calls to an inserting
     function count. A row inserted directly would be a false positive.
     None exists today, since the gate is green (guess).
   - **The guard heuristic is fragile.** It takes the last `select ... from
     (\w+) where ... ;` before each raise. A table alias (`from t x
     where`), a join, or a `select into` with the table later in the
     statement makes the regex miss, and the guard is silently dropped. A
     dropped guard is a false negative, which nobody sees.
   - **Literal arguments only.** `'x'::text` or `E'x'` is not literal, so
     the call is skipped silently.

   None of this duplicates the database: only a full recreate finds these
   failures, and it takes minutes. Keep the gate and fix its model.

3. **Python accidents copied.**
   - **The two-pass shape.** The populate pass builds one alternation regex
     from the "interesting" function names found in the create pass. That
     regex is only a filter: it is equivalent to "find `\w+\s*(`, then keep
     the name if it is in a set". Every function ORE defines ends in `_fn`
     (measured), so "every `\w+_fn\s*(` call with its arguments" can be
     extracted from a populate file without knowing anything about
     `create/`. **The second pass does not need the first.** That is what
     makes a scan possible.
   - **Line counting is quadratic.** Each call counts newlines from the
     start of the file (`std::count(text.begin(), text.begin() + start)`, a
     copy of `text.count("\n", 0, start)`). `offset_of_line` then walks the
     bytes again for every call. For a file with thousands of calls, that is
     O(calls x file size). **Correct:** carry the offset; the finding needs
     only the offset.
   - **The select search is quadratic per body.** `select_re.find_all(body
     .substr(0, raise.start()))` reruns a lazy, dot-all regex over the
     prefix of the body for every raise.
   - `call_tuples` scans every call for every (guard, candidate) pair. Index
     the calls by function name instead.
   - All create files are joined into one string, which costs a copy, and
     the code needs a binary search to map an error offset back to a file.
     A function cannot span files, so parse per file.
   - `unbalanced_parentheses` stops the whole gate at the first bad call,
     copying the Python traceback. **Correct:** report it and continue.
   - Only `create or replace function` is recognised. That matches ORE
     today (measured), but plain `create function` would be invisible.
     `sql_hygiene` reports a bare `create function` anyway, so this does not
     matter much.

4. **Cost.** About 410 ms per SQL edit (given). It is a corpus function
   whose inputs are both listings and every file under `create/` and
   `populate/`, so any edit under either tree reruns it in full. That means
   reading 67 MB, copying it twice (`universal_newlines`, `blank_comments`),
   and running a case-insensitive alternation regex over 56 MB of populate
   SQL, most of it data files with no calls of interest (read and measured
   sizes; the split of the 410 ms between these steps is a guess).

   **Is the design necessary? No.** Split it into per-file facts:
   - for a create file: each function with its parameters, its guards
     `(table, [(column, param)])` and its inserts `(table, {column:
     expression})`;
   - for a populate file: each `*_fn` call with its offset and the literal
     value (or "not literal") of each top-level argument. ORE has about
     5,000 such calls outside the 32,290 argument-free tenant-id calls
     (measured), so the facts are small. Drop calls that have no literal
     argument.

   The reduce joins guards, definers and calls in memory. An edit to a
   populate file maps that file, and the reduce runs only when its call
   fact changed. An edit to a create file maps that file and reruns the
   join over stored facts. Expected edit cost: one file's lex plus a join
   over about 5,000 rows, a few ms (guess). Expected saving: about 400 ms
   of the 0.7 to 1.2 s SQL edit (guess, needs a profile to confirm).

5. **Shared data.** The comment-blanked text of `create/` and `populate/`,
   balanced parentheses and top-level comma splitting. `rls_widenings` and
   `ops::er_model_of` read the same create scripts, and the function bodies
   are also what `entity_writes` scans for DML. `sql_reachability` gives the
   execution order this gate should check against.

6. **Recommendation.** **Replace by a scan** over the shared SQL facts, and
   add the order rule: a lookup is satisfied only by a definition earlier in
   the execution order `sql_reachability` computes. Fix the quadratic line
   counting even if nothing else changes (small). Effort: medium. Saving:
   about 400 ms per SQL edit and most of the cold cost (guess). The order
   rule may add findings on ORE. That is the gate doing what its fix text
   already claims.

### rls_widenings

1. **Intent.** No row-level security policy lets a system-tenant session
   see every tenant's rows (`current = system` on the session side). A
   service that needs a tenant's data acts inside that tenant instead (ORE
   docstring, plus the three rule sources).

2. **Does it make sense.** Yes. The database enforces policies; it does not
   judge them. The docstring itself says the check is a guard for two
   spellings, not a proof. It is a per-file property, though, and it is
   written as a corpus function that rereads all of `create/` on any create
   edit. `sql_hygiene` already finds every `create policy NAME on TABLE` in
   masked text. The widening test is one more predicate on that statement.

3. **Python accidents copied.**
   - Comments are removed with `--[^\n]*`, which also cuts a `--` inside a
     string literal and leaves block comments in place. A `create policy`
     inside a `/* */` block would be reported. **Correct:** use code tokens.
     Verdict change on ORE: none expected (guess).
   - The policy body ends at the first `;`, even one inside a string.
     Statement splitting on code tokens fixes it.
   - Only two textual spellings of the comparison are recognised, with
     whitespace allowed. `IN (...)`, a negation, or a wrapper function slips
     through (documented in the original). With tokens, the operands of `=`
     can be normalised. That is the cheap improvement; a real expression
     check is not worth it.
   - The exception table moved to inline exceptions, which is a good change.
     ORE has no `compass:except(rls_widenings/...)` comment yet (measured:
     0), so ORE must add them before the codec replaces the script.

4. **Cost.** Corpus function: list `create/`, read 11 MB, run a regex
   substitute that allocates a copy per file, then a lazy dot-all regex.
   It reruns on every create edit. Tens of ms (guess).

5. **Shared data.** The same masked create text and `create policy`
   statements that `sql_hygiene`'s `not_re_runnable` already parses.

6. **Recommendation.** **Merge into the SQL scan** (as a rule of
   `sql_hygiene`, or of the combined SQL scan): a per-file finding with no
   reduce. If it stays its own codec for the rule name, make it a scan with
   no reduce at least. Effort: small. Saving: its whole edit-hook cost,
   tens of ms per create edit (guess). Merging renames the rule to
   `sql_hygiene/system_tenant_widening` or similar, so inline exceptions
   written before the merge would need the new name.

### handler_permissions

1. **Intent.** Every permission code a NATS handler of a registered
   component checks with `has_permission(ctx, "code")` is seeded by
   `iam_permissions_populate.sql`. An unseeded code denies every caller,
   administrators included, and looks like a broken feature.

2. **Does it make sense.** Yes. The database cannot know which codes the
   C++ asks for. Most handlers are generated: `cpp_nats_handler.hpp.mustache`
   builds the code from `{{component}}::{{entity_plural}}:{{permission}}`
   (read). The codegen contexts therefore know the generated codes exactly,
   without a regex. Hand-written handlers still need the text scan, though,
   so one regex pass over handler files remains the simplest complete
   source. Keep it.

3. **Python accidents copied.**
   - The seed regex matches any quoted `a::b:c` anywhere in the seed file,
     including comments. A commented-out seed counts as seeded.
     **Correct:** strip SQL comments (shared lexer), or better, read the
     first literal argument of each `ores_iam_permissions_upsert_fn` call,
     which is a `populate_refs` call fact. That is a possible verdict change
     (fewer codes seeded), cheap to count.
   - `has_permission` in a C++ comment counts. This is rare and harmless
     (a false positive at worst).
   - A built code `"prefix" + x` becomes `prefix*`. Any seed with the prefix
     satisfies it, scanned linearly over the set. Use `lower_bound` on the
     sorted set. This is trivial.
   - The component list is an option, matching ORE's REGISTRY. That is fine.

4. **Cost.** Corpus function: about ten listings over `projects/ores.<c>/`
   with two suffixes each, then reading the handler files and the seed file.
   It reruns when any handler or the seed changes. This is small (ORE
   measured the original at 0.2 s; compass is likely a few ms, guess).

5. **Shared data.** The seed file is a populate script that the
   `populate_refs` call facts already cover (`ores_iam_permissions_upsert_fn`,
   1,683 calls, measured). The handler `.hpp`/`.cpp` files are C++ sources
   that the `compass.cpp` scans read; the C++ side could be a per-file fact
   of a shared C++ scan.

6. **Recommendation.** **Keep; make it a scan** (map: the codes of one
   handler file; reduce: compare with the seeded codes) and take the seeded
   codes from the SQL call facts instead of a regex over the whole seed
   file. Effort: small. Saving: small, a few ms per handler edit (guess).

### curve_parameter_paths

1. **Intent.** Every `/CurveConfiguration/...` XPath in the ORE parameter
   artefact is one the ORE curve schema declares, or an exemption with an
   ORE source line explains it. A path that no document can hold describes
   a leaf that cannot be drawn or guarded.

2. **Does it make sense.** The invariant does. The implementation is a
   second, weaker XSD reader. `compass.crosscheck`'s `ops::ore_schema` already
   parses the ORE schemas with an XML parser and answers exactly this
   question (`schemas::declared(name, xpath)`), including inline anonymous
   complex types (read). It needs two extensions: types from `xs:include`d
   files, and `xs:extension` bases. This codec handles includes but neither
   of the other two. Also, the artefact is generated from `curve_terms.json`
   and the row modules. Checking the source (`curve_terms.json`, which the
   value set model already reads) would catch the defect upstream, in the
   file a person edits. That is a judgment call, though: the artefact also
   holds rows from other modules, so keep reading the artefact unless those
   rows carry no curve paths.

3. **Python accidents copied.**
   - A complex type ends at the regex `\n  </xs:complexType>`, so the
     reader depends on two-space indentation. A nested named type, or a
     reformatted schema, breaks it silently.
   - Inline `<xs:element name="X"><xs:complexType>` has no `type=`, so a
     path through it fails as undeclared. `xs:extension`, `xs:group ref`
     and `xs:element ref` are not followed (18 such constructs in
     `curveconfig.xsd`, measured). **Consequence:** some exemptions may
     exist only because the walker cannot follow these constructs. The
     first rule exempts "settings every segment kind shares on the base
     class", which is plausibly an `xs:extension` the regex does not follow
     (guess; check against the schema). With a real reader that exemption
     would turn `stale_exemption`. That is a correct verdict change.
   - The JSON exemptions file is read with a brace counter and a per-key
     regex. `ops::json_value` exists in crosscheck. Use it.
   - The children of a type are rebuilt by regex for every path segment of
     every path (328 distinct paths, measured). This is cheap, but it
     disappears with a parsed model.

4. **Cost.** Corpus function over three inputs, so it reruns only when the
   artefact, a schema or the exemptions change, which is rare. A few ms
   (guess). Not a hot spot.

5. **Shared data.** The ORE XSDs: `ops::ore_schema` in the value set model
   (`ore_value_sets`, `parameter_value_sets`), and the future
   `file-types-seed` and `xml-bindings` ports. The artefact is a populate
   file that the SQL scan lexes anyway.

6. **Recommendation.** **Replace the regex XSD reader with
   `ops::ore_schema`** (extended for includes and extension), and the JSON
   reader with `ops::json_value`. Longer term, make it a rule of the value
   set model checked against `curve_terms.json`. Effort: medium. Saving:
   negligible time; the value is correctness and one schema reader.

### ops::er_model_of (used by crosscheck's `er_diagram`)

Not a gate in this component, but it is the largest consumer of the same
create scripts. It parses `create table if not exists` by regex, reads
columns line by line, and re-derives the primary key, unique indexes and
foreign keys from comments and constraints. It is the fourth independent
"what tables does the schema have" parser, after `model_sql_types`,
`generated_column_refs` and `entity_binary_types`. Its output feeds a
generated diagram, so it must keep `plantuml_er_parse_sql.py`'s quirks for
byte parity. The others are checks and need not. **Recommendation:** a
per-file DDL fact (table, columns with type, nullability, PK, unique,
references) from the shared lexer feeds the crosscheck checks. `er_model_of`
can keep its own parse until ORE accepts a diagram regenerated from the
shared model. Effort: large across those codecs. Saving: each of them stops
rereading 11 MB on a create edit, tens of ms each (guess).

---

## compass.codegen

### codegen

1. **Intent.** The committed output of `ores.codegen regenerate --all`
   (26,212 files, 193 MB) is what the models and templates render to. This
   replaces the `drift-sweep` and `component-drift --all` gates (350 to 420 s
   in Python).

2. **Does it make sense.** Yes. It is the largest win in the program
   (read: 2.3 s CPU cold, and a warm edit regenerates only the models that
   read what changed). The staged design with recorded facts is sound.

3. **Python accidents copied.**
   - **The year of the run.** With `year` empty (the default, and the
     documented ORE setting), the licence year is the clock's year. Every
     January, all 26,212 outputs and all 202 `component_files.cmake` lists
     drift with no input change. For codegen the year is inside the
     `library` fact, but prepare is a unit whose inputs are files. If the
     record stands in for prepare, the fact is not recomputed, and a warm
     run keeps last year's bytes while a cold run writes this year's (guess
     from the overview's description of when prepare reruns; worth a test
     that sets the clock). **Correct:** the year is configuration (set
     `year` in ORE's settings) or a property of each file, never the clock.
     This is the one place where the cache state can change a verdict.
   - **Shared outputs rendered N times, last wins.** Every entity and
     junction of a component names its shell aggregator, so
     `trading_commands.hpp` is rendered by 78 models and the later one wins
     (read, from the overview). The aggregator is an aggregate over the
     component's models. It is a component-level output that ores.codegen
     happened to attach to every model. **Correct:** one part per component
     aggregate, rendered once. This also removes the runtime rule "a unit of
     a staged codec brings every unit of that codec", which exists because
     "which part a shared path belongs to is known only with all of them".
     No verdict change if all 78 renders are identical (guess; verify by
     rendering two of them).
   - **A later claim of a table wins** in `load_entity_registry` (path-order
     scan, read). Two models that claim one table is a modelling defect.
     **Correct:** report it. Possible verdict change: new findings if ORE
     has duplicates.

4. **Cost.** Prepare is about 140 ms, all models about 2.3 s CPU (1.6 s on
   eight cores), and a warm run is near zero (read, the overview's numbers).
   An edit regenerates only the dependent models.

5. **Shared data.** Every org model document, loaded by `compass.model`.
   `pending_models`, crosscheck's `model_drift`, `paste_blocks`,
   `model_sql_types`, `entity_*` and `protocol_twins` read the same models.
   The templates are shared with `component_files` and `template_sections`.

6. **Recommendation.** **Keep.** Fix the clock-dependent year (small, and
   the most important fix in this codec). Make component aggregates their
   own parts (medium). Report duplicate table claims (small). Saving: the
   aggregate fix removes 77 redundant renders per aggregate and the "every
   unit" read of about 3 MB of heads on a staged edit (read: 826 units,
   3 MB as heads); tens of ms per model edit (guess).

### component_files

1. **Intent.** Each component's `src/` and `tests/` `component_files.cmake`
   lists exactly the `.cpp` and `.hpp` files on disk, so a new or deleted
   source is in the build without hand-editing CMake.

2. **Does it make sense.** Yes, as long as ORE wants explicit source lists
   (`file(GLOB CONFIGURE_DEPENDS)` would make the gate unnecessary, but that
   is ORE's build decision). It does not duplicate anything.

3. **Python accidents copied.**
   - The year of the run, as above. Here it is worse: the year is not an
     input of this corpus unit. Its configuration hash covers `year = ""`,
     not the value it resolves to. A warm run keeps last year's output and a
     cold run writes this year's (read: the option is the only place the
     year enters, and option values hash as configured). **Correct:** a
     configured year, or record the resolved year as an input.
   - **Two components with one `#+full_name:`: the later path wins**
     silently. This should be a finding.
   - Components are found by regex over the raw text of each
     `component_overview.org`. Those files are corpus documents with parsed
     keywords. `find_keyword(d.tables, "full_name")` reads them without a
     second parse.
   - The `tests` unit lists headers from `include/`, like the `src` unit.
     This is a copy of the script and presumably ignored by the tests
     template (read; harmless).

4. **Cost.** Corpus function: one listing of the overviews, then about 400
   prefix listings (two per component) and two template renders per
   component. Measured at 1.2 to 1.3 s cold by the port task, which is
   slower than the 1.15 s Python original. That number is not explained by
   rendering 202 small files. `matching_files` over the checkout's file list
   per call is the likely cost (guess). Profile before changing it. Warm, it
   is reused unless a listing changes, but any new `.cpp`/`.hpp` anywhere
   under a component reruns all 202 lists.

5. **Shared data.** The library licence, modeline and templates, shared with
   `codegen`. The component overviews, shared with codegen's catalogue
   reading and the compass.docs inventories.

6. **Recommendation.** **Keep; simplify.** Read `full_name` from parsed
   keywords. Report duplicate names. Fix the year. Make it a staged codec
   with one part per component (or a part of `codegen`), so that a new file
   re-renders one list. Profile the 1.3 s cold. Effort: small (year,
   keywords, duplicates) to medium (parts). Saving: most of a cold second if
   the listing is the cost (guess). Warm: a new source file re-renders one
   component instead of 202.

### pending_models

1. **Intent.** Every gated model (`:gated:` or `:pending_change:` in its
   Flags) has a derived `<singular>_change.org` model beside it, in sync with
   its entity. The derived model then feeds codegen like any other model.

2. **Does it make sense.** Yes. It is a model-to-model transform, and the
   derived file is committed because codegen and the docs read it. It is
   written, however, as string surgery on the raw org text with regexes and
   `split_before`. The models are corpus documents with parsed headings,
   properties and keywords. `codegen` and `compass.model` already load them.
   The derivation also has a hidden ordering relation with `codegen`:
   codegen reads the committed `_change.org`. When the derived model is
   stale, codegen renders from the stale file, and both codecs report drift
   in one run.

3. **Python accidents copied.**
   - **A refused model stops the codec silently.** `derive_all` catches
     `refused` and returns with a partial file table and no message (read).
     In Python, the script stopped with a traceback, so at least the user saw
     why. In compass, the reason is lost. The outputs after the refused model
     are missing, and that shows only as `drift`, with no explanation.
     **Correct:** throw (as `codegen` does when a model does not load, so the
     run reports the codec's error), or split out a check codec that reports
     the refusal as a finding. This is a verdict change only when a model is
     refused, and it is the most important fix here.
   - `pending_tables(v, dir)` re-lists, re-reads and re-splits every
     `ores.*.org` of the directory for each gated model that has foreign
     keys: O(gated x models in directory). Compute it once per directory.
   - `is_gated` splits the model, `derive_all` splits it again for the front
     matter, and `derive` splits it a third time. `keyword()` compiles a new
     regex on every call.
   - Org parsing by regex: `^:(gated|pending_change):\s*true\s*$`
     anywhere in the first Flags section, keywords by `^#\+name:`, and
     properties by `^:(\w+):[ \t]+(.*)$`. The parsed tables give the same
     answers without the edge cases (a property line inside a source block
     matches the regex).
   - `derived_on = "2026-10-10"` is a constant, not the clock, which is the
     right choice.

4. **Cost.** Corpus function over `projects/*/modeling/ores.*.org` read as
   files. An edit to any model reruns it (the listing and every model file
   are inputs), re-reading about 825 models (read: the codegen overview's
   model count). Tens of ms per model edit (guess).

5. **Shared data.** The same model documents `codegen` loads (as parsed
   tables). Its output is an input of `codegen`.

6. **Recommendation.** **Keep; simplify.** Report refusals instead of
   swallowing them. Compute the pending tables once. Read gated flags and
   keywords from the parsed tables, and make it a document codec over model
   documents, so an edit re-derives one model (with `pending_tables` as a
   recorded per-directory fact or a lookup). Effort: small (refusal,
   caching) to medium (document codec). Saving: tens of ms per model edit
   (guess). Longer term, derive the pending model in codegen's prepare step
   so the generated code never lags the derived model.

---

## compass.codec

### tangle and tangle_check

1. **Intent.** The committed files that org-babel source blocks tangle to
   (the 171 codegen templates, and through the recipe library codecs, the
   shell and HTTP scripts) are what Emacs would tangle. `tangle_check`
   reports blocks whose header arguments the port cannot reproduce, so the
   files never silently differ from what Emacs would write.

2. **Does it make sense.** Yes. It replaced 11 to 41 s of Emacs per gate
   with an in-process tangle. `tangle_check` exists only because "a codec
   emits files or diagnostics, not both". This is the same split the
   value-set evidence names, and it has the same cost: both codecs call
   `ops::tangle(d)` on the same document.

3. **Python accidents copied.** None; this is a port of Emacs, not Python,
   and the rules are Org 9.7's. Two structural issues:
   - **The scope is an option, not an input.** Both codecs read
     `every_document()` and return early when the path is outside
     `directory`. That makes about 11,000 document units per codec (measured
     document count) that do nothing, each recorded in the dependency record
     and checked on every run, to tangle 226 templates. **Correct:**
     restrict the codec's inputs by path (a descriptor-level path prefix or
     glob), so that out-of-scope documents are not units.
   - **The recipe library codecs drop the problems.**
     `compass.docs/.../recipe_libraries.cpp` calls `ops::tangle` and uses
     only `targets`, so an unsupported header argument in a shell or HTTP
     recipe is never reported. `tangle_check` is limited to one `directory`,
     so it cannot cover them. **Correct:** a problem from any tangle is a
     finding.

4. **Cost.** Two document codecs: two tangles per in-scope document and two
   no-op units per out-of-scope document. The tangle is cheap (it reads the
   parsed block table). The cost is in unit bookkeeping: about 22,000
   no-op unit rows (guess at their weight in the 268 MB record and the
   per-run selection; worth measuring).

5. **Shared data.** The block table of each org document. Also read by the
   recipe library codecs (`shell_recipe_library`, `http_recipe_library`),
   and by `template_sections` in crosscheck for the templates.

6. **Recommendation.** **Merge** `tangle_check` into one codec with
   `tangle` once the runtime lets one unit emit files and findings (the
   "Combine gates" task's shared step). Until then, scope both by path at
   the descriptor. Route the recipe codecs' problems into the check. Effort:
   medium (runtime path scope), small (recipe problems). Saving: about
   22,000 no-op units out of every run's record and selection (guess at the
   time; likely tens of ms warm), and one of two tangles.

### document_index

1. **Intent.** A machine-readable list of every document's ID, title, type
   and path, for tools and agents.
2. **Does it make sense.** Yes. It is a cheap generated artefact. It is
   compass's own, not a Python port.
3. **Python accidents copied.** None. Tabs in a field become spaces, which
   is fine. The order is byte order of the paths, which is deterministic.
4. **Cost.** Corpus function that enumerates every document, so any
   document edit reruns it and rewrites 11,000 rows. A few ms (guess).
   Not worth a scan.
5. **Shared data.** The `:ID:`, `#+title:` and `#+type:` of every
   document, which the corpus index and every inventory codec in
   compass.docs also read. They come from the parsed tables, so there is no
   extra tokenisation.
6. **Recommendation.** **Keep.** Effort: none.

### required_sections

1. **Intent.** Every typed document has the sections its type's contract
   requires (memory `8B29A8D8`).
2. **Does it make sense.** Yes. It reads the parsed heading table, which is
   the right data.
3. **Accidents.** A required section is satisfied by a heading of that
   title at any level. A `** Status` inside another section satisfies
   `* Status`. **Correct:** compare at level 1 (the heading table has
   `levels`). Possible verdict change: new findings where a required
   section exists only as a sub-heading (unknown count; cheap to measure).
   The fix text already says `* {section}`.
4. **Cost.** A document codec over typed documents; an edit reruns one
   document. Minimal.
5. **Shared data.** The heading table of each document. Other
   structure checks in compass.docs walk the same headings, but they cost
   nothing extra, because the parse is shared.
6. **Recommendation.** **Keep; compare at level 1.** Effort: small. Saving:
   none (correctness).

### catalogue (all three components)

These are plain lists of descriptors. There is nothing to review. If
`rls_widenings` merges into the SQL scan, it leaves the list.

---

## Shared passes

Grouped by the data each gate reads and how it tokenises that data.

### 1. SQL text under `projects/ores.sql` (85 MB, 3,475 files)

| Gate | Part of the tree | Tokenisation today |
|------+------------------+--------------------|
| `sql_hygiene` | all | four lexers (nested block comments, quotes, dollar bodies) |
| `sql_reachability` | all | line regex on raw text |
| `populate_refs` | `create/`, `populate/` | `blank_comments` lexer, balanced parentheses, regex |
| `rls_widenings` | `create/` | regex comment blanking, lazy statement regex |
| `handler_permissions` (seed side) | one populate script | regex for quoted codes |
| `curve_parameter_paths` (artefact side) | one populate script | regex for quoted paths |
| crosscheck: `er_diagram` (`ops::er_model_of`), `model_sql_types`, `generated_column_refs`, `entity_binary_types`, `marketdata_identity_columns`, `entity_writes` | `create/` | each its own `create table` / DML regex |

**Proposed shared pass:** one lexer over each `.sql` file, producing
token spans. It handles `--`, nested `/* */`, `'...'` with `''`, `E'...'`
with backslash escapes, `"..."`, and dollar bodies with tags; it recurses
into bodies for function analysis. From the tokens, one per-file fact,
keyed by content ID like today's scans:

- top-level statements, split on code `;`;
- psql meta-commands at line start on code tokens (`\i`, `\ir`, `\include`,
  `\include_relative`);
- `create`/`drop` heads with kind, name, table and the idempotency
  modifiers (serves `not_re_runnable` and `duplicate_index`);
- `create policy` name, table and expression tokens (serves
  `rls_widenings`);
- functions: name, parameters, guards and inserts from the body tokens
  (serves `populate_refs`);
- `*_fn` calls with literal arguments and offsets (serves `populate_refs`
  and the permission seeds);
- `create table` columns, types, keys and references (serves the crosscheck
  DDL checks; `er_model_of` later).

Per-file findings: all of `sql_hygiene` plus `rls_widenings`. Reduce:
reachability plus execution order, then the `populate_refs` join in that
order, then the seeded permission codes.

Runtime need: a codec emits files or findings, and a scan's facts belong to
one codec. Sharing the fact across codecs (`er_diagram` emits files, the
crosscheck checks are separate codecs) needs the shared-prepare or
shared-scan support the "Combine gates that read the same data" task owns.
**Without that support**, the diagnostics-only SQL gates (`sql_hygiene`,
`sql_reachability`, `populate_refs`, `rls_widenings`) can already be one
scan codec, at the price of rule names changing their codec prefix.

Expected effect: a SQL edit maps one file and runs in-memory reduces,
instead of rereading 67 MB for `populate_refs` and 11 MB for each
create-tree corpus codec. That is about 400 ms from `populate_refs`
alone, plus tens of ms per crosscheck codec moved onto the facts (guess).
It is the largest share of the 0.7 to 1.2 s edit-hook cost. Cold: one lex
of 85 MB instead of five or more.

### 2. ORE XSD schemas (`external/ore/xsd`)

| Gate | Tokenisation |
|------+--------------|
| `curve_parameter_paths` | regex over XML, indentation-dependent |
| crosscheck `ore_value_sets`, `parameter_value_sets` | `ops::ore_schema`, XML parser |
| future `file-types-seed`, `xml-bindings-drift` | (to be ported) |

**Shared pass:** `ops::ore_schema`, extended with `xs:include` and
`xs:extension`, resolved once and shared (the same runtime need as the value
set model, which already resolves twice per cold run).

### 3. Org model documents (`projects/*/modeling/ores.*.org`, about 825)

| Gate | Tokenisation |
|------+--------------|
| `codegen` | parsed tables through `compass.model` |
| `pending_models` | raw text, regex and line splitting |
| crosscheck `model_drift`, `paste_blocks`, `model_sql_types`, `entity_*` | parsed tables or regex (outside scope) |

**Shared pass:** the parse already exists. `pending_models` should read the
parsed tables, not the raw bytes. Longer term, the pending derivation
belongs inside codegen's prepare step.

### 4. Codegen library (templates, licence, modeline, component overviews)

| Gate | Reads |
|------+-------|
| `codegen` | all templates, licence, modeline, catalogue |
| `component_files` | two templates, licence, modeline, component overviews (raw regex) |
| `tangle` (directory = templates) | template org documents' block tables |
| crosscheck `template_sections` | the templates |

**Shared pass:** `component_files` could be a part of `codegen` and reuse
its compiled templates and library values. It should read `full_name` from
parsed keywords. The saving is small; the gain is one home for the year
and the licence.

### 5. Org documents generally (about 11,000)

| Gate | Reads |
|------+-------|
| `document_index` | ID, title, type of every document |
| `required_sections` | heading table |
| `tangle`, `tangle_check`, recipe library codecs | block table |

The parse is shared already. The waste is per-document units for codecs
scoped to one directory (`tangle`, `tangle_check`, and the recipe codecs,
which also filter by path at run time). **Fix:** path-scoped inputs in the
descriptor.

### 6. C++ handler sources

`handler_permissions` reads `messaging/*_handler.{hpp,cpp}` by regex. The
`compass.cpp` scans read the same C++ files. A shared C++ per-file fact
(string-literal arguments of named calls) could serve it. The saving is
small; do this only if a shared C++ scan is built anyway.

---

## Summary

| Gate | Recommendation | Effort | Expected saving |
|------+----------------+--------+-----------------|
| `sql_hygiene` | keep; one lexer for four rules; host the shared SQL facts | medium | about 2/3 of its cold time (guess); enables the rest |
| `sql_reachability` | keep; in-order walk; publish execution order; includes from code tokens | small | none alone; enables the order rule |
| `populate_refs` | replace by a scan on per-file facts; add definition-before-use; fix quadratic line counting | medium | about 400 ms per SQL edit (guess) |
| `rls_widenings` | merge into the SQL scan (per-file rule) | small | tens of ms per create edit (guess) |
| `handler_permissions` | keep; make a scan; seeds from SQL call facts | small | a few ms (guess) |
| `curve_parameter_paths` | replace the regex XSD and JSON readers with `ops::ore_schema` and `ops::json_value` | medium | correctness; stale exemptions surface |
| `codegen` | keep; fix the clock year; component aggregates as parts; report duplicate table claims | small to medium | tens of ms per model edit; removes a cache-dependent verdict |
| `component_files` | keep; fix the year; parsed keywords; duplicate names as findings; per-component parts; profile 1.3 s cold | small to medium | up to about 1 s cold if listing is the cost (guess) |
| `pending_models` | keep; stop swallowing refusals; cache pending tables; read parsed tables | small to medium | tens of ms per model edit (guess) |
| `tangle` + `tangle_check` | merge once the runtime allows it; scope by path at the descriptor; report recipe tangle problems | small to medium | about 22,000 no-op units per run (guess at time) |
| `document_index` | keep | none | none |
| `required_sections` | keep; compare at level 1 | small | none (correctness) |

No gate in this scope should be dropped. Every one protects an invariant that
neither the database nor the compiler checks. `rls_widenings` should
disappear as a separate codec by merging into the SQL scan, not by losing its
rule.
