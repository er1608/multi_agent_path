#pragma once
#include "mapf/types.hpp"
#include <optional>
#include <string>
#include <vector>
namespace mapf {
struct LowerBoundResult {
    bool valid_input = false;
    bool individually_reachable = false;
    long long soc = -1;
    std::vector<int> distances;
    std::string message;
};
LowerBoundResult independent_lower_bound(const Instance&);
struct GapResult {
    bool valid = false;
    bool exact = false;
    std::optional<double> relative_gap;
    std::optional<double> ratio;
    std::string message;
};
// A caller-supplied optimum/certified bound must come from an audited source.
GapResult compute_gap(long long soc, long long bound, bool is_optimum);
}
