// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake-authoring.hpp"
#include "dome/document.hpp"
#include "dome/import.hpp"
#include "dome/native-authoring.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/rig-edit.hpp"
#include "dome/rig-rest.hpp"
#include "dome/shapekey-authoring.hpp"
#include "dome/source-placement.hpp"
#include "native-surface-heat.hpp"
#include "texture-set-paint-access.hpp"
#include "texture-set-resample.hpp"
#include "affine-interaction.hpp"
#include "baked-normal-field.hpp"
#include "source-bake-normal.hpp"
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

std::shared_ptr<const RetopoView> move_patch(Document &document, bool affine) {
  document.begin_retopology();
  drafted(document);
  document.edit_retopology(RetopoAction::MoveMode);
  drafted(document);
  const auto start = document.frame().state->retopology;
  const auto &vertices = start->topology->vertices();
  const auto &[corner, vertex] = *std::ranges::find_if(vertices, [](const auto &v) { return !v.second.pinned; });
  const auto original_world = point(affine_similarity(start->transform), vertex.position);
  const auto toward_center = original_world + (affine ? Vec3{.04f,.02f,0} : Vec3{-.035f,.025f,0});
  check(document.begin_desktop(), "Desktop retopology admits the patch Move gesture");
  Brush drag;
  drag.from = drag.to = original_world;
  document.stamp_desktop(drag);
  drafted(document);
  drag.to = toward_center;
  document.stamp_desktop(drag);
  document.end_desktop();
  drafted(document);
  const auto draft = document.frame().state->retopology;
  check(length(draft->topology->vertices().at(corner).position-vertex.position)>.02f,
        "Controller-shared workspace changes a real patch vertex before Apply");
  return draft;
}

