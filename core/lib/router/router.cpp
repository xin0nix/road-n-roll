//
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url

#include "router.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/url/decode_view.hpp>
#include <boost/url/detail/replacement_field_rule.hpp>
#include <boost/url/grammar/alnum_chars.hpp>
#include <boost/url/grammar/token_rule.hpp>
#include <boost/url/grammar/tuple_rule.hpp>
#include <boost/url/grammar/variant_rule.hpp>
#include <boost/url/rfc/detail/path_rules.hpp>
#include <boost/url/url.hpp>

#include <vector>

namespace boost {
namespace urls {
namespace router {

bool SegmentPattern::match(pct_string_view seg) const {
  if (isLiteral_)
    return *seg == str_;

  // Другие узлы матчатся с любой строкой
  return true;
}

core::string_view SegmentPattern::id() const {
  BOOST_ASSERT(!isLiteral());
  return {str_};
}

auto SegmentPatternRule::parse(char const *&it, char const *end) const noexcept
    -> system::result<value_type> {
  SegmentPattern segmentPattern;
  // Поле замены, например "/{name}", где name это идентификатор
  static constexpr auto idRule = grammar::tuple_rule(
      grammar::squelch(grammar::delim_rule('{')), detail::identifier_rule,
      grammar::squelch(grammar::delim_rule('}')));

  core::string_view seg(it, end);
  if (auto res = grammar::parse(seg, idRule); res) {
    segmentPattern.str_ = *res;
    segmentPattern.isLiteral_ = false;
    return segmentPattern;
  }

  // Литеральный сегмент
  auto rv = grammar::parse(it, end, urls::detail::segment_rule);
  BOOST_ASSERT(rv);
  rv->decode({}, urls::string_token::assign_to(segmentPattern.str_));
  segmentPattern.isLiteral_ = true;
  return segmentPattern;
}

void ResourceTree::insertImpl(core::string_view non_normalized_path,
                              AnyResource const *v) {
  urls::url u(non_normalized_path);
  core::string_view path = u.normalize_path().encoded_path();
  // Parse dynamic route segments
  if (path.starts_with("/"))
    path.remove_prefix(1);
  auto segsr = grammar::parse(path, kPathPatternRule);
  if (!segsr) {
    delete v;
    std::ignore = segsr.value();
  }
  auto segs = *segsr;
  auto it = segs.begin();
  auto end = segs.end();

  // Iterate existing nodes
  ResourceNode *cur = &nodes_.front();
  int level = 0;
  while (it != end) {
    core::string_view seg = (*it).string();
    BOOST_ASSERT(!seg.starts_with("."));
    // look for child
    auto cit = std::find_if(
        cur->children.begin(), cur->children.end(),
        [this, &it](std::size_t ci) -> bool { return nodes_[ci].seg == *it; });
    if (cit != cur->children.end()) {
      // move to existing child
      cur = &nodes_[*cit];
    } else {
      // create child if it doesn't exist
      ResourceNode child;
      child.seg = *it;
      std::size_t cur_id = cur - nodes_.data();
      child.parent = cur_id;
      nodes_.push_back(std::move(child));
      nodes_[cur_id].children.push_back(nodes_.size() - 1);
      if (nodes_[cur_id].children.size() > 1) {
        // FIXME(xin0nix): std::sort?
        // keep nodes sorted
        auto &cs = nodes_[cur_id].children;
        std::size_t n = cs.size() - 1;
        while (n) {
          if (nodes_[cs.begin()[n]].seg < nodes_[cs.begin()[n - 1]].seg)
            std::swap(cs.begin()[n], cs.begin()[n - 1]);
          else
            break;
          --n;
        }
      }
      cur = &nodes_.back();
    }
    ++it;
  }
  BOOST_ASSERT(level != 0);
  cur->resource = v;
  cur->pathPattern = path;
}

ResourceNode const *ResourceTree::tryMatch(
    segments_encoded_view::const_iterator it,
    segments_encoded_view::const_iterator end, ResourceNode const *cur,
    int level, core::string_view *&matches, core::string_view *&ids) const {
  while (it != end) {
    pct_string_view s = *it;
    BOOST_ASSERT(!s.starts_with("."));

    // Определяем, нужно ли выполнять ветвление.
    // Ветвление необходимо, если есть более одного потенциального совпадения
    // или если есть нелитеральный сегмент с модификатором.
    bool needBranching = false;
    size_t matchCount = 0UL;
    // FIXME(xin0nix): нужна ли здесь проверка на cur->children.size() > 1 ??
    for (auto childIdx : cur->children) {
      auto &child = nodes_[childIdx];
      matchCount += static_cast<size_t>(child.seg.match(s));
    }
    if (matchCount > 1) {
      needBranching = true;
    }

    // Попытка сопоставить каждый дочерний узел
    ResourceNode const *r = nullptr;
    bool matchesAny = false;
    for (auto i : cur->children) {
      auto &c = nodes_[i];
      if (c.seg.match(s)) {
        if (c.seg.isLiteral()) {
          // Просто продолжаем со следующего сегмента
          if (needBranching) {
            if (r = tryMatch(std::next(it), end, &c, level, matches, ids); r) {
              return r;
            }
          } else {
            cur = &c;
            matchesAny = true;
            break;
          }
        } else {
          // Просто продолжаем со следующего сегмента
          if (needBranching) {
            auto matches0 = matches;
            auto ids0 = ids;
            *matches++ = *it;
            *ids++ = c.seg.id();
            if (r = tryMatch(std::next(it), end, &c, level, matches, ids); r) {
              return r;
            }
            // "Откат"
            matches = matches0;
            ids = ids0;
          } else {
            // Найден единственный возможный путь для продолжения поиска
            *matches++ = *it;
            *ids++ = c.seg.id();
            cur = &c;
            matchesAny = true;
            break;
          }
        }
      }
    }
    if (r)
      return r;
    ++it;
  }

  // Путь закончился ниже или выше существующего узла
  BOOST_ASSERT(level == 0);

  if (!cur->resource) {
    // Мы обработали весь входной путь и достигли
    // узла без ресурса
    return nullptr;
  }
  return cur;
}

AnyResource const *ResourceTree::findImpl(segments_encoded_view path,
                                          core::string_view *&matches,
                                          core::string_view *&ids) const {
  if (path.empty())
    path = segments_encoded_view("./");
  ResourceNode const *p =
      tryMatch(path.begin(), path.end(), &nodes_.front(), 0, matches, ids);
  if (p)
    return p->resource;
  return nullptr;
}

} // namespace router
} // namespace urls
} // namespace boost
