#pragma once

#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>

#include <boost/hana.hpp>

namespace database {
namespace hana = boost::hana;

using RowFields =
    std::unordered_map<std::string, std::variant<std::string, int16_t, int32_t,
                                                 int64_t, float, double>>;

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