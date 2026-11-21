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
#include "compass.crosscheck/ops/xml_bindings.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include "compass.platform/text/xml.hpp"

/*
 * The port follows generate_xml_bindings.py function by function. The
 * script edits the schema tree as it walks it, copies nodes, and keys its
 * tables by node identity, and its output depends on all three, so the
 * port keeps a tree of its own with the same identities and edits.
 */
namespace compass::crosscheck::ops::xml_bindings {

namespace {

namespace pt = compass::platform::text;

using strings = std::vector<std::string>;
using maybe = std::optional<std::string>;

/**
 * @brief One element of a schema tree, by its local name, as ElementTree
 * holds it: the script reads, sets and copies attributes in place.
 */
struct node {
    std::string tag;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<node*> children;

    [[nodiscard]] maybe get(std::string_view key) const {
        for (const auto& [k, v] : attributes)
            if (k == key)
                return v;
        return std::nullopt;
    }

    [[nodiscard]] std::string get(std::string_view key,
        std::string_view fallback) const {
        return get(key).value_or(std::string(fallback));
    }

    void set(std::string_view key, std::string_view value) {
        for (auto& [k, v] : attributes)
            if (k == key) {
                v = value;
                return;
            }
        attributes.emplace_back(key, value);
    }
};

/**
 * @brief Python's `a or b` over attribute values: an empty string is false.
 */
maybe either(const maybe& a, const maybe& b) {
    return a && !a->empty() ? a : b;
}

const std::map<std::string, std::string, std::less<>> primitives{
    {"xs:string", "text"}, {"xs:integer", "integer"}, {"xs:int", "integer"},
    {"xs:long", "integer"}, {"xs:decimal", "decimal"}, {"xs:double", "real"},
    {"xs:float", "real"}, {"xs:date", "text"}, {"xs:time", "text"},
    {"xs:dateTime", "text"}, {"xs:boolean", "text"},
    {"xs:nonNegativeInteger", "integer"}, {"xs:positiveInteger", "integer"},
    {"xs:unsignedInt", "integer"}, {"xs:unsignedLong", "integer"},
    {"xs:unsignedShort", "integer"}, {"xs:byte", "integer"},
    {"xs:short", "integer"}, {"xs:anyURI", "text"}, {"xs:token", "text"},
    {"xs:normalizedString", "text"}};

const std::map<std::string, std::string, std::less<>> cpp_type{
    {"text", "std::string"}, {"enumeration", "std::int16_t"},
    {"integer", "std::int64_t"}, {"real", "double"}};

std::string_view cpp_zero(std::string_view cpp) {
    return cpp == "std::int64_t" || cpp == "std::int16_t" || cpp == "double"
        ? " = 0" : "";
}

const std::map<std::string, std::string, std::less<>> scalar{
    {"text", "scalar_kind::text"}, {"integer", "scalar_kind::integer"},
    {"decimal", "scalar_kind::decimal"}, {"real", "scalar_kind::real"},
    {"enumeration", "scalar_kind::enumeration"}};

constexpr std::string_view licence =
    R"(/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
)";

constexpr std::size_t banner_width = 80;

/**
 * @brief Names the engine already uses in the generated namespace.
 */
const std::set<std::string, std::less<>> reserved{"field", "record_type",
    "document_type", "event_type", "reader", "binder", "field_kind",
    "scalar_kind", "residual_element", "bind_error", "token", "token_kind",
    "document", "is_blank", "no_text"};

constexpr std::size_t mask_word_bits = 64;
constexpr std::size_t max_mask_bits = 255;
constexpr std::size_t column_choice_members = 64;

bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }
bool is_lower(char c) { return c >= 'a' && c <= 'z'; }
bool is_digit(char c) { return c >= '0' && c <= '9'; }

std::string snake(std::string_view name) {
    std::string step;
    for (std::size_t i = 0; i < name.size(); ++i) {
        if (i > 0 && i + 1 < name.size() && is_upper(name[i]) &&
            is_upper(name[i - 1]) && is_lower(name[i + 1]))
            step.push_back('_');
        step.push_back(name[i]);
    }
    std::string out;
    for (std::size_t i = 0; i < step.size(); ++i) {
        if (i > 0 && is_upper(step[i]) &&
            (is_lower(step[i - 1]) || is_digit(step[i - 1])))
            out.push_back('_');
        out.push_back(step[i]);
    }
    for (auto& c : out)
        if (is_upper(c))
            c = static_cast<char>(c - 'A' + 'a');
    return out;
}

/**
 * @brief FNV-1a over the code points of @p name, as the script's
 * hash_name folds ord() of each character.
 */
std::uint32_t hash_name(std::string_view name) {
    std::uint32_t h = 2166136261u;
    for (std::size_t i = 0; i < name.size();) {
        const auto lead = static_cast<unsigned char>(name[i]);
        std::uint32_t code = lead;
        std::size_t extra = 0;
        if (lead >= 0xF0) {
            code = lead & 0x07u;
            extra = 3;
        } else if (lead >= 0xE0) {
            code = lead & 0x0Fu;
            extra = 2;
        } else if (lead >= 0xC0) {
            code = lead & 0x1Fu;
            extra = 1;
        }
        ++i;
        for (std::size_t k = 0; k < extra && i < name.size(); ++k, ++i)
            code = (code << 6) |
                (static_cast<unsigned char>(name[i]) & 0x3Fu);
        h ^= code;
        h *= 16777619u;
    }
    return h;
}

std::string quoted(const std::string& s) { return "'" + s + "'"; }

std::string joined(const strings& parts, std::string_view sep) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0)
            out += sep;
        out += parts[i];
    }
    return out;
}

/**
 * @brief The comment that heads each generated file: the licence, then a
 * banner naming @p schemas and how to regenerate, wrapped to banner_width.
 */
std::string heading(std::span<const std::string> schemas) {
    std::string sentence = "Generated by compass from";
    for (std::size_t i = 0; i < schemas.size(); ++i) {
        if (i > 0)
            sentence += i + 1 == schemas.size() ? " and" : ",";
        sentence += " " + schemas[i];
    }
    sentence += "; regenerate with compass generate.";
    std::string out(licence);
    std::string line = " *";
    std::size_t at = 0;
    while (at < sentence.size()) {
        auto end = sentence.find(' ', at);
        if (end == std::string::npos)
            end = sentence.size();
        const auto word = sentence.substr(at, end - at);
        if (line.size() > 2 && line.size() + 1 + word.size() > banner_width) {
            out += line + "\n";
            line = " *";
        }
        line += " " + word;
        at = end + 1;
    }
    return out + line + "\n */\n";
}

/**
 * @brief @p include in upper case, every character but a letter or digit
 * an underscore: the include guard of the header it names.
 */
