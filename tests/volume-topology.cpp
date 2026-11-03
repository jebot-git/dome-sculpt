// SPDX-License-Identifier: GPL-3.0-or-later
#include "volume-topology.hpp"
#include <iostream>
using namespace dome::detail::volume_topology;
struct Budget{size_t work=0;void step(size_t n=1){work+=n;}};
size_t checks=0;
void test(Triangle a,Triangle b,bool expected,const char*name){Budget budget;for(int sa=0;sa<3;++sa)for(int sb=0;sb<3;++sb)for(int reverse=0;reverse<4;++reverse){Triangle x,y;for(int k=0;k<3;++k){x[k]=a[(sa+k)%3];y[k]=b[(sb+k)%3];}if(reverse&1)std::swap(x[0],x[1]);if(reverse&2)std::swap(y[0],y[1]);if(unexpected_contact(x,y,budget)!=expected||unexpected_contact(y,x,budget)!=expected)throw std::runtime_error(name);++checks;}std::cout<<"PASS "<<name<<'\n';}
int main()try{Triangle a{{{0,0,0},{1,0,0},{0,1,0}}};
test(a,{{{1,1,0},{0,1,0},{1,0,0}}},false,"Adjacent coplanar triangles share only their genuine edge");
test(a,{{{0,0,0},{-.5f,0,0},{0,-.5f,0}}},false,"Single shared vertex is legal without an overlapping fan");
test(a,{{{0,0,0},{1,0,0},{0,0,1}}},false,"Noncoplanar adjacent triangles share their actual edge");
test(a,a,true,"Coincident duplicate faces are never a legal adjacency");
test(a,{{{0,0,0},{.5f,.5f,-1},{.5f,.5f,1}}},true,"One shared vertex does not authorize noncoplanar overlap beyond it");
test(a,{{{.1f,.1f,0},{.5f,.1f,0},{.1f,.5f,0}}},true,"Coplanar triangle contained in a face is an overlap");
test(a,{{{0,0,0},{1,0,0},{.25f,.25f,0}}},true,"Sharing an edge does not authorize same-side coplanar overlap");
test(a,{{{.25f,.25f,-1},{.25f,.25f,1},{1,1,0}}},true,"Interior noncoplanar crossing is rejected");
test(a,{{{.25f,.25f,0},{.25f,.25f,1},{1,1,1}}},true,"Unowned face-interior endpoint tangency is rejected");
test(a,{{{2,0,0},{3,0,0},{2,1,0}}},false,"Separate coplanar faces remain separate");
float gap=std::nextafter(0.f,1.f);test(a,{{{0,0,gap},{1,0,gap},{0,1,gap}}},false,"A one-subnormal parallel separation is not rounded to contact");
std::cout<<"PASS "<<checks<<" exact triangle order/orientation cases\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
