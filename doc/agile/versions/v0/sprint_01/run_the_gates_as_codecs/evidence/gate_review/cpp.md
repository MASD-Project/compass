# Review of the compass.cpp codecs

Scope: the 19 codecs in the 16 files of `projects/compass.cpp/src/codecs/`, plus
the ops they use (`ops/text`, `ops/paths`, `ops/glob`, `ops/report`,
`ops/enum_attributes`, `ops/exported_types`, `ops/test_case_reachability`).
The original scripts in `~/Development/OreStudio/ores_dev_prime_origin` were
read for their intent. Nothing was run. Every saving below is an estimate from
reading the code, labelled a guess, except the three header-edit timings the
brief gave.

## Facts this review leans on

- ORE has 24,812 tracked `.cpp`/`.hpp` files under `projects/`, 200 MB in all.
  21,737 of them (176 MB, 88%) carry `AUTO-GENERATED FILE`. Measured with
  `git ls-files | xargs cat | wc -c` at the ORE checkout.
- Eight codecs here map every C++ file under `projects/` or most of it:
  `portable_stop`, `enum_attributes`, `exported_types`,
  `forward_declarable_includes` (headers only), `unique_type_names` (headers
  only), `unity_collisions` (`.cpp` only), `stream_coverage` (non-test) and
  `test_case_reachability` (test sources). Three more in `compass.crosscheck`
  do the same: `subject_conformance`, `entity_writes`, `paste_blocks`. A cold
  run reads and tokenises the same 200 MB up to eleven times.
- There are **six different comment and literal blankers** in this component
  alone, and none agree:

| Codec | Blanker | Raw strings | Char literals | Splices | Decoding |
|---|---|---|---|---|---|
| `portable_stop` | regex `noise` | yes, delimited | **no** | no | `decode_ignoring` |
| `enum_attributes` | `erase_comments_and_literals` | **no** | yes, **runs across lines** (a digit separator `1'000` opens one) | for `#define` only | raw bytes |
| `unique_type_names` | regex `lexeme`, also blanks `#` lines | yes, delimited | yes | no | `decode_replacing` unless UTF-8 |
| `unity_collisions` | regex `lexeme` | **only `R"(`, no delimiter** | yes | no | skips non-UTF-8 |
| `forward_declarable_includes` | `strip_comments`, keeps literals | no | keeps | no | `decode_ignoring` |
| `stream_coverage` | regex, keeps strings | no | no | no | `decode_replacing` |
| `route_auth` | `mask_code` | yes, delimited | yes | no | raw bytes |
| `test_case_reachability` | `blank_comments_and_literals` | yes | yes, digit separators handled | yes | raw bytes |
| `exported_types`, registrars, shell, `translation_unit_budget` | none | | | | raw or replaced |

  `test_case_reachability`'s lexer is the only one that handles C++ as the
  compiler does. Each other one is a copy of what one script happened to write.

---

## portable_stop

1. **Intent.** No code outside `ores.platform` names `std::jthread`,
   `stop_token`, `stop_source` or `stop_callback`, or includes
   `<stop_token>`, because Apple's libc++ lacks them and the macOS build that
   would fail runs only at night.
2. **Does it make sense.** Yes, while Apple's libc++ lacks the types. The script
   says the gate and the shim go when it gains them. Nothing else in compass
   checks it. The compiler cannot do it on Linux. It is a "forbidden
   identifier in these paths" rule, and it is the simplest gate here: a token
   search.
3. **Python accidents copied.**
   - The blanker does not know char literals, so `'"'` opens a string that
     runs to the next `"` on the line and can hide code after it. The correct
     behaviour is the shared lexer. On ORE this is unlikely to change a
     verdict (guess).
   - It decodes with `errors=ignore` and normalises newlines, then maps the line
     back to a byte offset. A check on tokens needs neither: C++ sources are
     UTF-8 bytes and the identifiers are ASCII.
   - It runs a regex per line over a regex-blanked copy. A token search on the
     lexed text does the same work in one pass.
   - `std :: jthread` and `using std::jthread; jthread t;` are caught or not by
     accident. The second is caught because of the `using` line. A token rule
     (`std`, `::`, `jthread`) is exact.
4. **Cost.** A scan with an empty fact. Map reads the bytes, decodes them,
   normalises newlines, runs a lazy-dotall regex substitution over the whole
   file, splits the lines and runs two regexes per line. That is the heaviest
   per-byte work in the component for the simplest rule. Cold, it costs a full
   200 MB pass. Warm, an edit costs one map, which is fine.
5. **Shared data.** Every C++ file under `projects/`, 8 suffixes, outside
   `ores.platform` and the skipped dirs. Tokenisation: comments and string
   literals blanked, then lines. Same data and a compatible tokenisation as
   `enum_attributes`, `exported_types`, `unique_type_names`,
   `unity_collisions`, `test_case_reachability` and, in crosscheck,
   `subject_conformance`, `entity_writes` and `paste_blocks`.
6. **Recommendation.** Simplify, then merge. Express it as one row of a
   "forbidden names" table (identifier sequence, include, exempt path prefix)
   evaluated on the shared lexed form (see Shared passes). Until the shared
   pass exists, swap the regex blanker for `ops::scan_source`'s lexer and
   search tokens. Effort small. Saving: cold, one of eight full regex passes
   over 200 MB, roughly 1 to 2 s of CPU (guess). Warm, nothing.