std::string guard_of(std::string_view include) {
    std::string out;
    for (const char c : include)
        out.push_back(is_lower(c) ? static_cast<char>(c - 'a' + 'A')
            : is_upper(c) || is_digit(c) ? c : '_');
    return out;
}

[[noreturn]] void stop(const std::string& why) { throw error(why); }

/**
 * @brief An inline simpleType: the spellings of its enumeration, the
 * primitive kind its restriction resolves to, or neither.
 */
using inline_simple_result = std::variant<std::monostate, strings, maybe>;

struct field_shape_result {
    std::string kind;
    maybe nested;
};

/**
 * @brief What the script's Schema object holds, and the module-level
 * CHOICE_CHILDREN set, which one run of the script fills for one family.
 */
class generator {
public:
    generator(std::span<const std::string> schemas, const project& p)
        : project_(p) {
        for (const auto& text : schemas) {
            const auto doc = pt::xml_document::parse(text);
            if (!doc)
                stop("a schema is not well-formed XML");
            node* root = adopt(*doc, doc->root());
            apply_errata(root);
            register_inline(root);
            register_substitutes(root);
            for (node* n : iter(root)) {
                const auto name = n->get("name");
                if (n->tag == "complexType" && name && !name->empty()) {
                    complex_types_[*name] = n;
                } else if (n->tag == "simpleType" && name && !name->empty()) {
                    simple_types_[*name] = n;
                    strings values;
                    for (node* restriction : n->children)
                        if (restriction->tag == "restriction")
                            for (node* child : restriction->children)
                                if (child->tag == "enumeration")
                                    values.push_back(child->get("value",
                                        "None"));
                    if (!values.empty())
                        enumerations_[*name] = std::move(values);
                } else if (n->tag == "group" && name && !name->empty()) {
                    groups_.try_emplace(*name, n);
                } else if (n->tag == "element" && name && !name->empty()) {
                    elements_.try_emplace(*name, n);
                }
            }
        }
    }

    [[nodiscard]] bool has_complex_type(const std::string& name) const {
        return complex_types_.contains(name);
    }

    output render(const family& f);

private:
    node* make() { return &nodes_.emplace_back(); }

    node* adopt(const pt::xml_document& doc, const pt::xml_element& e) {
        node* n = make();
        const auto close = e.tag.find('}');
        n->tag = close == std::string::npos ? e.tag : e.tag.substr(close + 1);
        n->attributes = e.attributes;
        for (const auto id : e.children)
            n->children.push_back(adopt(doc, doc[id]));
        return n;
    }

    static void iter_into(node* n, std::vector<node*>& out) {
        out.push_back(n);
        for (node* c : n->children)
            iter_into(c, out);
    }

    static std::vector<node*> iter(node* n) {
        std::vector<node*> out;
        iter_into(n, out);
        return out;
    }

    void apply_errata(node* root) const {
        for (node* n : iter(root)) {
            if (n->tag != "element")
                continue;
            if (const auto name = n->get("name"))
                if (const auto e = std::ranges::find(project_.errata, *name,
                        &std::pair<std::string, std::string>::first);
                    e != project_.errata.end())
                    n->set("name", e->second);
        }
    }

    /**
     * @brief The C++ type of a member of the primitive kind @p kind.
     */
    [[nodiscard]] std::string cpp_of(const std::string& kind) const {
        return kind == "decimal" ? std::string(project_.decimal_type)
            : cpp_type.at(kind);
    }

    void register_inline(node* root) {
        std::unordered_map<node*, node*> parents;
        for (node* n : iter(root))
            for (node* c : n->children)
                parents[c] = n;
        const auto parent_of = [&](node* n) -> node* {
            const auto p = parents.find(n);
            return p == parents.end() ? nullptr : p->second;
        };
        for (node* n : iter(root)) {
            if (n->tag != "element")
                continue;
            node* inline_type = nullptr;
            for (node* c : n->children)
                if (c->tag == "complexType") {
                    inline_type = c;
                    break;
                }
            if (inline_type == nullptr)
                continue;
            const auto name = n->get("name");
            if (!name)
                continue;
            std::string chosen = *name;
            node* owner = parent_of(n);
            for (auto held = complex_types_.find(chosen);
                 held != complex_types_.end() && held->second != inline_type;
                 held = complex_types_.find(chosen)) {
                const maybe enclosing = owner ? owner->get("name")
                                              : std::nullopt;
                chosen = enclosing && !enclosing->empty()
                    ? *enclosing + "_" + chosen : chosen + "_type";
                owner = owner ? parent_of(owner) : nullptr;
            }
            complex_types_.try_emplace(chosen, inline_type);
            inline_name_[n] = chosen;
        }
    }

    void register_substitutes(node* root) {
        std::vector<std::string> order;
        std::unordered_map<std::string, node*> by_name;
        for (node* n : root->children) {
            if (n->tag != "element")
                continue;
            const auto name = n->get("name");
            if (!name || name->empty())
                continue;
            if (!by_name.contains(*name))
                order.push_back(*name);
            by_name[*name] = n;
        }
        for (node* n : iter(root)) {
            if (n->tag != "element")
                continue;
            if (const auto group = n->get("substitutionGroup"))
                substitutes_[*group].push_back(n);
        }
        for (const auto& name : order) {
            node* declared = by_name.at(name);
            const auto held = globals_.find(name);
            if (held != globals_.end() &&
                held->second->get("type") != declared->get("type"))
                stop("two schemas declare a global element named " + name +
                    " with different types");
            globals_.try_emplace(name, declared);
        }
    }

    node* clone_with_identity(node* original) {
        node* copy = make();
        copy->tag = original->tag;
        copy->attributes = original->attributes;
        if (const auto n = inline_name_.find(original); n != inline_name_.end())
            inline_name_[copy] = n->second;
        if (const auto e = inline_enum_.find(original); e != inline_enum_.end())
            inline_enum_[copy] = e->second;
        for (node* c : original->children)
            copy->children.push_back(clone_with_identity(c));
        return copy;
    }

    using bases_set = std::set<std::string>;

    static std::string type_name_of(const node* complex_type) {
        return complex_type->get("name", "None");
    }

