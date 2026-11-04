// SPDX-License-Identifier: GPL-3.0-or-later
#include "weight-boundary-sewing.hpp"
#include <iostream>
using namespace dome;
size_t checks=0;void check(bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);}
int main()try{
 std::function<bool()> no=[] {return false;};detail::weight_surface::Budget work{0,1000000,no};
 Mesh m;m.positions={{0,0,0},{1,0,0},{0,1,0},{1,0,0},{1,1,0},{0,1,0}};m.indices={0,1,2,3,4,5};
 auto r=detail::geometric_boundary_sewing(m,{},1<<20,work);check(r.pairs==1&&r.groups[1]==r.groups[3]&&r.groups[2]==r.groups[5],"Exact opposite seam has one common energy row per endpoint");
 check(m.positions.size()==6&&m.indices[3]==3,"Raw arrays unchanged");
 auto assembled=detail::weight_surface::assemble(m,work,detail::HeatOperator::PositiveGraph,r.groups);
 check(assembled.positions.size()==4&&assembled.raw_to_row[1]==assembled.raw_to_row[3],"Actual graph quotient preserves exact equal unknowns");
 check(std::abs(std::accumulate(assembled.mass.begin(),assembled.mass.end(),0.)-1)<1e-12,"Independent unit square mass retained");
 std::vector<std::vector<Vec3>> morph(1,std::vector<Vec3>(6));morph[0][3]={0,0,1};
 auto apart=detail::geometric_boundary_sewing(m,morph,1<<20,work);check(apart.pairs==0&&apart.groups[1]!=apart.groups[3],"Differing actual morph motion cannot be sewn");
 auto same=morph;same[0][1]=same[0][3];auto moving=detail::geometric_boundary_sewing(m,same,1<<20,work);check(moving.pairs==1,"Identical complete morph signature permits seam");
 auto near=m;near.positions[3].x+=1e-5f;auto nr=detail::geometric_boundary_sewing(near,{},1<<20,work);check(!nr.pairs,"Near coordinates are never welded");
 auto reversed=m;std::swap(reversed.indices[3],reversed.indices[5]);auto rr=detail::geometric_boundary_sewing(reversed,{},1<<20,work);check(!rr.pairs&&rr.rejected,"Equal oriented boundary is not opposite seam");
 auto ambiguous=m;ambiguous.positions.insert(ambiguous.positions.end(),m.positions.begin()+3,m.positions.end());ambiguous.indices.insert(ambiguous.indices.end(),{6,7,8});auto ar=detail::geometric_boundary_sewing(ambiguous,{},1<<20,work);check(!ar.pairs&&ar.ambiguous,"Multiple matching edge owners are ambiguous");
 bool fail=false;try{detail::geometric_boundary_sewing(m,{},1,work);}catch(const std::length_error&){fail=true;}check(fail,"Prospective bytes refuse before allocation");
 std::function<bool()> yes=[] {return true;};detail::weight_surface::Budget stop{0,1000000,yes};fail=false;try{detail::geometric_boundary_sewing(m,{},1<<20,stop);}catch(const std::exception&){fail=true;}check(fail,"Cancellation before staging");
 std::cout<<"PASS "<<checks<<" geometric energy sewing controls\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
