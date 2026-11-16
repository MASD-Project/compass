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
#include "compass.codegen/ops/entity_steps.hpp"

#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "entity_fixture.hpp"

namespace {

using namespace compass;
using strings = std::vector<std::string>;

codegen::types::entity_registry registry() {
    codegen::types::entity_registry r;
    auto& inbox = r.directories["projects/ores.inbox/modeling"];
    inbox.member_names = {"notification_delivery", "notification"};
    inbox.group_names = {"delivery_outcome"};
    r.domain_headers = {
        "projects/ores.diff/include/ores.diff/domain/diff_span.hpp",
        "projects/ores.diff/include/ores.diff/domain/diff_span_codec.hpp",
        "projects/ores.dq/api/include/ores.dq.api/domain/audit_record.hpp"};
    return r;
}

const codegen::types::entity_site site{
    "projects/ores.inbox/modeling/ores.inbox.notification_delivery.org",
    true, true};

}

/*
 * ores.inbox.notification_delivery's row holds the delivery_outcome group,
 * whose shape is in its own header (oracle: wire_codec_includes).
 */
TEST_CASE("a row's member types name the headers that declare them",
    "[codegen][entity][codecs]") {
    codegen::tests::entity_fixture f(R"j({"component": "inbox",
        "component_include": "inbox.api",
        "columns": [{"name": "outcome", "cpp_type": "delivery_outcome"},
            {"name": "parent", "cpp_type": "std::optional<notification>"},
            {"name": "at", "cpp_type": "std::chrono::system_clock::time_point"}],
        "primary_key": {"columns": [{"column": "id",
            "cpp_type": "boost::uuids::uuid"}]}})j");
    codegen::ops::derive_codec_includes(f.b, f.entity, registry(), site);
    CHECK(f.strings("wire_codec_includes") == strings{
        "ores.inbox.api/domain/delivery_outcome.hpp",
        "ores.inbox.api/domain/notification_codec.hpp"});
    CHECK(f.json("has_generated_wire_codec") == "true");
}

TEST_CASE("groups and the audit group state their own headers first",
    "[codegen][entity][codecs]") {
    codegen::tests::entity_fixture f(R"j({"component": "refdata",
        "subcomponent": "api",
        "domain_groups": [
            {"header": "\"ores.refdata.api/domain/convention_identity.hpp\""}],
        "has_audit_group": true,
        "audit_group_qualified": "ores::dq::domain::audit_record"})j");
    codegen::ops::derive_codec_includes(f.b, f.entity, registry(), site);
    CHECK(f.strings("wire_codec_includes") == strings{
        "ores.refdata.api/domain/convention_identity.hpp",
        "ores.dq.api/domain/audit_record.hpp"});
}

/*
 * ores.diff.diff_result: a message field naming another component's type
 * resolves through the header that exists, its codec header when it has
 * one; the envelope's utility types need nothing.
 */
TEST_CASE("message fields resolve foreign types through their headers",
    "[codegen][entity][codecs]") {
    codegen::tests::entity_fixture f(R"j({"component": "inbox",
        "component_include": "inbox.api",
        "messages": [{"fields": [
            {"cpp_type": "std::vector<ores::diff::domain::diff_span>"},
            {"cpp_type": "ores::utility::domain::result"},
            {"cpp_type": "std::optional<ores::inbox::domain::notification>"},
            {"cpp_type": "domain::delivery_outcome"}]}]})j");
    codegen::ops::derive_codec_includes(f.b, f.entity, registry(), site);
    CHECK(f.strings("message_codec_includes") == strings{
        "ores.diff/domain/diff_span_codec.hpp",
        "ores.inbox.api/domain/notification_codec.hpp",
        "ores.inbox.api/domain/delivery_outcome.hpp"});
    CHECK(f.strings("wire_codec_includes").empty());
}
