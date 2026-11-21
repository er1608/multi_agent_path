#pragma once

#include <string>
#include <vector>

namespace mapf {
struct Position {
    int row = 0;
    int col = 0;
    bool operator==(const Position& other) const {
        return row == other.row && col == other.col;
    }
    bool operator!=(const Position& other) const { return !(*this == other); }
};

// Temporary interchange format: '.' is free and '@' or '#' is blocked.
struct Instance {
    std::vector<std::string> grid;
    std::vector<Position> starts;
    std::vector<Position> goals;
};
using Path = std::vector<Position>;
using Paths = std::vector<Path>;

bool is_free(const Instance& instance, Position position);
// Empty means valid. Shared input checks only; collision checking stays in validator.
std::string instance_error(const Instance& instance);
}  // namespace mapf
