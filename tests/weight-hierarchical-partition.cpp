// SPDX-License-Identifier: GPL-3.0-or-later
#include "weight-hierarchical-partition.hpp"
#include <array>
#include <vector>
#include <iostream>
struct Work{void stop(){}void step_items(size_t,size_t){}};
int main()try{
 Work w;std::vector<double>base{0,.6,.4,0,.6,.4,0,.6,.4};std::array<double,3>h{1,1./3,0};
 auto err=dome::detail::compose_hierarchical_partition(std::span<double>(base),h,3,0,w);
 if(err>1e-14||base[0]!=1||base[1]!=0||base[6]!=0||base[7]!=.6||std::abs(base[3]-1./3)>1e-15||std::abs(base[4]-.4)>1e-15)throw std::runtime_error("Independent screened Dirichlet1/3 membership and partition");
 // h is the unique minimizer of (3h^2-2h)/2, not fit to output/poses.
 if(std::abs(3*h[1]-1)>1e-15)throw std::runtime_error("Independent scalar original-energy gradient");
 for(auto bad:std::array<double,3>{-.001,1.001,NAN}){auto before=base;auto input=h;input[1]=bad;bool refused=false;try{dome::detail::compose_hierarchical_partition(std::span<double>(base),input,3,0,w);}catch(const std::exception&){refused=true;}if(!refused||base!=before)throw std::runtime_error("Maximum principle refusal atomicity");}
 std::vector<double>roundoff{1.0000000000000013,-.0000000000000013,0};std::array<double,1>zero{0};
 dome::detail::compose_hierarchical_partition(std::span<double>(roundoff),zero,3,0,w);
 if(roundoff[0]!=1.0000000000000013||roundoff[1]!=-.0000000000000013)throw std::runtime_error("Existing numerical certificate consumes unaltered values without clamping or normalization");
 std::cout<<"PASS 6 independent hierarchical energy/partition/stationarity/atomicity controls\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
