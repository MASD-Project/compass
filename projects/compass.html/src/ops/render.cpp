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
#include "compass.html/ops/render.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "compass.html/ops/anchors.hpp"
#include "compass.org/ops/document_fields.hpp"
#include "objects.hpp"

namespace compass::html::ops {

using namespace org::types;

namespace {

constexpr std::uint32_t none = 0xFFFF'FFFF;

void escape(std::string& out, std::string_view text) {
    for (const char c : text) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        default: out += c; break;
        }
    }
}

/**
 * @brief Percent-encodes every byte but the unreserved ones of RFC 3986,
 * and the slash when @p keep_slash.
 */
std::string percent_encoded(std::string_view text, bool keep_slash) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string r;
    for (const char ch : text) {
        const auto c = static_cast<unsigned char>(ch);
        const bool unreserved = (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
            c == '.' || c == '_' || c == '~' || (keep_slash && c == '/');
        if (unreserved) {
            r += ch;
        } else {
            r += '%';
            r += hex[c >> 4];
            r += hex[c & 0xF];
        }
    }
    return r;
}

void replace_all(std::string& s, std::string_view from, std::string_view to) {
    for (auto at = s.find(from); at != std::string::npos;
         at = s.find(from, at + to.size()))
        s.replace(at, from.size(), to);
}

std::string document_url(std::string_view link_template,
    std::string_view id, std::string_view anchor) {
    std::string url(link_template);
    if (anchor.empty())
        replace_all(url, "#{anchor}", "");
    replace_all(url, "{id}", percent_encoded(id, false));
    replace_all(url, "{anchor}", percent_encoded(anchor, false));
    return url;
}

/**
 * @brief The path of @p link from the root, given the directory of the
 * document it is written in, or empty when it is absolute or leaves the
 * root.
 */
std::string resolved_path(std::string_view directory, std::string_view link) {
    if (link.starts_with('/') || link.starts_with('~') || link.empty())
        return {};
    std::vector<std::string_view> parts;
    const auto push = [&](std::string_view path) {
        while (!path.empty()) {
            const auto slash = path.find('/');
            const auto part = path.substr(0, slash);
            path = slash == std::string_view::npos ? std::string_view{}
                                                   : path.substr(slash + 1);
            if (part.empty() || part == ".")
                continue;
            if (part == "..") {
                if (parts.empty())
                    return false;
                parts.pop_back();
            } else {
                parts.push_back(part);
            }
        }
        return true;
    };
    if (!push(directory) || !push(link))
        return {};
    std::string r;
    for (const auto part : parts) {
        if (!r.empty())
            r += '/';
        r += part;
    }
    return r;
}

bool is_image(std::string_view path) {
    const auto dot = path.rfind('.');
    if (dot == std::string_view::npos)
        return false;
    std::string ext(path.substr(dot + 1));
    for (auto& c : ext)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" ||
        ext == "svg" || ext == "webp";
}

/**
 * @brief @p name when it is safe in a class attribute, else empty.
 */
std::string_view class_safe(std::string_view name) {
    const bool safe = !name.empty() &&
        std::all_of(name.begin(), name.end(), [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '+';
        });
    return safe ? name : std::string_view{};
}

std::string_view kind_name(element_kind k) noexcept {
    switch (k) {
    case element_kind::document: return "org-data";
    case element_kind::section: return "section";
    case element_kind::heading: return "headline";
    case element_kind::inlinetask: return "inlinetask";
    case element_kind::plain_list: return "plain-list";
    case element_kind::item: return "item";
    case element_kind::table: return "table";
    case element_kind::table_el: return "table.el";
    case element_kind::table_row: return "table-row";
    case element_kind::table_rule: return "table-rule";
    case element_kind::drawer: return "drawer";
    case element_kind::property_drawer: return "property-drawer";
    case element_kind::node_property: return "node-property";
    case element_kind::footnote_definition: return "footnote-definition";
    case element_kind::quote_block: return "quote-block";
    case element_kind::center_block: return "center-block";
    case element_kind::special_block: return "special-block";
    case element_kind::dynamic_block: return "dynamic-block";
    case element_kind::verse_block: return "verse-block";
    case element_kind::src_block: return "src-block";
    case element_kind::example_block: return "example-block";
    case element_kind::export_block: return "export-block";
    case element_kind::comment_block: return "comment-block";
    case element_kind::paragraph: return "paragraph";
    case element_kind::keyword: return "keyword";
    case element_kind::babel_call: return "babel-call";
    case element_kind::planning: return "planning";
    case element_kind::clock: return "clock";
    case element_kind::diary_sexp: return "diary-sexp";
    case element_kind::comment: return "comment";
    case element_kind::fixed_width: return "fixed-width";
    case element_kind::horizontal_rule: return "horizontal-rule";
    case element_kind::latex_environment: return "latex-environment";
    }
    return "unknown";
}

