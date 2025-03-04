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

#include <algorithm>
#include <ranges>
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

std::string_view SegmentPattern::id() const {
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

  std::string_view seg(it, end);
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

void ResourceTree::insertImpl(std::string_view path, AnyResource const *v) {
  urls::url u(path);
  std::string_view normalizedPath = u.normalize_path().encoded_path();
  if (normalizedPath.starts_with("/"))
    normalizedPath.remove_prefix(1);
  auto segsr = grammar::parse(normalizedPath, kPathPatternRule);
  if (!segsr) {
    delete v;
    throw std::invalid_argument("Не получилось распарсить путь");
  }
  auto segments = std::ranges::subrange(segsr->begin(), segsr->end());

  // Спускаемся по дереву ресурсов с корня, если нужно вставляем новые узлы
  ResourceNode &cur = nodes_.front();
  for (auto &&seg : segments) {
    // Проверим дочерний узел
    auto chPos = std::ranges::find_if(cur.children, [this, &seg](auto &&chIdx) {
      return chIdx.get().seg == seg;
    });
    if (chPos != cur.children.end()) {
      // Перемещаемся по вертикали, на дочерний узел
      cur = *chPos;
      continue;
    }
    // Нужно создать новый узел для соотв. сегмента
    auto &&child = nodes_.emplace_back();
    child.seg = seg;
    child.parent = cur;
    cur.children.push_back(std::ref(child));
    cur = child;
  }
  if (cur.resource) {
    delete cur.resource;
  }
  cur.resource = v;
  cur.pathPattern = normalizedPath;
}

ResourceNode const *ResourceTree::tryMatch(
    segments_encoded_view::const_iterator it,
    segments_encoded_view::const_iterator end, ResourceNode const *cur,
    int level, std::string_view *&matches, std::string_view *&ids) const {
  while (it != end) {
    pct_string_view s = *it;
    BOOST_ASSERT(!s.starts_with("."));

    // Определяем, нужно ли выполнять ветвление.
    // Ветвление необходимо, если есть более одного потенциального совпадения
    // или если есть нелитеральный сегмент с модификатором.
    bool needBranching = false;
    size_t matchCount = 0UL;
    // FIXME(xin0nix): нужна ли здесь проверка на cur->children.size() > 1 ??
    for (auto child : cur->children) {
      matchCount += static_cast<size_t>(child.get().seg.match(s));
    }
    if (matchCount > 1) {
      needBranching = true;
    }

    // Попытка сопоставить каждый дочерний узел
    ResourceNode const *r = nullptr;
    bool matchesAny = false;
    for (auto &cc : cur->children) {
      auto &&c = cc.get();
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
                                          std::string_view *&matches,
                                          std::string_view *&ids) const {
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
