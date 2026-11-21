#include "mapf/analysis.hpp"
#include "mapf/bypass.hpp"
#include "mapf/joint_state_oracle.hpp"
#include "mapf/validator.hpp"
#include <iostream>
int main() {
    const mapf::Instance instance{{"...", "..."}, {{0,0}, {1,2}}, {{0,2}, {1,0}}};
    const mapf::Paths paths{{{0,0}, {0,1}, {0,2}}, {{1,2}, {1,1}, {1,0}}};
    const auto check = mapf::validate(instance, paths);
    std::cout << "valid=" << check.valid << " SOC=" << check.soc
              << " makespan=" << check.makespan << '\n';
    const auto oracle = mapf::solve_joint_state(instance);
    std::cout << "oracle=" << mapf::status_name(oracle.status)
              << " optimal_SOC=" << oracle.optimal_soc << '\n';
    const auto bound=mapf::independent_lower_bound(instance);
    std::cout << "independent_LB=" << bound.soc << '\n';
    const mapf::Instance b{{"...","..."},{{0,0},{0,2}},{{1,2},{1,0}}};
    const mapf::Paths parent{{{0,0},{0,1},{0,2},{1,2}},{{0,2},{0,1},{0,0},{1,0}}};
    const mapf::Paths candidate{{{0,0},{1,0},{1,1},{1,2}},{{0,2},{0,1},{0,0},{1,0}}};
    // This example has no CT constraints, so the predicate is vacuously true.
    const auto bypass=mapf::evaluate_bypass(b,parent,candidate,[](const mapf::Paths&){return true;});
    std::cout << "bypass=" << bypass.accepted << " conflicts=" << bypass.parent_conflicts
              << " -> " << bypass.candidate_conflicts << '\n';
    return check.valid && oracle.status==mapf::OracleStatus::solved && bypass.accepted ? 0 : 1;
}