std::string_view kind_name(object_kind k) noexcept {
    switch (k) {
    case object_kind::bold: return "bold";
    case object_kind::italic: return "italic";
    case object_kind::underline: return "underline";
    case object_kind::strike_through: return "strike-through";
    case object_kind::code: return "code";
    case object_kind::verbatim: return "verbatim";
    case object_kind::link: return "link";
    case object_kind::timestamp: return "timestamp";
    case object_kind::footnote_reference: return "footnote-reference";
    case object_kind::inline_src_block: return "inline-src-block";
    case object_kind::inline_babel_call: return "inline-babel-call";
    case object_kind::macro: return "macro";
    case object_kind::export_snippet: return "export-snippet";
    case object_kind::latex_fragment: return "latex-fragment";
    case object_kind::entity: return "entity";
    case object_kind::statistics_cookie: return "statistics-cookie";
    case object_kind::radio_target: return "radio-target";
    case object_kind::target: return "target";
    case object_kind::line_break: return "line-break";
    case object_kind::subscript: return "subscript";
    case object_kind::superscript: return "superscript";
    case object_kind::table_cell: return "table-cell";
    case object_kind::citation: return "citation";
    case object_kind::citation_reference: return "citation-reference";
    }
    return "unknown";
}

/**
 * @brief The rows of a detail table by the element or object they belong
 * to: index[e] is the row of e, or none.
 */
template <typename Column>
std::vector<std::uint32_t> index_by_owner(std::size_t owners,
    const Column& column) {
    std::vector<std::uint32_t> r(owners, none);
    for (std::uint32_t i = 0; i < column.size(); ++i)
        r[column.values[i].value] = i;
    return r;
}

/**
 * @brief Removes the comma org puts before a line of a block body that
 * starts with * or #+.
 */
std::string unescaped_body(std::string_view body) {
    std::string r;
    r.reserve(body.size());
    bool line_start = true;
    for (std::size_t i = 0; i < body.size(); ++i) {
        if (line_start) {
            auto j = i;
            while (j < body.size() && (body[j] == ' ' || body[j] == '\t'))
                ++j;
            const auto rest = body.substr(j);
            if (rest.starts_with(",*") || rest.starts_with(",#+")) {
                r += body.substr(i, j - i);
                i = j + 1;
            }
        }
        r += body[i];
        line_start = body[i] == '\n';
    }
    return r;
}

class renderer final {
public:
    renderer(std::string_view source, const tables_view& t,
        std::string_view path, const types::render_config& config,
        const types::link_resolver& links)
        : source_(source), t_(t), config_(config), links_(links),
          anchors_(heading_anchors(source, t)),
          heading_of_(index_by_owner(t.elements.size(), t.headings.elements)),
          block_of_(index_by_owner(t.elements.size(), t.blocks.elements)),
          list_of_(index_by_owner(t.elements.size(), t.lists.elements)),
          item_of_(index_by_owner(t.elements.size(), t.items.elements)),
          keyword_of_(index_by_owner(t.elements.size(), t.keywords.elements)) {
        const auto slash = path.rfind('/');
        if (slash != std::string_view::npos)
            directory_ = path.substr(0, slash);
        for (std::uint32_t i = 0; i < t.headings.size(); ++i) {
            const heading_id h{i};
            const auto id = detail::heading_property(source, t, h, "ID");
            if (!id.empty())
                local_.emplace(std::string("id:") + std::string(id),
                    anchors_[i]);
            const auto custom = detail::heading_property(source, t, h,
                "CUSTOM_ID");
            if (!custom.empty())
                local_.emplace("#" + std::string(custom), anchors_[i]);
            std::string title;
            detail::append_plain_text(title, source, t, t.headings.titles[h],
                detail::own_objects(t, t.headings.elements[h]));
            local_.emplace("*" + title, anchors_[i]);
        }
    }

