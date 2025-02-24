//
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url

#include "router.hpp"
#include "boost/core/detail/string_view.hpp"
#include "boost/url/url.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <boost/url/decode_view.hpp>
#include <boost/url/detail/replacement_field_rule.hpp>
#include <boost/url/grammar/alnum_chars.hpp>
#include <boost/url/grammar/alpha_chars.hpp>
#include <boost/url/grammar/lut_chars.hpp>
#include <boost/url/grammar/optional_rule.hpp>
#include <boost/url/grammar/token_rule.hpp>
#include <boost/url/grammar/variant_rule.hpp>
#include <boost/url/rfc/detail/path_rules.hpp>

#include <vector>

namespace boost {
namespace urls {
namespace detail {

bool SegmentPattern::match(pct_string_view seg) const {
  if (isLiteral_)
    return *seg == str_;

  // Другие узлы матчатся с любой строкой
  return true;
}

core::string_view SegmentPattern::id() const {
  // if (is_literal_) return {};
  BOOST_ASSERT(!isLiteral());
  core::string_view r = {str_};
  r.remove_prefix(1);
  r.remove_suffix(1);
  if (r.ends_with('?') || r.ends_with('+') || r.ends_with('*'))
    r.remove_suffix(1);
  return r;
}

// Паттерн сегмента - это либо строковый литерал, либо поле замены (как в
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

constexpr auto kSegmentPatternRule = SegmentPatternRule{};

constexpr auto kPathPatternRule = grammar::tuple_rule(
    grammar::squelch(grammar::optional_rule(grammar::delim_rule('/'))),
    grammar::range_rule(
        kSegmentPatternRule,
        grammar::tuple_rule(grammar::squelch(grammar::delim_rule('/')),
                            kSegmentPatternRule)));

auto SegmentPatternRule::parse(char const *&it, char const *end) const noexcept
    -> system::result<value_type> {
  SegmentPattern segmentPattern;
  if (it != end && *it == '{') {
    // replacement field
    auto it0 = it;
    ++it;
    auto send = grammar::find_if(it, end, grammar::lut_chars('}'));
    if (send != end) {
      core::string_view s(it, send);
      static constexpr auto modifiers_cs = grammar::lut_chars("?*+");
      static constexpr auto id_rule = grammar::tuple_rule(
          grammar::optional_rule(arg_id_rule),
          grammar::optional_rule(grammar::delim_rule(modifiers_cs)));
      if (s.empty() || grammar::parse(s, id_rule)) {
        it = send + 1;

        segmentPattern.str_ = core::string_view(it0, send + 1);
        segmentPattern.isLiteral_ = false;
        if (s.ends_with('?'))
          segmentPattern.modifier_ = SegmentPattern::modifier::optional;
        else if (s.ends_with('*'))
          segmentPattern.modifier_ = SegmentPattern::modifier::star;
        else if (s.ends_with('+'))
          segmentPattern.modifier_ = SegmentPattern::modifier::plus;
        return segmentPattern;
      }
    }
    it = it0;
  }
  // literal segment
  auto rv = grammar::parse(it, end, urls::detail::segment_rule);
  BOOST_ASSERT(rv);
  rv->decode({}, urls::string_token::assign_to(segmentPattern.str_));
  segmentPattern.isLiteral_ = true;
  return segmentPattern;
}

ResourceNode const *ResourceTree::findOptionalResource(
    const ResourceNode *root, std::vector<ResourceNode> const &nodes,
    core::string_view *&matches, core::string_view *&ids) {
  BOOST_ASSERT(root);
  if (root->resource)
    return root;
  BOOST_ASSERT(!root->children.empty());
  for (auto childIdx : root->children) {
    auto &cur = nodes[childIdx];
    if (!cur.seg.isOptional() && !cur.seg.isStar())
      continue;
    // Child nodes are also
    // potentially optional.
    auto matches0 = matches;
    auto ids0 = ids;
    *matches++ = {};
    *ids++ = cur.seg.id();
    auto node = findOptionalResource(&cur, nodes, matches, ids);
    if (node)
      return node;
    matches = matches0;
    ids = ids0;
  }
  return nullptr;
}

void ResourceTree::insertImpl(core::string_view non_normalized_path,
                              Routerbase::AnyResource const *v) {
  urls::url u(non_normalized_path);
  core::string_view path = u.normalize_path().encoded_path();
  // Parse dynamic route segments
  if (path.starts_with("/"))
    path.remove_prefix(1);
  auto segsr = grammar::parse(path, detail::kPathPatternRule);
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

    // Вычисляем нижнюю границу возможного количества ветвей, чтобы определить,
    // нужно ли выполнять ветвление. Мы выполняем ветвление, когда на этом
    // уровне может быть более одного совпадающего дочернего узла. Если это так,
    // нам потенциально нужно выполнить ветвление, чтобы найти, какой путь ведет
    // к действительному ресурсу. В противном случае мы можем просто обработать
    // узел и входные данные без каких-либо рекурсивных вызовов функции.
    bool branch = false;
    if (cur->children.size() > 1) {
      int branchesLowerBound = 0;
      for (auto i : cur->children) {
        auto &c = nodes_[i];
        if (c.seg.isLiteral() || !c.seg.hasModifier()) {
          // Литеральный путь учитывается только если он совпадает
          branchesLowerBound += c.seg.match(s);
        } else {
          // Всё, что не совпадает с одним конкретным путем, уже считается более
          // чем одним путем
          branchesLowerBound = 2;
        }
        if (branchesLowerBound > 1) {
          // Уже знаем, что нам нужно выполнить ветвление
          branch = true;
          break;
        }
      }
    }

    // Попытка сопоставить каждый дочерний узел
    ResourceNode const *r = nullptr;
    bool match_any = false;
    for (auto i : cur->children) {
      auto &c = nodes_[i];
      if (c.seg.match(s)) {
        if (c.seg.isLiteral()) {
          // Просто продолжаем со следующего сегмента
          if (branch) {
            r = tryMatch(std::next(it), end, &c, level, matches, ids);
            if (r)
              break;
          } else {
            cur = &c;
            match_any = true;
            break;
          }
        } else if (!c.seg.hasModifier()) {
          // Просто продолжаем со следующего сегмента
          if (branch) {
            auto matches0 = matches;
            auto ids0 = ids;
            *matches++ = *it;
            *ids++ = c.seg.id();
            r = tryMatch(std::next(it), end, &c, level, matches, ids);
            if (r) {
              break;
            } else {
              // "Откат" (или "перемотка назад") происходит, когда алгоритм
              // пробует определенный путь в дереве маршрутизации, но этот путь
              // не приводит к успешному сопоставлению. В этом случае алгоритм
              // "откатывается" назад, возвращая указатели matches и ids в их
              // предыдущее состояние.
              matches = matches0;
              ids = ids0;
            }
          } else {
            // Найден единственный возможный путь для продолжения поиска
            *matches++ = *it;
            *ids++ = c.seg.id();
            cur = &c;
            match_any = true;
            break;
          }
        } else if (c.seg.isOptional()) {
          // Попытка сопоставления, игнорируя и не игнорируя сегмент. Сначала
          // пробуем полное продолжение, потребляя входные данные, что является
          // самым длинным и наиболее вероятным совпадением
          auto matches0 = matches;
          auto ids0 = ids;
          *matches++ = *it;
          *ids++ = c.seg.id();
          r = tryMatch(std::next(it), end, &c, level, matches, ids);
          if (r)
            break;
          // "Откат" (или "перемотка назад")
          matches = matches0;
          ids = ids0;
          // Пробуем полное продолжение, не потребляя сегмент
          *matches++ = {};
          *ids++ = c.seg.id();
          r = tryMatch(it, end, &c, level, matches, ids);
          if (r)
            break;
          // "Откат" (или "перемотка назад")
          matches = matches0;
          ids = ids0;
        } else {
          // Проверяем, не отправят ли нас следующие сегменты в родительский
          // каталог (это запрещено, на вход должны идти нормализованные пути)
          auto first = it;
          BOOST_ASSERT(!it->starts_with("."));

          // Попытка сопоставить множество сегментов
          auto matches0 = matches;
          auto ids0 = ids;
          *matches++ = *it;
          *ids++ = c.seg.id();
          // Если это сегмент с плюсом, мы уже обработали первый сегмент
          if (c.seg.isPlus()) {
            ++first;
          }
          // {*} обычно является последним
          // совпадением в пути.
          // Пробуем полное продолжение
          // сопоставления для каждого подинтервала
          // от {последний, последний} до
          // {первый, последний}.
          // Мы также пробуем {последний, последний}
          // сначала, потому что это
          // самое длинное совпадение.
          auto start = end;
          while (start != first) {
            r = tryMatch(start, end, &c, level, matches, ids);
            if (r) {
              core::string_view prev = *std::prev(start);
              *matches0 = {matches0->data(), prev.data() + prev.size()};
              break;
            }
            matches = matches0 + 1;
            ids = ids0 + 1;
            --start;
          }
          if (r) {
            break;
          }
          // первый == последний
          matches = matches0 + 1;
          ids = ids0 + 1;
          r = tryMatch(start, end, &c, level, matches, ids);
          if (r) {
            if (!c.seg.isPlus())
              *matches0 = {};
            break;
          }
        }
      }
    }
    // r представляет собой уже найденный терминальный узел, который является
    // совпадением
    if (r)
      return r;
    // FIXME(xin0nix): удалить
    // // if we couldn't match anything, we go
    // // one level up in the implicit tree
    // // because the path might still have a
    // // "..".
    // if (!match_any)
    //   ++level;
    ++it;
  }

