#include "mapf/bypass.hpp"
#include "mapf/validator.hpp"
#include <algorithm>
namespace mapf {
namespace {
std::size_t count_conflicts(const Paths& paths) {
    std::size_t horizon=0, count=0;
    for (const auto& p:paths) horizon=std::max(horizon,p.size()-1);
    auto at=[&](std::size_t i,std::size_t t) {return paths[i][std::min(t,paths[i].size()-1)];};
    for(std::size_t t=0;t<=horizon;++t)
        for(std::size_t i=0;i<paths.size();++i)
            for(std::size_t j=i+1;j<paths.size();++j) {
                if(at(i,t)==at(j,t)) ++count;
                if(t && at(i,t-1)==at(j,t) && at(j,t-1)==at(i,t)) ++count;
            }
    return count;
}
}
BypassResult evaluate_bypass(const Instance& in,const Paths& parent,const Paths& candidate,
                            const std::function<bool(const Paths&)>& respects) {
    BypassResult out;
    out.message=instance_error(in);
    if(!out.message.empty()) return out;
    if(parent.size()!=in.starts.size() || candidate.size()!=parent.size()) {
        out.message="wrong path count"; return out;
    }
    Paths normalized_parent=parent, normalized_candidate=candidate;
    for(std::size_t i=0;i<parent.size();++i) {
        Instance one{in.grid,{in.starts[i]},{in.goals[i]}};
        const auto a=validate(one,{parent[i]}), b=validate(one,{candidate[i]});
        if(!a.valid || !b.valid || a.soc!=b.soc) {
            out.message="invalid individual path or per-agent cost changed"; return out;
        }
        normalized_parent[i].resize(a.soc+1);
        normalized_candidate[i].resize(b.soc+1);
    }
    if(!respects || !respects(candidate)) {
        out.message="parent constraints not certified"; return out;
    }
    out.parent_conflicts=count_conflicts(normalized_parent);
    out.candidate_conflicts=count_conflicts(normalized_candidate);
    out.accepted=out.candidate_conflicts<out.parent_conflicts;
    out.message=out.accepted?"eligible: same individual costs, fewer conflicts, parent constraints hold":
                             "conflict count did not strictly decrease";
    return out;
}
}
