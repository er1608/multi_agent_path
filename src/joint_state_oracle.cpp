#include "mapf/joint_state_oracle.hpp"
#include "mapf/validator.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <unordered_map>
#include <utility>

namespace mapf {
const char* status_name(OracleStatus s) {
    switch(s) {
        case OracleStatus::solved: return "solved";
        case OracleStatus::infeasible: return "infeasible";
        case OracleStatus::timeout: return "timeout";
        case OracleStatus::resource_limit: return "resource_limit";
        case OracleStatus::invalid_input: return "invalid_input";
        default: return "error";
    }
}
namespace {
// Every agent contributes (cell, committed). Commitment has zero cost and freezes
// an agent forever; until commitment, every synchronous step costs one per agent.
using State = std::vector<int>;
struct Hash {
    std::size_t operator()(const State& s) const {
        std::size_t h=0;
        for (int x:s) h ^= std::hash<int>{}(x)+0x9e3779b9+(h<<6)+(h>>2);
        return h;
    }
};
struct Record { State state; long long cost; std::size_t parent; bool tick; };
}
OracleResult solve_joint_state(const Instance& in, const OracleOptions& options) {
    using Clock=std::chrono::steady_clock;
    const auto start=Clock::now();
    OracleResult out;
    auto finish=[&](OracleStatus s, const std::string& message) {
        out.status=s; out.message=message;
        out.runtime_seconds=std::chrono::duration<double>(Clock::now()-start).count();
        return out;
    };
    const auto error=instance_error(in);
    if (!error.empty()) return finish(OracleStatus::invalid_input,error);
    if (!std::isfinite(options.timeout_seconds) || options.timeout_seconds<=0 ||
        !options.max_agents || !options.max_states || in.starts.size()>options.max_agents)
        return finish(OracleStatus::invalid_input,"invalid options or too many agents");
    if (in.grid.size()>static_cast<std::size_t>(std::numeric_limits<int>::max()) /
                         in.grid.front().size())
        return finish(OracleStatus::invalid_input,"grid exceeds cell indexing limit");
    auto expired=[&] { return std::chrono::duration<double>(Clock::now()-start).count()
                               >=options.timeout_seconds; };
    const int width=static_cast<int>(in.grid.front().size());
    const std::size_t n=in.starts.size(), none=std::numeric_limits<std::size_t>::max();
    auto cell=[&](Position p) { return p.row*width+p.col; };
    auto position=[&](int c) { return Position{c/width,c%width}; };
    State initial(2*n);
    for (std::size_t i=0;i<n;++i) initial[2*i]=cell(in.starts[i]);
    std::vector<Record> records{{initial,0,none,false}};
    std::unordered_map<State,std::size_t,Hash> best{{initial,0}};
    using Entry=std::pair<long long,std::size_t>;
    std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> open;
    open.push({0,0}); out.generated=1;
    bool limited=false;
    auto relax=[&](const State& state,long long cost,std::size_t parent,bool tick) {
        const auto found=best.find(state);
        if (found!=best.end() && records[found->second].cost<=cost) return;
        // Bound record storage too: improvements preserve old immutable parents.
        if (records.size()>=options.max_states) { limited=true; return; }
        const auto id=records.size();
        records.push_back({state,cost,parent,tick}); best[state]=id;
        open.push({cost,id}); ++out.generated;
    };
    while (!open.empty()) {
        if (expired()) return finish(OracleStatus::timeout,"time limit reached");
        const auto [cost,id]=open.top(); open.pop();
        const auto current=records[id].state; // copy: relax may reallocate records
        if (best.at(current)!=id) continue;
        ++out.expanded;
        std::size_t active=0;
        for (std::size_t i=0;i<n;++i) active+=current[2*i+1]==0;
        if (!active) {
            std::vector<std::size_t> chain;
            for (auto k=id;k!=none;k=records[k].parent) chain.push_back(k);
            std::reverse(chain.begin(),chain.end());
            out.paths.resize(n);
            for (auto k:chain) if (k==0 || records[k].tick)
                for (std::size_t i=0;i<n;++i)
                    out.paths[i].push_back(position(records[k].state[2*i]));
            for (std::size_t i=0;i<n;++i)
                while (out.paths[i].size()>1 &&
                       out.paths[i][out.paths[i].size()-2]==in.goals[i])
                    out.paths[i].pop_back();
            const auto checked=validate(in,out.paths);
            if (!checked.valid || static_cast<long long>(checked.soc)!=cost)
                return finish(OracleStatus::error,"internal reconstructed path/cost mismatch");
            out.optimal_soc=cost;
            return finish(OracleStatus::solved,"optimal SOC certified by joint-state Dijkstra");
        }
        for (std::size_t i=0;i<n;++i) {
            if (!current[2*i+1] && current[2*i]==cell(in.goals[i])) {
                auto committed=current; committed[2*i+1]=1;
                relax(committed,cost,id,false);
            }
        }
        State next=current;
        bool timed_out=false;
        std::function<void(std::size_t)> generate=[&](std::size_t i) {
            if (limited || timed_out) return;
            if (expired()) { timed_out=true; return; }
            if (i==n) { relax(next,cost+static_cast<long long>(active),id,true); return; }
            const auto p=position(current[2*i]);
            constexpr int dr[]={0,1,-1,0,0}, dc[]={0,0,0,1,-1};
            for (int d=0;d<(current[2*i+1]?1:5);++d) {
                const Position q{p.row+dr[d],p.col+dc[d]};
                if (!is_free(in,q)) continue;
                next[2*i]=cell(q);
                bool conflict=false;
                for (std::size_t j=0;j<i;++j)
                    if (next[2*i]==next[2*j] ||
                        (next[2*i]==current[2*j] && next[2*j]==current[2*i]))
                        conflict=true;
                if (!conflict) generate(i+1);
            }
        };
        generate(0);
        if (timed_out) return finish(OracleStatus::timeout,"time limit reached");
        if (limited) return finish(OracleStatus::resource_limit,"record limit reached; optimum unknown");
    }
    return finish(OracleStatus::infeasible,"complete finite joint-state frontier exhausted");
}
}
