# Review: the compass.crosscheck codecs

Scope: the 25 codecs in `projects/compass.crosscheck/src/codecs/` (24 gates plus the catalogue, with `registry_gates.cpp` holding three gates), and the ops they use. `value_sets.cpp` is out of scope; this review only notes what data it shares.

Method: I read each codec, the ops it calls, the ORE script it ports (in `~/Development/OreStudio/ores_dev_prime_origin`), and the overviews of compass.crosscheck, compass.codec, compass.model, compass.codegen and compass.sql. Claims about ORE's tree were measured with grep or find on that checkout and are marked *measured*. Claims about cost that I did not time are marked *guess*. I ran nothing in the compass checkout.

## Findings that cut across every gate

1. **No crosscheck codec reads a document.** Every descriptor declares `.reads = ct::inputs::of_types({})` and reads org files as raw bytes through `read_file`, then splits them into lines and matches them with regular expressions. The host has already parsed every org file into compass.org tables (headings, drawers, keywords, source blocks and tables). compass.model loads the 844 codegen models from those tables, and compass.codegen loads them again in its prepare step. The crosscheck gates throw that work away. Ten gates re-derive facts the loaded model already holds: `money_decimal_columns`, `model_sql_types`, `model_drift`, `http_areas`, `route_units`, `protocol_twins` (opt-out), `subject_conformance` (declared subjects), `paste_blocks` (`:implements` blocks), and the `:tablename:` lookups in `ops::curve_columns` and `ops::convention_table`.
2. **Silent passes where Python fails.** A files codec whose inputs are malformed returns without writing (`return;` or `catch (const stop&) {}`). The runtime only diffs the files a codec emits, so the gate passes. The Python script would have failed. The overview acknowledges this: "its drift findings are silent where the gate would fail". This affects `curve_sections_wiring`, `curve_column_guards`, `convention_kind_terms`, `ore_renderings_tables`, `script_library_seed`, `ore_output_catalogue`, `er_diagram`, `conventions_dataset` and `route_inventory`. The analysis codecs `conventions_coverage` and `marketdata_identity_columns` stop silently on a missing input. Only the value set pair reports why (`parameter_value_sets/unresolved`). This is a correctness regression, not an efficiency issue. Each generator needs a diagnostics twin, or a shared `unresolved` rule, so that a stop is a finding.
3. **Four `create table` parsers over the same SQL.** These four codecs each parse ORE's create scripts with their own regular expressions, and each gets slightly different answers:
   - `model_sql_types`: the first `create table` per `*_create.sql`, line by line, with `\s` spacing.
   - `entity_binary_types`: every `create table` and `create view` in `create/*/*_create.sql`, with ` +` spacing only.
   - `generated_column_refs`: every `create table` and `create index` in generated `create/**/*.sql` files.
   - `er_diagram`, through `compass.sql::er_model_of`: the parser of `plantuml_er_parse_sql.py`.

   `marketdata_identity_columns` parses one more table by hand. One SQL table model (table, columns, types, constraints, indexes, views) in compass.sql, built as a scan per file, would serve all of them.
4. **Floors exist only because parsing is a guess.** `money_decimal_columns/too_few_columns` (1000), `model_sql_types/too_few_compared` (500) and `entity_binary_types/too_few_compared` (500) are there to notice when the regular expression stops matching the format. A gate that reads the loaded model cannot silently read nothing, so these rules go away.
5. **Hand-ported Python functions.** These are policy written in C++ that the Python source still owns, so an edit in Python is not seen:
   - the two `COMPOSITE_KINDS` decoders in `conventions_dataset`;
   - the curve `OWN_SETS` in value_sets.

   This is the same issue the generator-gates investigation recorded.

---

## paste_blocks

1. **Intent.** Every `ores::a::b::name` that a model's `:implements` C++ block pastes still names a symbol, so a rename cannot leave a stale paste behind. The trigger was the ORE `ores.service` clean-up, where the model and the hand-edited generated handler disagreed.
2. **Does it make sense.** Mostly no, once compass runs codegen drift. The docstring names two cases.
   - (a) The model and the committed generated file disagree. Codegen drift reports that.
   - (b) Both agree and both are stale. In that case the committed generated file names a symbol that does not exist, so the next build of that component fails. The compiler is the authority here, and it runs on every CI build.

   The script existed because ORE's drift gate regenerated a component only when someone ran it. Compass's `codegen` codec is a staged codec that regenerates only what changed, so drift is cheap on every check. The resolver is also weak by design. It accepts a name when *any* identifier, including a use, appears inside a brace-scope of that path ("errs towards resolving"), so it is not a symbol table.
3. **Python accidents copied.**
   - Regex block extraction stands in for the org source-block table: `^#\+begin_src\s+cpp…:implements`. compass.org already has every block with its header arguments.
   - Brace counting treats every `{` as a scope, including initialisers and lambdas. Any identifier in a scope counts as declared there. Correct behaviour would be declarations only, or simply the compiler.
   - `ores(?:[_a-z0-9]*)?(::…)+` admits `ores_foo::x` as a root.
   - Changing to declarations only would raise new findings on ORE (likely false resolutions today). *Guess.*