## enum_attributes

1. **Intent.** No visibility or DLL storage macro decorates an enum. clang-cl
   fails on `__declspec(dllimport)` on an enum, and only the Windows build sees
   it.
2. **Does it make sense.** Yes. Linux and macOS accept the attribute, so no
   local compiler reports it. It is a sibling of `exported_types`: both are
   about `ORES_*_EXPORT` on the Microsoft ABI, both read every C++ file under
   `projects/`, and both work out which macros are export macros. Here the set
   is "any `#define` anywhere whose body names `BOOST_SYMBOL_*`,
   `__declspec(dll*)` or `__attribute__((visibility`". In `exported_types` it is
   the macro each `export.hpp` defines. On ORE they are the same set plus the
   two Boost names (guess, not diffed).
3. **Python accidents copied.**
   - The eraser has no raw strings, and a `'` runs to the next `'` across
     lines, so `1'000'000` blanks real code until the next quote. The correct
     behaviour is the shared lexer. It can only hide a site, never invent one.
     It changes no verdict on ORE today, since there are no findings (guess).
   - The macro set is learned from every file, comments and strings removed but
     `#if 0` blocks included. Reading it from the export headers, as
     `exported_types` does, states the actual rule ("an export macro"), not a
     heuristic.
   - Finding enums is a hand-written scanner with a "follows `using`" special
     case. On lexed tokens it is `enum [class|struct] MACRO name (:|{|;)`.
4. **Cost.** A scan. Map erases the file (one pass), joins continued lines,
   splits lines, scans `#define`s, then finds every `enum`. The fact is the
   macros the file defines plus every candidate site, with the source line
   copied in. Reduce unions the macro sets and filters the sites, which is cheap.
   It costs one full 200 MB pass cold.
5. **Shared data.** C++ under `projects/` (`.hpp .h .cpp .ipp .cc`).
   Comment- and literal-blanked text, `#define` directives, identifier runs.
   It shares the export-macro fact with `exported_types` and the lexed form with
   every C++ scan.
6. **Recommendation.** Merge with `exported_types` into one scan,
   "windows_exports", with rules `visibility_macro_on_enum` and
   `uncompiled_exported_type`. Map emits, from one lexed pass: the export macro
   if the file is an `export.hpp`, the quoted includes, the `class|struct MACRO
   name` declarations, and the `enum MACRO name` sites. Effort medium (the rule
   names change, so the inline exceptions that name `enum_attributes/…` move.
   ORE has none, guess). Saving: one full C++ pass cold (1 to 2 s CPU, guess).

## exported_types

1. **Intent.** A library that marks a type with its export macro compiles that
   header in at least one of its own translation units. Otherwise every
   Windows consumer imports a symbol nobody defines, and the build fails at
   link, hours in.
2. **Does it make sense.** Yes. It is the most valuable of the Windows gates and
   was the slowest Python gate (62 s). The compiler cannot see it on Linux.
3. **Python accidents copied.**
   - **Includes are read from raw bytes**, so an `#include "x.hpp"` inside a
     comment, a string or an `#if 0` block counts as reaching `x.hpp`. That can
     hide a real finding: a false negative. The correct reading is the include
     directives of the lexed text. Whether it changes a verdict on ORE is
     unknown. Diff the two include sets over ORE to find out.
   - **Resolution against every `projects/*/*/include` root, in pathlib
     order**, first hit wins. The compiler uses the target's include
     directories. Two components with the same relative header would resolve to
     the wrong one. ORE namespaces its include dirs (`ores.x.y/…`), so this is
     unlikely to bite (guess). A correct check would take the include roots
     from the component layout (`component_architecture.org` defines it), not a
     global sorted list.
   - `..` "goes up only from a directory that exists, as the operating system
     resolves it". That is a faithful model, not an accident.
   - `AUTO-GENERATED FILE` anywhere in a header exempts all of it. That is
     defensible, since generated headers are compiled by generated sources, but
     it is a substring test on the whole file.
4. **Cost.** A scan whose fact is the includes, the export macro and the
   exported types. Reduce calls `v.list_files("")`, **the listing of every path
   in the checkout**, so adding or removing any file anywhere (a doc, a test
   fixture) makes the reduce run. It then builds an include graph per library
   from all its `.cpp`. A header edit that changes the header's includes or
   exports reruns the whole reduce: about 300 ms, as the brief gives. It also
   reads, through the view, any included file that is not scanned, and every
   such file then also reruns the reduce when edited.
5. **Shared data.** All C++ under `projects/` (9 suffixes). Raw-byte include
   directives, `#define … BOOST_SYMBOL`, `class|struct MACRO name` lines. The
   include fact is the same one `forward_declarable_includes` builds (with
   another regex, on decoded text) and that a future include-graph gate would
   want. The export macro fact is `enum_attributes`'.
6. **Recommendation.** Merge with `enum_attributes` (above), on the shared
   lexed form. In the reduce:
   - list `projects/` with the scanned suffixes instead of `""`;
   - take include roots from the component layout;
   - make the reachability walk per library and memoise "headers reached from
     the library's sources" so that an edit outside a library does not redo its
     walk.

   Effort medium. Saving: the 300 ms header-edit reduce falls to the libraries
   whose graph changed (estimate: tens of ms, guess). It also stops reruns on
   unrelated file additions.