    types::rendered_document run() {
        types::rendered_document r;
        r.id = document_property("ID");
        r.title = keyword("title");
        r.description = keyword("description");
        if (t_.elements.size() > 0)
            element(element_id{0});
        r.body = std::move(out_);
        r.unsupported = std::move(unsupported_);
        return r;
    }

private:
    std::string document_property(std::string_view key) const {
        const auto p = org::ops::find_document_property(t_, key);
        if (p.is_none())
            return {};
        return std::string(
            org::ops::trimmed(source_, t_.properties.values[p]).of(source_));
    }

    std::string keyword(std::string_view key) const {
        const auto k = org::ops::find_keyword(t_, key);
        if (k.is_none())
            return {};
        return std::string(
            org::ops::trimmed(source_, t_.keywords.values[k]).of(source_));
    }

    std::string_view name(name_id n) const {
        return n.is_none() ? std::string_view{} : org::ops::name_text(t_, n);
    }

    void text(std::string_view s) { escape(out_, s); }

    void text(source_range r) { escape(out_, r.of(source_)); }

    void children(element_id e) {
        const auto end = t_.elements.subtree_ends[e];
        for (auto c = e.next(); c < end; c = t_.elements.subtree_ends[c])
            element(c);
    }

    /**
     * @brief Whether @p e is in the section before the first heading, whose
     * keywords make the document's header.
     */
    bool in_zeroth_section(element_id e) const {
        const auto parent = t_.elements.parents[e];
        if (parent.is_none())
            return false;
        if (t_.elements.kinds[parent] != element_kind::section)
            return parent.value == 0;
        return t_.elements.parents[parent].value == 0;
    }

    void header() {
        bool open = false;
        for (std::uint32_t i = 0; i < t_.keywords.size(); ++i) {
            const keyword_id k{i};
            const auto key = name(t_.keywords.names[k]);
            if (key == "title" || key == "description" ||
                !in_zeroth_section(t_.keywords.elements[k]))
                continue;
            if (!open)
                out_ += "<dl class=\"org-keywords\">\n";
            open = true;
            out_ += "<dt>";
            text(key);
            out_ += "</dt><dd>";
            text(org::ops::trimmed(source_, t_.keywords.values[k]));
            out_ += "</dd>\n";
        }
        if (open)
            out_ += "</dl>\n";
    }

    void unsupported(element_id e) {
        const auto kind = kind_name(t_.elements.kinds[e]);
        unsupported_.emplace_back(kind);
        out_ += "<div class=\"org-unsupported\" data-org-kind=\"";
        text(kind);
        out_ += "\"><pre>";
        text(org::ops::trimmed(source_, t_.elements.ranges[e]));
        out_ += "</pre></div>\n";
    }

