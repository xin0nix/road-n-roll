//
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#pragma once

#include <boost/url/decode_view.hpp>
#include <boost/url/detail/config.hpp>
#include <boost/url/detail/except.hpp>
#include <boost/url/grammar/unsigned_rule.hpp>
#include <boost/url/parse_path.hpp>

#include "matches.hpp"

namespace boost {
namespace urls {

namespace detail {

// A path segment template
class segment_template {
  enum class modifier : unsigned char {
    none,
    // {id?}
    optional,
    // {id*}
    star,
    // {id+}
    plus
  };

  std::string str_;
  bool is_literal_ = true;
  modifier modifier_ = modifier::none;

  friend struct segment_template_rule_t;

public:
  segment_template() = default;

  bool match(pct_string_view seg) const;

  core::string_view string() const { return str_; }

  core::string_view id() const;

  bool empty() const { return str_.empty(); }

  bool is_literal() const { return is_literal_; }

  bool has_modifier() const {
    return !is_literal() && modifier_ != modifier::none;
  }

  bool is_optional() const { return modifier_ == modifier::optional; }

  bool is_star() const { return modifier_ == modifier::star; }

  bool is_plus() const { return modifier_ == modifier::plus; }

  friend bool operator==(segment_template const &a, segment_template const &b) {
    if (a.is_literal_ != b.is_literal_)
      return false;
    if (a.is_literal_)
      return a.str_ == b.str_;
    return a.modifier_ == b.modifier_;
  }

  // segments have precedence:
  //     - literal
  //     - unique
  //     - optional
  //     - plus
  //     - star
  friend bool operator<(segment_template const &a, segment_template const &b) {
    if (b.is_literal())
      return false;
    if (a.is_literal())
      return !b.is_literal();
    return a.modifier_ < b.modifier_;
  }
};

// A segment template is either a literal string
// or a replacement field (as in a format_string).
// Fields cannot contain format specs and might
// have one of the following modifiers:
// - ?: optional segment
// - *: zero or more segments
// - +: one or more segments
struct segment_template_rule_t {
  using value_type = segment_template;

  system::result<value_type> parse(char const *&it,
                                   char const *end) const noexcept;
};

class router_base {
  void *impl_{nullptr};

public:
  // A type-erased router resource
  struct any_resource {
    virtual ~any_resource() = default;
    virtual void const *get() const noexcept = 0;
  };

protected:
  router_base();

  virtual ~router_base();

  void insert_impl(core::string_view s, any_resource const *v);

  any_resource const *find_impl(segments_encoded_view path,
                                core::string_view *&matches,
                                core::string_view *&names) const noexcept;
};

// a small vector for child nodes...
// we shouldn't expect many children per node, and
// we don't want to allocate for that. But we also
// cannot cap the max number of child nodes because
// especially the root nodes can potentially an
// exponentially higher number of child nodes.
class child_idx_vector {
  static constexpr std::size_t N = 5;
  std::size_t static_child_idx_[N]{};
  std::size_t *child_idx{nullptr};
  std::size_t size_{0};
  std::size_t cap_{0};

public:
  ~child_idx_vector() { delete[] child_idx; }

  child_idx_vector() = default;

  child_idx_vector(child_idx_vector const &other)
      : size_{other.size_}, cap_{other.cap_} {
    if (other.child_idx) {
      child_idx = new std::size_t[cap_];
      std::memcpy(child_idx, other.child_idx, size_ * sizeof(std::size_t));
      return;
    }
    std::memcpy(static_child_idx_, other.static_child_idx_,
                size_ * sizeof(std::size_t));
  }

  child_idx_vector(child_idx_vector &&other)
      : child_idx{other.child_idx}, size_{other.size_}, cap_{other.cap_} {
    std::memcpy(static_child_idx_, other.static_child_idx_, N);
    other.child_idx = nullptr;
  }

  bool empty() const { return size_ == 0; }

  std::size_t size() const { return size_; }

  std::size_t *begin() {
    if (child_idx)
      return child_idx;
    return static_child_idx_;
  }

  std::size_t *end() { return begin() + size_; }

  std::size_t const *begin() const {
    if (child_idx)
      return child_idx;
    return static_child_idx_;
  }

  std::size_t const *end() const { return begin() + size_; }

  void erase(std::size_t *it) {
    BOOST_ASSERT(it - begin() >= 0);
    std::memmove(it - 1, it, end() - it);
    --size_;
  }

