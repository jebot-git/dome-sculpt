// SPDX-License-Identifier: GPL-3.0-or-later
#include "weight-user-boundary.hpp"
#include <array>
#include <iostream>
namespace {
struct Budget{size_t used=0,maximum=1000000;bool cancel=false;void stop()const{if(cancel)throw std::runtime_error("cancelled");}void step(size_t n=1){stop();if(n>maximum-used)throw std::length_error("work");used+=n;}void step_items(size_t n,size_t u){if(u&&n>(maximum-used)/u)throw std::length_error("work");step(n*u);}};
struct Graph{std::vector<size_t>starts{0,1,3,4};std::vector<uint32_t>columns{1,0,2,1};std::vector<double>values{-1,-1,-2,-2},diagonal{3,5,4};bool operator==(const Graph&)const=default;};
size_t checks=0;void check(bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);}
}
int main()try{
 Graph g,original=g;std::vector<double>rhs{2,0,1,1,0,2};auto before=rhs;const std::array<uint32_t,3>fixed{0,UINT32_MAX,1};Budget w;
 auto bytes=dome::detail::apply_user_weight_boundaries(g,rhs,fixed,2,1<<20,w);
 check(g.columns.empty()&&g.starts==std::vector<size_t>({0,0,0,0}),"Fixed adjacency eliminated symmetrically");
 check(g.diagonal==std::vector<double>({1,5,1}),"Free diagonal retains both boundary springs");
 check(rhs==std::vector<double>({1,0,2,3,0,1}),"Independent exact boundary load");
 // Original free energy derivatives5*x-(oldRhs+w0*(1,0)+w2*(0,1))=0.
 check(rhs[2]/g.diagonal[1]==.4&&rhs[3]/g.diagonal[1]==.6,"Independent free minimizer and partition");
 check(bytes>0&&w.used>0,"Construction charged");
 auto refused=[&](std::array<uint32_t,3>f,size_t max,Budget work){Graph a=original;auto b=before;bool threw=false;try{dome::detail::apply_user_weight_boundaries(a,b,f,2,max,work);}catch(const std::exception&){threw=true;}check(threw&&a==original&&b==before,"Boundary refusal leaves exact original system");};
 refused({0,2,1},1<<20,{});refused(fixed,1,{});Budget stopped;stopped.cancel=true;refused(fixed,1<<20,stopped);Budget exhausted;exhausted.maximum=1;refused(fixed,1<<20,exhausted);
 // No declared boundary preserves the exact existing operator/RHS.
 Graph a=original;auto b=before;const std::array<uint32_t,3>none{UINT32_MAX,UINT32_MAX,UINT32_MAX};dome::detail::apply_user_weight_boundaries(a,b,none,2,1<<20,w);check(a==original&&b==before,"No-op declaration keeps original energy exact");
 std::cout<<"PASS "<<checks<<" independent Dirichlet/load/partition/cancel/atomicity controls\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
