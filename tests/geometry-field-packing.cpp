// SPDX-License-Identifier: GPL-3.0-or-later
#include "geometry-field-packing.hpp"
#include <iostream>
#include <numeric>
#include <random>
#include <chrono>
#include <cstdlib>
#include <new>
thread_local bool counting=false;thread_local size_t calls=0;
void*operator new(size_t n){if(counting)++calls;auto p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}void*operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
using namespace dome;using namespace dome::detail;
VertexWeights old(std::span<const double> row,std::span<const uint32_t>joints,WeightLimits limits){VertexWeights values;for(size_t n=0;n<row.size();++n)if(row[n]>0)values.push_back({joints[n],float(row[n])});return normalize_weights(std::move(values),256,{},limits);}
int main()try{
 const std::function<bool()>never;weight_surface::Budget work{0,500000000,never};std::mt19937 rng(0x814773);std::vector<uint32_t>labels(54);std::iota(labels.begin(),labels.end(),0);std::vector<std::vector<double>>rows;
 rows.push_back(std::vector<double>(54,1./54));rows.push_back(std::vector<double>(54,0));rows.push_back(std::vector<double>(54,1e-300));rows.push_back(std::vector<double>(54,std::nextafter(double(.25f),1.)));rows.push_back(std::vector<double>(54,1e-5));rows.push_back(std::vector<double>(54,std::nextafter(double(1e-5f),1.)));
 for(size_t n=0;n<512;++n){std::vector<double>r(54);for(auto&v:r)v=double(rng()%100000)/100000.;r[n%54]=0;if(n%4==0)r[(n+1)%54]=-1;if(n%5==0)r[(n+2)%54]=std::numeric_limits<double>::quiet_NaN();if(n%7==0)r[(n+3)%54]=std::numeric_limits<double>::infinity();rows.push_back(std::move(r));}
 size_t checked=0;for(uint32_t k:{1,2,4,8})for(float prune:{0.f,1e-5f,.1f,.9f}){
  std::shuffle(labels.begin(),labels.end(),rng);WeightLimits limits{k,prune};GeometryFieldPacker pack(labels,256,limits,work);
  for(const auto&r:rows){bool a=true,b=true;VertexWeights x,y;try{x=old(r,labels,limits);}catch(const std::invalid_argument&){a=false;}try{y=pack.pack(r,work);}catch(const std::invalid_argument&){b=false;}if(a!=b||(a&&x!=y))throw std::runtime_error("Exact independent old packet mismatch");++checked;}
 }
 std::vector<double>row(54);for(size_t n=0;n<row.size();++n)row[n]=double(n+1)/1500;
 WeightLimits limits{4,1e-5f};GeometryFieldPacker pack(labels,256,limits,work);size_t old_calls=0,new_calls=0;double old_seconds=0,new_seconds=0;
 for(size_t n=0;n<1000;++n){calls=0;counting=true;auto start=std::chrono::steady_clock::now();auto x=old(row,labels,limits);old_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();counting=false;old_calls+=calls;
  calls=0;counting=true;start=std::chrono::steady_clock::now();auto y=pack.pack(row,work);new_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();counting=false;new_calls+=calls;if(x!=y)throw std::runtime_error("Measured packet differed");}
 if(!(new_calls<old_calls))throw std::runtime_error("No actual allocation reduction");
 bool refused=false;try{auto bad=labels;bad[1]=bad[0];GeometryFieldPacker p(bad,256,limits,work);}catch(const std::invalid_argument&){refused=true;}if(!refused)throw std::runtime_error("Duplicate label accepted");
 refused=false;try{weight_surface::Budget small{0,100,never};GeometryFieldPacker p(labels,256,limits,small);}catch(const std::length_error&){refused=true;}if(!refused)throw std::runtime_error("Work cap ignored");
 std::cout<<"PASS exact packing rows="<<checked<<" primitivework="<<work.used<<" old_allocations="<<old_calls<<" new_allocations="<<new_calls<<" old_seconds="<<old_seconds<<" new_seconds="<<new_seconds<<std::endl;return 0;
}catch(const std::exception&e){counting=false;std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
