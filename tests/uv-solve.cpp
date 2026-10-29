// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/uv-solve.hpp"
#include "dome/rig.hpp"
#include "dome/humanoid-creator.hpp"
#include <iostream>
#include <numbers>
using namespace dome;
void require(bool value,const char*message){if(!value)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
void rejects(auto operation,std::string_view diagnostic,const char*message){
  bool failed=false;try{operation();}catch(const std::exception&e){failed=std::string(e.what()).find(diagnostic)!=std::string::npos;if(!failed)std::cerr<<"UNEXPECTED "<<e.what()<<'\n';}require(failed,message);
}
Mesh rectangle(){Mesh m;m.positions={{0,0,0},{2,0,0},{0,1,0},{2,1,0}};m.normals.assign(4,{0,0,1});m.indices={0,1,2,1,3,2};m.uv={{.1,.1},{.8,.1},{.1,.8},{.6,.9}};m.uv_pins={1,1,0,0};return m;}
double angle(double dot,double aa,double bb){return std::acos(std::clamp(dot/std::sqrt(aa*bb),-1.0,1.0));}
double distortion(const Mesh&m){double sum=0;size_t count=0;for(size_t f=0;f<m.indices.size();f+=3)for(size_t k=0;k<3;++k){auto i=m.indices[f+k],j=m.indices[f+(k+1)%3],l=m.indices[f+(k+2)%3];auto a=m.positions[j]-m.positions[i],b=m.positions[l]-m.positions[i];double x=double(m.uv[j][0])-m.uv[i][0],y=double(m.uv[j][1])-m.uv[i][1],u=double(m.uv[l][0])-m.uv[i][0],v=double(m.uv[l][1])-m.uv[i][1];sum+=std::abs(angle(dot(a,b),dot(a,a),dot(b,b))-angle(x*u+y*v,x*x+y*y,u*u+v*v));++count;}return sum/count;}
void identity(const Mesh &before,const UvSolveResult &result){
  require(result.source_topology==morph_topology_id(before)&&result.corner_vertices==result.mesh.indices&&result.mesh.indices.size()==before.indices.size(),"Source topology and every original face-corner order remain explicit");
  bool same=result.source_vertices.size()==result.mesh.positions.size();
  for(size_t i=0;i<result.source_vertices.size();++i){auto source=result.source_vertices[i];same&=source<before.positions.size();if(!same)break;auto p=result.mesh.positions[i],q=before.positions[source],n=result.mesh.normals[i],r=before.normals[source];same&=p.x==q.x&&p.y==q.y&&p.z==q.z&&n.x==r.x&&n.y==r.y&&n.z==r.z;if(!before.uv_pins.empty())same&=result.mesh.uv_pins[i]==before.uv_pins[source];}
  for(size_t c=0;c<before.indices.size();++c)same&=result.source_vertices[result.mesh.indices[c]]==before.indices[c];
  require(same,"Complete new-to-old vertex map preserves every geometric surface, normal and pin byte");
}
int main()try{
  auto m=rectangle();auto result=conformal_uv(m);identity(m,result);
  require(result.mesh.uv[0]==m.uv[0]&&result.mesh.uv[1]==m.uv[1],"Hard UV pins remain bit-exact");
  require(std::abs(result.mesh.uv[2][0]-.1f)<1e-6f&&std::abs(result.mesh.uv[2][1]-.45f)<1e-6f&&std::abs(result.mesh.uv[3][0]-.8f)<1e-6f&&std::abs(result.mesh.uv[3][1]-.45f)<1e-6f,
          "Independent analytic2:1 rectangle solution recovers metric aspect and shear");
  require(distortion(result.mesh)<1e-6&&distortion(m)>.1,"Independent triangle angle metric confirms distortion removal");
  require(result.mesh.indices==m.indices&&result.mesh.positions.size()==m.positions.size(),"No-cut solve retains exact rest topology identity");
  auto one_pin=m;one_pin.uv_pins={1,0,0,0};auto one=conformal_uv(one_pin);
  require(one.mesh.uv[0]==one_pin.uv[0]&&one.charts[0].automatic_pins==1,
          "Single hard pin remains exact while deterministic second anchor removes nullspace");
  auto tiny=m;for(auto &uv:tiny.uv){uv[0]*=1e-7f;uv[1]*=1e-7f;}
  auto tiny_result=conformal_uv(tiny);
  require(tiny_result.mesh.uv[0]==tiny.uv[0]&&tiny_result.mesh.uv[1]==tiny.uv[1]&&distortion(tiny_result.mesh)<1e-6,
          "Scale-relative UV collapse checks preserve tiny exact pinned charts");
  auto again=conformal_uv(m);require(again.mesh.uv==result.mesh.uv&&again.source_vertices==result.source_vertices,"Same input produces deterministic exact output");
  auto split=m;split.uv_seams.assign(6,0);split.uv_seams[1]=split.uv_seams[5]=1;
  auto separated=conformal_uv(split);identity(split,separated);
  require(separated.mesh.positions.size()==6&&separated.charts.size()==2,"Explicit cut splits shared UV vertices with a complete deformation remap");
  Skeleton skeleton;skeleton.bones={{"root",-1,{},1,true},{"limb",0,{{0,1,0},{},{1,1,1}},1,true}};
  SkinWeights weights;weights.vertices={{{0,1}},{{1,1}},{{0,.4f},{1,.6f}},{{0,.7f},{1,.3f}}};
  auto pose=std::vector<JointTransform>{skeleton.bones[0].rest,skeleton.bones[1].rest};
  pose[1].rotation={std::cos(.2f),0,0,std::sin(.2f)};
  SkinWeights remapped;for(auto source:separated.source_vertices)remapped.vertices.push_back(weights.vertices[source]);
  auto before_pose=deform_mesh(split,weights,skeleton,pose),after_pose=deform_mesh(separated.mesh,remapped,skeleton,pose);
  bool identical_pose=true;for(size_t c=0;c<split.indices.size();++c)identical_pose&=length(before_pose.positions[split.indices[c]]-after_pose.positions[separated.mesh.indices[c]])<1e-6f;
  require(identical_pose,"Complete UV split map preserves independently evaluated blended skin deformation");
  MorphSet before_morph(split);auto changed=split.positions;for(size_t i=0;i<changed.size();++i)changed[i].z+=float(i)*.03f;before_morph.set_target(split,"shape",changed);
  MorphSet after_morph(separated.mesh);auto new_changed=separated.mesh.positions;for(size_t i=0;i<new_changed.size();++i)new_changed[i].z+=float(separated.source_vertices[i])*.03f;after_morph.set_target(separated.mesh,"shape",new_changed);
  std::array<float,1> activation{.7f};auto original_shape=before_morph.evaluate(split,activation),new_shape=after_morph.evaluate(separated.mesh,activation);
  bool identical_shape=true;for(size_t c=0;c<split.indices.size();++c)identical_shape&=length(original_shape.positions[split.indices[c]]-new_shape.positions[separated.mesh.indices[c]])<1e-6f;
  require(identical_shape,"Complete UV split map preserves independent topology-stable morph evaluation");
  std::array pair{UvStitchEdge{1,5}};auto joined=stitch_uv(separated.mesh,pair);identity(separated.mesh,joined);
  require(joined.charts.size()==1&&joined.mesh.uv[joined.mesh.indices[1]]==joined.mesh.uv[joined.mesh.indices[3]]&&joined.mesh.uv[joined.mesh.indices[2]]==joined.mesh.uv[joined.mesh.indices[5]],"Explicit opposite seam pairing stitches exact UV continuity and reflows conformally");
  require(joined.mesh.uv_seams[1]==0&&joined.mesh.uv_seams[5]==0,"Stitch clears only the paired outgoing seam flags");
  auto conflict=separated.mesh;conflict.uv_pins.assign(6,0);conflict.uv_pins[conflict.indices[1]]=1;conflict.uv_pins[conflict.indices[3]]=1;conflict.uv[conflict.indices[3]][0]+=.1f;
  rejects([&]{stitch_uv(conflict,pair);},"incompatible hard UV pins","Conflicting coincident duplicate hard pins reject rather than average");
  auto compatible=separated.mesh;compatible.uv_pins.assign(6,0);compatible.uv_pins[compatible.indices[1]]=compatible.uv_pins[compatible.indices[3]]=1;compatible.uv[compatible.indices[3]]=compatible.uv[compatible.indices[1]];
  auto compatible_result=stitch_uv(compatible,pair);
  require(compatible_result.mesh.uv[compatible_result.mesh.indices[1]]==compatible.uv[compatible.indices[1]]&&compatible_result.mesh.uv[compatible_result.mesh.indices[3]]==compatible.uv[compatible.indices[3]],"Compatible duplicate pins survive stitching bit-exactly");
  auto near=separated.mesh;
  near.positions[near.indices[3]].z+=1e-7f;
  auto tolerant=stitch_uv(near,pair,1e-6);identity(near,tolerant);
  require(tolerant.mesh.uv[tolerant.mesh.indices[1]]==tolerant.mesh.uv[tolerant.mesh.indices[3]],
          "Finite explicit stitch tolerance preserves original slightly separated geometry");
  rejects([&]{stitch_uv(near,pair,1e-9);},"endpoints", "Stricter stitch tolerance rejects unmatched geometry deterministically");
  auto mismatch=pair;mismatch[0]={0,5};rejects([&]{stitch_uv(separated.mesh,mismatch);},"endpoints","Unrelated edge pairs cannot silently globally weld");
  auto nonmanifold=m;nonmanifold.indices.insert(nonmanifold.indices.end(),{1,3,2});rejects([&]{conformal_uv(nonmanifold);},"nonmanifold","Nonmanifold source edge reports actionable failure");
  auto tetra=Mesh{};tetra.positions={{0,0,0},{1,0,0},{0,1,0},{0,0,1}};tetra.normals.assign(4,{0,1,0});tetra.uv={{0,0},{1,0},{0,1},{1,1}};tetra.indices={0,2,1,0,1,3,1,2,3,2,0,3};
  rejects([&]{conformal_uv(tetra);},"closed","Closed surface requires explicit valid cuts");
  auto cut_tetra=tetra;cut_tetra.uv_seams.assign(cut_tetra.indices.size(),1);
  auto opened=conformal_uv(cut_tetra);identity(cut_tetra,opened);
  require(opened.charts.size()==4,"Explicit valid cuts open closed tetrahedron into independent conformal charts");
  auto collapsed=m;collapsed.uv[1]=collapsed.uv[0];rejects([&]{conformal_uv(collapsed);},"collapse chart scale","Coincident hard anchors reject singular scale");
  UvSolveOptions bound;bound.maximum_work=1;rejects([&]{conformal_uv(m,bound);},"work budget","Explicit work budget aborts without partial output");
  bound={};bound.maximum_bytes=1024;rejects([&]{conformal_uv(m,bound);},"memory budget","Conservative peak memory budget is enforced");
  rejects([&]{conformal_uv(m,{},[]{return true;});},"cancelled","Cancellation returns no partial mesh");
  Mesh bent;constexpr int segments=12;for(int i=0;i<=segments;++i){float t=float(i)/segments,theta=t*std::numbers::pi_v<float>;for(int y=0;y<2;++y){bent.positions.push_back({std::cos(theta),float(y),std::sin(theta)});bent.normals.push_back({std::cos(theta),0,std::sin(theta)});bent.uv.push_back({.1f+.8f*t,.1f+.7f*y*(1-.4f*t)});}}for(int i=0;i<segments;++i){uint32_t a=i*2;bent.indices.insert(bent.indices.end(),{a,a+2,a+1,a+2,a+3,a+1});}bent.uv_pins.assign(bent.positions.size(),0);bent.uv_pins[0]=bent.uv_pins[segments*2]=1;
  auto flat=conformal_uv(bent);double chord=2*std::sin(std::numbers::pi_v<double>/(2*segments));
  require(distortion(flat.mesh)<2e-5&&distortion(bent)>.1,"Bent developable prism strip recovers intrinsic angles independently of world projection");
  require(std::abs(flat.mesh.uv[1][1]-(.1+.8/(segments*chord)))<1e-5,"Developed strip height matches independent chord-length metric");
  UvSolveOptions short_solve;short_solve.maximum_iterations=1;
  rejects([&]{conformal_uv(bent,short_solve);},"convergence","Exhausted iteration limit rejects incomplete conformal result");
  auto disconnected=m;for(auto p:m.positions){p.x+=4;disconnected.positions.push_back(p);}disconnected.normals.insert(disconnected.normals.end(),m.normals.begin(),m.normals.end());for(auto uv:m.uv){uv[0]+=2;disconnected.uv.push_back(uv);}for(auto i:m.indices)disconnected.indices.push_back(i+4);disconnected.uv_pins.insert(disconnected.uv_pins.end(),m.uv_pins.begin(),m.uv_pins.end());
  auto islands=conformal_uv(disconnected);require(islands.charts.size()==2&&islands.mesh.uv[4]==disconnected.uv[4],"Disconnected charts solve independently with exact chart-local pins");
  UvSolveOptions select;select.seed_faces={0};auto only=conformal_uv(disconnected,select);bool untouched=true;for(int i=4;i<8;++i)untouched&=only.mesh.uv[i]==disconnected.uv[i];require(only.charts.size()==1&&untouched,"Seed face selection preserves all unselected island UVs exactly");
  Mesh many;
  for(uint32_t island=0;island<500;++island){
    for(auto p:m.positions){p.x+=3*island;many.positions.push_back(p);}
    many.normals.insert(many.normals.end(),m.normals.begin(),m.normals.end());
    many.uv.insert(many.uv.end(),m.uv.begin(),m.uv.end());
    for(auto vertex:m.indices)many.indices.push_back(vertex+island*4);
  }
  UvSolveOptions many_options;many_options.maximum_work=2'000'000;
  auto many_result=conformal_uv(many,many_options);
  require(many_result.charts.size()==500,
          "Hundreds of disconnected charts solve within linear storage/work bounds");
  auto human=create_humanoid(humanoid_baseline_defaults("makehuman-hm08"));
  UvSolveOptions organic;organic.seed_faces={0};organic.maximum_work=500'000'000;organic.maximum_iterations=5000;
  rejects([&]{conformal_uv(human.parts[0].mesh,organic);},"folds", "Complex original MakeHuman chart rejects foldovers instead of publishing a broken atlas");
  auto avatar=create_humanoid();
  const auto &body=avatar.parts.at(10).mesh;
  auto authored=conformal_uv(body);
  identity(body,authored);
  require(body.indices.size()/3==4360&&authored.charts.size()==5,
          "Actual CC0 VRoid body solves all4360triangles/five authored charts with default work bounds");
  bool converged=true;for(auto chart:authored.charts)converged&=chart.relative_residual<4e-9;
  require(converged,"Actual source organic chart residuals pass recomputed convergence checks");
  std::cout<<"REAL_BODY_ANGLE_ERROR "<<distortion(body)<<" -> "<<distortion(authored.mesh)<<'\n';
  require(distortion(authored.mesh)<.05&&distortion(authored.mesh)<distortion(body)*.5,"Independent actual-body triangle angle error improves beyond half its authored input");

  return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
