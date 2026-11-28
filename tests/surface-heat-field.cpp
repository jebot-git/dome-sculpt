// SPDX-License-Identifier: GPL-3.0-or-later
#include "screened-continuation-solve.hpp"
#include "surface-heat-field.hpp"
#include "weight-surface-system.hpp"
#include <iostream>

using namespace dome::detail;
namespace {
void require(bool okay,const char *why){if(!okay)throw std::runtime_error(why);}

weight_surface::System grid(uint32_t width,uint32_t height) {
  weight_surface::System system;
  const auto count=width*height;
  system.starts.push_back(0);
  for(uint32_t i=0;i<count;++i){
    const auto x=i%width,y=i/width;
    const auto before=system.columns.size();
    if(y)system.columns.push_back(i-width);
    if(x)system.columns.push_back(i-1);
    if(x+1<width)system.columns.push_back(i+1);
    if(y+1<height)system.columns.push_back(i+width);
    const auto degree=system.columns.size()-before;
    system.diagonal.push_back(double(degree)+(x==0||x+1==width?2.:0.));
    system.values.insert(system.values.end(),degree,-1.);
    system.starts.push_back(system.columns.size());
  }
  return system;
}
} // namespace

int main() try {
  constexpr uint32_t width=51,height=65;
  auto system=grid(width,height);
  std::vector<double> expected(width*height),rhs(expected.size()),result(expected.size(),-1.);
  for(size_t i=0;i<expected.size();++i)expected[i]=.1+.8*double(i%width)/(width-1);
  const std::function<bool()> never;
  weight_surface::Budget work{0,100'000'000,never};
  auto scalar=grid(1,1);
  scalar.diagonal[0]=4.;
  visibility_heat::FieldSolver exact_solver(scalar,work);
  const std::array scalar_rhs{3.};
  std::array scalar_field{-1.};
  ContinuationReport exact_step;
  exact_solver.solve(scalar_rhs,scalar_field,work,exact_step);
  require(exact_step.iterations==1&&scalar_field[0]==.75,
          "A solved field must stop updating within its first batch");

  system.multiply(expected,rhs,work);
  visibility_heat::FieldSolver solver(system,work);
  ContinuationReport cancelled;
  const std::function<bool()> stop=[&]{return cancelled.iterations>=3;};
  weight_surface::Budget interrupted{0,100'000'000,stop};
  bool failed=false;
  try{solver.solve(rhs,result,interrupted,cancelled);}
  catch(const std::runtime_error &error){failed=std::string_view(error.what())=="Bone heat cancelled";}
  require(failed&&cancelled.iterations>=3,"Expected cancellation inside the field solve");
  require(std::all_of(result.begin(),result.end(),[](double value){return value==-1.;}),
          "Cancelled solve changed the caller's field");

  ContinuationReport proof;
  solver.solve(rhs,result,work,proof);
  auto replay=std::vector<double>(expected.size());
  system.multiply(result,replay,work);
  double worst=0,error=0,norm=0;
  for(size_t i=0;i<expected.size();++i){
    worst=std::max(worst,std::abs(result[i]-expected[i]));
    const double delta=replay[i]-rhs[i];
    error+=delta*delta;
    norm+=rhs[i]*rhs[i];
  }
  const auto residual=std::sqrt(error/norm);
  require(worst<1e-11&&residual<=1e-9&&proof.residual<=1e-9,"Surface Heat exceeded the independent field or residual bound");
  std::cout<<"PASS manufactured open-grid field after Cancel and retry, rows="<<expected.size()
           <<" error="<<worst<<" replay="<<residual<<" iterations="<<proof.iterations<<'\n';
  std::cout<<"PASS exact single-step field stops at its first converged iteration\n";
  return 0;
} catch(const std::exception &error) {
  std::cerr<<"FAIL "<<error.what()<<'\n';
  return 1;
}
