// SPDX-License-Identifier: GPL-3.0-or-later
#include "bake-fixture-identity.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/document.hpp"
#include "dome/source-placement.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <condition_variable>
#include <mutex>
// This gate observes the actual detached atlas computation after real unwrap,
// before native publication. It does not replace or claim an internal poll.
namespace atlas_gate {
std::mutex mutex;std::condition_variable changed;
bool armed=false,entered=false,released=false;
void arm(){std::lock_guard lock(mutex);armed=true;entered=released=false;}
void release(){std::lock_guard lock(mutex);released=true;armed=false;changed.notify_all();}
bool wait(){std::unique_lock lock(mutex);return changed.wait_for(lock,std::chrono::seconds(120),[]{return entered;});}
struct Release {~Release(){release();}};
}
extern "C" void real_unwrap(dome::Mesh &,unsigned,unsigned,const std::function<bool()> &)
  asm("__real__ZN4dome6unwrapERNS_4MeshEjjRKSt8functionIFbvEE");
extern "C" void wrapped_unwrap(dome::Mesh &,unsigned,unsigned,const std::function<bool()> &)
  asm("__wrap__ZN4dome6unwrapERNS_4MeshEjjRKSt8functionIFbvEE");
extern "C" void wrapped_unwrap(dome::Mesh &mesh,unsigned size,unsigned padding,const std::function<bool()> &cancelled){
  real_unwrap(mesh,size,padding,cancelled);
  std::unique_lock lock(atlas_gate::mutex);
  if(!atlas_gate::armed)return;
  atlas_gate::entered=true;atlas_gate::changed.notify_all();
  if(!atlas_gate::changed.wait_for(lock,std::chrono::seconds(120),[]{return atlas_gate::released;}))
    throw std::runtime_error("Real retopo atlas publication gate timed out");
}
using namespace dome;
namespace {
size_t checks = 0;
void check(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
  ++checks;
}
template<class F> bool refuses(F &&f) {
  try { f(); return false; } catch (const std::exception &) { return true; }
}
using Matrix = std::array<double,16>;
Vec3 point(const Matrix &m, Vec3 p) {
  return {float(m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12]),
          float(m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13]),
          float(m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14])};
}
double determinant(const Matrix &m) {
  return m[0]*(m[5]*m[10]-m[9]*m[6])-m[4]*(m[1]*m[10]-m[9]*m[2])+m[8]*(m[1]*m[6]-m[5]*m[2]);
}
Vec3 evaluated(Vec3 p, bool posed) {
  if (!posed) return p;
  return {p.x*1.1f+.01f,p.y*.85f-.02f,
          (p.z+.35f*.04f*(p.x+.4f))*1.2f+.06f};
}
Scene source(const Matrix &m, bool posed) {
  Mesh mesh;mesh.positions={{-.4f,-.3f,-.11f},{.4f,-.3f,.05f},{.4f,.3f,.11f},{-.4f,.3f,-.05f}};
  mesh.normals.assign(4,normalized(Vec3{-.2f,-.1f,1}));
  mesh.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};mesh.indices={0,1,2,0,2,3};
  Scene s;auto &o=s.active();o.name="Retopo source — původní";o.mesh_active=true;
  o.material.color={.42f,.05f,.05f,1};
  o.painted=std::make_shared<PaintedMesh>(mesh,64);
  o.painted->begin_stroke();Brush b;b.radius=.18f;b.from=b.to={0,0,0};b.normal=mesh.normals.front();
  PaintSettings paint;paint.color={.12f,.6f,.3f,.8f};o.painted->stamp(b,paint);
  check(o.painted->end_stroke(),"Fixture has genuine sparse paint");
  o.painted->begin_stroke();paint.channel=PaintChannel::Emission;
  paint.emission_units=PaintEmissionUnits::Physical;paint.emission={4,.125f,.05f};
  o.painted->stamp(b,paint);
  check(o.painted->end_stroke(),"Fixture has a physical emission stroke");
  DeformationState d(mesh);d.skeleton=Skeleton{{{"root",-1,{},.2f}}};
  d.skin=SkinWeights{};d.skin->vertices.assign(4,{{0,1}});
  d.morphs=MorphSet(mesh);auto target=mesh.positions;
  for(auto &p:target)p.z+=.04f*(p.x+.4f);
  d.morphs->set_target(mesh,"crease",target);d.morph_weights={posed?.35f:0};
  d.expressions=AvatarExpressions{{{"aa",{{"crease",.6f}}}}};
  if(posed){d.pose.resize(1);d.pose[0].translation={.01f,-.02f,.06f};d.pose[0].scale={1.1f,.85f,1.2f};}
  s.set_deformation(o.id,std::move(d));o.affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(m));
  s.validate();return s;
}
void ready(Document &d) {
  check(d.wait_retopology(),"Retopology worker reaches endpoint");
  if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);
}
void place(Document &d, Vec3 p) {
  check(d.begin_desktop(),"Actual desktop gesture owns retopo placement");
  Brush b;b.radius=.01f;b.from=b.to=p;d.stamp_desktop(b);d.end_desktop();ready(d);
}
void bake_patch(Document &doc, uint64_t source, const std::filesystem::path &path) {
  const auto original=doc.copy_scene();
  const auto before=original.encode();
  const auto sources=doc.frame().state->bake_sources;
  check(std::find(sources.begin(),sources.end(),source)!=sources.end(),
        "The Bake chooser includes the original hidden affine mesh");
  ObjectBakeSettings settings;settings.front_mm=2;settings.back_mm=2;
  settings.ao_samples=0;settings.padding=0;
  doc.bake_from_object(source,settings);
  doc.save_async(path).get();
  check(doc.wait_asset(),"Bake and ordered Save finish");
  if(!doc.frame().state->error.empty())throw std::runtime_error(doc.frame().state->error);
  const auto accepted=doc.copy_scene();
  const auto baked_scene=accepted.encode();
  const auto reloaded=Scene::load(path);
  const auto baked=reloaded.active().painted->baked();
  check(baked&&baked->emission_field&&baked->maps.emission.rgba.empty(),
        "The patch keeps physical emission as FLOAT beside its baked color and normals");
  size_t green=0,red=0,hdr=0;
  for(size_t texel=0;texel<baked->coverage.size();++texel){
    if(baked->coverage[texel]!=255)continue;
    const auto *rgba=baked->maps.color.rgba.data()+texel*4;
    green+=rgba[1]>rgba[0]+16;
    red+=rgba[0]>rgba[1]+16;
    hdr+=baked->emission_field->values()[texel].absolute[0]>2;
  }
  const auto pixels=baked->coverage.size();
  check(green>pixels/100&&red>pixels/100&&hdr>pixels/100,
        "World-metric projection reaches both sparse painted detail and the unpainted base");
  check(bake_fixture::isolated_scene(accepted,source).encode()==
        bake_fixture::isolated_scene(original,source).encode()&&reloaded.encode()==baked_scene,
        "Bake and reopen preserve the hidden donor, source pose and all paint layers exactly");
  check(doc.undo()&&doc.copy_scene().encode()==before&&doc.redo()&&doc.copy_scene().encode()==baked_scene,
        "Undo and Redo affect only the accepted patch Bake");
}
void first_native_material(const std::filesystem::path &out,uint32_t resolution=64){
  const auto plane=[](float height) {
    Mesh mesh;
    mesh.positions={{-1,-1,height},{1,-1,height},{1,1,height},{-1,1,height}};
    mesh.normals.assign(4,{0,0,1});
    mesh.indices={0,1,2,0,2,3};
    mesh.uv={{0,0},{1,0},{1,1},{0,1}};
    return mesh;
  };
  const auto quad=[](float left,float right) {
    RetopoMesh patch;
    const std::array<Vec3,4> positions{{{left,-.6f,0},{right,-.6f,0},{right,.6f,0},{left,.6f,0}}};
    std::array<TopologyID,4> vertices;
    for(size_t i=0;i<positions.size();++i)vertices[i]=patch.add_vertex({positions[i]});
    patch.add_face(vertices);
    return patch;
  };
  Scene input;
  const auto guide=input.active().id;
  input.active().name="Native painted guide";
  input.active().mesh_active=true;
  input.active().painted=std::make_shared<PaintedMesh>(plane(0),resolution);
  auto &paint=*input.active().painted;
  Brush brush;brush.radius=.49f;brush.normal={0,0,1};
  PaintSettings ink;ink.color={.05f,.8f,.1f,1};
  paint.layer_properties("Ink",true,.75f);
  paint.begin_stroke();paint.stamp(brush,ink);
  ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={3,.1f,.2f};
  paint.stamp(brush,ink);
  check(paint.end_stroke(),"Native guide contains both editable color and FLOAT layer emission");
  paint.add_layer("保護");
  ink.channel=PaintChannel::Mask;ink.mask=0;
  paint.begin_stroke();paint.stamp(brush,ink);
  check(paint.end_stroke(),"Native guide contains a real editable mask");
  paint.layer_properties("保護",false,0);
  const auto mask=paint.active_layer(), color=paint.layers().front().id;
  const auto donor=input.add("Red HDR Bake donor",Volume{});
  auto &source=input.object(donor);
  source.mesh_active=true;source.visible=false;
  source.painted=std::make_shared<PaintedMesh>(plane(.012f),64);
  source.material.color={1,0,0,1};
  source.material.emissive={4,.125f,.5f};
  input.select(guide);
  ObjectBakeSettings settings;settings.front_mm=20;settings.back_mm=0;settings.ao_samples=0;settings.padding=0;
  bake_object(input,donor,settings);
  const auto original=input.encode();
  input.save(out/"first-native-input.dsculpt");
  auto accepted=input.snapshot();
  const auto original_guide=accepted.object(guide).painted;
  publish_retopology(accepted,quad(-.6f,.6f),guide,{});
  const auto &result=*accepted.active().painted;
  check(result.resolution()==resolution&&result.active_layer()==mask&&result.layers().size()==2&&
      result.layers()[0].id==color&&result.layers()[0].name=="Ink"&&result.layers()[0].opacity==.75f&&
      result.layers()[1].id==mask&&result.layers()[1].name=="保護"&&!result.layers()[1].visible&&result.layers()[1].opacity==0,
      "The first patch keeps authored paint resolution, layer order, UTF-8 names, IDs, visibility, opacity and active layer");
  check(result.baked()&&result.baked()->emission_field&&accepted.active().bake_recipe==input.active().bake_recipe,
      "The first native patch has its own Bake maps, FLOAT field and exact saved recipe");
  const auto &baked=*result.baked();
  size_t tinted=0,protected_pixels=0,emissive_layer=0,emissive_bake=0;
  for(size_t i=0;i<baked.coverage.size();++i){
    if(baked.coverage[i]<250)continue;
    const auto x=uint32_t(i%resolution),y=uint32_t(i/resolution);
    const auto &pixel=result.pixel(color,x,y);
    tinted+=pixel.data[9]>180&&pixel.data[1]>pixel.data[0]+30;
    protected_pixels+=result.pixel(mask,x,y).data[13]<128;
    emissive_layer+=result.emission_pixel(color,x,y).physical[0]>1;
    emissive_bake+=baked.emission_field->texel(i).resolve({},1)[0]==4&&
        baked.maps.color.rgba[i*4]==255&&baked.maps.normal.rgba[i*4+2]>=254;
  }
  const auto pixels=size_t(resolution)*resolution;
  check(tinted>pixels/100&&protected_pixels>pixels/100&&emissive_layer>pixels/100&&emissive_bake>pixels/4,
      "Projected texels carry editable color and mask detail plus separate FLOAT layer and Bake radiance");
  check(accepted.object(guide).painted.get()==original_guide.get()&&!accepted.object(guide).visible&&input.encode()==original,
      "Apply keeps the hidden original paint owner and leaves the source project exact");
  const auto file=out/"first-native-material.dsculpt";
  accepted.save(file);
  check(Scene::load(file).encode()==accepted.encode(),"Native Save and cold Open preserve the complete projected paint and recipe");
  bool missed=false;
  try{publish_retopology(input,quad(.64f,1.5f),guide,{});}
  catch(const std::invalid_argument &e){missed=std::string_view(e.what()).find("no matching source")!=std::string_view::npos;}
  check(missed&&input.encode()==original,"First Apply rejects partially unmatched painted geometry without publishing an incomplete patch");
  auto cleared=input.snapshot();
  cleared.active().painted->clear_bake();
  publish_retopology(cleared,quad(-.6f,.6f),guide,{});
  check(!cleared.active().painted->baked()&&cleared.active().bake_recipe&&cleared.active().painted->has_physical_emission(),
      "A cleared Bake stays cleared while its recipe and editable FLOAT paint survive");
  Scene empty;
  empty.active().mesh_active=true;
  empty.active().painted=std::make_shared<PaintedMesh>(plane(0),64);
  auto &empty_paint=*empty.active().painted;
  empty_paint.add_layer("Deleted");
  empty_paint.delete_layer();
  publish_retopology(empty,quad(-.6f,.6f),empty.active().id,{});
  auto cursor=empty.active().painted->snapshot();
  cursor->add_layer("New layer");
  check(empty.active().painted->resolution()==64&&cursor->active_layer()==3,
      "An otherwise empty native layer stack preserves its monotonic ID cursor");
}
void topology_transform() {
  RetopoMesh t;std::array<TopologyID,4> v;
  const Vec3 p[4]{{-.2f,-.2f,0},{.2f,-.2f,0},{.2f,.2f,0},{-.2f,.2f,0}};
  for(size_t i=0;i<4;++i)v[i]=t.add_vertex({p[i],{float(i&1),float(i>>1)},i==0});
  t.add_face(v,7);t.set_seam(t.edges().begin()->first,true);
  const auto before=t.encode();const auto next=t.next_id();
  Matrix m{-1.3,.1,0,0,.35,.8,.05,0,.1,.15,1.2,0,.3,-.2,.1,1};
  auto transformed=t;transformed.transform(AffineTransform(m));
  check(transformed.next_id()==next&&transformed.edges().size()==t.edges().size(),"Affine conversion preserves stable IDs/allocation cursor");
  for(auto &[id,a]:t.vertices()){
    const auto &b=transformed.vertices().at(id);
    check(length(b.position-point(m,a.position))<1e-6f&&a.uv==b.uv&&a.pinned==b.pinned,"Actual full affine vertex conversion preserves UVs and pins");
  }
  for(auto &[id,a]:t.edges()){const auto &b=transformed.edges().at(id);check(a.a==b.a&&a.b==b.b&&a.seam==b.seam,"Edge identities and seams survive reflection");}
  const auto &a=t.faces().begin()->second;const auto &b=transformed.faces().begin()->second;
  check(a.material==b.material&&b.vertices==std::vector<TopologyID>{v[0],v[3],v[2],v[1]},"Reflection reverses one real face loop without reallocating its identity");
  size_t polls=0;auto count=t;count.transform(AffineTransform(m),[&]{++polls;return false;});
  for(size_t fail=1;fail<=polls;++fail){auto copy=t;size_t seen=0;check(refuses([&]{copy.transform(AffineTransform(m),[&]{return ++seen==fail;});})&&copy.encode()==before,"Every transform cancellation leaves complete caller topology exact");}
  auto huge=m;huge[12]=200;check(refuses([&]{t.transform(AffineTransform(huge));})&&t.encode()==before,"Existing100m topology bound refuses atomically");
}
}
int main(int argc,char **argv)try {
  if(argc==3&&std::string_view(argv[1])=="--first-native-1024"){
    const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);
    first_native_material(out,1024);
    std::cout<<"PASS "<<checks<<" native 1024-pixel first-patch material checks\n";return 0;
  }
  if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
  topology_transform();
  first_native_material(out);
  const Matrix matrices[]{
    {0,1,0,0,-1,0,0,0,0,0,1,0,.15,-.1,.2,1},
    {1.4,0,0,0,0,.7,0,0,0,0,1.2,0,.15,-.1,.2,1},
    {1.3,.1,0,0,.35,.8,.05,0,.1,.15,1.2,0,.15,-.1,.2,1},
    {-1.3,.1,0,0,.35,.8,.05,0,.1,.15,1.2,0,.15,-.1,.2,1}};
  for(size_t mode=0;mode<4;++mode)for(bool posed:{false,true}){
    std::cout<<"CASE affine="<<mode<<" posed="<<posed<<std::endl;
    const auto &m=matrices[mode];auto s=source(m,posed);const auto original=s.encode();
    const auto source_id=s.active().id;const auto old_paint=s.active().painted->encode();
    const auto old_deform=s.active().deformation->encode(s.active().painted->mesh());
    const Vec3 rest[4]{{-.2f,-.15f,-.055f},{.2f,-.15f,.025f},{.2f,.15f,.055f},{-.2f,.15f,-.025f}};
    std::array<Vec3,4> world;for(size_t i=0;i<4;++i)world[i]=point(m,evaluated(rest[i],posed));
    const auto center=(world[0]+world[1]+world[2]+world[3])*.25f;
    const auto normal=normalized(cross(world[1]-world[0],world[3]-world[0]))*(determinant(m)<0?-1.f:1.f);
    RetopoWorkspace w(s);check(w.view().transform.scale==1&&w.view().transform.position.x==0,"Affine workspace uses genuine world metres");
    const auto hit=w.view().source->closest(center+normal*.02f);
    check(length(hit.position-center)<2e-6f&&dot(hit.normal,normal)>1-2e-5f,"Independent affine/skin/morph plane oracle verifies source positions and normals");
    check(w.view().source->raycast(center+normal*.1f,normal*-1,.11f).has_value()&&!w.view().source->raycast(center+normal*.1f,normal*-1,.09f),"World ray bounds remain physical under nonuniform/shear/reflection");
    w.edit(RetopoAction::BeginGesture);check(refuses([&]{w.edit(RetopoAction::Sample,center+normal*.051f,50);})&&w.view().topology->vertices().empty(),"Exact50mm projection bound rejects51mm world miss");w.edit(RetopoAction::CancelGesture);
    check(s.encode()==original,"Workspace never mutates original paint/pose/meta/placement");
    Document d;d.replace(s.snapshot());check(d.wait_mesh(),"Actual native source mesh publishes");
    check(d.frame().state->can_retopologize,"Shared Retopo entry enabled for the genuine affine source");
    d.begin_retopology();ready(d);
    const auto ray=d.raycast(center+normal*.1f,normal*-1);
    check(ray&&length(*ray-center)<2e-6f&&dot(d.gradient(center),normal)>1-2e-5f,"Actual native ray and brush normal use immutable retopo source frame");
    place(d,world[0]+normal*.02f);d.end_retopology(false);
    check(d.copy_scene().encode()==original,"Cancel after genuine affine placement preserves exact Scene/history");
    d.begin_retopology();ready(d);for(auto p:world)place(d,p+normal*.02f);
    check(d.undo(),"Native Undo admits one draft point");ready(d);check(d.frame().state->retopology->topology->vertices().size()==3,"Draft Undo removes exactly one vertex");
    check(d.redo(),"Native Redo restores draft point");ready(d);
    d.edit_retopology(RetopoAction::Face);ready(d);const auto topology=d.frame().state->retopology->topology->encode();
    atlas_gate::Release release;atlas_gate::arm();d.end_retopology(true);
    check(atlas_gate::wait()&&d.frame().state->asset_busy,"Actual detached atlas finishes before the accepted asset worker publishes");
    const auto path=out/(std::to_string(mode)+(posed?"-posed.dsculpt":"-rest.dsculpt"));auto saved=d.save_async(path);
    atlas_gate::release();saved.get();
    check(d.wait_asset()&&!d.frame().state->retopology&&d.frame().state->error.empty(),"Apply and ordered Save finish actual worker publication");
    const auto accepted=d.copy_scene();const auto exact=accepted.encode();
    check(accepted.active().painted->resolution()==64&&accepted.active().painted->has_physical_emission(),
          "The first patch carries editable FLOAT paint from the evaluated source");
    check(!accepted.object(source_id).visible&&accepted.object(source_id).painted->encode()==old_paint&&accepted.object(source_id).deformation->encode(accepted.object(source_id).painted->mesh())==old_deform&&accepted.object(source_id).affine_edit==s.active().affine_edit,"Publication retains exact hidden original paint/skin/morph/viseme/placement owner");
    check(accepted.active().retopology->topology.encode()==topology&&Scene::load(path).encode()==exact,"Save/reopen retains editable stable topology and whole exact Scene");
    for(auto &[id,p]:accepted.active().retopology->topology.vertices()){(void)id;check(w.view().source->closest(p.position).distance<2e-6f,"Published topology lies on independently checked evaluated source");}
    check(d.undo()&&d.copy_scene().encode()==original,"One global Undo removes the entire accepted patch");check(d.redo()&&d.copy_scene().encode()==exact,"Global Redo restores complete accepted source and patch");
    d.begin_retopology();ready(d);check(d.frame().state->retopology->topology->encode()==topology,"Reopen uses retained source rather than atlas triangles");d.end_retopology(false);check(d.copy_scene().encode()==exact,"Reopened Cancel is exact");
    if(mode==3&&posed)bake_patch(d,source_id,out/"reflected-posed-baked.dsculpt");
    d.begin_retopology();ready(d);place(d,world[0]);d.replace(s.snapshot());check(d.wait_retopology()&&!d.frame().state->retopology&&d.copy_scene().encode()==original,"Document replacement rejects any stale affine workspace publication");
  }
  std::cout<<"PASS "<<checks<<" affine manual-retopo checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