std::shared_ptr<const RetopoView> relocate_patch(Document &document,const Scene &before,
                                                Vec3 displacement,bool subdivide) {
  const auto &old=before.active();
  auto topology=old.retopology->topology;
  topology.transform(before.world_matrix(old.id));
  CanvasTransform move;
  move.position={displacement.x,displacement.y,displacement.z};
  topology.transform(affine_similarity(move));
  if(subdivide){
    const auto seam=std::ranges::find_if(topology.edges(),[](const auto &entry){return entry.second.seam;});
    const auto midpoint=topology.subdivide_edge(seam->first);
    check(topology.faces().size()==3&&topology.vertices().at(midpoint).split_edge[0],
          "A detached replacement has an actual subdivided edge and a complete prior corner lineage");
  }
  auto draft=std::make_shared<RetopoView>();
  draft->topology=std::make_shared<const RetopoMesh>(std::move(topology));
  const auto guide=old.retopology->source;
  document.edit_scene("Apply detached retopology",[=](Scene &scene,const auto &cancelled){
    publish_retopology(scene,*draft->topology,guide,draft->transform,cancelled);
  });
  return draft;
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

void reapply_patch(bool affine, const std::filesystem::path &out) {
  auto scene = author_patch(affine, out);
  const auto old_id = scene.active().id;
  const auto donor = scene.active().retopology->source;
  SkinAuthoringOptions skin;
  skin.reference = donor;
  skin.mode = SkinAuthoringMode::SurfaceHeat;
  skin.seed_distance = affine ? .65f : .30f;
  check(author_skin(scene, old_id, skin), "The editable patch has a real Surface Heat solution");
  auto &old = scene.active();
  const auto &mesh = old.painted->mesh();
  auto deformation = *old.deformation;
  deformation.morphs.emplace(mesh);
  auto shaped = mesh.positions;
  for (auto &p : shaped) p.z += .03f + .02f * p.x;
  deformation.morphs->set_target(mesh, "Raise patch", shaped);
  deformation.morph_weights = {.4f};
  scene.set_deformation(old_id, std::move(deformation));
  RigEdit pose;
  pose.linked_pose = false;
  pose.joint = JointAdjustment{1,{0,10,0},{0,0,.03f}};
  edit_rig(scene,old_id,pose);
  auto &painted_patch = scene.active();
  painted_patch.painted = painted_patch.painted->snapshot();
  auto &paint = *painted_patch.painted;
  Brush brush;
  brush.from = brush.to = {0,0,0};
  brush.normal = {0,0,1};
  brush.radius = .22f;
  brush.strength = 1;
  PaintSettings mask;
  mask.channel = PaintChannel::Mask;
  mask.mask = .15f;
  mask.front_only = false;
  paint.add_layer("Hidden stencil");
  paint.begin_stroke(); paint.stamp(brush,mask);
  check(paint.end_stroke(), "The old patch has an authored mask");
  paint.layer_properties("Hidden stencil",false,.6f);
  PaintSettings ink;
  ink.channel = PaintChannel::Color;
  ink.color = {.82f,.09f,.19f,1};
  ink.front_only = false;
  paint.add_layer("Surface pigment");
  paint.begin_stroke(); paint.stamp(brush,ink);
  check(paint.end_stroke(), "The old patch has independent editable pigment above its Bake");
  PaintSettings glow;
  glow.channel = PaintChannel::Emission;
  glow.emission_units = PaintEmissionUnits::Physical;
  glow.emission = {4,2,.5f};
  glow.front_only = false;
  paint.add_layer("Physical glow");
  paint.begin_stroke(); paint.stamp(brush,glow);
  check(paint.end_stroke() && paint.has_physical_emission(), "The old patch has a physical emission layer");

  // Add an independent sub-R8 emission coefficient to the genuine projected
  // Bake while retaining its color, normal, ORM, common coverage, and recipe.
  auto bake = *paint.baked();
  bake.emission_field = BakedEmissionField::generate_compact(paint.resolution(), [&](size_t i) {
    if (!bake.coverage[i]) return BakedEmissionSample{};
    return BakedEmissionSample{{.0005f,0,0},{3,.125f,0},.001f};
  });
  bake.maps.emission = {};
  paint.clear_bake();
  paint.install_bake(std::move(bake));
  const auto previous = scene.encode();
  scene.save(out/"before-reapply.dsculpt");
  const auto layer = paint.active_layer();

  Document document;
  document.replace(scene);
  settled(document);
  const auto draft = move_patch(document, affine);
  document.end_retopology(true);
  document.save_async(out/"reapplied.dsculpt").get();
  settled(document);
  const auto changed = document.copy_scene();
  const auto &current = changed.active();
  check(current.id != old_id && current.retopology->source == donor && !changed.object(old_id).visible &&
        changed.object(old_id).painted->encode()==paint.encode() &&
        changed.object(old_id).deformation==scene.active().deformation,
        "Apply keeps the previous patch and its independent authored state exact");
  check(current.bake_recipe==scene.active().bake_recipe && current.painted->baked() &&
        current.painted->baked()->emission_field && current.painted->has_physical_emission() &&
        current.painted->active_layer()==layer && current.painted->layers().size()==paint.layers().size(),
        "Replacement keeps editable layers, Bake settings, an active layer, and both FLOAT authorities");
  const auto &mask_layer = current.painted->layers()[1];
  check(!mask_layer.visible && mask_layer.opacity==.6f && mask_layer.name=="Hidden stencil" &&
        std::ranges::any_of(mask_layer.tiles,[](const auto &tile) {
          return tile && std::ranges::any_of(tile->pixels,[](const auto &p) {return p.data[13]<100;});
        }), "The hidden layer retains its opacity and independently editable mask samples");
  check(std::ranges::any_of(current.painted->layers()[2].tiles,[](const auto &tile) {
          return tile && std::ranges::any_of(tile->pixels,[](const auto &p) {return p.data[9]>120&&p.data[0]>180;});
        }), "The replacement atlas contains authored pigment samples");
  check(std::ranges::any_of(current.painted->baked()->emission_field->values(),[](const auto &p) {
          return p.absolute[0]>2.8f && p.legacy[0]>.00045f && p.coverage>0 && p.coverage<.002f;
        }), "Bake preserves separate FLOAT coefficients below one byte of coverage");
  const auto &base = *current.painted->baked();
  const auto &rgb = base.maps.color.rgba;
  size_t green = 0;
  for (size_t i = 0; i < base.coverage.size(); ++i)
    green += base.coverage[i]>128 && rgb[4*i+1]>rgb[4*i]+15;
  check(green>64 && !base.maps.normal.rgba.empty() && !base.maps.metal_rough.rgba.empty(),
        "The original Bake's color, normal and material maps survive below the layers");
  const auto &model = *current.deformation;
  const auto &key = model.morphs->targets.front();
  check(key.name=="Raise patch" && model.morph_weights==std::vector{.4f} && model.pose.size()==2 &&
        model.skin->vertices.size()==current.painted->mesh().positions.size(),
        "Skin and shapekey transfer keeps the original rig, pose and active key");
  for (size_t v = 0; v < key.delta.size(); ++v) {
    const auto p = current.painted->mesh().positions[v];
    check(length(key.delta[v]-Vec3{0,0,.03f+.02f*p.x})<.002f,
          "The projected key delta agrees with an independent linear rest-space oracle");
  }
  const auto committed = changed.encode();
  check(Scene::load(out/"reapplied.dsculpt").encode()==committed && document.undo() &&
        document.copy_scene().encode()==previous && document.redo() &&
        document.copy_scene().encode()==committed,
        "Ordered Save, Cold Open and one Undo/Redo preserve both patch generations");

  auto direct = scene;
  size_t checks_before_publication = 0;
  publish_retopology(direct,*draft->topology,donor,draft->transform,[&] {
    ++checks_before_publication;
    return false;
  });
  check(direct.encode()==committed,"The worker and detached reprojection publish the same scene");
  size_t probes = 0;
  bool cancelled = false;
  try { publish_retopology(scene,*draft->topology,donor,draft->transform,[&] {
      return ++probes == checks_before_publication;
    });
  } catch (const std::runtime_error &) { cancelled = true; }
  check(cancelled && scene.encode()==previous,"Late transfer cancellation leaves the original project exact");

  settled(document);
  document.export_asset(out/"reapplied.glb",AssetExportFormat::Glb).get();
  ImportLimits limits;
  limits.mode = ImportMode::PreserveGltf;
  const auto exported = import_mesh(out/"reapplied.glb",limits);
  check(exported.primitives.size()==1 && exported.skins.size()==1 &&
        !exported.primitives[0].influences.empty() && exported.primitives[0].morph_targets.size()==1,
        "Portable export carries the replacement's real skin and shapekey");

  auto wire = current.retopology->topology;
  const auto first = wire.vertices().begin()->second.position;
  wire.add_vertex({first + Vec3{.06f,.08f,0}});
  auto wire_only = changed;
  publish_retopology(wire_only,wire,donor,current.transform);
  check(wire_only.active().retopology->topology.vertices().size()==wire.vertices().size() &&
        wire_only.active().painted->encode()==current.painted->encode() &&
        wire_only.active().painted->mesh_owner()==current.painted->mesh_owner() &&
        wire_only.active().deformation==current.deformation,
        "Wire-only edits keep the existing atlas and exact deformation owners");

  auto framed = scene;
  framed.active().painted = framed.active().painted->snapshot();
  framed.active().painted->clear_bake();
  ShapeKeyEdit add_key;
  add_key.name = "Geometry frame";
  const auto created = author_shapekey(framed,old_id,add_key);
  const auto old_frames = framed.active().material_bindings->normal_frames;
  publish_retopology(framed,*draft->topology,donor,draft->transform);
  const auto &replacement = framed.active();
  const auto frames = replacement.material_bindings->normal_frames;
  check(created.changed && frames!=old_frames && frames->current_targets.size()==2 &&
        frames->pbr->origin_vertices==replacement.painted->mesh().indices &&
        replacement.deformation->morphs->targets[created.target].name==add_key.name,
        "Native shapekey authoring retains its targets and uses new atlas-corner frames");
  check(Scene::decode(framed.encode()).encode()==framed.encode(),
        "Cold replay authenticates the reshaped project's native frame ownership");
}

void affine_wire_edit(const std::filesystem::path &out) {
  auto source = author_patch(true, out);
  const auto patch = source.active().id;
  const auto donor = source.active().retopology->source;
  Document document;
  document.replace(source);
  settled(document);
  document.convert_baked_normals();
  settled(document);
  document.edit_scene("Place prepared patch under affine transform",[=](Scene &scene, const auto &) {
    scene.object(patch).affine_edit = std::make_shared<const NativeAffineEdit>(AffineTransform(
        std::array<double,16>{-.94,.18,0,0,.21,1.05,0,0,0,0,1,0,.02,-.03,.008,1}));
  });
  settled(document);
  const auto before = document.copy_scene();
  const auto &prepared = before.active();
  const auto before_world = before.world_matrix(patch);
  const auto baked = prepared.painted->baked();
  check(prepared.material_bindings && prepared.material_bindings->canonical_attributes &&
        baked && baked->normal_field,
        "The affine patch has a genuine converted normal field and original material owner");
  document.save_async(out/"affine-wire-before.dsculpt").get();

  document.begin_retopology();
  drafted(document);
  const auto workspace = document.frame().state->retopology;
  check(affine_similarity(workspace->transform).matrix()==AffineTransform{}.matrix() &&
        before_world.orientation()==AffineOrientation::Reversing,
        "The reflected and sheared patch opens in the actual world-space workspace");
  const auto touch_point = point(before.world_matrix(donor),{.45f,.35f,0});
  touch(document, touch_point);
  const auto draft = document.frame().state->retopology;
  const auto added = draft->selected.front();
  check(draft->topology->vertices().size()==prepared.retopology->topology.vertices().size()+1 &&
        draft->topology->faces().size()==prepared.retopology->topology.faces().size(),
        "The actual worker places a new wire point without changing surface faces");

  document.end_retopology(true);
  const auto path = out/"affine-wire-prepared.dsculpt";
  document.save_async(path).get();
  settled(document);
  const auto after = document.copy_scene();
  const auto &replacement = after.active();
  check(after.world_matrix(replacement.id).matrix()==before_world.matrix() &&
        replacement.painted->mesh_owner()==prepared.painted->mesh_owner() &&
        replacement.painted->baked()==baked && replacement.material_bindings==prepared.material_bindings,
        "Wire Apply shares the original atlas, prepared normal field, material and exact local frame");
  const auto &vertices = replacement.retopology->topology.vertices();
  for (const auto &[id, vertex] : prepared.retopology->topology.vertices())
    check(length(vertices.at(id).position-vertex.position)==0.f,
          "An untouched point keeps its exact original local coordinates");
  check(length(point(before_world,vertices.at(added).position)-draft->topology->vertices().at(added).position)<1e-6f &&
        replacement.retopology->topology.faces().begin()->second.vertices==
            prepared.retopology->topology.faces().begin()->second.vertices,
        "A new wire point returns to the same world position and preserves reflected face winding");
  check(Scene::load(path).encode()==after.encode() && document.undo() &&
        document.copy_scene().encode()==before.encode() && document.redo() &&
        document.copy_scene().encode()==after.encode(),
        "Ordered Save, cold Open and Undo/Redo preserve the prepared affine wire edit");
}

void prepared_geometry_edit(bool affine,const std::filesystem::path &out,bool detached=false) {
  auto source = author_patch(affine,out);
  const auto previous = source.active().id;
  const auto guide = source.active().retopology->source;
  if (detached) {
    const auto &mesh=source.active().painted->mesh();
    DeformationState authored(mesh);
    authored.skeleton=Skeleton{{Bone{"Root"},Bone{"Tip",0}}};
    authored.skin.emplace();
    for (auto p : mesh.positions) {
      const float tip=.5f+.3f*p.x;
      authored.skin->vertices.push_back({{0,1-tip},{1,tip}});
    }
    authored.morphs.emplace(mesh);
    auto target=mesh.positions;
    for (auto &p : target) p.z+=.05f+.02f*p.x;
    authored.morphs->set_target(mesh,"Incline",target);
    authored.morph_weights={0};
    source.set_deformation(previous,std::move(authored));
  }
  Document document;
  document.replace(source);
  settled(document);
  document.convert_baked_normals();
  settled(document);
  document.edit_scene("Add independent normal detail",[=](Scene &scene,const auto &) {
    auto &object=scene.object(previous);
    auto paint=object.painted->snapshot();
    auto bake=*paint->baked();
    const auto field=bake.normal_field;
    auto pbr=*field->pbr(), toon=*field->mtoon();
    const std::vector<uint8_t> normal_coverage(bake.coverage.size(),255);
    for (size_t i=0;i<bake.coverage.size();++i) {
      const size_t x=i%paint->resolution();
      const auto tilt=uint8_t(170+x*16/paint->resolution());
      const std::array<uint8_t,4> a{tilt,138,245,255};
      const std::array<uint8_t,4> b{113,tilt,245,255};
      std::copy(a.begin(),a.end(),pbr.rgba.begin()+i*4);
      std::copy(b.begin(),b.end(),toon.rgba.begin()+i*4);
      if (x>=paint->resolution()/2) bake.coverage[i]=0;
    }
    bake.normal_field=field->replace_planes(pbr,toon,normal_coverage);
    paint->clear_bake();
    paint->install_bake(std::move(bake));
    PaintSettings ink;
    ink.color={.8f,.12f,.3f,1};
    ink.front_only=false;
    Brush brush;
    brush.from=brush.to={0,0,0};
    brush.radius=.24f;
    brush.normal={0,0,1};
    brush.strength=1;
    paint->add_layer("Independent detail");
    paint->begin_stroke();
    paint->stamp(brush,ink);
    if (!paint->end_stroke()) throw std::runtime_error("Prepared fixture needs actual editable paint");
    object.painted=std::move(paint);
  });
  settled(document);
  if (affine) {
    document.begin_uv();
    check(document.wait_uv() && document.frame().state->uv->normal_sampling_required,
          "The prepared case opens the native UV editor with required source normal sampling");
    document.edit_uv(UvAction::Scale,.82f);
    document.end_uv(true);
    settled(document);
    auto retained=document.copy_scene();
    check(retained.active().painted->paint_sampling_domain() &&
          retained.active().material_bindings->canonical_uv,
          "The edited atlas keeps independent original writable and normal sampling domains");
    document.edit_scene("Place retained patch with reflection and shear",[=](Scene &scene,const auto &) {
      scene.object(previous).affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(
          std::array<double,16>{-.97,.08,0,0,.13,.96,0,0,0,0,1.02,0,.035,-.015,.004,1}));
    });
    settled(document);
  }
  const auto before=document.copy_scene();
  const auto &old=before.active();
  const auto original=old.painted->baked()->normal_field;
  check(original->pbr()!=original->mtoon() && original->replacements()==1,
        "Original patch has two distinct prepared normal roles above an independent material coverage field");
  document.save_async(out/"prepared-before.dsculpt").get();
  const Vec3 displacement{0,0,-.72f};
  std::shared_ptr<const RetopoView> draft;
  if (detached) draft=relocate_patch(document,before,displacement,true);
  else {
    draft=move_patch(document,affine);
    document.end_retopology(true);
  }
  const auto file=out/"prepared-reapplied.dsculpt";
  document.save_async(file).get();
  settled(document);
  const auto after=document.copy_scene();
  const auto &patch=after.active();
  const auto &paint=*patch.painted;
  const auto field=paint.baked()->normal_field;
  check(patch.retopology->topology.encode()!=old.retopology->topology.encode() &&
        field->origin()==patch.material_bindings->canonical_attributes && field->origin()!=original->origin() &&
        field->pbr()!=field->mtoon() && after.object(previous).painted->baked()->normal_field==original,
        "Geometry Apply publishes a fresh prepared atlas, with both source roles and the old immutable authoring object");
  check(paint.layers().back().name=="Independent detail" && paint.has_paint_data() &&
        patch.bake_recipe==old.bake_recipe && Scene::load(file).encode()==after.encode(),
        "Editable paint, the Bake recipe and both normal fields survive ordered Save and cold Open");

  const auto old_world=before.world_matrix(previous), new_world=after.world_matrix(patch.id);
  const detail::AffineInteractionFrame old_frame(old_world), new_frame(new_world);
  const auto old_sample=detail::source_bake_normals(old,{}, {256*1024*1024,{},&old_frame});
  const auto new_sample=detail::source_bake_normals(patch,{}, {256*1024*1024,{},&new_frame});
  const auto &mesh=paint.mesh();
  const SurfaceQuery query(old.painted->mesh());
  SurfaceQueryOptions nearest;
  nearest.closest_maximum=.2f;
  size_t samples=0, normal_only=0;
  float error=0;
  const auto r=paint.resolution();
  for (uint32_t tri=0;tri<mesh.indices.size()/3;++tri) {
    const auto a=mesh.indices[3*tri], b=mesh.indices[3*tri+1], c=mesh.indices[3*tri+2];
    const auto uv=mesh.uv[a];
    auto uv_delta=[&](uint32_t vertex) {
      return std::array{mesh.uv[vertex][0]-uv[0],mesh.uv[vertex][1]-uv[1]};
    };
    const auto u=uv_delta(b),v=uv_delta(c);
    const auto det=u[0]*v[1]-v[0]*u[1];
    for (uint32_t y=1;y<r;y+=7) for (uint32_t x=1;x<r;x+=7) {
      const auto sx=(x+.5f)/r-uv[0],sy=(y+.5f)/r-uv[1];
      const auto b1=(sx*v[1]-sy*v[0])/det,b2=(u[0]*sy-u[1]*sx)/det;
      const std::array w{1.f-b1-b2,b1,b2};
      if (std::ranges::min(w)<.08f) continue;
      const auto location=point(new_world,mesh.positions[a]*w[0]+mesh.positions[b]*w[1]+mesh.positions[c]*w[2]);
      if (detached && query.nearest_result(location,old_world,nearest))
        throw std::runtime_error("Detached fixture must miss every spatial projection at the actual location");
      const auto expected=query.nearest_result(detached?location-displacement:location,old_world,nearest);
      if (!expected) continue;
      const auto old_normal=old_sample(expected->surface.triangle,expected->surface.barycentric);
      const auto new_normal=new_sample(tri,w);
      for (size_t role=0;role<old_normal.size();++role)
        error=std::max(error,length(old_normal[role]-new_normal[role]));
      ++samples;
      normal_only+=paint.baked()->coverage[size_t(y)*r+x]==0 &&
                   length(new_normal[0]-new_normal[1])>.15f;
    }
  }
  std::cout<<"PREPARED_TRANSFER samples="<<samples<<" normal_only="<<normal_only<<" error="<<error<<'\n';
  check(samples>64 && normal_only>16 && error<.025f,
        "Independent placed sampling verifies both normal maps at texel centres, including zero material coverage");
  if(detached){
    const auto &deformation=*patch.deformation;
    const auto inverse=old_world.inverse();
    float weight_error=0,shape_error=0;
    for (size_t vertex=0;vertex<mesh.positions.size();++vertex){
      const auto local=point(inverse,point(new_world,mesh.positions[vertex])-displacement);
      const auto &row=deformation.skin->vertices[vertex];
      const auto tip=std::ranges::find(row,1u,&Influence::joint);
      if(tip==row.end())throw std::runtime_error("Detached patch lost a source joint");
      weight_error=std::max(weight_error,std::abs(tip->weight-(.5f+.3f*local.x)));
      const auto former=old_world.direction({0,0,.05+.02*local.x});
      const auto delta=deformation.morphs->targets.front().delta[vertex];
      const auto rendered=new_world.direction({delta.x,delta.y,delta.z});
      for(size_t axis=0;axis<3;++axis)shape_error=std::max(shape_error,float(std::abs(former[axis]-rendered[axis])));
    }
    std::cout<<"DETACHED_SKIN weight_error="<<weight_error<<" shape_error="<<shape_error<<'\n';
    check(weight_error<2e-6f&&shape_error<2e-6f,
          "Independent rest-space weights and shapekey vectors survive the detached patch and its new midpoint");
  }
  check(document.undo() && document.copy_scene().encode()==before.encode() &&
        document.redo() && document.copy_scene().encode()==after.encode(),
        "Undo and Redo retain both complete prepared authoring generations");
  settled(document);
  document.export_asset(out/"prepared-editable.glb",AssetExportFormat::EditableGlb256).get();
  std::ifstream portable(out/"prepared-editable.glb",std::ios::binary);
  const std::vector<uint8_t> payload{std::istreambuf_iterator<char>(portable),{}};
  const auto reopened=extract_native_authoring(payload);
  check(reopened&&reopened->encode()==after.encode(),
        "Editable GLB retains both prepared normal roles and their complete native editing history");

  auto cleared=before.snapshot();
  cleared.active().painted->clear_bake();
  publish_retopology(cleared,*draft->topology,guide,draft->transform);
  check(!cleared.active().painted->baked() && cleared.active().material_bindings->canonical_attributes &&
        cleared.active().painted->layers().back().name=="Independent detail",
        "A changed patch whose Bake was cleared retains its new prepared frame and stays unbaked");
}