    void element(element_id e) {
        switch (t_.elements.kinds[e]) {
        case element_kind::document:
            header();
            children(e);
            return;
        case element_kind::section:
            children(e);
            return;
        case element_kind::heading:
            heading(e);
            return;
        case element_kind::plain_list:
            plain_list(e);
            return;
        case element_kind::item:
            children(e);
            return;
        case element_kind::table:
            table(e);
            return;
        case element_kind::paragraph:
            out_ += "<p>";
            own_objects(e);
            out_ += "</p>\n";
            return;
        case element_kind::src_block:
        case element_kind::example_block:
        case element_kind::export_block:
            literal_block(e);
            return;
        case element_kind::quote_block:
            out_ += "<blockquote>\n";
            children(e);
            out_ += "</blockquote>\n";
            return;
        case element_kind::center_block:
            out_ += "<div class=\"org-center\">\n";
            children(e);
            out_ += "</div>\n";
            return;
        case element_kind::special_block: {
            const auto b = block_of_[e.value];
            out_ += "<div class=\"org-special-block";
            if (b != none) {
                const auto type = class_safe(name(t_.blocks.names[block_id{b}]));
                if (!type.empty()) {
                    out_ += " org-";
                    out_ += type;
                }
            }
            out_ += "\">\n";
            children(e);
            out_ += "</div>\n";
            return;
        }
        case element_kind::dynamic_block:
            out_ += "<div class=\"org-dynamic-block\">\n";
            children(e);
            out_ += "</div>\n";
            return;
        case element_kind::verse_block:
            out_ += "<p class=\"org-verse\">";
            own_objects(e);
            out_ += "</p>\n";
            return;
        case element_kind::keyword:
            if (in_zeroth_section(e))
                return;
            keyword_line(e);
            return;
        case element_kind::planning:
            out_ += "<p class=\"org-planning\"><span class=\"org-planning\">";
            text(org::ops::trimmed(source_, t_.elements.ranges[e]));
            out_ += "</span></p>\n";
            return;
        case element_kind::clock:
            out_ += "<p class=\"org-clock\"><span class=\"org-clock\">";
            text(org::ops::trimmed(source_, t_.elements.ranges[e]));
            out_ += "</span></p>\n";
            return;
        case element_kind::fixed_width:
            fixed_width(e);
            return;
        case element_kind::horizontal_rule:
            out_ += "<hr>\n";
            return;
        case element_kind::drawer:
        case element_kind::property_drawer:
        case element_kind::node_property:
        case element_kind::comment:
        case element_kind::comment_block:
        case element_kind::table_row:
        case element_kind::table_rule:
            return;
        case element_kind::inlinetask:
        case element_kind::table_el:
        case element_kind::footnote_definition:
        case element_kind::babel_call:
        case element_kind::diary_sexp:
        case element_kind::latex_environment:
            unsupported(e);
            return;
        }
        unsupported(e);
    }

    void heading(element_id e) {
        const heading_id h{heading_of_[e.value]};
        if (t_.headings.comments[h] == comment_state::commented)
            return;
        const auto tag_first = t_.headings.tag_firsts[h].value;
        const auto tag_last = h.next().value < t_.headings.size()
            ? t_.headings.tag_firsts[h.next()].value
            : static_cast<std::uint32_t>(t_.heading_tags.size());
        for (auto g = tag_first; g < tag_last; ++g)
            if (name(t_.heading_tags.names[tag_id{g}]) == "noexport")
                return;

        const auto level = t_.headings.levels[h];
        const auto tag = std::to_string(std::min<unsigned>(level + 1u, 6u));
        out_ += "<section class=\"org-heading level-" + std::to_string(level) +
            "\">\n<h" + tag + " id=\"";
        text(anchors_[h.value]);
        out_ += "\">";
        if (const auto todo = t_.headings.todos[h]; !todo.is_none()) {
            out_ += t_.headings.todo_types[h] == todo_type::done
                ? "<span class=\"org-todo org-done\">"
                : "<span class=\"org-todo\">";
            text(name(todo));
            out_ += "</span> ";
        }
        if (const auto p = t_.headings.priorities[h]; p != 0) {
            out_ += "<span class=\"org-priority\">[#";
            if (p >= 'A' && p <= 'Z')
                out_ += static_cast<char>(p);
            else
                out_ += std::to_string(p);
            out_ += "]</span> ";
        }
        objects(t_.headings.titles[h], detail::own_objects(t_, e));
        if (tag_first < tag_last) {
            out_ += " <span class=\"org-tags\">";
            for (auto g = tag_first; g < tag_last; ++g) {
                out_ += "<span class=\"org-tag\">";
                text(name(t_.heading_tags.names[tag_id{g}]));
                out_ += "</span>";
            }
            out_ += "</span>";
        }
        out_ += "</h" + tag + ">\n";
        children(e);
        out_ += "</section>\n";
    }

