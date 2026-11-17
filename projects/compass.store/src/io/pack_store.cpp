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
#include "compass.store/io/pack_store.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <format>
#include <functional>
#include <random>
#include <system_error>
#include <utility>
#include <fcntl.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>
#include "compass.store/ops/action_object.hpp"

namespace compass::store::io {

namespace {

constexpr std::string_view pack_prefix = "pack-";
constexpr std::string_view pack_suffix = ".cpk";
constexpr std::size_t name_digits = 32;
constexpr int attempts = 1000;

/**
 * @brief A random word for the name of a file. The generator is seeded
 * again in a forked child, which would otherwise draw the names its
 * parent and its siblings draw.
 */
std::uint64_t random_word() {
    thread_local pid_t owner = 0;
    thread_local std::mt19937_64 generator;
    if (owner != ::getpid()) {
        owner = ::getpid();
        std::random_device device;
        generator.seed((std::uint64_t{device()} << 32) | device());
    }
    return generator();
}

std::int64_t now_ns() noexcept {
    struct timespec t{};
    ::clock_gettime(CLOCK_REALTIME, &t);
    return std::int64_t{t.tv_sec} * 1'000'000'000 + t.tv_nsec;
}

[[noreturn]] void fail(std::string_view what,
    const std::filesystem::path& path) {
    throw std::system_error(errno, std::generic_category(),
        std::string(what) + " " + path.string());
}

void write_all(int fd, const void* data, std::size_t size,
    const std::filesystem::path& path) {
    const auto* p = static_cast<const char*>(data);
    while (size != 0) {
        const auto n = ::write(fd, p, size);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            fail("cannot write", path);
        }
        p += n;
        size -= static_cast<std::size_t>(n);
    }
}

std::vector<std::filesystem::path> pack_paths(
    const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> out;
    std::error_code ignored;
    for (const auto& entry : std::filesystem::directory_iterator(dir,
             ignored)) {
        if (is_pack_name(entry.path().filename().string()))
            out.push_back(entry.path());
    }
    std::ranges::sort(out);
    return out;
}

}

bool is_pack_name(std::string_view name) noexcept {
    if (name.size() != pack_prefix.size() + name_digits + pack_suffix.size() ||
        !name.starts_with(pack_prefix) || !name.ends_with(pack_suffix))
        return false;
    return std::ranges::all_of(name.substr(pack_prefix.size(), name_digits),
        [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        });
}

std::vector<open_pack> open_packs(const std::filesystem::path& packs_dir,
    std::vector<std::filesystem::path>* rejected) {
    std::vector<open_pack> out;
    std::vector<std::filesystem::path> bad;
    std::vector<std::filesystem::path> previous;
    // A merge publishes its pack before it removes the ones it merged, so
    // a listing that missed both is caught by the next, which then
    // differs. Each round opens only the names it has not seen, so rounds
    // cost a directory listing, and the loop ends at the first two equal
    // listings in which no pack vanished before it could be opened.
    for (int attempt = 1; attempt <= attempts; ++attempt) {
        auto listed = pack_paths(packs_dir);
        bool vanished = false;
        for (const auto& path : listed) {
            if (std::ranges::contains(bad, path) ||
                std::ranges::any_of(out, [&](const open_pack& p) {
                    return p.path == path;
                }))
                continue;
            try {
                platform::filesystem::mapped_file file(path);
                const auto view = ops::parse_pack(file.bytes());
                if (view)
                    out.push_back({path, std::move(file), *view});
                else
                    bad.push_back(path);
            } catch (const std::system_error& e) {
                if (e.code() != std::errc::no_such_file_or_directory)
                    throw;
                vanished = true;
            }
        }
        if (!vanished && listed == previous)
            break;
        previous = std::move(listed);
    }
    std::ranges::sort(out, {}, &open_pack::path);
    if (rejected != nullptr)
        *rejected = std::move(bad);
    return out;
}

pack_reader::pack_reader(const std::filesystem::path& packs_dir)
    : packs_(open_packs(packs_dir)) {}

