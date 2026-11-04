// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/vrm-expression.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/material-visibility.hpp"
#include "dome/import-authoring.hpp"
#include <fstream>
#include "source-skin-world.hpp"
#include <iostream>
#include <new>
#include <cstdlib>
namespace {thread_local bool injecting=false,denied=false;thread_local size_t allocations=0,boundary=SIZE_MAX;}
[[gnu::noinline]]void *operator new(size_t n){if(injecting&&++allocations>=boundary){denied=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]]void *operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
using namespace dome;
namespace {
size_t checks=0;void check(bool good,const char *text){if(!good)throw std::runtime_error(text);++checks;std::cout<<"PASS "<<text<<std::endl;}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
bool same(const std::vector<Vec3> &a,const std::vector<Vec3> &b){if(a.size()!=b.size())return false;for(size_t v=0;v<a.size();++v)if(a[v].x!=b[v].x||a[v].y!=b[v].y||a[v].z!=b[v].z)return false;return true;}
ObjectSurface active_surface(const Scene&s){auto surfaces=s.preview_surfaces();for(auto &surface:surfaces)if(surface.id==s.active().id)return std::move(surface);throw std::runtime_error("Actual visible source skin missing");}
void unused_source(const std::filesystem::path &source,const std::filesystem::path &output){
 std::ifstream input(source,std::ios::binary);std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
 auto word=[&](size_t at){return uint32_t(bytes.at(at))|(uint32_t(bytes.at(at+1))<<8)|(uint32_t(bytes.at(at+2))<<16)|(uint32_t(bytes.at(at+3))<<24);};
 if(word(0)!=0x46546c67||word(16)!=0x4e4f534a)throw std::runtime_error("Actual fixture is not GLB");
 const size_t count=word(12),bin_at=20+count;if(word(bin_at+4)!=0x004e4942)throw std::runtime_error("Actual fixture lost BIN chunk");
 auto json=nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+count);for(size_t i=0;i<6000;++i)json["nodes"].push_back({{"name","Retained unused source node "+std::to_string(i)}});
 auto text=json.dump();while(text.size()%4)text.push_back(' ');std::vector<uint8_t> result;
 auto put=[&](uint32_t value){for(size_t k=0;k<4;++k)result.push_back(uint8_t(value>>(k*8)));};
 put(0x46546c67);put(2);put(uint32_t(20+text.size()+bytes.size()-bin_at));put(uint32_t(text.size()));put(0x4e4f534a);result.insert(result.end(),text.begin(),text.end());result.insert(result.end(),bytes.begin()+bin_at,bytes.end());
 std::ofstream file(output,std::ios::binary);file.write(reinterpret_cast<const char*>(result.data()),result.size());if(!file)throw std::runtime_error("Could not write genuine bounded raw fixture");
}
void ready(Document &doc){check(doc.wait_mesh(std::chrono::seconds(90))&&doc.frame().state->error.empty(),"Actual coherent source-world mesh/query publishes");}
}
int main(int argc,char **argv)try{
 const bool full_faults=argc==4&&std::string_view(argv[3])=="--faults";if(argc!=3&&!full_faults)return 2;const auto baseline=Scene::load(argv[1]);const auto original=baseline.encode();const auto id=baseline.active().id;const auto zero=bake_vrm1_expression_pose(baseline,id,{});const auto exact=zero.encode();const auto surface=active_surface(zero);const auto expected=detail::source_skin_world_positions(surface);check(expected&&expected->size()==surface.mesh.positions.size(),"Authenticated original source skin has a complete world position result");
 check(expected->at(0).x==.37999999523162842f&&expected->at(1).x==.62000000476837158f,"Original source corner X survives exact mesh-node inverse cancellation before final float conversion");
 const auto old=surface.preview_mesh();const auto frame=detail::placement_frame(surface);check(detail::placed_position(frame,old.positions[0]).x!=expected->at(0).x,"Original native float inverse/forward counterexample genuinely differs");
 auto wrong=surface;auto d=std::make_shared<DeformationState>(*wrong.deformation);auto skeleton=*d->skeleton;(*skeleton.space_matrix)[12]=std::nextafter((*skeleton.space_matrix)[12],1.f);d->skeleton=skeleton;wrong.deformation=d;check(!detail::source_skin_world_positions(wrong),"An independently edited native space matrix keeps generic native authority");
 wrong=surface;auto appearance=std::make_shared<CreatorAppearance>(*wrong.appearance);appearance->asset=std::shared_ptr<const ImportedAsset>(appearance->asset.get(),[](const auto*){});wrong.appearance=appearance;check(refuses([&]{detail::source_skin_world_positions(wrong);}),"A borrowed equal-address source owner cannot authenticate original skin evaluation");
 wrong=surface;for(auto &group:wrong.groups)if(group.source_placement){auto placement=std::make_shared<SourceNodePlacement>(*group.source_placement);++placement->source_instance;group.source_placement=placement;break;}check(refuses([&]{detail::source_skin_world_positions(wrong);}),"Mixed original source-instance ancestry refuses without changing the source");
 wrong=surface;for(auto &group:wrong.groups)if(group.source_placement){group.source_placement.reset();break;}check(refuses([&]{detail::source_skin_world_positions(wrong);}),"A missing original source ancestor cannot borrow the remaining mesh-node identity");
 wrong=surface;d=std::make_shared<DeformationState>(*wrong.deformation);skeleton=*d->skeleton;size_t changed=0;while(changed<skeleton.bones.size()&&skeleton.bones[changed].parent<0)++changed;check(changed<skeleton.bones.size(),"Actual fixture contains a real parented source joint");skeleton.bones[changed].parent=-1;d->skeleton=skeleton;wrong.deformation=d;check(refuses([&]{detail::source_skin_world_positions(wrong);}),"Unwitnessed current source-joint reparent cannot claim original graph authority");
 check(refuses([&]{detail::source_skin_world_positions(surface,{},1);})&&zero.encode()==exact,"Prospective tiny source-world budget refusal preserves complete native source state");
 size_t polls=0;for(int repeat=0;repeat<2;++repeat){size_t count=0;auto positions=detail::source_skin_world_positions(surface,[&]{++count;return false;});check(positions&&same(*positions,*expected)&&(!repeat||count==polls),"Repeated complete source-world computation has stable actual cancellation boundaries");polls=count;}
 for(size_t stop=1;stop<=polls;++stop){size_t count=0;check(refuses([&]{detail::source_skin_world_positions(surface,[&]{return ++count>=stop;});})&&count>=stop&&zero.encode()==exact,"Actual source-world cancellation leaves exact owning Scene unchanged");}
 size_t count=0;for(int repeat=0;repeat<2;++repeat){allocations=0;boundary=SIZE_MAX;injecting=true;std::optional<std::vector<Vec3>> result;try{result=detail::source_skin_world_positions(surface);}catch(...){injecting=false;throw;}injecting=false;check(result&&same(*result,*expected)&&(!repeat||count==allocations),"Repeated actual source-world allocation count is stable");count=allocations;}
 std::cout<<"SOURCE_WORLD measured_cancellation_boundaries="<<polls<<" measured_allocation_prefixes="<<count<<" exhaustive="<<full_faults<<std::endl;
 if(full_faults)for(size_t stop=1;stop<=count;++stop){allocations=0;boundary=stop;denied=false;bool caught=false;injecting=true;try{detail::source_skin_world_positions(surface);}catch(const std::bad_alloc&){caught=true;}injecting=false;check(caught&&denied&&zero.encode()==exact,"Persistent actual source-world C++ allocation failure leaves native authority exact");auto result=detail::source_skin_world_positions(surface);check(result&&same(*result,*expected),"Source-world retry after persistent allocation failure restores exact positions");}
 std::cout<<"SOURCE_WORLD cancellation_boundaries="<<polls<<" allocation_prefixes_executed="<<(full_faults?count:0)<<std::endl;
 const auto large_path=std::filesystem::path(argv[2]).parent_path()/(std::filesystem::path(argv[2]).stem().string()+"-unused.vrm");unused_source(std::filesystem::path(argv[1]).parent_path()/"source.vrm",large_path);
 ImportAuthoringOptions options;options.paint_resolution=64;options.prepare_texture_atlas=true;auto imported=prepare_authoring_import(Scene{},large_path,options);imported.scene.select(imported.objects.front());const auto large_exact=imported.scene.encode();const auto large=active_surface(imported.scene);
 check(large.appearance->asset->nodes.size()>=6000&&detail::source_skin_world_positions(large).has_value(),"Actual native import retains all6000 unused source nodes and default source-world admission");
 check(refuses([&]{detail::source_skin_world_positions(large,{},256u*1024u);})&&imported.scene.encode()==large_exact,"Prospective low budget charges the complete retained source graph before metadata allocation");
 Document doc;doc.replace(zero.snapshot());ready(doc);const auto mesh=doc.frame().surface;for(size_t corner=0;corner<surface.mesh.indices.size();++corner){const auto p=mesh->mesh.positions.at(mesh->mesh.indices.at(corner));const auto q=expected->at(surface.mesh.indices[corner]);check(p.x==q.x&&p.y==q.y&&p.z==q.z,"Actual packed worker corners use shared authenticated source-world evaluation");}
 const auto v0=mesh->mesh.positions.at(mesh->mesh.indices.at(0)),v1=mesh->mesh.positions.at(mesh->mesh.indices.at(1)),v2=mesh->mesh.positions.at(mesh->mesh.indices.at(2));const auto point=(v0+v1+v2)/3;const auto normal=normalized(cross(v1-v0,v2-v0));auto hit=mesh->picking->raycast(point+normal,normal*-1,2);check(hit&&length(hit->position-point)<1e-6f,"Actual immutable material query sees the same source-world triangle");
 PaintSettings ink;ink.color={.2f,.8f,.3f,.8f};ink.front_only=false;doc.paint_settings(ink);NativeStrokeSample sample;sample.origin=point+normal;sample.direction=normal*-1;sample.brush.radius=.04f;sample.brush.strength=.7f;sample.brush.normal=normal;
 check(doc.begin_native_stroke()&&doc.sample_native_stroke(sample),"Actual native ray input starts on the authenticated visible source skin");doc.end_native_stroke();const std::filesystem::path path=argv[2];doc.save_async(path).get();check(doc.wait_native_stroke(),"Actual native source-world paint worker retires");ready(doc);const auto painted=doc.copy_scene();check(painted.encode()!=exact&&Scene::load(path).encode()==painted.encode(),"Accepted native paint and immediate ordered Save/restart preserve the actual source endpoint");check(painted.active().appearance==zero.active().appearance&&painted.active().deformation==zero.active().deformation&&painted.active().material_bindings==zero.active().material_bindings,"Native source-world painting preserves exact original skin/morph/material/appearance owners");check(doc.undo()&&doc.copy_scene().encode()==exact&&doc.redo()&&doc.copy_scene().encode()==painted.encode(),"Accepted native source-world paint has one exact Undo/Redo boundary");ready(doc);check(doc.begin_native_stroke()&&doc.sample_native_stroke(sample),"Native source-world paint retries after history navigation");doc.end_native_stroke(false);check(doc.wait_native_stroke(),"Cancelled native source-world worker drains");ready(doc);check(doc.copy_scene().encode()==painted.encode(),"Actual Cancel restores the complete accepted source owner and paint state");
 check(baseline.encode()==original&&zero.encode()==exact,"All source-world controls preserve original complete Scene and actual raw source bytes");std::cout<<"PASS "<<checks<<" source-world ownership/native checks\n";return 0;
}catch(const std::exception&e){injecting=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
