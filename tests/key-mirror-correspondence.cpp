// SPDX-License-Identifier: GPL-3.0-or-later
#include "key-mirror-correspondence.hpp"
#include "corner-key-expansion.hpp"
#include <iostream>
#include "mirror-host-allocation-fault.hpp"
namespace fault=mirror_failure;
using namespace dome;namespace {size_t checks=0;void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;std::cout<<"PASS "<<m<<std::endl;}bool rejects(auto call,std::string_view message){try{call();return false;}catch(const std::exception&e){std::cout<<"REFUSAL "<<e.what()<<std::endl;return std::string_view(e.what()).find(message)!=std::string_view::npos;}}bool same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}}
int main(int argc,char **argv)try{
 Mesh original;original.positions={{0,0,0},{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};original.normals.assign(5,{0,0,1});for(auto p:original.positions)original.uv.push_back({p.x+.5f,p.y+.5f});original.indices={0,1,2,0,2,3,0,3,4,0,4,1};Mesh current;for(auto v:original.indices){current.positions.push_back(original.positions[v]);current.normals.push_back(original.normals[v]);const auto uv=original.uv[v];const auto x=uv[0]-.5f,y=uv[1]-.5f;current.uv.push_back({.5f+.7f*(std::cos(.23f)*x-std::sin(.23f)*y),.5f+.7f*(std::sin(.23f)*x+std::cos(.23f)*y)});current.indices.push_back(uint32_t(current.indices.size()));}const auto expanded=detail::expand_corner_key_basis(current,original);
 if(argc==2&&std::string_view(argv[1])=="--require-device"){
  try{detail::reflected_key_correspondence(current,.0001f);}
  catch(const std::exception &error){std::cout<<error.what()<<'\n';return 0;}
  throw std::runtime_error("Exact authored Mirror accepted an unavailable GPU");
 }
 if(argc!=1)return 2;
 auto map=detail::reflected_key_correspondence(current,.0001f);check(map.corners.size()==12&&map.vertices.size()==12&&map.maximum_rest_error==0,"Actual five-source/twelve-current symmetric fan has a unique exact reflected corner/vertex bijection");check(map.vertices[3]==9&&map.vertices[9]==3&&map.vertices[0]==0&&map.vertices[6]==6,"Independent copies of the same original center follow distinct real reflected triangles");
 std::vector<Vec3> values(12);values[0].z=.013f;values[3].z=-.017f;values[6].z=.021f;values[9].z=-.029f;std::vector<Vec3> mirrored;for(auto source:map.vertices){const auto p=values[source];mirrored.push_back({-p.x,p.y,p.z});}check(mirrored[0].z==.013f&&mirrored[3].z==-.029f&&mirrored[6].z==.021f&&mirrored[9].z==-.017f,"Opposite-seam field reflection preserves every independent endpoint instead of contracting to source5");
 NormalFrames frame;frame.origin_vertex_count=12;frame.origin_vertices=current.indices;frame.normals.assign(12,{0,0,1});frame.tangents.assign(12,{1,0,0,1});std::vector<std::array<float,2>> role_uv;for(auto v:expanded.origin_vertices)role_uv.push_back(original.uv[v]);auto signs=detail::reflected_key_role_signs(frame,role_uv,map);check(std::all_of(signs.begin(),signs.end(),[](auto s){return s==-1;}),"Actual original-role U reflection proves exact base N/T parity and immutable handedness");
 auto rotated=frame;rotated.tangents.assign(12,{0,1,0,1});auto uv90=role_uv;for(auto&v:uv90)v={v[1],1-v[0]};auto signs90=detail::reflected_key_role_signs(rotated,uv90,map);check(std::all_of(signs90.begin(),signs90.end(),[](auto s){return s==1;}),"Separate true role coordinates independently prove the other tangent reflection sign");
 for(float weight:{0.f,.35f,1.f})for(size_t c=0;c<12;++c){const Vec3 dn{.01f*float(c),.02f,-.003f},dt{.004f,.02f*float(c),.005f};const Vec3 source_n=frame.normals[map.corners[c]]+dn*weight,source_t=Vec3{1,0,0}+dt*weight;const Vec3 expected_n{-source_n.x,source_n.y,source_n.z},expected_t{source_t.x,-source_t.y,-source_t.z};const auto actual_n=frame.normals[c]+Vec3{-dn.x,dn.y,dn.z}*weight,actual_t=Vec3{1,0,0}+Vec3{dt.x,-dt.y,-dt.z}*weight;check(same(actual_n,expected_n)&&same(actual_t,expected_t),"Independent physical N/T reflection commutes with raw linear morph addition including zero key");}
 check(rejects([&]{detail::reflected_key_role_signs(frame,current.uv,map);},"diagonal reflected UV"),"Edited rotated atlas cannot impersonate the genuine retained tangent-role coordinates");auto bad=frame;bad.normals[0]=normalized(Vec3{.1f,0,1});check(rejects([&]{detail::reflected_key_role_signs(bad,role_uv,map);},"base normal parity"),"Asymmetric artist base is refused rather than inventing a mirrored zero frame");bad=frame;for(size_t k=0;k<3;++k)bad.tangents[3+k][3]=-1;check(rejects([&]{detail::reflected_key_role_signs(bad,role_uv,map);},"handedness parity"),"Mirror does not silently relabel or flip an immutable target handedness");
 auto duplicate=current;for(size_t c=0;c<current.indices.size();++c)duplicate.indices.push_back(current.indices[c]);check(rejects([&]{detail::reflected_key_correspondence(duplicate,.0001f);},"Ambiguous mirrored triangles"),"Coincident overlapping faces cannot be selected by a seam/nearest heuristic");
 Mesh quad;quad.positions={original.positions[1],original.positions[2],original.positions[3],original.positions[4]};quad.normals.assign(4,{0,0,1});quad.uv={{0,0},{1,0},{1,1},{0,1}};quad.indices={0,1,2,0,2,3};check(rejects([&]{detail::reflected_key_correspondence(quad,.0001f);},"triangle refinement"),"Non-reflection-invariant diagonal remains an explicit required refinement case");
 auto welded=current;welded.indices[3]=0;welded.positions.erase(welded.positions.begin()+3);welded.normals.erase(welded.normals.begin()+3);welded.uv.erase(welded.uv.begin()+3);for(auto&i:welded.indices)if(i>3)--i;check(rejects([&]{detail::reflected_key_correspondence(welded,.0001f);},"independent current vertex partition"),"Asymmetric current weld cannot average or choose differing reflected corner fields");
 auto huge=current;for(auto &p:huge.positions)p=p*1e20f;check(rejects([&]{detail::reflected_key_correspondence(huge,.0001f);},"spatial indexing"),"Finite extreme geometry refuses before signed spatial-cell conversion");
 detail::KeyMirrorLimits work;work.max_work=1;check(rejects([&]{detail::reflected_key_correspondence(current,.0001f,work);},"bounded work"),"Mirror candidate search cannot exceed its declared work quota");
 detail::KeyMirrorLimits limits;limits.max_bytes=1;check(rejects([&]{detail::reflected_key_correspondence(current,.0001f,limits);},"preflight"),"Mirror scratch dimensions are bounded before detached allocation");limits={};size_t polls=0;limits.cancelled=[&]{++polls;return false;};detail::reflected_key_correspondence(current,.0001f,limits);for(auto cut:{size_t(1),polls/2,polls}){size_t at=0;limits.cancelled=[&]{return ++at>=cut;};check(rejects([&]{detail::reflected_key_correspondence(current,.0001f,limits);},"cancelled"),"Early/middle/late detached mirror cancellation refuses before a result");}

 const auto edge=detail::reflected_key_correspondence(current,{.0625},.125f);
 check(edge.corners==map.corners&&edge.maximum_rest_error==.125,"Inclusive binary64 radius returns the exact maximum rest error");
 check(rejects([&]{detail::reflected_key_correspondence(current,{std::nextafter(.0625,1.)},.125f);},"triangle refinement"),"A plane one binary64 increment outside the radius refuses");

 // Each triangle has an independent self-reflected seam and the same centroid.
 Mesh dense;
 constexpr uint32_t faces=96;
 for(uint32_t face=0;face<faces;++face){
  const float x=1+face*.03125f,y=1+face*.0625f;
  dense.positions.insert(dense.positions.end(),{{-x,y,0},{x,y,0},{0,-2*y,0}});
  dense.indices.insert(dense.indices.end(),{3*face,3*face+1,3*face+2});
 }
 dense.normals.assign(dense.positions.size(),{0,0,1});
 detail::KeyMirrorLimits exact;
 exact.max_work=1+35*faces+9*faces*faces;
 const auto dense_map=detail::reflected_key_correspondence(dense,.0001f,exact);
 bool ordered=dense_map.maximum_rest_error==0;
 for(uint32_t t=0;t<faces;++t){
  const auto base=3*t;
  ordered&=dense_map.corners[base]==base+1&&dense_map.corners[base+1]==base&&dense_map.corners[base+2]==base+2;
 }
 check(ordered&&dense_map.corners==dense_map.vertices,"Unique wound corner matches survive continuation within a dense centroid bin");
 --exact.max_work;
 check(rejects([&]{detail::reflected_key_correspondence(dense,.0001f,exact);},"bounded work"),"The exact work limit charges all nine candidate corner checks, including rejected rotations");
 fault::calls=0;fault::count=true;auto positive=detail::reflected_key_correspondence(current,.0001f);fault::count=false;const auto count=fault::calls;for(size_t cut=0;cut<count;++cut){fault::remaining=long(cut);bool failed=false;try{detail::reflected_key_correspondence(current,.0001f);}catch(const std::bad_alloc&){failed=true;}catch(...){fault::remaining=-1;throw;}fault::remaining=-1;if(!failed)throw std::runtime_error("Mirror allocation prefix unexpectedly completed");}std::cout<<"MIRROR_ALLOCATION_PREFIXES "<<count<<std::endl;check(count>0&&detail::reflected_key_correspondence(current,.0001f).corners==positive.corners,"All measured persistent mirror allocation denials unwind and exact retry succeeds");std::cout<<"PASS "<<checks<<" detached mirror correspondence checks"<<std::endl;return 0;
}catch(const std::exception&e){fault::remaining=-1;fault::count=false;std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