void mixed_material_quad(const std::filesystem::path &out) {
  auto scene = reference(false);
  const auto donor = scene.active().id;
  RetopoMesh patch;
  const auto a = patch.add_vertex({{-.2f,-.2f,0}});
  const auto b = patch.add_vertex({{ .2f,-.2f,0}});
  const auto c = patch.add_vertex({{ .2f, .2f,0}});
  const auto d = patch.add_vertex({{-.2f, .2f,0}});
  patch.add_face(std::array{a,b,c,d});
  const auto original = publish_retopology(scene,patch,donor,{});
  const auto second = create_texture_set(scene,original,std::array<uint64_t,1>{2},"Blue trim",64);
  check(edit_texture_set(scene,original,second,[](PaintedMesh &paint,Material &) {
    paint.add_layer("Blue finish");
    PaintSettings ink;
    ink.front_only = false;
    ink.color = {.1f,.2f,.9f,1};
    Brush brush;
    brush.from = brush.to = {.14f,.14f,0};
    brush.normal = {0,0,1};
    brush.radius = .22f;
    brush.strength = 1.f;
    paint.begin_stroke();
    paint.stamp(brush,ink);
    return paint.end_stroke();
  }),"The second triangle of a single stable quad carries independently edited paint");
  const auto before = scene.snapshot();
  patch.move_vertex(b,{.40f,-.10f,0});
  patch.move_vertex(d,{-.14f,.40f,0});
  const auto tris = patch.triangulate();
  const auto &m = tris.mesh;
  const auto center = (m.positions[m.indices[0]] + m.positions[m.indices[1]] + m.positions[m.indices[2]]) / 3.f;
  check(SurfaceQuery(scene.active().painted->mesh()).closest(center).triangle==1,
        "The first triangle has moved across the original material boundary at its centre");
  publish_retopology(scene,patch,donor,{});
  const auto &replacement = scene.active();
  check(replacement.retopology->topology.faces().size()==1 &&
        replacement.texture_sets->triangle_sets()[0]==0 && replacement.texture_sets->triangle_sets()[1]==second,
        "The genuine triangle corners preserve both material slots on their side of the quad");
  const auto blue = texture_set_view(replacement,second).painted;
  check(blue->layers().back().name=="Blue finish" && std::ranges::any_of(blue->layers().back().tiles,[](const auto &tile) {
          return tile && std::ranges::any_of(tile->pixels,[](const auto &p) {return p.data[9]>128&&p.data[2]>200;});
        }),
        "The changed material field retains its own editable paint");
  const auto path = out/"mixed-material-quad.dsculpt";
  scene.save(path);
  check(Scene::load(path).encode()==scene.encode() && !scene.object(original).visible,
        "Cold Open retains the replacement quad and its hidden original authoring object");
  check(scene.object(original).texture_sets==before.active().texture_sets &&
        detail::TextureSetPaintAccess::same_current(*scene.object(original).painted,*before.active().painted),
        "The source raster remains exact after the new material allocation");
}

