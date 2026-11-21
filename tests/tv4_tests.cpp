#include "mapf/joint_state_oracle.hpp"
#include "mapf/validator.hpp"
#include <cstdlib>
#include <iostream>

void require(bool condition, const char* name) {
    if (!condition) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); }
}
int main() {
    using namespace mapf;
    const Instance single{{"...", ".#."}, {{0,0}}, {{0,2}}};
    auto result = validate(single, {{{0,0}, {0,0}, {0,1}, {0,2}, {0,2}}});
    require(result.valid && result.soc == 3 && result.makespan == 3, "wait cost");
    result = validate(single, {{{0,0}, {0,1}, {0,2}, {0,1}, {0,2}}});
    require(result.valid && result.soc == 4, "leave goal and return");
    require(!validate(single, {{{0,0}, {0,2}}}).valid, "teleport rejected");
    require(!validate(single, {{{0,0}, {0,1}, {1,1}, {0,1}, {0,2}}}).valid, "wall rejected");
    require(!validate(single, {}).valid, "missing path");
    require(!validate(single, {Path{}}).valid, "empty path");
    require(!validate(single, {{{0,0}, {0,1}}}).valid, "wrong goal");
    const Instance swap{{".."}, {{0,0}, {0,1}}, {{0,1}, {0,0}}};
    require(!validate(swap, {{{0,0}, {0,1}}, {{0,1}, {0,0}}}).valid, "edge swap");
    const Instance vertex{{"...", "..."}, {{0,0}, {1,1}}, {{0,2}, {0,1}}};
    require(!validate(vertex, {{{0,0}, {0,1}, {0,2}}, {{1,1}, {0,1}}}).valid, "vertex conflict");
    const Instance held{{"...", "..."}, {{0,0}, {0,2}}, {{0,0}, {1,0}}};
    require(!validate(held, {{{0,0}}, {{0,2}, {0,1}, {0,0}, {1,0}}}).valid,
            "stay-at-target after short path ends");
    const Instance stationary{{"."}, {{0,0}}, {{0,0}}};
    require(validate(stationary, {{{0,0}, {0,0}}}).soc == 0, "zero arrival cost");
    auto solved=solve_joint_state(single);
    require(solved.status==OracleStatus::solved && solved.optimal_soc==2, "single oracle optimum");
    OracleOptions options;
    options.timeout_seconds = 0;
    require(solve_joint_state(single, options).status == OracleStatus::invalid_input,
            "bad oracle options");
    std::cout << "All TV4 checks passed\n";
}
