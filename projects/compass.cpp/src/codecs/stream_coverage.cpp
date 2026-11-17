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
#include "compass.cpp/codecs/stream_coverage.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<ct::rule, 9> rules{{
    {"unresolved_declaration", "Every ensure_stream, serve_step_commands "
        "and js_queue_subscribe call names its stream and subjects in a way "
        "the check reads."},
    {"two_filter_sets", "Every ensure of one stream carries the same "
        "filters."},
    {"unread_entry_point", "The transport offers no JetStream call or "
        "stream-creation entry point the check does not read."},
    {"empty_catalogue", "The scan finds a stream."},
    {"overlapping_streams", "No two streams have overlapping filters."},
    {"wildcard_subject", "A durable consumer reads named subjects only."},
    {"uncovered_subject", "Every durable subject is matched by a stream."},
    {"many_streams", "No durable subject is matched by two streams."},
    {"unensured_stream", "A consumer names a stream something ensures, "
        "whose filters cover its subject."}}};

constexpr std::string_view excluded_prefix = "projects/ores.nats/";
constexpr std::string_view step_helper = "projects/ores.service/include/"
    "ores.service/messaging/step_command_stream.hpp";
constexpr std::string_view client_header =
    "projects/ores.nats/include/ores.nats/service/client.hpp";
constexpr std::string_view admin_header =
    "projects/ores.nats/include/ores.nats/service/jetstream_admin.hpp";
constexpr std::array<std::string_view, 6> skip_dirs{"venv", "node_modules",
    ".venv", "build", "target", ".git"};

struct unresolved : std::runtime_error {
    using std::runtime_error::runtime_error;
};

const pt::regex& identifier() {
    static const pt::regex r(R"(^[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*$)");
    return r;
}

std::string no_spaces(std::string_view s) {
    std::string out;
    for (const char c : s) {
        if (!ops::is_space(c))
            out.push_back(c);
    }
    return out;
}

/**
 * @brief The text with its comments blanked and its string literals kept,
 * every offset and newline in place.
 */