4. **Cost.** This is the most expensive codec: about 2.5 s cold. It blanks comments and literals in, and tokenises, every `.hpp` under an `include/` directory. On ORE that is 11,815 headers and 84 MB (*measured*). Every identifier per scope goes into the scan fact, so the scan table in the record is very large. The reduce step rebuilds a symbol table from all those facts on every reduce, though it filters to the wanted scopes. Warm is cheap through the scan, but any header edit costs a map plus a reduce over the full fact set.
5. **Shared data.**
   - Headers, blanked by `co::blank_comments_and_literals` and walked by braces. compass.cpp's `enum_attributes`, `unique_type_names`, `exported_types`, `forward_declarable_includes` and `test_case_reachability` tokenise the same headers.
   - Models: every org file under `projects/*/modeling/**`, as text.
6. **Recommendation.** Drop it once ORE's `compass check` enables `codegen` (drift on every check). Until then, simplify:
   - read blocks from the compass.org block table;
   - make the header fact the declared names only (namespace, class, struct, enum, using, function names at namespace scope) from a shared compass.cpp header scan.

   Effort: drop is small; simplify is medium. Saving: about 2.5 s cold and the largest scan table in the record.

## subject_conformance

1. **Intent.** Every NATS subject a person writes is inside the entity protocol grammar (`component.v1.resource.verb`, `….ops.op`, `….events.collection.action`). Derived subjects are already refused by the generator.
2. **Does it make sense.** The model half does. The C++ half does nothing.
   - The ported pattern `\w*subject\w*\s*=\s*\n?\s*\"([^\"]+)\'` opens on a double quote and closes on a single quote. That is a typo in the Python. It matches nothing in ORE: grep for it returns zero hits (*measured*). So the codec reads every `.hpp` and `.cpp` under `projects/` (5,066 contain "subject"; *measured*) to find nothing.
   - The overview admits "it reads almost nothing".
   - The model half is `:subject:` lines in `projects/*/modeling/*.org`. compass.model's `messages.cpp` already loads `subject` from the model.
3. **Python accidents copied.**
   - The quote typo. Fixing it naively to `"…"` would hit 4,109 distinct strings in 987 files, such as `subject_area` permission codes, test subjects and UUIDs (*measured*), so a fix needs a real shape. The candidates are `nats_subject = "…"` and `*_subject` constants in hand-written, non-generated files. The protocol headers are mostly generated, so what remains is hand-written constants.
   - A line regex on raw org text instead of the drawer table. It also matches `:subject:` outside drawers.
   - Any of these fixes changes verdicts on ORE, because the C++ half starts producing findings.
4. **Cost.** About 1.5 s cold. A scan over about 21,000 C++ files plus the models.
5. **Shared data.** Models (as with all model gates). C++ subject constants: `open_reads` parses `nats_subject = "…"` in protocol headers. `bff_list_requests` (compass.cpp) reads `_protocol.hpp`.
6. **Recommendation.** Replace.
   - Move `classify` into compass.model's loader (or a model-rule codec), where a declared `:subject:` is loaded and the derived ones are already checked.
   - Replace the C++ half with a narrow scan of hand-written files for `nats_subject`/`_subject` constants, sharing the protocol-header pass with `open_reads`. Or drop the C++ half outright, since it has never fired.

   Effort: small. Saving: about 1.5 s cold. The scan table shrinks from about 21,000 entries to a few hundred.

## entity_writes

1. **Intent.** Only the generated repositories write `ores_*_tbl` entity tables, so claims and audit stamping are not bypassed. SQL functions other than the purge, and hand-written C++, must go through a repository.
2. **Does it make sense.** Yes. This is an architecture rule nothing else checks. But it fires 136 times on ORE (the old baseline, now 136 findings to excuse inline). A rule with 136 standing exceptions says either the rule or the code base needs a decision. The likely refinement is to let the codegen model name a table's sanctioned writers, instead of 136 comments.
3. **Python accidents copied.**
   - The SQL function body is taken from the first `$$` after `create function` to the next `$$`. A `$body$` tag, or a `$$` inside a comment, breaks it. compass.sql's scanner already masks dollar bodies and comments correctly (`ops::mask_non_code`).
   - DML in comments counts as a write.
   - Findings are keyed by bare function name (`insert_or_assign`), so two functions with one name in two files collapse to the last one. That is dict order.
   - The repository exemption is a name regex (`ores_[a-z0-9]+_<stem>s?_tbl`), with plural by a trailing `s`.
   - Fixing the body extraction could add or drop a few SQL findings. *Guess.*
4. **Cost.** About 1.4 s cold. It decodes, normalises newlines and runs a case-insensitive regex over every hand-written `.cpp` and `.hpp`: about 21,000 files, of which only 794 contain `_tbl` (*measured*). A byte prefilter for `_tbl` (case-insensitive memmem) before decoding would skip about 96% of files.
5. **Shared data.**
   - SQL create scripts: compass.sql `sql_hygiene` (same files, already tokenised with dollar quoting), `populate_refs` (function bodies), `rls_widenings`.
   - C++: every hand-written source, as with the compass.cpp whole-tree scans (`portable_stop`, `stream_coverage`, `unity_collisions`).
6. **Recommendation.** Keep and simplify.
   - Add the `_tbl` prefilter.
   - Take SQL function bodies from compass.sql's masked scan, which could become one shared SQL statement scan.
   - Move the sanctioned-writer list into the models rather than 136 inline exceptions.

   Effort: small for the prefilter; medium for the SQL share. Saving: about 1 s or more cold. *Guess*, from the 96% skip rate.

## money_decimal_columns

