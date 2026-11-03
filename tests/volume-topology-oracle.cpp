// SPDX-License-Identifier: GPL-3.0-or-later
// Independent test-only unbounded-integer separating-axis oracle.
#include "volume-topology.hpp"
#include <boost/multiprecision/cpp_int.hpp>
#include <random>
#include <iostream>
using namespace dome::detail::volume_topology;
using Big=boost::multiprecision::cpp_int;
using V=std::array<Big,3>;
struct Budget{size_t work=0;void step(size_t n=1){work+=n;}};
V sub(V a,const V&b){for(int i=0;i<3;++i)a[i]-=b[i];return a;}
V cross(const V&a,const V&b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
Big dot(const V&a,const V&b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Big scalar(float value){uint32_t bits=std::bit_cast<uint32_t>(value),mantissa=bits&0x7fffffu;int e=int((bits>>23)&255);if(e)mantissa|=uint32_t(1)<<23;else e=1;Big n=mantissa;n<<=e-127-23+210;return bits>>31?-n:n;}
std::array<V,3> convert(Triangle a){std::array<V,3> out;for(int k=0;k<3;++k)for(int j=0;j<3;++j)out[k][j]=scalar(a[k][j]);return out;}
bool oracle(Triangle x,Triangle y){auto a=convert(x),b=convert(y);std::array<V,3> ea,eb;for(int k=0;k<3;++k){ea[k]=sub(a[(k+1)%3],a[k]);eb[k]=sub(b[(k+1)%3],b[k]);}auto na=cross(ea[0],ea[1]),nb=cross(eb[0],eb[1]);auto separates=[&](const V&axis){Big al=dot(a[0],axis),ah=al,bl=dot(b[0],axis),bh=bl;for(int k=1;k<3;++k){Big u=dot(a[k],axis),v=dot(b[k],axis);al=std::min(al,u);ah=std::max(ah,u);bl=std::min(bl,v);bh=std::max(bh,v);}return ah<bl||bh<al;};if(separates(na)||separates(nb))return false;for(auto&e:ea){if(separates(cross(na,e)))return false;for(auto&f:eb)if(separates(cross(e,f)))return false;}for(auto&f:eb)if(separates(cross(nb,f)))return false;return true;}
bool nondegenerate(Triangle t){auto p=convert(t);auto n=cross(sub(p[1],p[0]),sub(p[2],p[0]));return n[0]!=0||n[1]!=0||n[2]!=0;}
int main()try{std::mt19937_64 r(0x4735aba1);Budget budget;size_t checks=0;for(int i=0;i<5000;++i){Triangle a,b;for(auto*t:{&a,&b})for(auto&p:*t)for(auto&v:p)v=float(int(r()%33)-16)*std::ldexp(1.f,-int(r()%8));if(!nondegenerate(a)||!nondegenerate(b)){--i;continue;}bool shared=false;for(auto&p:a)for(auto&q:b)shared|=p==q;if(shared){--i;continue;}bool expected=oracle(a,b);for(int repeat=0;repeat<4;++repeat){if(unexpected_contact(a,b,budget)!=expected||unexpected_contact(b,a,budget)!=expected)throw std::runtime_error("Independent triangle SAT disagreement case"+std::to_string(i));std::rotate(a.begin(),a.begin()+1,a.end());std::swap(b[0],b[1]);++checks;}}
std::cout<<"PASS "<<checks<<" independent unbounded SAT comparisons; work="<<budget.work<<'\n';return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
