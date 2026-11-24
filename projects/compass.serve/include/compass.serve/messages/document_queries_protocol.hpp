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
/**
 * AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Template: masd_cpp_protocol.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_SERVE_MESSAGES_DOCUMENT_QUERIES_PROTOCOL_HPP
#define COMPASS_SERVE_MESSAGES_DOCUMENT_QUERIES_PROTOCOL_HPP

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include "compass.db/codec/text_form.hpp"
#include "compass.db/codec/wire_fields.hpp"
#include "compass.db/codec/wire_traits_fwd.hpp"

namespace compass::serve::messages {

#ifndef COMPASS_SERVE_MESSAGES_DOCUMENT_FORMAT_ENUM
/**
 * @brief The values :values: lists for document_format.
 */
enum class document_format { html, org };

/**
 * @brief Each value of document_format as the wire names it.
 */
[[nodiscard]] constexpr std::string_view to_string(document_format v) {
    switch (v) {
    case document_format::html: return "html";
    case document_format::org: return "org";
    }
    throw std::invalid_argument("Not a document_format: the value is not one of html, org");
}

/**
 * @brief The document_format @p text names, or none for any other text.
 */
[[nodiscard]] constexpr std::optional<document_format> document_format_of(
    std::string_view text) noexcept {
    if (text == "html")
        return document_format::html;
    if (text == "org")
        return document_format::org;
    return std::nullopt;
}
#endif

#ifndef COMPASS_SERVE_MESSAGES_ENVIRONMENT_KIND_ENUM
/**
 * @brief The values :values: lists for environment_kind.
 */
enum class environment_kind { worktree, branch, release };

/**
 * @brief Each value of environment_kind as the wire names it.
 */
[[nodiscard]] constexpr std::string_view to_string(environment_kind v) {
    switch (v) {
    case environment_kind::worktree: return "worktree";
    case environment_kind::branch: return "branch";
    case environment_kind::release: return "release";
    }
    throw std::invalid_argument("Not a environment_kind: the value is not one of worktree, branch, release");
}

/**
 * @brief The environment_kind @p text names, or none for any other text.
 */
[[nodiscard]] constexpr std::optional<environment_kind> environment_kind_of(
    std::string_view text) noexcept {
    if (text == "worktree")
        return environment_kind::worktree;
    if (text == "branch")
        return environment_kind::branch;
    if (text == "release")
        return environment_kind::release;
    return std::nullopt;
}
#endif

struct metadata_match {
    std::string name;
    std::string value;

    friend bool operator==(const metadata_match&, const metadata_match&) = default;
};

