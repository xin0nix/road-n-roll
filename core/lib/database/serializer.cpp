#include "serializer.hpp"

#include <sstream>

namespace database {
std::string stringify(const Field &field) {
  std::stringstream sss;
  sss << field;
  return sss.str();
}
} // namespace database