## unique_type_names

1. **Intent.** No two headers that can meet in one binary define a class or
   struct of the same qualified name. The linker keeps one copy of the
   templates instantiated for it and silently uses the wrong layout.
2. **Does it make sense.** Yes. This is an ODR violation that no compiler
   reports in a normal build. GCC's `-Wodr` under LTO does report it, but ORE
   does not build with LTO in CI (guess). Keep.
3. **Python accidents copied.**
   - The "binary" model is path-shaped: every header under an `include/` is one
     binary "public", and test headers group by the path up to `tests`. That is
     a reasonable approximation, but it is not the link graph: two public
     headers of components never linked together are still reported. That
     direction is conservative, so it stays a false positive, not a miss.
   - It blanks preprocessor lines entirely (`^\s*#[^\n]*`), so a type defined
     in a macro body is not seen. That matches what a token reader would do.
   - Offsets are zeroed for a file that is not valid UTF-8 or that has `\r`,
     only to agree with Python's offsets on decoded text. A byte-based lexer has
     no reason to do it.
   - Regex tokenising (`namespace … {`, `struct|class [MACRO] name … {`, `{`,
     `}`) on a regex-blanked copy. That is the same scope-stack model that
     `unity_collisions` and crosscheck's `paste_blocks` each build again with
     their own regexes.
4. **Cost.** A scan over headers in `include/` or `tests/`. Map runs two regex
   passes (a blanking substitution, then a token `find_all`) over every
   header, about 170 MB of mostly generated text. The fact is small. Reduce is
   cheap and a header edit is one map.
5. **Shared data.** `.hpp` under `projects/ores.*/…/{include,tests}/`.
   Comment/literal-blanked text, then a namespace/brace scope stack with class
   definitions. It shares that scope model with `unity_collisions` (`.cpp`) and
   `compass.crosscheck/paste_blocks` (all `.hpp`, `declared_symbols`).
6. **Recommendation.** Keep the rule. Put its map on a shared "namespace-scope
   declarations" fact per file: each declaration at namespace scope with its
   namespace path, name, kind (type definition, function, variable), and
   linkage (anonymous namespace, `static`, non-inline `const`). Effort medium,
   shared with the two others. Saving: one 170 MB regex pass cold (1 to 2 s
   CPU, guess).

## unity_collisions

1. **Intent.** No two sources of one target define the same internal-linkage
   name in the same namespace outside `cmake/unity_exclusions.cmake`, because a
   unity build merges them and fails.
2. **Does it make sense.** Partly. The unity build is the real check, and the
   script says the compiler already finds what this scan cannot (types in
   anonymous namespaces, aliases, ambiguous calls). The gate is an early,
   approximate warning. Keep it, because a unity build is slow, but treat it as
   a heuristic. `missing_recorded_file` is exact and cheap.
3. **Python accidents copied.**
   - **Raw strings are recognised only as `R"(…)"`, with no delimiter**,
     unlike the four other blankers that handle `R"d(…)d"`. A delimited raw
     string containing `"` or `//` desyncs the brace and paren counts that
     follow. The correct behaviour is the shared lexer. It could change a
     verdict where ORE has delimited raw strings in `.cpp` (unknown, worth one
     diff).
   - **Line-based declaration matching** with a 3-way regex, a "first word" deny
     list (`return`, `typedef`…), and a paren count from `str.count("(")` that
     counts across lines. It is a heuristic for "a declaration at namespace
     scope". On a token stream with a scope stack, the declaration's name is
     the identifier before the first `(`, `=`, `{` or `;` at depth zero.
   - **The target is "the directory of the nearest `CMakeLists.txt`"**, not the
     target's sources. A CMakeLists that builds two targets, or one that globs
     a subdirectory with its own CMakeLists, is modelled wrongly. The correct
     source is the component layout (part = target), which compass already has
     for codegen. Unknown verdict change.
   - Files that are not UTF-8 are skipped silently. A correct check reads bytes.
4. **Cost.** A scan over `.cpp` under `projects/ores.*`. Map is a regex
   substitution over the whole file, then a line loop with a regex and a
   `find_all` per line. Reduce probes `file_exists` up the tree per directory
   (memoised) and reads the exclusions file. The fact is small, so an edit is
   one map.
5. **Shared data.** `.cpp` under `projects/ores.*`, `CMakeLists.txt` existence,
   `cmake/unity_exclusions.cmake`. Comment/literal-blanked text, a namespace
   scope stack, declarations. It shares the scope model with
   `unique_type_names` and `paste_blocks`, and the CMake tree with
   `core_link_edges`.
6. **Recommendation.** Keep the rules. Move map onto the shared
   namespace-scope declarations fact (same as `unique_type_names`). Take the
   target from the component layout. Effort medium (shared). Saving: one
   regex pass over the `.cpp` files cold (about 1 s CPU, guess), plus fewer
   false positives.

## forward_declarable_includes

1. **Intent.** A header that only names a listed type (pointer, reference,
   `shared_ptr` argument, declaration) includes its forward declaration, not its
   definition, because `context.hpp` made most of the tree rebuild.
