// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "texture-set-field-source.hpp"
#include "texture-set-paint-access.hpp"
#include "polygon-surface-owner.hpp"
#include "polygon-surface-preview.hpp"
#include "polygon-surface-paint.hpp"
#include "texture-set-worker.hpp"
#include "polygon-surface-range.hpp"
#include "dome/material-visibility.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool yes,const char*s){if(!yes)throw std::runtime_error(s);++checks;}
template<class F>void refuses(F&&f,const char*s){bool caught=false;try{f();}catch(const std::exception&e){caught=true;std::cout<<"EXPECTED "<<e.what()<<'\n';}check(caught,s);}
void settled(Document&d){check(d.wait_asset(std::chrono::seconds(120))&&d.wait_mesh(std::chrono::seconds(120)),"Actual workers reach their endpoint");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
void polygon(Document&d){check(d.wait_polygon(std::chrono::seconds(120)),"Actual polygon queue reaches its endpoint");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
Scene source(){
 Mesh m;m.positions={{-.9f,-.5f,0},{-.1f,-.5f,0},{-.5f,.5f,0},{.1f,-.5f,0},{.9f,-.5f,0},{.5f,.5f,0}};m.normals.assign(6,{0,0,1});m.uv={{{.05f,.1f}},{{.45f,.1f}},{{.25f,.9f}},{{.55f,.1f}},{{.95f,.1f}},{{.75f,.9f}}};m.indices={0,1,2,3,4,5};Scene s;s.active().mesh_active=true;s.active().material.double_sided=true;s.active().painted=std::make_shared<PaintedMesh>(m,64);
 PaintSettings ink;ink.front_only=false;ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={12,.002f,3};Brush brush;brush.from=brush.to={0,0,0};brush.normal={0,0,1};brush.radius=2;brush.strength=1;s.active().painted->begin_stroke();s.active().painted->stamp(brush,ink);check(s.active().painted->end_stroke()&&s.active().painted->has_physical_emission(),"Actual native source contains genuine FLOAT planes");
 Document d;d.replace(s);settled(d);d.begin_polygon(true);polygon(d);d.edit_polygon(PolygonAction::SelectAll);polygon(d);d.edit_polygon(PolygonAction::FlipFaces);polygon(d);d.end_polygon(true);settled(d);const auto result=d.copy_scene();check(result.active().material_bindings&&result.active().material_bindings->polygon_surface_transfer&&result.encode().at("version")==39,"Real native Flip produces the genuine current39 owner");return result;
}
void run(){
 const auto scene=source();const auto exact=scene.encode();const auto&actor=scene.active();const auto owner=actor.material_bindings->polygon_surface_transfer;
 auto fields=detail::prepare_texture_set_field_sources(actor,std::array<TextureSetId,2>{0,1});check(fields.size()==2,"A detached actual39 source partitions into two real material fields");
 for(size_t i=0;i<fields.size();++i){
  const auto&f=fields[i];f->authenticate_owner(f);const auto&view=f->field();check(view.id==i&&view.domain->set()==i&&view.domain->triangles().size()==1,"Actual set and logical triangle membership remain explicit");check(view.painted->mesh_owner()==actor.painted->mesh_owner()&&f->original_paint()->mesh_owner()==actor.painted->mesh_owner(),"Every source/member shares the real geometry owner without a copied Mesh");
  check(!owner->matches_current(*view.painted),"An independently proved subset raster never impersonates the original39 attachment");const auto readable=f->read_view(view);check(owner->matches_current(*readable)&&readable->paint_sampling_attachment()==actor.painted->paint_sampling_attachment(),"Typed read view restores the actual original39 raster/attachment authority");check(readable->has_physical_emission()&&view.painted->has_physical_emission(),"Original FLOAT values remain in both editable subset and source interpretation");
  const auto triangle=view.domain->triangles().front();const auto sample=f->sample(triangle,{.2,.3,.5});const auto native_bary=f->sample(triangle,{double(.2f),double(.3f),double(.5f)});check(native_bary.face==sample.face,"Actual float SurfaceHit barycentrics retain their source triangle identity");check(sample.face==view.domain->topology()->triangle_faces()[triangle]&&sample.paint_uv==owner->paint_coordinates(triangle,{.2,.3,.5})&&!sample.normal_uv,"True face/corner/barycentric paint identity stays distinct from absent N-only normal coordinates");
  const auto maps=detail::PolygonSurfacePaintCodec::preview(*readable,view.material_bindings,view.material);check(maps->polygon_surface&&maps->polygon_surface->matches(view.material_bindings,readable->paint_sampling_attachment()),"Actual existing39 packet producer accepts the authenticated read field");
  const auto world=AffineTransform(std::array<double,16>{-1.2,.1,0,0,.3,.8,0,0,0,.15,1.1,0,.02,0,.1,1});const auto actual=f->normal(triangle,{.2,.3,.5},world,false);const auto expected=owner->native_normal(triangle,{.2,.3,.5},world);check(length(actual-expected)<1e-6f,"Field normal keeps original raw N and exact reflected/sheared world interpretation");check(length(f->normal(triangle,{.2,.3,.5},world,true)-expected)<1e-6f,"PBR shading of an N-only source uses its actual native N without inventing a prepared role");
  refuses([&]{f->sample(uint32_t(1-triangle),{.2,.3,.5});},"A foreign logical triangle cannot borrow another material field");refuses([&]{f->sample(triangle,{.2,.3,.6});},"Malformed barycentrics cannot mint a field sample");
 }
 auto partitioned=scene.snapshot();const auto face=fields[1]->field().domain->topology()->triangle_faces()[1];
 const auto set=create_texture_set(partitioned,actor.id,std::array<uint64_t,1>{face},"Actual source material",64);const auto&split=partitioned.active();
 check(set==1&&split.texture_sets&&split.texture_sets->primary_source()&&split.texture_sets->additional()[0].source,"Real collection creation retains both source field descriptors");
 ObjectSurface bundle;bundle.id=split.id;bundle.name=split.name;bundle.material=split.material;bundle.transform=split.transform;bundle.groups=partitioned.group_path(split.group);bundle.affine_edit=split.affine_edit;bundle.texture_set_surface=detail::prepare_texture_set_surface(split);
 const auto layout=detail::prepare_texture_set_packed(bundle.texture_set_surface);check(layout->has_source()&&layout->posed()==actor.painted->mesh_owner(),"One actual REST geometry feeds all retained field ranges");
 MeshSnapshot snapshot;detail::append_texture_set_surface(snapshot,bundle,layout,{});check(snapshot.ranges.size()==2&&snapshot.source_normals.size()==snapshot.mesh.positions.size()&&snapshot.paint_coordinates.size()==snapshot.mesh.positions.size(),"A first retained-set object publishes complete before-N and paint streams from an empty prefix");
 for(const auto&r:snapshot.ranges){
  r.texture_set_proof->authenticate(r);detail::validate_polygon_range(snapshot.mesh,r,snapshot.paint_coordinates,true,snapshot.source_normals,snapshot.source_tangents,snapshot.normal_coordinates,true);
  check(r.count==3&&r.source_frame&&r.source_normal_only&&r.textures->polygon_surface==r.texture_set_proof->field().source->source_preview(),"Each genuine range retains its shared raw-input packet and actual subset");
  for(size_t c=0;c<r.count;++c){const auto logical=r.texture_set_proof->logical_corner(c,r.paint_winding_reversed);const auto v=snapshot.mesh.indices[r.first+c];check(snapshot.paint_coordinates[v]==r.textures->polygon_surface->paint_coordinates()[logical],"Packed material range uses actual original paint logical corners");}
 }
 MaterialSurfaceQuery query(snapshot.mesh,snapshot.ranges,{},snapshot.source_colors,PaintVertexCoordinates{snapshot.paint_coordinates});
 for(uint32_t t=0;t<2;++t){check(query.material_identity(t).texture_set==snapshot.ranges[t].texture_set&&query.logical_triangle(t)==snapshot.ranges[t].texture_set_proof->logical_corner(0,false)/3,"Actual material query maps subset draws back to shared logical triangles");}
 auto corrupted=snapshot.paint_coordinates;corrupted[snapshot.mesh.indices[0]][0]+=.0625f;refuses([&]{MaterialSurfaceQuery wrong(snapshot.mesh,snapshot.ranges,{},snapshot.source_colors,PaintVertexCoordinates{corrupted});},"Independent paint stream corruption cannot pass actual source range validation");
 auto borrowedRange=snapshot.ranges[0];borrowedRange.material_bindings=snapshot.ranges[1].material_bindings?std::shared_ptr<const SourceMaterialBindings>(snapshot.ranges[1].material_bindings.get(),[](auto*){}):nullptr;refuses([&]{borrowedRange.texture_set_proof->authenticate(borrowedRange);},"Retained range cannot borrow a binding address from a foreign control block");
 auto changed=fields[1]->field();auto draft=changed.painted->snapshot();PaintSettings ink;ink.front_only=false;ink.color={.1f,.8f,.2f,1};Brush brush;brush.from=brush.to={.5f,0,0};brush.normal={0,0,1};brush.radius=1;brush.strength=1;draft->begin_stroke();draft->stamp(brush,ink);check(draft->end_stroke(),"Actual detached member field receives a genuine paint edit");changed.painted=draft->snapshot();auto updated=fields[1]->with_current(changed);updated->validate_field(updated->field());const auto readable=updated->read_view(updated->field());check(owner->matches_current(*readable)&&readable->has_physical_emission(),"New member pixels preserve independent FLOAT/original normal/source ownership");
 auto forged=updated->field();forged.domain=fields[0]->field().domain;refuses([&]{updated->validate_field(forged);},"Another actual set domain cannot authorize the current member");forged=updated->field();forged.material_bindings=std::shared_ptr<const SourceMaterialBindings>(changed.material_bindings.get(),[](auto*){});refuses([&]{updated->validate_field(forged);},"The same binding address with another control block cannot replace field provenance");
 const auto borrowed=std::shared_ptr<const detail::TextureSetFieldSource>(fields[1].get(),[](auto*){});refuses([&]{fields[1]->authenticate_owner(borrowed);},"Field descriptor control blocks remain genuine owners");const auto domain=changed.domain;const auto alias=std::shared_ptr<const TextureSetFaceDomain>(domain.get(),[](auto*){});refuses([&]{domain->authenticate_owner(alias);},"Membership control blocks cannot be fabricated by address equality");
 TextureSetLimits tiny;tiny.maximum_bytes=1024;refuses([&]{detail::prepare_texture_set_field_sources(actor,std::array<TextureSetId,2>{0,1},tiny);},"Prospective source/raster/read metadata quota refuses before publication");TextureSetLimits cancel;cancel.cancelled=[]{return true;};refuses([&]{detail::prepare_texture_set_field_sources(actor,std::array<TextureSetId,2>{0,1},cancel);},"Actual field preparation is cancellable without changing source state");
 check(scene.encode()==exact&&actor.painted->mesh_owner()==fields[0]->field().painted->mesh_owner(),"Every positive/negative/current-field preparation preserves the complete original39 Scene and shared geometry");
}
}
int main()try{run();std::cout<<"PASS "<<checks<<" actual retained material-field source controls\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
