// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creator-tool.hpp"
#include "dome/creation.hpp"
#include "dome/vrm.hpp"
#include "dome/document.hpp"
#include "dome/normal-frames.hpp"
#include "dome/rig-edit.hpp"
#include "dome/source-maps.hpp"
#include "dome/material-bindings.hpp"
#include "dome/material-visibility.hpp"
#include "dome/texture-set-resolution.hpp"
#include "document-authority.hpp"
#include "bake-target-resolution.hpp"
#include "import-controller-driver.hpp"
#include "cgltf.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <png.h>
using namespace dome;
namespace {
size_t checks=0;
void check(bool value,const char*label){if(!value)throw std::runtime_error(label);++checks;std::cout<<"PASS "<<label<<'\n';}
#include "creator-export-colour.hpp"
template<class F>bool refuses(F action){try{action();return false;}catch(const std::exception&){return true;}}
float difference(const Mesh&a,const Mesh&b){if(a.positions.size()!=b.positions.size())throw std::runtime_error("Unexpected vertex count");float maximum=0;for(size_t i=0;i<a.positions.size();++i)maximum=std::max(maximum,length(a.positions[i]-b.positions[i]));return maximum;}
void source_retained(const CreatedHumanoid&base,const CreatedHumanoid&next){
 check(next.parts.size()==19||next.parts.size()==20,"Creator outputs complete genuine source parts with only an optional new groom");
 for(size_t i=0;i<19;++i){const auto&a=base.parts[i],&b=next.parts[i];
  if(a.mesh.indices!=b.mesh.indices||a.mesh.uv!=b.mesh.uv||a.deformation.skin->vertices!=b.deformation.skin->vertices||
     a.appearance->original_bytes!=b.appearance->original_bytes||a.appearance->asset!=b.appearance->asset||a.appearance->decoded_images!=b.appearance->decoded_images||
     a.appearance->source_vertices!=b.appearance->source_vertices||a.appearance->source_joint_nodes!=b.appearance->source_joint_nodes||a.appearance->source_morph_targets!=b.appearance->source_morph_targets||
     a.appearance->material_json!=b.appearance->material_json||a.appearance->mtoon_json!=b.appearance->mtoon_json||*a.appearance->vrm_json!=*b.appearance->vrm_json)
   throw std::runtime_error("Creator changed original topology/UV/skin/source/image/material/morph authority");
  const auto &ar=*a.deformation.skeleton,&br=*b.deformation.skeleton;
  if(ar.bones.size()!=br.bones.size())throw std::runtime_error("Creator changed source rig size");
  for(size_t joint=0;joint<ar.bones.size();++joint)if(ar.bones[joint].name!=br.bones[joint].name||ar.bones[joint].parent!=br.bones[joint].parent)throw std::runtime_error("Creator changed authored joint hierarchy");
  if(bool(a.deformation.morphs)!=bool(b.deformation.morphs))throw std::runtime_error("Creator dropped source morphs");
  if(a.deformation.morphs){if(a.deformation.morphs->targets.size()!=b.deformation.morphs->targets.size())throw std::runtime_error("Creator changed morph identities");for(size_t target=0;target<a.deformation.morphs->targets.size();++target)if(a.deformation.morphs->targets[target].name!=b.deformation.morphs->targets[target].name)throw std::runtime_error("Creator renamed an original morph");}
  b.normal_frames->validate();for(auto n:b.mesh.normals)if(!std::isfinite(n.x)||!std::isfinite(n.y)||!std::isfinite(n.z)||std::abs(length(n)-1)>1e-3f)throw std::runtime_error("Creator produced invalid normal transport");
 }
 check(true,"Every original source vertex/UV/skin/joint/morph/image/material identity survives the actual control output");
}
void controls(const std::filesystem::path&root){
 nlohmann::json evidence=nlohmann::json::array();
 for(const char*id:{"vroid-beta-female","vroid-beta-male"}){
  const auto original_controls=humanoid_baseline_defaults(id);const auto original=create_humanoid(original_controls);
  const auto legacy=original_controls.encode();check(legacy.at("version")==2&&!legacy.contains("hair_recolor")&&HumanoidControls::decode(legacy).encode()==legacy,"Absent recolour keeps the exact version2 legacy recipe and round trip");
  auto enabled=original_controls;set_humanoid_creator_parameter(enabled,26,1);check(enabled.hair_recolor==std::optional(humanoid_hair_recolor_value(original_controls)),"Explicit recolour mode seeds the effective original tinted RGB without any white reset");set_humanoid_creator_parameter(enabled,26,0);check(enabled.encode()==legacy,"Returning to original tint exactly restores the complete old recipe");
  for(size_t channel=0;channel<3;++channel){
   auto a=original_controls;a.hair_length=a.hair_volume=a.fringe=.2f;a.hair_recolor=std::array<float,3>{.2f,.4f,.6f};auto b=a;(*b.hair_recolor)[channel]=.8f;
   const auto before=create_humanoid(a),after=create_humanoid(b);source_retained(before,after);
   const auto &x=*before.parts.back().source_maps->color.image,&y=*after.parts.back().source_maps->color.image;size_t changed=0;
   for(size_t pixel=0;pixel<x.rgba.size();pixel+=4){changed+=x.rgba[pixel+channel]!=y.rgba[pixel+channel];if(x.rgba[pixel+3]!=y.rgba[pixel+3])throw std::runtime_error("Hair recolour changed groom alpha");}
   check(changed>1000&&difference(before.parts.back().mesh,after.parts.back().mesh)==0,"Each absolute RGB channel changes actual fitted-groom strand pixels while preserving its geometry and alpha");
   check(b.encode().at("version")==3&&HumanoidControls::decode(b.encode()).encode()==b.encode(),"Explicit absolute recolour uses a distinct versioned persisted recipe");
  }

  for(size_t parameter=0;parameter<26;++parameter){
   auto c=original_controls;const auto descriptor=humanoid_creator_parameter(c,parameter);
   const float value=parameter==0?c.height+.10f:parameter>=17?.35f:.45f;
   set_humanoid_creator_parameter(c,parameter,value);auto changed=create_humanoid(c);source_retained(original,changed);
   float geometry=0,color=0;for(size_t part=0;part<19;++part){geometry=std::max(geometry,difference(original.parts[part].mesh,changed.parts[part].mesh));for(size_t k=0;k<4;++k)color=std::max(color,std::abs(original.parts[part].color[k]-changed.parts[part].color[k]));}
   std::cout<<"CONTROL "<<id<<" "<<parameter<<" "<<descriptor.label<<" geometry="<<geometry<<" material="<<color<<" parts="<<changed.parts.size()<<'\n';
   if(parameter<14)check(geometry>1e-5f,"Exposed body/face/style parameter changes genuine CC0 geometry");
   else if(parameter<17){
    check(changed.parts.size()==20&&geometry==0,"Hair controls add an original fitted groom without rewriting any source hair/body vertex");
    auto groom_controls=original_controls;groom_controls.hair_length=groom_controls.hair_volume=groom_controls.fringe=.2f;
    const auto groom_before=create_humanoid(groom_controls);set_humanoid_creator_parameter(groom_controls,parameter,.65f);const auto groom_after=create_humanoid(groom_controls);
    const auto &a=groom_before.parts.back().mesh,&b=groom_after.parts.back().mesh;
    check((a.positions.size()!=b.positions.size()||difference(a,b)>1e-6f),"Each exposed hair parameter changes the actual fitted groom geometry when a groom already exists");
    std::cout<<"GROOM "<<id<<" "<<descriptor.label<<" before_vertices="<<a.positions.size()<<" after_vertices="<<b.positions.size()<<" position_delta="<<(a.positions.size()==b.positions.size()?difference(a,b):-1.f)<<'\n';
   }
   else check(color>.01f&&geometry==0,"Exposed tint changes actual source material factors while retaining source geometry");
   evidence.push_back({{"baseline",id},{"parameter",descriptor.label},{"value",value},{"maximum_source_position_delta",geometry},{"maximum_material_factor_delta",color},{"parts",changed.parts.size()}});
  }
  auto styled=humanoid_preset(HumanoidPreset::Anime,id);styled.hair_length=.35f;styled.hair_volume=.25f;styled.fringe=.2f;styled.skin_color={.8f,.6f,.5f,1};styled.iris_color={.4f,.6f,.9f,1};styled.hair_color={.3f,.2f,.15f,1};styled.hair_recolor=std::array<float,3>{.8f,.2f,.1f};
  const auto model=create_humanoid(styled);source_retained(original,model);size_t moving=0,morphs=0;
  for(const auto&part:model.parts){auto deformation=part.deformation;const auto rest=deformation.preview(part.mesh);check(difference(rest,part.mesh)<2e-5f,"Rest skin/inverse binds preserve generated body and groom positions");
   for(const auto&m:deformation.skeleton->skin_matrices({}))for(size_t k=0;k<16;++k)if(std::abs(m[k]-(k%5==0?1.f:0.f))>2e-5f)throw std::runtime_error("Generated rest skin is not identity");
   deformation.pose.clear();for(const auto&b:deformation.skeleton->bones)deformation.pose.push_back(b.rest);const auto head=deformation.humanoid->joints.at("head");deformation.pose[head]=adjust_joint_pose(deformation.pose[head],arcball({0,1,0},normalized({.08f,1,.02f})),{});
   if(deformation.morphs){for(size_t t=0;t<deformation.morphs->targets.size();++t){bool nonzero=false;for(auto v:deformation.morphs->targets[t].delta)nonzero|=length(v)>1e-5f;if(nonzero){deformation.morph_weights.assign(deformation.morphs->targets.size(),0);deformation.morph_weights[t]=.3f;++morphs;break;}}}
   const auto preview=deformation.preview(part.mesh);preview.validate();check(preview.indices==part.mesh.indices&&preview.uv==part.mesh.uv,"Real posed/morphed creator output keeps its stable topology and authored UVs");moving+=difference(preview,part.mesh)>1e-5f;
  }
  check(moving>2&&morphs>0,"Real source head skin and authored facial targets remain usable after combined body/face/hair settings");
  Scene scene;append_humanoid(scene,styled,"Creator output");
  bool hair=false;for(const auto&o:scene.objects())if(o.material_bindings&&o.appearance&&o.name.find("HAIR")!=std::string::npos){
   hair=true;const auto &binding=*o.material_bindings;const auto &p=binding.material.parameters;
   check(o.material.color[0]==.8f&&o.material.color[1]==.2f&&o.material.color[2]==.1f&&p.base_color[0]==.8f&&p.base_color[1]==.2f&&p.base_color[2]==.1f,"Actual PBR and typed MToon creator base factors both use selected arbitrary RGB");
   const auto source=retained_mtoon(o.appearance.get())->parameters;const auto lum=[](auto c){return .2126f*c[0]+.7152f*c[1]+.0722f*c[2];};const std::array shade{source.shade_color.x,source.shade_color.y,source.shade_color.z};
   check(std::abs(p.shade_color.x/p.base_color[0]-lum(shade)/lum(source.base_color))<1e-6f&&std::abs(p.shade_color.y/p.base_color[1]-p.shade_color.x/p.base_color[0])<1e-6f,"Actual toon shade retains original source luminance contrast in the selected hair hue");
   check(binding.roles[size_t(MToonSlot::Base)].image==binding.roles[size_t(MToonSlot::Shade)].image&&binding.uv_origin->material->encode()==retained_mtoon(o.appearance.get())->encode(),"Original grayscale shade/base images and original material provenance remain immutable");
  }check(hair,"Combined creator fixture contains the genuine source hair material");
  const auto saved=root/(std::string(id)+"-styled.dsculpt");scene.save(saved);check(Scene::load(saved).encode()==scene.encode(),"Real recoloured avatar save/reopen preserves exact geometry, textures, skin, morphs, typed colour and recipe");
  const auto path=root/(std::string(id)+"-styled.glb");export_glb(scene.surfaces(true),path);
  cgltf_options options{};cgltf_data*raw=nullptr;check(cgltf_parse_file(&options,path.c_str(),&raw)==cgltf_result_success,"Independent cgltf parses actual styled creator export");std::unique_ptr<cgltf_data,decltype(&cgltf_free)>data(raw,cgltf_free);check(cgltf_load_buffers(&options,data.get(),path.c_str())==cgltf_result_success&&cgltf_validate(data.get())==cgltf_result_success,"Independent glTF consumer validates actual styled geometry, skins, morphs and image package");
  size_t primitives=0,skinned=0,targets=0;for(size_t n=0;n<data->nodes_count;++n)if(data->nodes[n].mesh){skinned+=bool(data->nodes[n].skin);for(size_t p=0;p<data->nodes[n].mesh->primitives_count;++p){++primitives;targets+=data->nodes[n].mesh->primitives[p].targets_count;}}
  check(primitives==20&&skinned>=19&&targets>0,"Export retains all real source primitives plus the authored groom and actual skin/morph payloads");
  consumer_glb_colours(*data,model);
  auto source_only=humanoid_baseline_defaults(id);source_only.hair_color=styled.hair_color;source_only.hair_recolor=styled.hair_recolor;Scene vrm;append_humanoid(vrm,source_only,"Recoloured source avatar");const auto vrm_path=root/(std::string(id)+"-recoloured.vrm");export_vrm(vrm.preview_surfaces(),vrm_path);
  cgltf_data*vrm_raw=nullptr;check(cgltf_parse_file(&options,vrm_path.c_str(),&vrm_raw)==cgltf_result_success,"Independent consumer parses colour-only neutral source-backed VRM export");std::unique_ptr<cgltf_data,decltype(&cgltf_free)>vrm_data(vrm_raw,cgltf_free);check(cgltf_load_buffers(&options,vrm_data.get(),vrm_path.c_str())==cgltf_result_success&&cgltf_validate(vrm_data.get())==cgltf_result_success,"Actual VRM retains valid complete source geometry, skin, morphs and images");
  const auto original_hair=std::find_if(model.parts.begin(),model.parts.end(),[](const auto&p){return p.name.find("HAIR")!=std::string::npos;});check(original_hair!=model.parts.end(),"Native source keeps actual original hair identity");consumer_vrm_colours(*vrm_data,*original_hair,*styled.hair_recolor);

 }
 std::ofstream(root/"creator-control-effects.json")<<evidence.dump(2)<<'\n';
}
void ready(Document&d){if(!d.wait_asset(std::chrono::seconds(180))||!d.wait_mesh(std::chrono::seconds(180)))throw std::runtime_error("Creator worker/surface timed out");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
void resize_source_grid(const std::filesystem::path &root){
 for(const char *baseline:{"vroid-beta-female","vroid-beta-male"}){
  Scene scene;
  const auto group=append_humanoid(scene,humanoid_baseline_defaults(baseline),"Resize original face",{},256);
  const auto face=std::find_if(scene.objects().begin(),scene.objects().end(),[&](const auto &part){
    return part.group==group && part.name.find("Face_00_SKIN")!=std::string::npos;
  });
  check(face!=scene.objects().end()&&face->appearance&&face->painted->source_maps(),
        "Resize fixture has a genuine source-authored VRoid face and its original images");
  const auto id=face->id;
  scene.select(id);
  const auto before=scene.encode();
  auto denied=scene;
  auto small=detail::bake_target_resolution_limits();
  small.maximum_bytes=1;
  check(refuses([&]{detail::resize_unpainted_bake_target(denied,id,1024,small);})&&denied.encode()==before,
        "Insufficient prospective raster memory refuses without changing the complete source scene");

  Document doc;
  doc.replace(scene);
  ready(doc);
  check(doc.frame().state->can_resize_bake_target,"An empty source-authored paint grid enables the native resolution control");
  ImportDriver hand(doc);
  hand.open();
  hand.sector(9);
  hand.sector(1);
  hand.sector(6);
  check(doc.frame().state->controls.parameter==VrParameter::TextureResolution,
        "The XR Materials and Texture Sets pages open the original mesh resolution control");
  hand.ray(1.f);
  hand.panel_button(true);
  const auto file=root/(std::string(baseline)+"-face2048.dsculpt");
  doc.save_async(file).get();
  ready(doc);
  const auto after=doc.copy_scene();
  const auto &current=after.object(id);
  check(current.painted->resolution()==2048&&current.painted->mesh_owner()==face->painted->mesh_owner()&&
        current.painted->source_maps()==face->painted->source_maps()&&current.appearance==face->appearance&&
        current.deformation==face->deformation&&current.material_bindings==face->material_bindings,
        "The controller changes only the selected native raster while retaining the mesh, images, rig and material");
  auto original_grid=after;
  original_grid.object(id).painted=face->painted;
  check(original_grid.encode()==before&&Scene::load(file).encode()==after.encode(),
        "The queued Save persists the changed grid and every unrelated source value exactly");
  check(doc.undo()&&doc.copy_scene().encode()==before&&doc.redo()&&doc.copy_scene().encode()==after.encode(),
        "Source-authored resolution change admits one exact Undo and Redo");

  auto painted=scene;
  auto &paint=painted.object(id).painted;
  paint=paint->snapshot();
  Brush brush;
  brush.from=brush.to=paint->mesh().positions.front();
  brush.radius=2;
  PaintSettings ink;
  ink.front_only=false;
  paint->begin_stroke();
  paint->stamp(brush,ink);
  check(paint->end_stroke(),"The protected-grid fixture contains actual artist paint");
  const auto painted_bytes=painted.encode();
  check(refuses([&]{detail::resize_unpainted_bake_target(painted,id,1024);})&&painted.encode()==painted_bytes,
        "The empty-grid route refuses a painted source without discarding artist data");
 }
}
void resize_painted_source_grid(const std::filesystem::path &root) {
  for (const char *baseline : {"vroid-beta-female", "vroid-beta-male"}) {
    Scene scene;
    const auto group = append_humanoid(scene, humanoid_baseline_defaults(baseline),
                                       "Painted source face", {}, 128);
    const auto face = std::ranges::find_if(scene.objects(), [&](const auto &part) {
      return part.group == group && part.name.find("Face_00_SKIN") != std::string::npos;
    });
    check(face != scene.objects().end() && face->appearance && face->painted->source_maps(),
          "Fixture contains an actual VRoid face with its source images");
    const auto id = face->id;
    scene.select(id);
    auto paint = face->painted->snapshot();
    scene.object(id).painted = paint;
    Brush brush;
    brush.from = brush.to = paint->mesh().positions.front();
    brush.radius = 2;
    PaintSettings ink;
    ink.front_only = false;
    ink.color = {.83f, .12f, .36f, .72f};
    const auto base = paint->active_layer();
    paint->begin_stroke();
    paint->stamp(brush, ink);
    ink.channel = PaintChannel::Mask;
    ink.mask = .59f;
    paint->stamp(brush, ink);
    check(paint->end_stroke(), "Source grid contains real painted colour, alpha and mask data");
    paint->add_layer("Hidden finish");
    const auto hidden = paint->active_layer();
    ink.channel = PaintChannel::Emission;
    ink.emission = {.25f, .68f, .42f};
    paint->begin_stroke();
    paint->stamp(brush, ink);
    check(paint->end_stroke(), "A second source layer contains authored emission");
    paint->layer_properties("Hidden finish", false, .48f);
    scene.validate();
    const auto before = scene.encode();
    TextureSetResolutionOptions options;
    options.resolution = 256;
    auto limits = detail::bake_target_resolution_limits();
    if (compute_backend() != "CPU") {
      check(refuses([&] { resample_native_texture_grid(scene, id, options, limits); }) &&
                scene.encode() == before,
            "Missing accelerated grid resampling refuses without CPU substitution or paint loss");
      continue;
    }

    auto constrained = limits;
    unsigned polls = 0;
    constrained.cancelled = [&] { return ++polls >= 500; };
    check(refuses([&] { resample_native_texture_grid(scene, id, options, constrained); }) &&
              polls >= 500 && scene.encode() == before && scene.object(id).painted == paint,
          "Cancellation during private grid work preserves all original paint owners");
    constrained = limits;
    constrained.maximum_bytes = 1;
    check(refuses([&] { resample_native_texture_grid(scene, id, options, constrained); }) &&
              scene.encode() == before,
          "A prospective byte-budget refusal preserves the complete painted source");
    auto exact = options;
    exact.resolution = 64;
    exact.maximum_component_error = exact.maximum_rms_error = 0;
    check(refuses([&] { resample_native_texture_grid(scene, id, exact, limits); }) &&
              scene.encode() == before,
          "Inexact paint reconstruction refuses at a zero tolerance without changing the document");

    Document doc;
    doc.replace(scene);
    ready(doc);
    check(doc.frame().state->can_resize_bake_target,
          "A painted original grid enables the native resolution control");
    ImportDriver hand(doc);
    hand.open();
    hand.sector(9);
    hand.sector(1);
    hand.sector(6);
    check(doc.frame().state->controls.parameter == VrParameter::TextureResolution,
          "The shared XR texture-set control opens for painted source data");
    hand.ray(.4f);
    hand.panel_button(true);
    const auto file = root / (std::string(baseline) + "-painted256.dsculpt");
    doc.save_async(file).get();
    ready(doc);
    const auto after = doc.copy_scene();
    const auto &resized = after.object(id);
    check(resized.painted->resolution() == 256 && resized.painted->mesh_owner() == paint->mesh_owner() &&
              resized.painted->source_maps() == paint->source_maps() &&
              resized.material_bindings == face->material_bindings && resized.appearance == face->appearance &&
              resized.deformation == face->deformation,
          "Accepted resize retains mesh, original sources, materials and rig");
    const auto &layers = resized.painted->layers();
    check(layers.size() == 2 && layers[0].id == base && layers[1].id == hidden &&
              resized.painted->active_layer() == hidden && !layers[1].visible &&
              layers[1].name == "Hidden finish" && layers[1].opacity == .48f,
          "Resampling preserves layer order, identity, selection, visibility and opacity");
    bool mask = false, emission = false, exact_layers = true;
    for (uint32_t y = 0; y < 256; ++y)
      for (uint32_t x = 0; x < 256; ++x) {
        const auto value = resized.painted->pixel(base, x, y);
        mask |= value.data[0] > 100 && value.data[9] > 64 &&
                value.data[13] > 80 && value.data[13] < 220;
        const auto finish = resized.painted->pixel(hidden, x, y);
        emission |= finish.data[7] > 100 && finish.data[12] > 64;
        exact_layers &= value == paint->pixel(base, x / 2, y / 2) &&
                        finish == paint->pixel(hidden, x / 2, y / 2);
      }
    check(mask && emission, "Painted colour, fractional masks and hidden emission all survive");
    check(exact_layers, "Enlargement retains every authored byte in both original layer grids");
    auto old_raster = after;
    old_raster.object(id).painted = paint;
    check(old_raster.encode() == before && Scene::load(file).encode() == after.encode(),
          "Ordered Save and reopen preserve the replacement grid and every unrelated Scene value");
    check(doc.undo() && doc.copy_scene().encode() == before &&
              doc.redo() && doc.copy_scene().encode() == after.encode(),
          "Resizing painted source data creates one exact Undo and Redo");
    auto reduced = after;
    options.resolution = 128;
    const auto report = resample_native_texture_grid(reduced, id, options, limits);
    auto restored = reduced.object(id).painted;
    bool restored_cells = true;
    for (uint32_t y = 0; y < 128; ++y)
      for (uint32_t x = 0; x < 128; ++x)
        restored_cells &= restored->pixel(base, x, y) == paint->pixel(base, x, y) &&
                          restored->pixel(hidden, x, y) == paint->pixel(hidden, x, y);
    check(report.changed && restored_cells && restored->source_maps() == paint->source_maps(),
          "Reducing the enlarged grid recovers every original authored cell and source owner");
    check(!resample_native_texture_grid(reduced, id, options, limits).changed &&
              reduced.object(id).painted == restored,
          "Same-size native resampling keeps the exact paint object");
  }
}
std::atomic_bool gate{false};std::mutex gate_mutex;std::condition_variable changed;bool entered=false,released=false;
void arm(){std::lock_guard lock(gate_mutex);entered=released=false;gate=true;}
void await_gate(){std::unique_lock lock(gate_mutex);if(!changed.wait_for(lock,std::chrono::seconds(30),[]{return entered;}))throw std::runtime_error("Creator Save barrier timed out");}
void release(){std::lock_guard lock(gate_mutex);released=true;changed.notify_all();}
Scene authored(){
 Scene scene;append_humanoid(scene,humanoid_preset(HumanoidPreset::Toon),"Existing 作家 avatar");
 auto &object=scene.active();object.painted=object.painted->snapshot();object.painted->add_layer("Existing artist paint");const auto&m=object.painted->mesh();bool painted=false;
 for(size_t i=0;i<m.indices.size()&&!painted;i+=3){const auto a=m.positions[m.indices[i]],b=m.positions[m.indices[i+1]],c=m.positions[m.indices[i+2]];Brush brush;brush.from=brush.to=(a+b+c)/3;brush.radius=.02f;PaintSettings ink;ink.color={.9f,.1f,.3f,1};ink.flow=1;object.painted->begin_stroke();object.painted->stamp(brush,ink);painted=object.painted->end_stroke();}
 check(painted,"Actual existing CC0 avatar fixture contains genuine artist-painted pixels");
 RigEdit edit;edit.joint=JointAdjustment{object.deformation->humanoid->joints.at("head"),{0,5,0},{}};edit_rig(scene,object.id,edit);scene.validate();return scene;
}
void old_avatar_retained(const Scene&before,const Scene&after){
 for(const auto&o:before.objects())if(o.mesh_active)check(detail::same_object_authority(o,after.object(o.id)),"New avatar creation preserves every immutable old mesh/paint/source/rest/skin/morph owner");
 for(const auto&g:before.groups()){const auto &next=after.group(g.id);check(g.name==next.name&&g.parent==next.parent&&detail::same_transform(g.transform,next.transform)&&detail::same_humanoid_controls(g.humanoid,next.humanoid),"Variation retains genuine original group transform and creator recipe exactly");}
}
void workspace(const std::filesystem::path&root){
 Document doc;const auto source=authored();doc.replace(source);ready(doc);const auto baseline=doc.copy_scene().encode();ImportDriver driver(doc);driver.open();for(int i:{8,10,2})driver.sector(i);
 check(doc.frame().state->creator&&doc.frame().state->menu_page==33,"Actual controller opens Objects / Avatars / Creator controls");
 check(doc.frame().state->creator->can_copy_selected&&!doc.frame().state->can_undo,"Creator exposes real saved settings while keeping global history separate");
 driver.sector(7);auto view=doc.frame().state->creator;check(view->settings.controls.encode()==source.group(source.active().group).humanoid->encode()&&view->settings.name.find("variation")!=std::string::npos,"Controller copies genuine saved creator parameters into a NEW-avatar draft");
 const auto copied=view->settings;driver.sector(5);check(doc.frame().state->creator->settings.controls.anime==.25f,"Actual controller Anime preset uses the existing bounded VRoid backend values");check(doc.undo()&&doc.frame().state->creator->settings.controls.encode()==copied.controls.encode(),"Local creator Undo restores the exact copied controls without touching the avatar");check(doc.redo()&&doc.frame().state->creator->settings.controls.anime==.25f,"Local creator Redo restores the exact style draft");
 driver.sector(6);check(doc.frame().state->controls.parameter==VrParameter::CreatorValue,"Actual controller opens the shared large creator value slider");const float height=doc.frame().state->creator->settings.controls.height;driver.ray(.85f);driver.panel_button(false);check(doc.frame().state->creator->settings.controls.height==height,"Cancel restores the exact prior creator value without any Scene/history edit");
 driver.sector(6);driver.ray(.7f);driver.panel_button(true);check(std::abs(doc.frame().state->creator->settings.controls.height-1.9f)<1e-6f,"Controller ray applies a real bounded metric body parameter");
 for(int i=0;i<14;++i)driver.sector(1);
 for(float value:{.2f,.15f,.1f}){driver.sector(6);driver.ray(value);driver.panel_button(true);driver.sector(1);}
 check(std::abs(doc.frame().state->creator->settings.controls.hair_length-.2f)<1e-6f&&std::abs(doc.frame().state->creator->settings.controls.hair_volume-.15f)<1e-6f&&std::abs(doc.frame().state->creator->settings.controls.fringe-.1f)<1e-6f,"Actual controller sliders author length, volume and fringe in the native groom settings");
 for(int i=0;i<6;++i)driver.sector(1);driver.sector(6);driver.ray(.3f);driver.panel_button(true);
 check(doc.frame().state->creator->settings.controls.hair_recolor&&std::abs(doc.frame().state->creator->settings.controls.hair_recolor->at(0)-.3f)<1e-6f,"Actual controller slider enables and authors arbitrary red hair recolour");
 check(doc.copy_scene().encode()==baseline,"All body/face/hair drafts leave genuine prior geometry, paint, source and rig untouched");
 driver.sector(11);auto saved=doc.save_async(root/"created.dsculpt");saved.get();ready(doc);const auto accepted_scene=doc.copy_scene();const auto accepted=accepted_scene.encode();old_avatar_retained(source,accepted_scene);
 check(accepted_scene.objects().size()==source.objects().size()+20&&!doc.frame().state->creator,"Controller Apply creates a complete distinct avatar through the native ordered worker");check(Scene::load(root/"created.dsculpt").encode()==accepted,"Save ordered immediately after Apply/reopen retains complete old and new creator content");check(doc.undo()&&doc.copy_scene().encode()==baseline,"Exactly one global Undo removes only the newly applied avatar");ready(doc);check(doc.redo()&&doc.copy_scene().encode()==accepted,"Exactly one global Redo restores the complete created avatar");ready(doc);
 doc.begin_humanoid_creator();auto old=doc.frame().state->creator;doc.end_humanoid_creator(false);doc.begin_humanoid_creator();auto current=doc.frame().state->creator;
 check(refuses([&]{doc.configure_humanoid_creator(old->settings,old->session);})&&doc.frame().state->creator==current,"Retired creator session cannot modify a new draft on the same source");check(refuses([&]{doc.end_humanoid_creator(false,old->session);})&&doc.frame().state->creator==current,"Retired cancellation cannot close another creator session");doc.end_humanoid_creator(false);
 doc.begin_humanoid_creator();auto epoch=doc.frame().state->creator;doc.hold_object(doc.frame().state->active_object);check(refuses([&]{doc.configure_humanoid_creator(epoch->settings,epoch->session);}),"Changed edit authority refuses an older creator draft");doc.end_humanoid_creator(false);doc.release_held_object();
 doc.begin_humanoid_creator();driver.frame.focused=false;driver.send();check(!doc.frame().state->creator&&doc.copy_scene().encode()==accepted,"Real focus loss cancels an unapplied creator draft without any geometry/history mutation");driver.frame.focused=true;driver.send();
 Document pending;pending.replace(Scene{});ready(pending);const auto empty=pending.copy_scene().encode();struct Release{~Release(){release();}}release_guard;
 arm();auto blocked=pending.save_async(root/"before-cancel.dsculpt");await_gate();pending.begin_humanoid_creator();pending.end_humanoid_creator(true);pending.cancel_asset_work();release();blocked.get();ready(pending);check(pending.copy_scene().encode()==empty&&!pending.frame().state->can_undo,"Accepted queued creator Cancel preserves the complete source and global history");
 arm();blocked=pending.save_async(root/"before-replace.dsculpt");await_gate();pending.begin_humanoid_creator();pending.end_humanoid_creator(true);pending.replace(Volume(.07f));const auto replacement=pending.copy_scene().encode();release();blocked.get();ready(pending);check(pending.copy_scene().encode()==replacement&&!pending.frame().state->can_undo,"Stale accepted creator worker cannot overwrite a replacement generation");
 HumanoidCreatorSettings legacy;legacy.controls=humanoid_baseline_defaults("makehuman-hm08");check(refuses([&]{legacy.validate();}),"The creator never offers or falls back to removed MakeHuman geometry");
}
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd){if(gate.exchange(false)){std::unique_lock lock(gate_mutex);entered=true;changed.notify_all();if(!changed.wait_for(lock,std::chrono::seconds(60),[]{return released;}))return -1;}return __real_fsync(fd);}
int main(int argc,char**argv)try{
 std::cout<<std::unitbuf;
 if(argc==5&&std::string_view(argv[1])=="--verify-xr"){
  const auto source=Scene::load(argv[2]);const auto actual=Scene::load(argv[3]);auto expected=source;
  if(std::string_view(argv[4])=="accept"||std::string_view(argv[4])=="accept-desktop"){
   auto group=source.group(source.active().group);if(!group.humanoid)throw std::runtime_error("Creator XR input lacks genuine saved controls");
   check(actual.groups().size()==source.groups().size()+1&&bool(actual.groups().back().humanoid),"Actual creator adds one separate avatar with persisted genuine bounded controls");
   const auto &persisted=*actual.groups().back().humanoid;
   check(std::abs(persisted.height-1.9f)<1e-5f&&std::abs(persisted.build-.4f)<1e-5f,"Actual released pressure and controller ray reach requested height and build within native float precision");
   auto controls=humanoid_preset(HumanoidPreset::Anime,group.humanoid->baseline);controls.height=persisted.height;controls.build=persisted.build;
   check(persisted.hair_recolor&&std::abs(persisted.hair_recolor->at(0)-.7f)<1e-5f,"Actual released XR pressure enables arbitrary red hair colour");
   controls.hair_recolor=humanoid_hair_recolor_value(controls);controls.hair_recolor->at(0)=persisted.hair_recolor->at(0);
   check(controls.encode()==persisted.encode(),"Actual source, preset and all other body/face/hair/tint settings exactly match selected native draft actions");
   CanvasTransform transform{};transform.position.y=-controls.height*.5f;
   append_humanoid(expected,controls,group.name+(std::string_view(argv[4])=="accept-desktop"?" copy":" variation"),transform);
   // These are independent file loads: process-local shared_ptr identities
   // cannot cross serialization. The complete encoded Scene equality below
   // includes every old geometry/paint/source/rest/skin/morph/group byte.
   // In-process workspace tests retain old_avatar_retained() owner checks.
  }
  const auto actual_bytes=nlohmann::json::to_cbor(actual.encode());
  const auto expected_bytes=nlohmann::json::to_cbor(expected.encode());
  check(actual_bytes==expected_bytes,"Actual loader creator endpoint preserves complete canonical native bytes of the independent control result and every prior authored source");
  std::cout<<"CREATOR CHECKS "<<checks<<'\n';return 0;
 }
 const auto root=argc>2?std::filesystem::path(argv[2]):std::filesystem::temp_directory_path()/("creator-workspace-"+std::to_string(getpid()));std::filesystem::create_directories(root);
 if(argc>1&&std::string_view(argv[1])=="--controls")controls(root);
 else if(argc>1&&std::string_view(argv[1])=="--fixture"){authored().save(root/"creator-input.dsculpt");}
 else if(argc>1&&std::string_view(argv[1])=="--resize-source-grid")resize_source_grid(root);
 else if(argc>1&&std::string_view(argv[1])=="--resize-painted-source-grid")resize_painted_source_grid(root);
 else workspace(root);
 std::cout<<"CREATOR CHECKS "<<checks<<'\n';return 0;
}catch(const std::exception&e){release();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
