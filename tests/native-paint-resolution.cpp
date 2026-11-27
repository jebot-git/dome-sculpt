// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creation.hpp"
#include "dome/bake-authoring.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/material-bindings.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/texture-set-resolution.hpp"
#include "texture-set-resample.hpp"
#include "import-controller-driver.hpp"
#include "native-paint-sampling.hpp"
#include "paint-grid-filter.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

namespace {
using namespace dome;

void check(bool passed,const char *message) {
 if(!passed)throw std::runtime_error(message);
 std::cout << "PASS " << message << std::endl;
}

bool refuses(auto action) {
 try { action(); } catch(const std::exception &) { return true; }
 return false;
}

void ready(Document &doc) {
 if(!doc.wait_asset(std::chrono::seconds(180)))
  throw std::runtime_error("Paint-resolution worker timed out");
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(180);
 do {
  if(const auto state=doc.frame().state;!state->error.empty())
   throw std::runtime_error(state->error);
  if(doc.wait_mesh(std::chrono::milliseconds(250)))
   return;
 } while(std::chrono::steady_clock::now()<deadline);
 throw std::runtime_error("Paint-resolution surface timed out");
}

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
  auto small=detail::native_texture_grid_limits();
  small.maximum_bytes=1;
  check(refuses([&]{resample_native_texture_grid(denied,id,{1024},small);})&&denied.encode()==before,
        "Insufficient prospective raster memory refuses without changing the complete source scene");

  Document doc;
  doc.replace(scene);
  ready(doc);
  check(doc.can_resize_native_paint(),"An empty source-authored paint grid enables the native resolution control");
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
    auto limits = detail::native_texture_grid_limits();
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
    check(doc.can_resize_native_paint(),
          "A painted original grid enables the native resolution control");
    ImportDriver hand(doc);
    hand.open();
    hand.sector(9);
    hand.sector(1);
    hand.sector(6);
    check(doc.frame().state->controls.parameter == VrParameter::TextureResolution,
          "The shared XR texture-set control opens for painted source data");
    hand.ray(.4f);
    hand.panel_button(false);
    check(!doc.frame().state->asset_busy && doc.copy_scene().encode() == before,
          "Cancelling the controller resize keeps the scene and worker unchanged");
    if (!doc.frame().state->menu)
      hand.open();
    if (doc.frame().state->menu_page != 31) {
      hand.sector(9);
      hand.sector(1);
    }
    hand.sector(6);
    check(doc.frame().state->controls.parameter == VrParameter::TextureResolution,
          "The controller can reopen the size picker after cancellation");
    hand.ray(.4f);
    hand.panel_button(true);
    scene.save(root / (std::string(baseline) + "-painted128.dsculpt"));
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

    if (std::string_view(baseline) == "vroid-beta-female") {
      TextureSetEdit resize;
      resize.action = TextureSetAction::Resolution;
      resize.resolution = 1024;
      const auto source = doc.frame().state;
      doc.edit_texture_set(resize, {source->generation, source->revision,
                                   source->active_object, source->edit_authority});
      ready(doc);
      const auto large = doc.copy_scene();
      const auto &larger = *large.object(id).painted;
      bool exact_cells = larger.resolution() == 1024;
      for (uint32_t y = 0; y < larger.resolution(); ++y)
        for (uint32_t x = 0; x < larger.resolution(); ++x)
          exact_cells &= larger.pixel(hidden, x, y) == paint->pixel(hidden, x / 8, y / 8);
      check(exact_cells && larger.source_maps() == paint->source_maps(),
            "The desktop command enlarges sparse painted source layers to 1024");
      check(doc.undo() && doc.copy_scene().encode() == after.encode(),
            "One undo restores the previous native paint size");
    }
  }
}

