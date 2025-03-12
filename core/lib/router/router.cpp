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
  // Литералы требуют полного соответствия
  if (isLiteral_)
    return *seg == str_;

  // Поле замены, такое как "{name}" может быть чем угодно
  return true;
}

std::string_view SegmentPattern::id() const {
  BOOST_ASSERT(!isLiteral());
  return {str_};
}

auto SegmentPatternRule::parse(char const *&it, char const *end) const noexcept
    -> system::result<value_type> {
  // Поле замены, например "{name}", где name это идентификатор
  static constexpr auto idRule = grammar::tuple_rule(
      grammar::squelch(grammar::delim_rule('{')), detail::identifier_rule,
      grammar::squelch(grammar::delim_rule('}')));

  BOOST_ASSERT(*it != '/');
  SegmentPattern segmentPattern;
  auto next = std::find_if(it, end, [](char c) { return c == '/'; });

  std::string_view segment(it, next);
  it = next;

  if (auto res = grammar::parse(segment, idRule); res) {
    segmentPattern.str_ = *res;
    segmentPattern.isLiteral_ = false;
    return segmentPattern;
  }

  // Литеральный сегмент
  auto rv = grammar::parse(segment, urls::detail::segment_rule);
  BOOST_ASSERT(rv);
  rv->decode({}, urls::string_token::assign_to(segmentPattern.str_));
  segmentPattern.isLiteral_ = true;
  return segmentPattern;
}

void ResourceTree::insertImpl(std::string_view path, AnyResource const *v) {
  auto segsr = grammar::parse(path, kPathPatternRule);
  BOOST_ASSERT(segsr);
  auto segments = std::ranges::subrange(segsr->begin(), segsr->end());
  // Спускаемся по дереву ресурсов с корня, если нужно вставляем новые узлы
  size_t curIdx = 0UL;
  auto getNode = [this](size_t idx) -> auto & { return nodes_.at(idx); };
  for (auto &&seg : segments) {
    // Проверим дочерний узел
    auto &cur = getNode(curIdx);
    auto chPos = std::ranges::find_if(cur.children, [this, &seg](auto &&chIdx) {
      return nodes_.at(chIdx).seg == seg;
    });
    if (chPos != cur.children.end()) {
      // Перемещаемся по вертикали, на дочерний узел
      curIdx = *chPos;
      continue;
    }
    // Нужно создать новый узел для соотв. сегмента
    auto chIdx = nodes_.size();
    auto &&child = nodes_.emplace_back();
    child.seg = seg;
    child.parent = curIdx;
    cur.children.push_back(chIdx);
    curIdx = chIdx;
  }
  auto &cur = getNode(curIdx);
  cur.resource.reset(v);
}

ResourceNode const *
ResourceTree::tryMatch(segments_encoded_view::const_iterator it,
                       segments_encoded_view::const_iterator end,
                       ResourceNode const *cur, MatchesStorage &matches) const {
  for (; it != end; ++it) {
    pct_string_view segment = *it;
    BOOST_ASSERT(!segment.starts_with("."));

    // Определяем, нужно ли выполнять ветвление.
    // Ветвление необходимо, если есть более одного потенциального совпадения
    // или если есть нелитеральный сегмент с модификатором.
    bool needBranching = false;
    size_t matchCount = std::ranges::count_if(cur->children, [&](auto child) {
      return nodes_.at(child).seg.match(segment);
    });
    // Совпадений не найдено - смысла искать дальше попросту нет
    if (matchCount == 0) {
      return nullptr;
    }
    if (matchCount > 1) {
      needBranching = true;
    }

    // Мы знаем что среди дочерних узлов есть совпадения, чекнем их
    for (auto chIdx : cur->children) {
      const auto &child = nodes_.at(chIdx);
      // Мы знаем что один из дочерних узлов точно совпадает, идём к следующему
      // Здесь есть избыточность, ведь мы уже могли посетить нужный узел
      if (!child.seg.match(segment)) {
        continue;
      }
      // Литеральный сегмент
      if (child.seg.isLiteral()) {
        if (!needBranching) {
          // FIXME(xin0nix): почему мы не обновляем matches здесь?
          // Идём вглубь, мы нашли единственный вариант на этом уровне
          cur = &child;
          break;
        }
        if (auto *res = tryMatch(std::next(it), end, &child, matches); res) {
          return res;
        }
        // Продолжаем горизонтальный поиск
        continue;
      }
      // Поле замены
      auto &&id = child.seg.id();
      BOOST_ASSERT(!matches.contains(id));
      matches[id] = segment;
      if (!needBranching) {
        // Идём вглубь, мы нашли единственный вариант на этом уровне
        cur = &child;
        break;
      }
      if (auto *res = tryMatch(std::next(it), end, &child, matches); res) {
        return res;
      }
      // "Перемотка", нужно удалить сохранённое совпадение для поле замены
      matches.erase(id);
    }
  }

  if (!cur->resource) {
    // Мы обработали весь входной путь и достигли
    // узла без ресурса
    return nullptr;
  }
  return cur;
}

AnyResource const *ResourceTree::findImpl(segments_encoded_view path,
                                          MatchesStorage &matches) const {
  if (ResourceNode const *p =
          tryMatch(path.begin(), path.end(), &nodes_.front(), matches);
      p) {
    return p->resource.get();
  }
  return nullptr;
}

} // namespace router
} // namespace urls
} // namespace boost
