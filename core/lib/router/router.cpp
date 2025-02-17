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
  if (is_literal_)
    return *seg == str_;

  // Другие узлы матчатся с любой строкой
  return true;
}

core::string_view SegmentPattern::id() const {
  // if (is_literal_) return {};
  BOOST_ASSERT(!is_literal());
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
  SegmentPattern t;
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

        t.str_ = core::string_view(it0, send + 1);
        t.is_literal_ = false;
        if (s.ends_with('?'))
          t.modifier_ = SegmentPattern::modifier::optional;
        else if (s.ends_with('*'))
          t.modifier_ = SegmentPattern::modifier::star;
        else if (s.ends_with('+'))
          t.modifier_ = SegmentPattern::modifier::plus;
        return t;
      }
    }
    it = it0;
  }
  // literal segment
  auto rv = grammar::parse(it, end, urls::detail::segment_rule);
  BOOST_ASSERT(rv);
  rv->decode({}, urls::string_token::assign_to(t.str_));
  t.is_literal_ = true;
  return t;
}

ResourceNode const *ResourceTree::find_optional_resource(
    const ResourceNode *root, std::vector<ResourceNode> const &ns,
    core::string_view *&matches, core::string_view *&ids) {
  BOOST_ASSERT(root);
  if (root->resource)
    return root;
  BOOST_ASSERT(!root->child_idx.empty());
  for (auto i : root->child_idx) {
    auto &c = ns[i];
    if (!c.seg.is_optional() && !c.seg.is_star())
      continue;
    // Child nodes are also
    // potentially optional.
    auto matches0 = matches;
    auto ids0 = ids;
    *matches++ = {};
    *ids++ = c.seg.id();
    auto n = find_optional_resource(&c, ns, matches, ids);
    if (n)
      return n;
    matches = matches0;
    ids = ids0;
  }
  return nullptr;
}

