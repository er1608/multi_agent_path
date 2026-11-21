#include "mapf/analysis.hpp"
#include <queue>
namespace mapf {
LowerBoundResult independent_lower_bound(const Instance& in) {
    LowerBoundResult out;
    out.message=instance_error(in);
    if (!out.message.empty()) return out;
    out.valid_input=true; out.individually_reachable=true; out.soc=0;
    for (std::size_t i=0;i<in.starts.size();++i) {
        std::vector<std::vector<int>> distance(in.grid.size(),
            std::vector<int>(in.grid.front().size(),-1));
        std::queue<Position> open; open.push(in.starts[i]);
        distance[in.starts[i].row][in.starts[i].col]=0;
        while(!open.empty()) {
            auto p=open.front(); open.pop();
            constexpr int dr[]={1,-1,0,0}, dc[]={0,0,1,-1};
            for(int d=0;d<4;++d) {
                Position q{p.row+dr[d],p.col+dc[d]};
                if(is_free(in,q) && distance[q.row][q.col]<0) {
                    distance[q.row][q.col]=distance[p.row][p.col]+1; open.push(q);
                }
            }
        }
        const auto goal=in.goals[i]; const int d=distance[goal.row][goal.col];
        out.distances.push_back(d);
        if(d<0) out.individually_reachable=false;
        else out.soc+=d;
    }
    if(!out.individually_reachable) out.soc=-1;
    out.message=out.individually_reachable?"sum of independent shortest-path distances":
                                           "at least one goal is unreachable";
    return out;
}
GapResult compute_gap(long long soc,long long bound,bool exact) {
    GapResult out; out.exact=exact;
    if(soc<0 || bound<0 || bound>soc) { out.message="invalid cost/bound"; return out; }
    if(exact && bound==0 && soc!=0) {
        out.message="positive SOC contradicts zero optimum in this MAPF model"; return out;
    }
    out.valid=true;
    if(bound==0) {
        out.message=soc==0?"zero cost; relative gap and ratio undefined (0/0)":
                           "zero lower bound; relative gap and ratio undefined";
        return out;
    }
    out.relative_gap=static_cast<double>(soc-bound)/static_cast<double>(bound);
    out.ratio=static_cast<double>(soc)/static_cast<double>(bound);
    out.message=exact?"exact relative optimality gap":"upper bound on relative optimality gap";
    return out;
}
}
