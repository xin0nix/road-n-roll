#include "serializer.hpp"

#include <sstream>

namespace database {
std::ostream &operator<<(std::ostream &os, const Field &field) {
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
  };
  std::visit(visitor, field);
  return os;
}

std::string stringify(const Field &field) {
  std::stringstream sss;
  sss << field;
  return sss.str();
}
} // namespace database