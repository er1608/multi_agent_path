#include "mapf/analysis.hpp"
#include "mapf/bypass.hpp"
#include "mapf/io.hpp"
#include "mapf/joint_state_oracle.hpp"
#include "mapf/validator.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
void check(bool ok,const char* message) {
    if(!ok) { std::cerr<<"FAIL "<<message<<'\n'; std::exit(1); }
}
int main() {
    using namespace mapf;
    Instance tiny{{"...","..."},{{0,0},{1,2}},{{0,2},{1,0}}};
    auto result=solve_joint_state(tiny);
    check(result.status==OracleStatus::solved && result.optimal_soc==4,"two agent SOC");
    check(validate(tiny,result.paths).valid,"oracle independent validation");
    auto lb=independent_lower_bound(tiny);
    check(lb.individually_reachable && lb.soc==4,"independent LB");
    Instance triple{{"...","...","..."},{{0,0},{1,0},{2,0}},{{0,2},{1,2},{2,2}}};
    result=solve_joint_state(triple);
    check(result.status==OracleStatus::solved && result.optimal_soc==6,"three agents");
    Instance leave{{"...","#.#"},{{0,1},{0,0}},{{0,1},{0,2}}};
    result=solve_joint_state(leave);
    check(result.status==OracleStatus::solved && result.optimal_soc==4,"leave goal necessary");
    check(result.paths[0].size()==3 && result.paths[0][1]==Position{1,1},"goal excursion");
    Instance swap{{".."},{{0,0},{0,1}},{{0,1},{0,0}}};
    check(solve_joint_state(swap).status==OracleStatus::infeasible,"corridor swap infeasible");
    Instance detour{{"..",".."},{{0,0},{0,1}},{{0,1},{0,0}}};
    result=solve_joint_state(detour);
    check(result.status==OracleStatus::solved && result.optimal_soc==4,"swap detour SOC");
    Instance blocked{{".#."},{{0,0}},{{0,2}}};
    check(solve_joint_state(blocked).status==OracleStatus::infeasible,"disconnected");
    check(!independent_lower_bound(blocked).individually_reachable,"unreachable LB");
    Instance stationary{{"."},{{0,0}},{{0,0}}};
    result=solve_joint_state(stationary);
    check(result.status==OracleStatus::solved && result.optimal_soc==0,"zero optimum");
    OracleOptions options; options.max_states=1;
    check(solve_joint_state(tiny,options).status==OracleStatus::resource_limit,"resource limit");
    options=OracleOptions{}; options.timeout_seconds=1e-12;
    check(solve_joint_state(tiny,options).status==OracleStatus::timeout,"timeout");
    options.timeout_seconds=std::numeric_limits<double>::quiet_NaN();
    check(solve_joint_state(tiny,options).status==OracleStatus::invalid_input,"NaN rejected");
    auto gap=compute_gap(12,10,true);
    check(gap.valid && gap.exact && *gap.relative_gap==.2,"exact gap");
    check(compute_gap(12,8,false).relative_gap==.5,"LB gap");
    check(!compute_gap(8,10,true).valid,"bad bound");
    check(!compute_gap(0,0,true).relative_gap,"zero ratio undefined");
    check(!compute_gap(1,0,true).valid,"zero optimum contradiction");
    check(!validate_with_constraints(stationary,{{{0,0}}},
            {{0,100,{0,0},{0,0},false}}).valid,"future goal constraint");
    check(!validate_with_constraints(tiny,{{{0,0},{0,1},{0,2}},{{1,2},{1,1},{1,0}}},
            {{0,1,{0,0},{0,1},true}}).valid,"edge constraint");
    Instance b{{"...","..."},{{0,0},{0,2}},{{1,2},{1,0}}};
    Paths parent{{{0,0},{0,1},{0,2},{1,2}},{{0,2},{0,1},{0,0},{1,0}}};
    Paths candidate{{{0,0},{1,0},{1,1},{1,2}},{{0,2},{0,1},{0,0},{1,0}}};
    auto bypass=evaluate_bypass(b,parent,candidate,[](const Paths&) {return true;});
    check(bypass.accepted && bypass.parent_conflicts==1 && bypass.candidate_conflicts==0,"bypass");
    check(!evaluate_bypass(b,parent,candidate,[](const Paths&) {return false;}).accepted,"constraint rejection");
    check(!evaluate_bypass(b,parent,parent,[](const Paths&) {return true;}).accepted,"no improvement");
    check(!evaluate_bypass(b,parent,candidate,{}).accepted,"no constraint callback");
    std::stringstream stream;
    write_paths(stream,candidate);
    check(read_paths(stream)==candidate,"path IO roundtrip");
    // Exhaust all one-agent pairs on a tiny obstacle map; BFS is an independent optimum.
    Instance one{{"...",".#.","..."},{}, {}};
    for(int a=0;a<9;++a) for(int z=0;z<9;++z) {
        Position s{a/3,a%3}, g{z/3,z%3};
        if(!is_free(one,s) || !is_free(one,g)) continue;
        one.starts={s}; one.goals={g};
        result=solve_joint_state(one);
        check(result.status==OracleStatus::solved &&
              result.optimal_soc==independent_lower_bound(one).soc,"all single-agent BFS comparisons");
    }
    std::cout<<"All oracle/analysis/bypass checks passed\n";
}
