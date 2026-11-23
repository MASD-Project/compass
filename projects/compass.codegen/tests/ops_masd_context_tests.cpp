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
#include "compass.codegen/ops/masd_context.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.codegen/ops/compile.hpp"
#include "compass.codegen/ops/context.hpp"
#include "compass.codegen/ops/output.hpp"

namespace {

namespace mtypes = compass::model::types;
using namespace compass::codegen;
using Catch::Matchers::ContainsSubstring;

mtypes::masd_column column(std::string name, std::string type) {
    mtypes::masd_column c;
    c.name = name;
    c.member = std::move(name);
    c.sql_type = std::move(type);
    return c;
}

mtypes::masd_entity table(std::string schema, std::string entity) {
    mtypes::masd_entity e;
    e.product = "compass";
    e.component = "db";
    e.schema = std::move(schema);
    e.row_type = entity + "_row";
    e.entity = std::move(entity);
    return e;
}

/**
 * @brief The template of the MASD library that compass ships, as its
 * archetype page tangles it.
 */
std::string library_template(const std::string& name) {
    std::ifstream is(std::string(COMPASS_MASD_TEMPLATES_DIR) + "/" + name,
        std::ios::binary);
    REQUIRE(is);
    return {std::istreambuf_iterator<char>(is), {}};
}

/**
 * @brief @p name rendered from the context @p make builds.
 */
template <typename Make>
std::string rendered(const std::string& name, Make make) {
    mtypes::model_tables tables;
    compass::model::ops::builder b(tables);
    const auto library = ops::library_values(b, "Licence.", "", 2026);
    const auto context = make(b, library);
    std::string out;
    ops::render_output(out, name, ops::compile(library_template(name)),
        tables, context);
    return out;
}

/**
 * @brief A run of a campaign: an identity key, a now() default the
 * database writes, a nullable value list and a foreign key.
 */
mtypes::masd_entity run() {
    auto e = table("activity", "run");
    e.retained = true;
    auto id = column("id", "bigint");
    id.primary_key = true;
    id.key = mtypes::key_kind::identity;
    auto campaign = column("campaign", "bigint");
    auto started = column("started_at", "timestamptz");
    started.default_value = "now()";
    started.writer = mtypes::written_by::database;
    auto outcome = column("outcome", "text");
    outcome.nullable = true;
    outcome.values = {"passed", "failed"};
    outcome.enum_name = "run_outcome";
    e.columns = {id, campaign, started, outcome};
    e.foreign_keys = {{"campaign", "activity.campaign", false}};
    e.indexes = {{"campaign", {"campaign"}, false, "", "", ""}};
    return e;
}

mtypes::masd_entity campaign() {
    auto e = table("activity", "campaign");
    auto id = column("id", "bigint");
    id.primary_key = true;
    id.key = mtypes::key_kind::identity;
    e.columns = {id, column("story_id", "text"), column("environment", "text")};
    e.uniques = {{"environment", "story_id"}};
    return e;
}

/**
 * @brief A heading of a document: an owner's hash and an ord as its key,
 * a member named apart from its column, and a value list with a default.
 */
mtypes::masd_entity heading() {
    auto e = table("corpus", "heading");
    auto hash = column("hash", "bytea");
    hash.cpp_type = "store::types::blob_id";
    hash.primary_key = true;
    hash.key = mtypes::key_kind::content_hash;
    auto ord = column("ord", "integer");
    ord.primary_key = true;
    ord.key = mtypes::key_kind::ord;
    auto parent = column("parent_ord", "integer");
    parent.member = "parent";
    parent.nullable = true;
    auto state = column("state", "text");
    state.values = {"open", "closed"};
    state.default_value = "'closed'";
    state.enum_name = "heading_state";
    auto tags = column("tags", "text[]");
    e.columns = {hash, ord, parent, state, tags};
    e.indexes = {{"tags", {"tags"}, false, "gin", "", "parent_ord > 0"}};
    return e;
}

}

