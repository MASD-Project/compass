/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Marco Craveiro <marco.craveiro@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "compass.docs/codecs/skill_tables.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/glob.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.docs/ops/regions.hpp"
#include "compass.docs/ops/skills.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::docs::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

using ops::skill_names;
using ops::skills_dir;

struct domain {
    std::string_view prefix;
    std::string_view heading;
    std::string_view blurb;
};

constexpr std::array<domain, 10> domains{{
    {"agile", "Agile (=compass-agile-=)",
        "Stories, tasks, sprints, backlog, journal, orientation."},
    {"pr", "Pull requests (=compass-pr-=)", "PR lifecycle."},
    {"doc", "Documentation (=compass-doc-=)",
        "Recipes, knowledge, manual, memory, diagrams, search."},
    {"code", "Hand-written code (=compass-code-=)",
        "Build, tests, review, investigation \xe2\x80\x94 code written by "
        "hand."},
    {"codegen", "Generated code (=compass-codegen-=)",
        "Entities, components, and schemas produced by ores.codegen."},
    {"devops", "DevOps (=compass-devops-=)",
        "Environment, database, services, shell, client, site."},
    {"skill", "Skills (=compass-skill-=)", "The skills themselves."},
    {"principle", "Principles (=compass-principle-=)",
        "Local bindings on an upstream principle, supplying the project "
        "detail the rule cannot know."},
    {"method", "Methods (=compass-method-=)",
        "How a kind of work is approached, from framing to a verified end."},
    {"helm", "The mode (=compass-helm=)",
        "The sticky posture a session enters, which matches work to a "
        "method and routes to everything else."}}};

constexpr std::string_view catalogue_path =
    "doc/llm/skills/claude_code_skills.org";
constexpr std::string_view catalogue_begin = "# BEGIN generated catalogue "
    "(build/scripts/generate_skills_catalogue.py)";
constexpr std::string_view catalogue_end = "# END generated catalogue";
constexpr std::size_t max_description = 110;

/**
 * @brief The ID, title and description of a skill, or nothing when its
 * SKILL.org lacks one, which stops the script.
 */
std::optional<std::array<std::string, 3>> skill_meta(
    const ct::corpus_view& v, const std::string& name) {
    static const pt::regex id(R"(^:ID: (\S+))", {.multiline = true});
    static const pt::regex title(R"(^#\+title: (.*)$)", {.multiline = true});
    static const pt::regex description(R"(^description: (.*)$)",
        {.multiline = true});
    const auto text = v.read_file(std::string(skills_dir) + name +
        "/SKILL.org").value_or("");
    const auto i = id.search(text);
    const auto t = title.search(text);
    const auto d = description.search(text);
    if (!i || !t || !d)
        return std::nullopt;
    std::string desc(co::strip(d->group(1)));
    std::ranges::replace(desc, '|', '/');
    if (co::characters(desc) > max_description) {
        desc.resize(co::byte_of_character(desc, max_description - 3));
        desc = std::string(co::strip(desc)) + "...";
    }
    return std::array{std::string(co::strip(i->group(1))),
        std::string(co::strip(t->group(1))), desc};
}

