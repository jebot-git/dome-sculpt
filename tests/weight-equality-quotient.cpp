// SPDX-License-Identifier: GPL-3.0-or-later
#include "weight-equality-quotient.hpp"
#include <iostream>
struct Graph {std::vector<size_t>starts{0,1,3,5,6};std::vector<uint32_t>columns{1,0,2,1,3,2};std::vector<double>values{-1,-1,-2,-2,-3,-3},diagonal{2,5,8,7},mass{1,2,3,4};std::vector<std::array<double,3>>positions{{0,0,0},{1,0,0},{0,0,0},{2,0,0}};bool operator==(const Graph&)const=default;};
struct Budget{size_t used=0,maximum=1000000;bool cancel=false;void stop(){if(cancel)throw std::runtime_error("cancelled");}void step(size_t n=1){stop();if(n>maximum-used)throw std::length_error("work");used+=n;}void step_items(size_t n,size_t u){if(u&&n>(maximum-used)/u)throw std::length_error("work");step(n*u);}};
using Mask=std::array<uint64_t,4>;size_t checks=0;void check(bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);}
int main()try{
 Graph g;std::vector<double>rhs{1,0,2,0,0,3,0,4},screen{1,2,3,4};std::vector<uint32_t>fixed(4,UINT32_MAX);std::vector<Mask>masks(4);masks[0][0]=1;masks[2][0]=2;Budget work;std::array<std::array<uint32_t,2>,1>eq{{{0,2}}};
 auto q=dome::detail::quotient_weight_equalities(g,rhs,screen,fixed,masks,2,eq,1<<20,work);
 check(q.rows==std::vector<uint32_t>({0,1,0,2}),"Exact constrained unknown map");
 check(g.diagonal==std::vector<double>({10,5,7})&&g.values==std::vector<double>({-3,-3,-3,-3}),"Independent P^T A P dense coefficients");
 check(rhs==std::vector<double>({1,3,2,0,0,4})&&screen==std::vector<double>({4,2,4}),"Independent P^T b and screen mass");
 check(g.mass==std::vector<double>({4,2,4}),"Physical positive mass adds under the constrained map");
 check(masks[0][0]==3&&q.bytes>0&&work.used>0,"Owner evidence union and actual charges");
 // Independent dense gradient on arbitrary y must equal P^T(A*P*y-b).
 std::array<double,3>y{.2,.5,.7};std::array<double,4>x{y[0],y[1],y[0],y[2]};Graph old;std::vector<double>b{1,2,0,0},old_gradient(4),gradient(3);
 for(size_t i=0;i<4;++i){old_gradient[i]=old.diagonal[i]*x[i]-b[i];for(size_t e=old.starts[i];e<old.starts[i+1];++e)old_gradient[i]+=old.values[e]*x[old.columns[e]];}
 for(size_t i=0;i<3;++i){gradient[i]=g.diagonal[i]*y[i]-rhs[i*2];for(size_t e=g.starts[i];e<g.starts[i+1];++e)gradient[i]+=g.values[e]*y[g.columns[e]];}
 check(std::abs(gradient[0]-old_gradient[0]-old_gradient[2])<1e-14&&std::abs(gradient[1]-old_gradient[1])<1e-14&&std::abs(gradient[2]-old_gradient[3])<1e-14,"Independent constrained original-energy gradient");
 auto refused=[&](Graph a,std::vector<uint32_t>f,std::array<std::array<uint32_t,2>,1> e,size_t bytes,Budget w){auto before=a;std::vector<double>r{1,0,2,0,0,3,0,4},s{1,2,3,4};auto rb=r,sb=s;auto fb=f;std::vector<Mask>u(4),ub=u;bool threw=false;try{dome::detail::quotient_weight_equalities(a,r,s,f,u,2,e,bytes,w);}catch(const std::exception&){threw=true;}check(threw&&a==before&&r==rb&&s==sb&&f==fb&&u==ub,"Refusal preserves complete original operator/authority");};
 refused({}, {0,UINT32_MAX,1,UINT32_MAX},eq,1<<20,{});
 auto invalid=Graph{};invalid.positions[2][0]=.00001;refused(invalid,std::vector<uint32_t>(4,UINT32_MAX),eq,1<<20,{});
 invalid=Graph{};invalid.values.back()=-4;refused(invalid,std::vector<uint32_t>(4,UINT32_MAX),eq,1<<20,{});
 invalid=Graph{};invalid.starts[2]=999;refused(invalid,std::vector<uint32_t>(4,UINT32_MAX),eq,1<<20,{});
 refused({},std::vector<uint32_t>(4,UINT32_MAX),eq,1,{});Budget stop;stop.cancel=true;refused({},std::vector<uint32_t>(4,UINT32_MAX),eq,1<<20,stop);Budget low;low.maximum=2;refused({},std::vector<uint32_t>(4,UINT32_MAX),eq,1<<20,low);
 // Nonzero representatives stress dense-ID remapping (no parent overwrite).
 Graph z;std::array<std::array<uint32_t,2>,0>none{};std::vector<double>r{1,0,2,0,0,3,0,4},s{1,2,3,4};std::vector<uint32_t>f(4,UINT32_MAX);std::vector<Mask>u(4);auto same=dome::detail::quotient_weight_equalities(z,r,s,f,u,2,none,1<<20,work);check(z==Graph{}&&same.rows==std::vector<uint32_t>({0,1,2,3}),"No equalities preserves exact independent energy");
 std::cout<<"PASS "<<checks<<" independent equality-energy/gradient/authority/atomicity controls\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