void split_material_seam(bool affine, const std::filesystem::path &out, bool subdivide=false) {
  auto scene = reference(affine);
  const auto donor = scene.active().id;
  RetopoMesh quad;
  const auto a = quad.add_vertex({{-.2f,-.2f,0}});
  const auto b = quad.add_vertex({{ .2f,-.2f,0}});
  const auto c = quad.add_vertex({{ .2f, .2f,0}});
  const auto d = quad.add_vertex({{-.2f, .2f,0}});
  const auto face = quad.add_face(std::array{a,b,c,d});
  quad.set_pinned(a,true);
  const auto quad_bytes = quad.encode();
  check(nlohmann::json::parse(quad_bytes).at("version")==1 && RetopoMesh::decode(quad_bytes).encode()==quad_bytes,
        "Existing quad archives keep their original topology format and bytes");
  size_t cancellation = 0;
  bool cancelled = false;
  try { quad.toggle_seam(b,d,[&]{ return ++cancellation == 3; }); }
  catch (const std::runtime_error &) { cancelled = true; }
  check(cancelled && quad.encode()==quad_bytes,"Cancelling after building the split preserves the original topology and IDs");
  auto wired = quad;
  const auto wire = wired.add_edge(b,d);
  wired.toggle_seam(b,d);
  check(wired.faces().size()==2 && wired.edges().size()==5 && wired.edges().at(wire).seam,
        "An existing diagonal wire becomes the real seam without changing its ID");
  if (subdivide) {
    auto boundary = quad;
    const auto edge = boundary.edge_between(b,c);
    boundary.set_seam(edge,true);
    const auto added = boundary.subdivide_edge(edge);
    check(boundary.faces().size()==3 && boundary.edges().at(edge).b==added &&
          boundary.edges().at(boundary.edge_between(added,c)).seam,
          "Subdividing a quad boundary retains the edge ID and both seam halves without a T-junction");
  }

  const auto original = publish_retopology(scene,quad,donor,{});
  if (affine) scene.active().affine_edit = scene.object(donor).affine_edit;
  const auto secondary = create_texture_set(scene,original,std::array<uint64_t,1>{2},"Blue trim",64);
  for (const auto set : {0u,secondary}) {
    check(edit_texture_set(scene,original,set,[&](PaintedMesh &paint,Material &material) {
      paint.add_layer(set ? "Blue finish" : "Red finish");
      material.color = set ? std::array<float,4>{.15f,.3f,.9f,1} : std::array<float,4>{.9f,.2f,.12f,1};
      PaintSettings ink;
      ink.front_only = false;
      ink.color = material.color;
      Brush brush;
      brush.from = brush.to = set ? Vec3{.12f,.12f,0} : Vec3{-.12f,-.12f,0};
      brush.normal = {0,0,1};
      brush.radius = .24f;
      brush.strength = 1;
      paint.begin_stroke();
      paint.stamp(brush,ink);
      return paint.end_stroke();
    }),"Each side starts with distinct material and editable paint");
  }
  const auto before = scene.encode();
  const auto world = scene.world_matrix(original);
  scene.save(out/"before-seam.dsculpt");
  Document doc;
  doc.replace(scene);
  settled(doc);
  doc.begin_retopology();
  drafted(doc);
  const auto choose = [&](TopologyID id) { touch(doc,point(world,quad.vertices().at(id).position)); };
  choose(a);
  choose(c);
  const auto unsupported = doc.frame().state->retopology->topology;
  doc.edit_retopology(RetopoAction::Seam);
  check(doc.wait_retopology() && !doc.frame().state->retopology->error.empty() &&
        doc.frame().state->retopology->topology==unsupported,
        "The other diagonal cannot silently cut across the material boundary");
  doc.edit_retopology(RetopoAction::Clear);
  drafted(doc);
  choose(b);
  choose(d);
  const auto unsplit = doc.frame().state->retopology->topology;
  doc.edit_retopology(RetopoAction::Seam);
  drafted(doc);
  const auto split = doc.frame().state->retopology->topology;
  check(split->vertices().size()==4 && split->edges().size()==5 && split->faces().size()==2 &&
        split->vertices().at(a).pinned && split->faces().begin()->first==face &&
        split->faces().rbegin()->second.split_from==face &&
        std::ranges::any_of(split->edges(),[&](const auto &entry) {
          const auto &e = entry.second;
          return e.seam && ((e.a==b && e.b==d) || (e.a==d && e.b==b));
        }),"Seam Edge exposes the existing diagonal with two stable triangle faces and keeps the old vertices and pin");
  const auto split_bytes = split->encode();
  check(nlohmann::json::parse(split_bytes).at("version")==2 && RetopoMesh::decode(split_bytes).encode()==split_bytes,
        "Split provenance round-trips through the native topology archive");
  check(doc.undo() && doc.wait_retopology() && doc.frame().state->retopology->topology==unsplit &&
        doc.redo() && doc.wait_retopology() && doc.frame().state->retopology->topology==split,
        "Draft Undo and Redo restore the exact seam and selection snapshots");
  if (!affine && !subdivide) {
    auto reference = scene.snapshot();
    publish_retopology(reference,*split,donor,doc.frame().state->retopology->transform);
    reference.save(out/"seam-only-reference.dsculpt");
  }

  TopologyID middle=0,quarter=0;
  if (subdivide) {
    const auto edge = split->edge_between(b,d);
    check(edge && doc.frame().state->retopology->selected_edge_id==edge,
          "The selected shared edge enables the document subdivision command");
    SpatialFrame input;
    input.focused=input.head_valid=true;
    input.head.position={0,1.6f,0};
    input.hands[1].tracked.valid=true;
    const auto send = [&](float y=0,bool click=false) {
      ++input.sequence;input.time+=.02;
      input.hands[1].y=y;input.hands[1].joystick=click;
      doc.submit(input);
      check(doc.wait_input(),"Controller submission drains through the normal input worker");
    };
    send();send(0,true);send();
    check(doc.frame().state->menu && doc.frame().state->menu_page==16,
          "The controller opens the Retopology palette with the selected edge");
    send(1);send(1,true);send();
    drafted(doc);
    doc.cancel_spatial_menu();
    const auto first = doc.frame().state->retopology;
    check(first->selected.size()==1 && first->topology->faces().size()==4,
          "The controller edge sector actually subdivides both incident faces");
    middle=first->selected.front();
    check(first->topology->edges().at(edge).b==middle && !first->topology->vertices().at(middle).pinned,
          "The new midpoint is the editable selection and keeps the first edge ID");
    if (!affine) {
      auto reference = scene.snapshot();
      publish_retopology(reference,*first->topology,donor,first->transform);
      reference.save(out/"subdivision-only-reference.dsculpt");
    }
    choose(b);
    doc.edit_retopology(RetopoAction::SubdivideEdge);
    drafted(doc);
    const auto repeat = doc.frame().state->retopology;
    check(repeat->selected.size()==1 && repeat->topology->faces().size()==6 &&
          std::ranges::count_if(repeat->topology->edges(),[](const auto &e){return e.second.seam;})==3,
          "The desktop command subdivides a seam half again and retains all three seam segments");
    quarter=repeat->selected.front();
    const auto saved_subdivision=repeat->topology->encode();
    check(nlohmann::json::parse(saved_subdivision).at("version")==3 &&
          RetopoMesh::decode(saved_subdivision).encode()==saved_subdivision,
          "Native subdivision provenance survives an independent topology decode");
  }

  doc.edit_retopology(RetopoAction::MoveMode);
  drafted(doc);
  const auto drag = [&](TopologyID id, Vec3 destination) {
    check(doc.begin_desktop(),"Move admits a regular gesture on the new seam");
    Brush brush;
    const auto current = doc.frame().state->retopology;
    brush.from = brush.to = point(affine_similarity(current->transform),current->topology->vertices().at(id).position);
    doc.stamp_desktop(brush);
    drafted(doc);
    brush.to = point(world,destination);
    doc.stamp_desktop(brush);
    doc.end_desktop();
    drafted(doc);
  };
  const Vec3 new_b{.08f,-.32f,0}, new_d{-.32f,.08f,0};
  drag(b,new_b);
  if (subdivide) {
    drag(quarter,{-.02f,-.22f,0});
    drag(middle,{-.12f,-.12f,0});
  }
  drag(d,new_d);
  const auto center = (new_b + new_d + quad.vertices().at(c).position)/3.f;
  check(SurfaceQuery(scene.active().painted->mesh()).closest(center).triangle==0,
        "The new blue face centre lies on the old red side, ruling out centroid material assignment");
  doc.end_retopology(true);
  doc.save_async(out/"after-seam.dsculpt").get();
  settled(doc);
  const auto applied = doc.copy_scene();
  const auto &object = applied.active();
  const auto topology = object.retopology->topology.triangulate();
  const auto slots = object.texture_sets->triangle_sets();
  const size_t per_side = subdivide ? 3 : 1;
  check(topology.source_faces.size()==2*per_side && slots.size()==2*per_side,
        "Apply keeps both sides of the editable seam");
  for(size_t triangle=0;triangle<slots.size();++triangle) {
    const auto corner = [&](size_t k){return topology.source_vertices[topology.mesh.indices[3*triangle+k]];};
    const bool blue = corner(0)==c || corner(1)==c || corner(2)==c;
    check(slots[triangle]==(blue?secondary:0),"Every child face keeps its original material-side identity");
  }
  for (const auto set : {0u,secondary}) {
    const auto field = texture_set_view(object,set);
    const auto channel = set ? 2 : 0;
    check(field.painted->layers().back().name==(set ? "Blue finish" : "Red finish") &&
          std::ranges::any_of(field.painted->layers().back().tiles,[&](const auto &tile) {
            return tile && std::ranges::any_of(tile->pixels,[&](const auto &pixel) {
              return pixel.data[9]>128 && pixel.data[channel]>200;
            });
          }),"The replacement texture field still owns its original editable ink");
  }
  const auto exact = applied.encode();
  check(Scene::load(out/"after-seam.dsculpt").encode()==exact && !applied.object(original).visible &&
        applied.object(original).texture_sets==scene.active().texture_sets,
        "Ordered Save and cold Open preserve both split authority and the hidden original paint");
  doc.export_asset(out/"standard.glb",AssetExportFormat::Glb).get();
  doc.export_asset(out/"editable.glb",AssetExportFormat::EditableGlb512).get();
  const auto portable = import_mesh(out/"standard.glb");
  check(portable.primitives.size()==2 && std::ranges::all_of(portable.primitives,[&](const auto &primitive) {
          return primitive.mesh.indices.size()==3*per_side;
        }),"The Standard GLB carries every new triangle in its material primitive");
  std::ifstream editable(out/"editable.glb",std::ios::binary);
  const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(editable),{}};
  const auto reopened = extract_native_authoring(bytes);
  check(reopened && reopened->encode()==exact && doc.undo() && doc.copy_scene().encode()==before &&
        doc.redo() && doc.copy_scene().encode()==exact,
        "Editable GLB and global Undo and Redo retain the exact two authoring generations");
  if (subdivide) {
    doc.open_async(out/"after-seam.dsculpt").get();
    settled(doc);
    doc.begin_retopology();
    drafted(doc);
    const auto current = doc.frame().state->retopology;
    for (auto id : {b,quarter})
      touch(doc,point(affine_similarity(current->transform),current->topology->vertices().at(id).position));
    doc.edit_retopology(RetopoAction::SubdivideEdge);
    drafted(doc);
    doc.end_retopology(true);
    doc.save_async(out/"resubdivided.dsculpt").get();
    settled(doc);
    const auto next = doc.copy_scene();
    const auto next_slots = next.active().texture_sets->triangle_sets();
    check(next_slots.size()==8 && std::ranges::count(next_slots,secondary)==4 &&
          Scene::load(out/"resubdivided.dsculpt").encode()==next.encode(),
          "A cold project subdivides a moved seam again using the last saved material-side geometry");
  }
}

