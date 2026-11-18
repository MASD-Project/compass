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
#ifndef COMPASS_CROSSCHECK_TESTS_CURVE_FIXTURE_HPP
#define COMPASS_CROSSCHECK_TESTS_CURVE_FIXTURE_HPP

#include <string>
#include <string_view>
#include <vector>

/**
 * @brief A curve configuration projection and schema small enough to read,
 * with the two refdata models its entities name, shared by the tests of
 * the codecs that read the curve wiring.
 */
namespace compass::crosscheck::tests {

// generate_curve_section_parameters.py and generate_curve_column_guards.py
// were run on this fixture; the expected texts are what they wrote.
inline const std::string curve_projection = R"cpp(constexpr std::string_view yield_section = "YieldCurves";
constexpr std::string_view default_section = "DefaultCurves";

template <> struct document_part<refdata::domain::curve_segment_simple> {
    static constexpr auto vector = &curve_document::segment_simples;
    static constexpr auto key = &refdata::domain::curve_segment_simple::segment_id;
};
template <> struct document_part<refdata::domain::default_curve_spec> {
    static constexpr auto vector = &curve_document::default_curve_specs;
    static constexpr auto key = &refdata::domain::default_curve_spec::configuration_id;
};

const std::vector<section_access>& sections() {
    static const std::vector<section_access> all{
        {yield_section, "YieldCurve"},
        {default_section, "DefaultCurve"},
    };
    return all;
}

std::string import_context::segment(const record_reader& r, const std::string& definition_id, const std::string& type) {
    refdata::domain::curve_segment s;
    s.curve_definition_id = definition_id;
    s.segment_type = type;
    out.curve_segments.push_back(s);
    return s.id;
}

void import_quotes(import_context& ctx, const std::string& segment_id, const record_reader& v) {
    refdata::domain::curve_segment_quote q;
    q.segment_id = segment_id;
}

void import_simple(import_context& ctx, const std::string& segment_id, const record_reader& r) {
    refdata::domain::curve_segment_simple p;
    p.convention = required_text(r, "Conventions");
    p.projection = r.text("ProjectionCurve");
    ctx.out.segment_simples.push_back(p);
}

void import_segments(import_context& ctx, const record_reader& v) {
    for (const auto& r : v.children("Simple")) {
        const auto id = ctx.segment(r, "x", "Simple");
        import_simple(ctx, id, r);
    }
    for (const auto& r : v.children("Direct")) {
        const auto id = ctx.segment(r, "x", "Direct");
        import_simple(ctx, id, r);
    }
}

default_curve_parts parts_of_default_curve_type(const std::string& type) {
    default_curve_parts out;
    if (type == "Fitted" || type == "Spread") {
        out.spec = true;
    } else if (type == "Benchmark" || type == "Fitted") {
        out.spec = true;
    }
    return out;
}

void import_yield(import_context& ctx, curve_document& out, const record_reader& v) {
    add_definition(out, yield_section, required_text(v, "CurveId"),
        required_text(v, "CurveDescription"));
    refdata::domain::yield_curve y;
    y.currency = required_spelling(v, "Currency");
    y.day_counter = v.spelling("DayCounter");
    y.has_pillars = v.has("Pillars");
    y.id = ctx.mint();
    if (const auto pillars = v.child("Pillars"); pillars) {
        const auto inner = pillars->child("Inner");
    }
    if (const auto segs = v.child("Segments"); segs)
        import_segments(ctx, *segs);
    out.yield_curves.push_back(std::move(y));
}

void import_default(import_context& ctx, curve_document& out, const record_reader& v) {
    add_definition(out, default_section, required_text(v, "CurveId"),
        required_text(v, "CurveDescription"));
    refdata::domain::default_curve d;
    d.currency = required_spelling(v, "Currency");
    if (const auto c = v.child("Configuration"); c) {
        refdata::domain::default_curve_spec s;
    }
    out.default_curves.push_back(std::move(d));
}

void import_document(import_context& ctx, curve_document& out) {
    for (const auto& e : each("YieldCurves", "YieldCurve"))
        import_yield(ctx, out, e);
    for (const auto& e : each("DefaultCurves", "DefaultCurve"))
        import_default(ctx, out, e);
}
)cpp";

inline const std::string curve_schema = R"xsd(<xs:schema>
  <xs:complexType name="curveconfiguration">
    <xs:all>
      <xs:element type="yieldCurves" name="YieldCurves" minOccurs="0"/>
      <xs:element type="defaultCurves" name="DefaultCurves" minOccurs="0"/>
      <xs:element type="reportConfiguration" name="ReportConfiguration" minOccurs="0"/>
    </xs:all>
  </xs:complexType>
  <xs:complexType name="yieldCurves">
    <xs:sequence>
      <xs:element type="yieldCurve" name="YieldCurve" maxOccurs="unbounded"/>
    </xs:sequence>
  </xs:complexType>
  <xs:complexType name="defaultCurves">
    <xs:sequence>
      <xs:element type="defaultCurve" name="DefaultCurve" maxOccurs="unbounded"/>
    </xs:sequence>
  </xs:complexType>
  <xs:complexType name="reportConfiguration">
    <xs:sequence>
      <xs:element type="a" name="A"/>
      <xs:element type="b" name="B"/>
    </xs:sequence>
  </xs:complexType>
  <xs:complexType name="yieldCurve">
    <xs:sequence>
      <xs:element name="CurveId" type="xs:string"/>
      <xs:element name="CurveDescription" type="xs:string"/>
      <xs:element name="Currency" type="xs:string"/>
      <xs:element name="DayCounter" type="xs:string"/>
      <xs:element name="Pillars">
        <xs:complexType>
          <xs:element name="Inner" type="xs:string"/>
        </xs:complexType>
      </xs:element>
      <xs:element name="Segments" type="segments"/>
    </xs:sequence>
  </xs:complexType>
  <xs:complexType name="defaultCurve">
    <xs:sequence>
      <xs:element name="CurveId" type="xs:string"/>
      <xs:element name="CurveDescription" type="xs:string"/>
      <xs:element name="Currency" type="xs:string"/>
      <xs:element name="Configuration" type="xs:string"/>
    </xs:sequence>
  </xs:complexType>
</xs:schema>
)xsd";

inline const std::string yield_curve_model =
    "#+title: x\n:tablename: ores_refdata_yield_curves_tbl  \n";
inline const std::string default_curve_model =
    ":tablename:ores_refdata_default_curves_tbl\n";

}

#endif
