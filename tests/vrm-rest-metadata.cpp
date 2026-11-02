// SPDX-License-Identifier: GPL-3.0-or-later
#include "vrm-source-graph.hpp"
#include "dome/creator-archive.hpp"
#include <cmath>
#include <iostream>
using namespace dome;
using namespace dome::detail;
using Json=nlohmann::json;
namespace {
size_t checks=0;
void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);++checks;}
template<class F>void reject(F call,const char *why){try{call();}catch(const std::exception&){++checks;return;}throw std::runtime_error(why);}
using M=std::array<double,16>;using V=std::array<double,3>;
M identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
M mat(const ImportedMatrix &v){M r{};for(size_t i=0;i<16;++i)r[i]=v[i];return r;}
ImportedMatrix raw(const M&v){ImportedMatrix r{};for(size_t i=0;i<16;++i)r[i]=float(v[i]);return r;}
M mul(const M&a,const M&b){M r{};for(int c=0;c<4;++c)for(int y=0;y<4;++y)for(int k=0;k<4;++k)r[c*4+y]+=a[k*4+y]*b[c*4+k];return r;}
V point(const M&m,V p){V r{};for(int y=0;y<3;++y){r[y]=m[12+y];for(int x=0;x<3;++x)r[y]+=m[x*4+y]*p[x];}return r;}
void near(V a,V b){for(int i=0;i<3;++i)if(std::abs(a[i]-b[i])>2e-6)throw std::runtime_error("Independent metadata WORLD point mismatch");++checks;}
void near(M a,M b){for(int i=0;i<16;++i)if(std::abs(a[i]-b[i])>2e-6)throw std::runtime_error("Independent attachment WORLD basis mismatch");++checks;}
V unity(V p){p[2]=-p[2];return p;}
V offset(const Json&j){return {j.at("x").get<double>(),j.at("y").get<double>(),j.at("z").get<double>()};}
M translated(double x,double y,double z){auto m=identity();m[12]=x;m[13]=y;m[14]=z;return m;}
struct Fixture {
 VrmSourceGraph graph;std::shared_ptr<ImportedAsset> asset=std::make_shared<ImportedAsset>();Json vrm;
 Fixture(){
  asset->nodes.resize(7);graph.parents={-1,0,1,0,0,4,4};
  std::vector<M> locals{identity(),translated(.02,.3,-.04),translated(.05,.15,.08),translated(-.2,.1,.02),translated(.1,-.1,.2),translated(.01,.06,-.04),translated(-.01,.04,.03)};
  for(size_t n=0;n<locals.size();++n){auto &node=asset->nodes[n];node.parent=graph.parents[n];node.local=raw(locals[n]);node.world=node.parent<0?node.local:raw(mul(mat(asset->nodes[size_t(node.parent)].world),locals[n]));graph.rest_local.push_back(node.local);graph.rest_world.push_back(node.world);graph.current_local.push_back(node.local);}
  asset->nodes[0].children={4,3,1};asset->nodes[1].children={2};asset->nodes[4].children={6,5};
  auto model=std::make_shared<CreatorSourceModel>();model->asset=asset;graph.model=model;
  graph.names.resize(asset->nodes.size());for(const auto&node:asset->nodes)graph.original_nodes.push_back({{"matrix",node.local}});
  graph.humanoid={{"humanBones",Json::array({Json{{"bone","head"},{"node",2},{"useDefaultValues",true}}})}};
  vrm={{"firstPerson",{{"firstPersonBone",2},{"firstPersonBoneOffset",{{"x",.017},{"y",.061},{"z",-.033}}}}},
       {"secondaryAnimation",{{"colliderGroups",Json::array({Json{{"node",3},{"colliders",Json::array({Json{{"offset",{{"x",.02},{"y",-.03},{"z",.04}}},{"radius",.12}}})}}})},
       {"boneGroups",Json::array({Json{{"bones",{4}},{"center",1},{"dragForce",.4},{"gravityDir",{{"x",0},{"y",-1},{"z",0}}},{"hitRadius",.025}}})}}}};
 }
 void rest(size_t node,M value){graph.rest_local[node]=raw(value);for(size_t n=0;n<asset->nodes.size();++n)graph.rest_world[n]=graph.parents[n]<0?graph.rest_local[n]:raw(mul(mat(graph.rest_world[size_t(graph.parents[n])]),mat(graph.rest_local[n])));}
};
}
int main()try{
 Fixture base;auto unchanged=prepare_vrm0_rest_metadata(base.graph,base.vrm);
 check(unchanged.first_person==base.vrm.at("firstPerson")&&!unchanged.camera_changed&&!unchanged.colliders[0]&&!unchanged.centers[0],"Unedited metadata must be byte/value exact with no helper");
 const std::array<M,3> edits{translated(.08,.11,-.05),M{0,1.1,0,0,-.8,0,0,0,0,0,.9,0,.07,.12,-.03,1},M{-1,.15,0,0,.2,.9,0,0,0,0,1.2,0,.02,.16,.04,1}};
 for(const auto &edit:edits){
  auto f=base;f.rest(2,edit);f.rest(3,translated(-.16,.08,.05));f.rest(1,mul(translated(.01,.02,-.03),mat(f.graph.rest_local[1])));
  const auto raw_meta=f.vrm;auto out=prepare_vrm0_rest_metadata(f.graph,f.vrm);
  check(f.vrm==raw_meta,"Preparing metadata must not mutate raw JSON");check(out.camera_changed&&out.first_person.at("firstPersonBone")==2,"Offset changes must retain the true head-erasure bone");
  const auto before=point(mat(f.asset->nodes[2].world),unity(offset(raw_meta.at("firstPerson").at("firstPersonBoneOffset"))));
  const auto after=point(mat(f.graph.rest_world[2]),unity(offset(out.first_person.at("firstPersonBoneOffset"))));near(before,after);
  const auto wrong=point(mat(f.graph.rest_world[2]),offset(out.first_person.at("firstPersonBoneOffset")));
  check(std::abs(before[2]-wrong[2])>.01,"Wrong handedness must visibly fail the independent point oracle");
  check(out.colliders[0]&&out.colliders[0]->source==3&&out.centers[0]&&out.centers[0]->source==1,"Collider and center helpers retain actual source identities");
  for(auto frame:{*out.colliders[0],*out.centers[0]}){
   const auto resolved=mul(mat(f.graph.rest_world[frame.source]),mat(frame.local));near(resolved,mat(f.asset->nodes[frame.source].world));
   // Independent columns imply equal sphere center AND maximum column scale.
   // No radius approximation is used, and no dynamic simulation claim follows.
   const auto p=unity(V{.02,-.03,.04});near(point(resolved,p),point(mat(f.asset->nodes[frame.source].world),p));
   auto radius=[&](M m){double scale=0;for(int c=0;c<3;++c){double square=0;for(int r=0;r<3;++r)square+=m[c*4+r]*m[c*4+r];scale=std::max(scale,std::sqrt(square));}return .12*scale;};
   check(std::abs(radius(resolved)-radius(mat(f.asset->nodes[frame.source].world)))<2e-6,"Full basis transport must preserve matrix-derived raw sphere radius");
   const auto pose=mul(translated(.13,-.02,.04),M{.8,.6,0,0,-.6,.8,0,0,0,0,1,0,0,0,0,1});
   near(mul(pose,resolved),mul(pose,mat(f.asset->nodes[frame.source].world)));
  }
  // Prefix affine placement is an outer transform; it cannot change local JSON.
  auto with_prefix=f;SceneGroup group;group.transform.position={.3f,-.2f,.1f};group.transform.scale=1.3f;with_prefix.graph.prefix.push_back(group);
  auto prefixed=prepare_vrm0_rest_metadata(with_prefix.graph,with_prefix.vrm);check(prefixed.first_person==out.first_person&&prefixed.colliders[0]->local==out.colliders[0]->local,"Native prefix placement must not be folded twice into source-local metadata");
 }
 {auto f=base;f.rest(1,edits[2]);auto out=prepare_vrm0_rest_metadata(f.graph,f.vrm);check(out.centers[0].has_value(),"General affine center retains its complete matrix without pretending to be a sphere");near(mul(mat(f.graph.rest_world[1]),mat(out.centers[0]->local)),mat(f.asset->nodes[1].world));}
 {auto f=base;f.rest(3,edits[1]);reject([&]{prepare_vrm0_rest_metadata(f.graph,f.vrm);},"Arbitrary collider linear/shear correction must retain an explicit runtime-radius guard");}
 {auto f=base;f.vrm["firstPerson"]["firstPersonBoneOffset"]={{"y",.061}};f.rest(2,edits[0]);auto out=prepare_vrm0_rest_metadata(f.graph,f.vrm);near(point(mat(f.graph.rest_world[2]),unity(offset(out.first_person.at("firstPersonBoneOffset")))),point(mat(f.asset->nodes[2].world),V{0,.061,0}));}
 {
  auto f=base;f.rest(3,translated(-.16,.08,.05));f.rest(1,edits[2]);f.graph.current_local=f.graph.rest_local;f.graph.rest_metadata=prepare_vrm0_rest_metadata(f.graph,f.vrm);
  GltfDocument document;document.json={{"nodes",Json::array()},{"scenes",Json::array({Json{{"nodes",Json::array()}}})}};std::vector<int> metadata;
  const auto nodes=emit_vrm_source_graph(document,{},f.graph,{},&metadata);
  const auto &children=document.json.at("nodes").at(nodes[4]).at("children");check(children==Json::array({nodes[6],nodes[5]}),"Actual emitter preserves authored spring first-child order rather than sorting numeric source IDs");
  check(metadata[3]!=nodes[3]&&metadata[1]!=nodes[1]&&metadata[2]==nodes[2],"Actual emitted attachment references stay separate from humanoid/first-person identities");
  std::vector<int> parents(document.json.at("nodes").size(),-1);for(size_t n=0;n<parents.size();++n)if(document.json.at("nodes").at(n).contains("children"))for(const auto&child:document.json.at("nodes").at(n).at("children"))parents[child.get<size_t>()]=int(n);
  auto world=[&](auto&&self,size_t n)->M{const auto&node=document.json.at("nodes").at(n);auto local=node.contains("matrix")?node.at("matrix").get<M>():identity();return parents[n]<0?local:mul(self(self,size_t(parents[n])),local);};
  for(auto n:{1,3})near(world(world,size_t(metadata[n])),mat(f.asset->nodes[n].world));
 }
 for(bool sentinel:{false,true}){auto f=base;f.vrm["firstPerson"].erase("firstPersonBoneOffset");if(sentinel)f.vrm["firstPerson"]["firstPersonBone"]=-1;else f.vrm["firstPerson"].erase("firstPersonBone");f.rest(2,edits[0]);auto out=prepare_vrm0_rest_metadata(f.graph,f.vrm);near(point(mat(f.graph.rest_world[2]),unity(offset(out.first_person.at("firstPersonBoneOffset")))),point(mat(f.asset->nodes[2].world),V{}));check(out.first_person.contains("firstPersonBone")==sentinel,"Missing/default head identity must remain authored, not synthesized");}
 {auto f=base;f.graph.rest_local[4][4]=.2f;f.graph.current_local[4][4]=.2f;reject([&]{prepare_vrm0_rest_metadata(f.graph,f.vrm);},"Sheared spring helper must refuse");}
 {auto f=base;f.asset=std::make_shared<ImportedAsset>(*base.asset);auto model=std::make_shared<CreatorSourceModel>(*base.graph.model);model->asset=f.asset;f.graph.model=model;f.asset->nodes[4].mesh=0;reject([&]{prepare_vrm0_rest_metadata(f.graph,f.vrm);},"Rendered child inside spring chain must refuse");}
 {auto f=base;f.graph.parents[5]=0;reject([&]{prepare_vrm0_rest_metadata(f.graph,f.vrm);},"Removed spring child must refuse");}
 {auto f=base;f.graph.parents[3]=4;reject([&]{prepare_vrm0_rest_metadata(f.graph,f.vrm);},"Inserted spring child must refuse");}
 {auto f=base;f.rest(4,translated(.2,.1,-.1));reject([&]{prepare_vrm0_rest_metadata(f.graph,f.vrm);},"Changed spring rest tail/axes must refuse");}
 {auto f=base;f.vrm["secondaryAnimation"]["colliderGroups"][0]["node"]=4;f.graph.rest_world[4][12]+=.01f;reject([&]{prepare_vrm0_rest_metadata(f.graph,f.vrm);},"A helper inside a spring chain must refuse");}
 {auto f=base;f.graph.humanoid["humanBones"][0]["useDefaultValues"]=false;f.rest(2,edits[1]);reject([&]{prepare_vrm0_rest_metadata(f.graph,f.vrm);},"Custom human limits cannot silently follow changed axes");}
 {auto f=base;f.vrm["firstPerson"]["firstPersonBone"]=uint64_t(-1);reject([&]{prepare_vrm0_rest_metadata(f.graph,f.vrm);},"Unsigned overflow must not masquerade as sentinel minus one");}
 {auto f=base;f.graph.parents[0]=2;reject([&]{prepare_vrm0_rest_metadata(f.graph,f.vrm);},"Current graph cycle must refuse");}
 {auto f=base;f.vrm["secondaryAnimation"]["boneGroups"][0]["bones"]={100};reject([&]{prepare_vrm0_rest_metadata(f.graph,f.vrm);},"Out-of-graph spring root must refuse");}
 reject([&]{prepare_vrm0_rest_metadata(base.graph,base.vrm,[]{return true;});},"Cancellation must refuse before publication");
 std::cout<<checks<<" independent VRM rest metadata checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
