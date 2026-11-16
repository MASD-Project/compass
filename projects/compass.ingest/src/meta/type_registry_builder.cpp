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
#include "compass.ingest/meta/type_registry_builder.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <numeric>
#include <optional>
#include <unordered_map>
#include <utility>

namespace compass::ingest::meta {

namespace {

namespace ot = compass::org::types;

constexpr std::string_view title_prefix = "Document type: ";

std::string_view trim(std::string_view s) noexcept {
    const auto blank = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };
    while (!s.empty() && blank(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && blank(s.back()))
        s.remove_suffix(1);
    return s;
}

bool contains_word(std::string_view s, std::string_view lower_word) {
    std::string lower(s);
    std::ranges::transform(lower, lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower.find(lower_word) != std::string::npos;
}

std::optional<ot::type_level> parse_level(std::string_view s) noexcept {
    if (s == "cross")
        return ot::type_level::cross;
    if (s.size() == 2 && s[0] == 's' && s[1] >= '1' && s[1] <= '5')
        return static_cast<ot::type_level>(s[1] - '0');
    return std::nullopt;
}

/**
 * @brief Read access to the tables of one parsed page.
 */
class page_view {
public:
    page_view(std::string_view source, const ot::tables_view& t) noexcept
        : src_(source), t_(t) {}

    std::string_view name(ot::name_id n) const noexcept {
        const auto first = t_.names.firsts[n];
        const auto end = n.next().value < t_.names.size()
            ? t_.names.firsts[n.next()]
            : static_cast<std::uint32_t>(t_.name_bytes.size());
        return {t_.name_bytes.data() + first, end - first};
    }

    std::string_view text(ot::source_range r) const noexcept {
        return r.of(src_);
    }

    std::string_view keyword(std::string_view key) const noexcept {
        for (std::uint32_t k = 0; k < t_.keywords.size(); ++k) {
            const ot::keyword_id id{k};
            if (name(t_.keywords.names[id]) == key)
                return trim(text(t_.keywords.values[id]));
        }
        return {};
    }

    ot::element_id end_of(ot::element_id e) const noexcept {
        return t_.elements.subtree_ends[e];
    }

    ot::element_kind kind(ot::element_id e) const noexcept {
        return t_.elements.kinds[e];
    }

    /**
     * @brief The level-1 heading titled Contract, or none.
     */
    ot::element_id contract() const noexcept {
        for (std::uint32_t h = 0; h < t_.headings.size(); ++h) {
            const ot::heading_id id{h};
            if (t_.headings.levels[id] == 1 &&
                trim(text(t_.headings.titles[id])) == "Contract")
                return t_.headings.elements[id];
        }
        return ot::element_id::none();
    }

    /**
     * @brief The objects of the elements in [first, end), in pre-order.
     */
    std::pair<std::uint32_t, std::uint32_t> objects(ot::element_id first,
        ot::element_id end) const noexcept {
        const auto at = [&](ot::element_id e) {
            return e.value < t_.elements.size()
                ? t_.elements.object_firsts[e].value
                : static_cast<std::uint32_t>(t_.objects.size());
        };
        return {at(first), at(end)};
    }

    bool is_verbatim(std::uint32_t o) const noexcept {
        const auto k = t_.objects.kinds[ot::object_id{o}];
        return k == ot::object_kind::verbatim || k == ot::object_kind::code;
    }

    std::string_view verbatim_text(std::uint32_t o) const noexcept {
        const auto s = trim(text(t_.objects.ranges[ot::object_id{o}]));
        return s.size() >= 2 ? s.substr(1, s.size() - 2) : std::string_view{};
    }

    /**
     * @brief The text of the first verbatim or code object in the elements
     * of [first, end), or nothing.
     */
    std::optional<std::string_view> first_verbatim(ot::element_id first,
        ot::element_id end) const noexcept {
        const auto [begin, last] = objects(first, end);
        for (auto o = begin; o < last; ++o) {
            if (is_verbatim(o))
                return verbatim_text(o);
        }
        return std::nullopt;
    }

    /**
     * @brief The verbatim that opens paragraph @p p, or nothing.
     */
    std::optional<std::string_view> opening_verbatim(
        ot::element_id p) const noexcept {
        const auto [begin, last] = objects(p, p.next());
        if (begin == last || !is_verbatim(begin) ||
            t_.objects.ranges[ot::object_id{begin}].begin !=
                t_.elements.contents[p].begin)
            return std::nullopt;
        return verbatim_text(begin);
    }

    std::string_view contents(ot::element_id e) const noexcept {
        return text(t_.elements.contents[e]);
    }

    /**
     * @brief The item rows whose elements lie in [first, end).
     */
    std::pair<std::uint32_t, std::uint32_t> items(ot::element_id first,
        ot::element_id end) const noexcept {
        const auto& v = t_.items.elements.values;
        const auto lo = std::ranges::lower_bound(v, first) - v.begin();
        const auto hi = std::ranges::lower_bound(v, end) - v.begin();
        return {static_cast<std::uint32_t>(lo), static_cast<std::uint32_t>(hi)};
    }

    ot::element_id item_element(std::uint32_t i) const noexcept {
        return t_.items.elements[ot::item_id{i}];
    }

    std::string_view item_tag(std::uint32_t i) const noexcept {
        return trim(text(t_.items.tags[ot::item_id{i}]));
    }

    const ot::tables_view& tables() const noexcept { return t_; }

private:
    std::string_view src_;
    const ot::tables_view& t_;
};

struct todo_keywords {
    std::vector<std::string> active;
    std::vector<std::string> done;
};

/**
 * @brief Splits an org TODO vocabulary. Without a bar the last keyword is
 * the done one, as in org; a fast-access key such as (d) is dropped.
 */
todo_keywords parse_todos(std::string_view s) {
    todo_keywords r;
    std::vector<std::string> words;
    bool bar = false;
    while (true) {
        s = trim(s);
        if (s.empty())
            break;
        const auto end = std::min(s.find_first_of(" \t"), s.size());
        auto word = s.substr(0, end);
        s.remove_prefix(end);
        if (word == "|") {
            r.active = std::move(words);
            words.clear();
            bar = true;
            continue;
        }
        word = word.substr(0, word.find('('));
        words.emplace_back(word);
    }
    if (bar) {
        r.done = std::move(words);
    } else if (!words.empty()) {
        r.done.push_back(std::move(words.back()));
        words.pop_back();
        r.active = std::move(words);
    }
    return r;
}

/**
 * @brief The title of a heading written as =* Title :tags:=, or nothing
 * when the text is not a heading.
 */
std::optional<std::string_view> heading_title(std::string_view s) noexcept {
    const auto stars = s.find_first_not_of('*');
    if (stars == 0 || stars == std::string_view::npos || s[stars] != ' ')
        return std::nullopt;
    auto title = trim(s.substr(stars));
    if (title.size() > 1 && title.back() == ':') {
        const auto space = title.rfind(' ');
        if (space != std::string_view::npos && title[space + 1] == ':')
            title = trim(title.substr(0, space));
    }
    if (title.empty())
        return std::nullopt;
    return title;
}

struct contract_fields {
    contract_report report;
    ot::type_level level = ot::type_level::none;
    std::string folder;
    todo_keywords todos;
    std::vector<std::string> sections;
};

class contract_reader {
public:
    explicit contract_reader(const page_view& p) noexcept : p_(p) {}

    void read(ot::element_id contract, contract_fields& f) const {
        const auto end = p_.end_of(contract);
        const auto [first_item, last_item] = p_.items(contract, end);
        for (auto i = first_item; i < last_item; ++i)
            read_item(i, f);
        if (f.report.state == state_source::missing)
            read_blocks(contract, end, f);
        if (f.report.sections_from == sections_source::missing)
            read_section_headings(contract, end, f);
    }

private:
    void read_item(std::uint32_t i, contract_fields& f) const {
        const auto e = p_.item_element(i);
        const auto body = e.next();
        const auto end = p_.end_of(e);
        const auto tag = p_.item_tag(i);
        auto& r = f.report;
        if (tag == "Level" && r.level == level_source::missing) {
            const auto v = p_.first_verbatim(body, end);
            const auto level = v ? parse_level(*v) : std::nullopt;
            r.level = level ? level_source::item : level_source::unknown_value;
            f.level = level.value_or(ot::type_level::none);
        } else if ((tag == "Folder" || tag == "Path") &&
            r.folder == folder_source::missing) {
            if (const auto v = p_.first_verbatim(body, end)) {
                r.folder = tag == "Folder" ? folder_source::folder_item
                                           : folder_source::path_item;
                f.folder = *v;
            }
        } else if (tag == "TODO vocabulary" &&
            r.state == state_source::missing) {
            if (const auto v = p_.first_verbatim(body, end)) {
                r.state = state_source::todo_vocabulary_item;
                f.todos = parse_todos(*v);
            }
        } else if (tag == "State" && r.state == state_source::missing) {
            const auto v = p_.first_verbatim(body, end);
            constexpr std::string_view todo = "#+todo:";
            if (v && v->starts_with(todo)) {
                r.state = state_source::state_item;
                f.todos = parse_todos(v->substr(todo.size()));
            }
        } else if (tag.starts_with("Required") &&
            r.sections_from == sections_source::missing) {
            r.sections_from = sections_source::item;
            read_section_list(body, end, f);
        } else if (tag.empty() && r.state == state_source::missing &&
            body < end && p_.kind(body) == ot::element_kind::paragraph &&
            p_.contents(body).starts_with("Not stateful")) {
            r.state = state_source::not_stateful;
        }
    }

    /**
     * @brief Reads the items of the first plain list among the children of
     * the elements in [first, end).
     */
    void read_section_list(ot::element_id first, ot::element_id end,
        contract_fields& f) const {
        for (auto c = first; c < end; c = p_.end_of(c)) {
            if (p_.kind(c) != ot::element_kind::plain_list)
                continue;
            for (auto item = c.next(); item < p_.end_of(c);
                 item = p_.end_of(item)) {
                const auto para = item.next();
                if (para >= p_.end_of(item) ||
                    p_.kind(para) != ot::element_kind::paragraph)
                    continue;
                const auto v = p_.opening_verbatim(para);
                const auto title = v ? heading_title(*v) : std::nullopt;
                if (title && !contains_word(p_.contents(para), "optional")) {
                    f.sections.emplace_back(*title);
                    ++f.report.sections;
                }
            }
            return;
        }
    }

    void read_blocks(ot::element_id first, ot::element_id end,
        contract_fields& f) const {
        constexpr std::string_view todo = "#+todo:";
        const auto& b = p_.tables().blocks;
        for (std::uint32_t i = 0; i < b.size(); ++i) {
            const ot::block_id id{i};
            if (b.elements[id] < first || !(b.elements[id] < end))
                continue;
            const auto body = p_.text(b.bodies[id]);
            const auto at = body.find(todo);
            if (at == std::string_view::npos)
                continue;
            const auto line = body.substr(at + todo.size(),
                body.find('\n', at) - at - todo.size());
            f.report.state = state_source::todo_block;
            f.todos = parse_todos(line);
            return;
        }
    }

    void read_section_headings(ot::element_id contract, ot::element_id end,
        contract_fields& f) const {
        const auto& h = p_.tables().headings;
        for (std::uint32_t i = 0; i < h.size(); ++i) {
            const ot::heading_id id{i};
            const auto e = h.elements[id];
            if (!(contract < e) || !(e < end) ||
                !contains_word(p_.text(h.titles[id]), "sections"))
                continue;
            f.report.sections_from = sections_source::heading;
            const auto section = e.next();
            if (section < p_.end_of(e) &&
                p_.kind(section) == ot::element_kind::section)
                read_section_list(section.next(), p_.end_of(section), f);
            return;
        }
    }

    const page_view& p_;
};

}

bool fits(const contract_report& r) noexcept {
    return r.types != 0 && r.contract && !r.duplicate &&
        r.level == level_source::item &&
        r.folder != folder_source::missing &&
        r.state != state_source::missing && r.sections != 0;
}

bool type_registry_builder::has(std::string_view name) const {
    return std::ranges::any_of(rows_,
        [&](const row& r) { return r.name == name; });
}

contract_report type_registry_builder::add_contract(std::string_view source,
    const ot::tables_view& page) {
    const page_view p(source, page);
    contract_fields f;
    const auto contract = p.contract();
    f.report.contract = !contract.is_none();
    if (f.report.contract)
        contract_reader(p).read(contract, f);

    auto title = p.keyword("title");
    if (!title.starts_with(title_prefix))
        return f.report;
    title.remove_prefix(title_prefix.size());
    while (!title.empty()) {
        const auto comma = std::min(title.find(','), title.size());
        const auto name = trim(title.substr(0, comma));
        title.remove_prefix(std::min(comma + 1, title.size()));
        if (name.empty())
            continue;
        ++f.report.types;
        if (has(name)) {
            f.report.duplicate = true;
            continue;
        }
        rows_.push_back({std::string(name), ot::type_kind::document, f.level,
            f.folder, f.todos.active, f.todos.done, f.sections});
    }
    return f.report;
}

bool type_registry_builder::add_model(std::string_view name) {
    if (has(name))
        return false;
    rows_.push_back({std::string(name), ot::type_kind::model,
        ot::type_level::none, {}, {}, {}, {}});
    return true;
}

ot::type_registry type_registry_builder::build() const {
    ot::type_registry r;
    std::unordered_map<std::string_view, ot::name_id> interned;
    const auto intern = [&](std::string_view s) {
        const auto [it, added] = interned.try_emplace(s,
            ot::name_id{static_cast<std::uint32_t>(r.names.size())});
        if (added) {
            r.names.firsts.values.push_back(
                static_cast<std::uint32_t>(r.name_bytes.size()));
            r.name_bytes.insert(r.name_bytes.end(), s.begin(), s.end());
        }
        return it->second;
    };
    const auto entry = [&] {
        return ot::type_entry_id{static_cast<std::uint32_t>(r.entries.size())};
    };
    const auto add_entries = [&](const std::vector<std::string>& names) {
        for (const auto& n : names)
            r.entries.names.values.push_back(intern(n));
    };

    std::vector<std::size_t> order(rows_.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::sort(order, {}, [&](std::size_t i) -> std::string_view {
        return rows_[i].name;
    });

    auto& t = r.types;
    for (const auto i : order) {
        const auto& row = rows_[i];
        t.names.values.push_back(intern(row.name));
        t.kinds.values.push_back(row.kind);
        t.levels.values.push_back(row.level);
        t.folders.values.push_back(
            row.folder.empty() ? ot::name_id::none() : intern(row.folder));
        t.todo_firsts.values.push_back(entry());
        add_entries(row.active);
        t.done_firsts.values.push_back(entry());
        add_entries(row.done);
        t.section_firsts.values.push_back(entry());
        add_entries(row.sections);
    }
    return r;
}

}
