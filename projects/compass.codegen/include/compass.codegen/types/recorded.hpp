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
#ifndef COMPASS_CODEGEN_TYPES_RECORDED_HPP
#define COMPASS_CODEGEN_TYPES_RECORDED_HPP

#include <concepts>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace compass::codegen::types {

/**
 * @brief The names of the facts one model's generation read, each once, in
 * the order of the first read.
 */
class fact_reads final {
public:
    void add(std::string name) {
        if (seen_.insert(name).second)
            names_.push_back(std::move(name));
    }

    [[nodiscard]] const std::vector<std::string>& names() const noexcept {
        return names_;
    }

private:
    std::vector<std::string> names_;
    std::unordered_set<std::string> seen_;
};

/**
 * @brief The separator of a fact's collection and its key, and of the
 * parts of a key that is a tuple.
 */
inline constexpr char fact_separator = '\0';
inline constexpr char key_separator = '\x1f';

inline void put_fact_key(std::string& out, std::string_view key) {
    out += key;
}

template <typename... T>
void put_fact_key(std::string& out, const std::tuple<T...>& key) {
    bool first = true;
    std::apply([&](const auto&... part) {
        ((out += first ? "" : std::string(1, key_separator), out += part,
          first = false), ...);
    }, key);
}

/**
 * @brief The name of the fact that is the element at @p key of the
 * collection @p collection.
 */
template <typename Key>
[[nodiscard]] std::string fact_name(std::string_view collection,
    const Key& key) {
    std::string name(collection);
    name.push_back(fact_separator);
    put_fact_key(name, key);
    return name;
}

/**
 * @brief A collection derived from many models that says what was read of
 * it, so that the generation of one model depends only on the elements it
 * looked up.
 * A lookup by key records the fact fact_name(name, key), found or not.
 * Anything that sees every element (iteration, the size, the conversion to
 * the collection) records the fact named name, the whole collection. end()
 * records nothing. edit() and the non-const operations build the
 * collection and record nothing.
 * A copy shares the elements of its source and records to the same
 * fact_reads until record_to gives it another, so a model's generation
 * takes a copy that records to its own reads at the cost of a pointer.
 */
template <typename Container>
class recorded final {
public:
    using const_iterator = typename Container::const_iterator;

    explicit recorded(std::string_view name) : name_(name) {}

    recorded& operator=(Container c) {
        *data_ = std::move(c);
        return *this;
    }

    void record_to(fact_reads* reads) noexcept { reads_ = reads; }

    [[nodiscard]] std::string_view name() const noexcept { return name_; }

    [[nodiscard]] Container& edit() noexcept { return *data_; }

    [[nodiscard]] const Container& unrecorded() const noexcept {
        return *data_;
    }

    template <typename Key>
    [[nodiscard]] decltype(auto) operator[](Key&& key) {
        return (*data_)[std::forward<Key>(key)];
    }

    template <typename... Args>
    decltype(auto) emplace(Args&&... args) {
        return data_->emplace(std::forward<Args>(args)...);
    }

    template <typename Value>
    void push_back(Value&& value) {
        data_->push_back(std::forward<Value>(value));
    }

    template <typename Key>
    [[nodiscard]] const_iterator find(const Key& key) const {
        element(key);
        return data_->find(key);
    }

    template <typename Key>
    [[nodiscard]] bool contains(const Key& key) const {
        element(key);
        return data_->contains(key);
    }

    [[nodiscard]] bool contains(
        const typename Container::value_type& key) const
        requires std::same_as<typename Container::value_type,
            typename Container::key_type> {
        element(key);
        return data_->contains(key);
    }

    [[nodiscard]] const_iterator begin() const {
        whole();
        return data_->begin();
    }

    [[nodiscard]] const_iterator end() const noexcept { return data_->end(); }

    [[nodiscard]] std::size_t size() const {
        whole();
        return data_->size();
    }

    [[nodiscard]] bool empty() const {
        whole();
        return data_->empty();
    }

    [[nodiscard]] operator const Container&() const {
        whole();
        return *data_;
    }

    [[nodiscard]] friend bool operator==(const recorded& a,
        const Container& b) {
        return *a.data_ == b;
    }

private:
    template <typename Key>
    void element(const Key& key) const {
        if (reads_ != nullptr)
            reads_->add(fact_name(name_, key));
    }

    void whole() const {
        if (reads_ != nullptr)
            reads_->add(std::string(name_));
    }

    std::shared_ptr<Container> data_ = std::make_shared<Container>();
    std::string_view name_;
    fact_reads* reads_ = nullptr;
};

}

#endif
