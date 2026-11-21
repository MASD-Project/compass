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
#include "compass.cli/app/codecs.hpp"

#include <algorithm>
#include <array>
#include <vector>
#include "compass.cli/app/check_inventory.hpp"
#include "compass.cli/ops/provenance.hpp"
#include "compass.codec/ops/data_tables.hpp"
#include "compass.codec/codecs/catalogue.hpp"
#include "compass.codec/ops/rule_table.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codegen/codecs/catalogue.hpp"
#include "compass.cpp/codecs/catalogue.hpp"
#include "compass.crosscheck/codecs/catalogue.hpp"
#include "compass.docs/codecs/catalogue.hpp"
#include "compass.env/codecs/catalogue.hpp"
#include "compass.sql/codecs/catalogue.hpp"

namespace compass::cli::app {

namespace {

void rule_table(const codec::types::corpus_view& v,
    codec::types::file_output& out) {
    out.files.add(v.option("path"), codec::ops::rule_table(codecs(), v));
}

void check_inventory(const codec::types::corpus_view& v,
    codec::types::file_output& out) {
    out.files.add(v.option("path"), inventory_page(v));
}

/**
 * @brief Reports the check document of commit_provenance when its tables
 * or its options do not read, so a plain check finds what check --ratchet
 * would skip. The commits themselves are checked by check --ratchet.
 */
void commit_provenance(const codec::types::corpus_view& v,
    codec::types::diagnostic_table& out) {
    const auto data = v.option(codec::ops::data_option);
    const auto rules = ops::provenance_rules_of(data,
        v.option("require_pull_request"));
    if (rules)
        return;
    const auto source = codec::ops::decode_data_tables(data);
    const auto list = ops::provenance_rule_list();
    const auto unreadable = std::ranges::find(list, "unreadable",
        &codec::types::rule::name) - list.begin();
    out.add({.document = org::types::document_id::none(),
        .range = {},
        .rule = codec::types::rule_id{static_cast<std::uint32_t>(unreadable)},
        .level = codec::types::severity::error,
        .message = rules.error(),
        .fix = "give the check document the tables and the options the "
               "message names",
        .file = source && !source->source.empty()
            ? std::string_view(source->source)
            : std::string_view("compass/checks/commit_provenance.org")});
}

constexpr std::array<codec::types::option, 3> provenance_options{{
    {codec::ops::data_option, "", "The tables of the check document whose "
        "Definition names commit_provenance with :CODEC:: Trailers, one row "
        "per trailer with its document type or pattern and its pull request "
        "row, and Exemptions, optional, the subjects whose commits may omit "
        "a trailer. compass sets it; not a setting."},
    {"require_pull_request", "false", "true makes a branch with no pull "
        "request gh can read an error, for CI; false only says so."},
    {"gh", "gh", "The program that reads the branch's pull request, run as "
        "gh pr view <branch> --json number,body,url; empty reads none."}}};

constexpr std::array<codec::types::option, 9> inventory_options{{
    {"path", "doc/knowledge/compass/check_inventory.org", "The page, "
        "relative to the output root: a knowledge page, so under "
        "doc/knowledge/ by the knowledge contract, and never under "
        "compass/checks/, whose every page is read as a check document."},
    {"id", "", "The page's :ID:, a fixed uppercase UUID, so links to it "
        "survive regeneration; no drawer when empty."},
    {"title", "Check inventory", "The page's #+title:."},
    {"type", "knowledge", "The page's #+type:."},
    {"level", "cross", "The page's #+level:."},
    {"filetags", ":compass:checks:inventory:", "The page's #+filetags:."},
    {"created", "", "The page's #+created:, a fixed date; none when empty."},
    {"updated", "", "The page's #+updated:, a fixed date the project moves "
        "when it wants to; none when empty."},
    {inventory_option, "", "The codecs and checks of the run, which compass "
        "sets from the configuration and the check documents; not a "
        "setting."}}};

}

std::span<const codec::types::descriptor> codecs() noexcept {
    static const auto all = [] {
        const auto shipped = codec::codecs::catalogue();
        std::vector<codec::types::descriptor> out(shipped.begin(),
            shipped.end());
        for (const auto& d : codegen::codecs::catalogue())
            out.push_back(d);
        for (const auto& d : cpp::codecs::catalogue())
            out.push_back(d);
        for (const auto& d : sql::codecs::catalogue())
            out.push_back(d);
        for (const auto& d : crosscheck::codecs::catalogue())
            out.push_back(d);
        for (const auto& d : docs::codecs::catalogue())
            out.push_back(d);
        for (const auto& d : env::codecs::catalogue())
            out.push_back(d);
        out.push_back({
            .name = ops::provenance_codec,
            .version = 1,
            .code = code_hash(),
            .summary = "Checks that every commit of a branch, and the "
                       "branch's pull request, state the work they belong "
                       "to, as the project's check document asks; check "
                       "--ratchet checks the commits.",
            .reads = codec::types::inputs::of_types({}),
            .options = provenance_options,
            .rules = ops::provenance_rule_list(),
            .enabled_by_default = false,
            .run = codec::types::corpus_function<
                codec::types::diagnostic_table>{commit_provenance}});
        out.push_back({
            .name = "check_inventory",
            .version = 1,
            .code = code_hash(),
            .summary = "Writes the page of every codec and check that runs "
                       "here: whether it is on, its purpose, its rules and "
                       "where it is defined.",
            .reads = codec::types::inputs::of_types({}),
            .options = inventory_options,
            .rules = {},
            .enabled_by_default = false,
            .run = codec::types::corpus_function<codec::types::file_output>{
                check_inventory}});
        out.push_back({
            .name = "rule_table",
            .version = codec::ops::rule_table_version(out),
            .code = code_hash(),
            .summary = "Writes the page of every rule, the memories and "
                       "documents it cites, and the memories no rule cites.",
            .reads = codec::types::inputs::every_typed_document(),
            .options = codec::ops::rule_table_options,
            .rules = {},
            .enabled_by_default = false,
            .run = codec::types::corpus_function<codec::types::file_output>{
                rule_table}});
        return out;
    }();
    return all;
}

}
