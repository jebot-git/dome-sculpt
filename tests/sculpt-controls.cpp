// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "brush_kernel.hpp"
#include "sculpt-symmetry.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
#ifdef DOME_TEST_INSTRUMENTED
namespace {
size_t dispatches = 0;
bool backend_failure = false, backend_completed = false;
int64_t fail_after_compute = -1;
thread_local int64_t allocations_until_failure = -1;
}
void *operator new(size_t n) {
  if (allocations_until_failure == 0) throw std::bad_alloc();
  if (allocations_until_failure > 0) --allocations_until_failure;
  if (auto p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }
namespace dome {
void instrumented_brush(std::span<Sample>, const Brush &) asm("__wrap__ZN4dome11apply_brushESt4spanINS_6SampleELm18446744073709551615EERKNS_5BrushE");
void instrumented_brush(std::span<Sample> samples, const Brush &b) {
  ++dispatches;
  if (backend_failure) throw std::runtime_error("Instrumented compute failure");
  for (auto &sample : samples) sample.distance = brush_sample(sample,b);
  backend_completed = true;
  if (fail_after_compute >= 0) { allocations_until_failure = fail_after_compute; fail_after_compute = -1; }
}
std::string_view instrumented_backend() asm("__wrap__ZN4dome15compute_backendEv");
std::string_view instrumented_backend() { return "INSTRUMENTED"; }
}
#endif
using namespace dome;
namespace {
void check(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
  std::cout << "PASS " << message << '\n';
}
template<class F> bool rejects(F f) { try { f(); return false; } catch (const std::exception &) { return true; } }
auto ordered(const Volume &v) {
  auto j = v.encode();
  std::sort(j["samples"].begin(),j["samples"].end());
  for (auto &layer : j["sculpt_layers"]) std::sort(layer["delta"].begin(),layer["delta"].end());
  if (j.contains("sculpt_mask")) std::sort(j["sculpt_mask"]["values"].begin(),j["sculpt_mask"]["values"].end());
  return j;
}
Volume seed() {
  Volume::Voxels values;
  for (int z=-6;z<=6;++z)for(int y=-6;y<=6;++y)for(int x=-6;x<=6;++x) {
    auto p=Vec3{x*.03f,y*.03f,z*.03f};
    values[{x,y,z}]=std::clamp(length(p)-.14f,-.12f,.12f);
  }
  return Volume::from_samples(.03f,std::move(values));
}
void stroke(Volume &volume, Brush b) { volume.begin_stroke();volume.stamp(b);volume.end_stroke(); }
void near(float a,float b,const char *message) { check(std::abs(a-b)<2e-6f,message); }
}
int main(int argc,char **argv) try {
  std::cout << "Backend " << compute_backend() << '\n';
  if(argc==2 && std::string_view(argv[1])=="--dispatch-fixture") {
    auto volume=seed();auto controls=volume.sculpt_settings();controls.mask_edit=true;controls.symmetry_axes=7;
    volume.sculpt_settings(controls);Brush brush;brush.from=brush.to={.09f,.03f,.03f};brush.radius=.08f;brush.blend=0;
    stroke(volume,brush);
    check(volume.mask_at({3,1,1})>.99999f && volume.mask_at({-3,-1,-1})>.99999f,
          "Actual selected-backend mask stamp updates all eight reflected supports");
#ifdef DOME_TEST_INSTRUMENTED
    check(dispatches==1,"Single eight-way mask stamp dispatched exactly once");
#endif
    std::cout<<"EXPECTED_KERNEL_LAUNCHES=1\n";return 0;
  }
  {
    Volume constant(.05f);constant.fill_mask(.75f);
    check(constant.mask_sample({.013f,.017f,.023f})==.75f&&constant.mask_sample({-.013f,.117f,-.223f})==.75f,
          "Uniform global protection samples retain exact scalar bits at fractional lattice positions");
    check(rejects([&]{constant.mask_sample({2000,0,0});}),
          "Uniform-mask sampling still rejects positions outside its finite workspace");
    auto encoded=constant.encode();encoded["sculpt_mask"]["default"]=0.f;
    for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x)
      encoded["sculpt_mask"]["values"].push_back({x,y,z,.75f});
    auto cell=Volume::decode(encoded);
    check(cell.mask_sample({.013f,.017f,.023f})==.75f,
          "A constant sparse neighbourhood preserves exact protection without relying on a uniform-map shortcut");
    for(auto &corner:encoded["sculpt_mask"]["values"])
      corner[3]=.1f+.1f*corner[0].get<int>()+.2f*corner[1].get<int>()+.3f*corner[2].get<int>();
    auto gradient=Volume::decode(encoded);
    near(gradient.mask_sample({.013f,.017f,.023f}),.332f,
         "Sparse protection interpolation reproduces an independently known affine field");
  }
  Brush b;b.radius=.2f;b.strength=.8f;b.pressure=.6f;b.blend=0;
  // Independent analytic endpoints: protection changes delta, not the radius.
  for (int tool=0;tool<4;++tool) {
    Volume::Voxels input;input[{0,0,0}]=.02f;
    for(Int3 q:{Int3{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}})input[q]=-.02f;
    auto v=Volume::from_samples(.03f,input);v.fill_mask(.25f);
    b.tool=Tool(tool);b.from=b.to=tool==3?Vec3{.05f,0,0}:Vec3{};b.normal={1,0,0};
    float radius=.2f*(.25f+.75f*std::sqrt(.6f));
    float target=tool==0?-radius:tool==1?radius:tool==2?-.02f:-.05f;
    float falloff=tool==3?std::pow(1-.05f/radius,2.f):1.f;
    float expected=.02f+(target-.02f)*falloff*.8f*.6f*.75f;
    stroke(v,b);near(v.at({0,0,0}),expected,"Every sculpt tool matches an independent partial-mask/radius-pressure oracle");
    v.fill_mask(1);auto before=ordered(v);auto history=v.undo_count();
    stroke(v,b);check(ordered(v)==before && v.undo_count()==history,"Fully protected tools are exact no-ops including history");
  }
  auto v=seed();auto original=ordered(v);auto settings=v.sculpt_settings();
  settings.mask_edit=true;settings.symmetry_axes=7;settings.mask_strength=.5f;v.sculpt_settings(settings);
  auto configured=ordered(v);auto shared=v.geometry_snapshot();
  b.from=b.to={.09f,.03f,.03f};b.radius=.08f;b.pressure=1;b.strength=1;b.tool=Tool::Clay;
#ifdef DOME_TEST_INSTRUMENTED
  auto calls=dispatches;
#endif
  stroke(v,b);
#ifdef DOME_TEST_INSTRUMENTED
  check(dispatches==calls+1,"All eight mirror/mask copies use one selected-backend dispatch");
#endif
  check(v.voxels()==shared.voxels() && v.mask_at({3,1,1})>0 && v.mask_at({-3,-1,-1})>0,
        "Mask painting reflects in three local planes without editing geometry");
  check(shared.mask_values().empty() && shared.mask_default()==0,"Published sparse mask snapshot remains immutable");
  auto masked=ordered(v);check(v.undo() && ordered(v)==configured && v.redo() && ordered(v)==masked,
      "Mask stroke undo/redo restores exact sparse defaults, exceptions and controls");
  v.begin_stroke();b.from=b.to={.12f,0,0};v.stamp(b);v.cancel_stroke();
  check(ordered(v)==masked,"Cancelling a mask stroke restores the complete authoring state");
  settings.mask_tool=Volume::MaskTool::Subtract;v.sculpt_settings(settings);
  b.from=b.to={.09f,.03f,.03f};auto protected_value=v.mask_at({3,1,1});stroke(v,b);
  check(v.mask_at({3,1,1})<protected_value,"Subtractive mask painting can unprotect already protected samples");
  settings.mask_tool=Volume::MaskTool::Smooth;v.sculpt_settings(settings);auto rough=v.mask_at({3,1,1});stroke(v,b);
  check(v.mask_at({3,1,1})!=rough,"Mask smoothing changes the protection field through the compute backend");
  auto uniform=seed();uniform.fill_mask(.37f);auto uniform_controls=uniform.sculpt_settings();
  uniform_controls.mask_edit=true;uniform_controls.mask_tool=Volume::MaskTool::Smooth;uniform.sculpt_settings(uniform_controls);
  auto uniform_before=ordered(uniform);auto uniform_history=uniform.undo_count();stroke(uniform,b);
  check(ordered(uniform)==uniform_before && uniform.undo_count()==uniform_history,
      "Smoothing a uniform mask default is bit-exact without sparse roundoff exceptions");
  auto saved=ordered(v);auto loaded=Volume::decode(v.encode());
  check(ordered(loaded)==saved && saved["version"]==3,"Volume3 saves/reopens exact masks and per-object controls");
  auto inverted=ordered(v);v.invert_mask();auto inversion=ordered(v);
  check(v.mask_default()==1 && v.mask_at({1000,0,0})==1,"Invert uses a global default and protects future geometry without enumerating empty space");
  check(v.undo() && ordered(v)==inverted && v.redo() && ordered(v)==inversion,"Invert is one exact independent history transaction");
  check(v.fill_mask(1) && v.mask_values().empty() && v.mask_default()==1,"Fill collapses mask exceptions into a constant global protection default");
  auto filled=ordered(v);check(!v.fill_mask(1) && ordered(v)==filled,"Repeated fill has exact no-op semantics");
  settings=v.sculpt_settings();settings.mask_edit=false;v.sculpt_settings(settings);
  auto layer=v.add_layer("Masked detail");v.layer_properties(layer,"Masked detail",true,true,.5f);
  check(v.mask_default()==1 && v.mask_values().empty(),"Layer creation/locking/strength changes retain the independent object mask");
  check(rejects([&]{v.begin_stroke();}),"Locked geometry layers still reject geometry editing");
  settings.mask_edit=true;settings.mask_tool=Volume::MaskTool::Subtract;v.sculpt_settings(settings);stroke(v,b);
  check(v.mask_at({3,1,1})<1,"Independent masks remain editable when the geometry layer is locked");
  auto retained_mask=v.mask_values();v.erase_layer(layer);
  check(v.mask_values()==retained_mask,"Deleting a displacement layer does not discard protection");
  v.fill_mask(0);check(v.mask_values().empty() && v.mask_default()==0,"Clear returns the entire mask to zero without enumerating support");
  // Geometry pressure at an exactly centered brush is identical with 0/1/3 axes.
  b.from=b.to={};b.normal={0,1,0};b.tool=Tool::Carve;b.radius=.09f;b.pressure=.37f;
  auto plain=seed();auto unedited=plain.voxels();stroke(plain,b);check(plain.voxels()!=unedited,"The centered seam fixture performs a real pressure edit");
  for(unsigned bits:{1u,3u,7u}) {
    auto mirrored=seed();auto controls=mirrored.sculpt_settings();controls.symmetry_axes=bits;mirrored.sculpt_settings(controls);
    stroke(mirrored,b);check(mirrored.voxels()==plain.voxels(),"Center-plane symmetry does not double brush pressure or seam strength");
  }
  // Compare disjoint mirrored flatten/smooth against independently transformed
  // brush endpoints AND normals on the same source field.
  for(auto tool:{Tool::Flatten,Tool::Smooth}) {
    auto symmetric=seed(),one=seed(),other=seed();auto controls=symmetric.sculpt_settings();controls.symmetry_axes=1;symmetric.sculpt_settings(controls);
    b.tool=tool;b.from={.09f,.03f,.03f};b.to={.12f,.03f,.03f};b.radius=.045f;b.pressure=.7f;b.normal=normalized({1,2,3});
    auto reflection=b;reflection.from.x=-b.from.x;reflection.to.x=-b.to.x;reflection.normal.x=-b.normal.x;
    stroke(one,b);stroke(other,reflection);stroke(symmetric,b);
    bool correct=true;
    for(int z=-6;z<=6;++z)for(int y=-6;y<=6;++y)for(int x=-6;x<=6;++x)
      if(std::abs(symmetric.at({x,y,z})-(x>=0?one.at({x,y,z}):other.at({x,y,z})))>2e-6f)correct=false;
    check(correct,"Mirrored flatten/smooth paths and normals match independent single-brush fields");
  }
  auto separated=seed();settings=separated.sculpt_settings();settings.symmetry_axes=1;separated.sculpt_settings(settings);
  b.from=b.to={50,0,0};b.radius=.03f;b.tool=Tool::Clay;stroke(separated,b);
  check(separated.at({1667,0,0})<separated.band() && separated.at({-1667,0,0})<separated.band(),
        "Far mirrored strokes enumerate local supports rather than the huge empty gap");
  auto budget=seed();settings=budget.sculpt_settings();settings.symmetry_axes=7;settings.mask_edit=true;budget.sculpt_settings(settings);
  auto untouched=ordered(budget);budget.begin_stroke();b.from=b.to={.09f,.03f,.03f};b.radius=.08f;
  check(rejects([&]{budget.stamp(b,budget.stored_samples());}) && !budget.pending_changes(),"Shared displacement/mask resource limits reject before publication");
  budget.cancel_stroke();check(ordered(budget)==untouched,"Rejected mask stamps preserve all persisted data");
  budget.begin_stroke();int polls=0;
  check(rejects([&]{budget.stamp(b,4'000'000,[&]{return ++polls==4;});}) && !budget.pending_changes(),"Cooperative cancellation rejects detached mask work before publication");
  budget.cancel_stroke();check(ordered(budget)==untouched,"Cancelled authoring retains exact global defaults, layers and settings");
#ifdef DOME_TEST_INSTRUMENTED
  backend_completed=false;budget.begin_stroke();int post_compute=0;
  check(rejects([&]{budget.stamp(b,4'000'000,[&]{return backend_completed && ++post_compute==3;});}) && !budget.pending_changes(),
      "Cancellation during staged post-compute mask publication is transactional");
  budget.cancel_stroke();check(ordered(budget)==untouched,"Post-compute cancellation preserves exact authoring state");
  backend_failure=true;budget.begin_stroke();
  check(rejects([&]{budget.stamp(b);}) && !budget.pending_changes(),"A failed selected backend cannot publish partial mask changes or silently fall back");
  backend_failure=false;budget.cancel_stroke();
  // Exercise allocation failures in input support, detached result and staged
  // field/history maps. Surviving attempts are cancelled before comparison.
  for(int64_t fault:{0,1,2,4,8,16,32,64,128,256,512,1024}) {
    auto candidate=seed();auto before=ordered(candidate);candidate.begin_stroke();
    allocations_until_failure=fault;
    try {candidate.stamp(b);}catch(const std::bad_alloc &){}
    allocations_until_failure=-1;candidate.cancel_stroke();
    check(ordered(candidate)==before,"Allocation interruption leaves exact recoverable stroke state");
  }
  for(int64_t fault:{0,1,4,16,64,256,1024,2048,4096,8192,16384}) {
    auto candidate=seed();auto before=ordered(candidate);candidate.begin_stroke();
    fail_after_compute=fault;bool failed=false;
    try {candidate.stamp(b);}catch(const std::bad_alloc &){failed=true;}
    allocations_until_failure=-1;fail_after_compute=-1;
    if(failed)check(!candidate.pending_changes(),"Post-compute allocation failure cannot partially publish field/history changes");
    candidate.cancel_stroke();check(ordered(candidate)==before,"Post-compute allocation failure retains exact recoverable geometry");
  }
  for(bool mask_edit:{false,true}) {
    auto candidate=seed();auto controls=candidate.sculpt_settings();controls.mask_edit=mask_edit;candidate.sculpt_settings(controls);
    if(mask_edit)candidate.fill_mask(.37f);
    auto before=ordered(candidate);candidate.begin_stroke();
    auto carve=b;carve.tool=Tool::Carve;carve.from=carve.to={};carve.radius=.2f;carve.pressure=carve.strength=1;
    candidate.stamp(carve);check(candidate.pending_changes(),"Allocation-free cancellation fixture performs a real edit");
    allocations_until_failure=0;candidate.cancel_stroke();allocations_until_failure=-1;
    check(ordered(candidate)==before,"Geometry/mask cancellation restores exact COW state even when all new allocations fail");
  }
#endif
  auto invalid=saved;invalid["sculpt_mask"]["default"]=2;
  check(rejects([&]{Volume::decode(invalid);}),"Malformed protection values are refused");
  invalid=saved;invalid["sculpt_mask"]["values"].push_back(invalid["sculpt_mask"]["values"][0]);
  check(rejects([&]{Volume::decode(invalid);}),"Duplicate persisted mask coordinates are refused");
  invalid=saved;invalid["sculpt_controls"]["symmetry_axes"]=8;
  check(rejects([&]{Volume::decode(invalid);}),"Invalid symmetry controls are refused");
  invalid=saved;invalid["version"]=2;
  check(rejects([&]{Volume::decode(invalid);}),"Older volume formats cannot silently discard new mask authoring");
  auto legacy=Volume::decode(original);check(legacy.mask_default()==0 && legacy.mask_values().empty() && legacy.sculpt_settings().symmetry_axes==0,
      "Volume2 remains fully unmasked with symmetry disabled");
  Scene scene;scene.active().volume=std::make_shared<Volume>(Volume::decode(saved));auto first=ordered(*scene.active().volume);
  auto copy=scene.duplicate(scene.active().id);scene.object(copy).volume->fill_mask(1);
  check(ordered(*scene.object(1).volume)==first && scene.object(copy).volume->mask_default()==1,"Duplicated objects retain independent COW mask state");
  auto roundtrip=Scene::decode(scene.encode());check(ordered(*roundtrip.object(1).volume)==first && roundtrip.object(copy).volume->mask_default()==1,
      "Native multi-object scenes preserve independent durable mask/control authoring");
  return 0;
} catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