SourceMaterialBindings retopo_toon_binding(const TextureSetView &slot,bool trim,bool legacy) {
  auto info=[](int index) {
    return nlohmann::json{{"index",index},{"extensions",{{"KHR_texture_transform",{
        {"rotation",.37},{"offset",{-.15,.16}},{"scale",{1.3,.81}}
    }}}}};
  };
  SourceMaterialBindings binding;
  auto extension=nlohmann::json{
    {"specVersion","1.0"},{"shadeColorFactor",{.35,.55,.6}},
    {"shadeMultiplyTexture",info(1)},{"matcapTexture",info(2)},{"rimMultiplyTexture",info(4)},
    {"uvAnimationMaskTexture",info(5)},{"shadingShiftTexture",info(6)}
  };
  if (trim) {
    extension["outlineWidthMode"]="worldCoordinates";
    extension["outlineWidthFactor"]=.012;
    extension["outlineWidthMultiplyTexture"]=info(7);
  }
  auto json=nlohmann::json{
    {"alphaMode","MASK"},{"alphaCutoff",.33},
    {"pbrMetallicRoughness",{{"baseColorTexture",info(0)}}},
    {"emissiveTexture",info(3)},{"emissiveFactor",{.4,.3,.15}},
    {"extensions",{{"VRMC_materials_mtoon",extension}}}
  };
  if (!trim) {
    json["normalTexture"]=info(7);
    json["normalTexture"]["scale"]=.67;
  }
  if (legacy) {
    binding.material=MToonMaterial::from_vrm0({
      {"shader","VRM/MToon"},
      {"textureProperties",{{"_MainTex",0},{"_ShadeTexture",1},{"_SphereAdd",2},{"_EmissionMap",3},
                            {"_RimTexture",4},{"_UvAnimMaskTexture",5},{"_ReceiveShadowTexture",6},
                            {"_ShadingGradeTexture",7},{"_OutlineWidthTexture",8}}},
      {"floatProperties",{{"_BlendMode",1},{"_Cutoff",.33},{"_OutlineWidthMode",1},{"_OutlineWidth",.012}}},
      {"vectorProperties",{{"_MainTex",{.16,.31,-1.2,.83}}}}
    },9);
  } else {
    binding.material=MToonMaterial::from_vrm1(json,8);
  }
  const auto &native=slot.material;
  binding.native_reference={native.color,native.emissive,int(native.alpha),native.alpha_cutoff,native.double_sided};
  for (size_t role=0;role<binding.roles.size();++role) {
    if (binding.material.textures[role].index<0) continue;
    Image image;
    image.width=trim?96:9;
    image.height=8;
    image.rgba.resize(size_t(image.width)*image.height*4);
    for (size_t at=0;at<image.rgba.size()/4;++at) {
      const size_t x=at%image.width, y=at/image.width;
      const std::array<uint8_t,4> pixel=role==size_t(MToonSlot::Normal)
          ? std::array<uint8_t,4>{uint8_t(151+x*3),uint8_t(105+y*5),236,255}
          : std::array<uint8_t,4>{uint8_t((trim?12:142)+x%15+y),uint8_t(30+role*10+y*21),
                                 uint8_t(60+role*9+x%70),uint8_t(120+x%101)};
      std::copy(pixel.begin(),pixel.end(),image.rgba.begin()+at*4);
    }
    binding.roles[role].image=std::make_shared<const Image>(std::move(image));
    binding.roles[role].wrap_t=33648;
  }
  return binding;
}

