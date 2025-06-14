#pragma once

#include "serializer.hpp"

namespace database {
struct AbstractDatabase {
    virtual void insert(std::string tableName, RowFields fields) = 0;
};
} // namespace database