    void walk(node* n, const node* complex_type, std::vector<node*>& elements,
        std::vector<node*>& attributes, const bases_set& bases,
        const maybe& repeat_max_in, bool in_choice, bool optional_in,
        const std::set<std::string>& groups) {
        const auto& name = n->tag;
        auto repeat_max = repeat_max_in;
        auto optional = optional_in;
        if (name == "annotation")
            return;
        if (name == "group") {
            const auto reference = n->get("ref");
            const auto definition = reference
                ? groups_.find(*reference) : groups_.end();
            if (definition == groups_.end())
                stop("the type " + type_name_of(complex_type) +
                    " references the group " + reference.value_or("None") +
                    ", which is not a group this generator read");
            if (groups.contains(*reference))
                stop("the type " + type_name_of(complex_type) +
                    " references the group " + *reference + " in a cycle");
            if (const auto own = n->get("maxOccurs", "1"); own != "1")
                repeat_max = own;
            optional = optional || n->get("minOccurs", "1") == "0";
            auto inner = groups;
            inner.insert(*reference);
            for (node* child : definition->second->children) {
                node* cloned = clone_with_identity(child);
                if (repeat_max && cloned->get("maxOccurs", "1") == "1")
                    cloned->set("maxOccurs", *repeat_max);
                walk(cloned, complex_type, elements, attributes, bases,
                    repeat_max, in_choice, optional, inner);
            }
            return;
        }
        if (name == "sequence" || name == "all" || name == "choice") {
            if (const auto own = n->get("maxOccurs", "1"); own != "1")
                repeat_max = own;
            optional = optional || n->get("minOccurs", "1") == "0";
            const bool alternatives = in_choice || name == "choice";
            for (node* child : n->children)
                walk(child, complex_type, elements, attributes, bases,
                    repeat_max, alternatives, optional, groups);
            return;
        }
        if (name == "element") {
            if (repeat_max && n->get("maxOccurs", "1") == "1")
                n->set("maxOccurs", *repeat_max);
            if (optional && n->get("minOccurs", "1") != "0")
                n->set("minOccurs", "0");
            elements.push_back(n);
            if (in_choice)
                choice_children_.insert(n);
            return;
        }
        if (name == "attribute") {
            attributes.push_back(n);
            return;
        }
        if (name == "simpleContent") {
            for (node* extension : n->children) {
                if (extension->tag != "extension")
                    continue;
                for (node* child : extension->children)
                    walk(child, complex_type, elements, attributes, bases, {},
                        false, false, {});
            }
            return;
        }
        if (name == "complexContent") {
            for (node* derivation : n->children) {
                if (derivation->tag == "restriction")
                    stop("the type " + type_name_of(complex_type) +
                        " restricts a base type, which is not supported");
                if (derivation->tag != "extension")
                    continue;
                const auto base = derivation->get("base");
                const auto base_type = base
                    ? complex_types_.find(*base) : complex_types_.end();
                if (base_type == complex_types_.end())
                    stop("the type " + type_name_of(complex_type) +
                        " extends " + base.value_or("None") +
                        ", which is not a complexType this generator read");
                if (bases.contains(*base))
                    stop("the type " + type_name_of(complex_type) +
                        " derives from " + *base + " in a cycle");
                auto deeper = bases;
                deeper.insert(*base);
                auto [base_elements, base_attributes] =
                    children_of(base_type->second, deeper);
                elements.insert(elements.end(), base_elements.begin(),
                    base_elements.end());
                attributes.insert(attributes.end(), base_attributes.begin(),
                    base_attributes.end());
                for (node* child : derivation->children)
                    walk(child, complex_type, elements, attributes, bases, {},
                        false, false, {});
            }
            return;
        }
        if (name == "attributeGroup" || name == "anyAttribute")
            stop("the type " + type_name_of(complex_type) + " uses xs:" +
                name + ", which is not supported");
    }

    using shape_key = std::tuple<maybe, maybe, maybe, maybe, strings>;

    shape_key shape(node* element) {
        const auto reference = element->get("ref");
        node* target = element;
        if (reference)
            if (const auto e = elements_.find(*reference); e != elements_.end())
                target = e->second;
        maybe inline_type;
        if (const auto n = inline_name_.find(target); n != inline_name_.end())
            inline_type = n->second;
        strings spellings;
        if (const auto e = inline_enum_.find(target); e != inline_enum_.end())
            spellings = e->second;
        return {either(reference, target->get("name")),
            either(target->get("type"), inline_type),
            element->get("minOccurs"), element->get("maxOccurs"),
            std::move(spellings)};
    }

    std::pair<std::vector<node*>, std::vector<node*>> children_of(
        node* complex_type, const bases_set& bases = {}) {
        std::vector<node*> elements;
        std::vector<node*> attributes;
        for (node* n : complex_type->children)
            walk(n, complex_type, elements, attributes, bases, {}, false,
                false, {});

        std::map<maybe, node*> first_by_key;
        std::vector<node*> kept;
        for (node* element : elements) {
            const auto key = std::get<0>(shape(element));
            const auto first = first_by_key.find(key);
            if (first == first_by_key.end()) {
                first_by_key.emplace(key, element);
                kept.push_back(element);
                continue;
            }
            const bool both_choice = choice_children_.contains(element) &&
                choice_children_.contains(first->second);
            if (both_choice && shape(element) == shape(first->second))
                continue;
            kept.push_back(element);
        }
        elements = std::move(kept);

        std::vector<std::string> element_names;
        for (node* e : elements)
            element_names.push_back(either(e->get("name"), e->get("ref"))
                .value_or("None"));
        std::vector<std::string> attribute_names;
        for (node* a : attributes)
            attribute_names.push_back(a->get("name", "None"));
        const auto count = [](const strings& s, const std::string& x) {
            return std::ranges::count(s, x);
        };
        for (const auto& name : element_names) {
            if (count(element_names, name) > 1)
                stop(type_name_of(complex_type) + " holds two elements named " +
                    quoted(name));
            if (count(attribute_names, name) > 0)
                stop(type_name_of(complex_type) +
                    " declares both an element and an attribute named " +
                    quoted(name));
        }
        for (const auto& name : attribute_names)
            if (count(attribute_names, name) > 1)
                stop(type_name_of(complex_type) +
                    " holds two attributes named " + quoted(name));
        return {std::move(elements), std::move(attributes)};
    }

    static maybe simple_content_base(const node* complex_type) {
        for (node* n : complex_type->children) {
            if (n->tag != "simpleContent")
                continue;
            for (node* extension : n->children)
                if (extension->tag == "extension")
                    return extension->get("base");
        }
        return std::nullopt;
    }

    maybe resolve_enumeration(const maybe& name, std::set<std::string>& seen) {
        if (!name || seen.contains(*name))
            return std::nullopt;
        seen.insert(*name);
        if (enumerations_.contains(*name))
            return name;
        const auto n = simple_types_.find(*name);
        if (n == simple_types_.end())
            return std::nullopt;
        for (node* restriction : n->second->children) {
            if (restriction->tag != "restriction")
                continue;
            if (auto resolved = resolve_enumeration(restriction->get("base"),
                    seen))
                return resolved;
        }
        return std::nullopt;
    }