2. **Does it make sense.** Yes. It is a build-time hygiene rule the compiler
   does not state. include-what-you-use does the general version, but it is far
   too slow for an always-on check. The 29-line list keeps it targeted.
3. **Python accidents copied.**
   - Whether a use "needs the definition" is decided by regexes over the text
     before and after each occurrence (`key_before`, `alias_before`,
     `enclosing_template` that walks back to the nearest `<`). It is a heuristic,
     and the script states that. A correct version on tokens would classify the
     token after the type name (`*`, `&`, `>`, `,`, `)`, an identifier) and the
     template the name sits in, which is the same rule without rescanning the
     prefix with `$`-anchored regexes. Verdict change: none expected (guess).
   - `strip_comments` keeps literals, so a type name inside a string counts as
     a use. That is a false "needs" in a rare case.
   - `include_line` finds the line by substring (`line.find(include) &&
     line.find("#include")`). The map already had the directive offsets.
   - The finding message carries a `(name:template:…)` key that was the
     baseline's key. With inline exceptions it is noise.
4. **Cost.** This is the defect. **Map records only the include list, and
   reduce reads again every header that includes a defining header**, then
   compiles and runs about ten patterns per type per header. Because those
   headers are read by the reduce, an edit to any of them reruns the whole
   reduce: the 530 ms on a header edit in the brief. The list file is read by
   reduce for the same reason: map cannot read files.
5. **Shared data.** `.hpp .h .ipp` under `projects/`. Include directives
   (a regex over decoded text), then comment-stripped text with literals kept.
   It shares the include fact with `exported_types` (raw bytes, another parser)
   and the lexed form with every C++ scan.
6. **Recommendation.** Simplify. **Do the whole verdict in map**, so the reduce
   only gathers findings and an edit costs one map. That needs map to know the
   list. Either:
   - (a) make the list the codec's option value (29 rows in `.compass.org`, as
     `translation_unit_budget` and `xml_family_census` already hold their
     tables); or
   - (b) add runtime support for a scan "context file" whose content ID joins
     every map's key.

   (a) is small and needs no runtime change. Only headers whose includes name
   a listed defining header do the regex work, so map stays cheap for the
   rest. Effort small. Saving: about 530 ms per header edit, down to one map
   (a few ms, guess).

## stream_coverage

1. **Intent.** Every durably consumed NATS subject is covered by exactly one
   stream, a consumer names a stream that something ensures and that covers
   it, and no two streams overlap. The broker refuses these at start, or a
   publisher times out.
