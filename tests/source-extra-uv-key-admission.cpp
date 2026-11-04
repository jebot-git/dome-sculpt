// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/material-bindings.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/shapekey-authoring.hpp"
#include "dome/import-authoring.hpp"
#include "dome/source-maps.hpp"
#include "import-authoring-fixtures.hpp"
#include <iostream>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool value,const char *why){if(!value)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
std::array<Vec3,3> basis(const std::vector<Vec3>&p,const Mesh&m,const std::vector<std::array<float,2>>&uv){
 auto a=m.indices[0],b=m.indices[1],c=m.indices[2];auto e1=p[b]-p[a],e2=p[c]-p[a];auto q0=uv[a],q1=uv[b],q2=uv[c];float u1=q1[0]-q0[0],v1=q1[1]-q0[1],u2=q2[0]-q0[0],v2=q2[1]-q0[1],det=u1*v2-u2*v1;
 auto n=normalized(cross(e1,e2)),t=normalized((e1*v2-e2*v1)/det);auto raw_b=(e2*u1-e1*u2)/det;auto b_axis=cross(n,t)*(dot(cross(n,t),raw_b)<0?-1.f:1.f);return {t,b_axis,n};
}
Scene source(const std::filesystem::path&file,bool atlas=false){ImportAuthoringOptions o;o.paint_resolution=128;o.prepare_texture_atlas=atlas;return prepare_authoring_import(Scene{},file,o).scene;}
void ready(Document&d){if(!d.wait_native_stroke()||!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error("Key admission endpoint: "+d.frame().state->error);}

}
int main(int argc,char**argv)try{
 if(argc<2)return 2;const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);import_fixture::Fixture f;auto &p=f.j["meshes"][0]["primitives"][0];p["attributes"].erase("TANGENT");p["targets"][0].erase("TANGENT");f.j["accessors"][p["indices"].get<int>()]["count"]=3;
 while(f.bin.size()%4)f.bin.push_back(0);auto uv=f.values({0,0,0,1,1,1,1,0},2,"VEC2");p["attributes"]["TEXCOORD_2"]=uv;f.j["materials"][0]["normalTexture"]={{"index",0},{"texCoord",2}};f.j["materials"][0].erase("extensions");f.j["materials"][0]["alphaMode"]="OPAQUE";f.j.erase("extensionsUsed");auto file=root/"generated-uv2.glb";f.save(file);
 auto initial=source(file);const auto id=initial.active().id;const auto &object=initial.active();const auto &rest=object.painted->mesh();const auto frame=object.material_bindings->normal_frames->pbr;
 check(frame&&frame->generated_uv&&*frame->generated_uv==2&&object.painted->source_maps()->normal.texcoord==2,"Actual raw native import retains a generated UV2 tangent frame and its distinct normal role");
 const auto before=initial.encode();ShapeKeyEdit edit;edit.action=ShapeKeyAction::Replace;edit.target=0;std::vector<Vec3> old=rest.positions;for(size_t v=0;v<old.size();++v)old[v]+=object.deformation->morphs->targets[0].delta[v];edit.positions.reserve(old.size());for(auto p:old)edit.positions.push_back({1.8f*p.x+.7f*p.y,.3f*p.x+.9f*p.y,.2f*p.x+.4f*p.y});
 auto check_attributes=[&](const Scene &scene,const std::shared_ptr<const NormalFrames>&base,
     const std::vector<std::array<float,2>>&uv,const std::shared_ptr<const NormalFrames>&now){
  const auto &previous=base->targets[0],&actual=now->targets[0];
  auto a=basis(old,rest,uv),b=basis(edit.positions,rest,uv);float ne=0,te=0;
  for(size_t c=0;c<base->normals.size();++c){auto rotate=[&](Vec3 v){return b[0]*dot(v,a[0])+b[1]*dot(v,a[1])+b[2]*dot(v,a[2]);};
   auto n=base->normals[c]+previous.normals[c];auto t=base->tangents[c];Vec3 tangent{t[0],t[1],t[2]};auto endpoint=tangent+(previous.tangents.empty()?Vec3{}:previous.tangents[c]);
   ne=std::max(ne,length(actual.normals[c]-(rotate(n)-base->normals[c])));te=std::max(te,length(actual.tangents[c]-(rotate(endpoint)-tangent)));
  }
  std::cout<<"ROLE_UV_JACOBIAN normal_error="<<ne<<" tangent_error="<<te<<std::endl;
  check(ne<2e-6f&&te<2e-6f,"Independent UV Jacobian proves reauthored artist N/T endpoint transport");
  check(now->encode().at("normals")==base->encode().at("normals")&&now->tangents==base->tangents&&now->generated_uv==base->generated_uv&&now->source_vertices==base->source_vertices&&now->source_corners==base->source_corners,
    "New target attributes preserve original base/provenance/source-corner identities");
 };
 auto direct=initial;author_shapekey(direct,id,edit);check_attributes(direct,frame,*object.painted->source_maps()->extra_uv_sets.at(2),direct.active().material_bindings->normal_frames->pbr);
 check(direct.active().shape_source&&direct.active().shape_source->material_bindings==object.material_bindings&&direct.active().deformation->morph_weights==object.deformation->morph_weights,
   "Geometry-key authoring retains the exact original source capsule and saved mixed weights");
 auto saved=root/"authored-uv2.dsculpt";direct.save(saved);check(Scene::load(saved).encode()==direct.encode(),"New UV2 key and its complete source survive native Save/reopen");
 ShapeKeyEdit restore;restore.action=ShapeKeyAction::Restore;author_shapekey(direct,id,restore);check(direct.encode()==before,"Restore returns the complete original raw/frame/paint/deformation graph exactly");
 unsigned cancelled=0;auto cancelled_scene=initial;try{author_shapekey(cancelled_scene,id,edit,[&]{return ++cancelled==8;});throw std::runtime_error("Cancelled key unexpectedly committed");}catch(const std::exception&e){check(cancelled_scene.encode()==before&&cancelled>=8,"Cancellation during coordinate/frame capture leaves exact source and history endpoint");}
 Document d;d.replace(initial);ready(d);for(int view=0;view<3;++view){if(view==0)d.authored_shading();else d.shading(view==2);ready(d);d.author_shape(edit);ready(d);
  check_attributes(d.copy_scene(),frame,*object.painted->source_maps()->extra_uv_sets.at(2),d.copy_scene().active().material_bindings->normal_frames->pbr);
  check(d.undo()&&d.copy_scene().encode()==before&&d.redo(),"Native authored/CorePBR/Toon key action has exact one Undo/Redo");ready(d);
  auto endpoint=d.copy_scene().encode();d.author_shape(restore);ready(d);check(d.copy_scene().encode()==before&&d.undo()&&d.copy_scene().encode()==endpoint,"Native Restore and its Undo retain complete edited and original UV2 identities");ready(d);d.replace(initial);ready(d);
 }
 d.configure_shape_sculpt(true);ready(d);check(d.frame().state->shape_sculpt,"Native/controller sculpt mode admits the coordinate-aware UV2 key factory");
 auto local_hit=old[0]*.8f+old[1]*.1f+old[2]*.1f;auto world=initial.world_matrix(id).point({local_hit.x,local_hit.y,local_hit.z});Vec3 hit{float(world[0]),float(world[1]),float(world[2])};
 NativeStrokeSample sample;sample.origin=hit+Vec3{0,0,1};sample.direction={0,0,-1};sample.brush.radius=.16f;sample.brush.tool=Tool::Clay;
 check(d.begin_native_stroke()&&d.sample_native_stroke(sample),"Actual native UV2 target stroke enters the ordered worker");d.end_native_stroke();auto stroke_file=root/"native-stroke.dsculpt";d.save_async(stroke_file).get();ready(d);auto stroked=d.copy_scene().encode();
 check(stroked!=before&&Scene::load(stroke_file).encode()==stroked&&d.copy_scene().active().painted->encode()==initial.active().painted->encode(),
   "Accepted UV2 geometry stroke changes the key, retains paint and participates in ordered Save/reopen");
 check(d.undo()&&d.copy_scene().encode()==before&&d.redo()&&d.copy_scene().encode()==stroked,"Native UV2 target stroke has one exact Undo/Redo");ready(d);
 check(d.begin_native_stroke()&&d.sample_native_stroke(sample),"A second UV2 target gesture starts");d.end_native_stroke(false);ready(d);check(d.copy_scene().encode()==stroked,"Cancelled native target gesture retains complete UV2 attributes/source/pose/paint");
 d.configure_shape_sculpt(false);ready(d);
 Document xr;xr.replace(initial);ready(xr);xr.configure_shape_sculpt(true);ready(xr);SpatialFrame input;input.focused=input.head_valid=true;input.head.position={0,1.6f,0};auto &hand=input.hands[1].tracked;hand.valid=hand.aimValid=true;hand.grip.position=hand.aim.position=ob::transform(xr.frame().state->canvas,{sample.origin.x,sample.origin.y,sample.origin.z});hand.grip.orientation=hand.aim.orientation=xr.frame().state->canvas.orientation;
 auto send=[&](float trigger){input.sequence++;input.time+=.05;hand.trigger=trigger;xr.submit(input);if(!xr.wait_input())throw std::runtime_error("UV2 controller input timeout");};
 send(0);hand.grip.position=hand.aim.position=ob::transform(xr.frame().state->canvas,{sample.origin.x,sample.origin.y,sample.origin.z});hand.grip.orientation=hand.aim.orientation=xr.frame().state->canvas.orientation;send(0);send(.8f);send(0);ready(xr);auto xr_endpoint=xr.copy_scene().encode();check(xr_endpoint!=before&&xr.copy_scene().active().painted->encode()==initial.active().painted->encode(),"Actual controller ray/pressure durably authors the UV2 key while retaining original paint");
 send(.8f);input.focused=false;send(.8f);ready(xr);check(xr.copy_scene().encode()==xr_endpoint,"Controller focus loss cancels an unaccepted UV2 target gesture exactly");

 // Provided artist tangents may alias at capture even when latent material
 // roles select different coordinate bases. Reauthor each role independently.
 import_fixture::Fixture supplied;auto &sp=supplied.j["meshes"][0]["primitives"][0];supplied.j["accessors"][sp["indices"].get<int>()]["count"]=3;
 while(supplied.bin.size()%4)supplied.bin.push_back(0);sp["attributes"]["TEXCOORD_2"]=supplied.values({0,0,0,1,1,1,1,0},2,"VEC2");
 supplied.j["materials"][0]["normalTexture"]={{"index",0},{"texCoord",2}};supplied.j["materials"][0].erase("extensions");supplied.j.erase("extensionsUsed");auto supplied_file=root/"artist-uv2.glb";supplied.save(supplied_file);auto dual=source(supplied_file);
 auto binding=std::make_shared<SourceMaterialBindings>(*dual.active().material_bindings);auto origin=std::make_shared<SourceUvOrigin>(*binding->uv_origin);
 binding->has_mtoon=true;binding->material=MToonMaterial::from_vrm0({{"shader","VRM/MToon"},{"textureProperties",{{"_BumpMap",0}}}});
 binding->roles[size_t(MToonSlot::Normal)]=dual.active().painted->source_maps()->normal;binding->roles[size_t(MToonSlot::Normal)].texcoord=0;
 origin->material=binding->material;origin->roles=binding->roles;binding->uv_origin=origin;auto fs=std::make_shared<NormalFrameSet>(*binding->normal_frames);fs->mtoon=fs->pbr;binding->normal_frames=fs;dual.active().material_bindings=binding;dual.validate();
 auto dual_before=dual.encode();const auto artist=fs->pbr;check(!artist->generated_tangents&&fs->pbr==fs->mtoon,"Actual supplied artist tangents begin with aliased captured PBR/MToon frames");
 author_shapekey(dual,dual.active().id,edit);auto authored=dual.active().material_bindings->normal_frames;
 check(authored->pbr!=authored->mtoon,"Geometric authoring separates aliased source frames when latent roles use distinct UV bases");
 check_attributes(dual,artist,*dual.active().painted->source_maps()->extra_uv_sets.at(2),authored->pbr);
 check_attributes(dual,artist,rest.uv,authored->mtoon);
 ShapeKeyEdit dual_restore;dual_restore.action=ShapeKeyAction::Restore;author_shapekey(dual,dual.active().id,dual_restore);check(dual.encode()==dual_before,"Dual-role authoring restores the exact original artist-frame alias/source graph");
 for(auto action:{ShapeKeyAction::Duplicate,ShapeKeyAction::CaptureMix,ShapeKeyAction::Create}){auto scene=initial;ShapeKeyEdit supported;supported.action=action;supported.target=0;supported.name="Exact retained attributes";author_shapekey(scene,id,supported);check(scene.active().material_bindings->normal_frames->pbr->generated_uv==frame->generated_uv,"Non-geometric key action retains exact original UV2 base tangent provenance");ShapeKeyEdit restore;restore.action=ShapeKeyAction::Restore;author_shapekey(scene,id,restore);check(scene.encode()==before,"Non-geometric raw key action restores the complete original source exactly");}
 auto atlas=source(file,true);const auto atlas_before=atlas.encode();const auto &prepared=atlas.active();const auto paint=prepared.painted;const auto maps=paint->source_maps();const auto original=paint->source_sampling_original();const auto atlas_origin=prepared.material_bindings->uv_origin;
 check(paint->source_sampling_prepared()&&paint->valid_source_sampling_attachment()&&paint->surface_diagnostics().policy==PaintUvPolicy::Strict&&!prepared.material_bindings->sampling_reparameterized&&maps&&maps->normal.texcoord==2&&maps->extra_uv_sets.contains(2)&&original&&atlas_origin&&atlas_origin->paint&&original->encode()==initial.active().painted->encode()&&atlas_origin->paint->encode()==original->encode(),"Explicit atlas owns a strict independent writable grid and authentic exact original UV2 paint instead of flattening the normal role");
 const auto vertices=paint->source_sampling_vertices();check(vertices.size()==paint->mesh().positions.size(),"Source4 writable vertices carry a complete immutable original-vertex remap");
 for(size_t v=0;v<vertices.size();++v){check(vertices[v]<original->mesh().positions.size()&&length(paint->mesh().positions[v]-original->mesh().positions[vertices[v]])==0&&maps->extra_uv_sets.at(2)->at(v)==original->source_maps()->extra_uv_sets.at(2)->at(vertices[v]),"Every current geometry/UV2 entry resolves to its genuine original source vertex");}
 check(maps->normal.image==original->source_maps()->normal.image&&maps->normal_scale==original->source_maps()->normal_scale&&maps->normal.uv_transform==original->source_maps()->normal.uv_transform,"Atlas leaves actual normal image, strength and affine sampling intent unchanged");
 auto positive=edit;positive.positions.clear();const auto &mesh=paint->mesh();for(size_t v=0;v<mesh.positions.size();++v){const auto p=mesh.positions[v]+prepared.deformation->morphs->targets[0].delta[v];positive.positions.push_back({1.8f*p.x+.7f*p.y,.3f*p.x+.9f*p.y,.2f*p.x+.4f*p.y+.01f});}
 Document atlas_doc;atlas_doc.replace(atlas);ready(atlas_doc);atlas_doc.author_shape(positive);auto atlas_file=root/"atlas-key.dsculpt";atlas_doc.save_async(atlas_file).get();ready(atlas_doc);const auto keyed=atlas_doc.copy_scene();check(keyed.active().shape_source&&keyed.active().painted->encode()==paint->encode()&&keyed.active().appearance==prepared.appearance&&keyed.active().material_bindings->uv_origin==atlas_origin,"Actual native Source4 Replace preserves all paint/source owners while authoring a geometric UV2 key");
 check_attributes(keyed,prepared.material_bindings->normal_frames->pbr,*object.painted->source_maps()->extra_uv_sets.at(2),keyed.active().material_bindings->normal_frames->pbr);
 check(Scene::load(atlas_file).encode()==keyed.encode()&&atlas_doc.undo()&&atlas_doc.copy_scene().encode()==atlas_before&&atlas_doc.redo()&&atlas_doc.copy_scene().encode()==keyed.encode(),"Accepted Source4 key has exact ordered Save/restart and one native Undo/Redo");ready(atlas_doc);
 atlas_doc.author_shape(restore);ready(atlas_doc);check(atlas_doc.copy_scene().encode()==atlas_before,"Native Source4 key Restore returns every writable/original UV, normal, paint and deformation owner exactly");

 std::cout<<"PASS "<<checks<<" source UV2 key frame checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