1. **Intent.** A money column is exact (numeric in SQL, decimal in C++). A continuous quantity is not typed exact. Stated in `exact_numbers_and_economic_change.org`.
2. **Does it make sense.** Yes. It is a domain rule the generator does not enforce. But it belongs on the loaded model: the entity's `columns`, each with `type` and `cpp_type`, and the field groups' fields.
3. **Python accidents copied.**
   - The column reader is a line scanner: any `** <one-word>` heading in any org file under any `modeling/` directory is a column. That includes component overviews and non-entity documents. A `:column:` key outside a heading starts another column.
   - The first `:type:` or `:cpp_type:` after the heading wins, even from a nested drawer of another sub-heading.
   - The `min_columns` floor exists to catch the scanner going blind.
   - The name classification (head-noun tokens) is the policy itself, and is fine to keep.
   - Moving to the loaded model would drop false columns from non-entity documents and might change a few verdicts. *Guess.*
4. **Cost.** About 0.8 s in Python. Here, a corpus function that reads every model file on any model edit, with five regexes per line.
5. **Shared data.** Model org text, read as with `model_sql_types`, `model_drift`, `http_areas`, `route_units`, `protocol_twins` and `subject_conformance`. Column types: `model_sql_types` and `entity_binary_types`.
6. **Recommendation.** Merge into a single per-model rules codec (see Shared passes, P1) over the loaded entity and field group models. Drop `too_few_columns`. Effort: medium (needs the model codec). Saving: the model text pass; incremental per model.

## model_sql_types

1. **Intent.** A column's model `:type:` is the type its table is created with. The trigger was precision changes that a regeneration had not yet reached.
2. **Does it make sense.** Not as a separate gate. 1,307 of 1,472 create scripts are generated (*measured*), so a mismatch between a model and its generated create script is codegen drift by definition. Compass's `codegen` codec regenerates the create script from the model and diffs it against the committed file. Only the roughly 165 hand-written create scripts that a model also claims carry independent information.
3. **Python accidents copied.**
   - It compares against the *first* `create table` of the file only.
   - The constraint split is by a keyword regex, so `default` inside a type expression truncates it.
   - Model columns come from a `* Columns` line scan with first-`:type:`-wins.
   - The table is found by a raw `:tablename:` regex.
4. **Cost.** About 0.3 s in Python. It re-reads every create script and every model on any change.
5. **Shared data.** Create SQL (four parsers; cross-cutting finding 3). Models (as above).
6. **Recommendation.** Drop for generated scripts (drift covers them). For the hand-written remainder, fold one rule into the model rules codec, using the shared SQL table model. Effort: small. Saving: one SQL pass and one model pass.

## entity_binary_types

1. **Intent.** A generated row binds each column in the binary wire format of the column's SQL type. Otherwise the server rejects the bind at runtime with an error that names nothing.
2. **Does it make sense.** Yes, the invariant is real and the compiler cannot see it. But both sides (the generated `repository/*_entity.hpp` and the generated `create.sql`) are products of one model and the templates. The fact is decided at generation time. The classification gate document already flagged it as "reads generated output for facts the models state". The right home is a check over codegen's own context for the entity row: the row member types the template emits and the SQL types from the same model. That catches it before files exist, and needs no regex over generated C++.
3. **Python accidents copied.**
   - The member regex is anchored on four-space indentation and a trailing `;`, so it depends on clang-format layout.
   - `query_builder::bind(…)` is matched on collapsed text.
   - The create-table regex uses ` +` only, so a tab breaks it.
   - The wire tables are hand lists, so an unlisted type is `unclassified`.
   - The `min_compared` floor.
4. **Cost.** About 0.4 s in Python. It reads every generated entity header and every create script.
5. **Shared data.**
   - Generated outputs. `generated_column_refs`, `open_reads` and `protocol_twins` read them too, and the `codegen` codec produces them in memory.
   - Create SQL (four parsers).
6. **Recommendation.** Replace with a rule over the codegen entity context: a type-pair table (cpp type → wire, sql type → wire) evaluated per entity during or beside generation. Short term, merge its SQL reading into the shared SQL table model. Effort: medium. Saving: removes one read of the generated tree and one SQL pass.

## generated_column_refs

1. **Intent.** Two free-text model sections that reach generated code, `** Table display` (C++) and `** Indexes` (SQL), name only real columns. So a removed column cannot survive there and break the build or the database create after merge.
2. **Does it make sense.** The rule does. Reading generated output for it does not. Both sections and `* Columns` are in the loaded entity model. The flag-derived columns (version, tenant_id, modified_by …) are known to the loader and context builder. A model rule (`display names ⊆ members`, `index columns ⊆ columns ∪ flag columns`) catches it at model load, before generation. compass.model's `entity_checks.cpp` already does exactly this kind of check ("column belongs to no domain group").
3. **Python accidents copied.**
   - "Member" means *any word* in the entity header without comments, so a type name or a word in a string resolves.
   - The table writer must have a `const auto& x :` loop or it reports.
   - The comment regex strips `//` inside string literals.
   - The index regex stops at the first `)`, so functional indexes (`lower(x)`) are cut.
4. **Cost.** About 0.3 s in Python. It globs `*_table.cpp` and headers, plus all create SQL.
5. **Shared data.** Generated outputs (with `entity_binary_types`, `open_reads` and `protocol_twins`). Create SQL (four parsers). Model.
6. **Recommendation.** Replace with two rules in compass.model's entity checks, or in the model rules codec. Effort: small to medium. Saving: one generated-tree read and one SQL pass.

## open_reads

