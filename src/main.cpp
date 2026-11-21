#include "mapf/analysis.hpp"
#include "mapf/io.hpp"
#include "mapf/joint_state_oracle.hpp"
#include "mapf/validator.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace {
std::string quote(const std::string& s) {
    std::ostringstream out; out<<'"';
    for(unsigned char c:s) {
        if(c=='"' || c=='\\') out<<'\\'<<c;
        else if(c<32) out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<int>(c);
        else out<<c;
    }
    out<<'"'; return out.str();
}
std::string peak_memory() {
#if defined(__unix__) || defined(__APPLE__)
    rusage usage{};
    if(getrusage(RUSAGE_SELF,&usage)==0) {
#if defined(__APPLE__)
        return std::to_string(static_cast<long long>(usage.ru_maxrss));
#else
        return std::to_string(static_cast<long long>(usage.ru_maxrss)*1024);
#endif
    }
#endif
    return "null";
}
long long integer(const std::string& value) {
    std::size_t used=0; auto n=std::stoll(value,&used);
    if(used!=value.size() || n<0) throw std::runtime_error("expected nonnegative integer");
    return n;
}
double real(const std::string& value) {
    std::size_t used=0; auto x=std::stod(value,&used);
    if(used!=value.size()) throw std::runtime_error("invalid floating-point value");
    return x;
}
}
int main(int argc,char** argv) {
    try {
        if(argc==1 || (argc==2 && std::string(argv[1])=="--help")) {
            std::cout<<"TV4 MAPF\n"
              "  --mode oracle|validate|lb|gap\n"
              "  --instance FILE [--paths FILE] [--output FILE]\n"
              "  oracle: [--timeout 10] [--max-agents 3] [--max-states 500000] [--paths-output FILE]\n"
              "  gap: --soc N --bound N [--bound-kind optimum|lb]\n";
            return 0;
        }
        std::map<std::string,std::string> args;
        const std::map<std::string,bool> allowed{{"--mode",true},{"--instance",true},{"--paths",true},
            {"--output",true},{"--timeout",true},{"--max-agents",true},{"--max-states",true},
            {"--paths-output",true},{"--soc",true},{"--bound",true},{"--bound-kind",true}};
        for(int i=1;i<argc;i+=2) {
            if(i+1>=argc || !allowed.count(argv[i]) || args.count(argv[i]))
                throw std::runtime_error("unknown, duplicate, or missing option");
            args[argv[i]]=argv[i+1];
        }
        auto required=[&](const std::string& key) {
            if(!args.count(key)) throw std::runtime_error("missing "+key);
            return args.at(key);
        };
        const auto mode=required("--mode");
        std::ostringstream json; json<<std::setprecision(12);
        int exit_code=0;
        if(mode=="gap") {
            const auto kind=args.count("--bound-kind")?args.at("--bound-kind"):"lb";
            if(kind!="lb" && kind!="optimum") throw std::runtime_error("invalid bound kind");
            auto gap=mapf::compute_gap(integer(required("--soc")),integer(required("--bound")),kind=="optimum");
            json<<"{\"valid\":"<<(gap.valid?"true":"false")<<",\"exact\":"<<(gap.exact?"true":"false")
                <<",\"relative_gap\":";
            if(gap.relative_gap) json<<*gap.relative_gap; else json<<"null";
            json<<",\"ratio\":"; if(gap.ratio) json<<*gap.ratio; else json<<"null";
            json<<",\"message\":"<<quote(gap.message)<<"}"; exit_code=gap.valid?0:2;
        } else {
            std::ifstream input(required("--instance"));
            if(!input) throw std::runtime_error("cannot open instance file");
            const auto in=mapf::read_instance(input);
            if(mode=="validate") {
                std::ifstream paths_file(required("--paths"));
                if(!paths_file) throw std::runtime_error("cannot open paths file");
                const auto check=mapf::validate(in,mapf::read_paths(paths_file));
                json<<"{\"status\":"<<quote(check.valid?"solved":"invalid")
                    <<",\"validity\":"<<(check.valid?"true":"false")<<",\"cost\":";
                if(check.valid) json<<check.soc; else json<<"null";
                json<<",\"makespan\":"; if(check.valid) json<<check.makespan; else json<<"null";
                json<<",\"message\":"<<quote(check.reason)<<"}"; exit_code=check.valid?0:2;
            } else if(mode=="lb") {
                const auto bound=mapf::independent_lower_bound(in);
                json<<"{\"status\":"<<quote(bound.individually_reachable?"bounded":"infeasible")
                    <<",\"certified_lb\":";
                if(bound.individually_reachable) json<<bound.soc; else json<<"null";
                json<<",\"lb_source\":\"independent_shortest_paths\",\"message\":"<<quote(bound.message)<<"}";
            } else if(mode=="oracle") {
                mapf::OracleOptions options;
                if(args.count("--timeout")) options.timeout_seconds=real(args.at("--timeout"));
                if(args.count("--max-agents")) options.max_agents=integer(args.at("--max-agents"));
                if(args.count("--max-states")) options.max_states=integer(args.at("--max-states"));
                const auto result=mapf::solve_joint_state(in,options);
                const bool solved=result.status==mapf::OracleStatus::solved;
                const auto bound=mapf::independent_lower_bound(in);
                json<<"{\"status\":"<<quote(mapf::status_name(result.status))
                    <<",\"validity\":"<<(solved?"true":"null")<<",\"cost\":";
                if(solved) json<<result.optimal_soc; else json<<"null";
                json<<",\"certified_lb\":";
                if(solved) json<<result.optimal_soc;
                else if(bound.individually_reachable) json<<bound.soc;
                else json<<"null";
                json<<",\"lb_source\":"<<quote(solved?"joint_state_optimum":"independent_shortest_paths")
                    <<",\"expanded\":"<<result.expanded<<",\"generated\":"<<result.generated
                    <<",\"runtime\":"<<result.runtime_seconds<<",\"peak_memory_bytes\":"<<peak_memory()
                    <<",\"message\":"<<quote(result.message)<<",\"paths\":[";
                for(std::size_t i=0;i<result.paths.size();++i) {
                    if(i) json<<',';
                    json<<'[';
                    for(std::size_t t=0;t<result.paths[i].size();++t) {
                        if(t) json<<',';
                        auto p=result.paths[i][t]; json<<'['<<p.row<<','<<p.col<<']';
                    }
                    json<<']';
                }
                json<<"]}";
                if(solved && args.count("--paths-output")) {
                    std::ofstream file(args.at("--paths-output"));
                    if(!file) throw std::runtime_error("cannot open paths output");
                    mapf::write_paths(file,result.paths);
                    if(!file) throw std::runtime_error("failed writing paths");
                }
                exit_code=solved || result.status==mapf::OracleStatus::infeasible?0:2;
            } else throw std::runtime_error("unknown mode");
        }
        if(args.count("--output")) {
            std::ofstream file(args.at("--output"));
            if(!file) throw std::runtime_error("cannot open JSON output");
            file<<json.str()<<'\n';
            if(!file) throw std::runtime_error("failed writing JSON");
        }
        std::cout<<json.str()<<'\n'; return exit_code;
    } catch(const std::exception& error) {
        std::cerr<<"{\"status\":\"error\",\"message\":"<<quote(error.what())<<"}\n";
        return 2;
    }
}
