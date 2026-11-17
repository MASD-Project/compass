# Gate review: combined plan

Sources: cpp.md, sql_codegen_codec.md, crosscheck.md and docs.md in this
directory. All four were read-only reviews of every ported gate. Their
timings are estimates unless marked measured.

## 1. Correctness defects (fix first, whatever else is decided)

- **Silent passes.** When some generators cannot produce their output, they
  write nothing, so the gate passes where the Python failed. There are 14:
  - crosscheck: curve_sections_wiring, curve_column_guards,
    convention_kind_terms, ore_renderings_tables, script_library_seed,
    ore_output_catalogue, er_diagram, conventions_dataset, route_inventory;
  - docs: skill_catalogue, skill_type_tables, skill_levels,
    recipe_inventories, trade_type_coverage.

  pending_models swallows refusals and writes a partial table instead. The
  fix is that a refusal becomes a finding, as parameter_value_sets already
  does. This needs the runtime to let one unit emit both files and findings,
  or a diagnostics twin for each generator. One untested risk: after a good
  run, a refusal might make the sink delete the page.
- **The licence year comes from the clock.** Every January, all 26,212
  codegen outputs and 202 component lists drift with no change to their
  inputs. The year is not an input of the unit, so a warm run and a cold run
  give different bytes.
- **Copied Python bugs:**
  - subject_conformance: its C++ pattern opens on `"` and closes on `'`, so it
    matches nothing on ORE (measured).
  - exported_types: counts includes inside comments and `#if 0` blocks.
  - registrars: count commented-out calls.
  - enum_attributes: `1'000` opens a char literal.
  - shell_menus: misses `Insert("x"` when no newline follows it.
  - no_pr_triggers: misses `on: [push, pull_request]` and
    `pull_request_target`.
  - lint: treats 32 `:ID:` lines inside blocks as page IDs on ORE.
  - unity_collisions: knows raw strings only when they have no delimiter.
  - required_sections: accepts a required section at any heading level.

## 2. Gates to drop or reduce

- **No conditions:**
  - paste_blocks, about 2.5 s cold. Codegen drift and the compiler cover
    both failures it looks for.
  - the C++ half of subject_conformance, which never matches.
  - model_sql_types for the 1,307 generated create scripts, which drift
    covers.
  - two model_drift rules that the model loader already throws.
  - skill_recipes and skill_level_tags.
  - component_docs/missing_section, which is the same check as
    required_sections.
  - prototypes' react_tree and no_discovery, which guard a finished
    migration.
- **Once ORE changes something:**
  - route_auth, once ORE has a full-router test or a typestate builder.
  - the three registrar codecs and shell_command_composition, once codegen
    emits the composition itself.
  - shell_recipe_provenance, once the generated library owns its directory.
  - er_diagram, if ORE generates the diagrams at site build instead of
    committing them.
  - the recipe libraries' drift check, if ORE stops committing 5,429
    generated files.

## 3. Shared passes (the "combine gates" task)

- **C++.** 200 MB (88% generated) is lexed by up to 11 scans cold, using six
  different comment blankers. Replace them with one lexer and a per-file
  facts step that every C++ gate reads. Merge exported_types with
  enum_attributes. Make forward_declarable_includes decide in map (530 ms
  per header edit), and have stream_coverage keep declarations rather than
  whole files (640 ms per header edit, tens of MB).
- **SQL.** Four or five regex parsers re-read `create/`, 11 MB, and
  populate_refs also reads `populate/`, 56 MB, on any SQL edit. Replace them
  with one lexer (sql_hygiene's) and one table and call model, seeded from
  `er_model_of`. That turns populate_refs into a scan, which saves about
  400 ms per SQL edit, and adds the define-before-use order check it was
  meant to have. rls_widenings becomes a rule.
- **Models.** About ten crosscheck gates regex raw org bytes instead of
  using the loaded models. Replace them with one per-model rules codec over
  the models compass already loads, run incrementally. This also removes
  three floor rules that exist only because regexes can go blind. Move
  entity_binary_types, generated_column_refs and part of protocol_twins to
  rules over the model or the codegen context.
- **Org.** Make one per-page codec over the parsed tables: heading_numbers,
  org_links, the backtick rule and lint's per-page rules. Add one ID-link
  graph pass with a reduce for lint, pattern_uses and structure_notes, which
  becomes a missing_backlink check. Today they build three ID indexes and
  copy up to 70 MB each. The reason given for keeping regexes does not
  hold: org-element handles indented and upper-case blocks.
- **Repeated derivations.** The curve wiring is computed four times per cold
  run, so merge curve_sections_wiring with curve_column_guards. The
  convention terms are derived three times, so fold convention_kind_terms
  into ore_value_sets. Share one XML pass between xml_family_census and
  conventions_coverage. Fold the routes gates (route_units, route_inventory,
  http_areas) into one routes codec.
- **Tangle.** tangle and tangle_check run over all of about 11,000 documents
  to produce 226 templates. Scope them by path, or merge them once one unit
  can emit both files and findings.

## 4. ORE policy compiled into compass

Nine codecs copy tables from ORE:

- 76 skill levels
- 38 rule-section codes
- 16 skill-to-recipe links
- 4 backlog page heads
- one story path with three task UUIDs
- money column lists

ORE can't change any of these without a compass release. Move each into the
org page it describes, and have compass read it there.

## 5. Proposed order

1. Correctness: refusal findings, the licence year, pending_models, and the
   copied bugs. Small to medium.
2. Drops with no conditions. Small.
3. Shared passes, in this order:
   - SQL model and lexer (largest edit-hook win);
   - C++ lexer and facts (largest cold win);
   - org parsed-table and link-graph passes;
   - per-model rules codec.
4. ORE policy tables into ORE pages (needs ORE-side changes).
5. Conditional drops, as ORE makes its changes.