1. **Intent.** A generated read handler that checks no permission serves only a subject the Authorised reads page allow-lists. History registrars name a literal read code, and the allow-list has no stale rows.
2. **Does it make sense.** Yes, it is a security invariant. Part of it is a model fact: whether an entity's or operation's read has a permission code, and which subject it serves. But hand-injected methods (pasted blocks) land in the generated handler, so reading the handler is defensible. Reading the allow-list page by regex is not: it is an org document with a table, already parsed.
3. **Python accidents copied.**
   - The method boundary is `^    void name(ores::nats::message msg) {$` (four spaces, exact signature).
   - "Checks a permission" is `body.contains("has_permission(")`, so a comment or a dead branch counts.
   - The request-to-subject mapping is "the first `nats_subject` after a `struct X_request {` line".
   - Doc-comment `@brief Serves x.` overrides the decoded subject.
   - The `list_`/`get_` name fallback.
4. **Cost.** About 0.4 s in Python. It reads all protocol headers, generated handlers and registrars.
5. **Shared data.** Protocol headers: `subject_conformance` (C++ half), `protocol_twins` (existence), compass.cpp `bff_list_requests`. Generated handlers: codegen output. Org page: a compass.org table.
6. **Recommendation.** Keep and simplify. Read the allow-list from the document's table through the view. Share one protocol-header pass (request struct → subject) with `bff_list_requests` and the C++ half of `subject_conformance`. Longer term, evaluate it on codegen's handler context plus the pasted blocks. Effort: small (table) to medium (shared pass). Saving: small.

## http_areas

1. **Intent.** Every `#+http_area:` a model declares is a published area of `route_classification.AREAS` minus `NO_AREA`.
2. **Does it make sense.** Yes, but it is tiny and it is a model rule. `org_loader` reads `http_area`.
3. **Python accidents copied.** A raw `^#\+http_area:` regex over the text instead of the keyword table.
4. **Cost.** Negligible (0.1 s in Python). But it re-reads every model on any model edit.
5. **Shared data.** Models. `route_classification.py` through the literal reader, which `route_inventory` also reads.
6. **Recommendation.** Merge into the model rules codec, with AREAS read once in a routes model shared with `route_inventory`. Effort: small. Saving: one model pass.

## route_units

1. **Intent.** Each `ores.http` component's route aggregate registers every unit its models opt into, each once, and every registered unit has its source.
2. **Does it make sense.** Yes. The aggregate `*_routes.cpp` is hand-written (no generated marker in any of seven; *measured*), so drift does not cover it.
3. **Python accidents copied.**
   - The opt-in is a raw regex on `:ores.cpp.http-route.enabled: true`, at the start of a line only. compass.codegen resolves `:ores.<address>.enabled:` overrides properly.
   - The singular comes from `#+entity_singular|name_singular` regexes.
   - The components come from `list_files("projects/ores.http/")`, which lists every file in the adapter just to collect directory names. Adding any file there changes the listing input.
4. **Cost.** Small.
5. **Shared data.** Models (the enablement facet, which the codegen graph already resolves). `ores.http` sources: `route_inventory` and compass.cpp `route_auth` read the same `.cpp` files.
6. **Recommendation.** Merge into one routes codec with `route_inventory` and `route_auth`, reading the opt-in from the codegen model units. Effort: medium. Saving: one `ores.http` pass.

## registry_exceptions

1. **Intent.** Every `ACCEPTED_EXCEPTIONS` entry in `component_registry.py` names a listed component, an item the clean standard defines, a reason, an acceptor and an ISO date.
2. **Does it make sense.** Yes, while the registry stays a Python module. Its policy (justified, dated, accepted exceptions) duplicates compass's own inline exceptions (`exception/incomplete`, `exception/expired`). The long-term move is to retire the registry's exception list in favour of compass exceptions on the component overview.
3. **Python accidents copied.** The checklist items are a regex `^\|\s*([A-Z]\d{2})\s*\|` over the org standard, instead of its table. There is no expiry, unlike compass exceptions.
4. **Cost.** Negligible.
5. **Shared data.** `component_registry.py` `COMPONENTS_UNDER_TEST` is also read by `protocol_twins`.
6. **Recommendation.** Keep. Read the standard's table through compass.org. Plan to replace the registry's exceptions with compass inline exceptions. Effort: small. Saving: none worth measuring.

## model_drift

1. **Intent.** A model that binds a profile does not contradict a feature the profile fixes, sets the features the profile requires, and does not state one feature two ways.
2. **Does it make sense.** Partly duplicated. compass.model's loader already reads the profiles and already throws `unknown profile` and `conflicting profiles` (`entity.cpp`, `apply_profile`). So `unknown_profile` and `profile_conflict` repeat the loader, and codegen fails those models anyway. The remaining three rules belong in `apply_profile`, where the model's own value and the profile's are both in hand. There the profile is parsed once, by the same reader that decides what codegen does.
3. **Python accidents copied.**
   - The profiles are re-parsed with a table-row regex that allows an id link, skipping rows whose value is all `-`/`+`.
   - Any drawer property anywhere in the model, including in unrelated sub-headings, counts as a feature statement. That is why `self_contradicting` exists.
   - The heading stack is computed from `*` counts on stripped lines, so an indented `*` inside a src block opens a heading.
4. **Cost.** About 0.15 s in Python. Every model is re-read.
5. **Shared data.** Models and profiles: compass.model reads both.
6. **Recommendation.** Replace by loader rules (or the model rules codec over loaded models). Delete the two duplicated rules. Effort: small. Saving: one model pass, and one profile parser.

## template_sections