  // Путь закончился ниже или выше существующего узла
  BOOST_ASSERT(level != 0);

  if (!cur->resource) {
    // Мы обработали весь входной путь и достигли
    // узла без ресурса, но у него все еще могут
    // быть дочерние опциональные сегменты
    // с ресурсами, которые мы можем достичь, не
    // потребляя дополнительного входного пути
    return findOptionalResource(cur, nodes_, matches, ids);
  }
  return cur;
}

Routerbase::AnyResource const *
ResourceTree::findImpl(segments_encoded_view path, core::string_view *&matches,
                       core::string_view *&ids) const {
  if (path.empty())
    path = segments_encoded_view("./");
  ResourceNode const *p =
      tryMatch(path.begin(), path.end(), &nodes_.front(), 0, matches, ids);
  if (p)
    return p->resource;
  return nullptr;
}

Routerbase::Routerbase() : impl_(new ResourceTree{}) {}

Routerbase::~Routerbase() { delete reinterpret_cast<ResourceTree *>(impl_); }

void Routerbase::insertImpl(core::string_view s, AnyResource const *v) {
  reinterpret_cast<ResourceTree *>(impl_)->insertImpl(s, v);
}

auto Routerbase::findImpl(segments_encoded_view path,
                          core::string_view *&matches,
                          core::string_view *&ids) const noexcept
    -> AnyResource const * {
  return reinterpret_cast<ResourceTree *>(impl_)->findImpl(path, matches, ids);
}

} // namespace detail
} // namespace urls
} // namespace boost
