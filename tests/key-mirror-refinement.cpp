// SPDX-License-Identifier: GPL-3.0-or-later
#include "mirror-host-allocation-fault.hpp"
namespace allocation=mirror_failure;
#include "key-mirror-refinement.hpp"
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <iostream>
using namespace dome;
using namespace dome::detail;
namespace {
size_t checks = 0;
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
  ++checks;
  std::cout << "PASS " << m << std::endl;
}
bool same_positions(const std::vector<Vec3> &a, const std::vector<Vec3> &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].z != b[i].z)
      return false;
  return true;
}
bool refuses(auto f, std::string_view diagnostic) {
  try {
    f();
    return false;
  } catch (const std::exception &e) {
    std::cout << "REFUSAL " << e.what() << std::endl;
    return std::string_view(e.what()).find(diagnostic) != std::string_view::npos;
  }
}
std::shared_ptr<const Mesh> square() {
  Mesh m;
  m.positions = {{-.5f, -.5f, 0}, {.5f, -.5f, 0}, {.5f, .5f, 0}, {-.5f, .5f, 0}};
  m.normals.assign(4, {0, 0, 1});
  m.uv = {{{0, 0}}, {{1, 0}}, {{1, 1}}, {{0, 1}}};
  m.indices = {0, 1, 2, 0, 2, 3};
  return std::make_shared<const Mesh>(std::move(m));
}
Vec3 reflect(Vec3 p) { return {-p.x, p.y, p.z}; }
Vec3 at(const Mesh &m, const MirrorSurfacePoint &s) {
  Vec3 p;
  for (size_t k = 0; k < 3; ++k)
    p += m.positions[m.indices[s.triangle * 3 + k]] * float(s.barycentric[k]);
  return p;
}
bool same(const KeyMirrorRefinement &a, const KeyMirrorRefinement &b) {
  if (a.source != b.source || a.opposite != b.opposite || a.corners.size() != b.corners.size() ||
      a.maximum_rest_error != b.maximum_rest_error || a.work_units != b.work_units ||
      a.plane_offset != b.plane_offset)
    return false;
  for (size_t c = 0; c < a.corners.size(); ++c) {
    const auto &x = a.corners[c];
    const auto &y = b.corners[c];
    if (x.position.x != y.position.x || x.position.y != y.position.y ||
        x.position.z != y.position.z || x.current.triangle != y.current.triangle ||
        x.current.barycentric != y.current.barycentric ||
        x.reflected.triangle != y.reflected.triangle ||
        x.reflected.barycentric != y.reflected.barycentric)
      return false;
  }
  return true;
}
// Independent physical piecewise-linear field, not a runtime correspondence.
double field(double x, double y) {
  const double u = x + .5, v = y + .5;
  return v <= u ? .04 * (u - v) - .06 * v : -.06 * u + .09 * (v - u);
}
double sampled(const MirrorSurfacePoint &p) {
  constexpr double z[] = {0, .04, -.06, 0, -.06, .09};
  double sum = 0;
  for (size_t k = 0; k < 3; ++k)
    sum += z[p.triangle * 3 + k] * p.barycentric[k];
  return sum;
}

Scene native_fixture(){
  Scene scene;
  const auto id=scene.active().id;
  auto mesh=*square();
  mesh.uv_pins={1,0,0,0};
  mesh.uv_seams={1,0,0,0,0,0};
  auto &object=scene.active();
  object.mesh_active=true;
  object.painted=std::make_shared<PaintedMesh>(mesh,64);
  Brush brush;brush.from=brush.to={-.12f,-.03f,0};brush.normal={0,0,1};brush.radius=.24f;
  PaintSettings ink;ink.color={.12f,.6f,.35f,.8f};
  object.painted->begin_stroke();object.painted->stamp(brush,ink);object.painted->end_stroke();
  object.painted->add_layer("Hidden original");
  brush.from=brush.to={.22f,.07f,0};ink.color={.8f,.15f,.1f,1};
  object.painted->begin_stroke();object.painted->stamp(brush,ink);object.painted->end_stroke();
  object.painted->layer_properties("Hidden original",false,.4f);
  ShapeKeyEdit edit;edit.name="Asymmetric fold";
  author_shapekey(scene,id,edit);
  edit.action=ShapeKeyAction::Replace;edit.positions=mesh.positions;
  for(auto &p:edit.positions)p.z=float(field(p.x,p.y));
  author_shapekey(scene,id,edit);
  auto deformation=*scene.active().deformation;deformation.morph_weights={.25f};
  scene.set_deformation(id,std::move(deformation));
  return scene;
}