1. **Intent.** A template line that chains several section tags of a section spanning lines would leak a blank line, because pystache strips only standalone single tags.
2. **Does it make sense.** Yes, it is a cheap lint. But compass.codegen compiles every template into a node table with source ranges (`compiled_template`), and its renderer already decides which lines are standalone. The rule is best computed from that table, or reported by the compiler.
3. **Python accidents copied.**
   - Tags by a regex `\{\{([#^/])\s*([^}]+?)\s*\}\}`, which ignores delimiter changes and comments.
   - Matching a close to the nearest open of the same name, with no nesting check.
4. **Cost.** Negligible (0.1 s in Python).
5. **Shared data.** `.mustache` templates: the codegen prepare step reads and compiles them.
6. **Recommendation.** Simplify to a rule over `compiled_template`. Effort: small. Saving: negligible time, one fewer tag parser.

## protocol_twins

1. **Intent.** For each listed component with committed TypeScript protocol output, every C++ protocol header has its `.ts` twin, and no model switches `ores.ts.protocol` off.
2. **Does it make sense.** Yes, during the roll-out. But it re-builds the physical-space graph (`cg::load_physical_space`) from the template nodes that the codegen prepare step already loads. And it decides by file existence what codegen's `model_units` states directly: whether a model emits a C++ protocol unit and a TS protocol unit. Hand-written protocol headers have no model, which is the one thing the file walk adds.
3. **Python accidents copied.**
   - The opt-out is a substring search for `:ores.ts.protocol.enabled:` anywhere in a model's bytes, including prose and code blocks.
   - The `-cpp` suffix stripping on component names.
   - "Committed output" means a non-empty listing of the twin directory.
4. **Cost.** About 0.15 s in Python. Here it lists the template nodes, re-parses the graph, reads the catalogue, and reads every model's bytes for the opt-out.
5. **Shared data.** The codegen graph and the catalogue (compass.codegen prepare). Models. `COMPONENTS_UNDER_TEST` (with `registry_exceptions`). Protocol headers (with `open_reads`).
6. **Recommendation.** Simplify. Take the graph and the units from codegen's prepare state (needs the shared prepare of F51A1EEB). Read the opt-out from the model drawers. Keep the file walk only for hand-written headers. Effort: medium. Saving: one graph load and one model pass.

## marketdata_identity_columns

1. **Intent.** The series identity table, the projector's field switch and index mapping, and the per-asset-class views agree with the codec schema and the index grammar.
2. **Does it make sense.** Yes. The projector and the views are hand-written, and the agreement is cross-artefact. But the identity table is generated (`marketdata_market_series_identity_create.sql` carries the generated marker, from `ores.marketdata.market_series_identity.org`; *measured*), so `table_columns` should read the loaded model's columns, not the SQL.
3. **Python accidents copied.**
   - Every source is read by a shape regex (`case field::(\w+):`, `inline constexpr std::array …{…};`, one multi-line `create or replace view` pattern).
   - On any mismatch of shape it reports one `unreadable_source` and nothing else, which is honest. It returns silently when a file is missing.
4. **Cost.** Negligible. Five files.
5. **Shared data.** Create SQL (fifth table parser). The model of the identity table.
6. **Recommendation.** Keep. Read the table columns from the model or the shared SQL table model. Effort: small. Saving: negligible.

## ore_renderings_tables

1. **Intent.** The reader's code tables `conventions_renderings.hpp` are generated from `ore_renderings.json`.
2. **Does it make sense.** Yes as a files codec. Whether the header needs committing is ORE's call: it is compiled, so committing keeps the build free of a generator, the same reasoning as the XML bindings. The script's second verdict (no code table stated in `conventions_fields.cpp`) was dropped. That is a source rule with real value; it should be a finding of this codec's diagnostics twin, not lost.
3. **Python accidents copied.** None material. It returns silently on bad JSON (cross-cutting finding 2).
4. **Cost.** Negligible.
5. **Shared data.** `ore_renderings.json`, which value_sets also parses with `ops::json_value`. Two JSON readers (compass.model `read_json` here, `ops::json_value` there) for one file.
6. **Recommendation.** Keep. Report a parse failure. Restore the dropped source rule as a finding. Parse the file with one reader shared with value_sets. Effort: small. Saving: negligible.

## script_library_seed

1. **Intent.** The trading script library seed is what ORE's `scriptlibrary.xml` gives: scripts, variants and the inputs mined from the code.
2. **Does it make sense.** Yes. The seed must stay committed, because psql loads it. The mined inputs are a heuristic (identifiers used and not declared with `NUMBER` or assigned), which is a product decision. The authority would be ORE's script parser.
3. **Python accidents copied.**
   - The doc comment is a regex over raw XML (`<!--(.*?)-->\s*<Script>\s*<Name>`) next to the XML reader.
   - Input typing comes from token patterns.
   - Changing it changes the seed, so it needs ORE's agreement.
4. **Cost.** Negligible.
5. **Shared data.** ORE XML (`scriptlibrary.xml` only).
6. **Recommendation.** Keep. Report the `stopped` case instead of writing nothing. Effort: small. Saving: none.

## curve_sections_wiring

