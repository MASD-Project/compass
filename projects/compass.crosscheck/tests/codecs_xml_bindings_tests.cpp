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
#include "compass.crosscheck/codecs/xml_bindings.hpp"

#include <array>
#include <fstream>
#include <regex>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.codec/io/output_sink.hpp"
#include "compass.codec/ops/data_tables.hpp"
#include "compass.codec/ops/outputs.hpp"
#include "compass.crosscheck/ops/xml_bindings.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using Catch::Matchers::ContainsSubstring;
using tests::files;
namespace xb = ops::xml_bindings;

constexpr std::string_view root_document = "root_document";
const std::vector<std::string> root_schema_names{"root.xsd"};

/**
 * @brief The files the generator writes for @p schemas with the root
 * element Root and the accessor root_document, as test_generate_xml_
 * bindings_derivation.py runs the script.
 */
xb::output emit(const std::vector<std::string>& schemas,
    std::string_view root_type = "rootType",
    std::span<const std::string_view> exact = {}) {
    return xb::generate(schemas, {.root_element = "Root",
        .root_type = root_type, .accessor = root_document,
        .name_space = "bindings::root", .include = "bindings/root.hpp",
        .schema_names = root_schema_names, .exact_decimals = exact},
        {.engine_include = "bindings/engine.hpp",
         .decimal_include = "bindings/decimal.hpp",
         .decimal_type = "bindings::decimal", .export_macro = "",
         .errata = {}});
}

/**
 * @brief The message the generator stops with for @p schemas.
 */
std::string refusal(const std::vector<std::string>& schemas,
    std::string_view root_type = "rootType") {
    try {
        (void)emit(schemas, root_type);
    } catch (const xb::error& e) {
        return e.what();
    }
    FAIL("the generator wrote the files");
    return {};
}

std::string record(const std::string& emitted, const std::string& name) {
    auto from = emitted.find("struct " + name + " {");
    REQUIRE(from != std::string::npos);
    const auto to = emitted.find("\n};", from);
    return emitted.substr(from, to - from);
}

/**
 * @brief The fields of the row one element states. The member offset is
 * an offsetof expression, whose arguments hold a comma, so it is replaced
 * before splitting.
 */
std::vector<std::string> row(const std::string& source,
    const std::string& name) {
    auto from = source.find("{\"" + name + "\"");
    REQUIRE(from != std::string::npos);
    auto text = source.substr(from, source.find('}', from) - from);
    text = std::regex_replace(text, std::regex(R"(offsetof\([^)]*\))"),
        "offset");
    std::vector<std::string> out;
    std::size_t at = 0;
    for (auto comma = text.find(','); comma != std::string::npos;
         comma = text.find(',', at)) {
        out.push_back(text.substr(at, comma - at));
        at = comma + 1;
    }
    out.push_back(text.substr(at));
    for (auto& f : out) {
        const auto b = f.find_first_not_of(' ');
        f = b == std::string::npos ? std::string() : f.substr(b);
    }
    return out;
}

std::size_t count(const std::string& text, const std::string& what) {
    std::size_t n = 0;
    for (auto at = text.find(what); at != std::string::npos;
         at = text.find(what, at + what.size()))
        ++n;
    return n;
}

std::string replaced(std::string text, const std::string& from,
    const std::string& to) {
    const auto at = text.find(from);
    REQUIRE(at != std::string::npos);
    return text.replace(at, from.size(), to);
}

const std::string base_schema = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:complexType name="baseType">
    <xs:sequence>
      <xs:element type="xs:string" name="Alpha"/>
    </xs:sequence>
    <xs:attribute type="xs:string" name="id" use="required"/>
  </xs:complexType>
</xs:schema>
)";

const std::string derived_schema = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:include schemaLocation="base.xsd"/>
  <xs:element type="derivedType" name="Root"/>
  <xs:complexType name="derivedType">
    <xs:complexContent>
      <xs:extension base="baseType">
        <xs:sequence>
          <xs:element type="xs:string" name="Beta"/>
        </xs:sequence>
        <xs:attribute type="xs:string" name="kind"/>
      </xs:extension>
    </xs:complexContent>
  </xs:complexType>
</xs:schema>
)";

const std::string duplicate_alternatives = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element type="choiceType" name="Root"/>
  <xs:complexType name="choiceType">
    <xs:choice>
      <xs:sequence>
        <xs:element type="xs:int" name="Shared"/>
        <xs:element type="xs:string" name="Alpha"/>
      </xs:sequence>
      <xs:sequence>
        <xs:element type="xs:int" name="Shared"/>
        <xs:element type="xs:string" name="Beta"/>
      </xs:sequence>
    </xs:choice>
  </xs:complexType>
</xs:schema>
)";

const std::string group_schema = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:group name="common">
    <xs:sequence>
      <xs:element type="xs:string" name="Alpha"/>
      <xs:element type="xs:string" name="Beta" minOccurs="0"/>
    </xs:sequence>
  </xs:group>
  <xs:element type="rootType" name="Root"/>
  <xs:complexType name="rootType">
    <xs:sequence>
      <xs:element type="xs:string" name="Head"/>
      <xs:group ref="common" maxOccurs="unbounded"/>
    </xs:sequence>
  </xs:complexType>
</xs:schema>
)";

const std::string recursive_schema = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element type="rootType" name="Root"/>
  <xs:complexType name="rootType">
    <xs:sequence>
      <xs:element type="xs:string" name="Head"/>
      <xs:element type="node" name="Node" minOccurs="0"/>
    </xs:sequence>
  </xs:complexType>
  <xs:complexType name="node">
    <xs:sequence>
      <xs:element type="xs:string" name="Value" minOccurs="0"/>
      <xs:element type="node" name="Child" minOccurs="0"/>
    </xs:sequence>
  </xs:complexType>
</xs:schema>
)";

std::string wide_schema(int fields) {
    std::string body;
    for (int i = 0; i < fields; ++i)
        body += "      <xs:element type=\"xs:string\" name=\"F" +
            std::to_string(i) + "\" minOccurs=\"0\"/>\n";
    return "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">\n"
           "  <xs:element type=\"rootType\" name=\"Root\"/>\n"
           "  <xs:complexType name=\"rootType\">\n    <xs:sequence>\n" +
        body + "    </xs:sequence>\n  </xs:complexType>\n</xs:schema>\n";
}

