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

void independent_domains(weight_surface::Budget &work) {
  constexpr size_t count=8;
  weight_surface::System system;
  system.diagonal.assign(count,3.);
  system.heat.assign(count,2.);
  system.ties.assign(count,1.);
  system.seeds.resize(count);
  std::array<visibility_heat::SupportMask,count> supports{};
  constexpr std::array<uint32_t,count> owners{0,0,255,0,70,130,255,70};
  for(uint32_t row=0;row<count;++row){
    system.parent.push_back(row%4);
    system.starts.push_back(row);
    system.columns.push_back(row^4);
    system.values.push_back(-1.);
    const auto joint=owners[row];
    const auto bit=uint64_t(1)<<(joint%64);
    system.seeds[row][joint/64]=bit;
    supports[row%4][joint/64]|=bit;
  }
  system.starts.push_back(count);
  ContinuationReport proof;
  visibility_heat::FieldDomainReport report;
  const auto solved=visibility_heat::solve_supported_domains(system,supports,256,{},work,proof,report);
  require(report.blocks==2&&report.coupled_rows==6&&report.rigid_rows==2&&
          report.largest_block==4&&report.field_rows==12&&report.whole_mesh_field_rows==24,
          "Components must share a solve only when their admitted joint sets agree");
  require(report.partition_error<=1e-7&&proof.residual<=1e-9,"Compact fields require residual and partition proof");
  for(size_t row=0;row<count;++row){
    const auto &weights=solved.vertices[row];
    auto expected=owners[row]==255?dome::VertexWeights{{255,1.f}}:
      dome::VertexWeights{{owners[row],.75f},{owners[row^4],.25f}};
    std::ranges::sort(expected,{},&dome::Influence::joint);
    require(weights.size()==expected.size(),"An excluded joint entered a disconnected component");
    for(size_t i=0;i<weights.size();++i)
      require(weights[i].joint==expected[i].joint&&weights[i].weight==expected[i].weight,
              "Interleaved rows must recover the exact two-node solution in native order");
  }

  system.columns[0]=5;
  bool refused=false;
  try{
    visibility_heat::FieldDomainReport bad_report;
    visibility_heat::solve_supported_domains(system,supports,256,{},work,proof,bad_report);
  }catch(const std::logic_error &error){
    refused=std::string_view(error.what()).find("cut a nonzero matrix entry")!=std::string_view::npos;
  }
  require(refused,"A false component partition cannot discard a coupled edge");
}
} // namespace

int main() try {
  constexpr uint32_t width=51,height=65;
  auto system=grid(width,height);
  std::vector<double> expected(width*height),rhs(expected.size()),result(expected.size(),-1.);
  for(size_t i=0;i<expected.size();++i)expected[i]=.1+.8*double(i%width)/(width-1);
  const std::function<bool()> never;
  weight_surface::Budget work{0,100'000'000,never};
  independent_domains(work);
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
  std::cout<<"PASS independent joint domains, interleaved rows and exact rigid weights\n";
  return 0;
} catch(const std::exception &error) {
  std::cerr<<"FAIL "<<error.what()<<'\n';
  return 1;
}
