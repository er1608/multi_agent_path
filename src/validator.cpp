#include "mapf/validator.hpp"
#include <algorithm>
#include <cstdlib>

namespace mapf {
bool is_free(const Instance& instance, Position p) {
    return p.row >= 0 && p.col >= 0 &&
           static_cast<std::size_t>(p.row) < instance.grid.size() &&
           static_cast<std::size_t>(p.col) < instance.grid[p.row].size() &&
           instance.grid[p.row][p.col] == '.';
}

std::string instance_error(const Instance& instance) {
    if (instance.grid.empty() || instance.grid.front().empty()) return "empty grid";
    for (const auto& row : instance.grid) {
        if (row.size() != instance.grid.front().size()) return "non-rectangular grid";
        for (char cell : row)
            if (cell != '.' && cell != '@' && cell != '#') return "unsupported grid cell";
    }
    if (instance.starts.empty() || instance.starts.size() != instance.goals.size())
        return "invalid agent count";
    for (std::size_t i = 0; i < instance.starts.size(); ++i) {
        if (!is_free(instance, instance.starts[i]) || !is_free(instance, instance.goals[i]))
            return "start or goal is blocked/out of bounds";
        for (std::size_t j = 0; j < i; ++j) {
            if (instance.starts[i] == instance.starts[j]) return "duplicate starts";
            if (instance.goals[i] == instance.goals[j]) return "duplicate goals";
        }
    }
    return {};
}

ValidationResult validate(const Instance& instance, const Paths& paths) {
    const auto fail = [](const std::string& message) {
        return ValidationResult{false, message, 0, 0};
    };
    const auto error = instance_error(instance);
    if (!error.empty()) return fail(error);
    if (paths.size() != instance.starts.size()) return fail("path count differs from agent count");
    std::size_t horizon = 0;
    ValidationResult result{true, "ok", 0, 0};
    for (std::size_t i = 0; i < paths.size(); ++i) {
        const auto& path = paths[i];
        if (path.empty()) return fail("empty path for agent " + std::to_string(i));
        if (path.front() != instance.starts[i]) return fail("wrong start");
        if (path.back() != instance.goals[i]) return fail("wrong goal");
        for (std::size_t t = 0; t < path.size(); ++t) {
            if (!is_free(instance, path[t])) return fail("blocked/out-of-bounds position");
            if (t > 0 && std::abs(path[t].row - path[t-1].row) +
                             std::abs(path[t].col - path[t-1].col) > 1)
                return fail("illegal move");
        }
        std::size_t arrival = path.size() - 1;
        while (arrival > 0 && path[arrival-1] == instance.goals[i]) --arrival;
        result.soc += arrival;
        result.makespan = std::max(result.makespan, arrival);
        horizon = std::max(horizon, path.size() - 1);
    }
    const auto at = [&paths](std::size_t agent, std::size_t time) {
        return paths[agent][std::min(time, paths[agent].size()-1)];
    };
    for (std::size_t t = 0; t <= horizon; ++t) {
        for (std::size_t i = 0; i < paths.size(); ++i) {
            for (std::size_t j = i+1; j < paths.size(); ++j) {
                if (at(i,t) == at(j,t)) return fail("vertex conflict at t=" + std::to_string(t));
                if (t > 0 && at(i,t-1) == at(j,t) && at(j,t-1) == at(i,t))
                    return fail("edge-swap conflict at t=" + std::to_string(t));
            }
        }
    }
    return result;
}
ValidationResult validate_with_constraints(const Instance& in, const Paths& paths,
                                          const std::vector<Constraint>& constraints) {
    auto result=validate(in,paths);
    if(!result.valid) return result;
    auto at=[&](std::size_t i,std::size_t t) {
        return paths[i][std::min(t,paths[i].size()-1)];
    };
    for(const auto& c:constraints) {
        if(c.agent>=paths.size() || !is_free(in,c.to) ||
           (c.edge && (!c.time || !is_free(in,c.from))))
            return {false,"invalid constraint",0,0};
        if(at(c.agent,c.time)==c.to &&
           (!c.edge || at(c.agent,c.time-1)==c.from))
            return {false,"constraint violated at t="+std::to_string(c.time),0,0};
    }
    return result;
}
}  // namespace mapf