TEST_CASE("schema.sql creates each table after the tables it references",
    "[codegen][masd]") {
    mtypes::masd_schema s;
    s.schemas = {"activity"};
    s.functions = {"CREATE FUNCTION activity.f() RETURNS int "
        "LANGUAGE sql AS $$ SELECT 1 $$;\n"};
    auto attached = table("activity", "attached");
    attached.columns = {column("run", "bigint")};
    attached.foreign_keys = {{"run", "activity.run", true}};
    const std::vector tables{attached, run(), campaign(),
        table("other", "ignored")};
    const auto sql = rendered("masd_sql_schema.sql.mustache",
        [&](auto& b, auto library) {
            return ops::masd_schema_context(b, library, s, tables);
        });
    const auto at = [&](std::string_view text) { return sql.find(text); };
    CHECK(at("CREATE SCHEMA activity;") < at("CREATE TABLE activity.campaign"));
    CHECK(at("CREATE TABLE activity.campaign") < at("CREATE TABLE activity.run"));
    CHECK(at("CREATE TABLE activity.run") <
        at("CREATE TABLE activity.attached"));
    CHECK_THAT(sql, ContainsSubstring(
        "    run bigint NOT NULL REFERENCES activity.run ON DELETE CASCADE\n"));
    CHECK(at("CREATE FUNCTION") > at("CREATE TABLE activity.run"));
    CHECK(at("other.ignored") == std::string::npos);
    CHECK_THAT(sql, ContainsSubstring(
        "    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,\n"
        "    story_id text NOT NULL,\n"
        "    environment text NOT NULL,\n"
        "    UNIQUE (environment, story_id)\n);"));
    CHECK_THAT(sql, ContainsSubstring(
        "-- activity.run is retained: its rows exist nowhere else"));
    CHECK_THAT(sql, ContainsSubstring(
        "    campaign bigint NOT NULL REFERENCES activity.campaign,\n"
        "    started_at timestamptz NOT NULL DEFAULT now(),\n"
        "    outcome text CHECK (outcome IN ('passed', 'failed'))\n);"));
    CHECK_THAT(sql, ContainsSubstring(
        "CREATE INDEX run_campaign ON activity.run (campaign);"));
}

TEST_CASE("schema.sql refuses tables that reference each other",
    "[codegen][masd]") {
    mtypes::masd_schema s;
    s.schemas = {"activity"};
    auto a = campaign();
    a.foreign_keys = {{"story_id", "activity.run", false}};
    const std::vector tables{run(), a};
    mtypes::model_tables t;
    compass::model::ops::builder b(t);
    CHECK_THROWS_AS(ops::masd_schema_context(b, b.detached_object(), s,
        tables), std::runtime_error);
}

TEST_CASE("a composite key, a partial index and a value list render as "
    "PostgreSQL states them", "[codegen][masd]") {
    mtypes::masd_schema s;
    s.schemas = {"corpus"};
    const std::vector tables{heading()};
    const auto sql = rendered("masd_sql_schema.sql.mustache",
        [&](auto& b, auto library) {
            return ops::masd_schema_context(b, library, s, tables);
        });
    CHECK_THAT(sql, ContainsSubstring(
        "    state text NOT NULL DEFAULT 'closed' CHECK (state IN ('open', "
        "'closed')),\n"));
    CHECK_THAT(sql, ContainsSubstring("    PRIMARY KEY (hash, ord)\n);"));
    CHECK_THAT(sql, ContainsSubstring("CREATE INDEX heading_tags ON "
        "corpus.heading USING gin (tags) WHERE parent_ord > 0;"));
}

TEST_CASE("a row holds what the row writes, and its key columns are the "
    "COPY writer's arguments", "[codegen][masd]") {
    const compass::model::ops::custom_types custom{
        {"store::types::blob_id", "\"compass.store/types/blob_id.hpp\""}};
    const auto make = [&](auto& b, auto library) {
        return ops::masd_entity_context(b, library, heading(), custom, {});
    };
    const auto row = rendered("masd_cpp_row.hpp.mustache", make);
    CHECK_THAT(row, ContainsSubstring(
        "struct heading_row {\n"
        "    std::optional<std::int32_t> parent;\n"
        "    heading_state state = heading_state::closed;\n"
        "    std::vector<std::string> tags;\n"
        "\n"
        "    bool operator==(const heading_row&) const = default;\n"
        "};"));
    CHECK_THAT(row, ContainsSubstring(
        "constexpr std::optional<heading_state> heading_state_of("));
    CHECK_THAT(row, ContainsSubstring("#include <cstdint>\n#include "
        "<optional>\n#include <string>\n#include <string_view>\n#include "
        "<vector>\n\nnamespace compass::db::types {"));

    const auto header = rendered("masd_cpp_copy.hpp.mustache", make);
    CHECK_THAT(header, ContainsSubstring(
        "inline constexpr std::string_view heading_copy_columns =\n"
        "    \"hash, ord, parent_ord, state, tags\";"));
    CHECK_THAT(header, ContainsSubstring(
        "void write_copy(ops::copy_writer& w,\n"
        "    const store::types::blob_id& hash,\n"
        "    std::size_t ord,\n"
        "    const types::heading_row& r);"));
    CHECK_THAT(header, ContainsSubstring(
        "#include \"compass.store/types/blob_id.hpp\""));

    const auto impl = rendered("masd_cpp_copy.cpp.mustache", make);
    CHECK_THAT(impl, ContainsSubstring(
        "    w.field(hash);\n    w.field(ord);\n    w.field(r.parent);\n"
        "    w.field(r.state);\n    w.field(r.tags);\n    w.end();"));
}