std::string strip_comments(std::string_view text) {
    static const pt::regex noise(R"re("(\\.|[^"\\])*"|//[^\n]*|/\*.*?\*/)re",
        {.dotall = true});
    return noise.substitute(text, [](const auto& m) {
        std::string token(m.group());
        if (token.starts_with('"'))
            return token;
        for (auto& c : token) {
            if (c != '\n' && static_cast<unsigned char>(c) < 0x80)
                c = ' ';
        }
        return token;
    });
}

std::vector<std::string> split_arguments(std::string_view text,
    std::size_t open_at) {
    std::vector<std::string> args;
    int depth = 0;
    auto start = open_at + 1;
    for (auto i = open_at; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '"') {
            ++i;
            while (i < text.size() && text[i] != '"')
                i += text[i] == '\\' ? 2 : 1;
        } else if (c == '(' || c == '{' || c == '[') {
            ++depth;
        } else if (c == ')' || c == '}' || c == ']') {
            if (--depth == 0) {
                args.emplace_back(text.substr(start, i - start));
                return args;
            }
        } else if (c == ',' && depth == 1) {
            args.emplace_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    throw unresolved("a call does not close");
}

std::string brace_body(std::string_view text, std::size_t open_at) {
    int depth = 0;
    for (auto i = open_at; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '"') {
            ++i;
            while (i < text.size() && text[i] != '"')
                i += text[i] == '\\' ? 2 : 1;
        } else if (c == '{') {
            ++depth;
        } else if (c == '}' && --depth == 0) {
            return std::string(text.substr(open_at + 1, i - open_at - 1));
        }
    }
    throw unresolved("a braced list does not close");
}

std::optional<std::string> string_literal(std::string_view expr) {
    static const pt::regex literal(R"re(\s*"([^"]*)"\s*)re");
    const auto m = literal.fullmatch(expr);
    if (!m)
        return std::nullopt;
    return std::string(m->group(1));
}

std::vector<std::size_t> find_call_sites(std::string_view text,
    std::string_view name) {
    std::vector<std::size_t> out;
    for (const auto& m : pt::regex(R"(\b)" + pt::escape(name) + R"(\s*\()")
             .find_all(text))
        out.push_back(m.start());
    return out;
}

const pt::regex& const_re() {
    static const pt::regex r(
        R"re((?:inline\s+)?constexpr\s+(?:static\s+)?)re"
        R"re((?:std::string_view|auto|char\s+const\s*\*|const\s+char\s*\*)\s+)re"
        R"re((\w+)\s*=\s*"([^"]*)")re");
    return r;
}

const pt::regex& file_local_string() {
    static const pt::regex r(
        R"re((?:static\s+)?(?:const|constexpr)\s+(?:auto|std::string_view)\s+(\w+)\s*=\s*"([^"]*)")re");
    return r;
}

std::string str(std::string_view s) { return std::string(s); }

/**
 * @brief The string constants, subject members and arrays of the tree, as
 * the script's Tree class reads them.
 */
class tree final {
public:
    /**
     * @pre @p raw holds every file that names a stream, a subject or a
     * string constant; @p add_callers adds every other file that calls the
     * function it is given.
     */
    tree(std::map<std::string, std::string>& raw,
        std::function<void(std::string_view)> add_callers)
        : raw_(raw), add_callers_(std::move(add_callers)) {
        static const pt::regex type_start(R"((?:struct|class)\s+(\w+)[^;{]*\{)");
        static const pt::regex member(R"re(nats_subject\s*=\s*"([^"]*)")re");
        for (const auto& [rel, source] : raw_) {
            if (!rel.ends_with(".hpp"))
                continue;
            if (source.find("nats_subject") == std::string::npos &&
                !const_re().search(source))
                continue;
            const auto& t = text(rel);
            auto& consts = file_consts_[rel];
            for (const auto& m : const_re().find_all(t)) {
                values_[str(m.group(1))].insert(str(m.group(2)));
                consts[str(m.group(1))] = str(m.group(2));
            }
            if (t.find("nats_subject") == std::string::npos)
                continue;
            const auto starts = type_start.find_all(t);
            for (std::size_t i = 0; i < starts.size(); ++i) {
                const auto end = i + 1 < starts.size() ? starts[i + 1].start()
                    : t.size();
                const auto region = std::string_view(t).substr(
                    starts[i].end(), end - std::min(end, starts[i].end()));
                if (const auto m = member.search(region))
                    members_[str(starts[i].group(1))].insert(str(m->group(1)));
            }
        }
    }

    const std::string& text(const std::string& rel) {
        auto it = stripped_.find(rel);
        if (it == stripped_.end())
            it = stripped_.emplace(rel, strip_comments(raw_.at(rel))).first;
        return it->second;
    }

    const std::map<std::string, std::string>& file_consts(
        const std::string& rel) {
        auto it = file_consts_.find(rel);
        if (it != file_consts_.end())
            return it->second;
        auto& consts = file_consts_[rel];
        for (const auto& m : const_re().find_all(text(rel)))
            consts.try_emplace(str(m.group(1)), str(m.group(2)));
        return consts;
    }

    std::vector<std::string> resolve(std::string_view raw_expr,
        const std::string& rel) {
        const auto expr = no_spaces(raw_expr);
        if (auto literal = string_literal(expr))
            return {*literal};
        if (!identifier().match(expr))
            throw unresolved("the expression '" + expr +
                "' is not a name this check reads");
        std::vector<std::string> parts;
        for (std::size_t s = 0;;) {
            const auto e = expr.find("::", s);
            parts.push_back(expr.substr(s, e - s));
            if (e == std::string::npos)
                break;
            s = e + 2;
        }
        const auto& last = parts.back();
        if (parts.size() >= 2 && last == "nats_subject") {
            const auto owner = members_.find(parts[parts.size() - 2]);
            if (owner != members_.end()) {
                if (owner->second.size() == 1)
                    return {*owner->second.begin()};
                throw unresolved(owner->first + "::nats_subject has " +
                    std::to_string(owner->second.size()) + " values");
            }
        }
        const auto& local = file_consts(rel);
        if (const auto it = local.find(last); it != local.end())
            return {it->second};
        if (const auto it = members_.find(last);
            it != members_.end() && it->second.size() == 1)
            return {*it->second.begin()};
        if (const auto it = values_.find(last); it != values_.end()) {
            if (it->second.size() == 1)
                return {*it->second.begin()};
            throw unresolved("the name '" + last + "' has " +
                std::to_string(it->second.size()) + " values");
        }
        throw unresolved("the name '" + last +
            "' is not a literal string this check can resolve");
    }

    std::vector<std::string> resolve_array(const std::string& name,
        const std::string& rel) {
        const auto& all = arrays(rel);
        const auto it = all.find(name);
        if (it == all.end())
            throw unresolved("the array '" + name + "' is not declared in " +
                str(rel.substr(rel.rfind('/') + 1)));
        std::vector<std::string> subjects;
        for (const auto& item : it->second) {
            for (auto& s : resolve(item, rel))
                subjects.push_back(std::move(s));
        }
        return subjects;
    }

    std::string stream_suffix(std::string_view raw_expr,
        const std::string& rel) {
        static const pt::regex make(R"re(nats\.make_stream_name\("([^"]*)"\))re");
        static const pt::regex stream_var(
            R"re((?:const\s+auto|const\s+std::string|std::string)\s+(\w+)\s*)re"
            R"re((?:=\s*|\(\s*)nats\.make_stream_name\s*\(\s*"([^"]*)"\s*\))re");
        const auto expr = no_spaces(raw_expr);
        if (const auto m = make.fullmatch(expr))
            return str(m->group(1));
        if (!identifier().match(expr))
            throw unresolved("the stream name '" + expr +
                "' is not a name this check reads");
        const auto& local = file_consts(rel);
        if (const auto it = local.find(expr); it != local.end())
            return it->second;
        for (const auto& m : stream_var.find_all(text(rel))) {
            if (m.group(1) == expr)
                return str(m.group(2));
        }
        throw unresolved("the stream name '" + expr +
            "' is not a make_stream_name call this check reads");
    }

    std::string service_name(std::string_view raw_expr,
        const std::string& rel, std::size_t position) {
        const auto expr = no_spaces(raw_expr);
        if (auto literal = string_literal(expr))
            return *literal;
        if (!identifier().match(expr))
            throw unresolved("the service name '" + expr +
                "' is not a name this check reads");
        for (const auto& m : file_local_string().find_all(text(rel))) {
            if (m.group(1) == expr)
                return str(m.group(2));
        }
        return bound_service(expr, rel, position);
    }

private:
    std::string bound_service(const std::string& param,
        const std::string& rel, std::size_t position) {
        static const pt::regex head(R"(^([A-Za-z_]\w*(?:::\w+)*)\s*\()",
            {.multiline = true});
        static const pt::regex word(R"([A-Za-z_]\w*)");
        const auto& t = text(rel);
        std::optional<pt::regex_match> enclosing;
        for (auto& m : head.find_all(t)) {
            if (m.start() < position)
                enclosing = std::move(m);
        }
        if (!enclosing)
            throw unresolved(
                "the call is not inside a function this check reads");
        std::vector<std::string> params;
        for (const auto& arg : split_arguments(t, enclosing->end() - 1)) {
            const auto names = word.find_all(arg);
            if (!names.empty())
                params.emplace_back(names.back().group());
        }
        auto function = str(enclosing->group(1));
        function = function.substr(function.rfind("::") == std::string::npos
            ? 0 : function.rfind("::") + 2);
        const auto p = std::ranges::find(params, param);
        if (p == params.end())
            throw unresolved("'" + param +
                "' is neither a constant here nor a parameter");
        const auto index = static_cast<std::size_t>(p - params.begin());
        std::set<std::string> found;
        add_callers_(function);
        for (const auto& [other_rel, other_text] : raw_) {
            if (other_text.find(function) == std::string::npos)
                continue;
            for (const auto at : find_call_sites(other_text, function)) {
                if (other_rel == rel)
                    continue;
                const auto args = split_arguments(other_text,
                    other_text.find('(', at));
                if (index >= args.size())
                    continue;
                if (auto literal = string_literal(args[index])) {
                    found.insert(*literal);
                    continue;
                }
                const auto expr = no_spaces(args[index]);
                for (const auto& m : file_local_string().find_all(
                         text(other_rel))) {
                    if (m.group(1) == expr) {
                        found.insert(str(m.group(2)));
                        break;
                    }
                }
            }
        }
        if (found.size() == 1)
            return *found.begin();
        throw unresolved("the parameter '" + param + "' of " + function +
            " resolves to " + (found.empty() ? std::string("nothing")
                : std::to_string(found.size()) + " values"));
    }

    const std::map<std::string, std::vector<std::string>>& arrays(
        const std::string& rel) {
        static const pt::regex array_start(
            R"((?:static\s+)?constexpr\s+)"
            R"((?:std::array\s*(?:<[^;{}]*>)?|std::string_view|std::string)"
            R"(|char\s+const\s*\*))"
            R"(\s*(\w+)\s*(?:\[\s*\])?\s*(?:=\s*)?\{)");
        auto it = arrays_.find(rel);
        if (it != arrays_.end())
            return it->second;
        std::map<std::string, std::vector<std::string>> found;
        const auto& t = text(rel);
        for (const auto& m : array_start.find_all(t)) {
            const auto body = "(" + brace_body(t, m.end() - 1) + ")";
            std::vector<std::string> items;
            for (const auto& item : split_arguments(body, 0)) {
                const auto s = ops::strip(item);
                if (!s.empty())
                    items.emplace_back(s);
            }
            found[str(m.group(1))] = std::move(items);
        }
        return arrays_.emplace(rel, std::move(found)).first->second;
    }

    std::map<std::string, std::string>& raw_;
    std::function<void(std::string_view)> add_callers_;
    std::map<std::string, std::string> stripped_;
    std::map<std::string, std::set<std::string>> values_;
    std::map<std::string, std::set<std::string>> members_;
    std::map<std::string, std::map<std::string, std::vector<std::string>>>
        arrays_;
    std::map<std::string, std::map<std::string, std::string>> file_consts_;
};

struct site {
    std::string file;
    std::uint32_t offset = 0;
    std::uint32_t line = 0;

    [[nodiscard]] std::string where() const {
        return file + ":" + std::to_string(line);
    }
};

struct stream {
    std::string suffix;
    std::vector<std::string> filters;
    std::vector<site> where;
};

struct consumer {
    std::vector<std::string> subjects;
    std::string named_stream;
    site where;
};

std::vector<std::string> covering(const std::vector<std::string>& subjects) {
    std::vector<std::string> filters;
    for (const auto& s : subjects) {
        filters.push_back(s);
        filters.push_back(s + ".>");
    }
    return filters;
}

std::vector<std::string> tokens(std::string_view s) {
    std::vector<std::string> out;
    for (std::size_t start = 0;;) {
        const auto dot = s.find('.', start);
        out.emplace_back(s.substr(start, dot - start));
        if (dot == std::string_view::npos)
            return out;
        start = dot + 1;
    }
}

bool filter_matches(std::string_view pattern, std::string_view subject) {
    const auto p = tokens(pattern);
    const auto s = tokens(subject);
    std::size_t i = 0;
    for (; i < p.size(); ++i) {
        if (p[i] == ">")
            return i < s.size() && i == p.size() - 1;
        if (i >= s.size())
            return false;
        if (p[i] != "*" && p[i] != s[i])
            return false;
    }
    return i == s.size();
}

bool filters_overlap(std::string_view a, std::string_view b) {
    const auto ta = tokens(a);
    const auto tb = tokens(b);
    std::size_t i = 0;
    for (; i < ta.size() && i < tb.size(); ++i) {
        if (ta[i] == ">" || tb[i] == ">")
            return true;
        if (ta[i] != "*" && tb[i] != "*" && ta[i] != tb[i])
            return false;
    }
    return i == ta.size() && i == tb.size();
}

std::string join(const std::vector<std::string>& items) {
    std::string out;
    for (const auto& i : items)
        out += (out.empty() ? "" : ", ") + i;
    return out;
}

std::vector<std::string_view> scanned(const ct::corpus_view& v) {
    std::vector<std::string_view> out;
    for (const auto suffix : {".cpp", ".hpp"}) {
        for (const auto p : v.list_files("projects/", suffix)) {
            const auto parts = ops::components_of(p);
            const bool skipped = std::ranges::any_of(parts, [](auto part) {
                return std::ranges::find(skip_dirs, part) != skip_dirs.end();
            });
            if (skipped || p.find("/tests/") != std::string_view::npos ||
                p.starts_with(excluded_prefix))
                continue;
            out.push_back(p);
        }
    }
    return out;
}

std::string decoded(std::string_view bytes) {
    return pt::universal_newlines(pt::decode_replacing(bytes));
}

/**
 * @brief The fact of a source: + and its decoded text when it names a
 * stream call, a subject member or a string constant; otherwise - and
 * each name it calls on a line of its own, as such a file only matters as
 * a caller of a function whose parameter names a service.
 */
void map(std::string_view, std::string_view bytes, const ct::option_values&,
    ct::file_scan& out) {
    static const pt::regex call(R"(\b([A-Za-z_]\w*)\s*\()");
    auto text = decoded(bytes);
    const bool named = text.find("nats_subject") != std::string::npos ||
        text.find("ensure_stream") != std::string::npos ||
        text.find("serve_step_commands") != std::string::npos ||
        text.find("js_queue_subscribe") != std::string::npos ||
        const_re().search(text);
    if (named) {
        out.fact = "+" + std::move(text);
        return;
    }
    std::set<std::string> called;
    for (const auto& m : call.find_all(text))
        called.emplace(m.group(1));
    out.fact = "-\n";
    for (const auto& c : called)
        out.fact += c + "\n";
}

void reduce(std::span<const ct::scanned_file> files,
    const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::map<std::string, std::string> raw;
    for (const auto& f : files) {
        if (f.fact.starts_with('+'))
            raw.emplace(f.path, f.fact.substr(1));
    }
    tree t(raw, [&](std::string_view function) {
        const auto line = "\n" + std::string(function) + "\n";
        for (const auto& f : files) {
            if (f.fact.starts_with('-') &&
                f.fact.find(line) != std::string_view::npos)
                raw.emplace(f.path,
                    decoded(v.read_file(f.path).value_or("")));
        }
    });
    std::map<std::string, stream> streams;
    std::vector<consumer> consumers;
    const auto report = [&](const site& s, std::uint32_t rule,
        const std::string& message, std::string_view fix) {
        const auto bytes = v.read_file(s.file).value_or("");
        ops::report(out, s.file, ops::offset_of_line(bytes, s.line), rule,
            message, fix);
    };
    const auto add_stream = [&](const std::string& suffix,
        std::vector<std::string> filters, const site& where) {
        auto it = streams.find(suffix);
        if (it == streams.end()) {
            streams.emplace(suffix, stream{suffix, std::move(filters),
                {where}});
            return;
        }
        const std::set<std::string> a(it->second.filters.begin(),
            it->second.filters.end());
        const std::set<std::string> b(filters.begin(), filters.end());
        if (a != b) {
            report(where, 1, "one stream, two filter sets: '" + suffix +
                "' is ensured with other filters at " +
                it->second.where[0].where(), "ensure the stream with one "
                "filter set; the later ensure drops the subjects only the "
                "earlier one covers");
            return;
        }
        it->second.where.push_back(where);
    };
    const std::string_view unresolved_fix = "name the stream and subjects "
        "as the check reads them, or teach the check the new form";

    for (const auto& [rel, source] : raw) {
        const auto& text = t.text(rel);
        const auto site_at = [&](std::size_t at) {
            return site{rel, static_cast<std::uint32_t>(at),
                ops::line_of(text, at)};
        };
        if (rel != step_helper) {
            if (text.find("ensure_stream") != std::string::npos) {
                for (const auto at : find_call_sites(text, "ensure_stream")) {
                    const auto where = site_at(at);
                    try {
                        const auto args = split_arguments(text,
                            text.find('(', at));
                        if (args.size() < 2)
                            throw unresolved(
                                "ensure_stream needs a name and a filter");
                        const auto suffix = t.stream_suffix(args[0], rel);
                        static const pt::regex head(
                            R"(^nats\s*\.\s*covering_subjects\s*\()");
                        static const pt::regex spaces(R"(\s+)");
                        const auto expr = spaces.substitute(
                            ops::strip(args[1]), " ");
                        const auto h = head.search(expr);
                        if (!h)
                            throw unresolved("the filter argument '" + expr +
                                "' is not a covering_subjects call");
                        const auto inner_args = split_arguments(expr,
                            h->end() - 1);
                        if (inner_args.size() != 1)
                            throw unresolved("covering_subjects takes one "
                                "argument");
                        const auto inner = std::string(
                            ops::strip(inner_args[0]));
                        std::vector<std::string> subjects;
                        if (inner.starts_with('{')) {
                            const auto list = "(" + inner.substr(1,
                                inner.size() - 2) + ")";
                            for (const auto& item : split_arguments(list, 0)) {
                                if (ops::strip(item).empty())
                                    continue;
                                for (auto& s : t.resolve(item, rel))
                                    subjects.push_back(std::move(s));
                            }
                        } else if (identifier().match(no_spaces(inner))) {
                            subjects = t.resolve_array(no_spaces(inner), rel);
                        } else {
                            throw unresolved("covering_subjects is given '" +
                                inner + "', which this check cannot read");
                        }
                        if (subjects.empty())
                            throw unresolved("ensure_stream covers no subject");
                        add_stream(suffix, covering(subjects), where);
                    } catch (const unresolved& e) {
                        report(where, 0, "ensure_stream: " + str(e.what()),
                            unresolved_fix);
                    }
                }
            }
            if (text.find("serve_step_commands") != std::string::npos) {
                for (const auto at :
                    find_call_sites(text, "serve_step_commands")) {
                    const auto where = site_at(at);
                    try {
                        const auto args = split_arguments(text,
                            text.find('(', at));
                        if (args.size() != 3 && args.size() != 4)
                            throw unresolved("serve_step_commands takes three "
                                "arguments and an optional worker count");
                        const auto service = t.service_name(args[1], rel, at);
                        const auto routes = no_spaces(args[2]);
                        if (!identifier().match(routes))
                            throw unresolved("the routes argument is not a "
                                "table");
                        static const pt::regex step_routes(
                            R"re(step_routes\s+(\w+)\s*(?:\{\s*"([^"]*)"\s*\})?\s*;)re");
                        std::optional<pt::regex_match> declaration;
                        for (auto& m : step_routes.find_all(text)) {
                            if (m.group(1) == routes) {
                                declaration = std::move(m);
                                break;
                            }
                        }
                        if (!declaration)
                            throw unresolved("the routes table '" + routes +
                                "' is not declared here");
                        const auto group = str(declaration->group(2));
                        const auto serve = text.find("serve_step_commands",
                            declaration->end());
                        const auto region = std::string_view(text).substr(
                            declaration->end(), serve == std::string::npos
                                ? std::string::npos
                                : serve - declaration->end());
                        static const pt::regex on(R"(\.on\s*\()");
                        std::vector<std::string> subjects;
                        bool seen = false;
                        for (const auto& m : on.find_all(region)) {
                            seen = true;
                            const auto expr = std::string(ops::strip(
                                split_arguments(region, m.end() - 1)[0]));
                            try {
                                for (auto& s : t.resolve(expr, rel))
                                    subjects.push_back(std::move(s));
                                continue;
                            } catch (const unresolved&) {
                            }
                            const pt::regex loop(R"(for\s*\([^)]*\b)" +
                                pt::escape(expr) +
                                R"(\s*:\s*([A-Za-z_]\w*)\s*\))");
                            const auto l = loop.search(region);
                            if (!l)
                                throw unresolved("the subject '" + expr +
                                    "' of " + routes + " cannot be resolved");
                            for (auto& s : t.resolve_array(str(l->group(1)),
                                     rel))
                                subjects.push_back(std::move(s));
                        }
                        if (!seen)
                            throw unresolved("the routes table '" + routes +
                                "' declares no subject");
                        auto name = service;
                        if (name.starts_with("ores."))
                            name = name.substr(5);
                        if (name.ends_with(".service"))
                            name = name.substr(0, name.size() - 8);
                        if (!group.empty())
                            name += "_" + group;
                        const auto suffix = name + "_steps";
                        add_stream(suffix, covering(subjects), where);
                        consumers.push_back({subjects, suffix, where});
                    } catch (const unresolved& e) {
                        report(where, 0, "serve_step_commands: " +
                            str(e.what()), unresolved_fix);
                    }
                }
            }
        }
        for (const auto call : {"js_queue_subscribe",
                 "js_queue_subscribe_multi"}) {
            if (rel == step_helper || text.find(call) == std::string::npos)
                continue;
            for (const auto at : find_call_sites(text, call)) {
                const auto where = site_at(at);
                try {
                    const auto args = split_arguments(text,
                        text.find('(', at));
                    if (args.size() < 7)
                        throw unresolved(str(call) + " takes seven arguments");
                    auto subjects = t.resolve(args[0], rel);
                    const auto named = t.stream_suffix(args[4], rel);
                    consumers.push_back({std::move(subjects), named, where});
                } catch (const unresolved& e) {
                    report(where, 0, "js_queue_subscribe: " + str(e.what()),
                        unresolved_fix);
                }
            }
        }
    }

    static const pt::regex js_call(R"(\bjs_[a-z_]+\b)");
    static const pt::regex stream_entry(
        R"(\b(?:ensure|create|add)_\w*stream\w*\s*\()");
    if (const auto client = v.read_file(client_header)) {
        const auto text = strip_comments(pt::decode_replacing(*client));
        std::set<std::string> names;
        for (const auto& m : js_call.find_all(text))
            names.insert(str(m.group()));
        for (const auto& n : names) {
            if (n != "js_publish" && n != "js_queue_subscribe" &&
                n != "js_queue_subscribe_multi")
                ops::report(out, client_header, 0, 2, "the transport offers a "
                    "JetStream call this check does not read: '" + n + "'",
                    "teach the check the subject and stream it declares, or "
                    "remove the call");
        }
    }
    if (const auto admin = v.read_file(admin_header)) {
        const auto text = strip_comments(pt::decode_replacing(*admin));
        std::set<std::string> names;
        for (const auto& m : stream_entry.find_all(text)) {
            auto n = str(m.group());
            while (!n.empty() && (n.back() == '(' || n.back() == ' ' ||
                       n.back() == '\n'))
                n.pop_back();
            names.insert(n);
        }
        for (const auto& n : names) {
            if (n != "ensure_stream")
                ops::report(out, admin_header, 0, 2, "the transport offers a "
                    "stream-creation entry point this check does not read: '" +
                    n + "'", "teach the check the filters it declares, or "
                    "remove the entry point");
        }
    }

    if (streams.empty())
        ops::report(out, "projects", 0, 3, "no stream declaration found; the "
            "scan is not reading the tree's ensure_stream sites",
            "teach the check how streams are declared now");

    std::vector<const stream*> sorted;
    for (const auto& [suffix, s] : streams)
        sorted.push_back(&s);
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        for (std::size_t j = i + 1; j < sorted.size(); ++j) {
            const auto& left = *sorted[i];
            const auto& right = *sorted[j];
            std::optional<std::pair<std::string, std::string>> pair;
            for (const auto& a : left.filters) {
                for (const auto& b : right.filters) {
                    if (!pair && (a == b || filters_overlap(a, b)))
                        pair = {a, b};
                }
            }
            if (pair)
                report(right.where[0], 4, "stream subject overlap: '" +
                    left.suffix + "' filter '" + pair->first + "' and '" +
                    right.suffix + "' filter '" + pair->second + "', at " +
                    left.where[0].where() + "; NATS refuses the second",
                    "give each stream subjects of its own");
        }
    }

    std::set<std::pair<std::string, std::vector<std::string>>> reported;
    for (const auto& c : consumers) {
        for (const auto& subject : c.subjects) {
            if (subject.find_first_of("*>") != std::string::npos) {
                report(c.where, 5, "durable subject this check cannot prove: '" +
                    subject + "'", "consume named subjects");
                continue;
            }
            std::vector<std::string> covering_streams;
            for (const auto* s : sorted) {
                if (std::ranges::any_of(s->filters, [&](const auto& f) {
                        return filter_matches(f, subject); }))
                    covering_streams.push_back(s->suffix);
            }
            if (covering_streams.empty()) {
                report(c.where, 6, "durable subject with no stream: '" +
                    subject + "'; a publish to it times out",
                    "ensure the stream that covers it in the owning "
                    "service's start path");
                continue;
            }
            if (covering_streams.size() > 1 &&
                reported.insert({subject, covering_streams}).second)
                report(c.where, 7, "durable subject with more than one "
                    "stream: '" + subject + "' is covered by " +
                    join(covering_streams), "let one stream claim it");
            const auto named = streams.find(c.named_stream);
            if (named == streams.end()) {
                report(c.where, 8, "durable consumer on an unensured stream: '" +
                    c.named_stream + "' for '" + subject + "'",
                    "ensure the stream the consumer names");
            } else if (std::ranges::none_of(named->second.filters,
                           [&](const auto& f) {
                               return filter_matches(f, subject); })) {
                report(c.where, 8, "durable consumer outside its stream: '" +
                    subject + "' on '" + c.named_stream + "'",
                    "read the subject on the stream that covers it");
            }
        }
    }
}

}

const ct::descriptor stream_coverage{
    .name = "stream_coverage",
    .version = 3,
    .summary = "Reports each durably consumed subject no single stream "
        "covers, and each pair of streams whose subjects overlap.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, reduce}};

}
