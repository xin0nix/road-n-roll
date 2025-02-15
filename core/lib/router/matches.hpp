//
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#pragma once

#include "boost/throw_exception.hpp"
#include <boost/url/detail/config.hpp>
#include <boost/url/string_view.hpp>

#include <span>
#include <vector>

namespace boost {
namespace urls {

// Base route match results
struct matches_base {
  using iterator = core::string_view *;
  using const_iterator = core::string_view const *;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = core::string_view &;
  using const_reference = core::string_view const &;
  using pointer = core::string_view *;
  using const_pointer = core::string_view const *;

  matches_base() = default;

  virtual ~matches_base() = default;

  virtual core::string_view const *matches() const = 0;

  virtual core::string_view const *ids() const = 0;

  virtual core::string_view *matches() = 0;

  virtual core::string_view *ids() = 0;

  virtual std::size_t size() const = 0;

  virtual void resize(std::size_t) = 0;

  const_reference at(size_type pos) const;

  const_reference at(core::string_view id) const;

  const_reference operator[](size_type pos) const;

  const_reference operator[](core::string_view id) const;

  const_iterator find(core::string_view id) const;

  const_iterator begin() const;

  const_iterator end() const;

  bool empty() const noexcept;
};

/// A range type with the match results
struct matches_storage : matches_base {
  std::vector<core::string_view> matches_storage_;
  std::vector<core::string_view> ids_storage_;

  matches_storage(std::span<core::string_view> matches,
                  std::span<core::string_view> ids) {
    BOOST_ASSERT(matches.size() == ids.size());
    resize(matches.size());
    for (std::size_t i = 0; i < matches.size(); ++i) {
      matches_storage_.push_back(matches[i]);
      ids_storage_.push_back(ids[i]);
    }
  }

  virtual core::string_view *matches() override {
    return matches_storage_.data();
  }

  virtual core::string_view *ids() override { return ids_storage_.data(); }

  matches_storage() = default;

  virtual core::string_view const *matches() const override {
    return matches_storage_.data();
  }

  virtual core::string_view const *ids() const override {
    return ids_storage_.data();
  }

  virtual std::size_t size() const override { return matches_storage_.size(); }

  virtual void resize(std::size_t n) override {
    matches_storage_.resize(n);
    ids_storage_.resize(n);
  }
};

/// Default type for storing route match results
using matches = matches_storage;

} // namespace urls
} // namespace boost