void catalogue(const ct::corpus_view& v, ct::file_output& out) {
    const auto text = v.read_file(catalogue_path);
    if (!text) {
        co::refuse(out, catalogue_path, "the page does not exist",
            "Restore the page with its generated region.");
        return;
    }
    std::map<std::string_view, std::vector<std::string>> groups;
    const auto names = skill_names(v);
    for (const auto& name : names) {
        std::string_view rest = name;
        if (rest.starts_with("compass-"))
            rest.remove_prefix(std::string_view("compass-").size());
        groups[rest.substr(0, rest.find('-'))].push_back(name);
    }
    std::vector<std::string> parts{std::string(catalogue_begin), ""};
    for (const auto& d : domains) {
        parts.push_back("* " + std::string(d.heading));
        parts.emplace_back();
        parts.emplace_back(d.blurb);
        parts.emplace_back();
        const auto& members = groups[d.prefix];
        if (members.empty())
            continue;
        parts.emplace_back("| Name | Title | Description |");
        parts.emplace_back("|------+-------+-------------|");
        for (const auto& name : members) {
            const auto meta = skill_meta(v, name);
            if (!meta) {
                co::refuse(out, catalogue_path, "the skill " + name +
                    " has no :ID:, #+title: or description: line",
                    "Give the skill's SKILL.org the line it lacks.");
                return;
            }
            parts.push_back("| [[id:" + (*meta)[0] + "][" + name + "]] | " +
                (*meta)[1] + " | " + (*meta)[2] + " |");
        }
        parts.emplace_back();
    }
    parts.emplace_back(catalogue_end);
    if (const auto fresh = ops::replace_region(*text, catalogue_begin,
            catalogue_end, ops::join_lines(parts), ops::replace::first))
        out.files.add(catalogue_path, *fresh);
    else
        co::refuse(out, catalogue_path, "the page has no generated catalogue "
            "region", "Restore its BEGIN and END generated catalogue "
            "markers.");
}

constexpr std::string_view tables_script =
    "build/scripts/generate_skill_type_tables.py";
constexpr std::string_view templates_dir =
    "projects/ores.codegen/library/templates/";
constexpr std::string_view compass_py = "projects/ores.compass/src/compass.py";

constexpr std::array<std::pair<std::string_view, std::string_view>, 7>
    model_types{{
        {"entity_org", "projects/ores.<component>/modeling/"},
        {"field_group", "projects/ores.<component>/modeling/"},
        {"dataset", "projects/ores.seeder/datasets/<name>/"},
        {"profile", "projects/modeling/"},
        {"feature", "projects/modeling/"},
        {"facet", "projects/ores.codegen/library/templates"},
        {"facet_group", "projects/ores.codegen/library/templates"}}};

constexpr std::array<std::string_view, 13> read_only{"show", "search",
    "list", "where", "fleet", "journal", "timeline", "heading", "bearings",
    "inbox", "next", "deferred", "discarded"};

constexpr std::array<std::pair<std::string_view, std::string_view>, 10>
    read_only_sub{{
        {"sprint status", "the sprint's stories grouped by state"},
        {"sprint audit", "drift between story files and the sprint table"},
        {"story status", "one story's tasks and their states"},
        {"services status",
            "which services run, which stopped or went missing"},
        {"pr checks", "CI state on a pull request"},
        {"test results", "the last ctest run"},
        {"review list", "the review comments on a pull request"},
        {"env list", "the provisioned worktrees"},
        {"db sql", "whatever the query returns"},
        {"skills report", "skill-selection quality over a window"}}};

/**
 * @brief dict(re.findall(@p pair, the body of @p table in @p source)):
 * later keys win.
 */
std::map<std::string, std::string> python_dict(std::string_view source,
    std::string_view table, const pt::regex& pair) {
    std::map<std::string, std::string> out;
    const pt::regex body(pt::escape(table) + R"( = \{(.*?)\})",
        {.dotall = true});
    const auto m = body.search(source);
    if (!m)
        return out;
    for (const auto& p : pair.find_all(m->group(1)))
        out.insert_or_assign(std::string(p.group(1)), std::string(p.group(2)));
    return out;
}