std::array<float,2> retopo_toon_test_uv(const Mesh &mesh,uint32_t face,std::array<float,3> weights) {
  std::array<float,2> uv{};
  for (size_t k=0;k<3;++k)
    for (size_t axis=0;axis<2;++axis)
      uv[axis]+=weights[k]*mesh.uv[mesh.indices[face*3+k]][axis];
  return uv;
}

Vec3 retopo_toon_test_normal(const TextureSetView &field,const AffineTransform &world,
    uint32_t face,std::array<float,3> bary) {
  const auto &mesh=field.painted->mesh();
  const auto &binding=*field.material_bindings;
  constexpr auto normal=MToonSlot::Normal;
  std::array<std::array<float,2>,3> uv{};
  std::array<Vec3,3> positions{};
  Vec3 n{};
  for (size_t c=0;c<3;++c) {
    const auto vertex=mesh.indices[face*3+c];
    uv[c]=mtoon_uv(binding.material,normal,mesh.uv[vertex],0);
    positions[c]=point(world,mesh.positions[vertex]);
    const auto local=mesh.normals[vertex];
    const auto placed=world.normal({local.x,local.y,local.z});
    n+=normalized({float(placed[0]),float(placed[1]),float(placed[2])})*bary[c];
  }
  n=normalized(n);
  const auto du=uv[1][0]-uv[0][0], dv=uv[1][1]-uv[0][1];
  const auto eu=uv[2][0]-uv[0][0], ev=uv[2][1]-uv[0][1];
  const auto determinant=du*ev-eu*dv;
  const auto dx=positions[1]-positions[0], dy=positions[2]-positions[0];
  auto t=(dx*ev-dy*dv)/determinant;
  t=normalized(t-n*dot(n,t));
  const auto b=(dy*du-dx*eu)/determinant;
  const auto sign=dot(cross(n,t),b)<0?-1.f:1.f;
  const auto q=mtoon_uv(binding.material,normal,retopo_toon_test_uv(mesh,face,bary),0);
  const auto sample=sample_source_texture(binding.roles[size_t(normal)],q[0],q[1],false);
  return mtoon_material_normal(binding.material,n,{t.x,t.y,t.z,sign},{sample[0],sample[1],sample[2]},n,true);
}