/**
 * @brief A root that repeats an item, which holds one of @p alternatives
 * kinds, three types between them.
 */
std::string choice_schema(int alternatives) {
    std::string members;
    for (int i = 0; i < alternatives; ++i)
        members += "        <xs:element type=\"kind" + std::to_string(i % 3) +
            "\" name=\"Alt" + std::to_string(i) + "\"/>\n";
    std::string kinds;
    for (int n = 0; n < 3; ++n) {
        const auto k = std::to_string(n);
        kinds += "  <xs:complexType name=\"kind" + k + "\">\n"
            "    <xs:sequence>\n"
            "      <xs:element type=\"xs:string\" name=\"Value" + k +
            "\" minOccurs=\"0\"/>\n    </xs:sequence>\n  </xs:complexType>\n";
    }
    return "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">\n"
           "  <xs:element type=\"rootType\" name=\"Root\"/>\n"
           "  <xs:complexType name=\"rootType\">\n    <xs:sequence>\n"
           "      <xs:element type=\"item\" name=\"Item\" "
           "maxOccurs=\"unbounded\"/>\n    </xs:sequence>\n"
           "  </xs:complexType>\n  <xs:complexType name=\"item\">\n"
           "    <xs:sequence>\n      <xs:element type=\"xs:string\" "
           "name=\"Label\" minOccurs=\"0\"/>\n      <xs:choice>\n" +
        members + "      </xs:choice>\n    </xs:sequence>\n"
                  "  </xs:complexType>\n" + kinds + "</xs:schema>\n";
}

std::string one_type(const std::string& body) {
    return "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">\n"
           "  <xs:element type=\"rootType\" name=\"Root\"/>\n"
           "  <xs:complexType name=\"rootType\">\n" + body +
        "  </xs:complexType>\n</xs:schema>\n";
}

const std::string leaf_type = R"(  <xs:complexType name="leaf">
    <xs:sequence><xs:element type="xs:string" name="Value" minOccurs="0"/></xs:sequence>
  </xs:complexType>
)";

std::string with_leaf(const std::string& body) {
    return replaced(one_type(body), "</xs:schema>", leaf_type + "</xs:schema>");
}

}

TEST_CASE("a derived type flattens its base, base first",
    "[codecs][xml_bindings]") {
    const auto out = emit({base_schema, derived_schema}, "derivedType");
    const auto derived = record(out.header, "derived_type");
    for (const auto member : {"alpha", "beta", "id", "kind"})
        CHECK_THAT(derived, ContainsSubstring(member));
    CHECK(derived.find("alpha") < derived.find("beta"));
}

TEST_CASE("a restriction of a base type is refused", "[codecs][xml_bindings]") {
    auto restricted = derived_schema;
    restricted = replaced(restricted, "<xs:extension", "<xs:restriction");
    restricted = replaced(restricted, "</xs:extension>", "</xs:restriction>");
    CHECK_THAT(refusal({restricted}, "derivedType"),
        ContainsSubstring("restricts a base type"));
}

TEST_CASE("two types that extend each other are refused",
    "[codecs][xml_bindings]") {
    const std::string loop = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element type="aType" name="Root"/>
  <xs:complexType name="aType">
    <xs:complexContent><xs:extension base="bType">
      <xs:sequence><xs:element type="xs:string" name="Alpha"/></xs:sequence>
    </xs:extension></xs:complexContent>
  </xs:complexType>
  <xs:complexType name="bType">
    <xs:complexContent><xs:extension base="aType">
      <xs:sequence><xs:element type="xs:string" name="Beta"/></xs:sequence>
    </xs:extension></xs:complexContent>
  </xs:complexType>
</xs:schema>
)";
    CHECK_THAT(refusal({loop}, "aType"), ContainsSubstring("cycle"));
}

TEST_CASE("a choice of sequences keeps every member",
    "[codecs][xml_bindings]") {
    const std::string nested = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element type="choiceType" name="Root"/>
  <xs:complexType name="choiceType">
    <xs:sequence>
      <xs:element type="xs:string" name="Head"/>
      <xs:choice>
        <xs:sequence>
          <xs:element type="xs:string" name="Alpha"/>
          <xs:element type="xs:string" name="Beta"/>
        </xs:sequence>
        <xs:sequence>
          <xs:element type="xs:string" name="Gamma" minOccurs="0"/>
          <xs:element type="xs:int" name="Delta"/>
        </xs:sequence>
      </xs:choice>
      <xs:element type="xs:string" name="Tail" minOccurs="0"/>
    </xs:sequence>
  </xs:complexType>
</xs:schema>
)";
    const auto r = record(emit({nested}, "choiceType").header, "choice_type");
    for (const auto member : {"head", "alpha", "beta", "gamma", "delta", "tail"})
        CHECK_THAT(r, ContainsSubstring(member));
    CHECK(r.find("alpha") < r.find("gamma"));
}

TEST_CASE("one element named by two alternatives is one member",
    "[codecs][xml_bindings]") {
    const auto r = record(emit({duplicate_alternatives}, "choiceType").header,
        "choice_type");
    CHECK(count(r, "shared") == 1);
    CHECK_THAT(r, ContainsSubstring("alpha"));
    CHECK_THAT(r, ContainsSubstring("beta"));
}

TEST_CASE("two alternatives that disagree about one name are refused",
    "[codecs][xml_bindings]") {
    const auto disagree = replaced(duplicate_alternatives,
        "<xs:element type=\"xs:int\" name=\"Shared\"/>\n        "
        "<xs:element type=\"xs:string\" name=\"Beta\"/>",
        "<xs:element type=\"xs:date\" name=\"Shared\"/>\n        "
        "<xs:element type=\"xs:string\" name=\"Beta\"/>");
    CHECK_THAT(refusal({disagree}, "choiceType"), ContainsSubstring("Shared"));
}

