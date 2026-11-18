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

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <sys/wait.h>
#include <unistd.h>
#include "compass.store/io/collection.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "git_repository.hpp"

namespace {

using namespace compass::store;
using types::action_kind;

struct listing {
    std::size_t packs = 0;
    std::size_t temporaries = 0;
    std::size_t other = 0;
};

listing list(const std::filesystem::path& dir) {
    listing l;
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        const auto name = e.path().filename().string();
        if (io::is_pack_name(name))
            ++l.packs;
        else if (name.starts_with("tmp_"))
            ++l.temporaries;
        else if (name != "lock")
            ++l.other;
    }
    return l;
}

/**
 * @brief Runs @p body in a child process and returns the child's ID. The
 * child exits 0 when @p body returns true, 1 when it returns false or
 * throws.
 */
pid_t spawn(const std::function<bool()>& body) {
    const auto pid = ::fork();
    if (pid == 0) {
        bool ok = false;
        try {
            ok = body();
        } catch (...) {
            ok = false;
        }
        ::_exit(ok ? 0 : 1);
    }
    return pid;
}

bool succeeded(pid_t pid) {
    int status = 0;
    return ::waitpid(pid, &status, 0) == pid && WIFEXITED(status) &&
        WEXITSTATUS(status) == 0;
}

std::string body_of(const std::string& name) {
    return "the body of " + name + std::string(name.size() * 7 % 61, '.');
}

}

TEST_CASE("many processes writing, reading, repacking and collecting one "
    "store at once lose nothing and leave a bounded number of files",
    "[io][pack_store][concurrency]") {
    tests::git_repository repo;
    repo.write("a.txt", "A");
    repo.git("add a.txt");
    repo.git("commit -q -m base");
    const auto packs = io::packs_dir(io::git_common_dir(repo.root()));
    const auto stop = repo.root().parent_path() / "stop";

    constexpr int writers = 6;
    constexpr int rounds = 25;
    constexpr int per_round = 40;
    const auto name = [](int w, int round, int k) {
        return "w" + std::to_string(w) + "/r" + std::to_string(round) + "/k" +
            std::to_string(k);
    };
    const auto shared = [](int k) { return "shared/k" + std::to_string(k); };

    // Each writer publishes a pack a round, each publish repacking, and
    // after it reads back everything it has written so far: what a run
    // wrote is never lost to another's merge or collection.
    std::vector<pid_t> children;
    for (int w = 0; w < writers; ++w) {
        children.push_back(spawn([&, w] {
            for (int round = 0; round < rounds; ++round) {
                io::pack_writer out(packs);
                for (int k = 0; k < per_round; ++k) {
                    const auto n = name(w, round, k);
                    out.add_action(action_kind::result, ops::blob_id_of(n),
                        body_of(n));
                }
                out.add_action(action_kind::result,
                    ops::blob_id_of(shared(round)), body_of(shared(round)));
                out.add_action(action_kind::manifest, ops::blob_id_of("m"),
                    name(w, round, 0));
                if (!out.publish())
                    return false;
                const io::pack_reader in(packs);
                for (int r = 0; r <= round; ++r) {
                    for (int k = 0; k < per_round; ++k) {
                        const auto n = name(w, r, k);
                        if (in.read_action(action_kind::result,
                                ops::blob_id_of(n)) != body_of(n))
                            return false;
                    }
                }
                if (!in.read_action(action_kind::manifest,
                        ops::blob_id_of("m")))
                    return false;
            }
            return true;
        }));
    }
    const auto writer_count = children.size();

    // Readers open the store over and over; whatever they find is whole.
    for (int r = 0; r < 2; ++r) {
        children.push_back(spawn([&] {
            while (!std::filesystem::exists(stop)) {
                const io::pack_reader in(packs);
                for (int w = 0; w < writers; ++w) {
                    for (int round = 0; round < rounds; round += 5) {
                        const auto n = name(w, round, round % per_round);
                        const auto body = in.read_action(action_kind::result,
                            ops::blob_id_of(n));
                        if (body && *body != body_of(n))
                            return false;
                    }
                }
            }
            return true;
        }));
    }

    // A collector rewrites every pack into one, again and again. Every
    // entry is within its grace period, so it may drop none of them.
    children.push_back(spawn([&] {
        while (!std::filesystem::exists(stop)) {
            const auto c = io::collect(repo.root(), {0, std::chrono::hours(1),
                false}, [](const std::filesystem::path&) {
                    return std::vector<types::blob_id>{};
                });
            if (c.plan.victims.objects != 0)
                return false;
        }
        return true;
    }));

    bool all = true;
    for (std::size_t i = 0; i < writer_count; ++i)
        all = succeeded(children[i]) && all;
    std::ofstream(stop) << "stop";
    for (std::size_t i = writer_count; i < children.size(); ++i)
        all = succeeded(children[i]) && all;
    CHECK(all);

    const io::pack_reader in(packs);
    std::size_t found = 0;
    for (int w = 0; w < writers; ++w) {
        for (int round = 0; round < rounds; ++round) {
            for (int k = 0; k < per_round; ++k) {
                const auto n = name(w, round, k);
                found += in.read_action(action_kind::result,
                    ops::blob_id_of(n)) == body_of(n) ? 1 : 0;
            }
        }
    }
    CHECK(found == std::size_t{writers} * rounds * per_round);
    for (int round = 0; round < rounds; ++round)
        CHECK(in.read_action(action_kind::result,
            ops::blob_id_of(shared(round))) == body_of(shared(round)));
    // A publish skips its repack while the collector holds the lock, so
    // the bound holds once a repack has run.
    (void)io::repack(packs);
    const auto files = list(packs);
    CHECK(files.temporaries == 0);
    CHECK(files.other == 0);
    CHECK(files.packs <= 12);
    INFO("packs left: " << files.packs);
}
