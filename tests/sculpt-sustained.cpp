// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/render.hpp"
#include "dome/material-visibility.hpp"
#include "dome/source-placement.hpp"
#include "sculpt-sustained-metrics.hpp"
#include "graphics.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sys/resource.h>
#include <thread>

using nlohmann::json;
namespace {
using namespace sustained;
struct Refusal:std::runtime_error{using std::runtime_error::runtime_error;};
void require(bool yes,const std::string &why){if(!yes)throw std::runtime_error(why);}
void write(const std::filesystem::path &p,const json &j){std::ofstream f(p);f<<j.dump(2)<<'\n';require(bool(f),"Cannot write benchmark evidence");}
struct Options {
  std::filesystem::path output;std::string tier="production";
  size_t objects=4,strokes=300,masks=120,pairs=2000,repetitions=3;float voxel=.0125f;
  int width=768,height=768;bool smoke=false,count_allocations=false;
};
Options options(int argc,char **argv){
  Options o;bool objects=false;
  for(int i=1;i<argc;++i){std::string k=argv[i];auto value=[&](){require(i+1<argc,"Missing value for "+k);return std::string(argv[++i]);};
    if(k=="--output")o.output=value();else if(k=="--tier")o.tier=value();else if(k=="--objects"){o.objects=std::stoul(value());objects=true;}
    else if(k=="--allocations")o.count_allocations=true;else if(k=="--smoke")o.smoke=true;else throw std::invalid_argument("Unknown benchmark argument: "+k);
  }
  require(!o.output.empty(),"Usage: sculpt-sustained --output directory [--tier small|medium|production|dense] [--objects 1|4|16] [--smoke] [--allocations]");
  require(o.objects==1||o.objects==4||o.objects==16,"Object count must be 1, 4 or 16");
  if(o.tier=="small")o.voxel=.025f;else if(o.tier=="medium"||o.tier=="production")o.voxel=.0125f;else if(o.tier=="dense")o.voxel=.00625f;else throw std::invalid_argument("Unknown tier");
  if(o.smoke){require(!objects||o.objects==4,"Smoke uses four explicit objects");o.objects=4;o.voxel=.04f;o.strokes=4;o.masks=3;o.pairs=8;o.repetitions=1;o.width=o.height=128;}
  return o;
}
void ready(Document &d){require(d.wait_mesh(std::chrono::seconds(120)),"Mesh worker publication timed out");auto f=d.frame();require(f.state->error.empty(),"Document error: "+f.state->error);require(f.surface->generation==f.state->generation&&f.surface->revision==f.state->revision&&f.surface->view_epoch==f.state->view_epoch,"Worker did not publish the accepted current source");}
Volume sphere(float voxel){
  // Genuine complete clipped SDF, including the negative interior. Omitting the
  // interior creates a different shell, so density/resource refusal stays real.
  constexpr float radius=.6f;Volume::Voxels values;const float band=voxel*4;
  const int n=int(std::ceil((radius+band)/voxel));
  for(int z=-n;z<=n;++z)for(int y=-n;y<=n;++y)for(int x=-n;x<=n;++x){
    const auto distance=length(Vec3{x*voxel,y*voxel,z*voxel})-radius;
    if(distance<band){if(values.size()>=4'000'000)throw Refusal("Declared sphere exceeds the existing 4,000,000 field sample limit");values.emplace(Int3{x,y,z},std::max(-band,distance));}
  }
  return Volume::from_samples(voxel,std::move(values));
}
Scene fixture(const Options &o){
  Scene s;auto v=sphere(o.voxel);s.active().volume=std::make_shared<Volume>(v);s.active().material.double_sided=true;
  for(size_t i=1;i<o.objects;++i){CanvasTransform pose;pose.position={float(i%4)*1.3f,float(i/4)*1.3f,0};
    const auto id=s.add("Measured sphere "+std::to_string(i),v,pose);s.object(id).material.double_sided=true;
    if(i%3==1){s.object(id).material.alpha=Material::AlphaMode::Blend;s.object(id).material.color[3]=.6f;}
    if(i==o.objects-1)s.object(id).affine_edit=std::make_shared<NativeAffineEdit>(AffineTransform(std::array<double,16>{-1,0,0,0,.17,1.1,0,0,.07,.09,.85,0,0,0,0,1}));
  }
  s.select(1);return s;
}
Scene grouped_regression(){
  // Original authenticated fixture, unchanged source, placement and guards.
  Volume v(.04f);Brush b;b.from=b.to={};b.radius=.36f;b.tool=Tool::Clay;b.blend=0;v.begin_stroke();v.stamp(b);v.end_stroke();
  Scene s;s.active().volume=std::make_shared<Volume>(v);CanvasTransform group;group.position={.12f,-.04f,.03f};group.scale=1.25f;
  const auto id=s.add_group("Original grouped regression",group);s.active().group=id;CanvasTransform other;other.position={1,0,0};const auto second=s.add("Original second sphere",v,other);s.object(second).group=id;s.select(1);return s;
}
json counters(const Counters &a,const Counters &b){
  static constexpr const char *names[]={"capture_inclusive","build_inclusive","update_inclusive","placement_inclusive","scalars","affine_nearest","affine_ray"};
  json stages=json::object();for(size_t i=0;i<StageCount;++i)stages[names[i]]={{"calls",b.stages[i].calls-a.stages[i].calls},{"wall_ns",b.stages[i].wall_ns-a.stages[i].wall_ns},{"thread_cpu_ns",b.stages[i].cpu_ns-a.stages[i].cpu_ns}};
  return {{"cpp_new_calls",b.news-a.news},{"cpp_new_requested_bytes",b.new_bytes-a.new_bytes},{"buffer_calls",b.buffer_calls-a.buffer_calls},{"buffer_allocation_bytes",b.buffer_allocation_bytes-a.buffer_allocation_bytes},{"buffer_payload_bytes",b.buffer_payload_bytes-a.buffer_payload_bytes},{"dynamic_array_payload_bytes",b.scalar_payload_bytes-a.scalar_payload_bytes},{"dynamic_array_calls",b.scalar_calls-a.scalar_calls},{"stages",stages}};
}
template<class F>json measure(std::string kind,size_t index,F fn){
  const auto before=Counters::read();const auto wall=clock_ns(CLOCK_MONOTONIC),cpu=clock_ns(CLOCK_THREAD_CPUTIME_ID);
  measuring.store(true);try{fn();}catch(...){measuring.store(false);throw;}measuring.store(false);
  auto row=counters(before,Counters::read());row["kind"]=kind;row["index"]=index;row["wall_ns"]=clock_ns(CLOCK_MONOTONIC)-wall;row["caller_thread_cpu_ns"]=clock_ns(CLOCK_THREAD_CPUTIME_ID)-cpu;return row;
}
json sizes(const Document &d){
  const auto f=d.frame();size_t vertices=f.surface->mesh.positions.size(),triangles=f.surface->mesh.indices.size()/3,chunks=0,samples=0,scalar_bytes=0;
  const auto scene=d.copy_scene();for(const auto &object:scene.objects())samples+=object.volume->stored_samples();
  for(const auto &object:f.surface->sculpt_chunks->objects()){vertices+=object.geometry->report().vertices;triangles+=object.geometry->report().indices/3;object.geometry->for_each_chunk([&](const auto &c){++chunks;if(object.masks)if(auto m=object.masks->find(c))scalar_bytes+=m->values.size()*sizeof(float);});}
  return {{"stored_samples",samples},{"visible_vertices",vertices},{"visible_triangles",triangles},{"chunks",chunks},{"scalar_bytes",scalar_bytes},{"retained_chunk_bytes",f.surface->sculpt_chunks->retained_bytes()}};
}
struct Fbo {
  GLuint f=0,color=0,depth=0;int width,height;
  Fbo(int w,int h):width(w),height(h){glGenFramebuffers(1,&f);glBindFramebuffer(GL_FRAMEBUFFER,f);glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,w,h);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Benchmark FBO incomplete");}
  ~Fbo(){glDeleteRenderbuffers(1,&depth);glDeleteTextures(1,&color);glDeleteFramebuffers(1,&f);}
  void bind(){glBindFramebuffer(GL_FRAMEBUFFER,f);}
  std::vector<uint8_t> pixels(){std::vector<uint8_t> p(size_t(width)*height*4);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,p.data());require(glGetError()==GL_NO_ERROR,"Diagnostic readback failed");return p;}
};
struct TimerPool {
  struct Slot{GLuint query=0;bool pending=false;size_t frame=0;};std::array<Slot,16> slots{};std::vector<json> samples;size_t skipped=0;bool supported=false;Slot *active=nullptr;
  TimerPool(){GLint bits=0;glGetQueryiv(GL_TIME_ELAPSED,GL_QUERY_COUNTER_BITS,&bits);supported=bits>0&&glGetError()==GL_NO_ERROR;if(supported)for(auto &s:slots)glGenQueries(1,&s.query);}
  ~TimerPool(){for(auto &s:slots)if(s.query)glDeleteQueries(1,&s.query);}
  void collect(){for(auto &s:slots)if(s.pending){GLuint available=0;glGetQueryObjectuiv(s.query,GL_QUERY_RESULT_AVAILABLE,&available);if(available){GLuint64 ns=0;glGetQueryObjectui64v(s.query,GL_QUERY_RESULT,&ns);samples.push_back({{"index",s.frame},{"gpu_ns",ns}});s.pending=false;}}}
  void begin(size_t frame){if(!supported)return;for(auto &s:slots)if(!s.pending){active=&s;s.frame=frame;glBeginQuery(GL_TIME_ELAPSED,s.query);return;}++skipped;}
  void end(){if(active){glEndQuery(GL_TIME_ELAPSED);active->pending=true;active=nullptr;}}
  json drain(){const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(20);glFlush();while(std::any_of(slots.begin(),slots.end(),[](auto &s){return s.pending;})&&std::chrono::steady_clock::now()<until){collect();std::this_thread::sleep_for(std::chrono::milliseconds(1));}return {{"supported",supported},{"skipped_pool_full",skipped},{"pending_after_bounded_drain",std::count_if(slots.begin(),slots.end(),[](auto &s){return s.pending;})},{"samples",samples}};}
};
Mat4 view(const Options &o){const float extent=o.objects==1?1.f:o.objects==4?5.f:5.5f;return {1/extent,0,0,0,0,1/extent,0,0,0,0,.2f,0,-.35f,-.15f,0,1};}
void draw(Document &d,SurfaceRenderer &r,Fbo &f,const Options &o,CanvasTransform canvas={},bool ghosts=false){
  auto frame=d.frame();auto display=frame.state->objects;if(ghosts)for(auto &x:display)x.ghost=x.id!=frame.state->active_object;
  f.bind();r.set_frame_time(0);r.draw(frame.surface,canvas,view(o),{0,0,3},f.width,f.height,true,false,false,display);
}
void source_changes(json &row,const std::shared_ptr<const MeshSnapshot> &old,const std::shared_ptr<const MeshSnapshot> &next){
  size_t objects=0,changes=0,removed=0,extracted=0,assembled=0,reconciles=0,scalars=0,placements=0;
  for(const auto &n:next->sculpt_chunks->objects()){const auto *b=old->sculpt_chunks->find(n.object);if(!b||b->geometry!=n.geometry){++objects;const auto &r=n.geometry->report();extracted+=r.extracted_tiles;assembled+=r.assembled_chunks;
      if(!b||!n.geometry->for_each_change(b->geometry,[&](Int3,const auto &c){++changes;if(!c)++removed;})){++reconciles;n.geometry->for_each_chunk([&](const auto &){++changes;});}}
    if(!b||b->masks!=n.masks)++scalars;if(!b||b->placed!=n.placed)++placements;
  }
  row["owner_work"]={{"geometry_objects_changed",objects},{"chunk_delta_or_manifest_entries",changes},{"removed_chunks",removed},{"whole_manifest_reconciles",reconciles},{"reported_extracted_tiles_on_changed_owner",extracted},{"reported_assembled_chunks_on_changed_owner",assembled},{"scalar_directories_changed",scalars},{"placed_directories_changed",placements}};
}
bool bits(Vec3 a,Vec3 b){return std::bit_cast<std::array<uint32_t,3>>(a)==std::bit_cast<std::array<uint32_t,3>>(b);}
// Independent whole extraction at each checkpoint. Compare real source-local
// corner order and normals without ever baking a world-float Mesh.
json checkpoint(Document &d,SurfaceRenderer &actual,Fbo &f,const Options &o){
  const auto start=clock_ns(CLOCK_MONOTONIC);const auto scene=d.copy_scene();const auto current=d.frame();auto whole=std::make_shared<MeshSnapshot>();whole->generation=current.surface->generation;whole->geometry_identity=std::make_shared<const int>(1);
  auto display=current.state->objects;size_t corners=0,scalars=0,queries=0;
  for(const auto &object:scene.objects()){
    if(!object.visible)continue;const auto *captured=current.surface->sculpt_chunks->find(object.id);require(captured,"Visible object missing from actual publication");
    const auto reference=surface_mesh(*object.volume);size_t at=0;
    captured->geometry->for_each_run([&](const auto &run){const auto &m=run.chunk->mesh;for(size_t c=run.first;c<run.first+run.count;++c){require(at<reference.indices.size(),"Chunk stream exceeds whole reference");const auto i=m.indices.at(c),j=reference.indices.at(at++);require(bits(m.positions.at(i),reference.positions.at(j))&&bits(m.normals.at(i),reference.normals.at(j)),"Ordered local corner/normal differs from whole extraction");++corners;}});
    require(at==reference.indices.size(),"Whole reference corners missing from published stream");
    captured->geometry->for_each_chunk([&](const auto &c){if(captured->masks){auto values=captured->masks->find(c);require(bool(values),"Scalar owner missing");for(size_t i=0;i<values->values.size();++i){require(values->values[i]==object.volume->mask_sample(c->mesh.positions[i]),"Published protection differs from real Volume sampler");++scalars;}}});
    // Both query paths run on actual placed/reflected/sheared frames.
    const auto matrix=scene.world_matrix(object.id);auto as_vec=[](AffineVector p){return Vec3{float(p[0]),float(p[1]),float(p[2])};};const auto origin=as_vec(matrix.point({0,0,1.5}));const auto target=as_vec(matrix.point({0,0,0}));const auto dir=normalized(target-origin);
    auto ray=current.surface->picking->raycast_target(origin,dir,5,0,object.id);require(ray&&ray->object==object.id&&!ray->packed(),"Typed production ray lost actual object/chunk identity");require(captured->geometry->owns(ray->chunk),"Typed hit has a foreign immutable owner");++queries;
    auto near=current.surface->picking->closest_target(ray->surface.position+ray->surface.normal*.003f,object.id);require(near.object==object.id&&near.surface.distance<.004f,"Affine production nearest metric incorrect");++queries;
    SurfaceQuery independent(reference);const auto ray_reference=independent.raycast_result(origin,dir,5,matrix,{});require(ray_reference&&std::abs(ray->surface.distance-ray_reference->surface.distance)<3e-5f&&length(ray->surface.position-ray_reference->surface.position)<3e-5f&&length(ray->surface.normal-ray_reference->surface.normal)<3e-4f,"Typed production ray differs from independent whole-geometry metric");
    const auto nearest_reference=independent.nearest_result(ray->surface.position+ray->surface.normal*.003f,matrix,{});require(nearest_reference&&std::abs(near.surface.distance-nearest_reference->surface.distance)<3e-5f&&length(near.surface.position-nearest_reference->surface.position)<3e-5f,"Typed production nearest differs from independent whole-geometry metric");
    const auto offset=whole->mesh.positions.size(),first=whole->mesh.indices.size();whole->mesh.positions.insert(whole->mesh.positions.end(),reference.positions.begin(),reference.positions.end());whole->mesh.normals.insert(whole->mesh.normals.end(),reference.normals.begin(),reference.normals.end());whole->mesh.uv.resize(whole->mesh.positions.size());
    for(auto i:reference.indices)whole->mesh.indices.push_back(uint32_t(offset+i));SurfaceRange range{};range.object=object.id;range.first=first;range.count=reference.indices.size();range.first_vertex=offset;range.material=object.material;range.world_frame_valid=true;range.affine_world=AffineTransform{};whole->ranges.push_back(range);
    for(size_t i=0;i<reference.positions.size();++i)whole->sculpt_masks.push_back(captured->masks?object.volume->mask_sample(reference.positions[i]):-1.f);
    auto item=std::find_if(display.begin(),display.end(),[&](const auto &x){return x.id==object.id;});require(item!=display.end(),"Live display object missing");item->affine_world=matrix;item->world_transform_valid=true;
  }
  const auto extraction_end=clock_ns(CLOCK_MONOTONIC);SurfaceRenderer oracle;const auto vp=view(o);
  for(int mode=0;mode<3;++mode){auto flags=display;if(mode==1)for(auto &x:flags)x.ghost=x.id!=scene.active().id;if(mode==2&&flags.size()>1)flags[1].visible=false;
    f.bind();actual.set_frame_time(0);actual.draw(current.surface,{},vp,{0,0,3},f.width,f.height,true,false,false,flags);const auto pixels=f.pixels();
    oracle.set_frame_time(0);oracle.draw(whole,{},vp,{0,0,3},f.width,f.height,true,false,false,flags);require(f.pixels()==pixels,"Actual production frame differs from independent whole-local reference");
    require(std::any_of(pixels.begin(),pixels.end(),[](auto v){return v!=0;}),"Production reference is blank");
  }
  return {{"observer_ns_excluded",clock_ns(CLOCK_MONOTONIC)-start},{"whole_extraction_and_checks_ns",extraction_end-start},{"ordered_corners",corners},{"checked_scalars",scalars},{"typed_queries",queries},{"exact_frame_modes",3}};
}
NativeStrokeSample input(size_t i,bool mask){NativeStrokeSample s;const float angle=float(i%30)*.20943951f;s.origin={.24f*std::cos(angle),.24f*std::sin(angle),1.5f};s.direction={0,0,-1};s.brush.radius=.045f;s.brush.blend=.006f;s.brush.strength=.23f;s.brush.tool=mask||i%2==0?Tool::Clay:Tool::Carve;return s;}
void stroke(Document &d,const NativeStrokeSample &sample){require(d.begin_native_stroke(),"Actual native gesture refused");require(d.sample_native_stroke(sample),"Actual native sample refused");d.end_native_stroke(true);require(d.wait_native_stroke(std::chrono::seconds(120)),"Native worker did not finish");ready(d);}
json history(Document &d,const std::filesystem::path &output){
  std::filesystem::create_directories(output);
  const auto original=d.copy_scene().encode();const auto kept=d.frame().surface;
  require(d.begin_native_stroke(),"Cancel gesture refused");require(d.sample_native_stroke(input(7,false)),"Cancel sample refused");d.end_native_stroke(false);require(d.wait_native_stroke(std::chrono::seconds(120)),"Cancel did not complete");ready(d);require(d.copy_scene().encode()==original,"Cancel changed source/history endpoint");
  stroke(d,input(9,false));const auto accepted=d.copy_scene().encode();require(accepted!=original,"History control did no real work");
  auto save=d.save_async(output/"accepted.dsculpt");save.get();require(Scene::load(output/"accepted.dsculpt").encode()==accepted,"Ordered save/reopen differs from accepted source");
  require(d.undo(),"Undo refused");ready(d);require(d.copy_scene().encode()==original,"Undo source mismatch");require(d.redo(),"Redo refused");ready(d);require(d.copy_scene().encode()==accepted,"Redo source mismatch");
  // Hold a genuine completed worker factory result at its actual return seam,
  // replace the source generation, then require the stale candidate rejected.
  auto replacement=d.copy_scene();sustained::capture_release=false;sustained::capture_waiting=false;sustained::stop_at_capture=true;
  struct Release{~Release(){sustained::stop_at_capture=false;sustained::capture_release=true;}}release;
  require(d.begin_native_stroke(),"Retired-source gesture refused");require(d.sample_native_stroke(input(11,false)),"Retired-source sample refused");d.end_native_stroke(true);require(d.wait_native_stroke(std::chrono::seconds(120)),"Retired native worker did not finish");
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(120);while(!sustained::capture_waiting.load()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));require(sustained::capture_waiting,"Real completed worker capture seam was not reached");
  d.replace(replacement,false);sustained::capture_release=true;ready(d);require(d.copy_scene().encode()==accepted,"Retired worker overwrote replacement source");require(d.frame().surface->generation!=sustained::blocked_generation&&d.frame().surface->generation!=kept->generation,"A real retired generation candidate was accepted");
  require(kept->sculpt_chunks&&kept->sculpt_chunks->find(1)->geometry->report().indices,"Previously acquired immutable source was not retained");
  return {{"cancel_exact",true},{"undo_redo_exact",true},{"ordered_save_reopen_exact",true},{"real_factory_return_barrier_reached",true},{"retired_native_generation_exact",true}};
}
json distribution(std::vector<double> values){if(values.empty())return {{"count",0}};std::sort(values.begin(),values.end());auto p=[&](double q){return values[std::min(values.size()-1,size_t(std::ceil(q*values.size())-1))];};double sum=0;for(auto v:values)sum+=v;return {{"count",values.size()},{"mean_ns",sum/values.size()},{"p50_ns",p(.5)},{"p90_ns",p(.9)},{"p95_ns",p(.95)},{"p99_ns",p(.99)},{"max_ns",values.back()}};}
json summarize(const json &rows){json report;for(const std::string kind:{"stroke","mask","view_pair","held_pair","authored_placement","mask_fill","mask_clear"}){std::vector<double> times;for(const auto &row:rows)if(row["kind"]==kind)times.push_back(row["wall_ns"].get<double>());report[kind]=distribution(std::move(times));}return report;}
}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
extern "C" void __real_glBufferSubData(GLenum,GLintptr,GLsizeiptr,const void*);
extern "C" void __wrap_glBufferData(GLenum target,GLsizeiptr n,const void *data,GLenum usage){if(sustained::measuring.load()&&n>0){++sustained::buffer_calls;sustained::buffer_allocation_bytes+=uint64_t(n);if(data)sustained::buffer_payload_bytes+=uint64_t(n);if(target==GL_ARRAY_BUFFER&&usage==GL_DYNAMIC_DRAW&&data){++sustained::scalar_calls;sustained::scalar_payload_bytes+=uint64_t(n);}}__real_glBufferData(target,n,data,usage);}
extern "C" void __wrap_glBufferSubData(GLenum target,GLintptr offset,GLsizeiptr n,const void *data){if(sustained::measuring.load()&&n>0){++sustained::buffer_calls;if(data)sustained::buffer_payload_bytes+=uint64_t(n);}__real_glBufferSubData(target,offset,n,data);}
int main(int argc,char **argv){
  json report={{"status","started"},{"scope","native CPU brushes, actual Document worker publication, immutable OpenGL renderer; no headset or OneAPI claim"},{"inclusive_stage_times_must_not_be_summed",true},{"allocations_scope","optional C++ new requested bytes only; no C malloc/driver/live-byte claim"},{"timing_scope","offscreen uncapped stereo pairs; asynchronous GL timer pool; no measured pixel readback or glFinish"}};
  std::filesystem::path output;
  try{
    const auto o=options(argc,argv);output=o.output;std::filesystem::create_directories(output);sustained::allocations=o.count_allocations;
    report["options"]={{"tier",o.tier},{"smoke",o.smoke},{"objects",o.objects},{"voxel",o.voxel},{"strokes",o.strokes},{"masks",o.masks},{"pairs",o.pairs},{"repetitions",o.repetitions},{"width",o.width},{"height",o.height},{"allocation_instrumentation",o.count_allocations}};
    const auto allocation_before=Counters::read();sustained::measuring=true;
    for(size_t alignment:{size_t(1),size_t(2),size_t(4),size_t(8),size_t(16)}){void *p=::operator new(37,std::align_val_t(alignment));require(reinterpret_cast<uintptr_t>(p)%alignment==0,"Actual aligned allocation control returned misaligned memory");::operator delete(p,std::align_val_t(alignment));}
    sustained::measuring=false;const auto allocation_after=Counters::read();require(allocation_after.news-allocation_before.news==(o.count_allocations?5u:0u)&&allocation_after.new_bytes-allocation_before.new_bytes==(o.count_allocations?185u:0u),"Actual aligned allocation counters differ from requested operations");report["aligned_cpp_new_controls"]={{"alignments",{1,2,4,8,16}},{"counted_calls",allocation_after.news-allocation_before.news},{"counted_requested_bytes",allocation_after.new_bytes-allocation_before.new_bytes}};
    report["backend"]=std::string(compute_backend());require(compute_backend()=="CPU","This gate is explicitly the CPU brush/mesher baseline");
    NativeGraphics graphics;report["gl"]={{"renderer",reinterpret_cast<const char*>(glGetString(GL_RENDERER))},{"vendor",reinterpret_cast<const char*>(glGetString(GL_VENDOR))},{"version",reinterpret_cast<const char*>(glGetString(GL_VERSION))}};
    const auto renderer=report["gl"]["renderer"].get<std::string>();require(renderer.find("llvmpipe")!=std::string::npos||renderer.find("softpipe")!=std::string::npos,"This executable stage requires actual software GL; strict Arc is a separately leased stage");
    Fbo target(o.width,o.height);{Document original;SurfaceRenderer gpu;original.replace(grouped_regression());original.configure_object_grab(true,false);ready(original);report["unchanged_original_grouped_regression"]=checkpoint(original,gpu,target,o);}
    const auto source=fixture(o);json inputs={{"strokes",json::array()},{"masks",json::array()}};for(size_t i=0;i<o.strokes;++i){auto s=input(i,false);inputs["strokes"].push_back({{"index",i},{"origin",{s.origin.x,s.origin.y,s.origin.z}},{"direction",{0,0,-1}},{"tool",int(s.brush.tool)},{"radius",s.brush.radius},{"strength",s.brush.strength},{"blend",s.brush.blend}});}for(size_t i=0;i<o.masks;++i){auto s=input(i/2,true);inputs["masks"].push_back({{"index",i},{"origin",{s.origin.x,s.origin.y,s.origin.z}},{"mask_tool",i%2?"subtract":"add"},{"radius",s.brush.radius},{"strength",s.brush.strength},{"blend",s.brush.blend}});}write(output/"fixed-inputs.json",inputs);
    report["repetitions"]=json::array();
    for(size_t rep=0;rep<o.repetitions;++rep){
      Document d;SurfaceRenderer gpu;auto cold=measure("cold_publication",0,[&]{d.replace(source.snapshot());d.configure_object_grab(true,false);ready(d);draw(d,gpu,target,o);});
      const auto size=sizes(d);report["initial_size"]=size;if(!o.smoke&&o.tier=="production"&&size["visible_triangles"].get<size_t>()<250000)throw Refusal("Production tier has fewer than 250,000 actual visible triangles");
      json run={{"index",rep},{"cold",cold},{"size",size},{"rows",json::array()},{"checkpoints",json::array()}};
      run["checkpoints"].push_back(checkpoint(d,gpu,target,o));
      // Declared unmeasured warm-up performs real work, then restores input.
      stroke(d,input(0,false));require(d.undo(),"Warm-up Undo refused");ready(d);draw(d,gpu,target,o);const auto retained=d.frame().surface;
      for(size_t i=0;i<o.strokes;++i){const auto before=d.frame().surface;auto row=measure("stroke",i,[&]{stroke(d,input(i,false));draw(d,gpu,target,o);});const auto after=d.frame().surface;require(before->sculpt_chunks->find(1)->geometry!=after->sculpt_chunks->find(1)->geometry,"Declared accepted local stroke made no geometry change");source_changes(row,before,after);run["rows"].push_back(std::move(row));if((i+1)%30==0)run["checkpoints"].push_back(checkpoint(d,gpu,target,o));}
      auto settings=d.copy_scene().active().volume->sculpt_settings();settings.mask_edit=true;settings.mask_visible=true;d.sculpt_settings(settings);ready(d);
      for(size_t i=0;i<o.masks;++i){settings.mask_tool=i%2?Volume::MaskTool::Subtract:Volume::MaskTool::Add;d.sculpt_settings(settings);ready(d);const auto before=d.frame().surface;const auto transfers=gpu.buffer_stats();auto row=measure("mask",i,[&]{stroke(d,input(i/2,true));draw(d,gpu,target,o);});const auto after=d.frame().surface;require(before->sculpt_chunks->find(1)->masks!=after->sculpt_chunks->find(1)->masks,"Declared mask stroke did no scalar work");require(before->sculpt_chunks->find(1)->geometry==after->sculpt_chunks->find(1)->geometry&&before->sculpt_chunks->find(1)->placed==after->sculpt_chunks->find(1)->placed,"Mask rebuilt geometry/query");require(gpu.buffer_stats().geometry_uploads==transfers.geometry_uploads,"Mask caused GPU geometry upload");source_changes(row,before,after);run["rows"].push_back(std::move(row));}
      for(const auto value:{.7f,0.f}){auto row=measure(value?"mask_fill":"mask_clear",0,[&]{d.sculpt_mask_fill(value);require(d.wait_asset(std::chrono::seconds(120)),"Mask file worker timed out");ready(d);draw(d,gpu,target,o);});run["rows"].push_back(std::move(row));}
      settings.mask_edit=false;d.sculpt_settings(settings);ready(d);run["checkpoints"].push_back(checkpoint(d,gpu,target,o));
      TimerPool timers;timers.samples.reserve(o.pairs);run["timer_query_unit"]="one complete two-eye view pair";
      for(size_t i=0;i<o.pairs;++i){CanvasTransform canvas;canvas.position.x=.03f*std::sin(float(i)*.01f);const auto before=gpu.buffer_stats();const auto observe=clock_ns(CLOCK_MONOTONIC);timers.collect();const auto observer_ns=clock_ns(CLOCK_MONOTONIC)-observe;auto row=measure("view_pair",i,[&]{require(d.pick_object({0,0,1.5f},{0,0,-1}).has_value(),"Real controller object ray missed");require(d.frame().surface->picking->nearby_target({0,0,.61f},.3f).has_value(),"Real direct controller contact missed");timers.begin(i);draw(d,gpu,target,o,canvas,i%3==1);canvas.position.x+=.004f;draw(d,gpu,target,o,canvas,i%3==1);timers.end();glFlush();});row["includes_actual_controller_ray_and_contact"]=true;row["timer_observer_ns_excluded"]=observer_ns;require(gpu.buffer_stats().geometry_uploads==before.geometry_uploads&&gpu.buffer_stats().mask_uploads==before.mask_uploads,"Unchanged stereo view caused geometry/scalar upload");run["rows"].push_back(std::move(row));}
      run["gpu_timers"]=timers.drain();require(run["gpu_timers"]["pending_after_bounded_drain"]==0,"GPU timer drain timed out");
      require(d.begin_object_grab(1),"Held motion refused");const auto held=d.frame().surface;const auto transfers=gpu.buffer_stats();
      for(size_t i=0;i<(o.smoke?4:120);++i){CanvasTransform delta;delta.position.x=.025f*std::sin(float(i)*.1f);run["rows"].push_back(measure("held_pair",i,[&]{d.move_held_object_world(delta);draw(d,gpu,target,o);draw(d,gpu,target,o);}));require(d.frame().surface==held,"Held motion baked a new geometry snapshot");}
      d.end_object_grab(false);ready(d);require(gpu.buffer_stats().geometry_uploads==transfers.geometry_uploads,"Held motion uploaded geometry");
      const auto ids=o.objects==1?std::vector<uint64_t>{1}:std::vector<uint64_t>{1,uint64_t(o.objects)};
      for(auto id:ids){auto authored=d.copy_scene();auto &obj=authored.object(id);const auto before=d.frame().surface;obj.transform.position.x+=.02f;
        auto move=measure("authored_placement",id,[&]{d.object_properties(obj.id,obj.name,obj.visible,obj.locked,obj.transform,obj.material);ready(d);draw(d,gpu,target,o);});const auto after=d.frame().surface;source_changes(move,before,after);const auto *old=before->sculpt_chunks->find(id),*now=after->sculpt_chunks->find(id);require(now->geometry==old->geometry,"Authored similarity/affine placement rebuilt local extraction");size_t reused=0;now->placed->for_each([&](const auto &p){const auto previous=old->placed->find(p->source());require(previous&&p->geometry_owner()==previous->geometry_owner()&&p->query()==previous->query(),"Authored similarity/affine motion rebuilt immutable local geometry/BVH");++reused;});move["actual_local_geometry_and_bvh_owners_reused"]=reused;run["rows"].push_back(std::move(move));}
      run["checkpoints"].push_back(checkpoint(d,gpu,target,o));run["history"]=history(d,output/("rep-"+std::to_string(rep)));require(retained->sculpt_chunks->find(1)->geometry->report().indices>0,"Old acquired owner invalid after sustained work");run["checkpoints"].push_back(checkpoint(d,gpu,target,o));run["summary"]=summarize(run["rows"]);report["repetitions"].push_back(std::move(run));write(output/"result.json",report);
    }
    const auto observed=Counters::read();require(observed.stages[Capture].calls&&observed.stages[Placement].calls&&observed.stages[ExtractionUpdate].calls&&observed.stages[Ray].calls,"Actual measurement wrappers were not reached");
    rusage usage{};getrusage(RUSAGE_SELF,&usage);report["peak_rss_kib"]=usage.ru_maxrss;report["status"]=o.smoke?"smoke-pass-not-production":"pass";report["remaining_costs"]={"whole GPU directory reconciliation","changed-mask object-wide scalar sampling","separate pre-chunk executable baseline","actual Arc device lease and measurements","physical headset pacing","affine Volume sculpt mutations remain refused"};write(output/"result.json",report);std::cout<<report["status"]<<'\n';return 0;
  }catch(const Refusal &e){report["status"]="resource-or-size-refusal";report["reason"]=e.what();if(!output.empty())write(output/"result.json",report);std::cerr<<"REFUSED "<<e.what()<<'\n';return 3;}
  catch(const std::exception &e){sustained::measuring=false;report["status"]="failed";report["reason"]=e.what();if(!output.empty())write(output/"result.json",report);std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
