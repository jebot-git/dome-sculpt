// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_VRM1_SOURCE_EXPORT_NO_MAIN
#include "vrm1-source-export.cpp"
#undef DOME_VRM1_SOURCE_EXPORT_NO_MAIN
#include "dome/vrm-authoring.hpp"
#include "dome/scene-file.hpp"
#include "dome/source-uv-bake.hpp"
#include "emission-package.hpp"
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <sys/stat.h>
namespace v1_file_gate {
std::mutex mutex;std::condition_variable changed;bool entered=false,released=false;std::atomic<bool> armed=false;
void arm(){std::lock_guard lock(mutex);entered=released=false;armed=true;}
void release(){std::lock_guard lock(mutex);released=true;changed.notify_all();}
bool wait(){std::unique_lock lock(mutex);return changed.wait_for(lock,std::chrono::seconds(120),[]{return entered;});}
struct Guard{~Guard(){release();}};
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd){const auto result=__real_fsync(fd);struct stat st{};if(!result&&!fstat(fd,&st)&&S_ISREG(st.st_mode)&&v1_file_gate::armed.exchange(false)){std::unique_lock lock(v1_file_gate::mutex);v1_file_gate::entered=true;v1_file_gate::changed.notify_all();v1_file_gate::changed.wait(lock,[]{return v1_file_gate::released;});}return result;}
namespace {
void paint(Document &document,PaintChannel channel=PaintChannel::Color){
 auto initial=document.copy_scene();const auto &object=initial.active();
 const auto evaluated=object.deformation?object.deformation->preview(object.painted->mesh()):object.painted->mesh();
 const auto a=evaluated.indices[0],b=evaluated.indices[1],c=evaluated.indices[2];
 const auto p=evaluated.positions[a]*.2f+evaluated.positions[b]*.3f+evaluated.positions[c]*.5f;
 const auto normal=normalized(cross(evaluated.positions[b]-evaluated.positions[a],evaluated.positions[c]-evaluated.positions[a]));
 const auto world=initial.world_matrix(object.id);const auto q=world.point({p.x,p.y,p.z}),n=world.normal({normal.x,normal.y,normal.z});
 const Vec3 point{float(q[0]),float(q[1]),float(q[2])},direction=normalized(Vec3{float(n[0]),float(n[1]),float(n[2])});
 PaintSettings ink;ink.channel=channel;ink.emission={.04f,.09f,.02f};ink.color={.2f,.8f,.3f,1};ink.front_only=false;document.paint_settings(ink);
 NativeStrokeSample sample;sample.origin=point+direction;sample.direction=direction*-1;sample.brush.radius=.2f;sample.brush.blend=0;
 check(document.begin_native_stroke()&&document.sample_native_stroke(sample),"Actual native worker accepts real source-owned V1 colour gesture");
 document.end_native_stroke();check(document.wait_native_stroke()&&document.wait_mesh()&&document.frame().state->error.empty(),"Actual native V1 paint completes without losing source authority");
 check(document.copy_scene().encode()!=initial.encode(),"Real native V1 gesture changes persistent paint data");
}
template<class F> std::string refusal(F f){try{f();}catch(const std::exception &e){return e.what();}return {};}
void geometry(const Scene &scene,const std::filesystem::path &path){
 const auto surfaces=scene.preview_surfaces();const auto raw=bytes(path);cgltf_options opts{};cgltf_data *data=nullptr;
 check(cgltf_parse(&opts,raw.data(),raw.size(),&data)==cgltf_result_success,"Independent cgltf parses actual painted V1 package");
 struct Guard{cgltf_data *p;~Guard(){cgltf_free(p);}}guard{data};
 check(cgltf_load_buffers(&opts,data,path.c_str())==cgltf_result_success&&cgltf_validate(data)==cgltf_result_success,"Independent consumer validates actual V1 buffers/accessors");
 auto attribute=[](const cgltf_attribute *a,size_t n,cgltf_attribute_type type,int set=0)->const cgltf_accessor*{for(size_t i=0;i<n;++i)if(a[i].type==type&&a[i].index==set)return a[i].data;return nullptr;};
 auto point=[](const std::array<float,16>&m,Vec3 p){return Vec3{m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};};
 const auto root=json(path);const auto &maps=root.at("extras").at("domeSculptAuthoring").at("objects");
 check(maps.size()==surfaces.size(),"Actual final V1 graph has one exact native object remap per surface");
 float maximum=0;size_t corners=0,skins=0,rigid=0;
 for(size_t s=0;s<surfaces.size();++s){const auto &surface=surfaces[s];const auto &map=maps.at(s);const auto id=map.at("object").get<std::string>();check(id==std::to_string(surface.id),"Final graph remap uses genuine native identity");
  auto &node=data->nodes[map.at("node").get<size_t>()];const auto &primitive=node.mesh->primitives[0];auto current=surface.preview_mesh();const auto world=surface.world_matrix();
  const auto *positions=attribute(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_position);check(positions,"Independent emitted POSITION accessor exists");
  const auto count=primitive.indices?primitive.indices->count:positions->count;check(count==current.indices.size(),"Actual V1 writer preserves all ordered triangle corners");
  std::array<float,16> node_world;cgltf_node_transform_world(&node,node_world.data());std::vector<std::array<float,16>> matrices;
  if(node.skin){++skins;for(size_t j=0;j<node.skin->joints_count;++j){std::array<float,16>w,ibm{},product{};cgltf_node_transform_world(node.skin->joints[j],w.data());check(node.skin->inverse_bind_matrices&&cgltf_accessor_read_float(node.skin->inverse_bind_matrices,j,ibm.data(),16),"Actual per-skin IBM independently decodes");for(size_t c=0;c<4;++c)for(size_t r=0;r<4;++r)for(size_t k=0;k<4;++k)product[c*4+r]+=w[k*4+r]*ibm[c*4+k];matrices.push_back(product);}}else ++rigid;
  for(size_t c=0;c<count;++c){const auto v=primitive.indices?cgltf_accessor_read_index(primitive.indices,c):c;float p[3];check(cgltf_accessor_read_float(positions,v,p,3),"Actual emitted position independently reads");
   for(size_t t=0;t<primitive.targets_count;++t){const auto *d=attribute(primitive.targets[t].attributes,primitive.targets[t].attributes_count,cgltf_attribute_type_position);if(d){float q[3];check(cgltf_accessor_read_float(d,v,q,3),"Actual morph endpoint independently reads");const float weight=node.weights_count?node.weights[t]:node.mesh->weights_count?node.mesh->weights[t]:0;for(size_t k=0;k<3;++k)p[k]+=q[k]*weight;}}
   Vec3 actual{},input{p[0],p[1],p[2]};if(!node.skin)actual=point(node_world,input);else for(int set=0;;++set){const auto *js=attribute(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_joints,set);if(!js)break;const auto *ws=attribute(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_weights,set);float j[4],w[4];check(ws&&cgltf_accessor_read_float(js,v,j,4)&&cgltf_accessor_read_float(ws,v,w,4),"Real skin weights/joint slots decode independently");for(size_t k=0;k<4;++k)if(w[k])actual+=point(matrices.at(size_t(j[k])),input)*w[k];}
   const auto wanted=current.positions[current.indices[c]];const auto q=world.point({wanted.x,wanted.y,wanted.z});maximum=std::max(maximum,length(actual-Vec3{float(q[0]),float(q[1]),float(q[2])}));++corners;
  }
 }
 check(skins==2&&rigid==1&&corners>0&&maximum<2e-6f,"Independent original-node skin/rigid/morph world geometry matches native within two micrometres");
 std::cout<<"PAINTED_V1_GEOMETRY corners="<<corners<<" max="<<maximum<<'\n';
}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 for(int variant:{0,1,2}){
  const bool expressions=variant==1;
  auto fixture=metadata_source();if(!expressions){auto &happy=fixture.j["extensions"]["VRMC_vrm"]["expressions"]["preset"]["happy"];happy.erase("materialColorBinds");happy.erase("textureTransformBinds");}
  if(variant==2){
   auto &material=fixture.j["materials"][0];material["alphaMode"]="OPAQUE";
   auto &toon=material["extensions"]["VRMC_materials_mtoon"];toon.erase("matcapTexture");toon["shadeColorFactor"]={.3f,.4f,.5f};
   toon["shadeMultiplyTexture"]=material["pbrMetallicRoughness"]["baseColorTexture"];
   material["emissiveFactor"]={.03f,.02f,.01f};material["emissiveTexture"]={{"index",0},{"texCoord",1}};
   Image flat{2,2,std::vector<uint8_t>(16)};for(size_t i=0;i<16;i+=4){flat.rgba[i]=180;flat.rgba[i+1]=210;flat.rgba[i+2]=160;flat.rgba[i+3]=255;}
   auto png=flat.png();while(fixture.bin.size()%4)fixture.bin.push_back(0);const auto view=fixture.j["bufferViews"].size();fixture.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",fixture.bin.size()},{"byteLength",png.size()}});fixture.bin.insert(fixture.bin.end(),png.begin(),png.end());fixture.j["images"][0]["bufferView"]=view;
  }
  const auto dir=out/(expressions?"expressions":variant==2?"graphics":"static");std::filesystem::create_directories(dir);const auto input=dir/"source.vrm";fixture.save(input);
  Scene initial;initial.active().visible=false;ImportAuthoringOptions import;import.paint_resolution=64;import.prepare_texture_atlas=true;
  auto admitted=prepare_authoring_import(initial,input,import);Document document;document.replace(admitted.scene);document.select_object(admitted.objects.front());check(document.wait_mesh()&&document.frame().state->error.empty(),"Actual V1 source import reaches immutable native worker endpoint");
  paint(document);if(variant==2)paint(document,PaintChannel::Emission);const auto native=document.copy_scene();const auto before=native.encode();const auto saved=dir/"painted.dsculpt";document.save_async(saved).get();check(Scene::load(saved).encode()==before,"Actual painted V1 native Save/restart retains exact source and layers");
  const auto ordinary=dir/"ordinary.vrm";const auto old=refusal([&]{export_vrm(native.preview_surfaces(),ordinary);});check(!old.empty()&&!std::filesystem::exists(ordinary),"Ordinary painted V1 writer retains explicit unsupported compiler boundary");std::cout<<"OLD_PAINTED_V1_REFUSAL "<<old<<'\n';
  NativeAuthoringExport owner;owner.scene=std::make_shared<const Scene>(native.snapshot());owner.allow_portable_fallback=true;VrmExportOptions options;options.paint_resolution=64;const auto package=dir/"editable.vrm";
  if(expressions){const auto error=refusal([&]{export_vrm_authoring(owner,package,options);});std::cout<<"EXPRESSION_REFUSAL "<<error<<'\n';check(native.active().material_bindings->material.parameters.alpha==MToonAlpha::Mask&&native.active().material_bindings->roles[size_t(MToonSlot::Matcap)].image&&error=="VRM1 expression layer role needs its independent consumer adapter"&&!std::filesystem::exists(package)&&native.encode()==before,"Actual painted MASK source with genuine MatCap retains its specific role-adapter refusal and exact file/Scene rollback");const auto error_worker=refusal([&]{document.export_asset(package,AssetExportFormat::EditableAvatar256).get();});document.save_async(dir/"expression-after.dsculpt").get();check(error_worker==error&&!std::filesystem::exists(package)&&Scene::load(dir/"expression-after.dsculpt").encode()==before,"Actual expression-guarded Avatar worker preserves file/Scene and exact following Save");continue;}
  const auto report=export_vrm_authoring(owner,package,options);check(report.meshes==3&&report.painted_materials==1,"Actual typed V1 compiler publishes all source parts and one genuinely painted material");
  const auto encoded=bytes(package);auto recovered=extract_native_authoring(encoded);check(recovered&&recovered->encode()==before&&native.encode()==before,"Final V1 package recovers exact native source/paint/history without mutating caller");check(load_scene_file(package).kind==SceneFileKind::AuthoringPackage&&Scene::load(package).encode()==before,"Real native package Open discriminator restores the V1 authoring project");
  const auto emitted=json(package);check(emitted.at("extensions").at("VRMC_vrm").at("specVersion")=="1.0"&&!emitted.at("extensions").contains("VRM")&&emitted.at("extensions").contains("VRMC_springBone"),"Actual V1 meta/expressions/spring version stays typed without VRM0 relabel");geometry(native,package);
  const auto worker_path=dir/"worker-editable.vrm";auto worker=document.export_asset(worker_path,AssetExportFormat::EditableAvatar256);auto ordered=document.save_async(dir/"worker-ordered.dsculpt");worker.get();ordered.get();
  auto worker_native=extract_native_authoring(bytes(worker_path));auto expected=detail::emission_export_subset(native,admitted.objects);
  check(worker_native&&worker_native->encode()==expected.encode()&&document.copy_scene().encode()==before&&Scene::load(dir/"worker-ordered.dsculpt").encode()==before,"Actual explicit Avatar FileJob retains declared selected-avatar subset plus dependencies and exact following full native Save");
  Document opened;opened.open_async(worker_path).get();check(opened.copy_scene().encode()==expected.encode()&&opened.frame().state->project_path.empty()&&opened.frame().state->dirty,"Actual editable V1 worker package Open restores unsaved exact native authoring data");
  opened.select_object(admitted.objects.front());opened.export_asset(dir/"worker-second.vrm",AssetExportFormat::EditableAvatar256).get();check(bytes(dir/"worker-second.vrm")==bytes(worker_path),"Actual Avatar worker Open/export cycle has no recursive native payload growth");
  if(variant==2){
   const auto queued=dir/"cancel-queued.vrm",cancel_save=dir/"cancel-ordered.dsculpt";{std::ofstream f(queued,std::ios::binary);f<<"Prior queued output";}const auto prior=bytes(worker_path),prior_queued=bytes(queued);
   v1_file_gate::Guard guard;v1_file_gate::arm();auto running=document.export_asset(worker_path,AssetExportFormat::EditableAvatar256);check(v1_file_gate::wait(),"Actual painted V1 FileJob reaches durable temporary output before atomic publication");auto waiting=document.export_asset(queued,AssetExportFormat::EditableAvatar256);auto after=document.save_async(cancel_save);check(document.frame().state->pending_exports==2&&document.frame().state->can_cancel_asset,"Actual running and queued editable V1 exports expose shared Cancel ownership");document.cancel_background_work();v1_file_gate::release();check(refusal([&]{running.get();}).find("cancelled")!=std::string::npos&&refusal([&]{waiting.get();}).find("cancelled")!=std::string::npos,"Actual shared Cancel retires running and queued painted V1 FileJobs");after.get();check(bytes(worker_path)==prior&&bytes(queued)==prior_queued&&document.copy_scene().encode()==before&&Scene::load(cancel_save).encode()==before,"Real V1 worker Cancel preserves both prior outputs and exact following native Save");document.export_asset(worker_path,AssetExportFormat::EditableAvatar256).get();check(bytes(worker_path)==prior&&!document.frame().state->pending_exports,"Actual painted V1 worker retry remains deterministic after cancellation");
  }
  const auto second=dir/"second.vrm";owner.scene=std::make_shared<const Scene>(recovered->snapshot());export_vrm_authoring(owner,second,options);check(bytes(second)==encoded,"Exact native V1 package re-export is byte-identical on second cycle");
  for(int limit=0;limit<5;++limit){SourceUvBakeOptions policy;policy.resolution=64;if(limit==0)policy.max_working_bytes=4;if(limit==1)policy.max_output_bytes=4;if(limit==2)policy.max_sample_operations=4;if(limit==3)policy.max_raster_samples=4;if(limit==4)policy.max_vertices=1;check(!refusal([&]{export_vrm_authoring(owner,package,options,policy);}).empty()&&bytes(package)==encoded&&owner.scene->encode()==before,"Prospective authoring quota refusal preserves exact native owner and prior file");}
  options.before_commit=[](){throw std::runtime_error("actual V1 atomic boundary cancelled");};check(refusal([&]{export_vrm_authoring(owner,package,options);}).find("cancelled")!=std::string::npos&&bytes(package)==encoded&&owner.scene->encode()==before,"Actual atomic V1 publication cancellation preserves prior package and native Scene");
 }
 std::cout<<"PASS "<<checks<<" actual typed painted V1 authoring checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