/**
 * @brief The members of metadata_match, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<metadata_match>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"name", &metadata_match::name},
        ::compass::db::codec::field{"value", &metadata_match::value},
    };
}

struct document_filter {
    std::optional<std::string> type;
    std::vector<std::string> tags;
    std::optional<std::string> state;
    std::optional<std::string> sprint;
    std::optional<std::string> level;
    std::optional<std::string> audience;
    std::vector<metadata_match> keywords;
    std::vector<metadata_match> properties;

    friend bool operator==(const document_filter&, const document_filter&) = default;
};

/**
 * @brief The members of document_filter, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<document_filter>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"type", &document_filter::type},
        ::compass::db::codec::field{"tags", &document_filter::tags, true},
        ::compass::db::codec::field{"state", &document_filter::state},
        ::compass::db::codec::field{"sprint", &document_filter::sprint},
        ::compass::db::codec::field{"level", &document_filter::level},
        ::compass::db::codec::field{"audience", &document_filter::audience},
        ::compass::db::codec::field{"keywords", &document_filter::keywords, true},
        ::compass::db::codec::field{"properties", &document_filter::properties, true},
    };
}

struct search_request {
    std::string environment;
    std::string q;
    document_filter filter;
    std::uint32_t limit = 20;
    std::uint32_t offset = 0;

    friend bool operator==(const search_request&, const search_request&) = default;
};

/**
 * @brief The members of search_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<search_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &search_request::environment},
        ::compass::db::codec::field{"q", &search_request::q},
        ::compass::db::codec::field{"filter", &search_request::filter, true},
        ::compass::db::codec::field{"limit", &search_request::limit, true},
        ::compass::db::codec::field{"offset", &search_request::offset, true},
    };
}

struct search_hit {
    std::string org_id;
    std::string title;
    std::string type;
    std::string description;
    std::string path;
    std::vector<std::string> heading_path;
    std::string anchor;
    std::string snippet;
    double rank = 0;

    friend bool operator==(const search_hit&, const search_hit&) = default;
};

/**
 * @brief The members of search_hit, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<search_hit>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"org_id", &search_hit::org_id},
        ::compass::db::codec::field{"title", &search_hit::title},
        ::compass::db::codec::field{"type", &search_hit::type},
        ::compass::db::codec::field{"description", &search_hit::description},
        ::compass::db::codec::field{"path", &search_hit::path},
        ::compass::db::codec::field{"heading_path", &search_hit::heading_path, true},
        ::compass::db::codec::field{"anchor", &search_hit::anchor},
        ::compass::db::codec::field{"snippet", &search_hit::snippet},
        ::compass::db::codec::field{"rank", &search_hit::rank, true},
    };
}

struct search_response {
    std::vector<search_hit> hits;
    std::uint64_t total = 0;
    std::string synced_at;

    friend bool operator==(const search_response&, const search_response&) = default;
};

/**
 * @brief The members of search_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<search_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"hits", &search_response::hits, true},
        ::compass::db::codec::field{"total", &search_response::total, true},
        ::compass::db::codec::field{"synced_at", &search_response::synced_at},
    };
}

struct facets_request {
    std::string environment;
    std::string q;
    document_filter filter;
    std::vector<std::string> keywords;
    std::vector<std::string> properties;

    friend bool operator==(const facets_request&, const facets_request&) = default;
};

/**
 * @brief The members of facets_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<facets_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &facets_request::environment},
        ::compass::db::codec::field{"q", &facets_request::q, true},
        ::compass::db::codec::field{"filter", &facets_request::filter, true},
        ::compass::db::codec::field{"keywords", &facets_request::keywords, true},
        ::compass::db::codec::field{"properties", &facets_request::properties, true},
    };
}

struct facet_count {
    std::string value;
    std::string label;
    std::uint64_t count = 0;

    friend bool operator==(const facet_count&, const facet_count&) = default;
};

/**
 * @brief The members of facet_count, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<facet_count>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"value", &facet_count::value},
        ::compass::db::codec::field{"label", &facet_count::label},
        ::compass::db::codec::field{"count", &facet_count::count, true},
    };
}

struct named_facet {
    std::string name;
    std::vector<facet_count> counts;

    friend bool operator==(const named_facet&, const named_facet&) = default;
};

/**
 * @brief The members of named_facet, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<named_facet>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"name", &named_facet::name},
        ::compass::db::codec::field{"counts", &named_facet::counts, true},
    };
}

struct facets_response {
    std::uint64_t documents = 0;
    std::vector<facet_count> types;
    std::vector<facet_count> tags;
    std::vector<facet_count> states;
    std::vector<facet_count> sprints;
    std::vector<facet_count> levels;
    std::vector<facet_count> audiences;
    std::vector<named_facet> keywords;
    std::vector<named_facet> properties;

    friend bool operator==(const facets_response&, const facets_response&) = default;
};

/**
 * @brief The members of facets_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<facets_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"documents", &facets_response::documents, true},
        ::compass::db::codec::field{"types", &facets_response::types, true},
        ::compass::db::codec::field{"tags", &facets_response::tags, true},
        ::compass::db::codec::field{"states", &facets_response::states, true},
        ::compass::db::codec::field{"sprints", &facets_response::sprints, true},
        ::compass::db::codec::field{"levels", &facets_response::levels, true},
        ::compass::db::codec::field{"audiences", &facets_response::audiences, true},
        ::compass::db::codec::field{"keywords", &facets_response::keywords, true},
        ::compass::db::codec::field{"properties", &facets_response::properties, true},
    };
}

struct document_summary {
    std::string org_id;
    std::string title;
    std::string type;
    std::string description;
    std::string path;
    std::vector<std::string> tags;
    std::string updated;

    friend bool operator==(const document_summary&, const document_summary&) = default;
};

/**
 * @brief The members of document_summary, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<document_summary>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"org_id", &document_summary::org_id},
        ::compass::db::codec::field{"title", &document_summary::title},
        ::compass::db::codec::field{"type", &document_summary::type},
        ::compass::db::codec::field{"description", &document_summary::description},
        ::compass::db::codec::field{"path", &document_summary::path},
        ::compass::db::codec::field{"tags", &document_summary::tags, true},
        ::compass::db::codec::field{"updated", &document_summary::updated},
    };
}

struct list_documents_request {
    std::string environment;
    document_filter filter;
    std::uint32_t limit = 50;
    std::uint32_t offset = 0;

    friend bool operator==(const list_documents_request&, const list_documents_request&) = default;
};

/**
 * @brief The members of list_documents_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<list_documents_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &list_documents_request::environment},
        ::compass::db::codec::field{"filter", &list_documents_request::filter, true},
        ::compass::db::codec::field{"limit", &list_documents_request::limit, true},
        ::compass::db::codec::field{"offset", &list_documents_request::offset, true},
    };
}

struct list_documents_response {
    std::vector<document_summary> documents;
    std::uint64_t total = 0;
    std::string synced_at;

    friend bool operator==(const list_documents_response&, const list_documents_response&) = default;
};

/**
 * @brief The members of list_documents_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<list_documents_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"documents", &list_documents_response::documents, true},
        ::compass::db::codec::field{"total", &list_documents_response::total, true},
        ::compass::db::codec::field{"synced_at", &list_documents_response::synced_at},
    };
}

struct get_document_request {
    std::string environment;
    std::string id;
    document_format format = document_format::html;

    friend bool operator==(const get_document_request&, const get_document_request&) = default;
};

/**
 * @brief The members of get_document_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<get_document_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &get_document_request::environment},
        ::compass::db::codec::field{"id", &get_document_request::id},
        ::compass::db::codec::field{"format", &get_document_request::format, true},
    };
}

struct get_document_response {
    document_summary document;
    document_format format{};
    std::string body;
    std::string synced_at;

    friend bool operator==(const get_document_response&, const get_document_response&) = default;
};

/**
 * @brief The members of get_document_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<get_document_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"document", &get_document_response::document, true},
        ::compass::db::codec::field{"format", &get_document_response::format},
        ::compass::db::codec::field{"body", &get_document_response::body},
        ::compass::db::codec::field{"synced_at", &get_document_response::synced_at},
    };
}

struct backlinks_request {
    std::string environment;
    std::string id;

    friend bool operator==(const backlinks_request&, const backlinks_request&) = default;
};

/**
 * @brief The members of backlinks_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<backlinks_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &backlinks_request::environment},
        ::compass::db::codec::field{"id", &backlinks_request::id},
    };
}

struct backlink {
    document_summary document;
    std::string anchor;
    std::string text;

    friend bool operator==(const backlink&, const backlink&) = default;
};

/**
 * @brief The members of backlink, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<backlink>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"document", &backlink::document, true},
        ::compass::db::codec::field{"anchor", &backlink::anchor},
        ::compass::db::codec::field{"text", &backlink::text},
    };
}

struct backlinks_response {
    std::vector<backlink> backlinks;
    std::string synced_at;

    friend bool operator==(const backlinks_response&, const backlinks_response&) = default;
};

/**
 * @brief The members of backlinks_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<backlinks_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"backlinks", &backlinks_response::backlinks, true},
        ::compass::db::codec::field{"synced_at", &backlinks_response::synced_at},
    };
}

struct list_environments_request {

    friend bool operator==(const list_environments_request&, const list_environments_request&) = default;
};

/**
 * @brief The members of list_environments_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<list_environments_request>) {
    return ::compass::db::codec::field_list{
    };
}

struct environment_summary {
    std::string name;
    environment_kind kind{};
    std::string root;
    std::string head;
    std::string synced_at;
    std::uint64_t documents = 0;

    friend bool operator==(const environment_summary&, const environment_summary&) = default;
};

/**
 * @brief The members of environment_summary, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<environment_summary>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"name", &environment_summary::name},
        ::compass::db::codec::field{"kind", &environment_summary::kind},
        ::compass::db::codec::field{"root", &environment_summary::root},
        ::compass::db::codec::field{"head", &environment_summary::head},
        ::compass::db::codec::field{"synced_at", &environment_summary::synced_at},
        ::compass::db::codec::field{"documents", &environment_summary::documents, true},
    };
}

struct list_environments_response {
    std::vector<environment_summary> environments;

    friend bool operator==(const list_environments_response&, const list_environments_response&) = default;
};

/**
 * @brief The members of list_environments_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<list_environments_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environments", &list_environments_response::environments, true},
    };
}

struct version_request {

    friend bool operator==(const version_request&, const version_request&) = default;
};

/**
 * @brief The members of version_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<version_request>) {
    return ::compass::db::codec::field_list{
    };
}

struct version_response {
    std::string api_version;
    std::string compass_version;

    friend bool operator==(const version_response&, const version_response&) = default;
};

/**
 * @brief The members of version_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<version_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"api_version", &version_response::api_version},
        ::compass::db::codec::field{"compass_version", &version_response::compass_version},
    };
}

struct sync_request {
    std::string environment;

    friend bool operator==(const sync_request&, const sync_request&) = default;
};

/**
 * @brief The members of sync_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<sync_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &sync_request::environment},
    };
}

struct sync_response {
    std::string environment;
    std::uint64_t placed = 0;
    std::uint64_t loaded = 0;
    std::uint64_t parsed = 0;
    std::vector<std::string> errors;
    std::string synced_at;

    friend bool operator==(const sync_response&, const sync_response&) = default;
};

/**
 * @brief The members of sync_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<sync_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &sync_response::environment},
        ::compass::db::codec::field{"placed", &sync_response::placed, true},
        ::compass::db::codec::field{"loaded", &sync_response::loaded, true},
        ::compass::db::codec::field{"parsed", &sync_response::parsed, true},
        ::compass::db::codec::field{"errors", &sync_response::errors, true},
        ::compass::db::codec::field{"synced_at", &sync_response::synced_at},
    };
}

}

namespace compass::db::codec {

#ifndef COMPASS_SERVE_MESSAGES_DOCUMENT_FORMAT_ENUM
#define COMPASS_SERVE_MESSAGES_DOCUMENT_FORMAT_ENUM
/**
 * @brief document_format travels as the name of its value.
 */
