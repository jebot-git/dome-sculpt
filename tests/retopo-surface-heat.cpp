// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake-authoring.hpp"
#include "dome/document.hpp"
#include "dome/import.hpp"
#include "dome/native-authoring.hpp"
#include "dome/rig-edit.hpp"
#include "dome/rig-rest.hpp"
#include "dome/source-placement.hpp"
#include "native-surface-heat.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>

using namespace dome;
namespace {
size_t checks = 0;

void check(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
  ++checks;
  std::cout << "PASS " << message << std::endl;
}

void settled(Document &document) {
  if (!document.wait_asset() || !document.wait_mesh() || !document.frame().state->error.empty())
    throw std::runtime_error("Document worker: " + document.frame().state->error);
}

void drafted(Document &document) {
  if (!document.wait_retopology() || !document.frame().state->retopology ||
      !document.frame().state->retopology->error.empty())
    throw std::runtime_error("Retopology worker: " + document.frame().state->error);
}

Vec3 point(const AffineTransform &world, Vec3 p) {
  const auto q = world.point({p.x, p.y, p.z});
  return {float(q[0]), float(q[1]), float(q[2])};
}

void touch(Document &document, Vec3 p) {
  if (!document.begin_desktop()) throw std::runtime_error("Retopology gesture refused");
  Brush brush;
  brush.from = brush.to = p;
  brush.radius = .012f;
  document.stamp_desktop(brush);
  document.end_desktop();
  drafted(document);
}

Scene reference(bool affine) {
  Mesh mesh;
  mesh.positions = {{-.55f,-.55f,0},{.55f,-.55f,0},{.55f,.55f,0},{-.55f,.55f,0}};
  mesh.normals.assign(4, {0,0,1});
  mesh.uv = {{0,0},{1,0},{1,1},{0,1}};
  mesh.indices = {0,1,2,0,2,3};
  Scene scene;
  auto &object = scene.active();
  object.name = "Painted retopology reference";
  object.mesh_active = true;
  object.painted = std::make_shared<PaintedMesh>(mesh,64);
  object.painted->add_layer("Original painted detail");
  PaintSettings ink;
  ink.front_only = false;
  ink.color = {.14f,.64f,.27f,1};
  Brush brush;
  brush.from = brush.to = {-.12f,.06f,0};
  brush.normal = {0,0,1};
  brush.radius = .4f;
  brush.strength = 1;
  object.painted->begin_stroke();
  object.painted->stamp(brush,ink);
  check(object.painted->end_stroke(),"Original surface owns independent nonempty editable paint");
  if (affine) {
    object.affine_edit = std::make_shared<const NativeAffineEdit>(AffineTransform(
        std::array<double,16>{-1.2,.12,0,0,.27,.83,0,0,0,.19,1.1,0,.08,-.04,.05,1}));
  }
  DeformationState deformation(mesh);
  Bone root{"Root"};
  root.rest.translation = {-.22f,-.16f,0};
  root.length = .43f;
  Bone tip{"Tip"};
  tip.parent = 0;
  tip.rest.translation = {.44f,0,0};
  tip.length = .43f;
  deformation.skeleton = Skeleton{{root,tip}};
  deformation.skin = SkinWeights{};
  deformation.skin->vertices = {{{0,1}},{{1,1}},{{1,1}},{{0,1}}};
  scene.set_deformation(object.id,std::move(deformation));
  return scene;
}

Scene author_patch(bool affine,const std::filesystem::path &out) {
  const auto source = reference(affine);
  const auto donor = source.active().id;
  const auto world = source.world_matrix(donor);
  Document document;
  document.replace(source);
  settled(document);
  document.begin_retopology();
  drafted(document);
  std::array<Vec3,4> centers{{{-.34f,-.30f,0},{.34f,-.30f,0},{.34f,.30f,0},{-.34f,.30f,0}}};
  for (auto &p : centers) p = point(world,p);
  for (auto p : centers) touch(document,p);
  document.edit_retopology(RetopoAction::Face);
  drafted(document);
  touch(document,centers[0]);
  document.edit_retopology(RetopoAction::Pin);
  drafted(document);
  touch(document,centers[1]);
  document.edit_retopology(RetopoAction::Seam);
  drafted(document);
  const auto topology = document.frame().state->retopology->topology;
  check(topology->faces().size()==1 && topology->faces().begin()->second.vertices.size()==4 &&
        std::ranges::count_if(topology->vertices(),[](const auto &v){return v.second.pinned;})==1 &&
        std::ranges::count_if(topology->edges(),[](const auto &e){return e.second.seam;})==1,
        "The real edit worker owns a stable quad, pinned vertex and marked seam");
  document.end_retopology(true);
  document.save_async(out/"patch.dsculpt").get();
  settled(document);
  auto patched = document.copy_scene();
  const auto patch_id = patched.active().id;
  check(patch_id!=donor && !patched.object(donor).visible && patched.active().retopology &&
        patched.active().retopology->source==donor &&
        patched.active().retopology->topology.faces().begin()->first==topology->faces().begin()->first,
        "Apply publishes a separate editable patch and hides its original source");

  ObjectBakeSettings bake;
  bake.front_mm = 35;
  bake.back_mm = 35;
  bake.ao_samples = 2;
  bake.allow_backfaces = true;
  // A small world-space gap makes projection onto the original plane explicit.
  document.edit_scene("Place patch below original",[=](Scene &scene,const auto &) {
    scene.object(patch_id).transform.position.z -= .014f;
  });
  settled(document);
  document.bake_from_object(donor,bake);
  document.save_async(out/"before-bind.dsculpt").get();
  settled(document);
  auto baked = document.copy_scene();
  const auto &target = baked.active();
  check(target.painted->baked() && std::ranges::any_of(target.painted->baked()->coverage,
        [](uint8_t alpha){return alpha!=0;}) && target.bake_recipe &&
        target.bake_recipe->source.object==donor && target.retopology==patched.active().retopology,
        "Bake uses the actual hidden source and retains the independent quad owner");
  const auto &color = target.painted->baked()->maps.color.rgba;
  size_t green_pixels = 0;
  for (size_t pixel=0;pixel<color.size();pixel+=4)
    green_pixels += color[pixel+1]>color[pixel]+15;
  check(green_pixels>64,"The baked atlas includes the original green painted detail");
  check(baked.object(donor).painted->encode()==source.active().painted->encode() &&
        baked.object(donor).deformation==source.active().deformation,
        "Patch and Bake leave the original painted and rigged mesh exact");
  return baked;
}

void bind_patch(bool affine,const std::filesystem::path &out) {
  const auto authored = author_patch(affine,out);
  const auto source = Scene::load(out/"before-bind.dsculpt");
  const auto initial = source.encode();
  const auto &original = source.active();
  const auto target = original.id;
  const auto reference_id = original.retopology->source;
  check(initial==authored.encode(),"Cold open retains the complete baked patch and original rig");

  SkinAuthoringOptions options;
  options.reference = reference_id;
  options.mode = SkinAuthoringMode::SurfaceHeat;
  options.seed_distance = affine ? .65f : .3f;
  detail::NativeSurfaceHeatOptions bounds;
  bounds.seed_distance = options.seed_distance;
  const auto retention = detail::native_weight_retention(source,bounds.maximum_total-bounds.maximum_workspace);
  detail::native_weight_retention_impl::Counter ledger(2*1024*1024);
  ledger.retopology(original.retopology);
  const auto bytes = ledger.used;
  ledger.retopology(original.retopology);
  check(ledger.count==1 && ledger.used==bytes,"The patch owner is charged once across current and retained references");
  bool null_refused = false;
  try {
    ledger.retopology(std::shared_ptr<const RetopoPatch>(original.retopology,nullptr));
  } catch(const std::invalid_argument &) { null_refused = true; }
  check(null_refused,"A malformed owned-null topology reference is refused by the scan");

  const std::array selected{target};
  bounds.maximum_total = bounds.maximum_workspace+retention.bytes;
  auto request = detail::capture_native_surface_heat(source,selected,reference_id,bounds);
  check(request.matches(source),"The exact owning budget admits the cold retopology and Bake graph");
  --bounds.maximum_total;
  bool short_input = false;
  try { (void)detail::capture_native_surface_heat(source,selected,reference_id,bounds); }
  catch(const std::length_error &) { short_input = true; }
  check(short_input && source.encode()==initial,"A one-byte-short request refuses before staging and preserves the project");
  auto stale = source;
  stale.active().retopology = std::make_shared<const RetopoPatch>(*original.retopology);
  auto result = detail::prepare_native_surface_heat(request);
  bool stale_refused = false;
  try { detail::adopt_native_surface_heat(stale,request,std::move(result)); }
  catch(const std::runtime_error &) { stale_refused = true; }
  check(stale_refused && stale.encode()==initial,"A stale result cannot use a different topology owner with the same value");

  auto detached = source;
  bool cancelled = false;
  try {
    detail::adopt_native_surface_heat(detached,request,std::move(result),[&]{cancelled=true;return true;});
  } catch(const std::runtime_error &) {}
  check(cancelled && detached.encode()==initial,"Late cancellation preserves the exact source and patch before publication");
  check(author_skin(detached,target,options),"Surface Heat binds both joints on the actual generated mesh");
  const auto &solved = detached.active();
  check(solved.retopology==original.retopology && solved.rig_source->retopology==original.retopology &&
        solved.painted==original.painted && solved.bake_recipe==original.bake_recipe &&
        solved.deformation->skin->vertices.size()==solved.painted->mesh().positions.size(),
        "Binding retains the stable quad, raster and Bake recipe and skins the full atlas mesh");
  const auto &rows = solved.deformation->skin->vertices;
  check(rows.size()>2 && std::ranges::any_of(rows,[&](const auto &row){return row!=rows.front();}),
        "Different parts of the patch have distinct solved weights");

  Document document;
  document.open_async(out/"before-bind.dsculpt").get();
  settled(document);
  document.bind_skin(options);
  document.save_async(out/"bound.dsculpt").get();
  settled(document);
  const auto expected = detached.encode();
  check(document.copy_scene().encode()==expected && Scene::load(out/"bound.dsculpt").encode()==expected,
        "Document binding and ordered Save match detached solving and cold reopen");
  check(document.undo() && document.copy_scene().encode()==initial && !document.frame().state->can_undo &&
        document.redo() && document.copy_scene().encode()==expected,"Bind adds one exact Undo and Redo");
  settled(document);
  document.begin_retopology();
  drafted(document);
  check(document.frame().state->retopology->topology->faces().size()==1,
        "The bound mesh still reopens as an editable quad");
  document.end_retopology(false);
  check(document.copy_scene().encode()==expected,"Cancelling the reopened workspace preserves skin and paint");

  auto restore = Scene::load(out/"bound.dsculpt");
  restore_rig_source(restore,target,false);
  check(restore.encode()==initial,"Cold rig Restore recovers the complete unbound patch and Bake");

  const auto before_brush = document.copy_scene();
  WeightToolSettings weight_tool;
  weight_tool.enabled = true;
  weight_tool.linked = false;
  const auto &first_weight = before_brush.active().deformation->skin->vertices.front();
  float root_weight = 0;
  for (const auto &influence : first_weight)
    if (influence.joint==0) root_weight = influence.weight;
  document.weight_settings(weight_tool,root_weight<.5f?0:1);
  settled(document);
  Brush weight_brush;
  weight_brush.from = weight_brush.to = before_brush.active().painted->mesh().positions.front();
  weight_brush.radius = .08f;
  check(document.begin_desktop(),"Desktop weight painting accepts the bound editable patch");
  document.stamp_desktop(weight_brush);
  document.end_desktop();
  settled(document);
  const auto brushed = document.copy_scene();
  check(brushed.active().deformation->skin->vertices.front()!=first_weight &&
        brushed.active().retopology==before_brush.active().retopology &&
        brushed.active().painted->baked()==before_brush.active().painted->baked(),
        "Weight painting changes a real row and retains the original topology and Bake owners");
  document.weight_settings(WeightToolSettings{});

  auto pose = document.copy_scene();
  RigEdit edit;
  edit.linked_pose = false;
  edit.joint = JointAdjustment{1,{0,15,0},{0,0,.06f}};
  edit_rig(pose,target,edit);
  document.replace(pose);
  settled(document);
  const auto posed = document.copy_scene().encode();
  document.save_async(out/"posed.dsculpt").get();
  document.export_asset(out/"standard.glb",AssetExportFormat::Glb).get();
  document.export_asset(out/"editable.glb",AssetExportFormat::EditableGlb512).get();
  ImportLimits limits;
  limits.mode = ImportMode::PreserveGltf;
  const auto portable = import_mesh(out/"standard.glb",limits);
  check(portable.primitives.size()==1 && portable.skins.size()==1 &&
        portable.skins.front().joints.size()==2 && portable.primitives.front().mesh.indices.size()==6 &&
        !portable.primitives.front().influences.empty(),
        "Portable GLB has the atlas triangles, weights and both joints");
  std::ifstream editable(out/"editable.glb",std::ios::binary);
  const std::vector<uint8_t> contents{std::istreambuf_iterator<char>(editable),{}};
  auto reopened = extract_native_authoring(contents);
  check(reopened && reopened->encode()==posed && document.copy_scene().encode()==posed,
        "Editable GLB recovers the complete posed project and its original editing history");
}
}

int main(int argc,char **argv) try {
  if (argc!=3 || (std::string_view(argv[2])!="plain" && std::string_view(argv[2])!="affine")) return 2;
  const std::filesystem::path out = argv[1];
  std::filesystem::create_directories(out);
  setenv("XDG_STATE_HOME",(out/"state").c_str(),1);
  setenv("XDG_CONFIG_HOME",(out/"config").c_str(),1);
  bind_patch(std::string_view(argv[2])=="affine",out);
  std::cout << "PASS " << checks << " retopology Surface Heat controls\n";
} catch(const std::exception &error) {
  std::cerr << "FAIL " << error.what() << '\n';
  return 1;
}