void retopo_toon_materials(bool affine,bool legacy,const std::filesystem::path &out,bool detached=false) {
  auto scene=reference(false);
  const auto guide=scene.active().id;
  RetopoMesh patch;
  const auto a=patch.add_vertex({{-.29f,-.27f,0}});
  const auto b=patch.add_vertex({{ .29f,-.27f,0}});
  const auto c=patch.add_vertex({{ .29f, .27f,0}});
  const auto d=patch.add_vertex({{-.29f, .27f,0}});
  patch.add_face(std::array{a,b,c,d});
  const auto previous=publish_retopology(scene,patch,guide,{});
  const auto trim=create_texture_set(scene,previous,std::array<uint64_t,1>{2},"Toon trim",64);
  for (const auto slot:{TextureSetId(0),trim}) {
    const auto typed=retopo_toon_binding(texture_set_view(scene.active(),slot),slot==trim,legacy && slot==trim);
    check(bind_texture_set_mtoon(scene,previous,slot,typed),"Each retopology slot owns its material through the native MToon edit");
  }
  check(edit_texture_set(scene,previous,trim,[](PaintedMesh &paint,Material &) {
    paint.add_layer("Retopo ink");
    PaintSettings ink;
    ink.front_only=false;
    ink.color={.09f,.28f,.31f,1};
    Brush brush;
    brush.from=brush.to={.18f,.18f,0};
    brush.normal={0,0,1};
    brush.radius=.19f;
    brush.strength=1;
    paint.begin_stroke();
    paint.stamp(brush,ink);
    return paint.end_stroke();
  }),"Trim owns editable paint independent of the typed textures");
  if (affine)
    scene.object(previous).affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(
        std::array<double,16>{-.96,.12,0,0,.11,.96,0,0,0,.13,1.02,0,.02,-.012,.003,1}));
  Document document;
  document.replace(scene);
  settled(document);
  const auto before=document.copy_scene();
  document.save_async(out/"mtoon-before.dsculpt").get();
  const Vec3 displacement{0,0,-.72f};
  const auto draft=detached?relocate_patch(document,before,displacement,false):move_patch(document,affine);
  if(!detached)document.end_retopology(true);
  const auto path=out/"mtoon-retopo.dsculpt";
  document.save_async(path).get();
  settled(document);
  const auto after=document.copy_scene();
  const auto &active=after.active();
  check(active.id!=previous && active.texture_sets->triangle_sets()[1]==trim &&
        active.retopology->topology.encode()!=before.active().retopology->topology.encode() &&
        !after.object(previous).visible && Scene::load(path).encode()==after.encode(),
        "Geometry Apply and ordered Save keep two texture sets and the hidden original project");
  size_t checks_at_texel=0;
  float role_error=0, normal_error=0;
  const auto old_world=before.world_matrix(previous), new_world=after.world_matrix(active.id);
  const SurfaceQuery query(before.active().painted->mesh());
  for (const auto slot:{TextureSetId(0),trim}) {
    const auto old=texture_set_view(before.active(),slot);
    const auto field=texture_set_view(active,slot);
    const auto &source=*old.material_bindings, &target=*field.material_bindings;
    const auto r=target.roles[0].image->width;
    check(r==(slot==trim?128u:512u) && target.roles[0].image!=source.roles[0].image &&
          target.roles[size_t(MToonSlot::Matcap)].image==source.roles[size_t(MToonSlot::Matcap)].image &&
          target.material.parameters.alpha==source.material.parameters.alpha &&
          target.material.version()==source.material.version() &&
          target.material.parameters.outline==source.material.parameters.outline &&
          texture_set_view(after.object(previous),slot).material_bindings==old.material_bindings,
          "Typed roles get slot-local grids while MatCap, alpha, outline and source owners remain intact");
    if (slot==trim)
      check(field.painted->resolution()==64 && field.painted->has_paint_data() &&
            field.painted->layers().back().name=="Retopo ink",
            "Projected source roles can have a finer raster than the editable paint");
    SurfaceQueryOptions nearest;
    nearest.closest_maximum=.2f;
    nearest.triangle_filter=[&](uint32_t triangle) {return old.domain->contains(triangle);};
    const auto &mesh=field.painted->mesh();
    for (const auto face:field.domain->triangles()) {
      const auto ia=mesh.indices[face*3], ib=mesh.indices[face*3+1], ic=mesh.indices[face*3+2];
      const auto uv=mesh.uv[ia];
      const auto u=mesh.uv[ib][0]-uv[0], v=mesh.uv[ib][1]-uv[1];
      const auto s=mesh.uv[ic][0]-uv[0], t=mesh.uv[ic][1]-uv[1];
      for (uint32_t y=0;y<r;y+=11) for (uint32_t x=0;x<r;x+=11) {
        const auto dx=(x+.5f)/r-uv[0], dy=(y+.5f)/r-uv[1];
        const auto p=(dx*t-dy*s)/(u*t-v*s), q=(dy*u-dx*v)/(u*t-v*s);
        const std::array w{1-p-q,p,q};
        if (std::ranges::min(w)<.08f) continue;
        const auto position=point(new_world,mesh.positions[ia]*w[0]+mesh.positions[ib]*w[1]+mesh.positions[ic]*w[2]);
        if(detached&&query.nearest_result(position,old_world,nearest))
          throw std::runtime_error("Detached typed material must miss the spatial projection");
        const auto hit=query.nearest_result(detached?position-displacement:position,old_world,nearest);
        if (!hit) continue;
        const auto from=hit->surface;
        const auto source_uv=retopo_toon_test_uv(old.painted->mesh(),from.triangle,from.barycentric);
        for (size_t role=0;role<source.roles.size();++role) {
          if (!source.roles[role].image || role==size_t(MToonSlot::Matcap)) continue;
          if (role==size_t(MToonSlot::Normal))
            normal_error=std::max(normal_error,length(retopo_toon_test_normal(old,old_world,from.triangle,from.barycentric)-
                                                    retopo_toon_test_normal(field,new_world,face,w)));
          else {
            const bool srgb=source.material.textures[role].encoding==MToonEncoding::Srgb;
            const auto uv=mtoon_uv(source.material,MToonSlot(role),source_uv,0);
            const auto expected=sample_source_texture(source.roles[role],uv[0],uv[1],srgb);
            const auto new_uv=mtoon_uv(target.material,MToonSlot(role),{(x+.5f)/r,(y+.5f)/r},0);
            const auto result=sample_source_texture(target.roles[role],new_uv[0],new_uv[1],srgb);
            for (size_t channel=0;channel<4;++channel)
              role_error=std::max(role_error,std::abs(result[channel]-expected[channel]));
          }
          ++checks_at_texel;
        }
      }
    }
  }
  std::cout<<"MTOON_TRANSFER samples="<<checks_at_texel<<" role_error="<<role_error
           <<" normal_error="<<normal_error<<'\n';
  check(checks_at_texel>200 && role_error<.005f && normal_error<.018f,
        "Independent same-slot world sampling agrees on typed colors, scalar maps and shading normals");
  check(document.undo() && document.copy_scene().encode()==before.encode() &&
        document.redo() && document.copy_scene().encode()==after.encode(),
        "Undo and Redo keep both typed material generations byte for byte");
  settled(document);
  if (!legacy) {
    const auto glb=out/"mtoon-retopo.glb";
    document.export_asset(glb,AssetExportFormat::EditableGlb256).get();
    const auto read=[](const std::filesystem::path &file) {
      std::ifstream stream(file,std::ios::binary);
      return std::vector<uint8_t>{std::istreambuf_iterator<char>(stream),{}};
    };
    const auto payload=read(glb);
    const auto reopened=extract_native_authoring(payload);
    check(reopened && reopened->encode()==after.encode(),"Editable GLB reopens both native typed material fields exactly");
    auto portable=after.snapshot();
    check(edit_texture_set(portable,active.id,trim,[](PaintedMesh &paint,Material &) {
      paint.delete_layer();
      return !paint.has_paint_data();
    }),"An independent export control keeps the projected typed roles without its additional ink overlay");
    Document portable_document;
    portable_document.replace(portable);
    settled(portable_document);
    const auto standard=out/"mtoon-portable.glb";
    portable_document.export_asset(standard,AssetExportFormat::Glb).get();
    const auto existing=read(standard);
    bool refused=false;
    try {document.export_asset(standard,AssetExportFormat::Glb).get();}
    catch(const std::invalid_argument &error) {
      refused=std::string_view(error.what()).find("portable material adapter")!=std::string_view::npos;
    }
    check(refused && !existing.empty() && read(standard)==existing,
          "An unsupported painted portable export preserves the existing typed GLB destination");
  }

  auto live=before.snapshot();
  auto animated=*texture_set_view(live.active(),trim).material_bindings;
  animated.material.parameters.scroll[0]=.1f;
  bind_texture_set_mtoon(live,previous,trim,animated);
  const auto exact=live.encode();
  bool refused=false;
  try {publish_retopology(live,*draft->topology,guide,draft->transform);}
  catch(const std::invalid_argument &error) {refused=std::string_view(error.what()).find("animated")!=std::string_view::npos;}
  check(refused && live.encode()==exact,"Live animated roles refuse geometry Apply without changing the source project");
  if (!legacy && !detached) {
    auto two_sided=before.snapshot();
    auto editable=*two_sided.active().material_bindings;
    editable.material.parameters.cull=MToonCull::Off;
    bind_texture_set_mtoon(two_sided,previous,0,editable);
    const auto both_faces=two_sided.encode();
    bool incompatible=false;
    try {publish_retopology(two_sided,*draft->topology,guide,draft->transform);}
    catch(const std::invalid_argument &error) {
      incompatible=std::string_view(error.what()).find("angular bound")!=std::string_view::npos;
    }
    check(incompatible && two_sided.encode()==both_faces,
          "A typed map that needs different front and back transports fails atomically");
  }
}

void paint_checkpoint(Scene &scene, std::string name, std::array<float,4> color) {
  auto &object = scene.active();
  object.painted = object.painted->snapshot();
  auto &paint = *object.painted;
  paint.add_layer(std::move(name));
  PaintSettings settings;
  settings.color = color;
  settings.front_only = false;
  Brush brush;
  brush.normal = {0,0,1};
  brush.radius = .19f;
  brush.strength = 1;
  paint.begin_stroke();
  paint.stamp(brush, settings);
  check(paint.end_stroke(), "Checkpoint owns distinct authored pigment");
}