    maybe resolve_enumeration(const maybe& name) {
        std::set<std::string> seen;
        return resolve_enumeration(name, seen);
    }

    maybe resolve_simple(const maybe& name, std::set<std::string>& seen) {
        if (!name)
            return std::nullopt;
        if (const auto p = primitives.find(*name); p != primitives.end())
            return p->second;
        if (enumerations_.contains(*name))
            return std::nullopt;
        const auto n = simple_types_.find(*name);
        if (n == simple_types_.end())
            return std::nullopt;
        if (seen.contains(*name))
            return std::nullopt;
        seen.insert(*name);
        for (node* restriction : n->second->children)
            if (restriction->tag == "restriction")
                if (const auto base = restriction->get("base"))
                    return resolve_simple(base, seen);
        for (node* u : n->second->children) {
            if (u->tag != "union")
                continue;
            const auto members = u->get("memberTypes", "");
            std::size_t at = 0;
            while (at < members.size()) {
                const auto begin = members.find_first_not_of(" \t\r\n", at);
                if (begin == std::string::npos)
                    break;
                auto end = members.find_first_of(" \t\r\n", begin);
                if (end == std::string::npos)
                    end = members.size();
                if (auto resolved = resolve_simple(
                        members.substr(begin, end - begin), seen))
                    return resolved;
                at = end;
            }
        }
        return std::nullopt;
    }

    maybe resolve_simple(const maybe& name) {
        std::set<std::string> seen;
        return resolve_simple(name, seen);
    }

    maybe content_kind(const node* complex_type) {
        const auto base = simple_content_base(complex_type);
        if (!base)
            return std::nullopt;
        if (resolve_enumeration(base))
            return "enumeration";
        if (const auto p = primitives.find(*base); p != primitives.end())
            return p->second;
        return resolve_simple(base).value_or("text");
    }

    maybe content_enumeration(const node* complex_type) {
        return resolve_enumeration(simple_content_base(complex_type));
    }

    static bool is_residual(node* complex_type) {
        for (node* n : iter(complex_type))
            if (n->tag == "any")
                return true;
        return false;
    }

    inline_simple_result inline_simple(const node* element) {
        for (node* child : element->children) {
            if (child->tag != "simpleType")
                continue;
            strings values;
            for (node* restriction : child->children)
                if (restriction->tag == "restriction")
                    for (node* e : restriction->children)
                        if (e->tag == "enumeration")
                            values.push_back(e->get("value", "None"));
            if (!values.empty())
                return values;
            for (node* restriction : child->children)
                if (restriction->tag == "restriction")
                    if (const auto base = restriction->get("base"))
                        return resolve_simple(base);
        }
        return std::monostate{};
    }

    field_shape_result field_shape(node* element) {
        const auto found = inline_simple(element);
        if (const auto* values = std::get_if<strings>(&found)) {
            inline_enum_[element] = *values;
            return {"enumeration", std::nullopt};
        }
        if (const auto* kind = std::get_if<maybe>(&found); kind && *kind)
            return {**kind, std::nullopt};
        const auto declared = element->get("type");
        if (declared) {
            if (const auto p = primitives.find(*declared);
                p != primitives.end())
                return {p->second, std::nullopt};
            if (declared->starts_with("xs:"))
                return {"text", std::nullopt};
            if (enumerations_.contains(*declared))
                return {"enumeration", std::nullopt};
            if (simple_types_.contains(*declared))
                return {resolve_simple(declared).value_or("text"),
                    std::nullopt};
            return {"nested", declared};
        }
        const auto has = [&](std::string_view tag) {
            return std::ranges::any_of(element->children,
                [&](const node* c) { return c->tag == tag; });
        };
        if (has("complexType")) {
            const auto n = inline_name_.find(element);
            if (n == inline_name_.end())
                stop("element " + element->get("name", "None") +
                    " has an inline complexType the generator did not name");
            return {"nested", n->second};
        }
        if (!has("simpleType"))
            return {"text", std::nullopt};
        stop("element " + element->get("name", "None") +
            " has neither a type nor a complexType");
    }

    std::vector<node*> expand(node* element) {
        const auto ref = element->get("ref");
        if (!ref)
            return {element};
        const auto members = substitutes_.find(*ref);
        const auto head_it = globals_.find(*ref);
        node* head = head_it == globals_.end() ? nullptr : head_it->second;
        if (members != substitutes_.end() && !members->second.empty()) {
            std::vector<node*> out;
            for (node* member : members->second) {
                node* stand_in = clone_with_identity(member);
                stand_in->set("minOccurs", "0");
                stand_in->set("maxOccurs", element->get("maxOccurs", "1"));
                out.push_back(stand_in);
            }
            return out;
        }
        if (head != nullptr && !head->get("type") &&
            inline_name_.contains(head)) {
            node* copy = clone_with_identity(head);
            copy->set("minOccurs", element->get("minOccurs", "1"));
            copy->set("maxOccurs", element->get("maxOccurs", "1"));
            return {copy};
        }
        if (head != nullptr && head->get("type")) {
            node* stand_in = make();
            stand_in->tag = "xs:element";
            stand_in->set("name", *ref);
            stand_in->set("type", *head->get("type"));
            stand_in->set("minOccurs", element->get("minOccurs", "1"));
            stand_in->set("maxOccurs", element->get("maxOccurs", "1"));
            return {stand_in};
        }
        stop("element reference " + *ref + " cannot be resolved: " +
            (head != nullptr ? "the global element states no type"
                             : "no such global element"));
    }

    void collect(const std::string& type_name, std::vector<std::string>& order,
        std::unordered_set<std::string>& seen) {
        if (seen.contains(type_name))
            return;
        seen.insert(type_name);
        order.push_back(type_name);
        const auto found = complex_types_.find(type_name);
        if (found == complex_types_.end())
            return;
        node* complex_type = found->second;
        for (node* n : complex_type->children) {
            if (n->tag != "complexContent")
                continue;
            for (node* derivation : n->children)
                if (const auto base = derivation->get("base"))
                    collect(*base, order, seen);
        }
        for (node* element : children_of(complex_type).first)
            for (node* field : expand(element)) {
                const auto s = field_shape(field);
                if (s.kind == "nested" && s.nested)
                    collect(*s.nested, order, seen);
            }
    }

    maybe nested_target(node* field) {
        auto s = field_shape(field);
        if (s.kind != "nested" || !s.nested)
            return std::nullopt;
        return s.nested;
    }

    bool is_complex(const maybe& name) const {
        return name && complex_types_.contains(*name);
    }