void check_authored(const Scene &before,const Scene &after){
  const auto &a=before.active(),&b=after.active();
  const auto &m=b.painted->mesh();
  const auto &keys=b.deformation->morphs->targets;
  check(m.indices.size()>a.painted->mesh().indices.size()&&keys.size()==2&&
        b.deformation->morph_weights==std::vector<float>({.25f,0}),
        "Native Mirror explicitly refines the topology and carries both key weights");
  bool interpolated=false;
  for(size_t v=0;v<m.positions.size();++v){
    const auto p=m.positions[v];
    if(std::abs(keys[0].delta[v].z-field(p.x,p.y))>1e-7||std::abs(keys[1].delta[v].z-field(-p.x,p.y))>1e-7)
      throw std::runtime_error("Authored native key disagrees with the independent continuous-field oracle");
    interpolated|=std::abs(p.x)<.49f&&std::abs(p.y)<.49f;
  }
  check(interpolated,"Prior and mirrored keys preserve their physical fields at new interior vertices");
  check(a.painted->encode()["layers"]==b.painted->encode()["layers"]&&
        a.painted->preview(a.material)->color.rgba==b.painted->preview(b.material)->color.rgba,
        "Every hidden and visible paint layer and the composited UV pixels survive refinement");
  check(std::any_of(m.uv_pins.begin(),m.uv_pins.end(),[](auto pin){return pin;})&&
        std::any_of(m.uv_seams.begin(),m.uv_seams.end(),[](auto seam){return seam;}),
        "Explicit original UV pins and seams carry into the refined surface");
  auto restored=after;
  ShapeKeyEdit restore;restore.action=ShapeKeyAction::Restore;
  author_shapekey(restored,b.id,restore);
  check(restored.encode()==before.encode(),"Restore recovers the exact pre-refinement mesh, frame data and layers");
}

