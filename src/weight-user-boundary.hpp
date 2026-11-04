// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>
namespace dome::detail {
// Explicit authoring Dirichlet data, not automatically inferred anatomy and
// never artist-weight copying. Preserve free/free springs and the original
// fixed/free energy; moving a fixed neighbor to RHS is exact elimination.
template<class System,class Budget>
size_t apply_user_weight_boundaries(System &s,std::vector<double>&rhs,
                                  std::span<const uint32_t> fixed,
                                  size_t labels,size_t max_bytes,Budget&work){
 const size_t n=s.diagonal.size();
 if(!n||n>2'000'000||!labels||labels>256||fixed.size()!=n||
    rhs.size()!=n*labels||s.starts.size()!=n+1||
    s.starts.back()!=s.columns.size()||s.values.size()!=s.columns.size())
  throw std::invalid_argument("User weight boundary dimensions");
 const size_t bytes=n*(sizeof(double)+sizeof(size_t))+sizeof(size_t)+
   s.columns.size()*(sizeof(uint32_t)+sizeof(double))+rhs.size()*sizeof(double);
 if(bytes>max_bytes)throw std::length_error("User boundary bytes before staging");
 work.step_items(n,labels+8);work.step_items(s.columns.size(),8);
 for(auto j:fixed)if(j!=UINT32_MAX&&j>=labels)
  throw std::invalid_argument("User boundary joint has no current label");
 std::vector<size_t>starts(n+1);
 std::vector<uint32_t>columns;columns.reserve(s.columns.size());
 std::vector<double>values;values.reserve(s.values.size());
 auto diagonal=s.diagonal;auto staged=rhs;
 for(size_t i=0;i<n;++i){
  work.stop();
  if(s.starts[i]>s.starts[i+1])throw std::invalid_argument("User boundary CSR order");
  if(fixed[i]!=UINT32_MAX){
   diagonal[i]=1;
   for(size_t l=0;l<labels;++l)staged[i*labels+l]=fixed[i]==l?1.:0.;
  }else{
   for(size_t e=s.starts[i];e<s.starts[i+1];++e){
    auto j=s.columns[e];auto a=s.values[e];
    if(j>=n||j==i||!(a<0)||!std::isfinite(a))
     throw std::invalid_argument("User boundary nonpositive graph authority");
    if(fixed[j]!=UINT32_MAX)staged[i*labels+fixed[j]]-=a;
    else{columns.push_back(j);values.push_back(a);}
   }
  }
  starts[i+1]=columns.size();
 }
 work.stop();
 // All fallible work precedes detached-system publication. No scene is here.
 s.starts.swap(starts);s.columns.swap(columns);s.values.swap(values);
 s.diagonal.swap(diagonal);rhs.swap(staged);
 return bytes;
}
} // namespace dome::detail
