// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <span>
#include <stdexcept>
#include <sstream>
#include <iomanip>
namespace dome::detail {
// A predeclared hierarchical partition of unity. The independently solved
// anatomical membership h blends the certified base partition with one rigid
// handle. This is not a normalization, failure retry, or fit to a posed oracle.
template<class Budget>
double compose_hierarchical_partition(std::span<double>fields,
 std::span<const double>membership,size_t labels,size_t handle,Budget&work){
 if(!labels||labels>256||handle>=labels||membership.empty()||fields.size()!=membership.size()*labels)
   throw std::invalid_argument("Hierarchical partition dimensions");
 // Same original residual/maximum-principle numerical certificate as
 // continuation_solve. Values are not clamped or renormalized here.
 work.step_items(membership.size(),labels*2+16);
 for(size_t i=0;i<membership.size();++i){
   const double h=membership[i];if(!std::isfinite(h)||h < -1e-9||h > 1+1e-9)
      throw std::runtime_error("Anatomical membership failed independent maximum principle");
   double sum=0;for(size_t l=0;l<labels;++l){auto v=fields[i*labels+l];if(!std::isfinite(v)||v < -1e-9||v > 1+1e-9){std::ostringstream out;out<<std::setprecision(17)<<"Uncertified base partition row="<<i<<" label="<<l<<" value="<<v<<" h="<<h;throw std::runtime_error(out.str());}sum+=v;}
   if(std::abs(sum-1)>1e-9)throw std::runtime_error("Base partition identity failed");
 }
 work.stop();double error=0;
 for(size_t i=0;i<membership.size();++i){const double h=membership[i];double sum=0;for(size_t l=0;l<labels;++l){auto&v=fields[i*labels+l];v=(1-h)*v+(l==handle?h:0);sum+=v;}error=std::max(error,std::abs(sum-1));}
 return error;
}
} // namespace dome::detail
