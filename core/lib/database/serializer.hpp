#pragma once

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <ostream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>

#include <boost/hana.hpp>

namespace {
template <typename... Ts> struct Overload : Ts... {
  using Ts::operator()...;
};
template <class... Ts> Overload(Ts...) -> Overload<Ts...>;
} // namespace

namespace database {
namespace hana = boost::hana;

using Field = std::variant<std::monostate, std::string, boost::uuids::uuid,
                           int16_t, int32_t, int64_t, float>;
using RowFields = std::unordered_map<std::string, Field>;

inline std::ostream &operator<<(std::ostream &os, const Field &field) {
  auto visitor = Overload{
      [&os](std::monostate) { os << "NULL"; },
      [&os](std::string s) { os << "'" << s << "'"; },
      [&os](boost::uuids::uuid u) {
        os << "'" << boost::uuids::to_string(u) << "'::uuid";
      },
      [&os](int16_t x) { os << x; },
      [&os](int32_t x) { os << x; },
      [&os](int64_t x) { os << x; },
      [&os](float x) { os << x; },
  }; // namespace database
  std::visit(visitor, field);
  return os;
}

inline std::string stringify(const Field &field);

template <typename T> T unpack(RowFields fields) {
  T object{};
  constexpr auto accessors = hana::accessors<T>();
  if (hana::length(accessors) != fields.size()) {
    throw std::out_of_range("Too many fields");
  }
  hana::for_each(accessors, [&](auto &&member) {
    auto memberName = hana::first(member).c_str();
    auto memberAccessor = hana::second(member);
    auto fieldValue = fields.at(memberName);
    using MemberType = std::remove_cvref_t<decltype(memberAccessor(object))>;
    // TODO: forward
    memberAccessor(object) = std::get<MemberType>(fieldValue);
  });
  return object;
}

template <typename T> RowFields pack(T &&object) {
  RowFields fields;
  using ObjectType = std::remove_reference_t<T>;
  constexpr auto accessors = hana::accessors<ObjectType>();
  hana::for_each(accessors, [&](auto &&member) {
    auto memberName = hana::first(member).c_str();
    auto memberAccessor = hana::second(member);
    // TODO: forward
    fields[memberName] = memberAccessor(object);
  });
  return fields;
}

} // namespace database