2. **Does it make sense.** The invariant does. The method is the most fragile
   in the component: it resolves C++ expressions (constants, `nats_subject`
   members, arrays, the arguments callers pass to a function's parameter) with
   regexes, and fails on any shape it does not know. That is the right failure
   mode, but it means the gate is a partial C++ evaluator. The subjects
   themselves come from the models (`:subject:`), which crosscheck's
   `subject_conformance` already reads. The streams would be a table if they
   were declared as data.
3. **Python accidents copied.**
   - The `tree` builds `values_` (every `constexpr … = "…"` in every header,
     keyed by bare name) and resolves a name by its last component when the
     qualified lookup fails. Two unrelated constants with one bare name make a
     subject unresolvable, and a wrong one with the same name resolves
     silently.
   - `members_` assigns a `nats_subject` to "the last `struct|class X {` before
     it", and the region of a struct runs to the next struct start, not its
     closing brace.
   - `bound_service` finds "the enclosing function" as the last line-start
     `name(` before the call, then scans every raw file's text for callers.
   - Comments are blanked but only ASCII bytes are replaced. Non-ASCII bytes in
     a comment are kept, a byte-for-byte copy of a Python quirk.
   - The `root_menu`-style formatting dependence (`serve_step_commands` region
     runs from the routes table declaration to the next `serve_step_commands`)
     makes the result depend on statement order in the file.

   Correct behaviour: either declare streams and consumers as data (a model
   facet, generated into the C++) and check the data, or keep the static
   reading but resolve names by qualified scope. The first removes the gate's
   reason to parse C++. Verdict change: none expected today (guess). It
   changes which future edits fail.
4. **Cost.** **The fact of a "named" file is its whole decoded text.** "Named"
   includes any file with a `constexpr … string_view|auto|char* name = "…"`,
   about 1,615 headers plus 1,827 non-test files that mention a NATS name
   (counted with grep on ORE). So the scan table holds tens of MB of source as
   facts. Any edit to such a file changes its fact, so the reduce, which
   re-strips comments of every named file and rebuilds the tree, runs on almost
   every header edit: the 640 ms in the brief. The reduce also reads files
   through the view (callers it adds, and every file a finding names), and
   those also force reruns.
5. **Shared data.** Non-test `.cpp`/`.hpp` under `projects/`, outside
   `ores.nats`. Decoded text with comments blanked and strings kept, string
   constants, `nats_subject` members, braced arrays, call sites with their
   argument lists. It shares the string-constant and subject fact with
   `compass.crosscheck/subject_conformance` (all `.hpp`/`.cpp`, regex
   `\w*subject\w*\s*=\s*"…"`) and `handler_registrars`
   (`(\w+_request)::nats_subject`). Three gates extract subject names from C++
   with three regexes.
6. **Recommendation.** Simplify now, replace later.
   - Now (medium): **move the extraction into map.** The fact holds the file's
     string constants (qualified by namespace and enclosing type), its
     `nats_subject` members, its braced arrays, each `ensure_stream`,
     `serve_step_commands` and `js_queue_subscribe` call with its raw argument
     texts and enclosing function signature, and the names it calls. The reduce
     resolves over facts only and reads no file. An edit that changes no
     declaration leaves the fact the same, and the reduce is skipped. Saving:
     about 640 ms on most header edits (guess: to a few ms), and the scan table
     shrinks by tens of MB, which also shrinks the record that every run loads.
   - Later (large, an ORE change): declare streams and durable consumers in the
     models next to `:subject:`, generate the C++ calls, and check the model.
     Then this gate and `subject_conformance` read one fact.

## handler_registrars, event_registrars, history_registrars

1. **Intent.** Every generated registrar (handler, event mapping, history
   provider) of the listed components is called from the component's
   composition file. For handlers, it can instead have every subject answered
   by hand. An uncalled registrar subscribes nothing and requests time out.
2. **Does it make sense.** As a gate, it is a patch on a generator gap: the
   generator emits the registrars but not the call that composes them. If the
   generator emitted the composition (a generated `registrar.cpp` calling
   every registrar of the component, with a hand-written hook for exceptions),
   the invariant would hold by construction and the three gates would go. The
   hard-coded component lists (`handler_components` 8, `event_components` 3,
   `history_components` 2) are a second smell: the script admits 54 uncalled
   registrars in other components, and those components are not checked at
   all.
3. **Python accidents copied.**
   - **Calls are matched in raw text, comments included.** A commented-out
     `register_x_handlers(` counts as "called", which is a false negative. A
     declaration outside the registrar's own files also counts. The correct
     rule counts calls in lexed code, not declarations. Verdict change on ORE:
     unknown, one diff.
   - "The last header of an entity wins", copied from a dict.
   - Ownership is decided by the stem ending in `_registrar`.
4. **Cost.** Handler is a scan over all `.cpp/.hpp` of 8 components (small
   facts, cheap). Event and history are corpus functions that list and read
   their headers directly: about 0.05 s, so they need no scan.
5. **Shared data.** `.cpp/.hpp` of the listed components. Raw text and three
   regexes (`register_(\w+)_handlers\s*\(`, `…_event_mapping`,
   `…_history_provider`, `(\w+_request)::nats_subject`). This overlaps
   `compass.sql/handler_permissions` (the same `*_handler.cpp` in messaging
   dirs) and the subject extraction of `stream_coverage` and
   `subject_conformance`.
6. **Recommendation.** Merge the three into one codec, "registrar_composition",
   with one listing per component and the call fact taken from lexed text.
   Effort small, saving negligible at runtime but three descriptors become one.
   Real fix: have the generator emit the composition and **drop all three**.
   Effort medium, in ORE's templates or compass codegen.

## shell_command_units, shell_menus, shell_command_composition

1. **Intent.** A shell menu is built once, by one file, from its name alone
   (the runtime reads the name back through `Prompt()`). A command unit is
   registered once, and every generated command unit is registered.
2. **Does it make sense.** Mostly. `shell_root_menu` now refuses a second claim
   at startup, as the script says. The gate stays for the `Prompt()` invariant
   and for units the fleet does not start. But two of the three codecs **report
   the same defect twice**: `make_unique<cli::Menu>("x")` matches both
   `run_units`'s `menu` regex and `run_menus`'s `construction` regex
   (`cli::Menu\s*>?\s*\(`). A menu built in two files is reported as
   `shell_command_units/menu_owned_twice` and again as
   `shell_menus/root_name_collides`. `shell_command_composition` is the same
   "generated but not composed" pattern as the registrars.
3. **Python accidents copied.**
   - `root_command` is `root_menu\.Insert\(\s*\n\s*"`: it **requires a
     newline after the paren**, so `root_menu.Insert("x", …)` on one line is
     not seen. That is a clang-format accident. Correct: `\s*`. Verdict change
     on ORE: unknown, depends on how clang-format wrapped each call.
   - Each file is read twice per codec (`read_file` for bytes, then
     `read_replacing` reads and decodes again), and offsets are zeroed when the
     decoded text differs from the bytes.
   - `composition_components` is hard-coded to `refdata` and `reporting`.
4. **Cost.** Three corpus functions over `projects/ores.shell/**/*.cpp`, each
   file read twice in two of them. They are cheap (0.05 to 0.2 s in Python),
   but any shell edit reruns all three whole.
5. **Shared data.** `.cpp` under `projects/ores.shell/`. Raw text and regexes
   for menu constructions, `Insert` calls and `register_commands` calls. All
   three read the same files.
6. **Recommendation.** Merge into one scan, "shell_composition": per-file facts
   (menus constructed with or without a prompt, root inserts, registrations),
   with one rule set that reports a duplicate menu once. Fix the `Insert`
   regex. Long term, generate the composition, as for registrars, and drop
   `shell_command_composition`. Effort small. Saving: small at runtime. It
   removes a duplicate finding and two descriptors.

## route_auth

1. **Intent.** Every `add_route` of `ores.http` states its authentication
   position (`auth_required`, `auth_optional`, `roles`), because a route with
   none used to answer anyone.
2. **Does it make sense.** **It duplicates a runtime check that already
   exists.** The route builder refuses such a route at `build()` and the router
   at `add_route()` (`router.hpp:185,252`, `auth_declared_`), and per-entity
   route tests check `auth_declared`. The gate is earlier than a server start,
   but it is a regex reading of what the type system could state. Make the
   builder a typestate (a builder with no auth position has no `build()`), and
   the compiler enforces it with no gate. Short of that, one test that
   registers every route module at startup makes the gate redundant.
3. **Python accidents copied.**
   - It follows `auto` locals one level through `x = y.build();` aliases and
     takes "the first `name =` on a line containing `auto`". That is a
     heuristic for data flow.
   - `too_few_routes` (fewer than 20 found) exists only because a regex might
     stop matching. A typestate or a runtime check has no such failure mode.
   - It counts `add_route` matches on a masked copy and again on the raw text.
4. **Cost.** A corpus function over 487 `ores.http` `.cpp`/`.hpp` files. It
   masks each file, builds a second masked copy, and compiles a regex per
   variable name it follows. It costs 2 s in Python, less here. Any `ores.http`
   edit reruns it all.
5. **Shared data.** `.cpp/.hpp` under `projects/ores.http/`, masked text and
   builder chains. **`compass.crosscheck/route_inventory` reads the same files**
   for `router->get("…")` and the BFF routes. Two codecs find the routes of one
   tree with two regexes.
6. **Recommendation.** **Drop**, once ORE either makes the route builder a
   typestate (medium, ORE) or adds one test that builds the full router (small,
   ORE). Until then, fold its detection into `route_inventory`'s read of the
   same files, so routes are found once. Effort small. Saving: one pass over
   `ores.http` and a whole codec.

## bff_list_requests

1. **Intent.** A hand-written BFF list request states `as_of` and its filter
   when the C++ protocol declares them, a literal filter states exactly the
   filter's members, `readAll` reads the rows the reply carries, and the
   generated `list_requests.ts` table is what the protocol headers give.
2. **Does it make sense.** It is two gates in one. `stale_table` is a generator
   drift check. The other three check hand-written TypeScript against C++
   structs with regexes. TypeScript has a compiler: if the generator emitted a
   TS type per list request (filter members, `as_of`, rows key) instead of, or
   beside, a runtime shape table, `tsc` would reject a missing member, an
   unknown filter key or a wrong rows key, and three rules would go.
   `completeListRequest` already fills missing members at run time, which
   makes `missing_member` softer than it reads.
3. **Python accidents copied.**
   - **Two definitions of "rows".** `requests()` calls a member a row when
     `vector<` appears in the 60 characters before the member's first substring
     occurrence in the reply body (substring: `id` matches inside `ids`).
     `render_table()` uses `std::vector<[^>]*>\s+member\s*;`, which fails on a
     nested template (`std::vector<std::pair<a, b>>`). `members_of` is also
     called with `skip_structs` false in one path and true in the other. That
     is the same defect the value-set review found: one fact, two rule lists.
     The correct behaviour is one parse of the protocol structs, used by both
     halves. Verdict change: unknown. It would show as a table diff or a
     `wrong_rows` change.
   - `struct\s+(\w+)\s*\{(.*?)\n\};` requires the closing `};` at column 0.
   - The renderer mimics prettier's array layout at width 100. That is
     necessary while the file is committed. It would be unnecessary if the file
     went through the codec formatter that `compass.codec` already has.
   - `render_table` compiles a new regex per member per request.
4. **Cost.** A corpus function. It reads every `*_protocol.hpp` under `api/`,
   every BFF `.ts`, and the table, then regex-parses structs with a lazy
   dotall regex. Any protocol header edit reruns everything, including the
   render. Cheap in absolute terms (1.25 s in Python).
5. **Shared data.** `projects/*/api/include/*/messaging/*_protocol.hpp`, BFF
   `src/*.ts`, `wire-protocol/src/generated/list_requests.ts`. Struct bodies and
   member lines. The BFF sources are also read by
   `compass.crosscheck/route_inventory`. The protocol headers are generated from
   the models that `compass.crosscheck/protocol_twins` and `subject_conformance`
   read, so the struct shape is already known upstream of the C++.
6. **Recommendation.** Split and replace. Make `stale_table` a files codec (the
   table is an output, with the runtime's `drift` rule and its formatter cache),
   generated from one parse of the protocol structs, or better from the models
   that generate those structs. Replace `missing_member`, `filter_mismatch` and
   `wrong_rows` with generated TS types that `tsc` checks. Effort medium,
   partly ORE. Saving: small runtime, removes a regex C++ parser and a
   duplicated rows rule.

## translation_unit_budget

1. **Intent.** A unit that includes `conventions_family_pack.hpp` names at most
   one convention family (or its recorded ceiling), because reflecting all
   families in one unit broke MSVC. Ceilings only come down.
2. **Does it make sense.** Yes, as a proxy. The real quantity is compile memory
   on MSVC, which only that build measures. Cheap and targeted. Keep.
3. **Python accidents copied.**
   - **One regex per family** (`(?<!…)domain::NAME(?!…)`), each searched over
     every file in scope. The correct form is one pass: find every
     `domain::(\w+)` and count the distinct names in the registry set.
   - Raw text, comments included. A family named in a comment counts toward
     the budget. The correct reading is lexed code. It could only lower counts,
     which would then fire `below_ceiling` (unknown on ORE).
   - The trigger is a substring `"conventions_family_pack.hpp"` anywhere,
     including comments. It should be an include directive.
4. **Cost.** A corpus function over five directories. It reads every file,
   decodes it, and for the few that include the pack runs N regexes. It is
   small (0.25 s in Python).
5. **Shared data.** C++ in five ORE directories, the registry header. Raw text,
   includes, qualified identifiers. Compatible with the shared lexed form
   (include directives plus identifier sequences).
6. **Recommendation.** Simplify: one identifier scan instead of N regexes,
   include directives instead of a substring. Optionally make it a scan.
   Effort small. Saving: negligible. It is cleaner and correct.

## xml_family_census

1. **Intent.** Every root element of ORE's example corpus is a classified
   family. Every classified family has documents. Supported families have
   generated codec tables and others do not. No XML suite tests an unused
   family.
2. **Does it make sense.** Yes. It ties three facts (corpus, census, generated
   tables) that drift independently. Keep.
3. **Python accidents copied.**
   - The root element is the first `<Name` after stripping `<!-- -->` and
     `<? ?>` with regexes. A DOCTYPE or CDATA before the root would mislead it.
     An XML reader that returns the root is exact. No verdict change expected
     on the ORE samples (guess).
   - Unused families are searched with one regex per family over each suite
     file.
4. **Cost.** A corpus function. It reads all 1,623 `external/ore/examples`
   XML files, plus generated tables and suite tests. Those change only on a
   re-vendor, so warm runs skip it. Cold, it reads and regex-strips all the XML.
5. **Shared data.** `external/ore/examples/**/*.xml`: root element.
   **`compass.crosscheck/conventions_coverage` scans the same corpus**
   (`example_files` over `examples/*.xml`). The generated XML tables under
   `ores.ore/core/src/xml/generated` are the output of the unported
   xml-bindings generator.
6. **Recommendation.** Keep. Make the root read a scan with a per-file fact
   (root name), so that a sample added on a re-vendor maps one file. Share that
   map with `conventions_coverage` once the runtime can share a pass. Effort
   small. Saving: cold only, and small.

## core_link_edges

1. **Intent.** No component's core links another component's core or service
   library. It links the api contract.
2. **Does it make sense.** Yes. It is an architecture rule, and the CMake
   `target_link_libraries` calls are its declaration. CMake could enforce it
   at configure time with a wrapper function, but that is slower feedback.
   Keep.
3. **Python accidents copied.**
   - The component set is "every `set(name ores.X.Y)` in any CMakeLists". The
     component layout document already defines the components and parts.
   - The core of a file is the first `set(name …)` in it, and variables are
     resolved from a fixed list of four (`${lib_target_name}` and others).
     Others are reported as `unresolved_link`. That is honest.
   - `#` comments are cut at the first `#` on a line, including a `#` inside a
     quoted argument. That is harmless for library names.
   - `seen` dedupes (core, link) across files, so a second offending line is
     not reported. An inline exception on the first then hides both.
4. **Cost.** A corpus function. It reads every `CMakeLists.txt` under
   `projects/`, twice for the core files (once for text, again for bytes to
   compute offsets). It is cheap (0.2 s in Python).
5. **Shared data.** All `CMakeLists.txt` under `projects/`. Lines without
   comments, `set(name …)`, and `target_link_libraries(…)` words.
   `unity_collisions` uses the same tree for "nearest CMakeLists" and
   `exported_types` for a library's `src/`. A CMake fact per file (name, link
   calls, subdirectories, sources) would serve all three.
6. **Recommendation.** Keep. Take the component set from the component layout,
   and read each file once. Effort small. Saving: negligible.

## test_case_reachability

1. **Intent.** No Catch2 case sits inside `#if/#ifdef/#ifndef`, because a build
   can compile it away and the suite stays green. Conditionals balance, and at
   least one case exists.
2. **Does it make sense.** Yes. Comparing `--list-tests` across platform builds
   would be exact but needs every build. The static rule is the cheap proxy.
   Keep.
3. **Python accidents copied.** Few. Its lexer removes splices, handles raw
   strings with delimiters, character literals, and digit separators.
   `raw_string_moves` is a real edge case it reports rather than guesses. The
   "quote closes within eight characters on one line" test for a char literal
   is a heuristic, but a sound one.
4. **Cost.** A good scan: per-file findings in map and a count as the fact.
   Reduce sums. An edit costs one map.
5. **Shared data.** Test sources under `projects/` (9 suffixes, `tests` dirs or
   `_tests` stems). Spliced, comment- and literal-blanked text, preprocessor
   directives, identifiers.
6. **Recommendation.** Keep as is. **Promote its lexer
   (`blank_comments_and_literals` plus `unsplice`) to the shared C++ lexer**
   for every other codec here. Effort: none for this gate.

---

## Shared passes

### Group A. Every C++ file under `projects/`, lexed

Members: `portable_stop`, `enum_attributes`, `exported_types`,
`forward_declarable_includes`, `unique_type_names`, `unity_collisions`,
`stream_coverage`, `test_case_reachability`, `handler_registrars`,
`translation_unit_budget` (subset), `route_auth` (`ores.http` subset), the
shell codecs (`ores.shell` subset), and `xml_family_census` (suite tests).
Outside this component: `compass.crosscheck/subject_conformance`,
`entity_writes`, `paste_blocks`, `route_inventory` (`ores.http`), and
`compass.sql/handler_permissions` (`*_handler.cpp`).

Each of them reads, decodes and blanks the same bytes with its own rules
(six blankers, see the table at the top). One lexer and one per-file fact
would serve them all, in layers:

| Layer | Content | Consumers |
|---|---|---|
| L1 lexed code | bytes with comments and literals blanked, splices removed, offsets kept; the literal spans kept aside | everything below |
| L2 directives | `#include` targets (quoted or angle), `#define name body`, `#if` stack with case positions | `exported_types`, `forward_declarable_includes`, `enum_attributes` (macros), `test_case_reachability`, `translation_unit_budget`, `portable_stop` (`<stop_token>`) |
| L3 namespace-scope declarations | scope path, name, kind (type definition, function, variable, enum), linkage, decorating macro | `unique_type_names`, `unity_collisions`, `enum_attributes`, `exported_types` (`class MACRO name`), `paste_blocks` |
| L4 tokens of interest | qualified identifier sequences and call names (`name(`) | `portable_stop` (forbidden names), `translation_unit_budget` (`domain::x`), registrars, shell, `route_auth`, `stream_coverage` (callers) |
| L5 string constants | `name = "literal"` with scope, `nats_subject` members, braced arrays | `stream_coverage`, `subject_conformance`, `handler_registrars` (`_request::nats_subject`) |

**How to share it.** There are two options, and the second is the target:

1. Merge codecs where the rules belong together and the rule names can move:
   - `enum_attributes` with `exported_types` ("windows_exports");
   - the three registrars;
   - the three shell codecs;
   - `unique_type_names` with `unity_collisions` ("odr_names").

   This needs no runtime change.
2. Add a runtime "file model" step, which is the task
   [Combine gates that read the same data] (F51A1EEB) owns. A versioned
   producer maps a file's bytes to L1 to L5 once per content ID and stores
   the result in the record. Each scan's map receives the model instead of
   the bytes. A cold run then lexes 200 MB once instead of eight to eleven
   times. A warm edit lexes the edited file once for all scans.

**Saving (guess).** Cold: the brief puts a slow scan at 2 to 3 s each, and
about seven of the passes are redundant, so roughly 10 to 15 s of CPU across
threads. That is perhaps 2 to 4 s of wall time on 8 cores (guess). Warm: a
little per edit, since each scan already maps one file. Most of the warm
saving comes from the three map/reduce fixes in group A2 below, not from
sharing.

**A2. Fix the reduces that do per-file work** (independent of sharing):

- `forward_declarable_includes`: the verdict goes in map, and the list becomes
  an option. About 530 ms per header edit.
- `stream_coverage`: map extracts the declarations, and the fact stops being
  the whole file. About 640 ms per header edit, and tens of MB out of the scan
  table.
- `exported_types`: stop depending on `list_files("")`, and do per-library
  walks. Up to 300 ms on edits that change includes.

These three are the whole of the header-edit cost the brief lists.

**Generated code lever.** 88% of the C++ bytes are generated. Checks whose
fix is the template (forward declarations already report per template) could
map a generated file only when its template or its pasted model blocks change.
That is cold-only and not free of risk, so it comes after the shared lexer.

### Group B. CMake

`core_link_edges` (all `CMakeLists.txt`), `unity_collisions` (nearest
CMakeLists, `cmake/unity_exclusions.cmake`), `exported_types` (a library's
`src/`). One CMake fact per file (target name, links, sources) would serve
them, and the component layout document could replace the "nearest
CMakeLists" and "any `set(name)`" guesses. The saving is small. The gain is
correctness.

### Group C. `ores.http` sources and BFF TypeScript

`route_auth` and `compass.crosscheck/route_inventory` both find the routes in
`projects/ores.http`. `bff_list_requests` and `route_inventory` both read BFF
`src/*.ts`. Fold `route_auth` into `route_inventory`'s read, or drop it (see
its section).

### Group D. Protocol and subjects

`bff_list_requests` (`*_protocol.hpp` structs), `stream_coverage` and
`handler_registrars` (`nats_subject`), and `compass.crosscheck/subject_conformance`
and `protocol_twins` (the models these headers are generated from). The models
are the source. Checks that read the generated C++ to recover what the model
said should read the model fact instead.

### Group E. ORE example XML

`xml_family_census` and `compass.crosscheck/conventions_coverage` both scan
`external/ore/examples/**/*.xml`. They could share one per-file root and
document fact. Cold only.

### Group F. Generator gaps posing as gates

`event_registrars`, `history_registrars`, `handler_registrars`,
`shell_command_composition`: each checks that generated code was composed by
hand. Generating the composition removes all four. `bff_list_requests`'
`stale_table` is a generator drift check and belongs in a files codec.
`route_auth` is a runtime invariant ORE already enforces and could make a
compile-time one.