TEST_CASE("an annotation inside a compositor is ignored",
    "[codecs][xml_bindings]") {
    const auto r = record(emit({one_type(R"(    <xs:sequence>
      <xs:annotation><xs:documentation>Notes to a reader.</xs:documentation></xs:annotation>
      <xs:element type="xs:string" name="Alpha"/>
    </xs:sequence>
)")}).header, "root_type");
    CHECK_THAT(r, ContainsSubstring("alpha"));
    CHECK_FALSE(r.contains("documentation"));
}

TEST_CASE("a repeated choice repeats its members", "[codecs][xml_bindings]") {
    const auto r = record(emit({one_type(R"(    <xs:choice maxOccurs="unbounded">
      <xs:element type="xs:string" name="Alpha"/>
      <xs:element type="xs:string" name="Beta"/>
    </xs:choice>
)")}).header, "root_type");
    CHECK_THAT(r, ContainsSubstring("std::vector<std::string> alpha"));
    CHECK_THAT(r, ContainsSubstring("std::vector<std::string> beta"));
}

TEST_CASE("a member under an optional group is not required",
    "[codecs][xml_bindings]") {
    const auto out = emit({one_type(R"(    <xs:sequence>
      <xs:sequence minOccurs="0">
        <xs:element type="xs:string" name="Alpha"/>
      </xs:sequence>
      <xs:element type="xs:string" name="Beta"/>
    </xs:sequence>
)")});
    CHECK_THAT(record(out.header, "root_type"), ContainsSubstring("alpha"));
    CHECK(row(out.source, "Alpha")[6] == "false");
    CHECK(row(out.source, "Beta")[6] == "true");
}

TEST_CASE("a member named like an earlier type is qualified",
    "[codecs][xml_bindings]") {
    const auto r = record(emit({with_leaf(R"(    <xs:sequence>
      <xs:element type="xs:string" name="leaf"/>
      <xs:element type="leaf" name="first"/>
      <xs:element type="leaf" name="second"/>
    </xs:sequence>
)")}).header, "root_type");
    CHECK_THAT(r, ContainsSubstring("std::string leaf;"));
    CHECK_THAT(r, ContainsSubstring("struct leaf first;"));
    CHECK_THAT(r, ContainsSubstring("struct leaf second;"));
}

TEST_CASE("two alternatives with different cardinality are refused",
    "[codecs][xml_bindings]") {
    const std::string schema = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element type="choiceType" name="Root"/>
  <xs:complexType name="choiceType">
    <xs:choice>
      <xs:element type="xs:int" name="Shared"/>
      <xs:element type="xs:int" name="Shared" maxOccurs="unbounded"/>
    </xs:choice>
  </xs:complexType>
</xs:schema>
)";
    CHECK_THAT(refusal({schema}, "choiceType"), ContainsSubstring("Shared"));
}

TEST_CASE("a member named like its own type is also qualified when used "
    "again", "[codecs][xml_bindings]") {
    const auto r = record(emit({with_leaf(R"(    <xs:sequence>
      <xs:element type="leaf" name="leaf"/>
      <xs:element type="leaf" name="second"/>
    </xs:sequence>
)")}).header, "root_type");
    CHECK_THAT(r, ContainsSubstring("struct leaf leaf;"));
    CHECK_THAT(r, ContainsSubstring("struct leaf second;"));
}

TEST_CASE("an attribute named after a type shadows it too",
    "[codecs][xml_bindings]") {
    const auto r = record(emit({with_leaf(R"(    <xs:sequence>
      <xs:element type="leaf" name="first"/>
    </xs:sequence>
    <xs:attribute type="xs:string" name="leaf"/>
)")}).header, "root_type");
    CHECK_THAT(r, ContainsSubstring("std::string leaf;"));
    CHECK_THAT(r, ContainsSubstring("struct leaf first;"));
}

TEST_CASE("two alternatives with different inline types are refused",
    "[codecs][xml_bindings]") {
    const std::string schema = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element type="choiceType" name="Root"/>
  <xs:complexType name="choiceType">
    <xs:choice>
      <xs:element name="Shared">
        <xs:complexType><xs:sequence>
          <xs:element type="xs:string" name="Alpha"/>
        </xs:sequence></xs:complexType>
      </xs:element>
      <xs:element name="Shared">
        <xs:complexType><xs:sequence>
          <xs:element type="xs:int" name="Beta"/>
        </xs:sequence></xs:complexType>
      </xs:element>
    </xs:choice>
  </xs:complexType>
</xs:schema>
)";
    CHECK_THAT(refusal({schema}, "choiceType"), ContainsSubstring("Shared"));
}

TEST_CASE("a reference to a global element becomes a member of its name",
    "[codecs][xml_bindings]") {
    const std::string schema = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element type="rootType" name="Root"/>
  <xs:element type="xs:string" name="Gamma"/>
  <xs:element type="xs:string" name="Delta"/>
  <xs:complexType name="rootType">
    <xs:sequence>
      <xs:element ref="Gamma"/>
      <xs:element ref="Delta"/>
    </xs:sequence>
  </xs:complexType>
</xs:schema>
)";
    const auto r = record(emit({schema}).header, "root_type");
    CHECK_THAT(r, ContainsSubstring("std::string gamma;"));
    CHECK_THAT(r, ContainsSubstring("std::string delta;"));
}

TEST_CASE("one reference with two cardinalities is refused",
    "[codecs][xml_bindings]") {
    const std::string schema = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element type="rootType" name="Root"/>
  <xs:element type="xs:string" name="Gamma"/>
  <xs:complexType name="rootType">
    <xs:choice>
      <xs:element ref="Gamma"/>
      <xs:element ref="Gamma" maxOccurs="unbounded"/>
    </xs:choice>
  </xs:complexType>
</xs:schema>
)";
    CHECK_THAT(refusal({schema}), ContainsSubstring("Gamma"));
}

TEST_CASE("two alternatives with different inline enumerations are refused",
    "[codecs][xml_bindings]") {
    const std::string schema = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element type="choiceType" name="Root"/>
  <xs:complexType name="choiceType">
    <xs:choice>
      <xs:element name="Shared">
        <xs:simpleType><xs:restriction base="xs:string">
          <xs:enumeration value="A"/>
        </xs:restriction></xs:simpleType>
      </xs:element>
      <xs:element name="Shared">
        <xs:simpleType><xs:restriction base="xs:string">
          <xs:enumeration value="B"/>
        </xs:restriction></xs:simpleType>
      </xs:element>
    </xs:choice>
  </xs:complexType>
</xs:schema>
)";
    CHECK_THAT(refusal({schema}, "choiceType"), ContainsSubstring("Shared"));
}

TEST_CASE("text typed by an enumerated simple type keeps its spellings",
    "[codecs][xml_bindings]") {
    const std::string schema = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:simpleType name="scheme">
    <xs:restriction base="xs:string">
      <xs:enumeration value="Forward"/>
      <xs:enumeration value="Backward"/>
    </xs:restriction>
  </xs:simpleType>
  <xs:complexType name="entry">
    <xs:simpleContent>
      <xs:extension base="scheme">
        <xs:attribute type="xs:string" name="id"/>
      </xs:extension>
    </xs:simpleContent>
  </xs:complexType>
  <xs:element type="rootType" name="Root"/>
  <xs:complexType name="rootType">
    <xs:sequence>
      <xs:element type="entry" name="Entry"/>
    </xs:sequence>
  </xs:complexType>
</xs:schema>
)";
    const auto out = emit({schema});
    CHECK_THAT(record(out.header, "entry"),
        ContainsSubstring("std::int16_t value"));
    const auto own = out.source.find("{\"\", 0u, field_kind::enumeration");
    REQUIRE(own != std::string::npos);
    CHECK_THAT(out.source.substr(own, out.source.find('}', own) - own),
        ContainsSubstring("scheme_spellings"));
    CHECK_THAT(out.source, ContainsSubstring("const char* scheme_spellings[] "
        "= {\"Forward\", \"Backward\", nullptr};"));
}

TEST_CASE("an enumeration behind a restriction chain keeps its spellings",
    "[codecs][xml_bindings]") {
    const std::string schema = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:simpleType name="scheme">
    <xs:restriction base="xs:string">
      <xs:enumeration value="Forward"/>
      <xs:enumeration value="Backward"/>
    </xs:restriction>
  </xs:simpleType>
  <xs:simpleType name="schemeAlias">
    <xs:restriction base="scheme"/>
  </xs:simpleType>
  <xs:complexType name="entry">
    <xs:simpleContent><xs:extension base="schemeAlias"/></xs:simpleContent>
  </xs:complexType>
  <xs:element type="rootType" name="Root"/>
  <xs:complexType name="rootType">
    <xs:sequence><xs:element type="entry" name="Entry"/></xs:sequence>
  </xs:complexType>
</xs:schema>
)";
    const auto out = emit({schema});
    CHECK_THAT(record(out.header, "entry"),
        ContainsSubstring("std::int16_t value"));
    CHECK_THAT(out.source, ContainsSubstring("scheme_spellings"));
}

TEST_CASE("a model group is flattened where it is referenced",
    "[codecs][xml_bindings]") {
    const auto r = record(emit({group_schema}).header, "root_type");
    CHECK_THAT(r, ContainsSubstring("std::string head"));
    CHECK_THAT(r, ContainsSubstring("std::vector<std::string> alpha"));
    CHECK_THAT(r, ContainsSubstring("std::vector<std::string> beta"));
}

TEST_CASE("a substitution member that writes its own type is a record",
    "[codecs][xml_bindings]") {
    const std::string schema = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element name="headGroup" abstract="true"/>
  <xs:element name="Member" substitutionGroup="headGroup">
    <xs:complexType>
      <xs:sequence><xs:element type="xs:string" name="Alpha"/></xs:sequence>
    </xs:complexType>
  </xs:element>
  <xs:element type="rootType" name="Root"/>
  <xs:complexType name="rootType">
    <xs:sequence>
      <xs:element ref="headGroup" minOccurs="0"/>
    </xs:sequence>
  </xs:complexType>
</xs:schema>
)";
    const auto out = emit({schema});
    CHECK_THAT(record(out.header, "root_type"),
        ContainsSubstring("member member"));
    CHECK_THAT(record(out.header, "member"),
        ContainsSubstring("std::string alpha"));
}

TEST_CASE("an element with no type is carried as text",
    "[codecs][xml_bindings]") {
    const auto r = record(emit({one_type(
        "    <xs:sequence><xs:element name=\"Free\"/></xs:sequence>\n")})
        .header, "root_type");
    CHECK_THAT(r, ContainsSubstring("std::string free"));
}

TEST_CASE("a type with more than one mask word states its mask as an array",
    "[codecs][xml_bindings]") {
    CHECK_THAT(record(emit({wide_schema(70)}).header, "root_type"),
        ContainsSubstring("std::uint64_t present[2] = {0"));
    CHECK_THAT(record(emit({wide_schema(3)}).header, "root_type"),
        ContainsSubstring("std::uint64_t present = 0"));
}

TEST_CASE("a type that holds itself indexes the row it cannot hold",
    "[codecs][xml_bindings]") {
    const auto out = emit({recursive_schema});
    const auto n = record(out.header, "node");
    CHECK_THAT(n, ContainsSubstring("std::string value"));
    CHECK_THAT(n, ContainsSubstring("std::vector<std::uint32_t> child"));
    CHECK_THAT(n, ContainsSubstring("std::vector<struct node> child_arena"));
    CHECK_THAT(out.source, ContainsSubstring("field_kind::indexed_nested"));
    CHECK_THAT(out.source, ContainsSubstring("&node_record"));
    CHECK_THAT(out.source, ContainsSubstring("child_arena_ops"));
    CHECK_THAT(record(out.header, "root_type"),
        ContainsSubstring("std::string head"));
}

TEST_CASE("a required member of a sequence group stays required",
    "[codecs][xml_bindings]") {
    const auto out = emit({group_schema});
    CHECK(row(out.source, "Head")[6] == "true");
    CHECK(row(out.source, "Alpha")[6] == "true");
}

TEST_CASE("one group referenced twice keeps each reference's cardinality",
    "[codecs][xml_bindings]") {
    const std::string schema = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:group name="common">
    <xs:sequence><xs:element type="xs:string" name="Alpha"/></xs:sequence>
  </xs:group>
  <xs:element type="rootType" name="Root"/>
  <xs:complexType name="rootType">
    <xs:sequence>
      <xs:element type="oneType" name="One"/>
      <xs:element type="twoType" name="Two"/>
    </xs:sequence>
  </xs:complexType>
  <xs:complexType name="oneType">
    <xs:sequence><xs:group ref="common" maxOccurs="unbounded"/></xs:sequence>
  </xs:complexType>
  <xs:complexType name="twoType">
    <xs:sequence><xs:group ref="common"/></xs:sequence>
  </xs:complexType>
</xs:schema>
)";
    const auto out = emit({schema});
    CHECK_THAT(record(out.header, "one_type"),
        ContainsSubstring("std::vector<std::string> alpha"));
    CHECK_THAT(record(out.header, "two_type"),
        ContainsSubstring("std::string alpha"));
}

TEST_CASE("a reference to a global element of an earlier schema resolves",
    "[codecs][xml_bindings]") {
    const std::string first = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element name="Shared" type="sharedType"/>
  <xs:complexType name="sharedType">
    <xs:sequence><xs:element type="xs:string" name="Alpha"/></xs:sequence>
  </xs:complexType>
</xs:schema>
)";
    const std::string second = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:include schemaLocation="first.xsd"/>
  <xs:element type="rootType" name="Root"/>
  <xs:complexType name="rootType">
    <xs:sequence><xs:element ref="Shared"/></xs:sequence>
  </xs:complexType>
</xs:schema>
)";
    CHECK_THAT(record(emit({first, second}).header, "root_type"),
        ContainsSubstring("shared_type shared"));
}

TEST_CASE("an inline enumeration on a group member keeps its spellings",
    "[codecs][xml_bindings]") {
    const std::string schema = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:group name="colourGroup">
    <xs:sequence>
      <xs:element name="Colour">
        <xs:simpleType>
          <xs:restriction base="xs:string">
            <xs:enumeration value="Red"/>
            <xs:enumeration value="Blue"/>
          </xs:restriction>
        </xs:simpleType>
      </xs:element>
    </xs:sequence>
  </xs:group>
  <xs:element type="rootType" name="Root"/>
  <xs:complexType name="rootType">
    <xs:sequence><xs:group ref="colourGroup"/></xs:sequence>
  </xs:complexType>
</xs:schema>
)";
    const auto out = emit({schema});
    CHECK_THAT(record(out.header, "root_type"),
        ContainsSubstring("std::int16_t colour"));
    CHECK_THAT(out.source, ContainsSubstring("const char* root_type_colour_"
        "spellings[] = {\"Red\", \"Blue\", nullptr};"));
}

TEST_CASE("two schemas that disagree about one global element are refused",
    "[codecs][xml_bindings]") {
    const std::string first = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element name="Shared" type="firstType"/>
  <xs:complexType name="firstType">
    <xs:sequence><xs:element type="xs:string" name="Alpha"/></xs:sequence>
  </xs:complexType>
</xs:schema>
)";
    const std::string second = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element name="Shared" type="secondType"/>
  <xs:complexType name="secondType">
    <xs:sequence><xs:element type="xs:string" name="Beta"/></xs:sequence>
  </xs:complexType>
  <xs:element type="rootType" name="Root"/>
  <xs:complexType name="rootType">
    <xs:sequence><xs:element ref="Shared"/></xs:sequence>
  </xs:complexType>
</xs:schema>
)";
    CHECK_THAT(refusal({first, second}), ContainsSubstring("Shared"));
}

TEST_CASE("a mutual cycle declares the table it indexes ahead of its row",
    "[codecs][xml_bindings]") {
    const std::string schema = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element type="alpha" name="Root"/>
  <xs:complexType name="alpha">
    <xs:sequence>
      <xs:element type="beta" name="Beta" minOccurs="0"/>
    </xs:sequence>
  </xs:complexType>
  <xs:complexType name="beta">
    <xs:sequence>
      <xs:element type="alpha" name="Alpha" minOccurs="0"/>
    </xs:sequence>
  </xs:complexType>
</xs:schema>
)";
    const auto out = emit({schema}, "alpha");
    const auto beta = record(out.header, "beta");
    CHECK_THAT(beta, ContainsSubstring("std::vector<std::uint32_t> alpha"));
    CHECK_THAT(beta, ContainsSubstring("std::vector<struct alpha> alpha_arena"));
    const auto declared = out.source.find(
        "extern const record_type alpha_record;");
    REQUIRE(declared != std::string::npos);
    CHECK(declared < out.source.find("const record_type alpha_record{"));
    CHECK_FALSE(out.source.contains("extern const record_type beta_record;"));
}

TEST_CASE("a recursive member the schema allows once states it",
    "[codecs][xml_bindings]") {
    const auto fields = row(emit({recursive_schema}).source, "Child");
    CHECK(fields[5] == "false");
    CHECK(fields[6] == "false");
    CHECK(fields[12].starts_with("true"));
}

TEST_CASE("an exact decimal leaves a text element as text",
    "[codecs][xml_bindings]") {
    constexpr std::array<std::string_view, 1> strike{"Strike"};
    const auto out = emit({one_type("    <xs:sequence><xs:element "
        "type=\"xs:string\" name=\"Strike\"/></xs:sequence>\n")}, "rootType",
        strike);
    CHECK_THAT(out.header, ContainsSubstring("std::string strike;"));
    CHECK_THAT(out.source, ContainsSubstring("field_kind::text"));
}

TEST_CASE("an exact decimal promotes a real", "[codecs][xml_bindings]") {
    constexpr std::array<std::string_view, 1> amount{"Amount"};
    const auto out = emit({one_type("    <xs:sequence><xs:element "
        "type=\"xs:double\" name=\"Amount\"/></xs:sequence>\n")}, "rootType",
        amount);
    CHECK_THAT(out.header, ContainsSubstring("decimal amount;"));
    CHECK_THAT(out.source, ContainsSubstring("field_kind::decimal"));
}

TEST_CASE("a wildcard carries every real exactly, the text of a simple "
    "content element too", "[codecs][xml_bindings]") {
    constexpr std::array<std::string_view, 1> every{"*"};
    const auto out = emit({one_type(R"(    <xs:sequence>
      <xs:element type="xs:double" name="Level"/>
      <xs:element name="Rate">
        <xs:complexType>
          <xs:simpleContent>
            <xs:extension base="xs:float">
              <xs:attribute type="xs:string" name="startDate"/>
            </xs:extension>
          </xs:simpleContent>
        </xs:complexType>
      </xs:element>
    </xs:sequence>
)")}, "rootType", every);
    CHECK_THAT(out.header, ContainsSubstring("decimal level;"));
    CHECK_THAT(out.header, ContainsSubstring("decimal value;"));
    CHECK(count(out.source, "field_kind::decimal") >= 2);
}

TEST_CASE("a wide choice keeps its alternatives in columns its holder owns",
    "[codecs][xml_bindings]") {
    const auto out = emit({choice_schema(70)});
    const auto item = record(out.header, "item");
    CHECK_THAT(item, ContainsSubstring("std::uint32_t kind_index = 0;"));
    CHECK_FALSE(item.contains("alt0"));
    CHECK_THAT(record(out.header, "root_type"),
        ContainsSubstring("item_kind_columns kind_columns;"));
    CHECK(count(out.source, "field_kind::column_nested") == 70);
}

TEST_CASE("alternatives of one type share one column",
    "[codecs][xml_bindings]") {
    const auto columns = record(emit({choice_schema(70)}).header,
        "item_kind_columns");
    CHECK(count(columns, "std::vector<struct kind") == 3);
    for (int n = 0; n < 3; ++n) {
        const auto k = std::to_string(n);
        CHECK_THAT(columns, ContainsSubstring("std::vector<struct kind" + k +
            "> kind" + k + "_rows;"));
    }
}

TEST_CASE("a narrow choice keeps its alternatives as members",
    "[codecs][xml_bindings]") {
    const auto out = emit({choice_schema(8)});
    CHECK_FALSE(out.header.contains("kind_index"));
    CHECK_FALSE(out.header.contains("kind_columns"));
    CHECK_FALSE(out.source.contains("column_nested"));
}

TEST_CASE("a column row tells the engine where to find the columns",
    "[codecs][xml_bindings]") {
    const auto out = emit({choice_schema(70)});
    CHECK_THAT(row(out.source, "Item")[2],
        ContainsSubstring("field_kind::repeated_nested"));
    CHECK_THAT(out.source,
        ContainsSubstring("offsetof(root_type, kind_columns)"));
    CHECK_THAT(row(out.source, "Alt4")[2],
        ContainsSubstring("field_kind::column_nested"));
    CHECK_THAT(out.source,
        ContainsSubstring("offsetof(item_kind_columns, kind1_rows)"));
}

namespace {

namespace cops = compass::codec::ops;

constexpr std::string_view document = "compass/checks/xml_bindings.org";

/**
 * @brief A table of the check document, its rows a line apart from line
 * @p line.
 */
cops::data_table table(std::string name, std::uint32_t line,
    std::vector<std::string> columns,
    std::vector<std::vector<std::string>> rows) {
    cops::data_table t{.name = std::move(name), .line = line,
        .columns = std::move(columns), .rows = std::move(rows)};
    for (std::size_t r = 0; r < t.rows.size(); ++r)
        t.row_lines.push_back(line + 2 + static_cast<std::uint32_t>(r));
    return t;
}

using rows = std::vector<std::vector<std::string>>;

/**
 * @brief A check document with @p settings, @p families, @p exact and
 * @p errata as its four tables.
 */
cops::data_tables check_document(rows settings, rows families, rows exact = {},
    rows errata = {}) {
    cops::data_tables t{.source = std::string(document)};
    t.tables = {
        table("Families", 10, {"stem", "root element", "root type", "schemas"},
            std::move(families)),
        table("Exact decimals", 30, {"family", "element"}, std::move(exact)),
        table("Schema errata", 40, {"written", "read"}, std::move(errata)),
        table("Settings", 50, {"setting", "value"}, std::move(settings))};
    return t;
}

tests::options data(const cops::data_tables& t) {
    return {{std::string(cops::data_option), cops::encode_data_tables(t)}};
}

/**
 * @brief The table @p name of @p t.
 */
cops::data_table& in(cops::data_tables& t, std::string_view name) {
    for (auto& table : t.tables)
        if (table.name == name)
            return table;
    FAIL("no table " << name);
    return t.tables.front();
}

const rows ore_settings{
    {"schema_directory", "external/ore/xsd"},
    {"header_directory",
     "projects/ores.ore/core/include/ores.ore.core/xml/generated"},
    {"source_directory", "projects/ores.ore/core/src/xml/generated"},
    {"include_prefix", "ores.ore.core/xml/generated"},
    {"namespace_prefix", "ores::ore::xml::generated"},
    {"engine_include", "ores.ore.core/xml/binding.hpp"},
    {"decimal_include", "ores.utility/decimal/decimal.hpp"},
    {"decimal_type", "ores::utility::decimal::decimal"},
    {"export_macro", "ORES_ORE_CORE_EXPORT"}};

/**
 * @brief ORE Studio's layout, with the calendar adjustments family, whose
 * schemas the checkout holds, and the IBOR fallback family, whose schemas
 * it does not.
 */
cops::data_tables ore_document() {
    return check_document(ore_settings,
        {{"calendar_adjustments", "CalendarAdjustments", "calendaradjustment",
          "calendaradjustment.xsd ore_types.xsd"},
         {"ibor_fallback_config", "IborFallbackConfig", "IborFallbackConfig",
          "iborfallbackconfig.xsd ore_types.xsd"}},
        {{"ibor_fallback_config", "Spread"}});
}

const std::string header_path = "projects/ores.ore/core/include/ores.ore.core/"
                                "xml/generated/calendar_adjustments.hpp";
const std::string source_path =
    "projects/ores.ore/core/src/xml/generated/calendar_adjustments.cpp";

/**
 * @brief A checkout holding the schemas of the calendar adjustments family
 * and of no other.
 */
files calendar_checkout() {
    return {{"external/ore/xsd/calendaradjustment.xsd",
                R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element type="calendaradjustment" name="CalendarAdjustments"/>
  <xs:complexType name="calendaradjustment">
    <xs:sequence>
      <xs:element type="xs:date" name="AdditionalHolidays" minOccurs="0" maxOccurs="unbounded"/>
      <xs:element type="weekday" name="Weekend" minOccurs="0"/>
    </xs:sequence>
    <xs:attribute type="xs:string" name="name" use="required"/>
  </xs:complexType>
</xs:schema>
)"},
        {"external/ore/xsd/ore_types.xsd",
            R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:simpleType name="weekday">
    <xs:restriction base="xs:string">
      <xs:enumeration value="Sat"/>
      <xs:enumeration value="Sun"/>
    </xs:restriction>
  </xs:simpleType>
</xs:schema>
)"}};
}

}

TEST_CASE("each family is a header and a source under the generated "
    "directories, and a family whose schemas are missing writes nothing",
    "[codecs][xml_bindings]") {
    const auto out = tests::generate(codecs::xml_bindings, calendar_checkout(),
        data(ore_document()));
    REQUIRE(out.size() == 2);
    const auto& header = out.at(header_path);
    CHECK_THAT(header, ContainsSubstring(
        "#ifndef ORES_ORE_CORE_XML_GENERATED_CALENDAR_ADJUSTMENTS_HPP"));
    CHECK_THAT(header, ContainsSubstring("namespace ores::ore::xml::generated::"
        "calendar_adjustments {"));
    CHECK_THAT(header, ContainsSubstring("ORES_ORE_CORE_EXPORT auto "
        "calendar_adjustments_document() -> const document_type&;"));
    CHECK_THAT(header, ContainsSubstring(" * Generated by compass from "
        "calendaradjustment.xsd and ore_types.xsd;\n"
        " * regenerate with compass generate.\n */\n"));
    const auto& source = out.at(source_path);
    CHECK_THAT(source, ContainsSubstring("#include \"ores.ore.core/xml/"
        "generated/calendar_adjustments.hpp\""));
    CHECK_THAT(source, ContainsSubstring("static const document_type doc{"
        "\"CalendarAdjustments\", &calendaradjustment_record};"));
    CHECK_THAT(source, ContainsSubstring(
        "const char* weekday_spellings[] = {\"Sat\", \"Sun\", nullptr};"));

    const auto r = tests::run(codecs::xml_bindings, calendar_checkout(),
        data(ore_document()));
    REQUIRE(r.findings.size() == 1);
    CHECK_THAT(std::get<2>(r.findings.front()), ContainsSubstring(
        "cannot read the schema external/ore/xsd/iborfallbackconfig.xsd of "
        "the ibor_fallback_config family"));
}

TEST_CASE("a family the generator refuses writes nothing and says why",
    "[codecs][xml_bindings]") {
    auto checkout = calendar_checkout();
    checkout.at("external/ore/xsd/calendaradjustment.xsd") = replaced(
        checkout.at("external/ore/xsd/calendaradjustment.xsd"),
        "<xs:sequence>", "<xs:sequence><xs:attributeGroup ref=\"common\"/>");
    CHECK(tests::generate(codecs::xml_bindings, checkout,
        data(ore_document())).empty());
    const auto r = tests::run(codecs::xml_bindings, checkout,
        data(ore_document()));
    REQUIRE(r.findings.size() == 2);
    CHECK_THAT(std::get<2>(r.findings.front()),
        ContainsSubstring("uses xs:attributeGroup, which is not supported"));
}

TEST_CASE("a family row taken out of the check document is no longer "
    "generated", "[codecs][xml_bindings]") {
    auto doc = ore_document();
    auto& families = in(doc, "Families").rows;
    families.erase(families.begin());
    auto& lines = in(doc, "Families").row_lines;
    lines.erase(lines.begin());
    CHECK(tests::generate(codecs::xml_bindings, calendar_checkout(),
        data(doc)).empty());
}

TEST_CASE("without its check document, a table, a column or a setting the "
    "codec writes nothing and says what is missing",
    "[codecs][xml_bindings]") {
    const auto only = [](const tests::outcome& r) {
        REQUIRE(r.findings.size() == 1);
        return std::get<2>(r.findings.front());
    };
    const auto none = tests::run(codecs::xml_bindings, calendar_checkout());
    CHECK(std::get<0>(none.findings.at(0)) == document);
    CHECK_THAT(only(none), ContainsSubstring("xml_bindings"));
    CHECK(tests::generate(codecs::xml_bindings, calendar_checkout()).empty());

    auto no_table = ore_document();
    no_table.tables.pop_back();
    CHECK_THAT(only(tests::run(codecs::xml_bindings, calendar_checkout(),
        data(no_table))), ContainsSubstring("Settings"));

    auto no_column = ore_document();
    in(no_column, "Families").columns[2] = "type";
    CHECK_THAT(only(tests::run(codecs::xml_bindings, calendar_checkout(),
        data(no_column))), ContainsSubstring("root type"));

    auto no_setting = ore_document();
    in(no_setting, "Settings").rows.pop_back();
    in(no_setting, "Settings").row_lines.pop_back();
    CHECK_THAT(only(tests::run(codecs::xml_bindings, calendar_checkout(),
        data(no_setting))), ContainsSubstring("no row states the setting "
        "'export_macro'"));
    CHECK(tests::generate(codecs::xml_bindings, calendar_checkout(),
        data(no_setting)).empty());

    auto stray = ore_document();
    in(stray, "Exact decimals").rows.push_back({"portfolio", "Strike"});
    in(stray, "Exact decimals").row_lines.push_back(40);
    CHECK_THAT(only(tests::run(codecs::xml_bindings, calendar_checkout(),
        data(stray))), ContainsSubstring(std::string(document) + ":40: the "
        "family 'portfolio' is no row of the table \"Families\""));
}

namespace {

/**
 * @brief A made-up project: a library of books under schemas/, whose
 * bindings go to src/include/shelf/bindings and src/lib/bindings in the
 * namespace shelf::bindings, carried by its own engine and money type. Its
 * schema misspells Title, and its price is money.
 */
files shelf_checkout() {
    return {{"schemas/library.xsd",
                R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:include schemaLocation="common.xsd"/>
  <xs:element type="libraryType" name="Library"/>
  <xs:complexType name="libraryType">
    <xs:sequence>
      <xs:element type="bookType" name="Book" maxOccurs="unbounded"/>
    </xs:sequence>
  </xs:complexType>
  <xs:complexType name="bookType">
    <xs:sequence>
      <xs:element type="xs:string" name="Titel"/>
      <xs:element type="genre" name="Genre" minOccurs="0"/>
      <xs:element type="xs:double" name="Price" minOccurs="0"/>
      <xs:element type="xs:double" name="Weight" minOccurs="0"/>
    </xs:sequence>
  </xs:complexType>
</xs:schema>
)"},
        {"schemas/common.xsd",
            R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:simpleType name="genre">
    <xs:restriction base="xs:string">
      <xs:enumeration value="Fiction"/>
      <xs:enumeration value="Poetry"/>
    </xs:restriction>
  </xs:simpleType>
</xs:schema>
)"}};
}

cops::data_tables shelf_document() {
    return check_document({{"schema_directory", "schemas/"},
            {"header_directory", "src/include/shelf/bindings"},
            {"source_directory", "src/lib/bindings/"},
            {"include_prefix", "shelf/bindings"},
            {"namespace_prefix", "shelf::bindings"},
            {"engine_include", "shelf/xml/engine.hpp"},
            {"decimal_include", "shelf/money.hpp"},
            {"decimal_type", "shelf::money"},
            {"export_macro", "SHELF_API"}},
        {{"library", "Library", "libraryType", "library.xsd common.xsd"}},
        {{"library", "Price"}}, {{"Titel", "Title"}});
}

}

TEST_CASE("a project of its own generates its bindings from its own check "
    "document, and nothing of ORE Studio's", "[codecs][xml_bindings]") {
    const auto r = tests::run(codecs::xml_bindings, shelf_checkout(),
        data(shelf_document()));
    CHECK(r.findings.empty());
    const auto out = tests::generate(codecs::xml_bindings, shelf_checkout(),
        data(shelf_document()));
    REQUIRE(out.size() == 2);
    const auto& header = out.at("src/include/shelf/bindings/library.hpp");
    const auto& source = out.at("src/lib/bindings/library.cpp");
    CHECK_THAT(header, ContainsSubstring(" * Generated by compass from "
        "library.xsd and common.xsd; regenerate with compass\n * generate.\n"));
    CHECK_THAT(header, ContainsSubstring(
        "#ifndef SHELF_BINDINGS_LIBRARY_HPP\n"));
    CHECK_THAT(header, ContainsSubstring(
        "#include \"shelf/xml/engine.hpp\"\n#include \"shelf/money.hpp\"\n"));
    CHECK_THAT(header, ContainsSubstring("namespace shelf::bindings::library {"));
    CHECK_THAT(header, ContainsSubstring("shelf::money price;"));
    CHECK_THAT(header, ContainsSubstring("double weight"));
    CHECK_THAT(header, ContainsSubstring("std::string title;"));
    CHECK_THAT(header, ContainsSubstring(
        "SHELF_API auto library_document() -> const document_type&;"));
    CHECK_THAT(source, ContainsSubstring(
        "#include \"shelf/bindings/library.hpp\""));
    CHECK_THAT(source, ContainsSubstring("{\"Title\""));
    CHECK_THAT(source, ContainsSubstring(
        "const char* genre_spellings[] = {\"Fiction\", \"Poetry\", nullptr};"));
    for (const auto& text : {header, source}) {
        CHECK_FALSE(text.contains("ores"));
        CHECK_FALSE(text.contains("ORES"));
        CHECK_FALSE(text.contains("Titel"));
    }

    auto no_export = shelf_document();
    in(no_export, "Settings").rows.back().back() = "";
    CHECK_THAT(tests::generate(codecs::xml_bindings, shelf_checkout(),
        data(no_export)).at("src/include/shelf/bindings/library.hpp"),
        ContainsSubstring("\nauto library_document() -> const "
        "document_type&;"));
}

TEST_CASE("a generated file edited by hand is drift",
    "[codecs][xml_bindings]") {
    namespace ct = compass::codec::types;
    namespace cio = compass::codec::io;
    const auto all = calendar_checkout();
    ct::checkout_files checkout;
    for (const auto& [path, bytes] : all)
        checkout.paths.push_back(path);
    checkout.content_of = [&](std::string_view p) {
        return std::optional(compass::store::ops::blob_id_of(
            all.find(p)->second));
    };
    checkout.read = [&](std::string_view p) {
        return std::optional(all.find(p)->second);
    };
    ct::corpus corpus;
    corpus.files = &checkout;
    const compass::org::types::type_registry types;
    ct::configuration settings;
    settings.set(codecs::xml_bindings.name, "enabled", "true");
    settings.set(codecs::xml_bindings.name, std::string(cops::data_option),
        cops::encode_data_tables(ore_document()));
    const std::array<ct::descriptor, 1> codecs{codecs::xml_bindings};
    const auto registry = compass::codec::ops::make_registry(codecs, types,
        settings);
    const auto result = compass::codec::ops::run(registry, corpus, types, 1);
    const auto outputs = compass::codec::ops::outputs_of(registry, result);
    REQUIRE(outputs.size() == 2);

    const compass::testing::filesystem::scoped_directory root(
        "compass.crosscheck.tests.xml_bindings");
    const auto written = cio::write_outputs(root.path(), outputs, {});
    REQUIRE(written.problems.empty());
    const auto unchanged = cio::plan_outputs(root.path(), outputs,
        written.record);
    for (const auto action : unchanged.actions)
        CHECK_FALSE(cio::changes_file(action));

    {
        std::ofstream edit(root.path() / source_path,
            std::ios::binary | std::ios::app);
        edit << "// edited by hand\n";
    }
    const auto plan = cio::plan_outputs(root.path(), outputs, written.record);
    std::vector<std::string> drifted;
    for (std::size_t i = 0; i < plan.size(); ++i)
        if (cio::changes_file(plan.actions[i]))
            drifted.emplace_back(plan.paths[i]);
    CHECK(drifted == std::vector<std::string>{source_path});
}