void resize_physical_native_grid(const std::filesystem::path &root) {
  Scene scene;
  Mesh mesh;
  mesh.positions = {{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};
  mesh.normals.assign(4, {0,0,1});
  mesh.uv = {{{0,0}},{{1,0}},{{1,1}},{{0,1}}};
  mesh.indices = {0,1,2,0,2,3};
  auto paint = std::make_shared<PaintedMesh>(std::move(mesh), 64);
  const auto layer = paint->active_layer();
  scene.active().painted = paint;
  scene.active().mesh_active = true;
  const auto id = scene.active().id;
  Brush brush;
  brush.from = brush.to = {0,0,0};
  brush.radius = .5f;
  PaintSettings ink;
  ink.front_only = false;
  ink.channel = PaintChannel::Emission;
  ink.emission_units = PaintEmissionUnits::Physical;
  ink.emission = {9500.f, .0125f, 57.f};
  ink.flow = .0001f;
  paint->begin_stroke();
  paint->stamp(brush, ink);
  check(paint->end_stroke() && paint->has_physical_emission(),
        "Native grid carries true physical emission with sub-byte coverage");
  const auto center = paint->emission_pixel(layer, 32, 32);
  check(center.coverage > 0 && center.coverage < 1.f/255 &&
            center.physical[0] > 9000 && paint->pixel(layer, 32, 32).data[12] == 0,
        "Physical coverage and radiance are independent of the legacy byte");
  paint->layer_properties("Hidden HDR", false, .64f);

  Document doc;
  doc.replace(scene);
  ready(doc);
  TextureSetEdit resize;
  resize.action = TextureSetAction::Resolution;
  resize.resolution = 128;
  doc.edit_texture_set(resize);
  const auto file = root / "native-physical128.dsculpt";
  doc.save_async(file).get();
  ready(doc);
  const auto after = doc.copy_scene();
  const auto &larger = *after.object(id).painted;
  bool exact = larger.resolution() == 128 && larger.has_physical_emission();
  for (uint32_t y = 0; y < larger.resolution(); ++y)
    for (uint32_t x = 0; x < larger.resolution(); ++x)
      exact &= larger.emission_pixel(layer, x, y) == paint->emission_pixel(layer, x/2, y/2);
  check(exact && !larger.layers().front().visible && larger.layers().front().opacity == .64f,
        "The new native grid retains every hidden FLOAT sample and its layer state");
  check(Scene::load(file).encode() == after.encode() &&
            doc.undo() && doc.copy_scene().encode() == scene.encode() &&
            doc.redo() && doc.copy_scene().encode() == after.encode(),
        "Physical paint survives ordered native Save, reopen, Undo and Redo");
}

void filter_binary64_rounding(){
  using namespace dome::paint_grid;
  // A genuine blended layer sample whose exact quotient lies just below a
  // binary32 rounding boundary. The legacy and FLOAT lanes share the filter.
  constexpr std::array<uint32_t,4> legacy{0x3cb1a100,0x3cb181c0,0x3cb15819,0x3cb138db};
  constexpr std::array<uint32_t,4> coverage{0x3f1a7ea4,0x3f1a896a,0x3f1a97cb,0x3f1aa296};
  constexpr std::array<uint8_t,4> masks{126,125,125,124};
  std::array<Pixel,4> input{};
  std::array<Pixel,1> output{};
  for(size_t i=0;i<input.size();++i) {
    input[i].emission[1]=std::bit_cast<uint64_t>(double(std::bit_cast<float>(legacy[i])));
    input[i].emission[6]=std::bit_cast<uint64_t>(double(std::bit_cast<float>(coverage[i])));
    input[i].mask=masks[i];
  }
  Tables tables{};
  for(unsigned i=0;i<256;++i)tables.unit[i]=std::bit_cast<uint64_t>(i/255.);
  for(unsigned i=0;i<255;++i)
    tables.unit_threshold[i]=std::bit_cast<uint64_t>((i+.5)/255.);
  Layout layout;
  layout.floats=FloatPlane::UnassociatedPaint;
  Filter backend(tables);
  backend.run({128,64,{0,0,2,2},{0,0,1,1}},layout,input,output,{});
  check(output[0].emission[1]==0x3f962d9d2f9f3b9eull,
        "The selected kernel keeps the exact binary64 quotient before FLOAT storage");
  layout.floats=FloatPlane::Absent;
  layout.mask=true;
  backend.run({64,128,{10,1,2,2},{22,4,1,1}},layout,input,output,{});
  check(output[0].mask==125,
        "Bilinear mask weights retain CPU multiply-before-divide rounding");
}

void resize_baked_native_grid(const std::filesystem::path &root,bool missing_device=false) {
  if(!missing_device)filter_binary64_rounding();
  const auto make_fixture=[](bool physical,bool sharp){
    Scene scene;
    Mesh mesh;
    mesh.positions={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};
    mesh.normals.assign(4,{0,0,1});
    mesh.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};
    mesh.indices={0,1,2,0,2,3};
    auto paint=std::make_shared<PaintedMesh>(std::move(mesh),128);
    BakedTextures bake;
    const auto image=[](){return Image{128,128,std::vector<uint8_t>(128*128*4,255)};};
    bake.maps.color=image();
    bake.maps.metal_rough=image();
    bake.maps.normal=image();
    if(!physical)bake.maps.emission=image();
    bake.coverage.resize(128*128);
    for(size_t i=0;i<bake.coverage.size();++i){
      const bool odd=i%2;
      bake.coverage[i]=sharp?255:odd?144:112;
      bake.maps.color.rgba[i*4]=odd?123:86;
      bake.maps.color.rgba[i*4+3]=odd?188:155;
      bake.maps.metal_rough.rgba[i*4+1]=odd?153:100;
      bake.maps.normal.rgba[i*4]=sharp?(odd?240:15):(odd?162:144);
      bake.maps.normal.rgba[i*4+1]=128;
      bake.maps.normal.rgba[i*4+2]=sharp?186:246;
      if(!physical)bake.maps.emission.rgba[i*4]=odd?102:78;
    }
    if(physical)bake.emission_field=BakedEmissionField::generate(128,[](size_t i){
      const float coverage=i%2?.0015f:.001f;
      return BakedEmissionSample{{.2f*coverage,.05f*coverage,0},
          {8000*coverage,50*coverage,.12f*coverage},coverage};
    });
    paint->install_bake(std::move(bake));
    scene.active().mesh_active=true;
    scene.active().painted=std::move(paint);
    return scene;
  };
  for(bool physical:{false,true}){
    auto scene=make_fixture(physical,false);
    const auto id=scene.active().id;
    const auto before=scene.encode();
    if(missing_device){
      const auto owner=scene.active().painted;
      check(compute_backend()!="CPU"&&
            refuses([&]{resample_native_texture_grid(scene,id,{64});})&&
            scene.encode()==before&&scene.active().painted==owner,
            "The selected unavailable GPU fails Bake reduction without publishing a replacement");
      continue;
    }
    const auto source=scene.active().painted;
    auto no_loss=TextureSetResolutionOptions{.resolution=64,.maximum_component_error=0,.maximum_rms_error=0};
    check(refuses([&]{resample_native_texture_grid(scene,id,no_loss);})&&scene.encode()==before,
          "A zero-tolerance Bake reduction refuses and keeps the exact original field");
    const auto result=resample_native_texture_grid(scene,id,{64});
    const auto &bake=*scene.active().painted->baked();
    const auto &prior=*source->baked();
    check(result.changed&&scene.active().painted->mesh_owner()==source->mesh_owner()&&
          bake.coverage.front()==128&&bake.maps.normal.width==64,
          "A reduced Bake keeps the mesh and reconstructs the linear normal and coverage grids");
    constexpr float even=7.f/16,odd=9.f/16;
    const auto blend=[](uint8_t a,uint8_t b){return (even*a+odd*b)/255.f;};
    const float wanted=even*srgb_to_linear(prior.maps.color.rgba[0]/255.f)+
                       odd*srgb_to_linear(prior.maps.color.rgba[4]/255.f);
    check(std::abs(srgb_to_linear(bake.maps.color.rgba[0]/255.f)-wanted)<.004f&&
          std::abs(bake.maps.color.rgba[3]/255.f-blend(prior.maps.color.rgba[3],prior.maps.color.rgba[7]))<.002f&&
          std::abs(bake.maps.metal_rough.rgba[1]/255.f-blend(100,153))<.002f&&
          std::abs(bake.maps.normal.rgba[0]/255.f-blend(144,162))<.002f,
          "Independent coverage-weighted reads agree for sRGB, alpha, ORM and the tangent vector");
    if(physical){
      const auto emission=bake.emission_field->texel(0);
      check(bake.maps.emission.rgba.empty()&&std::abs(emission.coverage-.00125f)<1e-9f&&emission.coverage<1.f/255&&
            std::abs(emission.absolute[0]-10.f)<1e-5f&&std::abs(emission.legacy[0]-.00025f)<1e-9f,
            "FLOAT reduction retains sub-byte coverage and both premultiplied emission planes");
    }else{
      const float wanted_emission=even*srgb_to_linear(78/255.f)+odd*srgb_to_linear(102/255.f);
      check(!bake.emission_field&&std::abs(srgb_to_linear(bake.maps.emission.rgba[0]/255.f)-wanted_emission)<.004f,
            "Legacy Bake emission remains an sRGB image with the correct coverage-weighted value");
    }
    const auto path=root/(physical?"native-float-bake64.dsculpt":"native-ldr-bake64.dsculpt");
    scene.save(path);
    check(Scene::load(path).encode()==scene.encode(),"Cold open retains the reduced Bake and coverage");
  }
  if(!missing_device){
    auto sharp=make_fixture(false,true);
    const auto before=sharp.encode();
    check(refuses([&]{resample_native_texture_grid(sharp,sharp.active().id,{64});})&&sharp.encode()==before,
          "The default quality guard refuses a reduction that would erase sharp normal detail");
  }
}