template <>
struct text_form<compass::serve::messages::document_format> {
    static constexpr std::string_view wire_name = "a document_format";

    static constexpr std::string_view names[] = { "html", "org" };

    static constexpr compass::serve::messages::document_format values[] = { compass::serve::messages::document_format::html, compass::serve::messages::document_format::org };

    static std::string to_text(compass::serve::messages::document_format v) {
        return std::string(to_string(v));
    }

    static compass::serve::messages::document_format from_text(std::string_view text) {
        if (const auto v = compass::serve::messages::document_format_of(text))
            return *v;
        throw std::invalid_argument("Not " + std::string(wire_name) + ": '" +
                                    std::string(text) + "' is not one of html, org");
    }
};
#endif

#ifndef COMPASS_SERVE_MESSAGES_ENVIRONMENT_KIND_ENUM
#define COMPASS_SERVE_MESSAGES_ENVIRONMENT_KIND_ENUM
/**
 * @brief environment_kind travels as the name of its value.
 */
template <>
struct text_form<compass::serve::messages::environment_kind> {
    static constexpr std::string_view wire_name = "a environment_kind";

    static constexpr std::string_view names[] = { "worktree", "branch", "release" };

    static constexpr compass::serve::messages::environment_kind values[] = { compass::serve::messages::environment_kind::worktree, compass::serve::messages::environment_kind::branch, compass::serve::messages::environment_kind::release };

