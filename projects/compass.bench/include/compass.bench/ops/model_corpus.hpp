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
#ifndef COMPASS_BENCH_OPS_MODEL_CORPUS_HPP
#define COMPASS_BENCH_OPS_MODEL_CORPUS_HPP

#include <deque>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include "compass.model/ops/load.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"

namespace compass::bench::ops {

/**
 * @brief Whether @p path is an .org file of a modeling/ directory under
 * projects/, where ores.codegen looks for models.
 */
[[nodiscard]] bool is_model_candidate(std::string_view path);

struct parsed_document {
    std::string path;
    compass::platform::filesystem::mapped_file file;
    compass::org::types::tables tables;
    compass::org::types::tables_view view;
};

/**
 * @brief The parsed documents of a checkout, by path, parsing one the first
 * time it is asked for. Addresses are stable, so a view handed out stays
 * valid.
 */
class model_corpus final {
public:
    explicit model_corpus(const std::filesystem::path& checkout)
        : checkout_(checkout) {}

    /**
     * @brief The document at @p path, or null when there is no such file.
     */
    parsed_document* get(std::string_view path);

    /**
     * @brief A finder over this corpus, for the model loader.
     */
    [[nodiscard]] compass::model::ops::document_finder finder();

private:
    std::filesystem::path checkout_;
    std::deque<parsed_document> documents_;
    std::map<std::string, parsed_document*, std::less<>> index_;
};

[[nodiscard]] compass::model::ops::document document_of(
    const parsed_document& p);

}

#endif
