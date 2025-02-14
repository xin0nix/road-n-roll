//
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#pragma once

#include <boost/mp11/algorithm.hpp>
#include <boost/url/decode_view.hpp>
#include <boost/url/detail/config.hpp>
#include <boost/url/detail/except.hpp>
#include <boost/url/grammar/unsigned_rule.hpp>
#include <boost/url/parse_path.hpp>

#include "matches.hpp"

namespace boost {
namespace urls {

namespace detail {

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
  BOOST_STATIC_ASSERT(std::is_same<T, U>::value ||
                      std::is_convertible<U, T>::value ||
                      std::is_base_of<T, U>::value);
  using U_ = typename std::decay<typename std::conditional<
      std::is_base_of<T, U>::value, U, T>::type>::type;
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