    void plain_list(element_id e) {
        const auto type = t_.lists.types[list_id{list_of_[e.value]}];
        const std::string_view tag = type == list_type::ordered ? "ol"
            : type == list_type::descriptive ? "dl" : "ul";
        out_ += "<";
        out_ += tag;
        out_ += ">\n";
        const auto end = t_.elements.subtree_ends[e];
        for (auto c = e.next(); c < end; c = t_.elements.subtree_ends[c]) {
            if (t_.elements.kinds[c] != element_kind::item) {
                element(c);
                continue;
            }
            const item_id i{item_of_[c.value]};
            if (type == list_type::descriptive) {
                out_ += "<dt>";
                objects(t_.items.tags[i], detail::own_objects(t_, c));
                out_ += "</dt><dd>";
            } else {
                out_ += "<li>";
            }
            checkbox(t_.items.checkboxes[i]);
            children(c);
            out_ += type == list_type::descriptive ? "</dd>\n" : "</li>\n";
        }
        out_ += "</";
        out_ += tag;
        out_ += ">\n";
    }

    void checkbox(org::types::checkbox c) {
        switch (c) {
        case org::types::checkbox::none: return;
        case org::types::checkbox::off:
            out_ += "<span class=\"org-checkbox org-off\">[ ]</span> ";
            return;
        case org::types::checkbox::on:
            out_ += "<span class=\"org-checkbox org-on\">[X]</span> ";
            return;
        case org::types::checkbox::partial:
            out_ += "<span class=\"org-checkbox org-partial\">[-]</span> ";
            return;
        }
    }

    void table(element_id e) {
        std::vector<element_id> rows;
        std::size_t first_rule = std::string::npos;
        const auto end = t_.elements.subtree_ends[e];
        for (auto c = e.next(); c < end; c = t_.elements.subtree_ends[c]) {
            if (t_.elements.kinds[c] == element_kind::table_rule) {
                if (first_rule == std::string::npos)
                    first_rule = rows.size();
                continue;
            }
            rows.push_back(c);
        }
        const auto header_rows =
            first_rule == std::string::npos || first_rule == rows.size()
            ? 0 : first_rule;
        out_ += "<table class=\"org-table\">\n";
        if (header_rows > 0) {
            out_ += "<thead>\n";
            for (std::size_t r = 0; r < header_rows; ++r)
                row(rows[r], "th");
            out_ += "</thead>\n";
        }
        out_ += "<tbody>\n";
        for (std::size_t r = header_rows; r < rows.size(); ++r)
            row(rows[r], "td");
        out_ += "</tbody>\n</table>\n";
    }

    void row(element_id e, std::string_view cell_tag) {
        out_ += "<tr>";
        const auto run = detail::own_objects(t_, e);
        for (auto o = run.first; o < run.last;
             o = t_.objects.subtree_ends[o]) {
            out_ += "<";
            out_ += cell_tag;
            out_ += ">";
            const auto c = t_.objects.contents[o];
            if (!c.empty())
                objects(c, detail::nested_objects(t_, o));
            out_ += "</";
            out_ += cell_tag;
            out_ += ">";
        }
        out_ += "</tr>\n";
    }

    void literal_block(element_id e) {
        const auto b = block_of_[e.value];
        if (b == none) {
            unsupported(e);
            return;
        }
        const block_id id{b};
        const auto body = unescaped_body(t_.blocks.bodies[id].of(source_));
        switch (t_.elements.kinds[e]) {
        case element_kind::src_block: {
            const auto language = class_safe(name(t_.blocks.names[id]));
            out_ += "<pre class=\"org-src\"><code";
            if (!language.empty()) {
                out_ += " class=\"language-";
                out_ += language;
                out_ += "\"";
            }
            out_ += ">";
            break;
        }
        case element_kind::export_block:
            if (name(t_.blocks.names[id]) != "html")
                return;
            out_ += "<pre class=\"org-export\"><code>";
            break;
        default:
            out_ += "<pre class=\"org-example\"><code>";
            break;
        }
        text(body);
        out_ += "</code></pre>\n";
    }