std::string add_table(const ct::corpus_view& v, std::string_view compass) {
    static const pt::regex static_pair(
        R"re(^\s*"([a-z_]+)":\s*"([^"]+)",\s*$)re", {.multiline = true});
    static const pt::regex default_pair(R"re("([a-z_]+)":\s*"([a-z_]+)")re");
    static const pt::regex heading(R"(^\* (.+?)\s*$)", {.multiline = true});
    static const pt::regex tags(R"(\s{2,}:[\w:]+:$)");
    const auto static_parent = python_dict(compass, "_STATIC_PARENT",
        static_pair);
    const auto defaultable = python_dict(compass, "_DEFAULTABLE_PARENT",
        default_pair);
    std::vector<std::string> lines{std::string("# BEGIN generated types (") +
        std::string(tables_script) + ")", "",
        "One row per document type. The scaffold command, where it lands, "
        "and the sections the template leaves for you to fill.", "",
        "#+ATTR_HTML: :class hug-leading",
        "| Type | Scaffold | Lands in | Sections to fill |",
        "|------+----------+----------+------------------|"};
    for (const auto path : co::glob(v, std::string(templates_dir) +
             "doc_*.org.mustache")) {
        const auto file = path.substr(templates_dir.size());
        const std::string name(file.substr(4, file.size() - 4 -
            std::string_view(".org.mustache").size()));
        const auto text = v.read_file(path).value_or("");
        std::vector<std::string> sections;
        for (const auto& m : heading.find_all(text)) {
            const auto s = co::strip(m.group(1));
            if (s.find(":noexport:") == std::string_view::npos)
                sections.push_back(tags.substitute(s, ""));
        }
        std::string home;
        if (const auto t = std::ranges::find(model_types, name,
                &std::pair<std::string_view, std::string_view>::first);
            t != model_types.end())
            home = t->second;
        else if (static_parent.contains(name))
            home = static_parent.at(name);
        else if (defaultable.contains(name))
            home = "the current " + defaultable.at(name);
        else
            home = "--parent-dir";
        std::string filled;
        for (const auto& s : sections)
            filled += (filled.empty() ? "=" : ", =") + s + "=";
        lines.push_back("| =" + name + "= | =compass add " + name +
            "= | =" + home + "= | " +
            (filled.empty() ? std::string("\xe2\x80\x94") : filled) + " |");
    }
    lines.emplace_back();
    lines.emplace_back("# END generated types");
    return ops::join_lines(lines);
}

std::string show_table(std::string_view compass) {
    static const pt::regex parser(
        R"re(subparsers\.add_parser\(\s*"([a-z-]+)"\s*,(.*?)\)\s*\n)re",
        {.dotall = true});
    static const pt::regex help(R"re(help=((?:\s*"(?:[^"\\]|\\.)*")+))re");
    static const pt::regex quoted(R"re("((?:[^"\\]|\\.)*)")re");
    std::map<std::string, std::string> helps;
    for (const auto& m : parser.find_all(compass)) {
        const auto h = help.search(m.group(2));
        if (!h)
            continue;
        std::string joined;
        for (const auto& q : quoted.find_all(h->group(1)))
            joined += (joined.empty() ? "" : " ") +
                std::string(co::strip(q.group(1)));
        helps.insert_or_assign(std::string(m.group(1)), joined);
    }
    std::vector<std::string> lines{std::string("# BEGIN generated targets (") +
        std::string(tables_script) + ")", "",
        "One row per read-only command. Each shows state and changes "
        "nothing, so any of them is safe to run while deciding what to do "
        "next.", "", "#+ATTR_HTML: :class hug-leading",
        "| Command | Shows |", "|---------+-------|"};
    for (const auto name : read_only) {
        const auto found = helps.find(std::string(name));
        std::string_view blurb = found == helps.end()
            ? std::string_view{} : std::string_view(found->second);
        blurb = blurb.substr(0, blurb.find(';'));
        if (const auto dash = blurb.rfind("\xe2\x80\x94");
            dash != std::string_view::npos)
            blurb = blurb.substr(dash + 3);
        blurb = co::strip(blurb);
        while (blurb.ends_with('.'))
            blurb.remove_suffix(1);
        lines.push_back("| =compass " + std::string(name) + "= | " +
            std::string(blurb.empty() ? name : blurb) + " |");
    }
    for (const auto& [sub, blurb] : read_only_sub)
        lines.push_back("| =compass " + std::string(sub) + "= | " +
            std::string(blurb) + " |");
    lines.emplace_back();
    lines.emplace_back("# END generated targets");
    return ops::join_lines(lines);
}

void type_tables(const ct::corpus_view& v, ct::file_output& out) {
    const auto add_skill = std::string(skills_dir) +
        "compass-doc-add/SKILL.org";
    const auto compass = v.read_file(compass_py);
    if (!compass) {
        co::refuse(out, add_skill, "cannot read " + std::string(compass_py),
            "Restore the file, or move the generator to where it went.");
        return;
    }
    struct target {
        std::string path;
        std::string begin;
        std::string_view end;
        std::string block;
    };
    const target targets[] = {
        {add_skill,
            "# BEGIN generated types (" + std::string(tables_script) + ")",
            "# END generated types", add_table(v, *compass)},
        {std::string(skills_dir) + "compass-doc-show/SKILL.org",
            "# BEGIN generated targets (" + std::string(tables_script) + ")",
            "# END generated targets", show_table(*compass)}};
    for (const auto& t : targets) {
        const auto text = v.read_file(t.path);
        if (!text) {
            co::refuse(out, t.path, "the skill page does not exist",
                "Restore the skill with its generated region.");
            return;
        }
        if (const auto fresh = ops::replace_region(*text, t.begin, t.end,
                t.block, ops::replace::every))
            out.files.add(t.path, *fresh);
        else
            co::refuse(out, t.path, "the page has no region " + t.begin,
                "Restore the region's BEGIN and END markers.");
    }
}

struct recipe_link {
    std::string_view recipe;
    std::string_view why;
};

struct skill_owner {
    std::string_view skill;
    std::array<recipe_link, 1> recipes;
};

constexpr std::array<skill_owner, 16> owners{{
    {"compass-devops-provision-environment",
        {{{"compass/how_do_i_provision_an_environment_with_compass",
            "creates the worktree, assigns ports, writes the .env"}}}},
    {"compass-devops-deprovision-environment",
        {{{"compass/how_do_i_deprovision_an_environment_with_compass",
            "stops the services and removes the worktree"}}}},
    {"compass-devops-run-shell",
        {{{"compass/how_do_i_run_the_ores_shell",
            "interactive and scripted invocations"}}}},
    {"compass-devops-run-sql",
        {{{"sql/how_do_i_run_sql_with_compass",
            "credentials and connection come from the .env"}}}},
    {"compass-devops-recreate-db",
        {{{"cmake/how_do_i_start_services",
            "the database rebuild and the service restart around it"}}}},
    {"compass-devops-start-services",
        {{{"cmake/how_do_i_start_services",
            "starting the fleet and verifying it"}}}},
    {"compass-devops-stop-services",
        {{{"compass/how_do_i_manage_the_environment_with_compass",
            "the environment lifecycle verbs"}}}},
    {"compass-devops-deploy-manual",
        {{{"cmake/how_do_i_build_the_system", "the deploy_manual target"}}}},
    {"compass-devops-deploy-site",
        {{{"cmake/how_do_i_deploy_the_site",
            "building and publishing the site"}}}},
    {"compass-devops-deploy-settings",
        {{{"cmake/how_do_i_deploy_the_settings",
            "tangling settings.json from its org source"}}}},
    {"compass-agile-close-task",
        {{{"agile/how_do_i_work_a_task",
            "the state transitions and the story and sprint sync"}}}},
    {"compass-code-start-branch",
        {{{"git/how_do_i_start_a_new_feature_branch_phase",
            "branching off a fresh main"}}}},
    {"compass-agile-review-sprint",
        {{{"agile/how_do_i_get_a_sprint_or_story_status",
            "the status and audit commands the review reads"}}}},
    {"compass-doc-add-component-model",
        {{{"codegen/how_do_i_create_a_component_overview",
            "scaffolding and filling the overview"}}}},
    {"compass-codegen-add-component",
        {{{"codegen/how_do_i_create_a_component_overview",
            "the overview every new component needs"}}}},
    {"compass-pr-sync-branch",
        {{{"git/how_do_i_rebase_my_branch_on_main",
            "the rebase and what to do when it conflicts"}}}}}};

/**
 * @brief The link line for @p link, or nothing when the recipe or its ID
 * is missing, which stops the script.
 */
std::optional<std::pair<std::string, std::string>> recipe_line(
    const ct::corpus_view& v, const recipe_link& link) {
    static const pt::regex id(R"(^:ID:\s*(\S+)\s*$)", {.multiline = true});
    static const pt::regex title(R"(^#\+title:\s*(.+?)\s*$)",
        {.multiline = true});
    const auto text = v.read_file("doc/recipes/" + std::string(link.recipe) +
        ".org");
    if (!text)
        return std::nullopt;
    const auto i = id.search(*text);
    if (!i)
        return std::nullopt;
    const auto t = title.search(*text);
    const std::string identifier(i->group(1));
    return std::pair{identifier, "- [[id:" + identifier + "][" +
        std::string(t ? t->group(1) : link.recipe) + "]] \xe2\x80\x94 " +
        std::string(link.why) + "."};
}

void recipes(const ct::corpus_view& v, ct::file_output& out) {
    static const pt::regex section(R"(^\* Recipes\s*$(.*?)(?=^\* ))",
        {.dotall = true, .multiline = true});
    for (const auto& owner : owners) {
        const auto path = std::string(skills_dir) + std::string(owner.skill) +
            "/SKILL.org";
        auto text = v.read_file(path);
        if (!text)
            continue;
        auto m = section.search(*text);
        if (!m) {
            constexpr std::string_view anchor = "* Artefacts";
            constexpr std::string_view block = "* Recipes\n\n* Reference\n\n";
            if (const auto at = text->find(anchor); at != std::string::npos) {
                text->insert(at, block);
            } else {
                while (text->ends_with('\n'))
                    text->pop_back();
                *text += "\n\n" + std::string(block);
            }
            m = section.search(*text);
        }
        const std::string body(m->group(1));
        const auto start = m->start(1);
        const auto end = m->end(1);
        std::string added;
        bool recipes_found = true;
        for (const auto& link : owner.recipes) {
            const auto line = recipe_line(v, link);
            if (!line) {
                recipes_found = false;
                break;
            }
            if (body.find("id:" + line->first) == std::string::npos)
                added += (added.empty() ? "" : "\n") + line->second;
        }
        if (!recipes_found)
            continue;
        if (added.empty()) {
            out.files.add(path, *text);
            continue;
        }
        auto existing = std::string_view(body);
        while (existing.starts_with('\n'))
            existing.remove_prefix(1);
        while (existing.ends_with('\n'))
            existing.remove_suffix(1);
        const auto fresh_body = existing.empty()
            ? "\n\n" + added + "\n\n"
            : "\n\n" + std::string(existing) + "\n" + added + "\n\n";
        out.files.add(path, text->substr(0, start) + fresh_body + text->substr(end));
    }
}

}

const ct::descriptor skill_catalogue{
    .name = "skill_catalogue",
    .version = 1,
    .summary = "Writes the generated catalogue of the skills.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = co::refusal_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{catalogue}};

const ct::descriptor skill_type_tables{
    .name = "skill_type_tables",
    .version = 1,
    .summary = "Writes the document type and read-only command tables of "
        "the polymorphic skills.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = co::refusal_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{type_tables}};

const ct::descriptor skill_recipes{
    .name = "skill_recipes",
    .version = 1,
    .summary = "Writes each skill with the links to the recipes that own "
        "its commands.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = {},
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{recipes}};

}