void workflow(const std::filesystem::path &out){
  std::filesystem::create_directories(out);
  const auto source=native_fixture();
  auto direct=source;
  ShapeKeyEdit edit;edit.action=ShapeKeyAction::Mirror;edit.mirror_frames=ShapeKeyMirrorFrames::RefinedSurface;
  edit.name="Reflected fold";
  const auto id=source.active().id;
  auto skinned=source;
  auto rig=*skinned.active().deformation;
  rig.skeleton=Skeleton{{{"root",-1,{},1}}};
  rig.skin=SkinWeights{};rig.skin->vertices.assign(4,{{0,1}});
  skinned.set_deformation(id,std::move(rig));
  const auto before_skin=skinned.encode();
  check(refuses([&]{author_shapekey(skinned,id,edit);},"unskinned")&&skinned.encode()==before_skin,
        "A skinned source is refused with its exact mesh, paint and binding intact");
  size_t polls=0;
  check(refuses([&]{author_shapekey(direct,id,edit,[&]{return ++polls>15;});},"cancelled")&&
        direct.encode()==source.encode(),"Cancelled refinement publishes no part of the detached topology");
  author_shapekey(direct,id,edit);
  check_authored(source,direct);
  Document document;document.replace(source);
  document.author_shape(edit);
  const auto file=out/"refined-mirror.dsculpt";
  document.save_async(file).get();
  check(document.wait_asset()&&document.wait_mesh()&&document.frame().state->error.empty(),
        "The ordered native worker publishes its refined mesh and Save");
  check(Scene::load(file).encode()==direct.encode()&&document.undo()&&document.copy_scene().encode()==source.encode()&&
        document.redo()&&document.copy_scene().encode()==direct.encode(),
        "Refined Mirror saves and reopens exactly and is one complete Undo/Redo step");
  auto reopened=Scene::load(file);
  ShapeKeyEdit replace;replace.action=ShapeKeyAction::Replace;replace.target=1;
  replace.positions=reopened.active().painted->mesh().positions;
  const auto &mirrored=reopened.active().deformation->morphs->targets[1].delta;
  for(size_t v=0;v<replace.positions.size();++v)replace.positions[v]+=mirrored[v];
  replace.positions[0].z+=.005f;
  check(author_shapekey(reopened,id,replace).changed&&Scene::decode(reopened.encode()).encode()==reopened.encode(),
        "The saved refined key remains editable and its later endpoint reopens exactly");
  source.save(out/"source.dsculpt");
}
} // namespace
int main(int argc,char **argv) try {
  const auto source = square();
  if(argc==3&&std::string_view(argv[1])=="--ui-fixture"){native_fixture().save(argv[2]);return 0;}
  if(argc==3&&std::string_view(argv[1])=="--xr-fixture"){
    auto scene=native_fixture();scene.active().transform.position={0,1.3f,-1.1f};scene.save(argv[2]);return 0;
  }
  if(argc==3&&std::string_view(argv[1])=="--authoring"){workflow(argv[2]);return 0;}
  if(argc==4&&std::string_view(argv[1])=="--verify-ui"){
    check_authored(Scene::load(argv[2]),Scene::load(argv[3]));return 0;
  }
  if(argc==2&&std::string_view(argv[1])=="--require-device"){
    try{refine_reflected_key(source,1e-6f);}
    catch(const std::exception &error){std::cout<<error.what()<<'\n';return 0;}
    throw std::runtime_error("Mirror refinement accepted an unavailable GPU");
  }
  if(argc!=1)return 2;
  const auto positions = source->positions;
  const auto indices = source->indices;
  check(refuses([&] { reflected_key_correspondence(*source, 1e-6f); }, "triangle refinement"),
        "Genuine opposite diagonals cannot use the historical corner bijection");
  const auto result = refine_reflected_key(source, 1e-6f);
  check(same(result, refine_reflected_key(source, KeyMirrorPlane{0}, 1e-6f)),
        "Historical zero-plane overload retains exact default geometry/provenance/work semantics");
  auto shifted = std::make_shared<Mesh>(*source);
  for (auto &p : shifted->positions)
    p.x += .125f;
  const auto shifted_positions = shifted->positions;
  const auto shifted_result = refine_reflected_key(shifted, KeyMirrorPlane{.125}, 1e-6f);
  bool plane_exact = shifted_result.plane_offset == .125 && shifted_result.source == shifted &&
                     !shifted_result.source.owner_before(shifted) &&
                     !shifted.owner_before(shifted_result.source) &&
                     shifted_result.corners.size() == result.corners.size() &&
                     shifted_result.opposite == result.opposite;
  for (size_t c = 0; c < result.corners.size() && plane_exact; ++c) {
    const auto &a = result.corners[c], &b = shifted_result.corners[c];
    plane_exact &= length(b.position - (a.position + Vec3{.125f, 0, 0})) < 1e-7f &&
                   a.current.triangle == b.current.triangle &&
                   a.current.barycentric == b.current.barycentric &&
                   a.reflected.triangle == b.reflected.triangle &&
                   a.reflected.barycentric == b.reflected.barycentric;
  }
  check(plane_exact && same_positions(shifted->positions, shifted_positions),
        "Explicit captured plane retains real source coordinates and exact source owners without "
        "recentering");
  check(refuses([&] { refine_reflected_key(shifted, 1e-6f); }, "hole") &&
            same_positions(shifted->positions, shifted_positions),
        "Default user zero plane is never silently shifted to the source center");
  check(refuses([&] { refine_reflected_key(source, KeyMirrorPlane{NAN}, 1e-6f); }, "plane"),
        "Nonfinite explicit plane refuses before derived output");
  check(result.source == source && source->positions.size() == 4 &&
            result.corners.size() > source->indices.size() && result.corners.size() % 3 == 0,
        "Detached refinement keeps the genuine raw4 source and owns newly derived corners");
  double area = 0, field_error = 0, normal_error = 0, tangent_error = 0;
  bool interior = false;
  for (size_t c = 0; c < result.corners.size(); ++c) {
    const auto &v = result.corners[c];
    const auto &o = result.corners[result.opposite[c]];
    check(length(at(*source, v.current) - v.position) < 1e-6f &&
              length(reflect(at(*source, v.reflected)) - v.position) < 1e-6f,
          "Every derived corner has actual source-triangle barycentrics on both sides");
    check(result.opposite[result.opposite[c]] == c &&
              length(v.position - reflect(o.position)) < 1e-6f,
          "Explicit opposite corners form a genuine reflection involution");
    field_error =
        std::max(field_error, std::abs(sampled(v.reflected) - field(-v.position.x, v.position.y)));
    for (auto w : v.current.barycentric)
      interior |= w > 1e-6 && w < 1 - 1e-6;
  }
  for (size_t c = 0; c < result.corners.size(); c += 3) {
    Vec3 p[3];
    std::array<float, 2> uv[3];
    double x = 0, y = 0;
    for (size_t k = 0; k < 3; ++k) {
      const auto &v = result.corners[c + k];
      p[k] = v.position;
      p[k].z += float(sampled(v.reflected));
      uv[k] = {v.position.x + .5f, v.position.y + .5f};
      x += v.position.x / 3.;
      y += v.position.y / 3.;
    }
    area += length(cross(result.corners[c + 1].position - result.corners[c].position,
                         result.corners[c + 2].position - result.corners[c].position)) *
            .5;
    const bool lower = y + .5 <= -x + .5;
    const double fx = lower ? -.04 : .15, fy = lower ? -.10 : .09;
    const auto expected_n = normalized(Vec3{float(-fx), float(-fy), 1});
    const auto actual_n = normalized(cross(p[1] - p[0], p[2] - p[0]));
    normal_error = std::max(normal_error, double(length(expected_n - actual_n)));
    const auto e1 = p[1] - p[0], e2 = p[2] - p[0];
    const float du1 = uv[1][0] - uv[0][0], dv1 = uv[1][1] - uv[0][1], du2 = uv[2][0] - uv[0][0],
                dv2 = uv[2][1] - uv[0][1];
    const auto actual_t = normalized((e1 * dv2 - e2 * dv1) / (du1 * dv2 - du2 * dv1));
    const auto expected_t = normalized(Vec3{1, 0, float(fx)});
    tangent_error = std::max(tangent_error, double(length(actual_t - expected_t)));
  }
  std::cout << "REFINEMENT triangles=" << result.corners.size() / 3 << " area=" << area
            << " field=" << field_error << " N=" << normal_error << " T=" << tangent_error
            << std::endl;
  check(interior && std::abs(area - 1) < 1e-6 && field_error < 1e-7,
        "Nonmatching diagonals retain full area and exact mirrored piecewise field, including real "
        "interior points");
  check(normal_error < 1e-6 && tangent_error < 1e-6,
        "Independent analytic slope and UV derivative verify geometry-provenance N/T construction");
  bool independent_seam = false;
  for (size_t i = 0; i < result.corners.size(); ++i)
    for (size_t j = i + 1; j < result.corners.size(); ++j) {
      const auto &a = result.corners[i];
      const auto &b = result.corners[j];
      if (length(a.position - b.position) < 1e-7f && a.reflected.triangle != b.reflected.triangle) {
        const double x = sampled(a.reflected) + (a.reflected.triangle ? .03 : -.02),
                     y = sampled(b.reflected) + (b.reflected.triangle ? .03 : -.02);
        independent_seam |= std::abs(x - y) > .049;
      }
    }
  check(independent_seam, "Coincident derived corners keep distinct original face fields without "
                          "choosing or averaging their seam");
  auto reversed_source = std::make_shared<Mesh>(*source);
  for (size_t c = 0; c < reversed_source->indices.size(); c += 3)
    std::swap(reversed_source->indices[c + 1], reversed_source->indices[c + 2]);
  reversed_source->normals.assign(4, {0, 0, -1});
  const auto reverse_result = refine_reflected_key(reversed_source, 1e-6f);
  bool reverse_winding = true;
  for (size_t c = 0; c < reverse_result.corners.size(); c += 3)
    reverse_winding &=
        cross(reverse_result.corners[c + 1].position - reverse_result.corners[c].position,
              reverse_result.corners[c + 2].position - reverse_result.corners[c].position)
            .z < 0;
  check(reverse_winding, "Refinement preserves actual original clockwise source winding rather "
                         "than imposing projection order");
  auto vertical = std::make_shared<Mesh>(*source);
  for (auto &p : vertical->positions)
    p = {0, p.x, p.y};
  vertical->normals.assign(4, {1, 0, 0});
  const auto vertical_result = refine_reflected_key(vertical, 1e-6f);
  bool vertical_winding = true;
  for (size_t c = 0; c < vertical_result.corners.size(); c += 3)
    vertical_winding &=
        cross(vertical_result.corners[c + 1].position - vertical_result.corners[c].position,
              vertical_result.corners[c + 2].position - vertical_result.corners[c].position)
            .x > 0;
  check(vertical_winding, "Geometry-only reflection on its fixed plane retains actual corner "
                          "winding; it mints no role-normal parity certificate");
  size_t low = 1, high = 1024 * 1024;
  while (low < high) {
    const size_t mid = low + (high - low) / 2;
    KeyMirrorLimits bound;
    bound.max_bytes = mid;
    bool accepted = false;
    try {
      accepted = same(result, refine_reflected_key(source, 1e-6f, bound));
    } catch (const std::length_error &) {
    }
    if (accepted)
      high = mid;
    else
      low = mid + 1;
  }
  KeyMirrorLimits exact_cap;
  exact_cap.max_bytes = low;
  check(same(result, refine_reflected_key(source, 1e-6f, exact_cap)),
        "Exact conservative prospective byte cap accepts deterministic output");
  exact_cap.max_bytes = low - 1;
  check(refuses([&] { refine_reflected_key(source, 1e-6f, exact_cap); }, "quota"),
        "One byte below the conservative prospective cap refuses before the prohibited allocation");
  std::cout << "EXACT_FORECAST_CAP " << low << std::endl;
  auto quantized = std::make_shared<Mesh>(*source);
  for (auto &p : quantized->positions)
    p.y += 8388600.f;
  check(
      refuses([&] { refine_reflected_key(quantized, 1e-6f); }, "represent"),
      "Derived float positions cannot hide barycentric quantization outside the actual tolerance");
  auto duplicate = std::make_shared<Mesh>(*source);
  duplicate->indices.insert(duplicate->indices.end(), source->indices.begin(),
                            source->indices.end());
  check(refuses([&] { refine_reflected_key(duplicate, 1e-6f); }, "ambiguous"),
        "Coincident distinct source faces cannot silently borrow a nearest seam owner");
  auto asymmetric = std::make_shared<Mesh>(*source);
  asymmetric->positions[1].x += .1f;
  check(refuses([&] { refine_reflected_key(asymmetric, 1e-6f); }, "hole"),
        "Unrepresented reflected geometry refuses without widening the declared tolerance");
  auto oversized = std::make_shared<Mesh>();
  oversized->positions.resize(2'000'001);
  check(refuses([&] { refine_reflected_key(oversized, 1e-6f); }, "source streams"),
        "Input stream cardinality is bounded before uninterruptible mesh validation");
  oversized.reset();
  KeyMirrorLimits limits;
  limits.max_bytes = 1;
  check(refuses([&] { refine_reflected_key(source, 1e-6f, limits); }, "quota"),
        "Actual scratch/output allocation is preflighted against its unchanged explicit cap");
  limits = {};
  limits.max_work = 1;
  check(refuses([&] { refine_reflected_key(source, 1e-6f, limits); }, "work"),
        "Actual pair work is bounded before broad-phase arrays");
  limits = {};
  size_t polls = 0;
  limits.cancelled = [&] {
    ++polls;
    return false;
  };
  refine_reflected_key(source, 1e-6f, limits);
  for (size_t cut : {size_t(1), polls / 2, polls}) {
    size_t at = 0;
    limits.cancelled = [&] { return ++at >= cut; };
    check(refuses([&] { refine_reflected_key(source, 1e-6f, limits); }, "cancelled") &&
              same_positions(source->positions, positions) && source->indices == indices,
          "Early middle and late cancellation return no partial derived domain or changed source");
  }
  limits = {};
  limits.max_work = result.work_units;
  check(same(result, refine_reflected_key(source, 1e-6f, limits)),
        "Exact measured work boundary accepts");
  limits.max_work = result.work_units - 1;
  check(refuses([&] { refine_reflected_key(source, 1e-6f, limits); }, "work"),
        "One work unit below refuses without a partial returned domain");
  allocation::calls = 0;
  allocation::count = true;
  auto measured = refine_reflected_key(source, 1e-6f);
  allocation::count = false;
  const auto calls = allocation::calls;
  check(calls > 0 && calls < 1000,
        "Detached common refinement has a finite measured allocation frontier");
  for (size_t cut = 0; cut < calls; ++cut) {
    allocation::remaining = long(cut);
    bool failed = false;
    try {
      refine_reflected_key(source, 1e-6f);
    } catch (const std::bad_alloc &) {
      failed = true;
    } catch (...) {
      allocation::remaining = -1;
      throw;
    }
    allocation::remaining = -1;
    if (!failed || !same_positions(source->positions, positions) || source->indices != indices)
      throw std::runtime_error("Persistent common-refinement allocation rollback failed at " +
                               std::to_string(cut));
  }
  check(same(result, refine_reflected_key(source, 1e-6f)),
        "All persistent allocation denials leave an exact deterministic retry and original source");
  std::cout << "ALLOCATION_PREFIXES " << calls << " POLLS " << polls << " WORK_UNITS "
            << result.work_units << '\n';

  auto broad=std::make_shared<Mesh>();
  constexpr size_t squares=5000;
  broad->positions.reserve(squares*4);
  broad->indices.reserve(squares*6);
  for(size_t i=0;i<squares;++i){
    const auto start=uint32_t(broad->positions.size());
    for(auto p:positions)broad->positions.push_back({p.x,p.y+float(i)*2,p.z});
    for(auto v:indices)broad->indices.push_back(start+v);
  }
  broad->normals.assign(broad->positions.size(),{0,0,1});
  const auto scaled=refine_reflected_key(broad,.002f);
  bool exact=scaled.corners.size()==squares*result.corners.size();
  for(size_t i=0;i<scaled.corners.size()&&exact;++i){
    const auto &c=scaled.corners[i];
    const auto &expected=result.corners[i%result.corners.size()];
    const auto square_index=i/result.corners.size();
    exact=c.current.triangle==square_index*2+expected.current.triangle&&
      c.reflected.triangle==square_index*2+expected.reflected.triangle&&
      c.current.barycentric==expected.current.barycentric&&c.reflected.barycentric==expected.reflected.barycentric&&
      scaled.opposite[i]==square_index*result.corners.size()+result.opposite[i%result.corners.size()];
  }
  check(exact&&scaled.work_units<3'000'000,"10,000 triangles pass the old pair-count limit and match every ordered corner and barycentric oracle");
  std::cout<<"INDEXED_REFINEMENT "<<broad->indices.size()/3<<" input triangles "<<scaled.corners.size()/3
           <<" output triangles "<<scaled.work_units<<" work units\nPASS "<<checks<<" detached common-refinement checks\n";
  return 0;
} catch (const std::exception &e) {
  allocation::remaining = -1;
  allocation::count = false;
  std::cerr << "FAIL " << e.what() << std::endl;
  return 1;
}
