# compass-ng — Claude Code Instructions

Read `doc/compass.org` before acting. It is the entry point to the
documentation tree.

- **Document contract.** Documents are org-mode and follow the ORE Studio
  contract verbatim (`~/Development/OreStudio/doc/meta/`). Copy the shape of
  the ORE Studio template for each type: `:ID:` drawer, `#+title:`,
  `#+description:`, `#+type:`, `#+level:`, `#+filetags:`, `#+created:`,
  `#+updated:`, `#+todo:`, the intro sentence and the `* Status` table.
- **Work is documents.** Track work as stories and tasks under
  `doc/agile/versions/`. When work moves, update the state in the document's
  `* Status` table and in its parent's table.
- **Upstream conventions.** ORE Studio (`~/Development/OreStudio`) is the
  reference for project structure, MASD vocabulary (component, part, facet,
  archetype) and the document contract. Where this repository is silent,
  follow ORE Studio.
- **Component layout** is defined in
  `projects/modeling/component_architecture.org`. Tooling must agree with it.
- **Pull requests only.** `main` is protected. Every change goes on a
  `feature/` branch and lands through a pull request.
- **Memory rule:** all project memories live in `doc/llm/memory/` and must be
  created with `./compass.sh add memory`, then listed in the catalogue of
  `doc/llm/memory/memory.org`. Never write to the Claude Code harness memory
  (`~/.claude/projects/*/memory/`); it is disabled via `autoMemoryEnabled: false`
  in `.claude/settings.json`.
- **Code comments:** use sparingly, place above the code, never end-of-line,
  never narrate edit history, no commented-out code.
