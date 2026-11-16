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
#include "compass.store/io/git_objects.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <system_error>
#include "compass.platform/filesystem/mapped_file.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.store/ops/git_object.hpp"

namespace compass::store::io {

namespace {

using platform::filesystem::mapped_file;

/**
 * Git's own delta base cache defaults to 96 MiB; a chain longer than
 * git's pack.depth limit of 4095 means the pack loops.
 */
constexpr std::size_t cache_slots = 4096;
constexpr std::size_t cache_budget = 96u << 20;
constexpr std::size_t largest_cached = 16u << 20;
constexpr std::size_t deepest_chain = 10000;
constexpr int deepest_alternates = 5;

struct pack {
    std::filesystem::path path;
    mapped_file index_file;
    mapped_file pack_file;
    ops::pack_index index;
};

struct located {
    std::size_t pack;
    std::uint64_t offset;
};

/**
 * @brief One delta of a chain: where it is and its header.
 */
struct delta_link {
    located at;
    ops::pack_entry entry;
};

using shared_object = std::shared_ptr<const types::git_object>;

std::runtime_error damaged(const types::object_id& id, const pack& p,
    std::string_view why) {
    return std::runtime_error("git object " + ops::to_hex(id) + " in " +
        p.path.string() + ": " + std::string(why));
}

bool is_whole(ops::pack_entry_kind k) noexcept {
    return k != ops::pack_entry_kind::offset_delta &&
        k != ops::pack_entry_kind::ref_delta;
}

std::vector<std::filesystem::path> alternates_of(
    const std::filesystem::path& objects) {
    std::vector<std::filesystem::path> out;
    std::ifstream in(objects / "info" / "alternates");
    for (std::string line; std::getline(in, line);) {
        if (line.empty() || line.front() == '#')
            continue;
        const std::filesystem::path named(line);
        out.push_back((named.is_absolute() ? named : objects / named)
            .lexically_normal());
    }
    return out;
}

}

struct git_objects::state {
    std::vector<std::unique_ptr<pack>> packs;
    std::vector<std::filesystem::path> loose;

    struct slot {
        std::mutex lock;
        std::size_t pack = 0;
        std::uint64_t offset = 0;
        shared_object object;
    };
    mutable std::array<slot, cache_slots> cache;
    mutable std::atomic<std::size_t> cached_bytes{0};

    void open(const std::filesystem::path& objects, int depth) {
        if (depth > deepest_alternates ||
            std::ranges::contains(loose, objects))
            return;
        loose.push_back(objects);
        std::vector<std::filesystem::path> indexes;
        std::error_code ec;
        for (const auto& e :
             std::filesystem::directory_iterator(objects / "pack", ec)) {
            if (e.path().extension() == ".idx")
                indexes.push_back(e.path());
        }
        std::ranges::sort(indexes);
        for (const auto& index : indexes) {
            auto pack_path = index;
            pack_path.replace_extension(".pack");
            if (!std::filesystem::exists(pack_path))
                continue;
            auto p = std::make_unique<pack>(pack{pack_path,
                mapped_file(index), mapped_file(pack_path), {}});
            const auto parsed = ops::parse_pack_index(p->index_file.bytes());
            if (!parsed)
                throw std::runtime_error(index.string() + ": " +
                    parsed.error());
            p->index = *parsed;
            const auto bytes = p->pack_file.text();
            if (bytes.size() < 12 || !bytes.starts_with("PACK"))
                throw std::runtime_error(pack_path.string() +
                    ": not a pack file");
            packs.push_back(std::move(p));
        }
        for (const auto& alternate : alternates_of(objects))
            open(alternate, depth + 1);
    }

    [[nodiscard]] std::optional<located> locate(
        const types::object_id& id) const noexcept {
        for (std::size_t i = 0; i < packs.size(); ++i) {
            if (const auto offset = ops::find_offset(packs[i]->index, id))
                return located{i, *offset};
        }
        return std::nullopt;
    }