    void keyword_line(element_id e) {
        const auto k = keyword_of_[e.value];
        if (k == none) {
            unsupported(e);
            return;
        }
        const keyword_id id{k};
        out_ += "<p class=\"org-keyword\"><span class=\"org-keyword-name\">";
        text(name(t_.keywords.names[id]));
        out_ += "</span> ";
        text(org::ops::trimmed(source_, t_.keywords.values[id]));
        out_ += "</p>\n";
    }

    void fixed_width(element_id e) {
        out_ += "<pre class=\"org-fixed-width\">";
        auto rest = org::ops::trimmed(source_, t_.elements.ranges[e])
            .of(source_);
        bool first = true;
        while (!rest.empty()) {
            const auto nl = rest.find('\n');
            auto line = rest.substr(0, nl);
            rest = nl == std::string_view::npos ? std::string_view{}
                                                : rest.substr(nl + 1);
            const auto colon = line.find(':');
            line = colon == std::string_view::npos ? line
                                                   : line.substr(colon + 1);
            if (line.starts_with(' '))
                line.remove_prefix(1);
            if (!first)
                out_ += '\n';
            first = false;
            text(line);
        }
        out_ += "</pre>\n";
    }

    void own_objects(element_id e) {
        objects(t_.elements.contents[e], detail::own_objects(t_, e));
    }

    /**
     * @brief Renders the text of @p contents, whose objects are @p run.
     */
    void objects(source_range contents, detail::object_run run) {
        auto cursor = contents.begin;
        for (auto o = run.first; o < run.last;
             o = t_.objects.subtree_ends[o]) {
            const auto r = detail::without_post_blanks(source_, t_, o);
            if (r.begin > cursor)
                text(source_.substr(cursor, r.begin - cursor));
            object(o, r);
            cursor = std::max(cursor, r.end);
        }
        if (cursor < contents.end)
            text(source_.substr(cursor, contents.end - cursor));
    }

    void wrapped(object_id o, std::string_view open, std::string_view close) {
        out_ += open;
        const auto c = t_.objects.contents[o];
        if (!c.empty())
            objects(c, detail::nested_objects(t_, o));
        out_ += close;
    }

    void object(object_id o, source_range r) {
        switch (t_.objects.kinds[o]) {
        case object_kind::bold: wrapped(o, "<strong>", "</strong>"); return;
        case object_kind::italic: wrapped(o, "<em>", "</em>"); return;
        case object_kind::underline:
            wrapped(o, "<span class=\"underline\">", "</span>");
            return;
        case object_kind::strike_through: wrapped(o, "<del>", "</del>"); return;
        case object_kind::code:
        case object_kind::verbatim:
            out_ += t_.objects.kinds[o] == object_kind::code
                ? "<code>" : "<code class=\"org-verbatim\">";
            if (r.size() >= 2)
                text(source_.substr(r.begin + 1, r.size() - 2));
            out_ += "</code>";
            return;
        case object_kind::link: link(o, r); return;
        case object_kind::timestamp:
            out_ += "<span class=\"org-timestamp\">";
            text(r);
            out_ += "</span>";
            return;
        case object_kind::statistics_cookie:
            out_ += "<span class=\"org-statistics-cookie\">";
            text(r);
            out_ += "</span>";
            return;
        case object_kind::line_break: out_ += "<br>\n"; return;
        case object_kind::subscript:
        case object_kind::superscript:
            text(r);
            return;
        case object_kind::export_snippet: {
            const auto snippet = r.of(source_);
            constexpr std::string_view html = "@@html:";
            if (snippet.starts_with(html) && snippet.size() >= html.size() + 2)
                text(snippet.substr(html.size(),
                    snippet.size() - html.size() - 2));
            return;
        }
        case object_kind::footnote_reference:
        case object_kind::inline_src_block:
        case object_kind::inline_babel_call:
        case object_kind::macro:
        case object_kind::latex_fragment:
        case object_kind::entity:
        case object_kind::radio_target:
        case object_kind::target:
        case object_kind::table_cell:
        case object_kind::citation:
        case object_kind::citation_reference:
            break;
        }
        const auto kind = kind_name(t_.objects.kinds[o]);
        unsupported_.emplace_back(kind);
        out_ += "<span class=\"org-unsupported\" data-org-kind=\"";
        text(kind);
        out_ += "\">";
        text(r);
        out_ += "</span>";
    }

