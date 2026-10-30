// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/procedural-sculpt.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
template<class F> bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
nlohmann::json archive(const Volume &v){auto j=v.encode();std::sort(j["samples"].begin(),j["samples"].end());for(auto &l:j["sculpt_layers"])std::sort(l["delta"].begin(),l["delta"].end());if(j.contains("sculpt_mask"))std::sort(j["sculpt_mask"]["values"].begin(),j["sculpt_mask"]["values"].end());return j;}
Volume cube(){Volume::Voxels samples;const float voxel=.04f,band=.16f;for(int z=-12;z<=12;++z)for(int y=-12;y<=12;++y)for(int x=-12;x<=12;++x){double q[]={std::abs(x*double(voxel))-.3,std::abs(y*double(voxel))-.3,std::abs(z*double(voxel))-.3};double outside=0;for(auto v:q)outside+=std::max(0.,v)*std::max(0.,v);float d=std::clamp(float(std::sqrt(outside)+std::min(std::max({q[0],q[1],q[2]}),0.)),-band,band);if(d<band)samples.emplace(Int3{x,y,z},d);}return Volume::from_samples(voxel,std::move(samples));}
}
int main()try {
  Volume source=cube();auto original=archive(source);auto owner=source.geometry_owner();
  CsgBox panel{{-.12f,.22f,-.12f},{.12f,.46f,.12f},.015f,CsgOperation::Union};
  ProceduralSculptOptions options;options.depth=.12f;
  auto raised=compose_sculpt_boxes(source,std::span(&panel,1),options,"Panels 日本語");
  check(raised.replacement&&raised.report.changed&&raised.report.changed_samples>0,"Detached CSG creates an actual changed layer");
  auto &r=*raised.replacement;
  check(source.geometry_owner()==owner&&archive(source)==original,"Preparation leaves source samples, layers, settings, IDs and archive exact");
  check(r.at({0,10,0})<0&&source.at({0,10,0})>0,"Raised panel adds a real solid outside the original cube");
  check(r.layers().size()==2&&r.layers().back().name=="Panels 日本語"&&r.layers()[0].delta.owner()==source.layers()[0].delta.owner(),"New named layer shares every unchanged original displacement page");
  check(r.geometry_change(owner).kind==Volume::GeometryChange::Kind::Local,"Procedural geometry carries authenticated local extraction history");
  auto raised_archive=archive(r);auto id=r.active_layer();r.layer_properties(id,"Panels 日本語",false,false,1);
  check(r.voxels()==source.voxels(),"Hiding the generated layer restores every original field sample");
  r.layer_properties(id,"Panels 日本語",true,false,1);check(archive(r)==raised_archive,"Showing the generated layer recomposes its exact original result");
  auto roundtrip=Volume::decode(r.encode());check(archive(roundtrip)==raised_archive,"Existing native archive restores generated layer, protection and geometry");
  panel.operation=CsgOperation::Difference;panel.minimum.y=.16f;
  auto cut=compose_sculpt_boxes(source,std::span(&panel,1),options);
  check(cut.replacement&&cut.replacement->at({0,7,0})>0&&source.at({0,7,0})<0,"Recess is a real subtractive cavity in the original solid");
  auto partial=source.geometry_snapshot();partial.fill_mask(.37f);
  auto protected_cut=compose_sculpt_boxes(partial,std::span(&panel,1),options);
  float before=source.at({0,7,0}),target=cut.replacement->at({0,7,0});
  check(protected_cut.replacement&&std::abs(protected_cut.replacement->at({0,7,0})-(before+(target-before)*.63f))<1e-7f,"Fractional protection blends once against the complete ordered Boolean result");
  auto all=source.geometry_snapshot();all.fill_mask(1);auto all_before=archive(all);
  check(!compose_sculpt_boxes(all,std::span(&panel,1),options).replacement&&archive(all)==all_before,"Full protection is exact no-op without layer or future-ID consumption");
  check(!compose_sculpt_boxes(source,{},options).replacement,"Empty primitive list is an exact no-op");
  auto outside=panel;outside.minimum={2,2,2};outside.maximum={2.1f,2.1f,2.1f};
  check(!compose_sculpt_boxes(source,std::span(&outside,1),options).replacement,"Subtraction outside the source creates no empty history layer");
  options.rows=3;options.columns=3;options.density=1;options.depth=.12f;
  auto a=procedural_sculpt(source,options),b=procedural_sculpt(source,options);
  check(a.replacement&&b.replacement&&a.report.projected==9&&archive(*a.replacement)==archive(*b.replacement),"Seeded projection reproduces nine surface-attached panel placements exactly");
  options.seed=17;auto c=procedural_sculpt(source,options);
  check(c.replacement&&c.replacement->voxels()!=a.replacement->voxels(),"Changing seed changes actual procedural geometry");
  for(unsigned axis=0;axis<3;++axis)for(bool reverse:{false,true}){options.axis=axis;options.reverse=reverse;auto v=procedural_sculpt(source,options);check(v.replacement&&v.report.projected==9,"Every positive and negative object-local face projects actual detail");}
  auto symmetric=source.geometry_snapshot();auto settings=symmetric.sculpt_settings();settings.symmetry_axes=2;symmetric.sculpt_settings(settings);
  options.axis=1;options.reverse=false;auto mirrored=procedural_sculpt(symmetric,options);
  check(mirrored.replacement&&mirrored.report.primitives==18,"Current symmetry settings mirror each procedural primitive");
  bool matches=true;for(auto [q,value]:mirrored.replacement->voxels())matches&=std::abs(value-mirrored.replacement->at({q.x,-q.y,q.z}))<2e-7f;
  check(matches,"Mirrored field is symmetric on both sides of the object");
  options.pattern=ProceduralSculptOptions::Pattern::Mixed;auto mixed=procedural_sculpt(source,options);
  check(mixed.replacement&&mixed.report.primitives==9,"Mixed seeded pattern composes raised and subtractive detail in one layer");
  options.pattern=ProceduralSculptOptions::Pattern::Recesses;auto recessed=procedural_sculpt(source,options);
  check(recessed.replacement&&recessed.replacement->voxels()!=a.replacement->voxels(),"Recess pattern is distinct from raised panel geometry");
  options.density=0;check(!procedural_sculpt(source,options).replacement,"Zero density creates neither data nor history");options.density=1;
  auto low=options;low.maximum_samples=8;check(refuses([&]{procedural_sculpt(source,low);}),"Sample overflow refuses the entire detached result");
  low=options;low.maximum_work=8;check(refuses([&]{procedural_sculpt(source,low);}),"Scan/work budget refuses without partial publication");
  low=options;low.maximum_workspace_bytes=1024;check(refuses([&]{procedural_sculpt(source,low);}),"Actual host allocations obey the explicit workspace byte budget");
  size_t callbacks=0;auto cancel=options;cancel.rows=cancel.columns=1;cancel.cancelled=[&]{++callbacks;return false;};procedural_sculpt(source,cancel);
  for(size_t boundary=0;boundary<callbacks;++boundary){size_t call=0;cancel.cancelled=[&]{return call++==boundary;};if(!refuses([&]{procedural_sculpt(source,cancel);})||source.geometry_owner()!=owner)throw std::runtime_error("Cancelled procedural work published source state");}
  check(archive(source)==original,"Every observed preparation/compute/publication cancellation boundary leaves exact source state");
  Scene scene;scene.active().volume=std::make_shared<Volume>(source.geometry_snapshot());scene.active().name="Retained";auto other=scene.add("Other",Volume(.04f));scene.select(scene.objects().front().id);auto other_owner=scene.object(other).volume;
  auto sid=scene.active().id;auto saved_name=scene.active().name;
  check(procedural_sculpt_object(scene,sid,options)&&scene.active().name==saved_name&&scene.object(other).volume==other_owner,"Scene publication preserves object identity, properties and every unrelated object");
  scene.active().locked=true;check(refuses([&]{procedural_sculpt_object(scene,sid,options);}),"Locked objects refuse procedural edits");
  check(archive(source)==original,"All failures retain the immutable source archive");
  std::cout<<checks<<" procedural sculpt checks passed; cancellation boundaries="<<callbacks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