void restore_patch_history(bool affine, const std::filesystem::path &out) {
  auto scene = author_patch(affine, out);
  const auto old_id = scene.active().id;
  const auto donor = scene.active().retopology->source;
  const bool shape_first = affine;
  auto bind = [&] {
    paint_checkpoint(scene, "Before rig", {.12f,.34f,.78f,1});
    SkinAuthoringOptions skin;
    skin.reference = donor;
    skin.mode = SkinAuthoringMode::SurfaceHeat;
    skin.seed_distance = affine ? .65f : .30f;
    check(author_skin(scene,old_id,skin) && scene.active().rig_source, "Surface Heat captures the real pre-rig checkpoint");
  };
  auto sculpt_key = [&] {
    paint_checkpoint(scene, "Before key", {.71f,.24f,.11f,1});
    scene.active().painted->clear_bake();
    ShapeKeyEdit edit;
    edit.name = "Lift shape";
    const auto added = author_shapekey(scene,old_id,edit);
    edit.action = ShapeKeyAction::Replace;
    edit.target = added.target;
    edit.positions = scene.active().painted->mesh().positions;
    for (auto &position : edit.positions) position.z += .03f + .02f * position.x;
    check(added.changed && author_shapekey(scene,old_id,edit).changed && scene.active().shape_source,
          "Shapekey edits capture the real pre-key checkpoint");
    auto posed = *scene.active().deformation;
    posed.morph_weights = {.4f};
    scene.set_deformation(old_id,std::move(posed));
  };
  if (shape_first) {
    auto &paint = scene.active().painted;
    paint = paint->snapshot();
    paint->clear_bake();
    paint = detail::TextureSetPaintAccess::resize_unpainted(*paint,256,detail::native_texture_grid_limits());
    sculpt_key();
    // Native archive fixtures can retain a different raster resolution in
    // an earlier checkpoint while sharing the same immutable REST mesh.
    TextureSetResolutionOptions options;
    options.resolution = 512;
    TextureSetResolutionReport report;
    scene.active().painted = detail::TextureSetPaintAccess::resample_plain(
        *scene.active().painted, options, detail::native_texture_grid_limits(), report);
    bind();
  } else { bind(); sculpt_key(); }
  paint_checkpoint(scene, "After both", {.12f,.92f,.27f,1});
  const auto before = scene.encode();
  scene.save(out/"history-before.dsculpt");

  Document document;
  document.open_async(out/"history-before.dsculpt").get();
  settled(document);
  const auto draft = move_patch(document, affine);
  document.end_retopology(true);
  document.save_async(out/"history-reapplied.dsculpt").get();
  settled(document);
  const auto changed = document.copy_scene();
  const auto &current = changed.active();
  const auto topology = draft->topology->encode();
  check(current.id!=old_id && current.shape_source && current.rig_source &&
        current.retopology==current.shape_source->retopology && current.retopology==current.rig_source->retopology &&
        current.shape_source->paint->mesh_owner()==current.painted->mesh_owner() &&
        current.rig_source->paint->mesh_owner()==current.painted->mesh_owner(),
        "Both independently projected checkpoints own the replacement topology and display atlas");
  check(shape_first ? current.rig_source->shape_source==current.shape_source
                    : current.shape_source->rig_source==current.rig_source,
        "The checkpoint DAG keeps the original rig/shape edit order and shared earlier node");
  check(current.shape_source->paint->resolution()==(shape_first ? 256 : 512) &&
        current.rig_source->paint->resolution()==current.painted->resolution() &&
        current.painted->resolution()==512,
        "Current and historical paint retain their own grid resolutions");
  const auto live_color = current.painted->composite(current.material);
  const auto rig_color = current.rig_source->paint->composite(current.rig_source->material);
  check(live_color->color.rgba!=rig_color->color.rgba &&
        current.painted->composite(current.material)->color.rgba==live_color->color.rgba,
        "A warmed live composite cannot replace historical pigment on the same raster");
  const auto after = changed.encode();
  check(document.undo() && document.copy_scene().encode()==before && document.redo() &&
        document.copy_scene().encode()==after,
        "Geometry reapply and Restore checkpoint propagation are one Undo transaction");

  document.open_async(out/"history-reapplied.dsculpt").get();
  settled(document);
  check(document.copy_scene().encode()==after && document.frame().state->can_restore_shapes,
        "Cold reopen restores both editing actions and their independent checkpoint data");
  auto perform_restore = [&](bool rig, const std::filesystem::path &file) {
    const auto prior = document.copy_scene();
    const auto &p = prior.active();
    const auto origin = rig ? p.rig_source : p.shape_source;
    if (!origin) throw std::runtime_error("Following the earlier edit lost its Restore checkpoint");
    if (rig) {
      document.begin_rig_fit();
      auto view = *document.frame().state->rig_fit;
      view.restore_source = true;
      view.linked_rest = false;
      document.configure_rig_fit(view);
      document.accept_rig_fit();
    } else {
      ShapeKeyEdit edit;
      edit.action = ShapeKeyAction::Restore;
      document.author_shape(edit);
    }
    document.save_async(file).get();
    settled(document);
    const auto actual = document.copy_scene();
    const auto &a = actual.active();
    check(a.id==p.id && actual.world_matrix(a.id).matrix()==prior.world_matrix(p.id).matrix() &&
          a.retopology->topology.encode()==topology && a.painted->mesh().indices==p.painted->mesh().indices,
          "Restore keeps the replacement placement and newly accepted topology");
    check(a.painted->encode()==origin->paint->encode() && a.bake_recipe==origin->bake_recipe &&
          a.deformation==origin->deformation && a.material_bindings==origin->material_bindings &&
          a.painted->layers().back().name==(rig ? "Before rig" : "Before key"),
          "Restore selects the actual historical paint and deformation endpoint");
    check(Scene::load(file).encode()==actual.encode() && document.undo() && document.copy_scene().encode()==prior.encode() &&
          document.redo() && document.copy_scene().encode()==actual.encode(),
          "Ordered Save and one Undo/Redo preserve the selected Restore endpoint");
    settled(document);
  };
  perform_restore(shape_first,out/"history-restore-latest.dsculpt");
  auto restored = document.copy_scene();
  check(shape_first ? (!restored.active().deformation->skeleton && restored.active().deformation->morphs->targets.size()==1)
                    : (bool(restored.active().deformation->skeleton) && !restored.active().deformation->morphs),
        "The other edit survives when it preceded the selected Restore checkpoint");
  perform_restore(!shape_first,out/"history-restore-first.dsculpt");
  restored = document.copy_scene();
  check(!restored.active().rig_source && !restored.active().shape_source &&
        !restored.active().deformation && bool(restored.active().painted->baked())==!shape_first,
        "The earliest endpoint restores its original Bake status and fully unedited rig/key state");

  auto wire_only = changed;
  auto edited = current.retopology->topology;
  const auto wire = edited.add_vertex({edited.vertices().begin()->second.position + Vec3{.05f,.07f,0}});
  publish_retopology(wire_only,edited,donor,current.transform);
  ShapeKeyEdit revert;
  revert.action = ShapeKeyAction::Restore;
  author_shapekey(wire_only,wire_only.active().id,revert);
  check(wire_only.active().retopology->topology.vertices().contains(wire) &&
        wire_only.active().painted->mesh_owner()==current.painted->mesh_owner(),
        "Restore retains a later wire-only edit and reuses the existing raster geometry");
}
}

int main(int argc,char **argv) try {
  if (argc!=3) return 2;
  const std::string_view mode = argv[2];
  if (mode!="plain" && mode!="affine" && mode!="affine-wire" && mode!="mixed-material" &&
      mode!="mtoon-plain" && mode!="mtoon-affine" && mode!="mtoon-v0" && mode!="mtoon-detached" &&
      mode!="reapply-plain" && mode!="reapply-affine" && mode!="prepared-plain" && mode!="prepared-affine" &&
      mode!="prepared-detached" && mode!="prepared-detached-affine" &&
      mode!="restore-plain" && mode!="restore-affine" && mode!="material-seam-plain" &&
      mode!="material-seam-affine" && mode!="subdivide-seam-plain" && mode!="subdivide-seam-affine") return 2;
  const std::filesystem::path out = argv[1];
  std::filesystem::create_directories(out);
  setenv("XDG_STATE_HOME",(out/"state").c_str(),1);
  setenv("XDG_CONFIG_HOME",(out/"config").c_str(),1);
  if (mode.starts_with("restore-")) restore_patch_history(mode.ends_with("affine"),out);
  else if (mode=="affine-wire") affine_wire_edit(out);
  else if (mode=="mixed-material") mixed_material_quad(out);
  else if (mode.starts_with("material-seam-")) split_material_seam(mode.ends_with("affine"),out);
  else if (mode.starts_with("subdivide-seam-")) split_material_seam(mode.ends_with("affine"),out,true);
  else if (mode.starts_with("mtoon-")) retopo_toon_materials(mode.ends_with("affine"),mode.ends_with("v0"),out,mode=="mtoon-detached");
  else if (mode.starts_with("prepared-")) prepared_geometry_edit(mode.ends_with("affine"),out,mode.starts_with("prepared-detached"));
  else if (mode.starts_with("reapply-")) reapply_patch(mode.ends_with("affine"),out);
  else bind_patch(mode=="affine",out);
  std::cout << "PASS " << checks << " retopology Surface Heat controls\n";
} catch(const std::exception &error) {
  std::cerr << "FAIL " << error.what() << '\n';
  return 1;
}