    /**
     * @brief The description of a link object, rendered, or its link text.
     */
    std::string label(object_id o) {
        std::string saved;
        std::swap(saved, out_);
        const auto c = t_.objects.contents[o];
        if (!c.empty())
            objects(c, detail::nested_objects(t_, o));
        else
            text(detail::link_text(source_, t_, o));
        std::swap(saved, out_);
        return saved;
    }

    void anchor_to(std::string_view url, std::string_view label_html) {
        out_ += "<a href=\"";
        text(url);
        out_ += "\">";
        out_ += label_html;
        out_ += "</a>";
    }

    /**
     * @brief The URL of a document the caller may see, or empty.
     */
    std::string visible_document(const std::optional<std::string>& id,
        std::string_view anchor) const {
        if (!id || id->empty() || !links_.visible || !links_.visible(*id))
            return {};
        return document_url(config_.link_template, *id, anchor);
    }

    void link(object_id o, source_range) {
        const auto l = detail::link_of(t_, o);
        const auto html = label(o);
        if (l.is_none()) {
            out_ += html;
            return;
        }
        const auto type = name(t_.links.types[l]);
        auto path = detail::link_path(source_, t_, l);

        if (type == "http" || type == "https") {
            anchor_to(std::string(type) + ":" + path, html);
            return;
        }
        if (type == "fuzzy" || type == "custom-id") {
            auto key = type == "custom-id" ? "#" + path : path;
            auto local = local_.find(key);
            if (local == local_.end() && type == "fuzzy")
                local = local_.find("*" + path);
            if (local != local_.end()) {
                anchor_to("#" + percent_encoded(local->second, false), html);
                return;
            }
            out_ += html;
            return;
        }
        if (type == "id") {
            if (const auto local = local_.find("id:" + path);
                local != local_.end()) {
                anchor_to("#" + percent_encoded(local->second, false), html);
                return;
            }
            const auto document = links_.document_of
                ? links_.document_of(path) : std::nullopt;
            const auto url = visible_document(document,
                document && *document != path ? std::string_view(path)
                                              : std::string_view{});
            if (url.empty())
                out_ += html;
            else
                anchor_to(url, html);
            return;
        }
        if (type == "file") {
            if (const auto search = path.find("::");
                search != std::string::npos)
                path.resize(search);
            const auto resolved = resolved_path(directory_, path);
            if (resolved.empty()) {
                out_ += html;
                return;
            }
            if (t_.objects.contents[o].empty() && is_image(resolved)) {
                std::string src(config_.image_template);
                replace_all(src, "{path}", percent_encoded(resolved, true));
                out_ += "<img src=\"";
                text(src);
                out_ += "\" alt=\"";
                text(path);
                out_ += "\">";
                return;
            }
            const auto document = links_.document_at
                ? links_.document_at(resolved) : std::nullopt;
            const auto url = visible_document(document, {});
            if (url.empty())
                out_ += html;
            else
                anchor_to(url, html);
            return;
        }
        out_ += html;
    }

    std::string_view source_;
    const tables_view& t_;
    const types::render_config& config_;
    const types::link_resolver& links_;
    std::string_view directory_;
    std::vector<std::string> anchors_;
    std::vector<std::uint32_t> heading_of_;
    std::vector<std::uint32_t> block_of_;
    std::vector<std::uint32_t> list_of_;
    std::vector<std::uint32_t> item_of_;
    std::vector<std::uint32_t> keyword_of_;
    std::unordered_map<std::string, std::string> local_;
    std::string out_;
    std::vector<std::string> unsupported_;
};

}

types::rendered_document render(std::string_view source,
    const tables_view& t, std::string_view path,
    const types::render_config& config, const types::link_resolver& links) {
    return renderer(source, t, path, config, links).run();
}

}
