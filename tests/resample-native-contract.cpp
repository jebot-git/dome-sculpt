// SPDX-License-Identifier: GPL-3.0-or-later
#include "sparse-resample-backend.hpp"
#include <iostream>
using namespace dome;
using namespace dome::sparse_resample;
int main()try {
 std::vector<Entry> entries;
 for(int z=-5;z<=5;++z)for(int y=-5;y<=5;++y)for(int x=-5;x<=5;++x)
   entries.push_back({{x,y,z},double(x*31+y*7-z*11)/137.});
 Grid grid(entries);std::vector<Point> points;
 for(int z=-8;z<=8;++z)for(int y=-8;y<=8;++y)for(int x=-8;x<=8;++x)points.push_back({x,y,z});
 size_t words=0;
 for(auto scale:std::vector<std::pair<float,float>>{{.037f,.023f},{.03f,.02f},{.02f,.037f}})
  for(double background:{0.,.37,1.}){
   auto p=parameters(scale.first,scale.second,background);SparseFieldSampler native(grid,p,257);auto actual=native.sample(points);
   for(size_t i=0;i<points.size();++i){
    auto encoded=sample(grid.cells().data(),grid.mask(),points[i],p);
    if(actual[i]!=encoded){std::cerr<<"Contract mismatch at "<<i<<std::hex<<" native="<<actual[i]<<" encoded="<<encoded<<'\n';return 1;}
    ++words;
   }
  }
 std::cout<<"PASS "<<words<<" native-architecture production sampler words match independently validated integer arithmetic";
#ifdef __FMA__
 std::cout<<"; FMA target available";
#endif
 std::cout<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