TEST_CASE("a SQL type with no C++ type is refused unless the model states "
    "one", "[codegen][masd]") {
    auto e = table("corpus", "section");
    e.columns = {column("tsv", "tsvector")};
    mtypes::model_tables t;
    compass::model::ops::builder b(t);
    CHECK_THROWS_AS(ops::masd_entity_context(b, b.detached_object(), e, {}, {}),
        std::runtime_error);
    e.columns[0].writer = mtypes::written_by::writer;
    CHECK_NOTHROW(ops::masd_entity_context(b, b.detached_object(), e, {}, {}));
}

TEST_CASE("an operation's messages follow the messages they hold, declare "
    "each enumeration once and carry base64 as bytes", "[codegen][masd]") {
    mtypes::masd_operation op;
    op.component = "serve";
    op.entity = "writes";
    op.ns = "compass::serve::messages";
    op.includes = {"<vector>", "<string>"};
    const auto field = [](std::string name, std::string type,
                           std::vector<std::string> values = {},
                           std::string encoding = "") {
        mtypes::masd_field f;
        f.name = std::move(name);
        f.cpp_type = std::move(type);
        f.values = std::move(values);
        f.encoding = std::move(encoding);
        return f;
    };
    mtypes::masd_message response{"start_response", "", "", {},
        {field("steps", "std::vector<run_step>"),
         field("outcome", "std::optional<step_outcome>", {"pass", "fail"})}};
    mtypes::masd_message step{"run_step", "", "", {},
        {field("ord", "std::int32_t"),
         field("outcome", "step_outcome", {"pass", "fail"}),
         field("format", "std::string", {"html", "org"}),
         field("bytes", "std::string", {}, "base64")}};
    op.messages = {response, step};
    const auto header = rendered("masd_cpp_protocol.hpp.mustache",
        [&](auto& b, auto library) {
            return ops::masd_operation_context(b, library, op, "compass",
                "db");
        });
    CHECK(header.find("struct run_step {") <
        header.find("struct start_response {"));
    CHECK_THAT(header, ContainsSubstring(
        "#include <optional>\n#include <stdexcept>\n#include <string>\n"
        "#include <string_view>\n#include <type_traits>\n#include <vector>\n"));
    CHECK_THAT(header, ContainsSubstring(
        "enum class step_outcome { pass, fail };"));
    CHECK(header.find("enum class step_outcome") ==
        header.rfind("enum class step_outcome"));
    CHECK_THAT(header, ContainsSubstring("    step_outcome outcome{};\n"
        "    std::string format;\n"
        "    compass::db::codec::base64_bytes bytes;\n"));
    CHECK_THAT(header, ContainsSubstring(
        "#include \"compass.db/codec/base64.hpp\""));
    CHECK_THAT(header, ContainsSubstring(
        "struct wire_traits<compass::serve::messages::run_step> {"));

    mtypes::masd_message other{"other", "", "", {},
        {field("outcome", "step_outcome", {"pass", "blocked"})}};
    op.messages.push_back(other);
    mtypes::model_tables t;
    compass::model::ops::builder b(t);
    CHECK_THROWS_AS(ops::masd_operation_context(b, b.detached_object(), op,
        "compass", "db"), std::runtime_error);
}

