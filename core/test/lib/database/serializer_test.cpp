#include <boost/hana.hpp>
#include <gtest/gtest.h>
#include <stdexcept>

using namespace std::string_literals;

#include "serializer.hpp"

namespace {
struct Person {
  BOOST_HANA_DEFINE_STRUCT(Person, (int64_t, personId),
                           (std::string, personName));
};

struct Empty {
  BOOST_HANA_DEFINE_STRUCT(Empty);
};
} // namespace

TEST(SerializerTest, BasicStructureUnpack) {
  {
    database::RowFields types{{"personId", int64_t(42)},
                              {"personName", "Bob"s}};
    SCOPED_TRACE("Success");
    auto res = database::unpack<Person>(types);
    ASSERT_EQ(res.personName, "Bob");
    ASSERT_EQ(res.personId, 42);
  }
  {
    database::RowFields types{{"personId", float(.42)},
                              {"personName", "junk"s}};
    SCOPED_TRACE("Type mismatch");
    EXPECT_THROW(database::unpack<Person>(types), std::bad_variant_access);
  }
  {
    SCOPED_TRACE("Empty field records");
    database::RowFields types{};
    EXPECT_THROW(database::unpack<Person>(types), std::out_of_range);
  }
  {
    SCOPED_TRACE("Not enough field records");
    database::RowFields types{{"personId", int64_t(42)}};
    EXPECT_THROW(database::unpack<Person>(types), std::out_of_range);
  }
  {
    SCOPED_TRACE("Too many field records");
    database::RowFields types{
        {"personId", int64_t(42)}, {"personName", "Bob"s}, {"pi", float(3.14)}};
    EXPECT_THROW(database::unpack<Person>(types), std::out_of_range);
  }
}

TEST(SerializerTest, BasicStructurePack) {
  {
    SCOPED_TRACE("Normal case");
    Person person{.personId = 101, .personName = "Jimmy"};
    auto fields = database::pack(person);
    EXPECT_EQ(std::get<int64_t>(fields.at("personId")), int64_t(101));
    EXPECT_EQ(std::get<std::string>(fields.at("personName")), "Jimmy"s);
    EXPECT_EQ(fields.size(), 2);
  }
  {
    SCOPED_TRACE("Empty structure");
    Empty empty{};
    auto fields = database::pack(empty);
    EXPECT_TRUE(fields.empty());
  }
}