    static std::string to_text(compass::serve::messages::environment_kind v) {
        return std::string(to_string(v));
    }

    static compass::serve::messages::environment_kind from_text(std::string_view text) {
        if (const auto v = compass::serve::messages::environment_kind_of(text))
            return *v;
        throw std::invalid_argument("Not " + std::string(wire_name) + ": '" +
                                    std::string(text) + "' is not one of worktree, branch, release");
    }
};
#endif

template <>
struct wire_traits<compass::serve::messages::metadata_match> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::metadata_match& v);

    static compass::serve::messages::metadata_match read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::document_filter> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::document_filter& v);

    static compass::serve::messages::document_filter read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::search_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::search_request& v);

    static compass::serve::messages::search_request read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::search_hit> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::search_hit& v);

    static compass::serve::messages::search_hit read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::search_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::search_response& v);

    static compass::serve::messages::search_response read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::facets_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::facets_request& v);

    static compass::serve::messages::facets_request read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::facet_count> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::facet_count& v);

    static compass::serve::messages::facet_count read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::named_facet> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::named_facet& v);

    static compass::serve::messages::named_facet read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::facets_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::facets_response& v);

    static compass::serve::messages::facets_response read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::document_summary> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::document_summary& v);

    static compass::serve::messages::document_summary read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::list_documents_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::list_documents_request& v);

    static compass::serve::messages::list_documents_request read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::list_documents_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::list_documents_response& v);

    static compass::serve::messages::list_documents_response read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::get_document_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::get_document_request& v);

    static compass::serve::messages::get_document_request read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::get_document_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::get_document_response& v);

    static compass::serve::messages::get_document_response read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::backlinks_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::backlinks_request& v);

    static compass::serve::messages::backlinks_request read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::backlink> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::backlink& v);

    static compass::serve::messages::backlink read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::backlinks_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::backlinks_response& v);

    static compass::serve::messages::backlinks_response read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::list_environments_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::list_environments_request& v);

    static compass::serve::messages::list_environments_request read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::environment_summary> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::environment_summary& v);

    static compass::serve::messages::environment_summary read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::list_environments_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::list_environments_response& v);

    static compass::serve::messages::list_environments_response read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::version_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::version_request& v);

    static compass::serve::messages::version_request read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::version_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::version_response& v);

    static compass::serve::messages::version_response read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::sync_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::sync_request& v);

    static compass::serve::messages::sync_request read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::sync_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::sync_response& v);

    static compass::serve::messages::sync_response read_json(std::string_view text);
};

}

#endif
