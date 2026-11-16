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
#ifndef COMPASS_CODEC_OPS_PARALLEL_FOR_HPP
#define COMPASS_CODEC_OPS_PARALLEL_FOR_HPP

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace compass::codec::ops {

/**
 * @brief Runs @p work(i) for every i below @p count on up to @p threads
 * threads, and rethrows the first exception once all have stopped.
 */
template <typename Work>
void parallel_for(std::size_t count, unsigned threads, Work&& work) {
    std::atomic<std::size_t> next{0};
    std::mutex failure_lock;
    std::exception_ptr failure;
    const auto loop = [&] {
        for (auto i = next++; i < count; i = next++) {
            try {
                work(i);
            } catch (...) {
                const std::lock_guard lock(failure_lock);
                if (!failure)
                    failure = std::current_exception();
            }
        }
    };
    {
        std::vector<std::jthread> workers;
        const auto n = std::min<std::size_t>(threads, count);
        workers.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            workers.emplace_back(loop);
    }
    if (failure)
        std::rethrow_exception(failure);
}

}

#endif
