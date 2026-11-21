#pragma once
#include "mapf/types.hpp"
#include <functional>
#include <string>
namespace mapf {
struct BypassResult {
    bool accepted = false;
    std::size_t parent_conflicts = 0, candidate_conflicts = 0;
    std::string message;
};
// Checks equal per-agent costs and strictly fewer conflicts. The mandatory
// predicate must check ALL parent CT constraints on candidate paths.
// This is an eligibility hook, not a CT mutation or a standalone CBS solver.
BypassResult evaluate_bypass(const Instance&, const Paths& parent, const Paths& candidate,
                            const std::function<bool(const Paths&)>& satisfies_parent_constraints);
}