  void push_back(std::size_t v) {
    if (size_ == N && !child_idx) {
      child_idx = new std::size_t[N * 2];
      cap_ = N * 2;
      std::memcpy(child_idx, static_child_idx_, N * sizeof(std::size_t));
    } else if (child_idx && size_ == cap_) {
      auto *tmp = new std::size_t[cap_ * 2];
      std::memcpy(tmp, child_idx, cap_ * sizeof(std::size_t));
      delete[] child_idx;
      child_idx = tmp;
      cap_ = cap_ * 2;
    }
    begin()[size_++] = v;
  }
};

// A node in the resource tree
// Each segment in the resource tree might be
// associated with
struct node {
  static constexpr std::size_t npos{std::size_t(-1)};

  // literal segment or replacement field
  detail::segment_template seg{};

  // A pointer to the resource
  router_base::any_resource const *resource{nullptr};

  // The complete match for the resource
  std::string path_template;

  // Index of the parent node in the
  // implementation pool of nodes
  std::size_t parent_idx{npos};

  // Index of child nodes in the pool
  detail::child_idx_vector child_idx;
};

class impl {
  // Pool of nodes in the resource tree
  std::vector<node> nodes_;

public:
  impl() {
    // root node with no resource
    nodes_.push_back(node{});
  }

  ~impl() {
    for (auto &r : nodes_)
      delete r.resource;
  }

  // include a node for a resource
  void insert_impl(core::string_view path, router_base::any_resource const *v);

  // match a node and return the element
  router_base::any_resource const *find_impl(segments_encoded_view path,
                                             core::string_view *&matches,
                                             core::string_view *&ids) const;

private:
  // try to match from this root node
  node const *try_match(segments_encoded_view::const_iterator it,
                        segments_encoded_view::const_iterator end,
                        node const *root, int level,
                        core::string_view *&matches,
                        core::string_view *&ids) const;

  // check if a node has a resource when we
  // also consider optional paths through
  // the child nodes.
  static node const *find_optional_resource(const node *root,
                                            std::vector<node> const &ns,
                                            core::string_view *&matches,
                                            core::string_view *&ids);
};

} // namespace detail

/** A URL router.

    This container matches static and dynamic
    URL requests to an object which represents
    how the it should be handled. These
    values are usually callback functions.

    @tparam T type of resource associated with
    each path template

    @tparam N maximum number of replacement fields
    in a path template

    @par Exception Safety

    @li Functions marked `noexcept` provide the
    no-throw guarantee, otherwise:

    @li Functions which throw offer the strong
    exception safety guarantee.

    @see
        @ref parse_absolute_uri,
        @ref parse_relative_ref,
        @ref parse_uri,
        @ref parse_uri_reference,
        @ref resolve.
*/
template <class T> class router : private detail::router_base {
public:
  /// Constructor
  router() = default;

  /** Route the specified URL path to a resource

      @param path A url path with dynamic segments
      @param resource A resource the path corresponds to
   */
  template <class U> void insert(core::string_view pattern, U &&v);

  /** Match URL path to corresponding resource

      @param request Request path
      @return The match results
   */
  T const *find(segments_encoded_view path, matches_base &m) const noexcept;
};

template <class T>
template <class U>
void router<T>::insert(core::string_view pattern, U &&v) {
  BOOST_STATIC_ASSERT(std::is_same_v<T, U> || std::is_convertible_v<U, T> ||
                      std::is_base_of_v<T, U>);
  using U_ = typename std::decay<
      typename std::conditional<std::is_base_of_v<T, U>, U, T>::type>::type;

  struct impl : any_resource {
    U_ u;

    explicit impl(U &&u_) : u(std::forward<U>(u_)) {}

    void const *get() const noexcept override {
      return static_cast<T const *>(&u);
    }
  };

  any_resource const *p = new impl(std::forward<U>(v));
  insert_impl(pattern, p);
}

template <class T>
T const *router<T>::find(segments_encoded_view path,
                         matches_base &m) const noexcept {
  core::string_view *matches_it = m.matches();
  core::string_view *ids_it = m.ids();
  any_resource const *p = find_impl(path, matches_it, ids_it);
  if (p) {
    BOOST_ASSERT(matches_it >= m.matches());
    m.resize(static_cast<std::size_t>(matches_it - m.matches()));
    return reinterpret_cast<T const *>(p->get());
  }
  m.resize(0);
  return nullptr;
}

} // namespace urls
} // namespace boost
