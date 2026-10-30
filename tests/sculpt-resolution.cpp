// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/sculpt-resolution.hpp"
#include "dome/source-maps.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <limits>
#include <unistd.h>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
template<class F>bool refuses(F call){try{call();return false;}catch(const std::exception &){return true;}}
nlohmann::json ordered(const Volume &v) {
 auto json=v.encode();std::sort(json["samples"].begin(),json["samples"].end());
 for(auto &l:json["sculpt_layers"])std::sort(l["delta"].begin(),l["delta"].end());
 if(json.contains("sculpt_mask"))std::sort(json["sculpt_mask"]["values"].begin(),json["sculpt_mask"]["values"].end());return json;
}
Volume sphere(float voxel=.04f,float radius=.237f,Vec3 center={.013f,.017f,-.011f}) {
 Volume v(voxel);v.begin_stroke();Brush b;b.radius=radius;b.blend=0;b.from=b.to=center;v.stamp(b);v.end_stroke();return v.geometry_snapshot();
}
void properties(const Volume &a,const Volume &b) {
 check(a.active_layer()==b.active_layer() && a.encode()["next_sculpt_layer"]==b.encode()["next_sculpt_layer"],"Active and future layer identities survive resampling");
 check(a.layers().size()==b.layers().size(),"Layer stack is not flattened");
 for(size_t i=0;i<a.layers().size();++i) {
  auto &x=a.layers()[i];auto &y=b.layers()[i];
  check(x.id==y.id && x.name==y.name && x.visible==y.visible && x.locked==y.locked && x.opacity==y.opacity,"Each layer retains exact identity, Unicode name, visibility, lock and opacity");
 }
 auto x=a.encode()["sculpt_controls"],y=b.encode()["sculpt_controls"];
 check(x==y && a.mask_default()==b.mask_default(),"Independent global mask and per-object controls remain exact");
}
}
int main() try {
 auto source=sphere();auto original=ordered(source);auto history=source.undo_count();auto owner=&source.voxels();
 SculptResolutionOptions options;options.voxel_size=source.voxel_size();
 auto no_op=resample_sculpt(source,options);
 check(!no_op.replacement && !no_op.report.changed && &source.voxels()==owner && source.undo_count()==history && ordered(source)==original,"Same voxel spacing is an exact no-op with original geometry/history ownership");
 options.voxel_size=.02f;auto fine=resample_sculpt(source,options);
 check(fine.replacement && fine.report.changed && fine.replacement->voxel_size()==.02f && fine.replacement->band()==source.band(),"Refinement retains the original physical metre band rather than shifting the layer baseline");
 check(fine.replacement->geometry_snapshot().band()==source.band(),"Immutable geometry snapshots retain the physical band");
 check(fine.report.source.closed_manifold() && fine.report.target.closed_manifold() && fine.report.source.components==fine.report.target.components && fine.report.source_euler==fine.report.target_euler,"Refinement preserves independently inspected closed topology");
 check(fine.report.maximum_surface_bound<=.02001f && fine.report.minimum_normal_dot>=.5f,"Bidirectional surface distance is certified and sampled normals meet explicit bounds");
 check(ordered(source)==original && &fine.replacement->voxels()!=owner,"Resolution result is detached and original geometry remains untouched");
 auto encoded=fine.replacement->encode();
 check(encoded["version"]==4 && encoded["distance_band"]==source.band(),"Changed physical band persists with strict independent Volume4 data");
 check(ordered(Volume::decode(encoded))==ordered(*fine.replacement),"Volume4 reload retains exact resampled field/layers/band");
 check(source.encode()["version"]==2 && !source.encode().contains("distance_band"),"Default legacy Volume2 encoding is unchanged");
 auto legacy3=source;auto settings=legacy3.sculpt_settings();settings.symmetry_axes=3;legacy3.sculpt_settings(settings);
 check(legacy3.encode()["version"]==3 && !legacy3.encode().contains("distance_band"),"Default-band mask/control Volume3 encoding is unchanged");
 for(auto version:{1,2,3}) {auto bad=encoded;bad["version"]=version;check(refuses([&]{Volume::decode(bad);}),"Legacy versions refuse a physical band instead of silently dropping it");}
 for(auto value:{0.f,-1.f,5.f,std::numeric_limits<float>::infinity()}) {auto bad=encoded;bad["distance_band"]=value;check(refuses([&]{Volume::decode(bad);}),"Invalid physical bands are rejected before field decoding");}
 auto bad=encoded;bad.erase("distance_band");check(refuses([&]{Volume::decode(bad);}),"Volume4 requires its physical band");
 bad=encoded;bad["unknown_resolution_data"]=1;check(refuses([&]{Volume::decode(bad);}),"Unknown Volume4 fields cannot silently alter resolution semantics");
 // Hidden/locked layers remain independent raw displacements; opacity does not
 // get baked into another layer merely to compensate the new voxel baseline.
 auto layered=source;auto second=layered.add_layer("Carve 日本語");layered.begin_stroke();Brush cut;cut.tool=Tool::Carve;cut.radius=.095f;cut.from=cut.to={.12f,.03f,.15f};layered.stamp(cut);layered.end_stroke();
 auto hidden=layered.add_layer("Hidden décor");layered.begin_stroke();Brush add;add.radius=.12f;add.from=add.to={.4f,0,0};layered.stamp(add);layered.end_stroke();
 layered.layer_properties(1,"Base ü",true,true,1);layered.layer_properties(second,"Carve 日本語",true,true,.8f);layered.layer_properties(hidden,"Hidden décor",false,true,.37f);
 layered.fill_mask(.37f);settings=layered.sculpt_settings();settings.mask_edit=true;settings.mask_tool=Volume::MaskTool::Subtract;settings.mask_strength=.6f;settings.symmetry_axes=5;settings.symmetry_origin={.013f,-.01f,.02f};layered.sculpt_settings(settings);
 layered.begin_stroke();Brush protect;protect.from=protect.to={.1f,0,.15f};protect.radius=.12f;layered.stamp(protect);layered.end_stroke();
 options.voxel_size=.02f;auto layer_result=resample_sculpt(layered,options);properties(layered,*layer_result.replacement);
 check(!layer_result.replacement->mask_values().empty(),"Sparse mask exceptions are resampled without flattening their global default");
 for(auto [q,value]:layered.mask_values()) {
  auto found=layer_result.replacement->mask_values().find({q.x*2,q.y*2,q.z*2});
  if(found==layer_result.replacement->mask_values().end() || found->second!=value)throw std::runtime_error("Aligned protection changed");
 }
 check(true,"Aligned mask lattice values retain exact protection");
 for(size_t i=0;i<layered.layers().size();++i)for(auto [q,value]:layered.layers()[i].delta) {
  auto found=layer_result.replacement->layers()[i].delta.find({q.x*2,q.y*2,q.z*2});
  if(value!=0 && (found==layer_result.replacement->layers()[i].delta.end() || found->second!=value))throw std::runtime_error("Aligned layer displacement changed physical units");
 }
 check(true,"Aligned hidden and visible layer displacements retain exact metre values independently");
 options.voxel_size=.06f;auto coarse=resample_sculpt(source,options);
 check(coarse.replacement && coarse.report.target.closed_manifold(),"Representable coarsening retains a closed surface with quality checks");
 auto thin=sphere(.02f,.027f,{.03f,.03f,.03f});auto thin_before=ordered(thin);options.voxel_size=.2f;
 check(refuses([&]{resample_sculpt(thin,options);}) && ordered(thin)==thin_before,"Coarsening that removes a thin closed feature is refused without mutation");
 options.voxel_size=.02f;options.maximum_work=128;
 check(refuses([&]{resample_sculpt(source,options);}) && ordered(source)==original,"Bounded work exhaustion retains the original field/layers");
 options={};options.voxel_size=.02f;options.maximum_samples=64;
 check(refuses([&]{resample_sculpt(source,options);}),"Insufficient displacement/mask storage is refused transactionally");
 options={};options.voxel_size=.02f;
 for(unsigned boundary:{1u,2u,16u,128u,1024u}) {
  unsigned calls=0;options.cancelled=[&]{return ++calls>=boundary;};
  check(refuses([&]{resample_sculpt(source,options);}) && ordered(source)==original,"Cancellation during preparation/quality preserves the exact original");
 }
 // All layer visibility/strength configurations retain their exact values
 // on shared original lattice nodes, without baking opacity into another layer.
 for(unsigned flags=0;flags<8;++flags) {
  auto old=layered.geometry_snapshot(),resampled=layer_result.replacement->geometry_snapshot();
  for(size_t i=0;i<old.layers().size();++i) {
   auto properties=old.layers()[i];float opacity=(flags&4)?.31f:properties.opacity;
   old.layer_properties(properties.id,properties.name,(flags&(1u<<i))!=0,properties.locked,opacity);
   resampled.layer_properties(properties.id,properties.name,(flags&(1u<<i))!=0,properties.locked,opacity);
  }
  bool aligned=true;
  for(auto &layer:old.layers())for(auto [q,value]:layer.delta) { (void)value;aligned&=old.at(q)==resampled.at({q.x*2,q.y*2,q.z*2}); }
  check(aligned,"Hidden/visible and changed-strength compositions retain exact common-node metre values");
 }
 // A hidden sparse layer and sparse protection cannot disappear merely because
 // a coarse grid happens to miss their support while the visible shape is empty.
 Volume sparse(.01f);sparse.add_layer("Hidden thin feature");sparse.begin_stroke();Brush mark;mark.radius=.012f;mark.from=mark.to={.055f,.055f,.055f};mark.blend=0;sparse.stamp(mark);sparse.end_stroke();
 sparse.layer_properties(sparse.active_layer(),"Hidden thin feature",false,true,1);
 options={};options.voxel_size=.2f;options.maximum_layer_error=1;
 check(refuses([&]{resample_sculpt(sparse,options);}),"A coarse grid cannot silently erase a hidden displacement layer");
 auto masked=Volume(.01f);settings=masked.sculpt_settings();settings.mask_edit=true;masked.sculpt_settings(settings);
 masked.begin_stroke();mark.radius=.012f;masked.stamp(mark);masked.end_stroke();
 options.maximum_mask_error=1;
 check(refuses([&]{resample_sculpt(masked,options);}),"A coarse grid cannot silently erase independent sparse protection");
 Scene scene;scene.active().volume=std::make_shared<Volume>(source.geometry_snapshot());
 auto group=scene.add_group("Retained group",{{1,2,3},{0,0,.70710678f,.70710678f},1.7f});scene.active().group=group;
 Mesh quad;quad.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};quad.normals.assign(4,{0,0,1});quad.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};quad.indices={0,1,2,0,2,3};
 auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=std::make_shared<const Image>(Image{2,1,{255,0,0,255,0,0,255,255}});
 scene.active().painted=PaintedMesh::from_maps(quad,maps,64);
 auto retained_paint=scene.active().painted;auto retained_images=scene.encode()["source_images"];
 auto other=scene.add("Other object",sphere());scene.select(1);auto other_field=scene.object(other).volume;
 auto before=scene.encode();options={};options.voxel_size=.02f;SculptResolutionReport report;
 check(resample_sculpt_object(scene,1,options,&report) && report.changed,"Detached Scene resolution operation publishes one fully validated replacement");
 auto after=scene.encode();auto old_object=before["objects"][0],new_object=after["objects"][0];old_object.erase("volume");new_object.erase("volume");
 check(old_object==new_object && after["groups"]==before["groups"] && scene.active().painted==retained_paint && scene.object(other).volume==other_field && after["source_images"]==retained_images,"Resolution preserves original paint/image archives, transforms, groups, identities and other objects exactly");
 auto path=std::filesystem::temp_directory_path()/("dome-resolution-"+std::to_string(getpid())+".dsculpt");scene.save(path);auto reopened=Scene::load(path);std::filesystem::remove(path);
 check(ordered(*reopened.active().volume)==ordered(*scene.active().volume) && reopened.active().painted->encode()==scene.active().painted->encode(),"Ordinary native Scene save/restart retains Volume4 and original source painting");
 auto before_failure=scene.active().volume;options.voxel_size=.01f;options.maximum_work=1;
 check(refuses([&]{resample_sculpt_object(scene,1,options);}) && scene.active().volume==before_failure,"Failed Scene resolution preserves the exact original Volume owner");
 std::cout<<"PASS "<<checks<<" sculpt resolution checks\n";return 0;
} catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