    [[nodiscard]] slot& slot_of(located at) const noexcept {
        const auto h = at.offset * 0x9e3779b97f4a7c15ull + at.pack;
        return cache[(h >> 20) % cache_slots];
    }

    [[nodiscard]] shared_object cached(located at) const {
        auto& s = slot_of(at);
        const std::scoped_lock guard(s.lock);
        if (s.object && s.pack == at.pack && s.offset == at.offset)
            return s.object;
        return nullptr;
    }

    void keep(located at, const shared_object& object) const {
        const auto size = object->bytes.size();
        if (size > largest_cached)
            return;
        auto& s = slot_of(at);
        const std::scoped_lock guard(s.lock);
        const auto evicted = s.object ? s.object->bytes.size() : 0;
        if (cached_bytes.load() - evicted + size > cache_budget)
            return;
        cached_bytes += size;
        cached_bytes -= evicted;
        s.pack = at.pack;
        s.offset = at.offset;
        s.object = object;
    }

    [[nodiscard]] std::optional<types::git_object> read_loose(
        const types::object_id& id) const {
        const auto hex = ops::to_hex(id);
        for (const auto& dir : loose) {
            const auto path = dir / hex.substr(0, 2) / hex.substr(2);
            std::optional<mapped_file> file;
            try {
                file.emplace(path);
            } catch (const std::system_error&) {
                continue;
            }
            auto object = ops::parse_loose_object(file->bytes());
            if (!object)
                throw std::runtime_error("git object " + hex + " at " +
                    path.string() + ": " + object.error());
            return std::move(*object);
        }
        return std::nullopt;
    }

    [[nodiscard]] std::string inflate(const types::object_id& id,
        located at, const ops::pack_entry& e) const {
        const auto& p = *packs[at.pack];
        auto bytes = ops::inflate(p.pack_file.bytes().subspan(e.data),
            e.size);
        if (!bytes)
            throw damaged(id, p, bytes.error());
        return std::move(*bytes);
    }

    /**
     * @brief The object @p id at @p at: walks its delta chain down to a
     * whole object or a cached one, then applies the deltas back up,
     * keeping each intermediate base in the cache.
     */
    [[nodiscard]] types::git_object read_packed(const types::object_id& id,
        located at) const {
        std::vector<delta_link> chain;
        shared_object base;
        for (auto here = at;;) {
            if (auto hit = cached(here)) {
                base = std::move(hit);
                break;
            }
            const auto& p = *packs[here.pack];
            const auto e = ops::parse_pack_entry(p.pack_file.bytes(),
                here.offset);
            if (!e)
                throw damaged(id, p, e.error());
            if (is_whole(e->kind)) {
                base = std::make_shared<const types::git_object>(
                    types::git_object{static_cast<types::object_type>(e->kind),
                        inflate(id, here, *e)});
                if (!chain.empty())
                    keep(here, base);
                break;
            }
            chain.push_back({here, *e});
            if (chain.size() > deepest_chain)
                throw damaged(id, p, "the delta chain loops");
            if (e->kind == ops::pack_entry_kind::offset_delta) {
                here = {here.pack, e->base_offset};
                continue;
            }
            if (const auto next = locate(e->base_id)) {
                here = *next;
                continue;
            }
            auto loose_base = read_loose(e->base_id);
            if (!loose_base)
                throw damaged(id, p, "the delta base " +
                    ops::to_hex(e->base_id) + " is missing");
            base = std::make_shared<const types::git_object>(
                std::move(*loose_base));
            break;
        }
        if (chain.empty())
            return *base;
        for (auto i = chain.size(); i-- > 1;) {
            base = std::make_shared<const types::git_object>(
                types::git_object{base->type, apply(id, chain[i], *base)});
            keep(chain[i].at, base);
        }
        return {base->type, apply(id, chain.front(), *base)};
    }

