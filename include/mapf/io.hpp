#pragma once
#include "mapf/types.hpp"
#include <iosfwd>
namespace mapf {
Instance read_instance(std::istream&);
Paths read_paths(std::istream&);
void write_paths(std::ostream&,const Paths&);
}
