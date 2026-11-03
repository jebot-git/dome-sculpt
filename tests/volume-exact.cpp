// SPDX-License-Identifier: GPL-3.0-or-later
// Independent test-only unbounded integer/rational oracle; not a product dependency.
#include "volume-exact.hpp"
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>
#include <iostream>
#include <random>
using namespace dome::detail::volume_exact;
using Big=boost::multiprecision::cpp_int;
using Rational=boost::rational<Big>;
struct Budget{size_t count=0;void step(size_t n=1){count+=n;}};
size_t checks=0;
void check(bool b){if(!b)throw std::runtime_error("Independent exact integer/clip mismatch");++checks;}
Big value(const Integer&i){Big b=0;for(size_t k=i.used;k;--k){b<<=32;b+=i.limb[k-1];}b<<=i.shift;return i.sign<0?-b:b;}
Big scalar(double x){uint64_t bits=std::bit_cast<uint64_t>(x),mantissa=bits&0xfffffffffffffULL;int e=int((bits>>52)&2047);if(e)mantissa|=uint64_t(1)<<52;else e=1;if(!mantissa)return Big(0);Big n=mantissa;int shift=e-1023-52+210;if(shift<0)throw std::range_error("oracle range");n<<=shift;return bits>>63?-n:n;}
Rational frac(const Fraction&f){return Rational(value(f.numerator),value(f.denominator));}
std::array<Rational,2> point(const Point2&p){std::array<Rational,2> q;for(int a=0;a<2;++a)q[a]=p.ordinary?Rational(scalar(p.free[a])):Rational(scalar(p.a[a])*(parameter_scale-p.parameter)+scalar(p.b[a])*p.parameter,Big(parameter_scale));return q;}
Rational orient(std::array<Rational,2>a,std::array<Rational,2>b,std::array<Rational,2>c){return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);}
std::optional<std::pair<Rational,Rational>> clip(const std::array<std::array<float,2>,3>&t,const Point2&p,const Point2&q){std::array<std::array<Rational,2>,3> v;for(int k=0;k<3;++k)for(int a=0;a<2;++a)v[k][a]=Rational(scalar(t[k][a]));auto dir=orient(v[0],v[1],v[2]);if(dir.numerator()==0)return {};Rational lo(0),hi(1);auto pp=point(p),qq=point(q);for(int e=0;e<3;++e){auto x=orient(v[e],v[(e+1)%3],pp),y=orient(v[e],v[(e+1)%3],qq);if(dir.numerator()<0){x=-x;y=-y;}if(x.numerator()>=0&&y.numerator()>=0)continue;if(x.numerator()<0&&y.numerator()<0)return {};auto at=x/(x-y);if(x.numerator()<0)lo=std::max(lo,at);else hi=std::min(hi,at);if(lo>hi)return {};}return {{lo,hi}};}
int main()try{Budget b;Arithmetic a(b);std::mt19937_64 r(0x816eb947);
for(int i=0;i<10000;++i){auto random=[&]{Integer n;n.sign=(r()&1)?1:-1;n.used=1+r()%15;n.shift=r()%20;for(size_t k=0;k<n.used;++k)n.limb[k]=uint32_t(r());n.limb[0]|=1;n.limb[n.used-1]|=1;return n;};auto x=random(),y=random();auto xx=value(x),yy=value(y);check(value(a.add(x,y))==xx+yy);check(value(a.subtract(x,y))==xx-yy);check(value(a.multiply(x,y))==xx*yy);}
for(int i=0;i<3000;++i){auto number=[&]{return float(int(r()%2049)-1024)*std::ldexp(1.f,-int(r()%100));};std::array<std::array<float,2>,3> t;for(auto&v:t)for(auto&x:v)x=number();Point2 p,q;for(auto*e:{&p,&q}){for(auto&x:e->a)x=number();for(auto&x:e->b)x=number();e->parameter=r()%(parameter_scale+1);if(r()&1){e->ordinary=true;e->free={double(number()),double(number())};}}auto expected=clip(t,p,q);auto actual=a.clip(t,p,q);check(bool(expected)==bool(actual));if(expected){check(frac(actual->enter)==expected->first);check(frac(actual->exit)==expected->second);}}
for(int i=0;i<2000;++i){std::array<std::array<float,3>,4> p;std::array<std::array<Big,3>,3> d;for(auto&v:p)for(auto&x:v)x=float(int(r()%2049)-1024)*std::ldexp(1.f,-int(r()%100));for(int k=0;k<3;++k)for(int j=0;j<3;++j)d[k][j]=scalar(p[k+1][j])-scalar(p[0][j]);Big determinant=d[0][0]*(d[1][1]*d[2][2]-d[1][2]*d[2][1])+d[0][1]*(d[1][2]*d[2][0]-d[1][0]*d[2][2])+d[0][2]*(d[1][0]*d[2][1]-d[1][1]*d[2][0]);check(a.orient3(p[0],p[1],p[2],p[3])==(determinant>0?1:determinant<0?-1:0));}
std::array<std::array<float,2>,3> left{{{0,0},{1,0},{0,1}}};Point2 p,q;p.ordinary=q.ordinary=true;p.free={0,.25};q.free={1,.25};auto l=a.clip(left,p,q);check(bool(l)&&frac(l->exit)==Rational(3,4));
// Exact adjacent face coverage meets; a one-binary32-ULP gap does not.
std::array<std::array<float,2>,3> right{{{1,0},{1,1},{0,1}}};auto rr=a.clip(right,p,q);check(bool(rr)&&a.compare(l->exit,rr->enter)==0);for(auto&v:right)if(v[0]==0)v[0]=std::nextafter(0.f,1.f); // min-subnormal remains supported by 210-bit scale
rr=a.clip(right,p,q);check(bool(rr)&&a.compare(l->exit,rr->enter)<0);
Integer top;top.sign=1;top.used=32;top.limb[0]=top.limb[31]=1;check(value(a.add(top,a.integer(1)))==value(top)+1);
bool overflow=false;try{a.multiply(top,top);}catch(const std::overflow_error&){overflow=true;}check(overflow);
bool range=false;try{a.scalar(10001.);}catch(const std::range_error&){range=true;}check(range);
range=false;try{a.scalar(std::ldexp(1.,-211));}catch(const std::range_error&){range=true;}check(range);
std::cout<<"PASS "<<checks<<" independent unbounded integer/rational checks work="<<b.count<<'\n';return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