1. **Intent.** The curve screen's column wiring (TypeScript) and the curve part validators (SQL) follow `curve_configuration_projection.cpp` and `curveconfig.xsd`.
2. **Does it make sense.** Yes. Committing the TS module is ORE's choice. The problem is the silent stop: `wiring()->problems`, and the problems of `default_curve_types`, make it write nothing, and nobody reports the problems.
3. **Python accidents copied.** C++ projection parsing by regex in `ops::curve_wiring` (785 lines). Kept for parity. The script reads the same way.
4. **Cost.** Small. But the curve wiring is computed four times per cold run: here, in `curve_column_guards` through `ops::curve_columns`, and in each of the two value set codecs.
5. **Shared data.** The projection `.cpp`, `curveconfig.xsd` (also `ops::ore_schema` in value_sets and compass.sql `curve_parameter_paths`, three XSD readers), and the refdata models' `:tablename:` read raw.
6. **Recommendation.** Merge with `curve_column_guards` into one curve files codec. Its diagnostics twin reports `wiring` problems. Share one wiring with value_sets once prepare can be shared. Effort: small (merge) to medium (sharing). Saving: three of four wiring computations cold.

## curve_column_guards

1. **Intent.** The curve column map, its value guard trigger and the guard's drop follow the projection.
2. **Does it make sense.** Yes. It is the second output of the same wiring as `curve_sections_wiring`.
3. **Python accidents copied.**
   - `curve_table` reads `:tablename:` by `starts_with` on raw lines. The loaded model has `tablename`.
   - It returns silently on any problem.
4. **Cost.** Small.
5. **Shared data.** As `curve_sections_wiring`.
6. **Recommendation.** Merge into the same curve codec as `curve_sections_wiring`. Effort: small. Saving: one wiring pass.

## convention_kind_terms

1. **Intent.** The convention kind terms seed, the term validation triggers and their drops follow the `ores.ore` conventions mapper and the ORE terms.
2. **Does it make sense.** Yes. But `ops::convention_terms` is computed three times per cold run: here and in the two value set codecs. On ORE 545cafca45 it writes nothing because of `r.id`, and only `parameter_value_sets` says why.
3. **Python accidents copied.**
   - The mapper is parsed by regex. `function_re` needs a function to end at `\n}` and match `conventions_mapper::map_\w+`.
   - The `NOT_A_COLUMN` exemption list.
   - `convention_table` reads `:tablename:` by regex. The mapper parse is the script's design: the C++ mapper is the source, not a table.
4. **Cost.** Small.
5. **Shared data.** The mapper `.cpp` files, `convention_terms.json`, the convention kinds populate SQL, and refdata models (raw). All are shared with value_sets.
6. **Recommendation.** Keep. Fold it into the value set files codec (`ore_value_sets` already writes the other seeds from the same terms), so one codec resolves the terms once and `parameter_value_sets` reports why it stopped. Effort: small. Saving: one of three terms resolutions.

## route_inventory

1. **Intent.** `http_route_inventory.org` lists every route the BFF and `ores.http` register, classified and paired.
2. **Does it make sense.** As a knowledge page, yes. Whether it must be committed is a fair question. It is documentation derived wholly from sources, so a site build could render it. It is an org document in ORE's doc tree, so committing keeps it browsable. The ported half drops the script's other `--check` verdicts on sources, which were the gate's teeth.
3. **Python accidents copied.**
   - `called_by` picks the *first* client module, in pathlib order, whose text contains the route's literal prefix (at least 6 characters). That is walk order deciding an attribution, the same class of accident as the file-types tie. A route called by two modules is attributed to whichever sorts first.
   - Routes are found by regex over TS and C++.
   - Correct behaviour: list every caller, or report an ambiguity. That changes the page on ORE wherever a route has two callers. *Guess* that some do.
4. **Cost.** About 0.3 s in Python. It reads every `.ts` under `ores.web/packages` (whole text, kept in memory) and every `ores.http` `.cpp`.
5. **Shared data.**
   - `ores.http` sources: `route_units`, and compass.cpp `route_auth`.
   - BFF and web TS: compass.cpp `bff_list_requests`.
   - `route_classification.py` literals: `http_areas`.
6. **Recommendation.** Keep. Merge into a routes codec with `route_units`, `http_areas` and `route_auth`: one route table, many rules, plus the inventory output. Fix `called_by`. Effort: medium. Saving: two to three `ores.http` and TS passes.

## ore_output_catalogue

1. **Intent.** The output catalogue data (C++), its header and the ORE output formats page follow the output samples ORE ships.
2. **Does it make sense.** Yes. The data is compiled, and the page is documentation, which could be site-built.
3. **Python accidents copied.**
   - The walk order is re-created to mimic `os.walk` with sorted directories. That is honest parity, but the family a CSV name first lands in depends on the source order.
   - An unmatched seed name returns silently (`return;` in `run`), where Python fails.
4. **Cost.** About 0.65 s in Python. It lists every file under `external/ore/examples` (a very large listing) and parses every CSV.
5. **Shared data.**
   - The ORE examples tree: `conventions_coverage` (XML), compass.cpp `xml_family_census` (XML), and the unported `file-types-seed` (samples walk).
   - `reporting_file_types_populate.sql` (the file-types seed) and `ore_files.py`, which file-types-seed owns.
6. **Recommendation.** Keep. Report the stop. Share one listing of `external/ore/examples` with the XML scans. When file-types-seed is ported, the two belong in one ORE-outputs codec, since they read the same seed and `ore_files.py`. Effort: small now, medium with file-types. Saving: small.

## er_diagram