    std::vector<node*> column_members(const std::string& type_name) {
        const auto found = complex_types_.find(type_name);
        if (found == complex_types_.end())
            return {};
        std::vector<node*> members;
        for (node* element : children_of(found->second).first)
            if (choice_children_.contains(element) && !element->get("ref") &&
                element->get("maxOccurs", "1") == "1" &&
                is_complex(nested_target(element)))
                members.push_back(element);
        return members.size() >= column_choice_members ? members
                                                        : std::vector<node*>{};
    }

    using edge = std::pair<std::string, std::string>;

    struct cut_edge {
        std::string holder;
        std::string member;
        std::string target;
        auto operator<=>(const cut_edge&) const = default;
    };

    std::vector<std::string> order_types(const std::string& root_type) {
        std::vector<std::string> reachable;
        std::unordered_set<std::string> seen;
        collect(root_type, reachable, seen);
        std::unordered_map<std::string, std::vector<edge>> edges;
        std::unordered_map<std::string, std::vector<edge>> held;
        for (const auto& name : reachable) {
            const auto found = complex_types_.find(name);
            if (found == complex_types_.end())
                continue;
            std::vector<edge> targets;
            std::vector<edge> by_value;
            std::set<std::string> columns;
            for (node* m : column_members(name))
                columns.insert(m->get("name", "None"));
            for (node* element : children_of(found->second).first)
                for (node* field : expand(element)) {
                    const auto target = nested_target(field);
                    if (!is_complex(target))
                        continue;
                    const auto field_name = field->get("name", "None");
                    targets.emplace_back(snake(field_name), *target);
                    if (!columns.contains(field_name))
                        by_value.emplace_back(snake(field_name), *target);
                }
            edges[name] = std::move(targets);
            held[name] = std::move(by_value);
        }

        std::set<cut_edge> cut;
        std::vector<std::string> ordered;
        std::unordered_set<std::string> done;
        std::unordered_set<std::string> stack;
        const auto visit = [&](this const auto& self,
                               const std::string& name) -> void {
            if (done.contains(name) || !complex_types_.contains(name))
                return;
            stack.insert(name);
            if (const auto h = held.find(name); h != held.end())
                for (const auto& [member, target] : h->second) {
                    if (stack.contains(target)) {
                        cut.insert({name, member, target});
                        continue;
                    }
                    self(target);
                }
            stack.erase(name);
            done.insert(name);
            ordered.push_back(name);
        };
        visit(root_type);
        for (const auto& name : reachable) {
            stack.clear();
            visit(name);
        }
        cut_members_ = std::move(cut);

        std::unordered_set<std::string> used;
        const auto mark = [&](this const auto& self,
                              const std::string& name) -> void {
            if (used.contains(name) || !complex_types_.contains(name))
                return;
            used.insert(name);
            if (const auto e = edges.find(name); e != edges.end())
                for (const auto& [member, target] : e->second)
                    self(target);
        };
        mark(root_type);
        used_types_ = std::move(used);
        return ordered;
    }

    maybe spellings_of(const std::string& owner, node* element) {
        if (inline_enum_.contains(element))
            return snake(owner) + "_" + snake(element->get("name", "None")) +
                "_spellings";
        const auto declared = element->get("type");
        if (declared && enumerations_.contains(*declared))
            return snake(*declared) + "_spellings";
        return std::nullopt;
    }

    static std::string ops_name(const std::string& type_name,
        const std::string& member) {
        return type_name + "_" + member + "_ops";
    }

    static std::string vector_element_type(const std::string& cpp) {
        constexpr std::string_view prefix = "std::vector<";
        if (!cpp.starts_with(prefix))
            stop(cpp + " is not a vector of one element type");
        return cpp.substr(prefix.size(), cpp.size() - prefix.size() - 1);
    }

    struct column_set {
        std::map<std::string, std::vector<node*>> members;
        std::map<std::string, std::set<std::string>> names;
        std::map<std::string, std::string> struct_of;
        std::vector<std::pair<std::string, std::vector<std::string>>> kinds;
        std::map<std::string, std::string> holder;
    };

    column_set columns_of(const std::vector<std::string>& emitted,
        const std::map<std::string, std::string>& names) {
        column_set c;
        std::vector<std::string> columnar;
        for (const auto& type_name : emitted) {
            auto alternatives = column_members(type_name);
            if (alternatives.empty())
                continue;
            columnar.push_back(type_name);
            c.members[type_name] = std::move(alternatives);
        }
        for (const auto& [t, members] : c.members)
            for (node* m : members)
                c.names[t].insert(m->get("name", "None"));
        using sharing = std::vector<std::pair<std::string, strings>>;
        std::vector<std::pair<std::set<std::string>, sharing>> groups;
        for (const auto& type_name : columnar) {
            strings kinds;
            for (node* m : c.members[type_name]) {
                const auto k = nested_target(m).value_or("None");
                if (std::ranges::find(kinds, k) == kinds.end())
                    kinds.push_back(k);
            }
            const std::set<std::string> key(kinds.begin(), kinds.end());
            auto g = std::ranges::find_if(groups,
                [&](const auto& e) { return e.first == key; });
            if (g == groups.end()) {
                groups.emplace_back(key, sharing{});
                g = std::prev(groups.end());
            }
            g->second.emplace_back(type_name, std::move(kinds));
        }
        for (const auto& [key, members] : groups) {
            const auto owner = std::ranges::min_element(members,
                [&](const auto& a, const auto& b) {
                    const auto& na = names.at(a.first);
                    const auto& nb = names.at(b.first);
                    return std::pair(na.size(), na) < std::pair(nb.size(), nb);
                });
            const auto st = names.at(owner->first) + "_kind_columns";
            if (auto k = std::ranges::find_if(c.kinds,
                    [&](const auto& e) { return e.first == st; });
                k != c.kinds.end())
                k->second = members.front().second;
            else
                c.kinds.emplace_back(st, members.front().second);
            for (const auto& [type_name, kinds] : members)
                c.struct_of[type_name] = st;
        }
        for (const auto& type_name : emitted) {
            const auto own = c.names.find(type_name);
            for (node* element :
                children_of(complex_types_.at(type_name)).first)
                for (node* field : expand(element)) {
                    const auto target = nested_target(field);
                    if (!target || !c.struct_of.contains(*target))
                        continue;
                    if (own != c.names.end() &&
                        own->second.contains(field->get("name", "None")))
                        continue;
                    const auto& st = c.struct_of.at(*target);
                    const auto h = c.holder.try_emplace(type_name, st).first;
                    if (h->second != st)
                        stop(type_name + " holds types with two sets of "
                            "columns");
                }
        }
        return c;
    }

    struct member_declaration_result {
        std::string member;
        std::string kind;
        std::string cpp;
        bool repeated = false;
    };