void ResourceTree::insert_impl(core::string_view non_normalized_path,
                               router_base::any_resource const *v) {
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
        cur->child_idx.begin(), cur->child_idx.end(),
        [this, &it](std::size_t ci) -> bool { return nodes_[ci].seg == *it; });
    if (cit != cur->child_idx.end()) {
      // move to existing child
      cur = &nodes_[*cit];
    } else {
      // create child if it doesn't exist
      ResourceNode child;
      child.seg = *it;
      std::size_t cur_id = cur - nodes_.data();
      child.parent_idx = cur_id;
      nodes_.push_back(std::move(child));
      nodes_[cur_id].child_idx.push_back(nodes_.size() - 1);
      if (nodes_[cur_id].child_idx.size() > 1) {
        // FIXME(xin0nix): std::sort?
        // keep nodes sorted
        auto &cs = nodes_[cur_id].child_idx;
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
  cur->path_template = path;
}

ResourceNode const *ResourceTree::try_match(
    segments_encoded_view::const_iterator it,
    segments_encoded_view::const_iterator end, ResourceNode const *cur,
    int level, core::string_view *&matches, core::string_view *&ids) const {
  while (it != end) {
    pct_string_view s = *it;
    BOOST_ASSERT(!s.starts_with("."));

    // calculate the lower bound on the
    // possible number of branches to
    // determine if we need to branch.
    // We branch when we might have more than
    // one child matching node at this level.
    // If so, we need to potentially branch
    // to find which path leads to a valid
    // resource. Otherwise, we can just
    // consume the node and input without
    // any recursive function calls.
    bool branch = false;
    if (cur->child_idx.size() > 1) {
      int branches_lb = 0;
      for (auto i : cur->child_idx) {
        auto &c = nodes_[i];
        if (c.seg.is_literal() || !c.seg.has_modifier()) {
          // a literal path counts only
          // if it matches
          branches_lb += c.seg.match(s);
        } else {
          // everything not matching
          // a single path counts as
          // more than one path already
          branches_lb = 2;
        }
        if (branches_lb > 1) {
          // already know we need to
          // branch
          branch = true;
          break;
        }
      }
    }

    // attempt to match each child node
    ResourceNode const *r = nullptr;
    bool match_any = false;
    for (auto i : cur->child_idx) {
      auto &c = nodes_[i];
      if (c.seg.match(s)) {
        if (c.seg.is_literal()) {
          // just continue from the
          // next segment
          if (branch) {
            r = try_match(std::next(it), end, &c, level, matches, ids);
            if (r)
              break;
          } else {
            cur = &c;
            match_any = true;
            break;
          }
        } else if (!c.seg.has_modifier()) {
          // just continue from the
          // next segment
          if (branch) {
            auto matches0 = matches;
            auto ids0 = ids;
            *matches++ = *it;
            *ids++ = c.seg.id();
            r = try_match(std::next(it), end, &c, level, matches, ids);
            if (r) {
              break;
            } else {
              // rewind
              matches = matches0;
              ids = ids0;
            }
          } else {
            // only path possible
            *matches++ = *it;
            *ids++ = c.seg.id();
            cur = &c;
            match_any = true;
            break;
          }
        } else if (c.seg.is_optional()) {
          // attempt to match by ignoring
          // and not ignoring the segment.
          // we first try the complete
          // continuation consuming the
          // input, which is the
          // longest and most likely
          // match
          auto matches0 = matches;
          auto ids0 = ids;
          *matches++ = *it;
          *ids++ = c.seg.id();
          r = try_match(std::next(it), end, &c, level, matches, ids);
          if (r)
            break;
          // rewind
          matches = matches0;
          ids = ids0;
          // try complete continuation
          // consuming no segment
          *matches++ = {};
          *ids++ = c.seg.id();
          r = try_match(it, end, &c, level, matches, ids);
          if (r)
            break;
          // rewind
          matches = matches0;
          ids = ids0;
        } else {
          // check if the next segments
          // won't send us to a parent
          // directory
          auto first = it;
          BOOST_ASSERT(!it->starts_with("."));

          // attempt to match many
          // segments
          auto matches0 = matches;
          auto ids0 = ids;
          *matches++ = *it;
          *ids++ = c.seg.id();
          // if this is a plus seg, we
          // already consumed the first
          // segment
          if (c.seg.is_plus()) {
            ++first;
          }
          // {*} is usually the last
          // match in a path.
          // try complete continuation
          // match for every subrange
          // from {last, last} to
          // {first, last}.
          // We also try {last, last}
          // first because it is the
          // longest match.
          auto start = end;
          while (start != first) {
            r = try_match(start, end, &c, level, matches, ids);
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
          // start == first
          matches = matches0 + 1;
          ids = ids0 + 1;
          r = try_match(start, end, &c, level, matches, ids);
          if (r) {
            if (!c.seg.is_plus())
              *matches0 = {};
            break;
          }
        }
      }
    }
    // r represent we already found a terminal
    // node which is a match
    if (r)
      return r;
    // if we couldn't match anything, we go
    // one level up in the implicit tree
    // because the path might still have a
    // "..".
    if (!match_any)
      ++level;
    ++it;
  }

  // the path ended below or above an
  // existing node
  BOOST_ASSERT(level != 0);

  if (!cur->resource) {
    // we consumed all the input and reached
    // a node with no resource, but it might
    // still have child optional segments
    // with resources we can reach without
    // consuming any input
    return find_optional_resource(cur, nodes_, matches, ids);
  }
  return cur;
}

router_base::any_resource const *
ResourceTree::find_impl(segments_encoded_view path, core::string_view *&matches,
                        core::string_view *&ids) const {
  // parse_path is inconsistent for empty paths
  if (path.empty())
    path = segments_encoded_view("./");

  // Iterate nodes from the root
  ResourceNode const *p =
      try_match(path.begin(), path.end(), &nodes_.front(), 0, matches, ids);
  if (p)
    return p->resource;
  return nullptr;
}

router_base::router_base() : impl_(new ResourceTree{}) {}

router_base::~router_base() { delete reinterpret_cast<ResourceTree *>(impl_); }

void router_base::insert_impl(core::string_view s, any_resource const *v) {
  reinterpret_cast<ResourceTree *>(impl_)->insert_impl(s, v);
}

auto router_base::find_impl(segments_encoded_view path,
                            core::string_view *&matches,
                            core::string_view *&ids) const noexcept
    -> any_resource const * {
  return reinterpret_cast<ResourceTree *>(impl_)->find_impl(path, matches, ids);
}

} // namespace detail
} // namespace urls
} // namespace boost