1. **Intent.** The committed PlantUML ER diagrams match the create scripts.
2. **Does it make sense.** Should the artefact be committed at all? Probably not. The diagrams are pure derived documentation, drawn from SQL already in the tree, and not compiled. ORE has a stale one at 545cafca45, which shows the commit-and-check loop costs more than it gives. Rendering them in the site build, or with `compass generate` on demand, would remove the gate.
3. **Python accidents copied.**
   - The parser's regex model of `create table` (column-per-line, type shortening, keys by name list `FK_COLUMNS`).
   - Data quality tables are split into four groups "by the shape of their names".
   - A stale group file is not reported, so a group that disappears leaves an orphan.
4. **Cost.** About 2.3 s in Python. Here it reads every create script and renders through pystache.
5. **Shared data.** Create SQL (four parsers; this one is `compass.sql::er_model_of`). Its table, column and FK model is the richest of the four, and is the natural base for the shared SQL table model.
6. **Recommendation.** Stop committing the diagrams, and generate them at site build. If they stay committed, keep the codec, but make `er_model_of` the shared SQL table model for the other three parsers. Effort: small (drop the commit; ORE's call) or medium (shared model). Saving: one SQL pass.

## conventions_dataset

1. **Intent.** `refdata_conventions_artefact_populate.sql` is what `conventions.tsv`, `oresmd_index_map.tsv` and `methodology.txt` generate.
2. **Does it make sense.** Should the artefact be committed? The SQL restates a TSV in the tree. psql's `\copy` can load a TSV directly, which would remove the generated SQL and this gate. That is ORE's call. If it stays, the codec is fine.
3. **Python accidents copied.**
   - Two Python decoder functions (commodity future, power load), which `COMPOSITE_KINDS` names, are hand-ported to C++. An edit to them in Python is not seen.
   - Numbers are written as Python's `str(int())`/`repr(float())`. That is parity, and it is right for byte equality.
4. **Cost.** Negligible.
5. **Shared data.** `conventions.tsv` and `oresmd_index_map.tsv`, which `conventions_coverage` parses again with the same `ops::csv_rows`.
6. **Recommendation.** Keep for now. Merge the dataset reading with `conventions_coverage` (one parsed dataset). Ask ORE whether the SQL can become a `\copy` of the TSV. Effort: small. Saving: one TSV parse; possibly the whole gate.

## conventions_coverage

1. **Intent.** The conventions dataset holds every convention ORE's examples hold, or `propose_decisions.tsv` says why not. Its indices are defined, and its rows have sources.
2. **Does it make sense.** Yes. It is the one check that the curated dataset tracks the vendored examples. Its inputs change only on re-vendoring.
3. **Python accidents copied.**
   - The "four rules", the `-CONVENTIONS` renames, and the MD5-of-signature pins of `decisions.tsv` are policy encoded as code.
   - Python's first-seen order of variants is kept, with the files sorted afterwards.
   - It returns silently when any of five inputs is missing.
4. **Cost.** About 0.7 s in Python. Here it is a scan of every `.xml` under `external/ore/examples`, each parsed once and cached.
5. **Shared data.** ORE example XML: compass.cpp `xml_family_census` reads the same files for root elements. One scan fact (root element plus `Conventions` entries) would serve both. Dataset TSVs: `conventions_dataset`.
6. **Recommendation.** Keep. Report a missing input. Share the examples XML scan with `xml_family_census` and the dataset parse with `conventions_dataset`. Effort: medium (cross-component scan). Saving: one cold XML pass over the examples.

---

## Shared passes

Each group is one data set and one tokenisation. Today each gate in a group reads and parses on its own.

### P1. Codegen models, loaded (the big one)

- **Data.** Every org model under `projects/**/modeling/`, plus profiles and custom types from `projects/modeling/`.
- **Today.** Raw bytes, then line regexes, in each gate.
- **Gates.**
  - `money_decimal_columns`: columns, `type`, `cpp_type`.
  - `model_sql_types`: columns and `:tablename:`.
  - `model_drift`: drawers and profiles.
  - `http_areas`: `#+http_area`.
  - `route_units`: http-route enabled and singular.
  - `protocol_twins`: opt-out key.
  - `subject_conformance`: `:subject:`.
  - `paste_blocks`: `:implements` blocks.
  - `generated_column_refs` and `entity_binary_types`: facts stated by the model, read back from output.
  - `marketdata_identity_columns`: table columns.
  - `ops::curve_columns` and `ops::convention_table`: `:tablename:`.
- **Already loaded by.** compass.model (all 844) inside the codegen prepare and parts. The loader already enforces `unknown profile`, `conflicting profiles` and the entity checks.
- **Shape.** A document codec over the codegen model types (`model_rules`). It loads each model through compass.model with the recorded finder and runs every per-model rule. That makes it incremental per model and parallel. Cross-model facts (AREAS, profiles) are read through the view and recorded. When the runtime gains a shared prepare (F51A1EEB), the codec reads codegen's loaded models instead of loading them again.

### P2. Create SQL, as a table model

- **Data.** `projects/ores.sql/create/**/*.sql`.
- **Today.** Four or five separate regex parsers:
  - `model_sql_types`;
  - `entity_binary_types`;
  - `generated_column_refs`;
  - `er_diagram` (`compass.sql::er_model_of`);
  - `marketdata_identity_columns`.
- **Other readers of the same files.** compass.sql `sql_hygiene` (lexer-accurate scan), `rls_widenings` and `populate_refs` (function bodies), and `entity_writes` (function bodies, by `$$` search).
- **Shape.** One compass.sql scan whose per-file fact is the masked statement list: tables with columns, types and constraints; indexes; views; and functions with bodies. It is built on the `sql_hygiene` lexer. The table-model gates, `entity_writes` and the ER diagram reduce over it.

### P3. C++ headers, tokenised

- **Data.** Every `.hpp` under `include/`: 11,815 files, 84 MB (*measured*).
- **Today.** `co::blank_comments_and_literals`, then identifier walks.
- **Gates.** `paste_blocks` here. In compass.cpp: `enum_attributes`, `unique_type_names`, `exported_types` and `forward_declarable_includes`.
- **Shape.** One compass.cpp header scan whose fact is the declarations by scope. Dropping `paste_blocks` removes the heaviest consumer.

### P4. Hand-written C++ sources, scanned for text patterns

- **Data.** Every hand-written `.cpp`/`.hpp` under `projects/ores.*`: about 21,000 files (*measured*).
- **Gates.** `entity_writes` (DML to `_tbl`) and `subject_conformance` (subject constants; dead today). In compass.cpp: `portable_stop`, `stream_coverage` and `unity_collisions`.
- **Shape.** Byte prefilters per gate (`_tbl`, `nats_subject`) before decoding. Better, one multi-pattern scan whose fact holds each gate's hits.

### P5. Protocol headers and generated messaging code

- **Data.**
  - `projects/ores.*/api/include/*/messaging/*_protocol.hpp`;
  - generated `*_handler.hpp`, `*_history_provider_registrar.cpp`, `*_table.cpp` and `repository/*_entity.hpp`.
- **Gates.**
  - `open_reads`: request struct to subject, handlers, registrars.
  - `protocol_twins`: header existence.
  - `subject_conformance`: the C++ half.
  - compass.cpp `bff_list_requests`: protocol headers.
  - `entity_binary_types` and `generated_column_refs`: generated entity and table code.
- **Shape.** Generated files are the `codegen` codec's own outputs, so rules on them belong on the codegen context (P1 and codegen). The hand-written protocol facts go in one scan: struct to subject, per header.

### P6. Routes

- **Data.**
  - `projects/ores.http/*/src/**/*.cpp`;
  - `projects/ores.web/packages/**/*.ts`;
  - `route_classification.py` and `route_pairing.py` literals.
- **Gates.** `route_inventory`, `route_units` and `http_areas`. In compass.cpp: `route_auth` and `bff_list_requests`.
- **Shape.** One routes codec. It produces one route table (method, path, unit, component, area, auth), the inventory page, and every route rule.

### P7. Curve projection and ORE curve schema

- **Data.**
  - `curve_configuration_projection.cpp`;
  - `external/ore/xsd/curveconfig.xsd`;
  - refdata models' `:tablename:`.
- **Gates.** `curve_sections_wiring`, `curve_column_guards`, and the two value set codecs through `ops::curve_columns`, so the wiring is computed four times per cold run. Also compass.sql `curve_parameter_paths`, which reads curveconfig.xsd with a third schema reader beside `cw::leaves_of_schema` and `ops::ore_schema`.
- **Shape.** One curve files codec for both outputs, plus a diagnostics twin for wiring problems. One schema reader (`ops::ore_schema`) for all three. Share the wiring with value_sets once prepare is shareable.

### P8. Convention terms

- **Data.**
  - the `ores.ore` conventions mapper `.cpp` files;
  - `convention_terms.json`;
  - the convention kinds populate SQL;
  - refdata models.
- **Gates.** `convention_kind_terms`, `ore_value_sets` and `parameter_value_sets`, so the terms are resolved three times per cold run.
- **Shape.** Fold `convention_kind_terms`' outputs into `ore_value_sets`, which already writes from the same resolution.

### P9. ORE JSON curation and renderings

- **Data.** `ore_renderings.json`, the curation JSON, and `curve_terms.json`.
- **Gates.** `ore_renderings_tables` (compass.model `read_json`) and value_sets (`ops::json_value`). Two JSON readers.
- **Shape.** One reader, `ops::json_value`.

### P10. ORE examples tree

- **Data.** `external/ore/examples/**` (XML and CSV) and `projects/ores.ore/data/samples/`.
- **Gates.** `conventions_coverage` (XML scan), compass.cpp `xml_family_census` (XML roots), `ore_output_catalogue` (CSV walk), and the unported file-types-seed (samples walk).
- **Shape.** One XML scan whose fact is the root element plus `Conventions` entries. One shared listing of the examples tree.

### P11. Conventions dataset TSVs

- **Data.** `conventions.tsv` and `oresmd_index_map.tsv`.
- **Gates.** `conventions_dataset` and `conventions_coverage`. Both use `ops::csv_rows`, separately.
- **Shape.** One parsed dataset.

### P12. Python policy modules, through the literal reader

- **Data.** `component_registry.py`, `route_classification.py`, `generate_output_catalogue.py`, `output_family_docs.py`, `ore_files.py`, `propose.py`, `generate_sql.py`, the ER parser's tables, and the value set modules.
- **Gates.**
  - `registry_exceptions` and `protocol_twins` both read `COMPONENTS_UNDER_TEST`.
  - `route_inventory` and `http_areas` both read `route_classification.py`.
- **Shape.** Cheap individually. The win is not speed: these tables should move to data files, as the generator-gates investigation already recommended.

### Templates

- **Data.** `.mustache` templates and the `ores.*.org` nodes.
- **Gates.** `template_sections` (tag regex) and `protocol_twins` (re-loads the physical-space graph). The codegen prepare step compiles both.
- **Shape.** Rules over `compiled_template` and the prepared graph.
