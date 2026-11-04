// SPDX-License-Identifier: GPL-3.0-or-later
#include "weight-shared-simplex.hpp"
#include <iostream>
using namespace dome::detail;
namespace {
struct Budget {
  size_t used=0, maximum=10'000'000;
  bool cancelled=false;
  void stop() const {if(cancelled)throw std::runtime_error("test cancellation");}
  void step(size_t n=1){stop();if(n>maximum-used)throw std::length_error("test work limit");used+=n;}
  void step_items(size_t n,size_t u){if(u&&n>(maximum-used)/u)throw std::length_error("test work forecast");step(n*u);}
};
struct Graph {
 std::vector<size_t> starts{0,1,2};
 std::vector<uint32_t> columns{1,0};
 std::vector<double> values{-1,-1};
};
size_t checks=0;
void require(bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);}
bool near(double a,double b){return std::abs(a-b)<1e-8;}
}
int main()try{
 Graph g;Budget work;std::vector<double>x;
 const std::array<double,2>screen{4,4};
 const std::array<double,6>rhs{3,1,0,1,0,3};
 const std::array<std::array<uint64_t,4>,2>masks{{{3,0,0,0},{5,0,0,0}}};
 const std::array<uint32_t,3>joints{0,1,2};
 auto r=solve_shared_simplex(g,screen,rhs,masks,joints,x,1<<20,work);
 // Independent elimination: e0=1-h0,e1=1-h1 gives
 // [9 -1;-1 9] h=[6;2] -> h=(.7,.3), unlike removed-contact(.75,.25).
 require(near(x[0],.7)&&near(x[3],.3),"Independent common-role dense solution");
 require(near(x[1],.3)&&near(x[5],.7),"Independent simplex eliminates private labels");
 require(x[2]==0&&x[4]==0,"Forbidden labels stay exact zero");
 require(r.kkt<=1e-9&&r.partition<=1e-12,"Original KKT and primal certificates");
 require(std::abs(x[0]-.75)>.01,"Common-role spring is genuinely retained");
 auto original=x;
 // Uniform energy scaling changes no minimizer, including the constraint.
 for(auto &v:g.values)v*=8;
 const std::array<double,2>scaledScreen{32,32};
 std::array<double,6>scaledRhs=rhs;for(auto &v:scaledRhs)v*=8;
 r=solve_shared_simplex(g,scaledScreen,scaledRhs,masks,joints,x,1<<20,work);
 for(size_t i=0;i<x.size();++i)require(near(x[i],original[i]),"Common metric/energy scale invariance");
 // Permuting real label IDs cannot alter the field associated with each role.
 const std::array<uint32_t,3>sparseJoints{130,5,254};
 const std::array<std::array<uint64_t,4>,2>sparseMasks{{{32,0,4,0},{0,0,4,uint64_t(1)<<62}}};
 r=solve_shared_simplex(g,scaledScreen,scaledRhs,sparseMasks,sparseJoints,x,1<<20,work);
 for(size_t i=0;i<x.size();++i)require(near(x[i],original[i]),"Actual joint identity remap invariant");
 // An active bound is part of optimization, not a post-solve clipped result.
 const std::array<double,6>boundaryRhs{4,0,0,4,0,0};
 r=solve_shared_simplex(g,scaledScreen,boundaryRhs,masks,joints,x,1<<20,work);
 // Both screens exceed RHS here: simplex still forces the genuine optimum;
 // no row-sum-one data assumption is used by the constrained solver.
 require(x[2]==0&&x[4]==0,"Active-bound case still exact forbidden zeros");
 require(r.kkt<=1e-9&&r.partition<=1e-12,"Boundary constrained certificate");
 bool refused=false;auto unchanged=x;
 try{solve_shared_simplex(g,screen,rhs,masks,joints,x,1,work);}catch(const std::length_error&){refused=true;}
 require(refused&&x==unchanged,"Byte refusal before output allocation");
 Budget exhausted;exhausted.maximum=1;refused=false;
 try{solve_shared_simplex(g,screen,rhs,masks,joints,x,1<<20,exhausted);}catch(const std::length_error&){refused=true;}
 require(refused&&x==unchanged,"Work forecast refusal before output allocation");
 Budget stopped;stopped.cancelled=true;refused=false;
 try{solve_shared_simplex(g,screen,rhs,masks,joints,x,1<<20,stopped);}catch(const std::runtime_error&){refused=true;}
 require(refused&&x==unchanged,"Actual cancellation before output allocation");
 std::cout<<"PASS "<<checks<<" independent shared-role/simplex/dense/scale/identity/budget/cancel controls\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
