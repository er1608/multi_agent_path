#include "mapf/io.hpp"
#include <istream>
#include <ostream>
#include <stdexcept>
namespace mapf {
namespace {
int number(std::istream& input,int upper) {
    long long x;
    if(!(input>>x) || x<0 || x>upper) throw std::runtime_error("invalid integer in input");
    return static_cast<int>(x);
}
void end(std::istream& input) {
    std::string extra;
    if(input>>extra) throw std::runtime_error("unexpected trailing data");
}
}
Instance read_instance(std::istream& input) {
    const int h=number(input,10000),w=number(input,10000);
    if(!h || !w || static_cast<long long>(h)*w>10000000)
        throw std::runtime_error("invalid or excessive grid size");
    Instance in; in.grid.resize(h);
    for(auto& row:in.grid)
        if(!(input>>row) || row.size()!=static_cast<std::size_t>(w))
            throw std::runtime_error("wrong grid row length");
    const int n=number(input,10000);
    for(int i=0;i<n;++i) {
        Position s{number(input,10000),number(input,10000)};
        Position g{number(input,10000),number(input,10000)};
        in.starts.push_back(s); in.goals.push_back(g);
    }
    end(input);
    const auto error=instance_error(in);
    if(!error.empty()) throw std::runtime_error(error);
    return in;
}
Paths read_paths(std::istream& input) {
    const int n=number(input,10000); Paths paths(n);
    std::size_t total=0;
    for(auto& path:paths) {
        const int length=number(input,1000000); total+=length;
        if(total>1000000) throw std::runtime_error("too many path positions");
        for(int t=0;t<length;++t) path.push_back({number(input,10000),number(input,10000)});
    }
    end(input); return paths;
}
void write_paths(std::ostream& output,const Paths& paths) {
    output<<paths.size()<<'\n';
    for(const auto& path:paths) {
        output<<path.size();
        for(auto p:path) output<<' '<<p.row<<' '<<p.col;
        output<<'\n';
    }
}
}
