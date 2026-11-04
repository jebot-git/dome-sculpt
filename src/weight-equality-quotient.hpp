// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <cmath>
#include <map>
#include <numeric>
#include <span>
#include <vector>
namespace dome::detail {
struct WeightEqualityQuotient {std::vector<uint32_t> rows;size_t bytes=0;};
// Exact constrained screened energy: x=P*y, reduced A=P^T*A*P and b=P^T*b.
// Neither solved-field averaging nor a penalty/fallback. The caller must own
// the geometric/source/morph evidence; matching positions alone grants none.
template<class System,class Mask,class Budget>
WeightEqualityQuotient quotient_weight_equalities(
 System&s,std::vector<double>&rhs,std::vector<double>&screen,
 std::vector<uint32_t>&fixed,std::vector<Mask>&usage,size_t labels,
 std::span<const std::array<uint32_t,2>> equalities,size_t max_bytes,Budget&work){
 const size_t n=s.diagonal.size(),nz=s.columns.size();
 if(!n||n>2'000'000||!labels||labels>256||rhs.size()!=n*labels||
    screen.size()!=n||s.mass.size()!=n||fixed.size()!=n||usage.size()!=n||s.positions.size()!=n||
    s.starts.size()!=n+1||s.starts.front()!=0||s.starts.back()!=nz||s.values.size()!=nz||equalities.size()>6'000'000)
   throw std::invalid_argument("Equality quotient dimensions");
 const size_t bytes=n*(128+labels*8+sizeof(Mask))+nz*96+equalities.size()*8+4096;
 if(bytes>max_bytes)throw std::length_error("Equality quotient bytes before staging");
 work.step_items(n,labels+32);work.step_items(nz,16);
 for(size_t i=0;i<n;++i){
   if(s.starts[i]>s.starts[i+1]||s.starts[i+1]>nz||!std::isfinite(s.mass[i])||!(s.mass[i]>0)||!std::isfinite(screen[i])||screen[i]<0||
      !std::isfinite(s.diagonal[i])||!(s.diagonal[i]>0)||(fixed[i]!=UINT32_MAX&&fixed[i]>=labels))
     throw std::invalid_argument("Equality quotient screened graph metadata");
   double d=screen[i];uint32_t previous=0;bool first=true;
   for(size_t e=s.starts[i];e<s.starts[i+1];++e){
     auto j=s.columns[e];auto a=s.values[e];
     if(j>=n||j==i||!std::isfinite(a)||!(a<0)||(!first&&j<=previous))
       throw std::invalid_argument("Equality quotient adjacency authority");
     first=false;previous=j;d-=a;
   }
   if(std::abs(d-s.diagonal[i])>1e-12*std::max(1.,std::abs(d)))
     throw std::invalid_argument("Equality quotient original energy mismatch");
   for(size_t l=0;l<labels;++l)if(!std::isfinite(rhs[i*labels+l])||rhs[i*labels+l]<0)
     throw std::invalid_argument("Equality quotient invalid load");
 }
 for(size_t i=0;i<n;++i)for(size_t e=s.starts[i];e<s.starts[i+1];++e){
   work.step();auto j=s.columns[e];size_t lo=s.starts[j],hi=s.starts[j+1];
   while(lo<hi){work.step();auto mid=lo+(hi-lo)/2;if(s.columns[mid]<i)lo=mid+1;else hi=mid;}
   if(lo==s.starts[j+1]||s.columns[lo]!=i||s.values[lo]!=s.values[e])
     throw std::invalid_argument("Equality quotient nonsymmetric graph");
 }
 WeightEqualityQuotient out;out.bytes=bytes;out.rows.resize(n);std::iota(out.rows.begin(),out.rows.end(),0);
 auto root=[&](uint32_t i){while(out.rows[i]!=i){work.step();out.rows[i]=out.rows[out.rows[i]];i=out.rows[i];}return i;};
 for(auto e:equalities){work.step(16);if(e[0]>=n||e[1]>=n||s.positions[e[0]]!=s.positions[e[1]])
     throw std::invalid_argument("Equality quotient lacks exact current metric correspondence");
   auto a=root(e[0]),b=root(e[1]);out.rows[std::max(a,b)]=std::min(a,b);
 }
 std::vector<uint32_t> dense(n,UINT32_MAX);size_t count=0;
 for(uint32_t i=0;i<n;++i){work.step();out.rows[i]=root(i);}
 for(uint32_t i=0;i<n;++i){work.step(4);auto r=out.rows[i];if(dense[r]==UINT32_MAX)dense[r]=uint32_t(count++);out.rows[i]=dense[r];}
 std::vector<double> reduced_rhs(count*labels),reduced_screen(count),diagonal(count),mass(count);
 std::vector<uint32_t> boundaries(count,UINT32_MAX);std::vector<Mask> masks(count);
 auto positions=s.positions;positions.resize(count);
 for(uint32_t i=0;i<n;++i){work.step(labels+16);auto g=out.rows[i];positions[g]=s.positions[i];
   mass[g]+=s.mass[i];reduced_screen[g]+=screen[i];for(size_t l=0;l<labels;++l)reduced_rhs[g*labels+l]+=rhs[i*labels+l];
   for(size_t k=0;k<masks[g].size();++k)masks[g][k]|=usage[i][k];
   if(fixed[i]!=UINT32_MAX){if(boundaries[g]!=UINT32_MAX&&boundaries[g]!=fixed[i])
      throw std::invalid_argument("Equality quotient conflicting hard boundaries");
     boundaries[g]=fixed[i];}
 }
 struct Less{Budget*w;bool operator()(const std::array<uint32_t,2>&a,const std::array<uint32_t,2>&b)const{w->step();return a<b;}};
 std::map<std::array<uint32_t,2>,double,Less>edges(Less{&work});
 for(uint32_t i=0;i<n;++i)for(size_t e=s.starts[i];e<s.starts[i+1];++e){work.step(4);auto j=s.columns[e];if(j<i)continue;auto a=out.rows[i],b=out.rows[j];if(a==b)continue;if(b<a)std::swap(a,b);edges[{a,b}]-=s.values[e];}
 diagonal=reduced_screen;std::vector<size_t>starts(count+1);
 for(const auto&[e,w]:edges){work.step(8);if(!std::isfinite(w)||!(w>0))throw std::invalid_argument("Equality quotient aggregate conductance");diagonal[e[0]]+=w;diagonal[e[1]]+=w;++starts[e[0]+1];++starts[e[1]+1];}
 for(size_t i=0;i<count;++i){work.step();starts[i+1]+=starts[i];}
 std::vector<uint32_t>columns(starts.back());std::vector<double>values(starts.back());auto cursor=starts;
 for(const auto&[e,w]:edges){work.step(4);auto a=cursor[e[0]]++,b=cursor[e[1]]++;columns[a]=e[1];values[a]=-w;columns[b]=e[0];values[b]=-w;}
 work.stop();s.starts.swap(starts);s.columns.swap(columns);s.values.swap(values);s.diagonal.swap(diagonal);s.positions.swap(positions);s.mass.swap(mass);
 rhs.swap(reduced_rhs);screen.swap(reduced_screen);fixed.swap(boundaries);usage.swap(masks);
 return out;
}
} // namespace dome::detail