std::optional<std::span<const std::byte>> pack_reader::find(
    types::entry_kind kind, const types::blob_id& key) const noexcept {
    const open_pack* best_pack = nullptr;
    types::pack_entry best;
    for (auto p = packs_.rbegin(); p != packs_.rend(); ++p) {
        const auto e = ops::find_entry(p->view, kind, key);
        if (!e || (best_pack != nullptr && e->written <= best.written))
            continue;
        best_pack = &*p;
        best = *e;
        if (kind != types::entry_kind::manifest)
            break;
    }
    if (best_pack == nullptr)
        return std::nullopt;
    return ops::bytes_of(best_pack->view, best);
}

std::optional<std::string> pack_reader::read_action(types::action_kind kind,
    const types::blob_id& key) const {
    const auto decoded = [&](std::span<const std::byte> bytes) {
        return ops::decode_action(key, std::string_view(
            reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    };
    const auto entry = types::entry_kind_of(kind);
    const auto bytes = find(entry, key);
    if (!bytes)
        return std::nullopt;
    if (const auto body = decoded(*bytes))
        return std::string(*body);
    // A damaged copy may hide a sound one another pack holds.
    std::vector<std::pair<std::int64_t, std::span<const std::byte>>> copies;
    for (const auto& p : packs_) {
        if (const auto e = ops::find_entry(p.view, entry, key))
            copies.emplace_back(e->written, ops::bytes_of(p.view, *e));
    }
    std::ranges::sort(copies, std::greater{}, [](const auto& c) {
        return c.first;
    });
    for (const auto& [written, copy] : copies) {
        if (const auto body = decoded(copy))
            return std::string(*body);
    }
    return std::nullopt;
}

pack_writer::pack_writer(std::filesystem::path packs_dir)
    : dir_(std::move(packs_dir)) {}

pack_writer::~pack_writer() {
    if (fd_ != -1) {
        ::close(fd_);
        ::unlink(temporary_.c_str());
    }
}

void pack_writer::add(types::entry_kind kind, const types::blob_id& key,
    std::span<const std::byte> bytes, std::int64_t written) {
    const std::scoped_lock lock(mutex_);
    if (fd_ == -1) {
        std::filesystem::create_directories(dir_);
        for (int attempt = 0; fd_ == -1; ++attempt) {
            temporary_ = dir_ / std::format("tmp_{:016x}", random_word());
            fd_ = ::open(temporary_.c_str(),
                O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
            if (fd_ == -1 && (errno != EEXIST || attempt == attempts))
                fail("cannot create", temporary_);
        }
        const auto header = ops::encode_pack_header();
        write_all(fd_, header.data(), header.size(), temporary_);
        offset_ = header.size();
    }
    static constexpr std::array<char, ops::pack_alignment> zeros{};
    const auto pad = ops::padding_for(offset_);
    write_all(fd_, zeros.data(), pad, temporary_);
    offset_ += pad;
    write_all(fd_, bytes.data(), bytes.size(), temporary_);
    entries_.push_back({key, kind, offset_, bytes.size(),
        written != 0 ? written : now_ns()});
    offset_ += bytes.size();
}

void pack_writer::add_action(types::action_kind kind,
    const types::blob_id& key, std::string_view body) {
    const auto bytes = ops::encode_action(key, body);
    add(types::entry_kind_of(kind), key, std::as_bytes(std::span(bytes)));
}

std::size_t pack_writer::size() const {
    const std::scoped_lock lock(mutex_);
    return entries_.size();
}

std::optional<std::filesystem::path> pack_writer::publish_only() {
    const std::scoped_lock lock(mutex_);
    if (fd_ == -1)
        return std::nullopt;
    static constexpr std::array<char, ops::pack_alignment> zeros{};
    const auto pad = ops::padding_for(offset_);
    write_all(fd_, zeros.data(), pad, temporary_);
    const auto index = ops::encode_pack_index(entries_, offset_ + pad);
    write_all(fd_, index.data(), index.size(), temporary_);
    const int fd = std::exchange(fd_, -1);
    if (::close(fd) != 0) {
        ::unlink(temporary_.c_str());
        fail("cannot write", temporary_);
    }
    const auto target = dir_ / std::format("{}{:016x}{:016x}{}", pack_prefix,
        static_cast<std::uint64_t>(now_ns()), random_word(), pack_suffix);
    if (::rename(temporary_.c_str(), target.c_str()) != 0) {
        const auto error = errno;
        ::unlink(temporary_.c_str());
        errno = error;
        fail("cannot publish", target);
    }
    entries_.clear();
    offset_ = 0;
    return target;
}

std::optional<std::filesystem::path> pack_writer::publish() {
    auto published = publish_only();
    if (published) {
        try {
            repack(dir_);
        } catch (const std::system_error&) {
            // A repack that fails leaves the packs as they were, all
            // readable; the next one tries again.
        }
    }
    return published;
}

pack_lock::pack_lock(const std::filesystem::path& packs_dir, bool wait) {
    std::filesystem::create_directories(packs_dir);
    const auto path = packs_dir / "lock";
    fd_ = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (fd_ == -1)
        fail("cannot open", path);
    while (true) {
        if (::flock(fd_, LOCK_EX | (wait ? 0 : LOCK_NB)) == 0) {
            held_ = true;
            return;
        }
        if (errno == EINTR)
            continue;
        if (errno == EWOULDBLOCK && !wait)
            return;
        fail("cannot lock", path);
    }
}

pack_lock::~pack_lock() {
    if (fd_ != -1)
        ::close(fd_);
}

std::optional<std::filesystem::path> rewrite_packs(
    const std::filesystem::path& packs_dir, std::span<const open_pack> packs,
    std::span<const std::vector<std::uint8_t>> keep) {
    struct kept {
        types::pack_entry entry;
        std::size_t pack;
    };
    std::vector<kept> entries;
    for (std::size_t p = 0; p < packs.size(); ++p) {
        for (std::uint64_t row = 0; row < packs[p].view.count; ++row) {
            if (keep[p][row] != 0)
                entries.push_back({ops::entry_at(packs[p].view, row), p});
        }
    }
    std::ranges::sort(entries, [](const kept& a, const kept& b) {
        const auto& x = a.entry;
        const auto& y = b.entry;
        if (x.key != y.key)
            return x.key < y.key;
        if (x.kind != y.kind)
            return x.kind < y.kind;
        return x.written > y.written;
    });
    entries.erase(std::ranges::unique(entries, [](const kept& a,
        const kept& b) {
        return a.entry.key == b.entry.key && a.entry.kind == b.entry.kind;
    }).begin(), entries.end());

    pack_writer writer(packs_dir);
    for (const auto& k : entries)
        writer.add(k.entry.kind, k.entry.key,
            ops::bytes_of(packs[k.pack].view, k.entry), k.entry.written);
    auto published = writer.publish_only();
    for (const auto& p : packs)
        ::unlink(p.path.c_str());
    return published;
}

std::size_t repack(const std::filesystem::path& packs_dir) {
    const pack_lock lock(packs_dir, false);
    if (!lock.held())
        return 0;
    auto packs = open_packs(packs_dir);
    std::ranges::sort(packs, {}, [](const open_pack& p) {
        return p.file.size();
    });
    std::vector<std::uint64_t> sizes;
    for (const auto& p : packs)
        sizes.push_back(p.file.size());
    const auto merge = ops::packs_to_merge(sizes);
    if (merge < 2)
        return 0;
    packs.erase(packs.begin() + static_cast<std::ptrdiff_t>(merge),
        packs.end());
    std::vector<std::vector<std::uint8_t>> keep;
    for (const auto& p : packs)
        keep.emplace_back(p.view.count, std::uint8_t{1});
    rewrite_packs(packs_dir, packs, keep);
    return merge;
}

}
