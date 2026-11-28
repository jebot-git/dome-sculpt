// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake-authoring.hpp"
#include "dome/document.hpp"
#include "dome/import.hpp"
#include "dome/native-authoring.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/paint-sampling.hpp"
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
#include "source-color-fixtures.hpp"
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
  const auto mask_id=paint.active_layer();
  paint.begin_stroke(); paint.stamp(brush,mask);
  check(paint.end_stroke(), "The old patch has an authored mask");
  paint.layer_properties("Hidden stencil",false,.6f);
  PaintSettings ink;
  ink.channel = PaintChannel::Color;
  ink.color = {.82f,.09f,.19f,1};
  ink.front_only = false;
  paint.add_layer("Surface pigment");
  const auto pigment_id=paint.active_layer();
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
  const auto &layers=current.painted->layers();
  const auto mask_layer=std::ranges::find(layers,mask_id,&PaintLayer::id);
  check(mask_layer!=layers.end()&&!mask_layer->visible && mask_layer->opacity==.6f && mask_layer->name=="Hidden stencil" &&
        std::ranges::any_of(mask_layer->tiles,[](const auto &tile) {
          return tile && std::ranges::any_of(tile->pixels,[](const auto &p) {return p.data[13]<100;});
        }), "The hidden layer retains its opacity and independently editable mask samples");
  const auto pigment_layer=std::ranges::find(layers,pigment_id,&PaintLayer::id);
  check(pigment_layer!=layers.end()&&std::ranges::any_of(pigment_layer->tiles,[](const auto &tile) {
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
  const auto stride=std::clamp(r/32,1u,7u);
  for (uint32_t tri=0;tri<mesh.indices.size()/3;++tri) {
    const auto a=mesh.indices[3*tri], b=mesh.indices[3*tri+1], c=mesh.indices[3*tri+2];
    const auto uv=mesh.uv[a];
    auto uv_delta=[&](uint32_t vertex) {
      return std::array{mesh.uv[vertex][0]-uv[0],mesh.uv[vertex][1]-uv[1]};
    };
    const auto u=uv_delta(b),v=uv_delta(c);
    const auto det=u[0]*v[1]-v[0]*u[1];
    for (uint32_t y=1;y<r;y+=stride) for (uint32_t x=1;x<r;x+=stride) {
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

enum class PreparedColorOrder { ColorOnly, ColorFirst, NormalFirst };

Scene edit_prepared_uv(Document &document,const std::filesystem::path &out,uint64_t guide) {
  const auto original=document.copy_scene();
  const auto original_json=original.encode();
  const auto &old=*original.active().painted;
  const auto &binding=*original.active().material_bindings;
  const auto scene_file=out/"retained-uv.dsculpt";
  original.save(out/"retained-uv-source.dsculpt");
  for (size_t pass=0;pass<2;++pass) {
    const auto prior=document.copy_scene().encode();
    document.begin_uv();
    check(document.wait_uv() && document.frame().state->uv->paint_sampling_required &&
          document.frame().state->uv->normal_sampling_required,
          "The UV editor keeps independent prepared paint and normal sampling by default");
    document.edit_uv(UvAction::Scale,pass==0?.81f:.85f);
    check(document.wait_uv(), "The actual UV edit validates a changed atlas");
    document.end_uv(true);
    document.save_async(scene_file).get();
    settled(document);
    const auto accepted=document.copy_scene();
    const auto &patch=accepted.active();
    check(patch.painted->mesh().uv!=old.mesh().uv && patch.painted->paint_sampling_domain() &&
          patch.painted->paint_sampling_domain()->basis()->uv==old.mesh().uv &&
          patch.painted->baked()==old.baked() && patch.painted->color_projection()==old.color_projection() &&
          patch.painted->layers().back().pbr_tiles==old.layers().back().pbr_tiles &&
          patch.material_bindings->color_preparation==binding.color_preparation &&
          patch.material_bindings->canonical_attributes==binding.canonical_attributes,
          "UV Apply changes display coordinates and shares the exact old Paint, Bake and preparation owners");
    const auto encoded=accepted.encode();
    check(Scene::load(scene_file).encode()==encoded && document.undo() &&
          document.copy_scene().encode()==prior && document.redo() && document.copy_scene().encode()==encoded,
          "Ordered Save, cold Open and Undo/Redo preserve both field owners after each UV edit");
    settled(document);
  }
  const auto edited=document.copy_scene();
  const auto before_bake=edited.encode();
  document.export_asset(out/"retained-uv-standard.glb",AssetExportFormat::Glb).get();
  ImportLimits limits;
  limits.mode=ImportMode::PreserveGltf;
  const auto standard=import_mesh(out/"retained-uv-standard.glb",limits);
  const auto &primitive=standard.primitives.front();
  const auto &material=standard.materials.at(primitive.material);
  const auto read_uv=[&](int set,uint32_t vertex) {
    if (set==0) return primitive.mesh.uv.at(vertex);
    if (set==1) return primitive.uv1.at(vertex);
    return primitive.extra_uv_sets.at(uint32_t(set)).at(vertex);
  };
  check(primitive.colors.empty() && material.color_texture>=0 && material.normal_texture>=0 &&
        primitive.mesh.indices.size()==old.mesh().indices.size(),
        "Standard GLB has its independently authored PBR and normal inputs");
  for (size_t c=0;c<primitive.mesh.indices.size();++c) {
    const auto vertex=primitive.mesh.indices[c];
    const auto expected=old.mesh().uv[old.mesh().indices[c]];
    if (read_uv(material.color_texcoord,vertex)!=expected || read_uv(material.normal_texcoord,vertex)!=expected)
      throw std::runtime_error("GLB prepared roles sampled the edited UVs instead of their respective original grids");
  }
  check(material.color_texcoord!=0 && material.normal_texcoord!=0,
        "Both emitted texture roles carry the original ordered UV sequence beside the edited layout");

  ObjectBakeSettings settings;
  settings.front_mm=settings.back_mm=35;
  settings.ao_samples=0;
  settings.allow_backfaces=true;
  document.bake_from_object(guide,settings);
  settled(document);
  const auto rebaked=document.copy_scene();
  const auto &prior=edited.active();
  const auto &next=rebaked.active();
  check(next.painted->baked()!=prior.painted->baked() && next.painted->baked()->normal_field &&
        next.painted->baked()->color_interpretation==BakedColorInterpretation::ResolvedPbr &&
        next.painted->paint_sampling_domain()==prior.painted->paint_sampling_domain() &&
        next.material_bindings->color_preparation==binding.color_preparation &&
        next.painted->baked()->normal_field->prior()==prior.painted->baked()->normal_field,
        "Rebake writes both prepared fields in their retained coordinates and keeps the previous normal owner");
  check(document.undo() && document.copy_scene().encode()==before_bake,
        "Undo Rebake recovers the original independent field data");
  settled(document);
  document.begin_uv();
  check(document.wait_uv(), "The retained UV history is available for Restore");
  document.edit_uv(UvAction::RestorePaintSource);
  settled(document);
  check(document.copy_scene().encode()==original_json && document.undo() &&
        document.copy_scene().encode()==before_bake,
        "Restore and Undo recover the exact paired source and edited projects");
  settled(document);
  return Scene::load(scene_file);
}

void prepared_color_edit(bool affine,const std::filesystem::path &out,PreparedColorOrder order,bool retained_uv=false) {
  const bool portable_normals = order!=PreparedColorOrder::ColorOnly;
  auto scene = author_patch(affine,out);
  const auto previous = scene.active().id;
  const auto guide = scene.active().retopology->source;
  auto donor = source_color_fixture::model(false,false).active();
  donor.id = guide;
  donor.visible = false;
  donor.affine_edit = scene.object(guide).affine_edit;
  scene.object(guide) = std::move(donor);
  if (order!=PreparedColorOrder::NormalFirst) scene.active().painted->clear_bake();
  scene.active().painted->add_layer("Independent painted color");
  const auto layer_id = scene.active().painted->active_layer();

  Document document;
  document.replace(scene);
  settled(document);
  if (order==PreparedColorOrder::NormalFirst) {
    document.convert_baked_normals();
    settled(document);
  }
  ObjectBakeSettings settings;
  settings.front_mm = settings.back_mm = 35;
  settings.ao_samples = 0;
  settings.allow_backfaces = true;
  settings.prepare_colors = true;
  document.bake_from_object(guide,settings);
  settled(document);
  const auto raw = document.copy_scene();
  const auto recipe = raw.active().material_bindings->color_preparation;
  const auto &bake = raw.active().painted->baked();
  check(recipe && recipe->receipt.source_field==SourceColorField::Identity && bake &&
        bake->color_interpretation==BakedColorInterpretation::ResolvedPbr &&
        bake->maps.color.rgba!=bake->maps.pbr_color.rgba,
        "Bake from original COLOR_0 produces truly different common and PBR pixels on the native patch");

  if (order==PreparedColorOrder::ColorFirst) {
    raw.save(out/"color-normal-unconverted.dsculpt");
    document.convert_baked_normals();
    settled(document);
    const auto converted = document.copy_scene();
    const auto &normal = converted.active().painted->baked();
    check(normal && normal->normal_field && converted.active().material_bindings->color_preparation==recipe &&
          normal->maps.color.rgba==bake->maps.color.rgba && normal->maps.pbr_color.rgba==bake->maps.pbr_color.rgba &&
          normal->coverage==bake->coverage && normal->normal_field->origin()->before_paint()->baked()->maps.normal.rgba==bake->maps.normal.rgba,
          "The portable normals command keeps the exact original resolved colors and proves its separate canonical normal pixels");
    check(document.undo() && document.copy_scene().encode()==raw.encode() &&
          document.redo() && document.copy_scene().encode()==converted.encode(),
          "Conversion of a colored Bake is one exact Undo/Redo transaction");
    converted.save(out/"color-normal-converted.dsculpt");
    settled(document);
  }
  if (order==PreparedColorOrder::NormalFirst)
    check(bake->normal_field && recipe->original_material && recipe->original_material->canonical_attributes,
          "Preparing colors and rebaking works with an earlier independent normal owner");

  auto painted = document.copy_scene().snapshot();
  auto &paint = *painted.active().painted;
  if (portable_normals) {
    auto live = *paint.baked();
    auto pbr = *live.normal_field->pbr();
    auto toon = *live.normal_field->mtoon();
    const std::array<uint8_t,4> pbr_texel{186,129,242,255}, toon_texel{127,174,246,255};
    const auto size = paint.resolution();
    for (size_t at=0;at<live.coverage.size();++at) {
      std::copy(pbr_texel.begin(),pbr_texel.end(),pbr.rgba.begin()+at*4);
      std::copy(toon_texel.begin(),toon_texel.end(),toon.rgba.begin()+at*4);
      if (at%size >= 3*size/4) live.coverage[at]=0;
    }
    live.normal_field=live.normal_field->replace_planes(pbr,toon,std::vector<uint8_t>(live.coverage.size(),255));
    paint.clear_bake();
    paint.install_bake(std::move(live));
  }
  const auto &mesh = paint.mesh();
  const Vec3 center = (mesh.positions[mesh.indices[0]] + mesh.positions[mesh.indices[1]] +
                       mesh.positions[mesh.indices[2]]) / 3.f;
  PaintSettings ink;
  ink.color = {.72f,.09f,.17f,.8f};
  ink.front_only = false;
  Brush brush;
  brush.from = brush.to = center;
  brush.radius = .18f;
  brush.normal = {0,0,1};
  brush.strength = 1;
  paint.begin_stroke();
  paint.stamp(brush,ink);
  check(paint.end_stroke(), "The prepared patch has an actual writable color layer");
  paint.layer_properties("Independent painted color",false,.625f);
  auto input = painted.encode();
  // Give the hidden layer a distinct resolved payload in a valid native file.
  // The opaque and RGB contributions remain independent of shared coverage.
  auto &objects = input.at("objects");
  auto object = std::ranges::find_if(objects,[&](const auto &o){return o.at("id")==previous;});
  auto &layer = object->at("painted").at("layers").back();
  for (auto &tile : layer.at("pbr_color_tiles")) {
    auto &bytes = tile.at("data").get_binary();
    for (size_t p=0;p<bytes.size();p+=4) {
      if (bytes[p+3]==0) continue;
      bytes[p] = 41;
      bytes[p+2] = 216;
      bytes[p+3] = 109;
    }
  }
  auto before = Scene::decode(input);
  document.replace(before);
  settled(document);
  if (retained_uv) {
    before=edit_prepared_uv(document,out,guide);
    document.replace(before);
    settled(document);
  }
  before.save(out/"color-before.dsculpt");
  std::shared_ptr<const RetopoView> draft;
  if (affine) draft = relocate_patch(document,before,{.72f,0,0},true);
  else {
    draft = move_patch(document,false);
    document.end_retopology(true);
  }
  document.save_async(out/"color-after.dsculpt").get();
  settled(document);
  const auto after = document.copy_scene();
  const auto &patch = after.active();
  const auto projection = patch.material_bindings ? patch.material_bindings->color_preparation : nullptr;
  const auto &new_paint = *patch.painted;
  check(patch.id!=previous && !after.object(previous).visible &&
        after.object(previous).material_bindings==before.active().material_bindings &&
        after.object(previous).painted->baked()==before.active().painted->baked() &&
        projection && projection->projection!=before.active().material_bindings->color_preparation->projection &&
        projection->original_paint->mesh_owner()==new_paint.mesh_owner() &&
        projection->receipt.accepted_projection_error==recipe->receipt.accepted_projection_error,
        "Apply keeps the previous patch and certifies the identity field in the replacement atlas");
  check(bool(new_paint.baked()), "The new prepared patch has an actual Bake payload");
  const auto &resolved = *new_paint.baked();
  const auto &hidden = new_paint.layers().back();
  check(hidden.id==layer_id && !hidden.visible && hidden.opacity==.625f && new_paint.active_layer()==layer_id,
        "Layer visibility, opacity and active ID survive color transport");
  size_t covered=0, independent_bake=0, independent_layer=0;
  size_t normal_without_color=0;
  const auto size = new_paint.resolution();
  for (uint32_t y=0;y<size;++y) for (uint32_t x=0;x<size;++x) {
    const auto at = size_t(y)*size + x;
    if (resolved.coverage[at]>240) {
      ++covered;
      independent_bake += int(resolved.maps.color.rgba[at*4+3])-resolved.maps.pbr_color.rgba[at*4+3]>25;
    }
    const auto &common = new_paint.pixel(layer_id,x,y).data;
    const auto pbr = new_paint.pbr_pixel(layer_id,x,y);
    independent_layer += common[9]>190 && common[0]>190 && pbr[0]==41 && pbr[2]==216 && pbr[3]==109;
    if (portable_normals && resolved.coverage[at]==0 && resolved.normal_field)
      normal_without_color += std::abs(int(resolved.normal_field->pbr()->rgba[at*4])-
                                       resolved.normal_field->mtoon()->rgba[at*4])>30;
  }
  std::cout << "RESOLVED_COLOR covered=" << covered << " independent_bake=" << independent_bake
            << " independent_layer=" << independent_layer << '\n';
  check(covered>1000 && independent_bake>1000 && independent_layer>100,
        "Replacement coverage retains both independently sampled Bake colors and the hidden editable PBR tile data");
  if (portable_normals) {
    std::cout << "COMBINED_NORMAL independent_without_color=" << normal_without_color << '\n';
    check(resolved.normal_field && resolved.normal_field->origin()==patch.material_bindings->canonical_attributes &&
          normal_without_color>1000 && resolved.normal_field->origin()->before_paint()->mesh_owner()==new_paint.mesh_owner(),
          "Both normal roles remain in the replacement frame with coverage independent from color");
  }
  const auto encoded = after.encode();
  check(Scene::load(out/"color-after.dsculpt").encode()==encoded && document.undo() &&
        document.copy_scene().encode()==before.encode() && document.redo() && document.copy_scene().encode()==encoded,
        "Ordered Save, cold replay and one Undo/Redo preserve both complete authoring states");
  settled(document);
  document.export_asset(out/"color-editable.glb",AssetExportFormat::EditableGlb256).get();
  std::ifstream file(out/"color-editable.glb",std::ios::binary);
  const std::vector<uint8_t> portable{std::istreambuf_iterator<char>(file),{}};
  const auto reopened = extract_native_authoring(portable);
  check(reopened && reopened->encode()==encoded,
        "Editable GLB independently reopens the projected PBR Bake and layer owners");
  document.export_asset(out/"color-standard.glb",AssetExportFormat::Glb).get();
  ImportLimits limits;
  limits.mode = ImportMode::PreserveGltf;
  const auto standard = import_mesh(out/"color-standard.glb",limits);
  check(standard.primitives.size()==1 && standard.primitives.front().colors.empty(),
        "Standard GLB emits the revised patch without a second vertex-color multiplier");
  const auto texture = standard.materials.at(standard.primitives.front().material).color_texture;
  const auto image = standard.textures.at(size_t(texture)).image;
  const auto exported = decode_projection_png(standard.images.at(size_t(image)).bytes);
  const auto composited = new_paint.composite(patch.material);
  check(exported.width==composited->pbr_color.width && exported.height==composited->pbr_color.height &&
        exported.rgba==composited->pbr_color.rgba && exported.rgba!=composited->color.rgba,
        "An independent GLB read gets the exact resolved PBR texture from the new atlas");

  // This render control varies only the resolved Bake, keeping geometry,
  // common paint, material and coverage fixed across both stereo draws.
  auto control = after.snapshot();
  auto fallback = resolved;
  fallback.maps.pbr_color = fallback.maps.color;
  control.active().painted->clear_bake();
  control.active().painted->install_bake(std::move(fallback));
  control.save(out/"color-common-control.dsculpt");

  if (portable_normals) {
    auto normal_control = after.snapshot();
    auto neutral = resolved;
    neutral.normal_field = detail::BakedNormalField::from_intrinsic(patch.material_bindings->canonical_attributes);
    normal_control.active().painted->clear_bake();
    normal_control.active().painted->install_bake(std::move(neutral));
    normal_control.save(out/"color-normal-flat-control.dsculpt");
  }

  auto cleared = before.snapshot();
  cleared.active().painted->clear_bake();
  publish_retopology(cleared,*draft->topology,guide,draft->transform);
  check(!cleared.active().painted->baked() && cleared.active().painted->color_projection() &&
        cleared.active().painted->layers().back().id==layer_id &&
        (!portable_normals || cleared.active().material_bindings->canonical_attributes),
        "A cleared Bake stays cleared while the prepared editable color remains available");
  if (portable_normals) {
    cleared.save(out/"color-normal-cleared.dsculpt");
    check(Scene::load(out/"color-normal-cleared.dsculpt").encode()==cleared.encode(),
          "Cold Open retains both independent preparation owners after Clear and Apply");
  }
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

void first_prepared_patch(bool affine, const std::filesystem::path &out) {
  Mesh mesh;
  mesh.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};
  mesh.normals.assign(4,{0,0,1});
  mesh.uv={{0,0},{1,0},{1,1},{0,1}};
  mesh.indices={0,1,2,0,2,3};
  Scene scene;
  auto &guide=scene.active();
  const auto source=guide.id;
  guide.mesh_active=true;
  guide.painted=std::make_shared<PaintedMesh>(mesh,64);
  BakedTextures bake;
  bake.coverage.assign(64*64,255);
  bake.maps.color={64,64,std::vector<uint8_t>(4*64*64,255)};
  bake.maps.metal_rough=bake.maps.color;
  bake.maps.normal=bake.maps.color;
  bake.maps.emission=bake.maps.color;
  for (size_t i=0;i<bake.coverage.size();++i) {
    bake.maps.normal.rgba[4*i]=158;
    bake.maps.normal.rgba[4*i+1]=109;
  }
  guide.painted->install_bake(std::move(bake));
  DeformationState pose(mesh);
  pose.skeleton=Skeleton{{Bone{"Retopology pose joint"}}};
  pose.skin=SkinWeights{};
  pose.skin->vertices.assign(mesh.positions.size(),{{0,1}});
  pose.morphs.emplace(mesh);
  MorphTarget target{"Translated sloping pose",{}};
  for (auto p:mesh.positions) target.delta.push_back({1.65f,0,.32f*p.x});
  pose.morphs->targets.push_back(std::move(target));
  scene.set_deformation(source,pose);
  const auto converted=convert_canonical_normals({guide.painted,{}, {},guide.deformation,guide.material,{}});
  guide.painted=converted.paint->snapshot();
  guide.material_bindings=converted.material_bindings;
  auto updated=*guide.painted->baked();
  auto to_pbr=*updated.normal_field->pbr(),to_toon=*updated.normal_field->mtoon();
  for (size_t i=0;i<updated.coverage.size();++i) {
    to_pbr.rgba[4*i]=uint8_t(158+i%64/2);
    to_toon.rgba[4*i+1]=uint8_t(157+i/64/2);
  }
  updated.normal_field=updated.normal_field->replace_planes(
      to_pbr,to_toon,std::vector<uint8_t>(updated.coverage.size(),255));
  guide.painted->clear_bake();
  guide.painted->install_bake(std::move(updated));
  pose.morph_weights={.8f};
  pose.pose.resize(1);
  pose.pose[0].rotation={.9659258f,0,.2588190f,0};
  pose.pose[0].translation={.12f,-.08f,.11f};
  scene.set_deformation(source,pose);
  if (affine) guide.affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(
      std::array<double,16>{-1.12,.13,.19,0,.27,.88,.07,0,.04,-.05,1.21,0,.08,-.11,.04,1}));
  const auto posed=pose.preview(mesh);
  const auto world=scene.world_matrix(source);
  check(SurfaceQuery(mesh).closest(posed.positions[0]).distance>.35f,
        "The guide pose moves the actual surface beyond REST projection reach");

  RetopoMesh topology;
  std::array<TopologyID,4> quad;
  for (size_t i=0;i<quad.size();++i)
    quad[i]=topology.add_vertex({point(world,posed.positions[i])});
  if (affine) std::reverse(quad.begin(),quad.end());
  topology.add_face(quad);
  scene.validate();
  scene.save(out/"posed-prepared-source.dsculpt");
  auto cleared=scene.snapshot();
  cleared.active().painted->clear_bake();
  publish_retopology(cleared,topology,source,{});
  check(!cleared.active().painted->baked() && cleared.active().material_bindings->canonical_attributes &&
        cleared.active().material_bindings->canonical_attributes->before_paint()->baked(),
        "First Apply on a posed cleared Bake preserves the conversion lease and live unbaked state");

  if (affine) {
    Document uv;
    uv.replace(scene);
    settled(uv);
    uv.begin_uv();
    check(uv.wait_uv(),"The posed prepared guide opens the independent UV editor");
    uv.edit_uv(UvAction::Pick,.5f,.5f);
    uv.edit_uv(UvAction::Seam);
    check(uv.wait_uv(),"The UV worker splits the original shared edge");
    uv.edit_uv(UvAction::Scale,.72f);
    check(uv.wait_uv() && uv.frame().state->uv->valid,"The split chart can be committed");
    uv.end_uv(true);
    settled(uv);
    auto split=uv.copy_scene();
    check(split.active().painted->mesh().positions.size()>mesh.positions.size() &&
          split.active().material_bindings->canonical_uv,
          "Independent coordinates and split skin rows survive the real UV operation");
    auto split_baked=split.snapshot();
    publish_retopology(split_baked,topology,source,{});
    check(split_baked.active().painted->baked()->normal_field &&
          split_baked.object(source).material_bindings==split.active().material_bindings,
          "First Apply samples live prepared normals through their original ordered corners after a seam split");
    uv.clear_bake();
    settled(uv);
    split=uv.copy_scene();
    publish_retopology(split,topology,source,{});
    check(!split.active().painted->baked() &&
          split.active().material_bindings->canonical_attributes->before_paint()->baked(),
          "A cleared Bake reprojects its historical atlas through the shared pose of the split seam");
    split.save(out/"posed-cleared-split.dsculpt");
    check(Scene::load(out/"posed-cleared-split.dsculpt").encode()==split.encode(),
          "Reopening preserves the cleared replacement and the split original material");
  }

  const auto original=guide.painted->baked()->normal_field;
  const auto exact=guide.material_bindings;
  Document doc;
  doc.replace(scene);
  settled(doc);
  doc.begin_retopology();
  drafted(doc);
  for (auto id:quad) touch(doc,topology.vertices().at(id).position);
  doc.edit_retopology(RetopoAction::Face);
  drafted(doc);
  doc.end_retopology(true);
  doc.save_async(out/"posed-prepared-patch.dsculpt").get();
  settled(doc);
  const auto after=doc.copy_scene();
  const auto &patch=after.active();
  const auto field=patch.painted->baked()->normal_field;
  check(!patch.deformation && patch.retopology && patch.retopology->source==source &&
        !after.object(source).visible && after.object(source).material_bindings==exact &&
        after.object(source).painted->baked()->normal_field==original && field->origin()==patch.material_bindings->canonical_attributes &&
        field->origin()!=original->origin() && field->pbr()!=field->mtoon(),
        "Apply captures a static patch with its own independent normal maps and retains the posed guide");
  check(Scene::load(out/"posed-prepared-patch.dsculpt").encode()==after.encode(),
        "The ordered save reopens with the original and replacement normal owners intact");

  const auto patch_world=after.world_matrix(patch.id);
  const detail::AffineInteractionFrame target_placement(patch_world);
  const auto applied=detail::source_bake_normals(patch,{}, {256*1024*1024,{},&target_placement});
  const auto &frame_set=*exact->normal_frames->pbr;
  check(frame_set.targets.size()==1 && !frame_set.targets[0].normals.empty(),
        "The guide owns authored directions for the active morph");
  const SurfaceQuery query(posed);
  const auto &result=patch.painted->mesh();
  float error=0,rest_difference=0;
  size_t samples=0;
  for (uint32_t tri=0;tri<result.indices.size()/3;++tri) {
    const auto a=result.indices[3*tri],b=result.indices[3*tri+1],c=result.indices[3*tri+2];
    for (int row=2;row<15;++row) for (int col=2;row+col<19;++col) {
      const std::array w{1.f-(row+col)*.05f,row*.05f,col*.05f};
      const auto p=point(patch_world,result.positions[a]*w[0]+result.positions[b]*w[1]+result.positions[c]*w[2]);
      const auto hit=query.nearest_result(p,world,{});
      if (!hit || std::ranges::min(hit->surface.barycentric)<.06f) continue;
      std::array<Vec3,3> posed_columns{},rest_columns{};
      std::array<float,2> uv{};
      for (size_t i=0;i<3;++i) {
        const auto corner=hit->surface.triangle*3+i;
        const auto j=mesh.indices[corner];
        const float weight=hit->surface.barycentric[i];
        for (size_t axis=0;axis<2;++axis) uv[axis]+=mesh.uv[j][axis]*weight;
        const auto n=frame_set.normals[corner];
        const auto at=frame_set.tangents[corner];
        const Vec3 t{at[0],at[1],at[2]};
        const auto accumulate=[&](std::array<Vec3,3> &frame,Vec3 normal,Vec3 tangent) {
          const auto wn=world.normal({normal.x,normal.y,normal.z});
          const auto wt=world.direction({tangent.x,tangent.y,tangent.z});
          const auto N=normalized(Vec3{float(wn[0]),float(wn[1]),float(wn[2])});
          const auto T=normalized(Vec3{float(wt[0]),float(wt[1]),float(wt[2])});
          const auto B=normalized(cross(N,T))*((affine?-1.f:1.f)*at[3]);
          frame[0]+=T*weight;
          frame[1]+=B*weight;
          frame[2]+=N*weight;
        };
        accumulate(rest_columns,n,t);
        const auto &morph=frame_set.targets[0];
        accumulate(posed_columns,pose.pose[0].rotation.rotate(n+morph.normals[corner]*.8f),
            pose.pose[0].rotation.rotate(t+(morph.tangents.empty()?Vec3{}:morph.tangents[corner]*.8f)));
      }
      const auto actual=applied(tri,w);
      for (size_t role=0;role<actual.size();++role) {
        PaintSourceTexture texture;
        texture.image=role?original->mtoon():original->pbr();
        texture.wrap_s=texture.wrap_t=33071;
        texture.min_filter=texture.mag_filter=9729;
        const auto pixel=sample_source_texture(texture,uv[0],uv[1],false);
        const auto direction=[&](const auto &columns) {
          return normalized(normalized(columns[0])*(2*pixel[0]-1)+
                            normalized(columns[1])*(2*pixel[1]-1)+
                            normalized(columns[2])*(2*pixel[2]-1));
        };
        error=std::max(error,length(actual[role]-direction(posed_columns)));
        rest_difference=std::max(rest_difference,length(actual[role]-direction(rest_columns)));
      }
      ++samples;
    }
  }
  std::cout<<"POSED_PREPARED samples="<<samples<<" error="<<error<<" rest_difference="<<rest_difference<<'\n';
  check(samples>100 && error<.026f && rest_difference>.20f,
        "Both varying normal maps agree with independent morph, joint rotation and placement reconstruction");
  check(doc.undo() && doc.copy_scene().encode()==scene.encode() && doc.redo() &&
        doc.copy_scene().encode()==after.encode(),"History recovers the exact posed source and captured patch");
  settled(doc);
}

void first_material_patch(bool affine, const std::filesystem::path &out, bool posed) {
  Mesh mesh;
  mesh.positions={{-.5f,-.5f,0},{0,-.5f,0},{.5f,-.5f,0},
                  {-.5f,.5f,0},{0,.5f,0},{.5f,.5f,0}};
  mesh.normals.assign(6,{0,0,1});
  mesh.uv={{0,0},{.5f,0},{1,0},{0,1},{.5f,1},{1,1}};
  mesh.indices={0,1,4,0,4,3,1,2,5,1,5,4};
  Material left,right;
  left.name="Painted red surface";
  left.color={.85f,.15f,.07f,1};
  right.name="Painted blue surface";
  right.color={.1f,.25f,.9f,1};
  Scene scene;
  scene.active().visible=false;
  const auto guide=scene.add("Two source materials",Volume{});
  initialize_texture_sets(scene,guide,mesh,
      std::array{TextureSetInitialField{left,64},TextureSetInitialField{right,128}},
      std::array<TextureSetId,4>{0,0,1,1});
  scene.select(guide);
  if (affine)
    scene.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(
        std::array<double,16>{-1.1,.12,0,0,.23,.89,0,0,0,0,1,0,.08,-.04,.06,1}));
  auto world=scene.world_matrix(guide);
  auto unpainted=scene.snapshot();

  // A separate Bake donor leaves a real recipe and coverage on the primary set.
  const auto bake_source=scene.add("Original Bake donor",Volume{});
  auto &donor=scene.object(bake_source);
  donor.mesh_active=true;
  auto donor_mesh=mesh;
  if (posed) donor_mesh.normals.assign(6,normalized(Vec3{.44f,-.17f,1}));
  donor.painted=std::make_shared<PaintedMesh>(donor_mesh,64);
  donor.material.color={.6f,.25f,.08f,1};
  auto donor_placement=world.matrix();
  donor_placement[14]+=.008;
  donor.affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(donor_placement));
  donor.visible=false;
  scene.select(guide);
  ObjectBakeSettings bake;
  bake.ao_samples=0;
  bake.allow_backfaces=true;
  bake.front_mm=bake.back_mm=30;
  bake_object(scene,bake_source,bake);

  if (affine)
    check(bind_texture_set_mtoon(scene,guide,1,retopo_toon_binding(texture_set_view(scene.active(),1),true,false)),
          "The reflected guide carries an actual static MToon field with typed images");
  for (const auto set:{0u,1u}) {
    check(edit_texture_set(scene,guide,set,[&](PaintedMesh &paint,Material &) {
      paint.add_layer(set ? "Blue ink" : "Red ink");
      PaintSettings ink;
      ink.front_only=false;
      ink.color=set?right.color:left.color;
      Brush brush;
      brush.from=brush.to={set?.25f:-.25f,0,0};
      brush.normal={0,0,1};
      brush.radius=.21f;
      brush.strength=1;
      paint.begin_stroke();
      paint.stamp(brush,ink);
      if (!paint.end_stroke()) return false;
      if (!set) paint.layer_properties("Red ink",false,.65f);
      return true;
    }),"Original material slot has its own editable coat before retopology");
  }
  check(select_texture_set(scene,guide,1),"Secondary material is active before first Apply");
  auto evaluated=mesh;
  if (posed) {
    DeformationState pose(mesh);
    pose.morphs.emplace(mesh);
    MorphTarget target{"Sloping wide pose",{}};
    for (auto p:mesh.positions)
      target.delta.push_back({1.3f+.06f*p.x+.06f*p.y,-.2f+.12f*p.y,.11f+.40f*p.x-.17f*p.y});
    pose.morphs->targets.push_back(std::move(target));
    pose.morph_weights={.8f};
    if (affine) {
      pose.skeleton=Skeleton{{Bone{"Guide pose joint"}}};
      pose.skin=SkinWeights{};
      pose.skin->vertices.assign(mesh.positions.size(),{{0,1}});
      pose.pose.resize(1);
      pose.pose[0].translation={.13f,-.08f,.11f};
      pose.pose[0].rotation={.9855848f,0,.16918235f,0};
    }
    scene.set_deformation(guide,pose);
    unpainted.set_deformation(guide,pose);
    evaluated=pose.preview(mesh);
    check(SurfaceQuery(mesh).closest(evaluated.positions[1]).distance>.45f &&
          evaluated.indices==mesh.indices && evaluated.uv==mesh.uv,
          "The active pose moves the source beyond REST projection reach while preserving material corners");
    // Keep the evaluated guide in the default stereo field of view while its
    // REST mesh remains far enough away to prove which surface was sampled.
    const auto middle=(evaluated.positions[0]+evaluated.positions[5])*.5f;
    const auto offset=world.point({middle.x,middle.y,middle.z});
    auto placement=world.matrix();
    for(size_t axis=0;axis<3;++axis) placement[12+axis]-=offset[axis];
    for(auto *current:{&scene,&unpainted}) {
      if(affine) current->active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(placement));
      else current->active().transform.position={float(placement[12]),float(placement[13]),float(placement[14])};
    }
    world=scene.world_matrix(guide);
  }
  const auto guide_local=[&](Vec3 p) {
    return evaluated.positions[0]+(evaluated.positions[2]-evaluated.positions[0])*(p.x+.5f)
                                +(evaluated.positions[3]-evaluated.positions[0])*(p.y+.5f);
  };
  const auto guide_point=[&](Vec3 p) {return point(world,guide_local(p));};
  const auto exact=scene.encode();
  scene.save(out/"first-material-source.dsculpt");

  // A continuous triangle crossing the material boundary is ambiguous even
  // when its midpoint picks a unique material. Neither side may be smeared.
  RetopoMesh crossing;
  std::array<TopologyID,3> points;
  const std::array<Vec3,3> samples{{{-.4f,-.3f,0},{.3f,-.3f,0},{-.25f,.3f,0}}};
  for (size_t i=0;i<points.size();++i) points[i]=crossing.add_vertex({guide_point(samples[i])});
  if (affine) std::swap(points[1],points[2]);
  crossing.add_face(points);
  const auto check_crossing=[&](Scene &input) {
    const auto before=input.encode();
    bool refused=false;
    try { publish_retopology(input,crossing,guide,{}); }
    catch (const std::invalid_argument &error) {
      refused=std::string_view(error.what()).find("source material boundary")!=std::string_view::npos;
    }
    check(refused && input.encode()==before,"Crossing a material boundary refuses before changing the original scene");
  };
  check_crossing(unpainted);
  check_crossing(scene);

  Document doc;
  doc.replace(scene);
  settled(doc);
  doc.begin_retopology();
  drafted(doc);
  const auto make_face=[&](std::array<Vec3,4> corners) {
    for (auto corner:corners) touch(doc,guide_point(corner));
    doc.edit_retopology(RetopoAction::Face);
    drafted(doc);
  };
  make_face({{{-.37f,-.32f,0},{0,-.32f,0},{0,.32f,0},{-.37f,.32f,0}}});
  make_face({{{0,-.32f,0},{.37f,-.32f,0},{.37f,.32f,0},{0,.32f,0}}});
  const auto draft=doc.frame().state->retopology;
  check(draft->topology->faces().size()==2 && draft->topology->vertices().size()==6,
        "The native worker creates two adjoining faces with an explicit shared material border");
  doc.end_retopology(true);
  const auto project=out/"first-material-patch.dsculpt";
  doc.save_async(project).get();
  settled(doc);
  const auto after=doc.copy_scene();
  const auto &patch=after.active();
  check(patch.id!=guide && patch.retopology->source==guide && !after.object(guide).visible &&
        after.object(guide).texture_sets==scene.active().texture_sets && patch.texture_sets->active()==1,
        "First Apply retains the hidden guide and the active material on the new patch");

  const auto patch_world=after.world_matrix(patch.id);
  const auto original_from_patch=world.inverse()*patch_world;
  const SurfaceQuery guide_query(evaluated);
  for (const auto set:{0u,1u}) {
    const auto before=texture_set_view(scene.active(),set);
    const auto field=texture_set_view(patch,set);
    const auto &layer=field.painted->layers().back();
    check(field.name==before.name && field.painted->resolution()==before.painted->resolution() &&
          layer.id==before.painted->layers().back().id && layer.visible==bool(set) &&
          layer.opacity==(set?1.f:.65f) && field.bake_recipe==before.bake_recipe && field.domain->triangles().size()==2,
          "Each projected material keeps its own raster, layer state and Bake recipe");
    check(std::ranges::any_of(layer.tiles,[set](const auto &tile) {
            return tile && std::ranges::any_of(tile->pixels,[set](const auto &pixel) {
              return pixel.data[9]>128 && pixel.data[set?2:0]>195 && pixel.data[set?0:2]<150;
            });
          }),"The replacement's layers contain the correct slot's projected pigment");
    for (auto triangle:field.domain->triangles()) {
      const auto &m=field.painted->mesh();
      Vec3 centre{};
      for (size_t c=0;c<3;++c) centre+=m.positions[m.indices[triangle*3+c]]/3.f;
      const auto hit=guide_query.closest(point(original_from_patch,centre));
      check(hit.distance<1e-5f && scene.active().texture_sets->triangle_sets()[hit.triangle]==set,
            "Every emitted triangle lies on the proper source material side in evaluated geometry");
    }
  }
  check(patch.painted->baked() && patch.bake_recipe->source.object==bake_source &&
        std::ranges::any_of(patch.painted->baked()->coverage,[](auto a) {return a>128;}),
        "The original colour and normal Bake remain covered on the replacement atlas");
  if (posed) {
    check(!patch.deformation && after.object(guide).deformation==scene.active().deformation,
          "The patch captures the visible geometry and the hidden guide retains its original pose and rig");
    const auto canonical_normal=[](const Mesh &surface,const AffineTransform &placement,
        const Tangent &t, uint32_t corner, const uint8_t *rgba) {
      const auto stretch=placement.direction({t[0],t[1],t[2]});
      const auto n=surface.normals[surface.indices[corner]];
      const auto inverse=placement.normal({n.x,n.y,n.z});
      const auto tangent=normalized(Vec3{float(stretch[0]),float(stretch[1]),float(stretch[2])});
      const auto normal=normalized(Vec3{float(inverse[0]),float(inverse[1]),float(inverse[2])});
      const float hand=t[3]*(placement.orientation()==AffineOrientation::Reversing?-1.f:1.f);
      return normalized(tangent*(rgba[0]/127.5f-1)+cross(normal,tangent)*hand*(rgba[1]/127.5f-1)+
                        normal*(rgba[2]/127.5f-1));
    };
    const auto prior=scene.active().painted->baked();
    const auto previous=std::ranges::find(prior->coverage,uint8_t(255))-prior->coverage.begin();
    check(size_t(previous)<prior->coverage.size() &&
          std::abs(int(prior->maps.normal.rgba[4*previous])-128)>20,
          "The source Bake contains authored off-axis normal detail");
    const auto old_tangents=tangent_corners(evaluated);
    const auto rest_tangents=tangent_corners(mesh);
    const auto *old_sample=prior->maps.normal.rgba.data()+4*previous;
    check(length(canonical_normal(evaluated,world,old_tangents[0],0,old_sample)-
                 canonical_normal(mesh,world,rest_tangents[0],0,old_sample))>.1f,
          "Using the REST normal frame would visibly change the posed Bake shading");
    const auto &target=patch.painted->mesh();
    const auto tangents=tangent_corners(target);
    const auto &output=*patch.painted->baked();
    const float r=patch.painted->resolution();
    size_t tested=0;
    float maximum=0;
    for (const auto face:patch.texture_sets->primary_domain()->triangles()) {
      const auto a=target.uv[target.indices[face*3]], b=target.uv[target.indices[face*3+1]], c=target.uv[target.indices[face*3+2]];
      const std::array u{b[0]-a[0],b[1]-a[1]}, v{c[0]-a[0],c[1]-a[1]};
      const float area=u[0]*v[1]-u[1]*v[0];
      for (size_t i=0;i<output.coverage.size();++i) {
        if(output.coverage[i]<250) continue;
        const float x=(float(i%size_t(r))+.5f)/r-a[0],y=(float(i/size_t(r))+.5f)/r-a[1];
        const float w1=(x*v[1]-y*v[0])/area,w2=(u[0]*y-u[1]*x)/area;
        if(std::min({w1,w2,1-w1-w2})<.12f) continue;
        const auto p=target.positions[target.indices[face*3]]*(1-w1-w2)+
                     target.positions[target.indices[face*3+1]]*w1+target.positions[target.indices[face*3+2]]*w2;
        const auto source=guide_query.closest(point(original_from_patch,p)).triangle;
        const auto old=canonical_normal(evaluated,world,old_tangents[source*3],source*3,old_sample);
        const auto now=canonical_normal(target,patch_world,tangents[face*3],face*3,output.maps.normal.rgba.data()+4*i);
        maximum=std::max(maximum,length(old-now));
        ++tested;
      }
    }
    std::cout<<"POSED_NORMAL samples="<<tested<<" max_chord="<<maximum<<'\n';
    check(tested>30 && maximum<.021f,"An independent placed tangent-frame check preserves posed Bake normals");
  }
  if (affine) {
    const auto old=texture_set_view(scene.active(),1).material_bindings;
    const auto field=texture_set_view(patch,1).material_bindings;
    check(field->has_mtoon && field->roles[0].image!=old->roles[0].image &&
          field->roles[0].image->width==128 && field->roles[size_t(MToonSlot::Matcap)].image==
          old->roles[size_t(MToonSlot::Matcap)].image,
          "The static MToon field projects independent role images and retains its view-dependent MatCap");
  }
  const auto accepted=after.encode();
  check(Scene::load(project).encode()==accepted && doc.undo() && doc.copy_scene().encode()==exact &&
        doc.redo() && doc.copy_scene().encode()==accepted,
        "Ordered Save, cold Open and one Undo/Redo preserve both material-bearing objects");
  settled(doc);
  const auto glb=out/"first-material-editable.glb";
  doc.export_asset(glb,AssetExportFormat::EditableGlb256).get();
  check(Scene::load(glb).encode()==accepted,"Editable GLB restores the complete transferred patch and original guide");
  if (!affine) {
    const auto path=out/"first-material-standard.glb";
    doc.export_asset(path,AssetExportFormat::Glb).get();
    ImportLimits limits;
    limits.mode=ImportMode::PreserveGltf;
    const auto exported=import_mesh(path,limits);
    check(exported.primitives.size()==2 && exported.primitives.front().material!=exported.primitives.back().material,
          "An independent GLB reader sees both materials on the new shared-surface patch");
  }
  const auto move=move_patch(doc,affine);
  doc.end_retopology(true);
  const auto edited=out/"first-material-reapplied.dsculpt";
  doc.save_async(edited).get();
  settled(doc);
  const auto replacement=doc.copy_scene();
  check(replacement.active().retopology->topology.encode()==move->topology->encode() &&
        std::ranges::equal(replacement.active().texture_sets->triangle_sets(),patch.texture_sets->triangle_sets()) &&
        !replacement.object(patch.id).visible && replacement.object(patch.id).texture_sets==patch.texture_sets &&
        Scene::load(edited).encode()==replacement.encode(),
        "Reapply changes the newly transferred patch, preserving all slot assignments and its hidden prior patch");
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
  constexpr auto base=size_t(MToonSlot::Base), shade=size_t(MToonSlot::Shade);
  for (const auto slot:{TextureSetId(0),trim}) {
    auto typed=retopo_toon_binding(texture_set_view(scene.active(),slot),slot==trim,legacy && slot==trim);
    typed.roles[shade]=typed.roles[base];
    typed.material.textures[shade].index=typed.material.textures[base].index;
    if (slot==trim && !legacy) {
      if (affine) typed.material.textures[shade].offset[0]+=.09f;
      else if (detached) typed.roles[shade].mag_filter=9728;
      else typed.roles[shade].wrap_t=33071;
    }
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
  const auto loaded=Scene::load(path);
  check(active.id!=previous && active.texture_sets->triangle_sets()[1]==trim &&
        active.retopology->topology.encode()!=before.active().retopology->topology.encode() &&
        !after.object(previous).visible && loaded.encode()==after.encode(),
        "Geometry Apply and ordered Save keep two texture sets and the hidden original project");
  size_t checks_at_texel=0;
  float role_error=0, normal_error=0;
  const auto old_world=before.world_matrix(previous), new_world=after.world_matrix(active.id);
  const SurfaceQuery query(before.active().painted->mesh());
  for (const auto slot:{TextureSetId(0),trim}) {
    const auto old=texture_set_view(before.active(),slot);
    const auto field=texture_set_view(active,slot);
    const auto &source=*old.material_bindings, &target=*field.material_bindings;
    const auto &reopened=*texture_set_view(loaded.active(),slot).material_bindings;
    const bool shared=slot==0 || legacy;
    check(source.roles[base].image==source.roles[shade].image &&
          (target.roles[base].image==target.roles[shade].image)==shared &&
          (reopened.roles[base].image==reopened.roles[shade].image)==shared,
          "Apply and cold Open preserve Base/Shade sharing only when the source samplers agree");
    const auto r=target.roles[0].image->width;
    check(old.painted->resolution()==64 && r==(slot==trim?128u:64u) &&
          target.roles[0].image!=source.roles[0].image &&
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
      const auto stride=std::max(r/32,1u);
      for (uint32_t y=0;y<r;y+=stride) for (uint32_t x=0;x<r;x+=stride) {
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
    for (const auto slot:{TextureSetId(0),trim}) {
      if (!texture_set_view(portable.active(),slot).painted->has_paint_data()) continue;
      check(edit_texture_set(portable,active.id,slot,[](PaintedMesh &paint,Material &) {
        paint.add_layer("Unpainted export control");
        while (paint.layers().size()>1) {
          paint.select_layer(paint.layers().front().id);
          paint.delete_layer();
        }
        return !paint.has_paint_data();
      }),"The independent export control removes inherited and additional paint from this material slot");
    }
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
      mode!="first-material-plain" && mode!="first-material-affine" &&
      mode!="first-material-posed-plain" && mode!="first-material-posed-affine" &&
      mode!="first-prepared-posed-plain" && mode!="first-prepared-posed-affine" &&
      mode!="mtoon-plain" && mode!="mtoon-affine" && mode!="mtoon-v0" && mode!="mtoon-detached" &&
      mode!="reapply-plain" && mode!="reapply-affine" && mode!="prepared-plain" && mode!="prepared-affine" &&
      mode!="prepared-detached" && mode!="prepared-detached-affine" &&
      mode!="prepared-color-plain" && mode!="prepared-color-affine" &&
      mode!="prepared-color-normal-plain" && mode!="prepared-color-normal-affine" &&
      mode!="prepared-color-normal-first" &&
      mode!="prepared-color-normal-uv-plain" && mode!="prepared-color-normal-uv-affine" &&
      mode!="restore-plain" && mode!="restore-affine" && mode!="material-seam-plain" &&
      mode!="material-seam-affine" && mode!="subdivide-seam-plain" && mode!="subdivide-seam-affine") return 2;
  const std::filesystem::path out = argv[1];
  std::filesystem::create_directories(out);
  setenv("XDG_STATE_HOME",(out/"state").c_str(),1);
  setenv("XDG_CONFIG_HOME",(out/"config").c_str(),1);
  if (mode.starts_with("restore-")) restore_patch_history(mode.ends_with("affine"),out);
  else if (mode=="affine-wire") affine_wire_edit(out);
  else if (mode=="mixed-material") mixed_material_quad(out);
  else if (mode.starts_with("first-material-")) first_material_patch(mode.ends_with("affine"),out,mode.starts_with("first-material-posed-"));
  else if (mode.starts_with("first-prepared-")) first_prepared_patch(mode.ends_with("affine"),out);
  else if (mode.starts_with("material-seam-")) split_material_seam(mode.ends_with("affine"),out);
  else if (mode.starts_with("subdivide-seam-")) split_material_seam(mode.ends_with("affine"),out,true);
  else if (mode.starts_with("mtoon-")) retopo_toon_materials(mode.ends_with("affine"),mode.ends_with("v0"),out,mode=="mtoon-detached");
  else if (mode=="prepared-color-normal-first") prepared_color_edit(false,out,PreparedColorOrder::NormalFirst);
  else if (mode.starts_with("prepared-color-normal-uv-"))
    prepared_color_edit(mode.ends_with("affine"),out,PreparedColorOrder::ColorFirst,true);
  else if (mode.starts_with("prepared-color-")) prepared_color_edit(mode.ends_with("affine"),out,
      mode.starts_with("prepared-color-normal-") ? PreparedColorOrder::ColorFirst : PreparedColorOrder::ColorOnly);
  else if (mode.starts_with("prepared-")) prepared_geometry_edit(mode.ends_with("affine"),out,mode.starts_with("prepared-detached"));
  else if (mode.starts_with("reapply-")) reapply_patch(mode.ends_with("affine"),out);
  else bind_patch(mode=="affine",out);
  std::cout << "PASS " << checks << " retopology Surface Heat controls\n";
} catch(const std::exception &error) {
  std::cerr << "FAIL " << error.what() << '\n';
  return 1;
}
