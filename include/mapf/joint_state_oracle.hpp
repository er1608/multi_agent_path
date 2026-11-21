#pragma once
#include "mapf/types.hpp"
#include <cstddef>
#include <string>
namespace mapf {
enum class OracleStatus { solved, infeasible, timeout, resource_limit, invalid_input, error };
const char* status_name(OracleStatus status);
struct OracleOptions {
    double timeout_seconds = 10.0;
    std::size_t max_agents = 3;
    std::size_t max_states = 500000;
};
struct OracleResult {
    OracleStatus status = OracleStatus::error;
    Paths paths;
    long long optimal_soc = -1;
    std::size_t expanded = 0;
    std::size_t generated = 0;
    double runtime_seconds = 0.0;
    std::string message;
};
OracleResult solve_joint_state(const Instance&, const OracleOptions& = {});
}