    member_declaration_result member_declaration(node* element,
        const std::set<std::string, std::less<>>& exact_decimals) {
        const auto name = element->get("name", "None");
        const auto member = snake(name);
        const bool repeated = element->get("maxOccurs", "1") != "1";
        const auto s = field_shape(element);
        if (s.kind == "nested")
            return {member, "nested", "", repeated};
        std::string kind;
        std::string cpp;
        if (s.kind == "enumeration") {
            kind = "enumeration";
            cpp = "std::int16_t";
        } else {
            kind = s.kind;
            cpp = cpp_of(kind);
        }
        if (s.kind == "real" &&
            (exact_decimals.contains("*") || exact_decimals.contains(name))) {
            kind = "decimal";
            cpp = cpp_of("decimal");
        }
        if (repeated)
            return {member, kind, "std::vector<" + cpp + ">", true};
        return {member, kind, cpp, false};
    }

    std::map<std::string, std::string> cpp_names(
        const std::vector<std::string>& ordered) {
        std::map<std::string, std::string> names;
        std::set<std::string> taken;
        for (const auto& type_name : ordered) {
            auto candidate = snake(type_name);
            while (taken.contains(candidate) || reserved.contains(candidate))
                candidate = "record_" + candidate;
            taken.insert(candidate);
            names[type_name] = candidate;
        }
        return names;
    }

    std::string target_of(node* field) {
        if (auto t = either(field->get("type"), std::nullopt))
            return *t;
        if (const auto n = inline_name_.find(field); n != inline_name_.end())
            return n->second;
        return field->get("name", "None");
    }

    std::set<std::pair<std::string, std::string>> shadowed_types(
        const std::vector<std::string>& ordered,
        const std::map<std::string, std::string>& names) {
        std::vector<std::pair<std::string, std::vector<std::pair<std::string,
            maybe>>>> fields;
        for (const auto& type_name : ordered) {
            auto [elements, attributes] =
                children_of(complex_types_.at(type_name));
            std::vector<std::pair<std::string, maybe>> held;
            for (node* a : attributes)
                held.emplace_back(snake(a->get("name", "None")), std::nullopt);
            for (node* element : elements)
                for (node* field : expand(element)) {
                    const auto name = field->get("name");
                    if (!name || name->empty())
                        continue;
                    held.emplace_back(snake(*name), target_of(field));
                }
            fields.emplace_back(type_name, std::move(held));
        }
        std::set<std::pair<std::string, std::string>> shadowed;
        for (const auto& [holder, held] : fields)
            for (const auto& [member, target] : held) {
                if (!target || !names.contains(*target))
                    continue;
                const auto& name = names.at(*target);
                if (std::ranges::any_of(held,
                        [&](const auto& m) { return m.first == name; }))
                    shadowed.emplace(holder, *target);
            }
        return shadowed;
    }

    project project_;
    std::deque<node> nodes_;
    std::unordered_map<std::string, node*> complex_types_;
    std::unordered_map<std::string, strings> enumerations_;
    std::unordered_map<std::string, node*> simple_types_;
    std::unordered_map<std::string, node*> elements_;
    std::unordered_map<const node*, std::string> inline_name_;
    std::unordered_map<std::string, std::vector<node*>> substitutes_;
    std::unordered_map<std::string, node*> groups_;
    std::unordered_map<std::string, node*> globals_;
    std::set<cut_edge> cut_members_;
    std::unordered_set<std::string> used_types_;
    std::unordered_map<const node*, strings> inline_enum_;
    std::unordered_set<const node*> choice_children_;
};

std::string boolean(bool b) { return b ? "true" : "false"; }