    [[nodiscard]] std::string apply(const types::object_id& id,
        const delta_link& l, const types::git_object& base) const {
        const auto delta = inflate(id, l.at, l.entry);
        auto bytes = ops::apply_delta(base.bytes, delta);
        if (!bytes)
            throw damaged(id, *packs[l.at.pack], bytes.error());
        return std::move(*bytes);
    }
};

git_objects::git_objects(const std::filesystem::path& objects)
    : state_(std::make_unique<state>()) {
    state_->open(objects.lexically_normal(), 0);
}

git_objects::~git_objects() = default;

std::optional<types::git_object> git_objects::read(
    const types::object_id& id) const {
    if (const auto at = state_->locate(id))
        return state_->read_packed(id, *at);
    return state_->read_loose(id);
}

std::size_t git_objects::packs() const noexcept {
    return state_->packs.size();
}

std::filesystem::path git_objects_dir(
    const std::filesystem::path& git_common_dir) {
    return git_common_dir / "objects";
}

namespace {

types::git_object read_typed(const git_objects& db,
    const types::object_id& id, types::object_type type,
    std::string_view what) {
    auto object = db.read(id);
    if (!object)
        throw std::runtime_error("the " + std::string(what) + " " +
            ops::to_hex(id) + " is missing");
    if (object->type != type)
        throw std::runtime_error("git object " + ops::to_hex(id) +
            " is not a " + std::string(what));
    return std::move(*object);
}

std::vector<types::tree_entry> entries_of(const types::git_object& tree,
    const types::object_id& id) {
    auto entries = ops::parse_tree(tree.bytes);
    if (!entries)
        throw std::runtime_error("tree " + ops::to_hex(id) + ": " +
            entries.error());
    return std::move(*entries);
}

types::object_id root_tree(const git_objects& db,
    const types::object_id& commit) {
    const auto object = read_typed(db, commit, types::object_type::commit,
        "commit");
    const auto tree = ops::tree_of_commit(object.bytes);
    if (!tree)
        throw std::runtime_error("commit " + ops::to_hex(commit) + ": " +
            tree.error());
    return *tree;
}

void add_tree(const git_objects& db, const types::object_id& id,
    const std::string& prefix, types::commit_files& out) {
    const auto tree = read_typed(db, id, types::object_type::tree, "tree");
    for (const auto& e : entries_of(tree, id)) {
        auto path = prefix + std::string(e.name);
        if (e.mode == types::tree_mode) {
            add_tree(db, e.id, path + "/", out);
            continue;
        }
        out.paths.push_back(std::move(path));
        out.modes.push_back(e.mode);
        out.ids.push_back(e.id);
    }
}

}

types::commit_files list_commit(const git_objects& db,
    const types::object_id& commit) {
    types::commit_files tree_order;
    add_tree(db, root_tree(db, commit), "", tree_order);
    std::vector<std::size_t> order(tree_order.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::sort(order, {}, [&](std::size_t row) {
        return std::string_view(tree_order.paths[row]);
    });
    types::commit_files out;
    out.paths.reserve(order.size());
    out.modes.reserve(order.size());
    out.ids.reserve(order.size());
    for (const auto row : order) {
        out.paths.push_back(std::move(tree_order.paths[row]));
        out.modes.push_back(tree_order.modes[row]);
        out.ids.push_back(tree_order.ids[row]);
    }
    return out;
}

std::optional<std::string> read_commit_file(const git_objects& db,
    const types::object_id& commit, std::string_view path) {
    auto id = root_tree(db, commit);
    for (;;) {
        const auto slash = path.find('/');
        const auto name = path.substr(0, slash);
        const auto tree = read_typed(db, id, types::object_type::tree, "tree");
        const auto entries = entries_of(tree, id);
        const auto e = std::ranges::find(entries, name,
            &types::tree_entry::name);
        if (e == entries.end())
            return std::nullopt;
        if (slash == std::string_view::npos) {
            if (e->mode == types::tree_mode || e->mode == types::gitlink_mode)
                return std::nullopt;
            return read_typed(db, e->id, types::object_type::blob,
                "blob").bytes;
        }
        if (e->mode != types::tree_mode)
            return std::nullopt;
        id = e->id;
        path.remove_prefix(slash + 1);
    }
}

}