TEST_CASE("a routed request binds each field where :http_in: puts it and "
    "declares its route", "[codegen][masd]") {
    mtypes::masd_operation op;
    op.component = "serve";
    op.entity = "queries";
    op.ns = "compass::serve::messages";
    const auto field = [](std::string name, std::string type,
                           std::string in, bool optional = false) {
        mtypes::masd_field f;
        f.name = std::move(name);
        f.cpp_type = std::move(type);
        f.http_in = std::move(in);
        f.optional = optional;
        return f;
    };
    mtypes::masd_message filter{"filter", "", "", {},
        {field("type", "std::optional<std::string>", ""),
         field("limit", "std::uint32_t", "")}};
    filter.fields[1].default_value = "50";
    filter.fields[1].http_name = "max";
    mtypes::masd_message search{"search_request", "search_response",
        "Sections \"matching\" a query.", {},
        {field("environment", "std::string", "path"),
         field("q", "std::string", "query"),
         field("filter", "filter", "query"),
         field("note", "std::string", "body"),
         field("tags", "std::vector<std::string>", "body")}};
    search.route = {"post", "/v1/{environment}/search", "search_documents",
        "documents", "document", false, false, false, "Search", "view", ""};
    mtypes::masd_message response{"search_response", "", "", {}, {}};
    op.messages = {filter, search, response};

    const auto impl = rendered("masd_cpp_http_routes.cpp.mustache",
        [&](auto& b, auto library) {
            return ops::masd_operation_context(b, library, op, "compass",
                "db");
        });
    CHECK_THAT(impl, ContainsSubstring(
        "    b.body(r, { \"note\", \"tags\" }, { \"note\" });\n"
        "    b.path(\"environment\", r.environment);\n"
        "    b.query(\"q\", r.q, true);\n"
        "    b.query(\"type\", r.filter.type, false);\n"
        "    b.query(\"max\", r.filter.limit, false);\n"
        "    b.finish();\n"));
    CHECK_THAT(impl, ContainsSubstring(
        "r.post(\"/v1/{environment}/search\")\n"
        "        .summary(\"Search\")\n"
        "        .description(\"Sections \\\"matching\\\" a query.\")\n"
        "        .catalogue(\"search_documents\", \"documents\", \"document\", "
        "false)\n"
        "        .caller(domain::caller_need::view)\n"));
    CHECK_THAT(impl, ContainsSubstring(
        ".query_param(\"max\", \"integer\", \"\", false, \"\")"));
    CHECK_THAT(impl, ContainsSubstring(
        ".body<compass::serve::messages::search_request>()"));
    CHECK_THAT(impl, ContainsSubstring(
        ".response<compass::serve::messages::search_response>()"));
    CHECK(impl.find("no_body") == std::string::npos);
    CHECK(impl.find("bind_filter") == std::string::npos);
}

TEST_CASE("a row's JSON codec states the row's members and the text form "
    "of each enumeration", "[codegen][masd]") {
    auto e = heading();
    auto source = column("source", "bytea");
    source.cpp_type = "store::types::blob_id";
    e.columns.push_back(source);
    const compass::model::ops::custom_types custom{
        {"store::types::blob_id", "\"compass.store/types/blob_id.hpp\""}};
    const compass::model::ops::custom_types text_forms{
        {"store::types::blob_id", "\"compass.db/codec/blob_id_form.hpp\""}};
    const auto make = [&](auto& b, auto library) {
        return ops::masd_entity_context(b, library, e, custom, text_forms);
    };
    const auto header = rendered("masd_cpp_json.hpp.mustache", make);
    CHECK_THAT(header, ContainsSubstring(
        "    return ::compass::db::codec::field_list{\n"
        "        ::compass::db::codec::field{\"parent\", &heading_row::parent},\n"
        "        ::compass::db::codec::field{\"state\", &heading_row::state},\n"
        "        ::compass::db::codec::field{\"tags\", &heading_row::tags},\n"
        "        ::compass::db::codec::field{\"source\", &heading_row::source},\n"
        "    };"));
    CHECK_THAT(header, ContainsSubstring(
        "struct text_form<compass::db::types::heading_state> {"));
    CHECK_THAT(header, ContainsSubstring(
        "names[] = { \"open\", \"closed\" };"));
    CHECK_THAT(header, ContainsSubstring("is not one of open, closed\");"));
    CHECK_THAT(header, ContainsSubstring(
        "#include \"compass.db/codec/blob_id_form.hpp\"\n"));
    CHECK_THAT(header, ContainsSubstring(
        "struct wire_traits<compass::db::types::heading_row> {"));

    const auto impl = rendered("masd_cpp_json.cpp.mustache", make);
    CHECK_THAT(impl, ContainsSubstring(
        "return object::read_json<compass::db::types::heading_row>(text);"));
}
