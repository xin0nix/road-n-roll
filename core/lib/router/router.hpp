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

// Паттерн сегмента пути к ресурсу
struct SegmentPattern {
  SegmentPattern() = default;

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

  friend bool operator==(SegmentPattern const &a, SegmentPattern const &b) {
    if (a.is_literal_ != b.is_literal_)
      return false;
    if (a.is_literal_)
      return a.str_ == b.str_;
    return a.modifier_ == b.modifier_;
  }

  // Сегменты имеют следующий приоритет:
  //     - литерал
  //     - уникальный
  //     - опциональный
  //     - плюс
  //     - звездочка
  friend bool operator<(SegmentPattern const &a, SegmentPattern const &b) {
    if (b.is_literal())
      return false;
    if (a.is_literal())
      return !b.is_literal();
    return a.modifier_ < b.modifier_;
  }

private:
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

  friend struct SegmentPatternRule;
};

// Паттерн сегмента - это либо литеральная строка, либо поле замены (как в
// format_string). Поля не могут содержать спецификаторы формата, но могут иметь
// один из следующих модификаторов:
// - ?: опциональный сегмент
// - *: ноль или более сегментов
// - +: один или более сегментов
struct SegmentPatternRule {
  using value_type = SegmentPattern;

  system::result<value_type> parse(char const *&it,
                                   char const *end) const noexcept;
};

struct router_base {
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

private:
  void *impl_{nullptr};
};

using ChildIdxVector = std::vector<std::size_t>;

// Узел в дереве ресурсов
// Каждый сегмент в дереве ресурсов может быть связан с:
struct ResourceNode {
  static constexpr std::size_t npos{std::size_t(-1)};

  // Литеральный сегмент или поле замены
  SegmentPattern seg{};

  // FIXME(xin0nix): меня смущает сырой указатель
  // Указатель на ресурс
  router_base::any_resource const *resource{nullptr};

  // Полное совпадение для ресурса
  std::string path_template;

  // Индекс родительского узла в реализации пула узлов
  std::size_t parent_idx{npos};

  // Индексы дочерних узлов в пуле узлов
  ChildIdxVector child_idx;
};

struct ResourceTree {
  ResourceTree() {
    // Корневой узел без каких-либо связанных с ним ресурсов
    nodes_.push_back(ResourceNode{});
  }

  ~ResourceTree() {
    for (auto &r : nodes_)
      delete r.resource;
  }

  // Добавим узел связанный с ресурсом
  void insert_impl(core::string_view path, router_base::any_resource const *v);

  // match a node and return the element
  router_base::any_resource const *find_impl(segments_encoded_view path,
                                             core::string_view *&matches,
                                             core::string_view *&ids) const;

private:
  // try to match from this root node
  ResourceNode const *try_match(segments_encoded_view::const_iterator it,
                                segments_encoded_view::const_iterator end,
                                ResourceNode const *root, int level,
                                core::string_view *&matches,
                                core::string_view *&ids) const;

  // check if a node has a resource when we
  // also consider optional paths through
  // the child nodes.
  static ResourceNode const *
  find_optional_resource(const ResourceNode *root,
                         std::vector<ResourceNode> const &ns,
                         core::string_view *&matches, core::string_view *&ids);

  // Pool of nodes in the resource tree
  std::vector<ResourceNode> nodes_;
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
