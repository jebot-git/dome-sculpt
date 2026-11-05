// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-placement.hpp"
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
  o.painted=std::make_shared<PaintedMesh>(mesh,64);
  o.painted->begin_stroke();Brush b;b.radius=.18f;b.from=b.to={0,0,0};b.normal=mesh.normals.front();
  PaintSettings paint;paint.color={.12f,.6f,.3f,.8f};o.painted->stamp(b,paint);
  check(o.painted->end_stroke(),"Fixture has genuine sparse paint");
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
  if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
  topology_transform();
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
    check(!accepted.object(source_id).visible&&accepted.object(source_id).painted->encode()==old_paint&&accepted.object(source_id).deformation->encode(accepted.object(source_id).painted->mesh())==old_deform&&accepted.object(source_id).affine_edit==s.active().affine_edit,"Publication retains exact hidden original paint/skin/morph/viseme/placement owner");
    check(accepted.active().retopology->topology.encode()==topology&&Scene::load(path).encode()==exact,"Save/reopen retains editable stable topology and whole exact Scene");
    for(auto &[id,p]:accepted.active().retopology->topology.vertices()){(void)id;check(w.view().source->closest(p.position).distance<2e-6f,"Published topology lies on independently checked evaluated source");}
    check(d.undo()&&d.copy_scene().encode()==original,"One global Undo removes the entire accepted patch");check(d.redo()&&d.copy_scene().encode()==exact,"Global Redo restores complete accepted source and patch");
    d.begin_retopology();ready(d);check(d.frame().state->retopology->topology->encode()==topology,"Reopen uses retained source rather than atlas triangles");d.end_retopology(false);check(d.copy_scene().encode()==exact,"Reopened Cancel is exact");
    d.begin_retopology();ready(d);place(d,world[0]);d.replace(s.snapshot());check(d.wait_retopology()&&!d.frame().state->retopology&&d.copy_scene().encode()==original,"Document replacement rejects any stale affine workspace publication");
  }
  std::cout<<"PASS "<<checks<<" affine manual-retopo checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
