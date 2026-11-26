// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_VRM1_SOURCE_EXPORT_NO_MAIN
#include "vrm1-source-export.cpp"
#undef DOME_VRM1_SOURCE_EXPORT_NO_MAIN
#include "dome/vrm-expression.hpp"
#include "dome/source-paint.hpp"
#include "dome/source-placement.hpp"
#include "source-paint-parameters.hpp"
#include "dome/vrm-authoring.hpp"
#include "dome/gltf-export.hpp"
#include "dome/native-authoring.hpp"
#include "dome/projection.hpp"
#include "import-skin-fixtures.hpp"
#include <future>
#include <condition_variable>
namespace {
void near(float a,float b,const char *why){check(std::abs(a-b)<3e-6f,why);}
template<class F>std::string refuses(F f){try{f();}catch(const std::exception &e){return e.what();}return {};}
void end(Document &d){check(d.wait_asset(std::chrono::seconds(120))&&d.wait_mesh(std::chrono::seconds(120))&&d.frame().state->error.empty(),"Actual static expression worker completes without source/error loss");}
void ink(Document &d){PaintSettings settings;settings.color={.15f,.85f,.3f,1};settings.front_only=false;d.paint_settings(settings);
 const auto s=d.copy_scene();const auto &o=s.active();auto m=o.deformation?o.deformation->preview(o.painted->mesh()):o.painted->mesh();const auto a=m.indices[0],b=m.indices[1],c=m.indices[2];auto p=m.positions[a]*.3f+m.positions[b]*.3f+m.positions[c]*.4f;auto n=normalized(cross(m.positions[b]-m.positions[a],m.positions[c]-m.positions[a]));const auto w=s.world_matrix(o.id);auto wp=w.point({p.x,p.y,p.z}),wn=w.normal({n.x,n.y,n.z});p={float(wp[0]),float(wp[1]),float(wp[2])};n=normalized(Vec3{float(wn[0]),float(wn[1]),float(wn[2])});NativeStrokeSample q;q.origin=p+n;q.direction=n*-1;q.brush.radius=.15f;q.brush.blend=0;
 check(d.begin_native_stroke()&&d.sample_native_stroke(q),"Real source-owned native paint accepts before expression authoring");d.end_native_stroke();check(d.wait_native_stroke()&&d.wait_mesh(),"Real paint worker completes");}

// The expected arrays are derived from fixture JSON and the VRM absolute
// target-minus-current rule, independently of the product expression helper.
void check_texture(const PaintSourceTexture &before,const PaintSourceTexture &after,
                   float rotation,float weight) {
  const auto &a=before.uv_transform;
  const float u=.7f*weight,v=.9f*weight;
  const std::array<float,6> expected{
    a[0]*(1-weight)+std::cos(rotation)*u,a[1]*(1-weight)-std::sin(rotation)*v,
    a[2]*(1-weight)+std::sin(rotation)*u,a[3]*(1-weight)+std::cos(rotation)*v,
    a[4]*(1-weight)+.2f*weight,a[5]*(1-weight)-.1f*weight};
  check(before.image&&after.image==before.image&&after.texcoord==before.texcoord&&
          after.wrap_s==before.wrap_s&&after.wrap_t==before.wrap_t&&
          after.min_filter==before.min_filter&&after.mag_filter==before.mag_filter,
        "Expression keeps the texture image, sampler and original UV index");
  for(size_t k=0;k<6;++k)near(after.uv_transform[k],expected[k],
    "Expression samples the exact independent original-role affine");
}

import_fixture::Fixture core_source(bool unlit) {
  auto fixture=metadata_source();
  auto &m=fixture.j["materials"][0];
  m["extensions"].erase("VRMC_materials_mtoon");
  m["extensions"].erase("KHR_materials_unlit");
  for(const char *key:{"extensionsUsed","extensionsRequired"}){
    auto &list=fixture.j[key];
    for(auto at=list.begin();at!=list.end();)
      if(*at=="VRMC_materials_mtoon"||*at=="KHR_materials_unlit")at=list.erase(at);else ++at;
  }
  m["pbrMetallicRoughness"]["baseColorFactor"]={.6f,.4f,.3f,.72f};
  m["pbrMetallicRoughness"]["metallicRoughnessTexture"]={{"index",0},{"texCoord",0}};
  m["emissiveFactor"]={.12f,.26f,.32f};
  m["emissiveTexture"]={{"index",0},{"texCoord",1}};
  m["occlusionTexture"]={{"index",0},{"strength",.75f}};
  m["normalTexture"]={{"index",0},{"texCoord",1},{"scale",.8f},
      {"extensions",{{"KHR_texture_transform",{{"rotation",-.31f},
          {"scale",{1.15f,.66f}},{"offset",{.04f,.08f}}}}}}};
  if(unlit){
    m["extensions"]["KHR_materials_unlit"]=J::object();
    fixture.j["extensionsUsed"].push_back("KHR_materials_unlit");
  }else{
    m["extensions"]["KHR_materials_emissive_strength"]={{"emissiveStrength",2.5f}};
    fixture.j["extensionsUsed"].push_back("KHR_materials_emissive_strength");
  }
  auto &happy=fixture.j["extensions"]["VRMC_vrm"]["expressions"]["preset"]["happy"];
  happy["isBinary"]=false;
  happy["materialColorBinds"].push_back({
    {"material",0},{"type","emissionColor"},{"targetValue",{.2f,.1f,.3f,13}}});
  happy["materialColorBinds"].push_back({
    {"material",0},{"type","shadeColor"},{"targetValue",{.91f,.83f,.72f,17}}});
  return fixture;
}

void core_expressions(const std::filesystem::path &out) {
  constexpr float weight=.25f;
  for(bool unlit:{false,true})for(bool atlas:{false,true}){
    auto fixture=core_source(unlit);
    const auto label=std::string(unlit?"unlit":"pbr")+(atlas?"-atlas":"-original");
    const auto input=out/(label+".vrm");
    fixture.save(input);
    Scene empty;empty.active().visible=false;
    ImportAuthoringOptions options;options.prepare_texture_atlas=atlas;options.paint_resolution=64;
    auto imported=prepare_authoring_import(empty,input,options);
    const auto id=imported.objects.front();
    imported.scene.select(id);
    Document doc;doc.replace(imported.scene);end(doc);
    if(atlas)ink(doc);
    const auto baseline=doc.copy_scene();
    const auto encoded=baseline.encode();
    const auto &start=baseline.object(id);
    check(start.material.unlit==unlit&&start.material_bindings&&!start.material_bindings->has_mtoon,
          "The source enters through the genuine plain glTF material importer");
    Vrm1ExpressionWeights weights;weights.preset["happy"]=weight;
    const auto posed=bake_vrm1_expression_pose(baseline,id,weights);
    const auto &actual=posed.object(id);
    check(baseline.encode()==encoded&&actual.appearance==start.appearance&&
          actual.material_bindings==start.material_bindings&&actual.material.unlit==unlit,
          "The expression preserves the immutable model and original frame/material owners");
    const std::array<float,4> base_target{.4f,.5f,.6f,.7f};
    const std::array<float,3> emission_target{.2f,.1f,.3f};
    for(size_t k=0;k<4;++k)near(actual.material.color[k],
      start.material.color[k]*(1-weight)+weight*base_target[k],"Plain core base factor matches the VRM expression arithmetic");
    for(size_t k=0;k<3;++k)near(actual.material.emissive[k],unlit?start.material.emissive[k]:
      start.material.emissive[k]*(1-weight)+weight*emission_target[k]*2.5f,
      "Only PBR emission follows the expression, in authored strength units");
    check(actual.material.metallic==start.material.metallic&&actual.material.roughness==start.material.roughness&&
          actual.material.alpha==start.material.alpha&&actual.material.alpha_cutoff==start.material.alpha_cutoff,
          "Unused MToon properties leave core shading and opacity controls intact");
    const auto old_grid=atlas?start.painted->source_sampling_original():start.painted;
    const auto now_grid=atlas?actual.painted->source_sampling_original():actual.painted;
    const auto &old=*old_grid->source_maps(),&now=*now_grid->source_maps();
    check(now.uv1==old.uv1&&now.uv1&&now.normal_scale==old.normal_scale&&now.occlusion_strength==old.occlusion_strength,
          "Authored original UV1, normal scale and occlusion strength stay intact");
    check_texture(old.color,now.color,.19f,weight);
    check_texture(old.normal,now.normal,-.31f,weight);
    check_texture(old.metal_rough,now.metal_rough,0,weight);
    check_texture(old.emission,now.emission,0,weight);
    check_texture(old.occlusion,now.occlusion,0,weight);
    check(actual.painted->mesh().uv==start.painted->mesh().uv&&actual.painted->source_sampling_prepared()==atlas&&
          actual.painted->valid_source_sampling_attachment(),"The current paint grid keeps its true coordinate domain");
    for(size_t layer=0;layer<start.painted->layers().size();++layer){
      const auto &a=start.painted->layers()[layer],&b=actual.painted->layers()[layer];
      check(a.id==b.id&&a.tiles==b.tiles&&a.pbr_tiles==b.pbr_tiles&&a.emission_tiles==b.emission_tiles,
            "Texture expressions keep the actual sparse native paint tiles");
    }
    if(!atlas)check(start.painted->mesh_owner()==actual.painted->mesh_owner(),
                  "Unprepared texture expressions keep the original raster geometry");
    doc.begin_expression_preview();
    check(doc.wait_mesh()&&doc.frame().state->expressions&&doc.frame().state->expressions->ready,
          "The public expression editor opens on a plain material avatar");
    auto view=doc.frame().state->expressions;
    size_t entry=0;
    while(entry<view->entries.size()&&(view->entries[entry].name!="happy"||view->entries[entry].custom))++entry;
    check(entry<view->entries.size(),"The actual imported preset is available in the expression editor");
    doc.configure_expression_preview(entry,weight,view->session);
    check(doc.wait_mesh()&&doc.frame().state->expressions->ready&&doc.frame().state->error.empty()&&
            doc.copy_scene().encode()==encoded,"Plain PBR expression preview publishes without editing the project");
    const auto preview=doc.frame().surface;
    const auto range=std::ranges::find_if(preview->ranges,[&](const auto &item){return item.object==id;});
    check(range!=preview->ranges.end()&&range->material.color==actual.material.color&&
          range->material.emissive==actual.material.emissive&&range->textures&&range->textures->source_maps&&
          range->textures->source_maps->color.uv_transform==now.color.uv_transform,
          "The immutable preview carries the evaluated original image transform and core factors");
    doc.end_expression_preview(true,view->session);
    const auto saved=out/(label+".dsculpt");
    doc.save_async(saved).get();end(doc);
    const auto wanted=posed.encode();
    check(doc.copy_scene().encode()==wanted&&Scene::load(saved).encode()==wanted,
          "Expression Apply and queued Save reach the same posed native state");
    check(doc.undo()&&doc.copy_scene().encode()==encoded&&doc.redo()&&doc.copy_scene().encode()==wanted,
          "A plain material expression is exactly one undoable edit");
    if(!atlas&&!unlit){
      NativeAuthoringExport authority;
      authority.scene=std::make_shared<const Scene>(posed.snapshot());
      authority.allow_portable_fallback=true;
      VrmExportOptions export_options;export_options.paint_resolution=64;
      const auto portable=out/"pbr-original-applied.vrm";
      export_vrm_authoring(authority,portable,export_options);
      const auto package=json(portable);
      const auto slot=package.at("meshes")[0].at("primitives")[0].at("material").get<size_t>();
      const auto &material=package.at("materials").at(slot);
      const auto factor=material.at("emissiveFactor").get<std::array<float,3>>();
      const auto extensions=material.value("extensions",J::object());
      const auto strength=extensions.value("KHR_materials_emissive_strength",J::object()).value("emissiveStrength",1.f);
      std::cout<<"Core PBR export emission strength "<<strength<<'\n';
      for(size_t k=0;k<3;++k)near(factor[k]*strength,actual.material.emissive[k],
        "The portable core material retains the exact applied effective emission");
      near(strength,2.5f,"The portable source keeps emission expression targets in the original strength units");
      check(authority.scene->encode()==wanted,"Portable VRM1 export preserves all applied native project data");
    }
  }
}

void core_unlit_live_paint(const std::filesystem::path &out) {
  auto fixture=core_source(true);
  auto &material=fixture.j["materials"][0];
  material["alphaMode"]="OPAQUE";
  material.erase("alphaCutoff");
  auto &expressions=fixture.j["extensions"]["VRMC_vrm"]["expressions"];
  const auto happy=expressions.at("preset").at("happy");
  expressions["custom"]["alternate"]={
      {"isBinary",false},
      {"materialColorBinds",{{{"material",0},{"type","color"},{"targetValue",{.7f,.2f,.5f,.9f}}}}},
      {"textureTransformBinds",{{{"material",0},{"scale",{0,.6f}},{"offset",{-.3f,.1f}}}}}};
  for (auto &node : fixture.j.at("nodes"))
    if (node.contains("children") && node.at("children").empty())
      node.erase("children");
  const auto input=out/"unlit-live-input.vrm";
  fixture.save(input);
  Scene empty;empty.active().visible=false;
  ImportAuthoringOptions import_options;
  import_options.prepare_texture_atlas=true;
  import_options.paint_resolution=64;
  auto imported=prepare_authoring_import(empty,input,import_options);
  imported.scene.select(imported.objects.front());
  Document doc;
  doc.replace(std::move(imported.scene));
  end(doc);
  ink(doc);
  const auto live=doc.copy_scene();
  const auto exact=live.encode();
  const auto &active=live.active();
  check(active.material.unlit&&active.material.alpha==Material::AlphaMode::Opaque&&
        active.painted->source_sampling_prepared()&&active.painted->layers().size()>0,
        "Opaque unlit export starts with real editable source-owned native paint");
  const auto output=out/"unlit-live-output.vrm";
  doc.export_asset(output,AssetExportFormat::EditableAvatar256).get();
  const auto saved=out/"unlit-live-preserved.dsculpt";
  doc.save_async(saved).get();
  end(doc);
  check(doc.copy_scene().encode()==exact&&Scene::load(saved).encode()==exact,
        "Live unlit export and ordered Save preserve every native source, layer and pose");
  auto output_json=json(output);
  const size_t mesh_slot=output_json.at("nodes").at(
    output_json.at("extensions").at("VRMC_vrm").at("expressions")
      .at("preset").at("happy").at("morphTargetBinds")[0].at("node").get<size_t>())
    .at("mesh").get<size_t>();
  const auto &primitives=output_json.at("meshes").at(mesh_slot).at("primitives");
  check(primitives.size()==2,"Exactly two portable draws preserve the original surface and fixed paint");
  const size_t lower=primitives[0].at("material"),upper=primitives[1].at("material");
  const auto &materials=output_json.at("materials");
  for(const auto index:{lower,upper})check(materials.at(index).at("extensions").contains("KHR_materials_unlit")&&
    !materials.at(index).at("extensions").contains("VRMC_materials_mtoon"),
    "Both independently composed draws are standard unlit materials");
  check(materials.at(lower).value("alphaMode","OPAQUE")=="OPAQUE"&&materials.at(upper).at("alphaMode")=="BLEND",
        "Opaque source ignores source alpha while the derived draw uses paint coverage");
  const auto output_bindings=output_json.at("extensions").at("VRMC_vrm").at("expressions");
  for(const auto *table:{"preset","custom"})for(const auto &binding:output_bindings.at(table)){
    for(const auto &entry:binding.value("materialColorBinds",J::array()))
      check(entry.at("material")!=upper,"Colour expressions do not tint the paint draw");
    for(const auto &entry:binding.value("textureTransformBinds",J::array()))
      check(entry.at("material")!=upper,"Texture expressions do not move the paint draw");
  }
  const auto &original_color=happy.at("materialColorBinds")[0].at("targetValue");
  const auto &remapped_happy=output_bindings.at("preset").at("happy");
  check(remapped_happy.at("materialColorBinds")[0].at("material")==lower&&
        remapped_happy.at("materialColorBinds")[0].at("targetValue")==original_color,
        "Original active colour binding survives on the lower source role");
  check(output_bindings.at("custom").at("alternate").at("textureTransformBinds")[0].at("scale")==J::array({0,.6f}),
        "A distinct singular UV expression remains authored on the lower source role");

  cgltf_options options{};
  cgltf_data *raw=nullptr;
  check(cgltf_parse_file(&options,output.c_str(),&raw)==cgltf_result_success,
        "Independent glTF parser recognizes the exported avatar");
  std::unique_ptr<cgltf_data,decltype(&cgltf_free)> parsed(raw,cgltf_free);
  check(cgltf_load_buffers(&options,raw,output.c_str())==cgltf_result_success&&
        cgltf_validate(raw)==cgltf_result_success,
        "Independent glTF validator accepts portable unlit geometry and materials");
  const auto &mesh=raw->meshes[mesh_slot];
  const auto *lower_rgb=cgltf_find_accessor(&mesh.primitives[0],cgltf_attribute_type_color,0);
  const auto *upper_rgb=cgltf_find_accessor(&mesh.primitives[1],cgltf_attribute_type_color,0);
  check(lower_rgb&&upper_rgb&&lower_rgb->type==cgltf_type_vec4&&upper_rgb->type==cgltf_type_vec3&&
        lower_rgb->count==upper_rgb->count&&mesh.primitives[0].indices==mesh.primitives[1].indices,
        "Independent accessors preserve geometry and decouple paint coverage from source vertex alpha");
  bool fractional_alpha=false;
  for(size_t vertex=0;vertex<upper_rgb->count;++vertex){
    std::array<float,4> source{},paint{0,0,0,1};
    check(cgltf_accessor_read_float(lower_rgb,vertex,source.data(),4)&&
          cgltf_accessor_read_float(upper_rgb,vertex,paint.data(),3)&&
          std::equal(source.begin(),source.begin()+3,paint.begin()),
          "Every renderer-visible vertex retains identical source RGB on both draws");
    fractional_alpha|=source[3]<1;
  }
  check(fractional_alpha,"The independent source contains real non-unit vertex alpha");
  const auto &picture=*mesh.primitives[1].material->pbr_metallic_roughness.base_color_texture.texture->image;
  const auto *pixels=cgltf_buffer_view_data(picture.buffer_view);
  const auto ink_image=decode_projection_png({pixels,picture.buffer_view->size});
  uint8_t minimum=255,maximum=0;
  for(size_t alpha=3;alpha<ink_image.rgba.size();alpha+=4){
    minimum=std::min(minimum,ink_image.rgba[alpha]);
    maximum=std::max(maximum,ink_image.rgba[alpha]);
  }
  check(minimum<maximum&&maximum>0,"Independent PNG decode observes spatially varying native paint coverage");
  auto exported_subset=live.snapshot();
  exported_subset.erase(empty.active().id);
  check(Scene::load(output).encode()==exported_subset.encode(),
        "The editable VRM reopens with exact source and paint for every exported object");

  expressions["custom"]["alternate"]["materialColorBinds"][0]["targetValue"][0]=1.25f;
  const auto invalid=out/"unlit-gamut-input.vrm";
  fixture.save(invalid);
  auto wide=prepare_authoring_import(empty,invalid,import_options);
  wide.scene.select(wide.objects.front());
  Document refusal;
  refusal.replace(std::move(wide.scene));
  end(refusal);
  ink(refusal);
  const auto original=refusal.copy_scene().encode();
  const auto previous=bytes(output);
  const auto failure=refuses([&]{refusal.export_asset(output,AssetExportFormat::EditableAvatar256).get();});
  check(failure=="VRM1 unlit expression colour exceeds the unit composition gamut"&&
        refusal.wait_asset()&&refusal.copy_scene().encode()==original&&bytes(output)==previous,
        "An out-of-gamut live expression retains the old portable file and exact native source");
}

void core_generated_frames(const std::filesystem::path &out) {
  auto fixture=core_source(false);
  for(auto &primitive:fixture.j["meshes"][0]["primitives"]){
    primitive["attributes"].erase("TANGENT");
    for(auto &target:primitive["targets"])target.erase("TANGENT");
  }
  for(auto &node:fixture.j["nodes"])
    if(node.contains("children")&&node["children"].empty())node.erase("children");
  Scene empty;
  empty.active().visible=false;
  const auto empty_data=empty.encode();
  auto &transform=fixture.j["materials"][0]["normalTexture"]["extensions"]["KHR_texture_transform"];
  transform["texCoord"]=1;
  fixture.j["materials"][0]["normalTexture"]["texCoord"]=0;
  constexpr float weight=.25f;
  for(bool mirror:{false,true})for(bool atlas:{false,true}){
    const float sx=mirror?-1.15f:1.15f,sy=.66f,rotation=-.31f;
    transform["scale"]={sx,sy};
    const auto label=std::string(mirror?"generated-mirror":"generated-rotate")+(atlas?"-atlas":"-original");
    const auto input=out/(label+".vrm");
    fixture.save(input);
    ImportAuthoringOptions options;options.prepare_texture_atlas=atlas;options.paint_resolution=64;
    auto imported=prepare_authoring_import(empty,input,options);
    const auto id=imported.objects.front();
    imported.scene.select(id);
    if(atlas){
      auto &object=imported.scene.object(id);
      auto paint=object.painted->snapshot();
      PaintSettings settings;settings.color={.15f,.85f,.3f,1};settings.front_only=false;
      Brush brush;brush.from=brush.to={0,0,0};brush.radius=1;brush.strength=1;
      paint->begin_stroke();
      paint->stamp(brush,settings);
      check(paint->end_stroke(),"A real native atlas stroke authors sparse paint before expression Apply");
      object.painted=std::move(paint);
    }
    const auto &baseline=imported.scene;
    const auto before=baseline.encode();
    const auto &initial=baseline.object(id);
    const auto &frame=*initial.material_bindings->normal_frames->pbr;
    check(frame.generated_tangents&&frame.generated_uv==1&&
          initial.appearance->asset->primitives.front().tangents.empty(),
          "Normal generation uses the actual KHR TEXCOORD override and no authored tangent payload");
    // The fixture has an axis-aligned square in object space with equally
    // spaced U and V. The first column of inverse(R*diag(sx,sy)) gives dP/du.
    const auto expected=normalized(Vec3{std::cos(rotation)/sx,-std::sin(rotation)/sy,0});
    for(const auto &t:frame.tangents){
      near(t[0],expected.x,"Generated tangent U follows the inverse sampler derivative");
      near(t[1],expected.y,"Generated tangent V uses rotation and anisotropic scale");
      near(t[2],0,"The analytic frame stays tangent to the source square");
      near(t[3],mirror?-1.f:1.f,"Reflection changes generated frame handedness");
    }
    check(frame.targets.size()==1&&!frame.targets.front().tangents.empty(),
          "The actual original shape key owns generated tangent deltas");
    Vrm1ExpressionWeights weights;weights.preset["happy"]=weight;
    const auto posed=bake_vrm1_expression_pose(baseline,id,weights);
    const auto &changed=posed.object(id);
    check(baseline.encode()==before&&changed.material_bindings==initial.material_bindings,
          "Texture expressions retain the fixed generated frame and leave the original scene intact");
    const auto orig=atlas?initial.painted->source_sampling_original():initial.painted;
    const auto now=atlas?changed.painted->source_sampling_original():changed.painted;
    check_texture(orig->source_maps()->normal,now->source_maps()->normal,rotation,weight);
    if(atlas){
      const auto &paint=initial.painted->layers().front();
      check(std::ranges::any_of(paint.tiles,[](const auto &tile){return bool(tile);})&&
            paint.tiles==changed.painted->layers().front().tiles,
            "Generated tangent expressions keep real atlas paint tiles");
    }
    const auto saved=out/(label+".dsculpt");
    posed.save(saved);
    const auto reopened=Scene::load(saved);
    check(reopened.encode()==posed.encode(),"Generated tangent frames and current samplers survive native reopen");
    if(atlas)continue;
    const auto portable=out/(label+"-applied.vrm");
    NativeAuthoringExport owner;
    owner.scene=std::make_shared<const Scene>(reopened);
    owner.allow_portable_fallback=true;
    VrmExportOptions export_options;export_options.paint_resolution=64;
    export_vrm_authoring(owner,portable,export_options);
    const auto portable_data=bytes(portable);
    cgltf_options consumer_options{};
    cgltf_data *parsed=nullptr;
    check(cgltf_parse(&consumer_options,portable_data.data(),portable_data.size(),&parsed)==cgltf_result_success&&
          parsed&&parsed->bin,"The independent glTF consumer reads the portable package");
    const std::unique_ptr<cgltf_data,decltype(&cgltf_free)> primary(parsed,cgltf_free);
    import_fixture::Fixture standalone;
    standalone.j=json(portable);
    check(standalone.j.at("extras").erase("domeSculptAuthoring")==1,
          "Standalone interop fixture bypasses the embedded native project");
    const auto *bin=static_cast<const uint8_t *>(primary->bin);
    standalone.bin.assign(bin,bin+primary->bin_size);
    const auto consumer=out/(label+"-portable-only.vrm");
    standalone.save(consumer);
    auto imported_again=prepare_authoring_import(empty,consumer,options);
    const auto &emitted_binding=imported_again.scene.object(imported_again.objects.front()).material_bindings;
    check(emitted_binding&&emitted_binding->normal_frames&&emitted_binding->normal_frames->pbr,
          "Portable-only import owns actual source normal attributes");
    const auto &emitted=*emitted_binding->normal_frames->pbr;
    check(!emitted.generated_tangents&&emitted.tangents==frame.tangents,
          "Portable export emits the exact generated frame as explicit glTF tangents");
    check(emitted.targets.size()==1&&emitted.targets[0].tangents.size()==frame.targets[0].tangents.size(),
          "Portable output retains the generated shape key tangent stream");
    for(size_t i=0;i<emitted.targets[0].tangents.size();++i)
      check(length(emitted.targets[0].tangents[i]-frame.targets[0].tangents[i])==0,
            "Portable shape key tangent deltas are exact after native reimport");

    auto forged=reopened.snapshot();
    auto binding=std::make_shared<SourceMaterialBindings>(*forged.object(id).material_bindings);
    auto frames=std::make_shared<NormalFrameSet>(*binding->normal_frames);
    auto foreign=std::make_shared<NormalFrames>(*frames->pbr);
    for(auto &t:foreign->tangents){t[0]=0;t[1]=1;}
    frames->pbr=foreign;binding->normal_frames=frames;
    forged.object(id).material_bindings=binding;
    const auto native_bytes=forged.encode();
    const auto old_output=bytes(portable);
    owner.scene=std::make_shared<const Scene>(std::move(forged));
    const auto error=refuses([&]{export_vrm_authoring(owner,portable,export_options);});
    check(error.find("generated normal frame differs from source replay")!=std::string::npos&&
          bytes(portable)==old_output&&owner.scene->encode()==native_bytes,
          "A forged generated frame refuses export and preserves the native scene and existing file");
  }
  transform["scale"]={0,1};
  const auto singular=out/"generated-singular.vrm";
  fixture.save(singular);
  const auto import_error=refuses([&]{prepare_authoring_import(empty,singular,{});});
  check(import_error.find("Generated core normal tangents require nonsingular")!=std::string::npos&&
        empty.encode()==empty_data,"A collapsed normal sampler cannot generate a tangent and refuses atomically");
}

void core_glb_normal_frames(const std::filesystem::path &out) {
  Scene empty;
  empty.active().visible=false;
  for(bool generated:{false,true})for(bool atlas:{false,true}){
    auto fixture=import_fixture::skinned(false);
    fixture.j["materials"]=core_source(false).j.at("materials");
    fixture.j["extensionsUsed"]={"KHR_texture_transform","KHR_materials_emissive_strength"};
    const int coordinate=generated?2:1;
    for(auto &primitive:fixture.j["meshes"][0]["primitives"]){
      if(!generated)continue;
      primitive["attributes"].erase("TANGENT");
      const auto extra=primitive["attributes"]["TEXCOORD_1"];
      primitive["attributes"]["TEXCOORD_2"]=extra;
      for(auto &target:primitive["targets"])target.erase("TANGENT");
    }
    auto &normal=fixture.j["materials"][0]["normalTexture"];
    normal["texCoord"]=0;
    auto &transform=normal["extensions"]["KHR_texture_transform"];
    transform["texCoord"]=coordinate;
    if(generated)transform["scale"]={-1.15f,.66f};
    const auto label=std::string(generated?"core-glb-generated":"core-glb-authored")+(atlas?"-atlas":"-original");
    const auto input=out/(label+".glb");
    fixture.save(input);
    ImportAuthoringOptions import;import.prepare_texture_atlas=atlas;import.paint_resolution=64;
    auto source=prepare_authoring_import(empty,input,import);
    source.scene.select(source.objects.front());
    const auto &initial=source.scene.active();
    const auto &frame=*initial.material_bindings->normal_frames->pbr;
    const auto initial_bytes=source.scene.encode();
    check(frame.generated_tangents==generated&&(!generated||frame.generated_uv==uint32_t(coordinate)),
          "Core GLB import owns its authored or generated original normal frame");

    Document worker;worker.replace(source.scene);
    const auto output=out/(label+"-portable.glb");
    worker.export_asset(output,AssetExportFormat::Glb).get();
    end(worker);
    check(worker.copy_scene().encode()==initial_bytes,"The public standard GLB file worker leaves its source intact");
    const auto emitted=bytes(output);
    cgltf_data *parsed=nullptr;
    const cgltf_options options{};
    check(cgltf_parse(&options,emitted.data(),emitted.size(),&parsed)==cgltf_result_success&&parsed&&
          cgltf_validate(parsed)==cgltf_result_success,"Independent consumer validates the standard GLB");
    const std::unique_ptr<cgltf_data,decltype(&cgltf_free)> primary(parsed,cgltf_free);
    const auto json_view=json(output);
    check(!json_view.contains("extras")||
          !json_view.at("extras").contains("domeSculptAuthoring"),"Standard GLB carries standalone portable attributes");
    const auto &sampler=json_view.at("materials").at(0).at("normalTexture");
    check(sampler.at("texCoord")==coordinate&&sampler.at("scale")==normal.at("scale"),
          "Standard GLB preserves the actual effective normal UV set and intensity");
    const auto &st=sampler.at("extensions").at("KHR_texture_transform");
    const auto &old_st=transform;
    const auto expected_offset=old_st.at("offset").get<std::array<float,2>>();
    const auto actual_offset=st.at("offset").get<std::array<float,2>>();
    for(size_t k=0;k<2;++k)near(actual_offset[k],expected_offset[k],"The normal sampler offset remains authored");
    const auto scales=st.at("scale").get<std::array<double,2>>();
    const auto source_scales=old_st.at("scale").get<std::array<double,2>>();
    const auto angle=st.at("rotation").get<double>(),old_angle=old_st.at("rotation").get<double>();
    near(float(std::cos(angle)*scales[0]),float(std::cos(old_angle)*source_scales[0]),
         "Exported normal sampler retains the signed U derivative");
    near(float(std::sin(angle)*scales[1]),float(std::sin(old_angle)*source_scales[1]),
         "Exported normal sampler retains the signed V derivative");
    import.prepare_texture_atlas=false;
    const auto reimported=prepare_authoring_import(empty,output,import);
    const auto &from_portable=reimported.scene.object(reimported.objects.front()).material_bindings;
    check(from_portable&&from_portable->normal_frames&&from_portable->normal_frames->pbr,
          "Standalone GLB reimport restores a real normal frame owner");
    const auto &read=*from_portable->normal_frames->pbr;
    check(!read.generated_tangents&&read.tangents==frame.tangents,"Standalone GLB retains every explicit tangent and sign");
    check(read.targets.size()==frame.targets.size()&&read.targets[0].tangents.size()==frame.targets[0].tangents.size(),
          "Standalone GLB retains the complete morph tangent stream");
    for(size_t k=0;k<read.targets[0].tangents.size();++k)
      check(length(read.targets[0].tangents[k]-frame.targets[0].tangents[k])==0,"Standalone GLB retains exact morph tangent deltas");

    if(atlas){
      const auto editable=out/(label+"-editable.glb");
      worker.export_asset(editable,AssetExportFormat::EditableGlb256).get();
      end(worker);
      check(extract_native_authoring(bytes(editable)).value().encode()==initial_bytes,
            "Editable GLB keeps the exact original native project with the portable normal view");
      continue;
    }
    auto surfaces=source.scene.export_surfaces();
    auto forged=surfaces;
    auto textures=std::make_shared<TextureMaps>(*forged.front().textures);
    auto maps=std::make_shared<PaintSourceMaps>(*textures->source_maps);
    auto &coords=coordinate==1?maps->uv1:maps->extra_uv_sets.at(uint32_t(coordinate));
    auto foreign=std::make_shared<std::vector<std::array<float,2>>>(*coords);
    foreign->front()[0]+=.1f;
    coords=std::move(foreign);
    textures->source_maps=std::move(maps);
    forged.front().textures=std::move(textures);
    check(refuses([&]{export_glb(forged,output);}).find("coordinates differ from the original source")!=std::string::npos&&
          bytes(output)==emitted,"A changed normal UV stream refuses atomically");
    GltfExportOptions cancel;
    cancel.before_commit=[] {throw std::runtime_error("Test cancelled at portable commit");};
    check(refuses([&]{export_glb(surfaces,output,cancel);}).find("Test cancelled at portable commit")!=std::string::npos&&
          bytes(output)==emitted&&source.scene.encode()==initial_bytes,"Portable commit cancellation preserves the existing file and source scene");
  }
}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 auto fixture=metadata_source();auto &program=fixture.j["extensions"]["VRMC_vrm"]["expressions"];
 auto &happy=program["preset"]["happy"];happy["isBinary"]=false;
 happy["materialColorBinds"]={{{"material",0},{"type","color"},{"targetValue",{.4f,.5f,.6f,.7f}}},{{"material",0},{"type","shadeColor"},{"targetValue",{.2f,.3f,.7f,13}}},{{"material",0},{"type","emissionColor"},{"targetValue",{.2f,.1f,.3f,17}}},{{"material",0},{"type","matcapColor"},{"targetValue",{.9f,.3f,.2f,19}}},{{"material",0},{"type","rimColor"},{"targetValue",{.1f,.2f,.3f,23}}},{{"material",0},{"type","outlineColor"},{"targetValue",{.2f,.4f,.1f,29}}}};
 program["preset"]["blink"]={{"isBinary",false},{"morphTargetBinds",{{{"node",1},{"index",0},{"weight",.8f}}}}};
 program["preset"]["aa"]={{"isBinary",true},{"overrideMouth","block"},{"morphTargetBinds",{{{"node",2},{"index",0},{"weight",.2f}}}}};
 program["custom"]["ink"]={{"overrideMouth","blend"},{"textureTransformBinds",{{{"material",0}}}}};
 // Schema-authoritative omission is valid. Old parser used at(scale/offset).
 const auto omission=refuses([&]{detail::validate_vrm1_metadata(fixture.j);});check(omission.empty(),"Actual optional UV-expression scale/offset defaults validate without fabricating raw fields");
 Vrm1ExpressionWeights weights;weights.preset={{"happy",.25f},{"blink",.8f},{"aa",.5f}};weights.custom={{"ink",.2f}};
 const auto e=evaluate_vrm1_expressions(fixture.j,weights);
 near(e.effective.preset.at("blink"),.6f,"Blend suppresses actual blink by happy effective weight");near(e.effective.preset.at("aa"),0,"Binary input exactly half is zero and ignores same-category override");
 near(e.morphs.at({1,0}),.25f*.35f+.6f*.8f,"Original node/target morph sums match independent arithmetic");
 check(e.colors.size()==6&&e.textures.size()==2&&e.textures[1].scale==std::array<float,2>{1,1}&&e.textures[1].offset==std::array<float,2>{0,0},"All six color roles and schema-default UV targets stay source-indexed");
 auto binary=fixture.j;binary["extensions"]["VRMC_vrm"]["expressions"]["preset"]["blink"]["isBinary"]=true;
 weights.preset["blink"]=1;near(evaluate_vrm1_expressions(binary,weights).effective.preset.at("blink"),0,"Any attenuation fully suppresses a binary target");
 binary["extensions"]["VRMC_vrm"]["expressions"]["preset"]["happy"]["isBinary"]=true;weights.preset["happy"]=.5f;
 near(evaluate_vrm1_expressions(binary,weights).effective.preset.at("blink"),1,"Inactive binary override uses binary output rather than raw half-weight");
 weights.preset["happy"]=.501f;near(evaluate_vrm1_expressions(binary,weights).effective.preset.at("blink"),0,"Active binary provider suppresses procedural group");
 auto bad=weights;bad.preset["missing"]=1;check(!refuses([&]{evaluate_vrm1_expressions(fixture.j,bad);}).empty(),"Unknown source expression is not silently dropped");bad=weights;bad.custom["ink"]=std::numeric_limits<float>::quiet_NaN();check(!refuses([&]{evaluate_vrm1_expressions(fixture.j,bad);}).empty(),"Nonfinite expression input refuses");
 weights={};weights.preset={{"happy",.25f},{"blink",.8f}};weights.custom={{"ink",.2f}};
 const auto input=out/"source.vrm";fixture.save(input);Scene empty;empty.active().visible=false;ImportAuthoringOptions options;options.prepare_texture_atlas=true;options.paint_resolution=64;
 auto imported=prepare_authoring_import(empty,input,options);Document doc;doc.replace(imported.scene);doc.select_object(imported.objects.front());check(doc.wait_mesh(),"Actual V1 source import meshes");ink(doc);
 const auto baseline=doc.copy_scene();const auto baseline_json=baseline.encode();const auto selected=doc.copy_scene().active().id;
 const auto baked=bake_vrm1_expression_pose(baseline,selected,weights);const auto &initial=baseline.object(selected);const auto &current=baked.object(selected);
 check(baseline.encode()==baseline_json&&baked.encode()!=baseline_json,"Detached real expression pose changes native data without mutating baseline");
 check(current.appearance==initial.appearance&&current.shape_source==initial.shape_source&&current.rig_source==initial.rig_source&&current.painted->source_sampling_prepared()&&current.painted->valid_source_sampling_attachment(),"Original source/first-shape/rig owners and authentic current paint authority survive");
 for(size_t i=0;i<initial.painted->layers().size();++i){const auto &a=initial.painted->layers()[i],&b=current.painted->layers()[i];check(a.id==b.id&&a.name==b.name&&a.visible==b.visible&&a.opacity==b.opacity&&a.tiles==b.tiles&&a.pbr_tiles==b.pbr_tiles&&a.emission_tiles==b.emission_tiles,"All actual sparse paint tile owners/metadata stay exact, not flattened");}
 const auto &old=initial.material_bindings->material;const auto &now=current.material_bindings->material;
 for(size_t k=0;k<4;++k)near(now.parameters.base_color[k],old.parameters.base_color[k]+(.25f*(std::array<float,4>{.4f,.5f,.6f,.7f}[k]-old.parameters.base_color[k])),"Source-material color matches independent target-minus-initial formula");
 check(now.textures[size_t(MToonSlot::Matcap)].offset==old.textures[size_t(MToonSlot::Matcap)].offset&&now.textures[size_t(MToonSlot::Matcap)].scale==old.textures[size_t(MToonSlot::Matcap)].scale,"View-dependent MatCap never receives mesh-UV expression");
 for(size_t slot:{size_t(MToonSlot::Base),size_t(MToonSlot::Shade),size_t(MToonSlot::Shift)})for(size_t k=0;k<2;++k){near(now.textures[slot].scale[k],old.textures[slot].scale[k]*.55f+.25f*std::array<float,2>{.7f,.9f}[k]+.2f,"Each UV-accessed role uses its own initial scale");near(now.textures[slot].offset[k],old.textures[slot].offset[k]*.55f+.25f*std::array<float,2>{.2f,-.1f}[k],"Each UV-accessed role uses independent offset while rotation/coordinate stay authored");}
 const std::array<std::pair<Vec3,std::array<float,3>>,5> rgb{{{now.parameters.shade_color,{.2f,.3f,.7f}},{now.parameters.emission,{.2f,.1f,.3f}},{now.parameters.matcap_color,{.9f,.3f,.2f}},{now.parameters.rim_color,{.1f,.2f,.3f}},{now.parameters.outline_color,{.2f,.4f,.1f}}}};
 const std::array<Vec3,5> rgb_initial{old.parameters.shade_color,old.parameters.emission,old.parameters.matcap_color,old.parameters.rim_color,old.parameters.outline_color};
 for(size_t i=0;i<rgb.size();++i){const auto a=rgb_initial[i],b=rgb[i].first;const std::array<float,3> start{a.x,a.y,a.z},actual{b.x,b.y,b.z};for(size_t k=0;k<3;++k)near(actual[k],start[k]+.25f*(rgb[i].second[k]-start[k]),"All RGB roles use independent initial factors and ignore unused fourth target component");}
 const auto before_maps=initial.painted->source_sampling_original()->source_maps(),after_maps=current.painted->source_sampling_original()->source_maps();
 const auto a=before_maps->color.uv_transform,b=after_maps->color.uv_transform;
 // This oracle operates on original source columns and explicit numerical
 // targets, independent of the evaluator and production ST conversion helper.
 const double angle=.19;const std::array<float,6> matrix_expected{float(.55*a[0]+std::cos(angle)*(.25*.7+.2)),float(.55*a[1]-std::sin(angle)*(.25*.9+.2)),float(.55*a[2]+std::sin(angle)*(.25*.7+.2)),float(.55*a[3]+std::cos(angle)*(.25*.9+.2)),float(.55*a[4]+.25*.2),float(.55*a[5]-.25*.1)};
 for(size_t k=0;k<6;++k)near(b[k],matrix_expected[k],"Original source UV columns preserve rotation, per-role initial scale and explicit target offsets");
 check(after_maps->color.image==before_maps->color.image&&after_maps->color.texcoord==before_maps->color.texcoord&&after_maps->color.wrap_s==before_maps->color.wrap_s&&after_maps->color.min_filter==before_maps->color.min_filter&&current.material_bindings->normal_frames==initial.material_bindings->normal_frames,"Real original image/sampler/coordinate and retained normal frame owners stay exact");
 std::array<std::array<float,6>,5> no_changes{before_maps->color.uv_transform,before_maps->metal_rough.uv_transform,before_maps->emission.uv_transform,before_maps->normal.uv_transform,before_maps->occlusion.uv_transform};SourcePaintOptions tiny;tiny.max_working_bytes=1;
 check(!refuses([&]{detail::rebind_source_paint_transforms(*initial.painted,no_changes,tiny);}).empty()&&baseline.encode()==baseline_json,"Prospective parameter-edit budget refuses before duplicating paint/raster state");
 check(current.painted->mesh().uv==initial.painted->mesh().uv&&current.painted->source_sampling_vertices().size()==initial.painted->source_sampling_vertices().size()&&current.material_bindings->uv_origin==initial.material_bindings->uv_origin,"Writable coordinates, source remap and first source capsule stay exact");
 auto twice=prepare_authoring_import(baseline,input,options);const auto independent=bake_vrm1_expression_pose(twice.scene,selected,weights);
 for(auto id:twice.objects){const auto &before=twice.scene.object(id),&after=independent.object(id);check(before.appearance==after.appearance&&before.material_bindings==after.material_bindings&&before.deformation==after.deformation&&before.painted->encode()==after.painted->encode(),"Repeated imported source node/material numbers never join independent native instances");}
 auto with_normal=fixture;with_normal.j["materials"][0]["normalTexture"]={{"index",0}};const auto normal_file=out/"normal.vrm";with_normal.save(normal_file);auto normal_scene=prepare_authoring_import(empty,normal_file,options);const auto normal_before=normal_scene.scene.encode();
 Vrm1ExpressionWeights normal_weights;normal_weights.preset["happy"]=.25f;
 const auto normal_id=normal_scene.objects.front();
 const auto normal_pose=bake_vrm1_expression_pose(normal_scene.scene,normal_id,normal_weights);
 const auto &normal_original=normal_scene.scene.object(normal_id),&normal_current=normal_pose.object(normal_id);
 check_texture(normal_original.painted->source_sampling_original()->source_maps()->normal,
               normal_current.painted->source_sampling_original()->source_maps()->normal,0,.25f);
 check(normal_scene.scene.encode()==normal_before&&
       normal_current.material_bindings->normal_frames==normal_original.material_bindings->normal_frames,
       "Typed MToon normal texture expressions preserve the original fixed frame");
 const auto saved=out/"static.dsculpt";baked.save(saved);check(Scene::load(saved).encode()==baked.encode(),"Native Save/restart retains actual static source transforms and paints");
 const auto repeat=bake_vrm1_expression_pose(baseline,selected,weights);check(repeat.encode()==baked.encode(),"Repeated sampling of immutable baseline has no accumulated expression drift");
 auto removed=baseline.snapshot();auto &part=removed.object(selected);auto deform=std::make_shared<DeformationState>(*part.deformation);deform->morphs.reset();deform->morph_weights.clear();part.deformation=deform;auto binding=std::make_shared<SourceMaterialBindings>(*part.material_bindings);auto frames=std::make_shared<NormalFrameSet>(*binding->normal_frames);frames->current_targets.clear();binding->normal_frames=frames;part.material_bindings=binding;
 removed.validate();const auto removed_before=removed.encode();const auto missing=refuses([&]{bake_vrm1_expression_pose(removed,selected,weights);});check(missing.find("original morph target is absent")!=std::string::npos&&removed.encode()==removed_before,"A genuinely absent current source morph cannot silently drop its bound expression contribution");
 auto locked=baseline;locked.object(selected).locked=true;const auto locked_before=locked.encode();check(!refuses([&]{bake_vrm1_expression_pose(locked,selected,weights);}).empty()&&locked.encode()==locked_before,"Locked linked-part authority refuses atomically");
 auto wrong=baseline;auto path=wrong.group_path(wrong.object(selected).group);auto leaf=std::find_if(path.rbegin(),path.rend(),[](const auto &g){return bool(g.source_placement);});auto foreign=std::make_shared<SourceNodePlacement>(*leaf->source_placement);foreign->source_instance=std::numeric_limits<uint64_t>::max();wrong.group(leaf->id).source_placement=foreign;check(!refuses([&]{bake_vrm1_expression_pose(wrong,selected,weights);}).empty(),"Forged numerical source-instance identity cannot authorize linked edits");
 size_t polls=0;auto control=bake_vrm1_expression_pose(baseline,selected,weights,[&]{++polls;return false;});check(control.encode()==baked.encode()&&polls>10,"Real detached computation exposes bounded cancellation checkpoints");
 for(size_t boundary:{size_t(1),polls/2,polls}){size_t n=0;const auto error=refuses([&]{bake_vrm1_expression_pose(baseline,selected,weights,[&]{return ++n>=boundary;});});check(error.find("cancelled")!=std::string::npos&&baseline.encode()==baseline_json,"Early/middle/final real computation cancellation preserves exact native baseline");}
 auto operation=[=](Scene &s,const std::function<bool()> &cancel){s=bake_vrm1_expression_pose(s,selected,weights,cancel);};
 const auto ordered=out/"ordered.dsculpt";doc.edit_scene("Bake static VRM1 expression",operation,doc.begin_import());auto save=doc.save_async(ordered);save.get();end(doc);check(doc.copy_scene().encode()==baked.encode()&&Scene::load(ordered).encode()==baked.encode(),"Actual native worker and immediately queued Save publish the real static expression pose");
 doc.undo();end(doc);check(doc.copy_scene().encode()==baseline_json,"Static expression authoring has exactly one native Undo");doc.redo();end(doc);check(doc.copy_scene().encode()==baked.encode(),"Native Redo retains current expression factors and all source/paint owners");
 // Actual detached computation completes; gate pauses only before publication.
 std::promise<void> entered,release;auto released=release.get_future().share();doc.undo();end(doc);doc.save_async(out/"cancel.dsculpt").get();const auto old_cancel=bytes(out/"cancel.dsculpt");const auto cancel_undo=doc.frame().state->can_undo,cancel_redo=doc.frame().state->can_redo;doc.edit_scene("Cancel static VRM1 expression",[&](Scene&s,const auto&cancel){std::optional<Scene> computed;try{computed.emplace(bake_vrm1_expression_pose(s,selected,weights,cancel));}catch(...){entered.set_exception(std::current_exception());throw;}entered.set_value();released.wait();s=std::move(*computed);},doc.begin_import());
 auto entry=entered.get_future();check(entry.wait_for(std::chrono::seconds(120))==std::future_status::ready,"Actual expression computation completes before publication cancellation gate");entry.get();auto after=doc.save_async(out/"cancel.dsculpt");doc.cancel_background_work();release.set_value();const auto cancel_error=refuses([&]{after.get();});check(cancel_error=="Scene changed during Cancel static VRM1 expression"&&doc.wait_asset()&&doc.wait_mesh()&&doc.copy_scene().encode()==baseline_json&&bytes(out/"cancel.dsculpt")==old_cancel&&doc.frame().state->can_undo==cancel_undo&&doc.frame().state->can_redo==cancel_redo,"Actual native late Cancel refuses dependent Save and preserves exact Scene/prior file/history");doc.save_async(out/"cancel-retry.dsculpt").get();check(Scene::load(out/"cancel-retry.dsculpt").encode()==baseline_json,"Actual Save retry recovers after cancelled accepted expression endpoint");
 NativeAuthoringExport owner;owner.scene=std::make_shared<const Scene>(baked.snapshot());owner.allow_portable_fallback=true;VrmExportOptions export_options;export_options.paint_resolution=64;const auto error=refuses([&]{export_vrm_authoring(owner,out/"still-dynamic.vrm",export_options);});std::cout<<"STATIC_DYNAMIC_REFUSAL "<<error<<'\n';check(baked.active().material_bindings->material.parameters.alpha==MToonAlpha::Mask&&error=="VRM1 differing UV targets couple moving upper roles to fixed paint"&&!std::filesystem::exists(out/"still-dynamic.vrm")&&owner.scene->encode()==baked.encode(),"Incompatible moving UV targets and fixed paint refuse portable export atomically");
 core_expressions(out);
 core_unlit_live_paint(out);
 core_generated_frames(out);
 core_glb_normal_frames(out);
 std::cout<<"PASS "<<checks<<" static VRM1 expression checks polls="<<polls<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
