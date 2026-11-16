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
#include "compass.sql/codecs/curve_parameter_paths.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::sql;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

files tree() {
    return {
        {"external/ore/xsd/curveconfig.xsd",
         "<xs:include schemaLocation=\"types.xsd\"/>\n"
         "<xs:element type=\"curveConfiguration\" name=\"CurveConfiguration\"/>\n"
         "  <xs:complexType name=\"curveConfiguration\">\n"
         "    <xs:element name=\"YieldCurves\" type=\"yieldCurves\"/>\n"
         "    <xs:element name=\"Note\" type=\"xs:string\"/>\n"
         "  </xs:complexType>\n"},
        {"external/ore/xsd/types.xsd",
         "  <xs:complexType name=\"yieldCurves\">\n"
         "    <xs:element name=\"YieldCurve\" type=\"yieldCurve\"/>\n"
         "  </xs:complexType>\n"
         "  <xs:complexType name=\"yieldCurve\">\n"
         "    <xs:element name=\"CurveId\" type=\"xs:string\"/>\n"
         "    <xs:element name=\"Segments\" type=\"segments\"/>\n"
         "  </xs:complexType>\n"
         "  <xs:complexType name=\"segments\">\n"
         "    <xs:element name=\"Simple\" type=\"simple\"/>\n"
         "  </xs:complexType>\n"
         "  <xs:complexType name=\"simple\">\n"
         "    <xs:element name=\"Quotes\"/>\n"
         "  </xs:complexType>\n"},
        {"projects/ores.sql/populate/reporting/"
         "reporting_ore_parameters_artefact_populate.sql",
         "insert into t values\n"
         "('/CurveConfiguration/YieldCurves/YieldCurve/CurveId'),\n"
         "('/CurveConfiguration/YieldCurves/YieldCurve/Colour'),\n"
         "('/CurveConfiguration/Note/Text'),\n"
         "('/CurveConfiguration/YieldCurves/YieldCurve/Segments/Simple/Priority'),\n"
         "('/CurveConfiguration/Missing');\n"},
        {"projects/ores.sql/utility/curve_schema_exemptions.json",
         "{\n  \"rules\": [\n"
         "    {\"reason\": \"read \\\"anyway\\\"\", \"path\": [\"YieldCurves\", "
         "\"YieldCurve\", \"Segments\", \"*\"], \"elements\": [\"Priority\"]},\n"
         "    {\"reason\": \"unused\", \"path\": [\"Gone\"], "
         "\"elements\": [\"X\"]}\n"
         "  ]\n}\n"}};
}

}

TEST_CASE("a path the schema does not declare and an exemption nothing "
    "needs are reported", "[codecs][curve_parameter_paths]") {
    const auto r = tests::run(codecs::curve_parameter_paths, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.sql/populate/reporting/"
         "reporting_ore_parameters_artefact_populate.sql", 6},
        {"projects/ores.sql/populate/reporting/"
         "reporting_ore_parameters_artefact_populate.sql", 4},
        {"projects/ores.sql/populate/reporting/"
         "reporting_ore_parameters_artefact_populate.sql", 3},
        {"projects/ores.sql/utility/curve_schema_exemptions.json", 4}});
}