void resize_retained_native_grid(const std::filesystem::path &root) {
  Mesh mesh;
  mesh.positions={{-.85f,-.75f,0},{.85f,-.75f,0},{.85f,.75f,0},{-.85f,.75f,0}};
  mesh.normals.assign(4,{0,0,1});
  mesh.uv={{{.05f,.05f}},{{.95f,.05f}},{{.95f,.95f}},{{.05f,.95f}}};
  mesh.indices={0,1,2,0,2,3};
  Scene scene;
  const auto id=scene.active().id;
  auto paint=std::make_shared<PaintedMesh>(mesh,128);
  scene.active().mesh_active=true;
  scene.active().painted=paint;
  PaintSettings ink;
  ink.color={.74f,.21f,.13f,.87f};
  ink.front_only=false;
  Brush brush;
  brush.radius=.65f;
  paint->begin_stroke();
  paint->stamp(brush,ink);
  ink.channel=PaintChannel::Mask;
  ink.mask=.44f;
  paint->stamp(brush,ink);
  check(paint->end_stroke(),"Original grid carries authored colour and an independent mask");
  paint->add_layer("Hidden HDR");
  ink.channel=PaintChannel::Emission;
  ink.emission_units=PaintEmissionUnits::Physical;
  ink.emission={2000,.018f,4};
  ink.flow=.0001f;
  paint->begin_stroke();
  paint->stamp(brush,ink);
  check(paint->end_stroke(),"Original grid carries hidden sub-byte FLOAT paint");
  paint->layer_properties("Hidden HDR",false,.71f);
  const auto donor=scene.add("Hidden normal and HDR donor",Volume{});
  for(auto &p:mesh.positions)p.z=.018f;
  mesh.normals.assign(4,normalized(Vec3{.25f,-.19f,1}));
  auto &source=scene.object(donor);
  source.painted=std::make_shared<PaintedMesh>(std::move(mesh),64);
  source.mesh_active=true;
  source.visible=false;
  source.material.emissive={2.9f,.17f,.05f};
  scene.select(id);
  const auto initial=scene.encode();
  Document doc;
  doc.replace(scene);
  ready(doc);
  const auto edit_uv=[&](float amount) {
    doc.begin_uv();
    check(doc.wait_uv()&&doc.frame().state->uv->paint_sampling_available,"UV editor offers the actual Keep Paint route");
    if(!doc.frame().state->uv->retain_paint_sampling)doc.edit_uv(UvAction::RetainPaint);
    doc.edit_uv(UvAction::Pick,.5f,.5f);
    doc.edit_uv(UvAction::Scale,amount);
    check(doc.wait_uv()&&doc.frame().state->uv->valid,"The edited display UV atlas validates");
    doc.end_uv(true);
    ready(doc);
  };
  edit_uv(.6f);
  const auto retained=doc.copy_scene();
  const auto &uv=retained.active();
  check(uv.material_bindings&&uv.material_bindings->native_paint_sampling&&uv.painted->mesh().uv!=paint->mesh().uv&&
        uv.painted->paint_sampling_domain()->basis()->uv==paint->mesh().uv,
        "Writable UVs remain on the original atlas after the authored display UV edit");
  auto edited=retained;
  check(resample_native_texture_grid(edited,id,{256}).changed,"A retained writable atlas can enlarge before Bake");
  edited.save(root/"unbaked256.dsculpt");
  doc.open(root/"unbaked256.dsculpt");
  ready(doc);
  edit_uv(.85f);
  const auto second=doc.copy_scene();
  const auto second_owner=second.active().material_bindings->native_paint_sampling;
  check(second.active().painted->resolution()==256&&second_owner->history_depth()==2&&
        second_owner->before_paint()->resolution()==256&&second_owner->original_paint()->resolution()==128,
        "A second UV edit captures the resized grid and keeps the original grid in its lineage");
  doc.save(root/"second-uv.dsculpt");
  check(Scene::load(root/"second-uv.dsculpt").encode()==second.encode(),"The mixed-resolution UV lineage replays on cold open");
  detail::restore_native_paint(edited,id);
  check(edited.encode()==initial,"Restore UV and paint recovers the exact original scene and grid size");

  doc.replace(retained);
  ready(doc);
  ObjectBakeSettings settings;
  settings.ao_samples=0;
  settings.front_mm=settings.back_mm=50;
  auto canonical=retained;
  check(bake_object(canonical,donor,settings).projected>0&&
        canonical.active().painted->baked()->normal_interpretation==BakedNormalInterpretation::CanonicalMikk,
        "Native Bake before a shape key uses its kept canonical tangent grid");
  check(resample_native_texture_grid(canonical,id,{256}).changed&&
        canonical.active().painted->baked()->maps.normal.width==256,
        "Native UV regridding also preserves an ordinary canonical Bake");
  ShapeKeyEdit key;
  key.name="Retained normal frame";
  doc.author_shape(key);
  ready(doc);
  doc.bake_from_object(donor,settings);
  doc.save_async(root/"baked128.dsculpt").get();
  ready(doc);
  const auto before=doc.copy_scene();
  const auto &old=before.active();
  const auto old_bake=old.painted->baked();
  check(old_bake&&old_bake->emission_field&&old.material_bindings->native_paint_frames&&
        old_bake->normal_interpretation==BakedNormalInterpretation::NativeAttributes&&old.bake_recipe,
        "Actual Bake on the kept atlas captures authored normal frames, FLOAT light and a recipe");
  auto cancelled=before;
  auto limits=detail::native_texture_grid_limits();
  size_t polls=0;
  limits.cancelled=[&]{return ++polls>300;};
  check(refuses([&]{resample_native_texture_grid(cancelled,id,{256},limits);})&&cancelled.encode()==before.encode(),
        "Cancellation during retained-grid work discards the private candidate and keeps owners");
  check(doc.can_resize_native_paint(),"The same DomeD Paint resolution command admits the retained Bake");
  ImportDriver hand(doc);
  hand.open();hand.sector(9);hand.sector(1);hand.sector(6);
  check(doc.frame().state->controls.parameter==VrParameter::TextureResolution,"XR opens the retained Bake grid control");
  hand.ray(.4f);hand.panel_button(true);
  doc.save_async(root/"baked256.dsculpt").get();
  ready(doc);
  const auto grown=doc.copy_scene();
  const auto &current=grown.active();
  const auto new_bake=current.painted->baked();
  check(new_bake&&new_bake->emission_field&&current.painted->resolution()==256&&
        current.material_bindings==old.material_bindings&&current.bake_recipe==old.bake_recipe&&
        current.painted->paint_sampling_attachment()==old.painted->paint_sampling_attachment()&&
        current.painted->mesh_owner()==old.painted->mesh_owner()&&new_bake->normal_interpretation==old_bake->normal_interpretation,
        "Resize keeps the Bake and sampling owners and the authored display geometry");
  bool exact=true;
  for(uint32_t y=0;y<256&&exact;++y)for(uint32_t x=0;x<256&&exact;++x){
    const size_t at=size_t(y)*256+x, prior=size_t(y/2)*128+x/2;
    exact=new_bake->coverage[at]==old_bake->coverage[prior]&&
      std::equal(new_bake->maps.normal.rgba.data()+4*at,new_bake->maps.normal.rgba.data()+4*at+4,
                 old_bake->maps.normal.rgba.data()+4*prior);
    const auto a=old_bake->emission_field->texel(prior),b=new_bake->emission_field->texel(at);
    exact&=a.coverage==b.coverage&&a.absolute==b.absolute&&a.legacy==b.legacy;
    for(const auto &layer:old.painted->layers())
      exact&=old.painted->pixel(layer.id,x/2,y/2)==current.painted->pixel(layer.id,x,y)&&
             old.painted->emission_pixel(layer.id,x/2,y/2)==current.painted->emission_pixel(layer.id,x,y);
  }
  check(exact,"Every authored cell, baked normal and FLOAT value survives enlargement");
  auto reduced=grown;
  check(resample_native_texture_grid(reduced,id,{128}).changed,"The kept native attribute grid reduces to its original size");
  const auto reduced_bake=reduced.active().painted->baked();
  check(reduced_bake&&reduced_bake->normal_interpretation==BakedNormalInterpretation::NativeAttributes&&
        reduced_bake->maps.normal.rgba==old_bake->maps.normal.rgba&&reduced_bake->coverage==old_bake->coverage,
        "Exact-cell reduction recovers the original authored normal samples");
  const auto saved=grown.encode();
  check(Scene::load(root/"baked256.dsculpt").encode()==saved&&doc.undo()&&doc.copy_scene().encode()==before.encode()&&
        doc.redo()&&doc.copy_scene().encode()==saved,"Ordered Save, cold Open and one Undo/Redo retain both resolutions");
  ready(doc);
  doc.export_asset(root/"resized-standard.glb",AssetExportFormat::Glb).get();
  doc.export_asset(root/"resized-editable.glb",AssetExportFormat::EditableGlb256).get();
  check(Scene::load(root/"resized-editable.glb").encode()==saved,"Editable GLB reopens the full retained UV authoring scene");
  doc.open(root/"baked256.dsculpt");
  ready(doc);
  doc.bake_from_object(donor,settings);
  ready(doc);
  const auto rebaked=doc.copy_scene();
  check(rebaked.active().painted->resolution()==256&&rebaked.active().painted->baked()->maps.normal.width==256,
        "Bake after cold reopen uses the new writable grid and retained native normal frame");
}

} // namespace

int main(int argc,char **argv) try {
 if(argc!=3)throw std::invalid_argument("Expected a fixture kind and output directory");
 const std::filesystem::path root=argv[2];
 std::filesystem::create_directories(root);
 if(std::string_view(argv[1])=="empty")
  resize_source_grid(root);
 else if(std::string_view(argv[1])=="painted") {
  resize_painted_source_grid(root);
  resize_physical_native_grid(root);
 } else if(std::string_view(argv[1])=="baked") {
  resize_baked_native_grid(root);
 } else if(std::string_view(argv[1])=="missing-bake-device") {
  resize_baked_native_grid(root,true);
 } else if(std::string_view(argv[1])=="retained") {
  resize_retained_native_grid(root);
 } else {
  throw std::invalid_argument("Unknown paint resolution fixture");
 }
 return 0;
} catch(const std::exception &error) {
 std::cerr << "FAIL " << error.what() << '\n';
 return 1;
}
