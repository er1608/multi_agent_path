#pragma once
#include "mapf/types.hpp"
#include <cstddef>
#include <string>

namespace mapf {
struct ValidationResult {
    bool valid = false;
    std::string reason;
    // Metrics are meaningful only if valid == true.
    std::size_t soc = 0;
    std::size_t makespan = 0;
};

// Paths have position at t=0. Shorter paths stay at their last position forever.
// SOC is the sum of final arrival times, excluding trailing waits at the goal.
ValidationResult validate(const Instance& instance, const Paths& paths);
// Vertex: forbid 'to' at time. Edge: forbid from(t-1) -> to(t).
struct Constraint {
    std::size_t agent = 0;
    std::size_t time = 0;
    Position from;
    Position to;
    bool edge = false;
};
ValidationResult validate_with_constraints(const Instance&, const Paths&,
                                          const std::vector<Constraint>&);
}  // namespace mapf
