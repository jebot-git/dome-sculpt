// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/material-visibility.hpp"
#include "texture-set-stroke.hpp"
#include "texture-set-controls.hpp"
#include "texture-set-paint-access.hpp"
#include <iostream>
#include <bit>
using namespace dome;
namespace {
size_t checks=0;void check(bool yes,const char*message){if(!yes)throw std::runtime_error(message);++checks;}
template<class F>void refuses(F&&f,const char*message){bool refused=false;try{f();}catch(const std::exception&e){refused=true;std::cout<<"EXPECTED "<<e.what()<<'\n';}check(refused,message);}
Scene fixture(){
 Mesh m;m.positions={{-.8f,-.8f,0},{.8f,-.8f,0},{0,.8f,0},{-.8f,-.8f,.5f},{.8f,-.8f,.5f},{0,.8f,.5f}};m.normals.assign(6,{0,0,1});m.uv={{{.1f,.1f}},{{.9f,.1f}},{{.5f,.9f}},{{.1f,.1f}},{{.9f,.1f}},{{.5f,.9f}}};m.indices={0,1,2,3,4,5};
 Scene s;std::array<TextureSetInitialField,2>fields;fields[0].resolution=64;fields[1].resolution=128;fields[0].material.name="Back";fields[1].material.name="Front";for(auto&f:fields)f.material.double_sided=true;
 initialize_texture_sets(s,s.active().id,m,fields,std::array<TextureSetId,2>{0,1});select_texture_set(s,s.active().id,1);return s;
}
void lease(){
 auto scene=fixture();const auto exact=scene.encode();const auto primary=scene.active().painted;const auto collection=scene.active().texture_sets;const auto extra=collection->at(1).painted;
 const auto lease=detail::capture_texture_set_stroke(scene,scene.active().id);lease->authenticate_owner(lease);lease->authenticate_scene(scene);
 auto draft=extra->snapshot();PaintSettings settings;settings.front_only=false;settings.color={1,0,0,1};Brush b;b.from=b.to={0,0,.5f};b.radius=1;b.normal={0,0,1};draft->begin_stroke();draft->stamp(b,settings);
 auto preview=scene.snapshot();const auto preview_primary=preview.active().painted;detail::adopt_texture_set_stroke(preview,lease,*draft);check(preview.encode()!=exact,"Actual in-progress COW paint can prepare a sealed selected-field preview");
 check(scene.encode()==exact&&scene.active().texture_sets==collection&&scene.active().painted==primary,"Preview adoption never mutates the initial Scene/collection/primary paint");
 check(preview.active().painted==preview_primary&&preview.active().texture_sets->at(1).painted!=extra,"Preview replaces only the selected extra field");
 check(preview.active().painted->mesh_owner()==preview.active().texture_sets->at(1).painted->mesh_owner(),"Independent resolution and paint retain one actual current geometry owner");
 draft->cancel_stroke();check(scene.encode()==exact,"Cancelling a private draft keeps all original pixels and memberships exact");
 auto changed=scene.snapshot();select_texture_set(changed,changed.active().id,0);refuses([&]{lease->authenticate_scene(changed);},"An old active-set lease cannot publish into a changed membership selection");
 changed=scene.snapshot();changed.active().material.color[0]=std::bit_cast<float>(std::bit_cast<uint32_t>(changed.active().material.color[0])^1u);refuses([&]{lease->authenticate_scene(changed);},"A one-bit sibling material mutation invalidates the actual recipient");
 changed=scene.snapshot();changed.active().name+=" changed";refuses([&]{lease->authenticate_scene(changed);},"Object metadata is part of recipient authority");
 const auto forged=std::shared_ptr<const detail::TextureSetStrokeLease>(lease.get(),[](auto*){});refuses([&]{lease->authenticate_owner(forged);},"The same lease address with another control block cannot grant authority");
 auto bad=extra->snapshot();bad->add_layer("Foreign layer");refuses([&]{detail::adopt_texture_set_stroke(scene,lease,*bad);},"A paint draft cannot smuggle layer-structure edits into stroke publication");check(scene.encode()==exact,"All forged/stale lease failures retain complete Scene values");
 VrControlState c;c.parameter=VrParameter::BaseColor;c.value={.2f,.4f,.6f,.8f};const auto edit=detail::texture_set_control_request(scene.active(),c);check(edit.set==1&&edit.material.color==c.value&&scene.encode()==exact,"Shared scalar controls stage the actual selected material without changing either field");
}
void ready(Document&doc){check(doc.wait_mesh(std::chrono::seconds(30)),"Actual current mesh worker reaches its ready fence");check(doc.frame().state->error.empty(),"Actual producer publishes no ownership error");}
void typed(){
 auto scene=fixture();const auto exact=scene.encode();Document doc;doc.replace(scene);ready(doc);
 doc.texture_set_action(SpatialCommand::TexturePickFaces);check(doc.frame().state->texture_face_select,"Shared native action enables material-face picking");
 check(doc.texture_face_pointer({0,0,2},{0,0,-1}),"Actual material-visible ray can select a foreground set face");check(doc.frame().state->texture_faces==std::vector<uint64_t>{2},"Depth-separated same-UV face retains its genuine logical face identity");
 doc.texture_set_action(SpatialCommand::TexturePickFaces);check(!doc.frame().state->texture_face_select&&doc.copy_scene().encode()==exact,"Leaving face picking changes no paint or persistent history");
 const auto before=doc.copy_scene();const auto primary=before.active().painted;const auto original=before.active().texture_sets;
 TextureSetEdit edit;edit.action=TextureSetAction::AddLayer;edit.set=1;edit.layer=active_texture_set(before.active()).painted->active_layer();doc.edit_texture_set(edit);check(doc.wait_asset(std::chrono::seconds(30)),"Typed selected-field worker terminates");ready(doc);
 const auto after=doc.copy_scene();check(after.active().texture_sets->at(1).painted->layers().size()==2&&after.active().painted->layers().size()==1,"Actual worker adds an independent layer only in its requested material set");check(after.active().painted->mesh_owner()==primary->mesh_owner()&&before.active().texture_sets==original&&before.encode()==exact,"Actual worker keeps recipient geometry and captured prior history exact");
 check(doc.undo()&&doc.copy_scene().encode()==exact,"One actual Undo removes the whole accepted field operation");check(doc.redo()&&doc.copy_scene().encode()==after.encode(),"One actual Redo restores its full field/membership/material endpoint");
 doc.save("sets-layer.dsculpt");check(Scene::load("sets-layer.dsculpt").encode()==after.encode(),"Native Save/reopen retains independent layers, resolutions and active set");
 edit={};edit.action=TextureSetAction::Material;edit.set=1;edit.material=active_texture_set(after.active()).material;edit.material.roughness=.125f;const auto state=doc.frame().state;const AssetSource stale{state->generation,state->revision,state->active_object,state->edit_authority};
 doc.edit_texture_set(edit);check(doc.wait_asset(std::chrono::seconds(30)),"Actual material-only worker terminates");const auto material_scene=doc.copy_scene();check(material_scene.active().material.roughness==scene.active().material.roughness&&material_scene.active().texture_sets->at(1).material.roughness==.125f,"A selected-set material request cannot leak into the primary material");
 refuses([&]{doc.edit_texture_set(edit,stale);},"A stale captured native editor request refuses before changing the later endpoint");check(doc.copy_scene().encode()==material_scene.encode(),"Stale editor refusal keeps the complete accepted endpoint");
}
void stroke(){
 auto scene=fixture();Document doc;doc.replace(scene);ready(doc);const auto exact=doc.copy_scene().encode();const auto primary=doc.copy_scene().active().painted;
 PaintSettings settings;settings.front_only=false;settings.color={.8f,.1f,.2f,1};doc.paint_settings(settings);doc.configure(Tool::Clay,.4f);
 NativeStrokeSample sample;sample.origin={0,0,2};sample.direction={0,0,-1};sample.brush.radius=.4f;sample.brush.pressure=1;sample.brush.strength=1;
 check(doc.begin_native_stroke()&&doc.sample_native_stroke(sample),"Actual desktop native ray starts with a genuine selected-field lease");doc.end_native_stroke(false);check(doc.wait_native_stroke(std::chrono::seconds(30)),"Actual cancelled selected-field native worker terminates");check(doc.copy_scene().encode()==exact&&!doc.frame().state->can_undo,"Escape-style Cancel preserves complete fields and adds no history");ready(doc);
 check(doc.begin_native_stroke()&&doc.sample_native_stroke(sample),"Actual selected-field stroke retries after cancellation");sample.origin.x=.1f;check(doc.sample_native_stroke(sample),"A second actual sample uses the same immutable visibility/membership basis");doc.end_native_stroke(true);
 auto save=doc.save_async("sets-stroke.dsculpt");check(doc.wait_native_stroke(std::chrono::seconds(30)),"Actual accepted multi-sample native worker terminates");save.get();const auto accepted=doc.copy_scene();check(accepted.encode()!=exact,"Actual native painting changes the target field");check(detail::TextureSetPaintAccess::same_current(*accepted.active().painted,*primary),"Other set's COW values, layers, paint and source owners stay exact");check(Scene::load("sets-stroke.dsculpt").encode()==accepted.encode(),"Ordered real native Save waits for the full accepted selected-field endpoint");
 check(doc.undo()&&doc.copy_scene().encode()==exact,"One native Undo removes the complete multi-sample field stroke");check(doc.redo()&&doc.copy_scene().encode()==accepted.encode(),"One native Redo restores all selected-field channels exactly");
}
}
int main(int argc,char**argv)try{if(argc>1){std::filesystem::create_directories(argv[1]);std::filesystem::current_path(argv[1]);}lease();typed();stroke();std::cout<<"PASS "<<checks<<" selected texture-field native controls\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