output generator::render(const family& f) {
    const std::string root_type(f.root_type);
    const std::set<std::string, std::less<>> exact_decimals(
        f.exact_decimals.begin(), f.exact_decimals.end());
    const bool every_real = exact_decimals.contains("*");
    const auto ordered = order_types(root_type);
    const auto names = cpp_names(ordered);
    std::set<std::pair<std::string, std::string>> cut;
    for (const auto& c : cut_members_)
        cut.emplace(c.holder, c.member);
    std::vector<std::string> emitted;
    for (const auto& name : ordered)
        if (used_types_.contains(name))
            emitted.push_back(name);
    const auto shadowed = shadowed_types(ordered, names);
    const auto columns = columns_of(emitted, names);

    const auto columns_offset = [&](const std::string& type_name,
                                    const std::string& target) -> maybe {
        if (!columns.struct_of.contains(target))
            return std::nullopt;
        return "std::uint32_t(offsetof(" + names.at(type_name) +
            ", kind_columns))";
    };
    const auto column_names = [&](const std::string& type_name)
        -> const std::set<std::string>* {
        const auto n = columns.names.find(type_name);
        return n == columns.names.end() ? nullptr : &n->second;
    };
    const auto is_column = [&](const std::string& type_name, node* field) {
        const auto* own = column_names(type_name);
        return own != nullptr && own->contains(field->get("name", "None"));
    };

    std::vector<std::tuple<std::string, std::string, std::string>> unresolved;
    strings declarations;
    strings declares;
    strings column_declarations;
    for (const auto& [st, kinds] : columns.kinds) {
        strings lines{"struct " + st + " {"};
        for (const auto& kind : kinds)
            lines.push_back("    std::vector<struct " + names.at(kind) + "> " +
                names.at(kind) + "_rows;");
        lines.emplace_back("};");
        column_declarations.push_back(joined(lines, "\n"));
    }
    for (const auto& type_name : emitted) {
        node* complex_type = complex_types_.at(type_name);
        auto [elements, attributes] = children_of(complex_type);
        const auto& self = names.at(type_name);
        declares.push_back("struct " + self + ";");
        strings body{"struct " + self + " {"};
        std::size_t field_count = 0;
        auto own_kind = content_kind(complex_type);
        if (own_kind == "real" && every_real)
            own_kind = "decimal";
        if (own_kind) {
            const auto own_cpp = cpp_of(*own_kind);
            body.push_back("    " + own_cpp + " value" +
                std::string(cpp_zero(own_cpp)) + ";");
            ++field_count;
        }
        for (node* a : attributes) {
            body.push_back("    std::string " + snake(a->get("name", "None")) +
                ";");
            ++field_count;
        }
        for (node* element : elements)
            for (node* field : expand(element)) {
                if (is_column(type_name, field)) {
                    ++field_count;
                    continue;
                }
                auto d = member_declaration(field, exact_decimals);
                if (d.kind == "nested") {
                    const auto target = target_of(field);
                    if (!names.contains(target)) {
                        unresolved.emplace_back(type_name, d.member, target);
                        continue;
                    }
                    d.cpp = names.at(target);
                    if (cut.contains({type_name, d.member})) {
                        body.push_back("    std::vector<std::uint32_t> " +
                            d.member + ";");
                        body.push_back("    std::vector<struct " + d.cpp +
                            "> " + d.member + "_arena;");
                        ++field_count;
                        continue;
                    }
                    if (shadowed.contains({type_name, target}))
                        d.cpp = "struct " + d.cpp;
                    if (d.repeated)
                        d.cpp = "std::vector<" + d.cpp + ">";
                }
                body.push_back("    " + d.cpp + " " + d.member +
                    std::string(cpp_zero(d.cpp)) + ";");
                ++field_count;
            }
        if (columns.members.contains(type_name))
            body.emplace_back("    std::uint32_t kind_index = 0;");
        if (const auto h = columns.holder.find(type_name);
            h != columns.holder.end())
            body.push_back("    " + h->second + " kind_columns;");
        if (is_residual(complex_type)) {
            body.emplace_back("    std::vector<residual_element> extras;");
            ++field_count;
        }
        body.emplace_back("};");
        const auto words = (field_count + mask_word_bits - 1) / mask_word_bits;
        body.insert(body.begin() + 1, words <= 1
            ? std::string("    std::uint64_t present = 0;")
            : "    std::uint64_t present[" + std::to_string(words) +
                "] = {0};");
        declarations.push_back(joined(body, "\n"));
    }

    if (!unresolved.empty()) {
        const auto& [holder, member, target] = unresolved.front();
        stop(holder + "." + member + " holds " + target +
            ", which is not a complexType this generator read");
    }

    std::set<std::string> column_kinds;
    for (const auto& [st, kinds] : columns.kinds)
        column_kinds.insert(kinds.begin(), kinds.end());
    strings indexed_targets;
    for (const auto& name : emitted)
        if (column_kinds.contains(name) ||
            std::ranges::any_of(cut_members_,
                [&](const cut_edge& c) { return c.target == name; }))
            indexed_targets.push_back(name);
    std::string indexed_declarations;
    if (!indexed_targets.empty()) {
        strings later;
        for (const auto& name : indexed_targets)
            later.push_back("extern const record_type " + names.at(name) +
                "_record;");
        indexed_declarations =
            "// A row that indexes an arena or a column names a record that is\n"
            "// defined later in this file: the edge that closes a type cycle is a\n"
            "// back edge, and an alternative held in a column orders nothing.\n" +
            joined(later, "\n") + "\n\n";
    }

    strings tables;
    std::vector<std::pair<std::string, std::string>> repeated_members;
    std::vector<std::pair<std::string, std::string>> repeated_arena;
    std::vector<std::pair<std::string, std::string>> column_ops;
    for (const auto& type_name : emitted) {
        node* complex_type = complex_types_.at(type_name);
        auto [elements, attributes] = children_of(complex_type);
        const auto& self = names.at(type_name);
        strings rows;
        std::size_t bit = 0;
        auto own_kind = content_kind(complex_type);
        if (own_kind == "real" && every_real)
            own_kind = "decimal";
        const auto own_enum = content_enumeration(complex_type);
        if (own_kind) {
            const auto tokens = own_enum && !own_enum->empty()
                ? snake(*own_enum) + "_spellings" : std::string("nullptr");
            rows.push_back("    {\"\", 0u, field_kind::" + *own_kind +
                ", std::uint32_t(offsetof(" + self + ", value)), " +
                std::to_string(bit) + ", false, false, " + tokens +
                ", nullptr, " + scalar.at(*own_kind) + ", nullptr},");
            ++bit;
        }
        for (node* a : attributes) {
            const auto name = a->get("name", "None");
            const bool required = a->get("use") == "required";
            rows.push_back("    {\"" + name + "\", " +
                std::to_string(hash_name(name)) + "u, field_kind::text, "
                "std::uint32_t(offsetof(" + self + ", " + snake(name) +
                ")), " + std::to_string(bit) + ", true, " + boolean(required) +
                ", nullptr, nullptr, scalar_kind::text, nullptr},");
            ++bit;
        }
        for (node* outer : elements)
            for (node* element : expand(outer)) {
                const auto name = element->get("name", "None");
                const auto hash = std::to_string(hash_name(name));
                auto d = member_declaration(element, exact_decimals);
                const bool required = element->get("minOccurs", "1") != "0" &&
                    !choice_children_.contains(element);
                const auto sc = scalar.contains(d.kind) ? scalar.at(d.kind)
                    : std::string("scalar_kind::text");
                if (is_column(type_name, element)) {
                    const auto kind_name = *nested_target(element);
                    const auto& st = columns.struct_of.at(type_name);
                    const auto& kind = names.at(kind_name);
                    const auto ops_instance = ops_name(st, kind + "_rows");
                    if (std::ranges::none_of(column_ops,
                            [&](const auto& o) { return o.first == ops_instance; }))
                        column_ops.emplace_back(ops_instance, kind);
                    rows.push_back("    {\"" + name + "\", " + hash +
                        "u, field_kind::column_nested, std::uint32_t(offsetof(" +
                        self + ", kind_index)), " + std::to_string(bit) +
                        ", false, false, nullptr, &" + kind +
                        "_record, scalar_kind::text, &" + ops_instance +
                        ", std::uint32_t(offsetof(" + st + ", " + kind +
                        "_rows))},");
                    ++bit;
                    continue;
                }
                if (d.kind == "nested") {
                    const auto target_name = target_of(element);
                    if (!names.contains(target_name))
                        continue;
                    const auto& target = names.at(target_name);
                    const auto member = snake(name);
                    const auto held_at = columns_offset(type_name, target_name);
                    if (cut.contains({type_name, member})) {
                        const auto ops_instance = ops_name(self,
                            member + "_arena");
                        repeated_arena.emplace_back(ops_instance, target);
                        rows.push_back("    {\"" + name + "\", " + hash +
                            "u, field_kind::indexed_nested, "
                            "std::uint32_t(offsetof(" + self + ", " + member +
                            ")), " + std::to_string(bit) + ", false, " +
                            boolean(required) + ", nullptr, &" + target +
                            "_record, scalar_kind::text, &" + ops_instance +
                            ", std::uint32_t(offsetof(" + self + ", " + member +
                            "_arena)), " + boolean(!d.repeated) +
                            (held_at ? ", " + *held_at : std::string()) + "},");
                        ++bit;
                        continue;
                    }
                    const std::string field_kind = d.repeated
                        ? "repeated_nested" : "nested";
                    std::string ops = "nullptr";
                    if (d.repeated) {
                        ops = "&" + ops_name(self, member);
                        repeated_members.emplace_back(ops_name(self, member),
                            target);
                    }
                    rows.push_back("    {\"" + name + "\", " + hash +
                        "u, field_kind::" + field_kind +
                        ", std::uint32_t(offsetof(" + self + ", " + member +
                        ")), " + std::to_string(bit) + ", false, " +
                        boolean(required) + ", nullptr, &" + target +
                        "_record, scalar_kind::text, " + ops +
                        (held_at ? ", no_arena, false, " + *held_at
                                 : std::string()) + "},");
                } else {
                    const auto field_kind = d.repeated
                        ? std::string("repeated_leaf") : d.kind;
                    const auto tokens = d.kind == "enumeration"
                        ? spellings_of(type_name, element)
                              .value_or("nullptr")
                        : std::string("nullptr");
                    std::string ops = "nullptr";
                    if (d.repeated) {
                        ops = "&" + ops_name(self, d.member);
                        repeated_members.emplace_back(ops_name(self, d.member),
                            vector_element_type(d.cpp));
                    }
                    rows.push_back("    {\"" + name + "\", " + hash +
                        "u, field_kind::" + field_kind +
                        ", std::uint32_t(offsetof(" + self + ", " + d.member +
                        ")), " + std::to_string(bit) + ", false, " +
                        boolean(required) + ", " + tokens + ", nullptr, " + sc +
                        ", " + ops + "},");
                }
                ++bit;
            }
        if (is_residual(complex_type)) {
            rows.push_back("    {\"\", 0u, field_kind::residual, "
                "std::uint32_t(offsetof(" + self + ", extras)), " +
                std::to_string(bit) + ", false, false, nullptr, nullptr, "
                "scalar_kind::text, nullptr},");
            ++bit;
        }
        if (bit > max_mask_bits)
            stop(type_name + " has " + std::to_string(bit) +
                " fields; a table holds " + std::to_string(max_mask_bits) +
                " at most");
        const std::string text_field = own_kind ? "0" : "no_text";
        tables.push_back("const field " + self + "_fields[] = {\n" +
            joined(rows, "\n") + "\n};\nconst record_type " + self +
            "_record{\"" + type_name + "\", " + self + "_fields, " +
            std::to_string(bit) + ", " + text_field + "};");
    }

    strings ops;
    std::vector<std::pair<std::string, std::string>> all_ops = repeated_arena;
    all_ops.insert(all_ops.end(), column_ops.begin(), column_ops.end());
    all_ops.insert(all_ops.end(), repeated_members.begin(),
        repeated_members.end());
    for (const auto& [instance, arguments] : all_ops)
        ops.push_back("constexpr vector_ops " + instance + "{&append_to<" +
            arguments + ">, &count_of<" + arguments + ">, &at_of<" +
            arguments + ">};");

    const auto spelling_line = [](const std::string& name,
                                   const strings& values) {
        strings quoted_values;
        for (const auto& v : values)
            quoted_values.push_back("\"" + v + "\"");
        return "const char* " + name + "[] = {" + joined(quoted_values, ", ") +
            ", nullptr};";
    };
    strings spellings;
    std::set<std::string> emitted_spellings;
    for (const auto& type_name : ordered) {
        const auto base = content_enumeration(complex_types_.at(type_name));
        if (base && emitted_spellings.insert(*base).second)
            spellings.push_back(spelling_line(snake(*base) + "_spellings",
                enumerations_.at(*base)));
    }
    for (const auto& type_name : ordered)
        for (node* element : children_of(complex_types_.at(type_name)).first)
            for (node* field : expand(element)) {
                (void)field_shape(field);
                const auto declared = field->get("type");
                if (const auto e = inline_enum_.find(field);
                    e != inline_enum_.end()) {
                    const auto name = *spellings_of(type_name, field);
                    if (emitted_spellings.insert(name).second)
                        spellings.push_back(spelling_line(name, e->second));
                } else if (declared && enumerations_.contains(*declared) &&
                    emitted_spellings.insert(*declared).second) {
                    spellings.push_back(spelling_line(
                        snake(*declared) + "_spellings",
                        enumerations_.at(*declared)));
                }
            }

    const std::string name_space(f.name_space);
    const std::string include(f.include);
    const auto guard = guard_of(include);
    const auto exported = project_.export_macro.empty() ? std::string()
        : std::string(project_.export_macro) + " ";
    const auto accessor = exported + "auto " + std::string(f.accessor);
    const auto banner = heading(f.schema_names);

    output out;
    out.header = banner + "#ifndef " + guard + "\n#define " + guard + "\n\n"
        "#include \"" + std::string(project_.engine_include) + "\"\n"
        "#include \"" + std::string(project_.decimal_include) + "\"\n\n"
        "#include <cstdint>\n#include <string>\n#include <vector>\n\n"
        "namespace " + name_space + " {\n\n" + joined(declares, "\n") +
        "\n\n" + (column_declarations.empty() ? std::string()
            : joined(column_declarations, "\n") + "\n\n") +
        joined(declarations, "\n") + "\n\n" + accessor +
        "() -> const document_type&;\n\n}\n\n#endif\n";
    out.source = banner + "#include \"" + include + "\"\n\n"
        "#include <cstddef>\n\n"
        "namespace " + name_space + " {\n\n"
        "namespace {\n\n"
        "constexpr std::uint8_t no_text = 0xFF;\n\n"
        "template <typename T>\n"
        "auto append_to(void* container, void** slot) -> void {\n"
        "    auto* held = static_cast<std::vector<T>*>(container);\n"
        "    held->emplace_back();\n"
        "    *slot = &held->back();\n"
        "}\n\n"
        "template <typename T>\n"
        "auto count_of(const void* container) -> std::size_t {\n"
        "    return static_cast<const std::vector<T>*>(container)->size();\n"
        "}\n\n"
        "template <typename T>\n"
        "auto at_of(const void* container, std::size_t index) -> const void* {\n"
        "    return &(*static_cast<const std::vector<T>*>(container))[index];\n"
        "}\n\n" +
        joined(spellings, "\n") + "\n\n" + joined(ops, "\n") + "\n\n" +
        indexed_declarations + joined(tables, "\n") + "\n\n}\n\n" + accessor +
        "() -> const document_type& {\n"
        "    static const document_type doc{\"" + std::string(f.root_element) +
        "\", &" + names.at(root_type) + "_record};\n"
        "    return doc;\n"
        "}\n\n}\n";
    return out;
}

}

output generate(std::span<const std::string> schemas, const family& f,
    const project& p) {
    generator g(schemas, p);
    if (!g.has_complex_type(std::string(f.root_type)))
        throw error("no complexType " + std::string(f.root_type));
    return g.render(f);
}

}
