// SPDX-License-Identifier: GPL-3.0-or-later
#include "sparse-resample-backend.hpp"
#include "dome/sculpt-resolution.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <unordered_map>
using namespace dome;
using namespace dome::sparse_resample;
namespace {
size_t checks=0,dispatches=0,queries_sent=0,fail_dispatch=0;
void check(bool b,const char *s){++checks;if(!b)throw std::runtime_error(s);}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
using Field=std::unordered_map<Int3,double,IntHash>;
uint64_t reference(const Field &field,Point q,float source,float target,double background){
 const double p[]={double(q.x)*target/source,double(q.y)*target/source,double(q.z)*target/source};
 Int3 lo{int(std::floor(p[0])),int(std::floor(p[1])),int(std::floor(p[2]))};
 const double f[]={p[0]-lo.x,p[1]-lo.y,p[2]-lo.z};double value=background;
 for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x){auto it=field.find(lo+Int3{x,y,z});if(it!=field.end())value+=(it->second-background)*(x?f[0]:1-f[0])*(y?f[1]:1-f[1])*(z?f[2]:1-f[2]);}
 return std::bit_cast<uint64_t>(value);
}
nlohmann::json ordered(const Volume &v){auto j=v.encode();std::sort(j["samples"].begin(),j["samples"].end());for(auto &l:j["sculpt_layers"])std::sort(l["delta"].begin(),l["delta"].end());if(j.contains("sculpt_mask"))std::sort(j["sculpt_mask"]["values"].begin(),j["sculpt_mask"]["values"].end());return j;}
void make_fixture(const char *path){
 check(compute_backend()=="CPU","Generate cross-backend input using native CPU only");
 Volume v(.04f);Brush b;b.radius=.177f;b.from=b.to={.013f,.017f,-.011f};b.blend=0;
 v.begin_stroke();v.stamp(b);v.end_stroke();
 auto id=v.add_layer("Hidden 日本語");b.radius=.09f;b.from=b.to={-.14f,.01f,.03f};v.begin_stroke();v.stamp(b);v.end_stroke();v.layer_properties(id,"Hidden 日本語",false,true,.37f);
 v.fill_mask(.37f);auto s=v.sculpt_settings();s.mask_edit=true;s.mask_tool=Volume::MaskTool::Subtract;s.mask_strength=.6f;v.sculpt_settings(s);b.radius=.12f;v.begin_stroke();v.stamp(b);v.end_stroke();
 std::ofstream(path)<<ordered(v).dump();
}
void volume(const char *input,const char *output){
 std::ifstream file(input);nlohmann::json j;file>>j;const auto source=Volume::decode(j);const auto before=ordered(source);const auto owner=source.geometry_owner();
 SculptResolutionOptions options;options.voxel_size=source.voxel_size();
 auto noop=resample_sculpt(source,options);check(!noop.replacement&&!dispatches,"Same-voxel no-op does not dispatch");
 options.voxel_size=.02f;const auto start=std::chrono::steady_clock::now();auto result=resample_sculpt(source,options);
 const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 check(result.replacement.has_value()&&result.report.source.closed_manifold()&&result.report.target.closed_manifold(),"Actual resampling passes unchanged quality and topology checks");
 check(ordered(source)==before&&source.geometry_owner()==owner,"Actual resampling preserves exact input and owner");
 if(compute_backend()=="CPU")check(dispatches==0,"CPU path retains original map interpolation");else check(dispatches>=3&&queries_sent>0,"Every nonempty layer/mask uses the selected backend");
 const auto good=ordered(*result.replacement);std::ofstream(output)<<good.dump();
 Scene scene;scene.active().volume=std::make_shared<Volume>(source.geometry_snapshot());const auto retained=scene.active().volume;
 if(compute_backend()!="CPU"){
   fail_dispatch=dispatches+2;
   check(refuses([&]{resample_sculpt_object(scene,scene.active().id,options);}),"Later selected-backend failure is propagated");
   check(scene.active().volume==retained&&ordered(*retained)==before,"Later backend failure cannot publish partial layers or mask");fail_dispatch=0;
 }
 options.cancelled=[] {return true;};check(refuses([&]{resample_sculpt_object(scene,scene.active().id,options);})&&scene.active().volume==retained,"Cancellation cannot publish a candidate");
 std::cout<<"VOLUME backend="<<compute_backend()<<" total_ms="<<elapsed<<" dispatches="<<dispatches<<" queries="<<queries_sent<<" layer_samples="<<result.report.layer_samples<<" mask_samples="<<result.report.mask_samples<<" work="<<result.report.work<<'\n';
}
void session(){
 Field field;std::vector<Entry> entries;
 for(int z=-5;z<=5;++z)for(int y=-5;y<=5;++y)for(int x=-5;x<=5;++x){double value=double(x*31+y*7-z*11)/137.;field.emplace(Int3{x,y,z},value);entries.push_back({{x,y,z},value});}
 std::vector<Point> queries;for(int z=-8;z<=8;++z)for(int y=-8;y<=8;++y)for(int x=-8;x<=8;++x)queries.push_back({x,y,z});
 Grid grid(entries);const auto p=parameters(.037f,.023f,.37);SparseFieldSampler sampler(grid,p,257);
 for(int iteration=0;iteration<3;++iteration){auto actual=sampler.sample(queries);check(actual.size()==queries.size(),"Session returns complete output");for(size_t i=0;i<actual.size();++i)check(actual[i]==reference(field,queries[i],.037f,.023f,.37),"Native backend result differs from independent map formula");std::reverse(queries.begin(),queries.end());}
 check(sampler.sample({}).empty(),"Empty query retains valid session");
 auto invalid=queries;invalid.front().x=INT32_MAX;check(refuses([&]{sampler.sample(invalid);}),"Invalid scaled coordinate refused");check(sampler.sample(queries).size()==queries.size(),"Admission refusal does not poison an unused queue");
 size_t cancel_calls=0;const size_t validation_calls=(queries.size()+255)/256+1;
 check(refuses([&]{sampler.sample(queries,[&]{return ++cancel_calls>validation_calls+2;});}),"Cancellation after completed query chunk refuses full result");
 if(compute_backend()!="CPU")check(refuses([&]{sampler.sample(queries);}),"Cancelled active GPU session is not reused");
 SparseFieldSampler retry(grid,p,65536);check(retry.sample(queries).size()==queries.size(),"Fresh session succeeds after cancelled session");
 check(refuses([&]{SparseFieldSampler bad(grid,p,0);})&&refuses([&]{SparseFieldSampler bad(grid,p,65537);}),"Chunk capacity is bounded before device allocation");
}
}
extern std::vector<uint64_t> real_sample(SparseFieldSampler*,std::span<const Point>,const std::function<bool()>&) asm("__real__ZN4dome18SparseFieldSampler6sampleESt4spanIKNS_15sparse_resample5PointELm18446744073709551615EERKSt8functionIFbvEE");
std::vector<uint64_t> wrapped_sample(SparseFieldSampler*,std::span<const Point>,const std::function<bool()>&) asm("__wrap__ZN4dome18SparseFieldSampler6sampleESt4spanIKNS_15sparse_resample5PointELm18446744073709551615EERKSt8functionIFbvEE");
std::vector<uint64_t> wrapped_sample(SparseFieldSampler *sampler,std::span<const Point> q,const std::function<bool()> &cancelled){++dispatches;queries_sent+=q.size();auto result=real_sample(sampler,q,cancelled);if(fail_dispatch==dispatches)throw std::runtime_error("Injected later selected-backend output failure");return result;}
int main(int argc,char **argv){try{
 if(argc==3&&std::string_view(argv[1])=="--make")make_fixture(argv[2]);
 else if(argc==4&&std::string_view(argv[1])=="--volume")volume(argv[2],argv[3]);
 else if(argc==2&&std::string_view(argv[1])=="--missing"){Grid grid(std::vector<Entry>{{{0,0,0},1}});check(refuses([&]{SparseFieldSampler sampler(grid,parameters(.04f,.02f,0));sampler.sample(std::vector<Point>{{0,0,0}});}),"Missing GPU fails explicitly");}
 else session();
 std::cout<<"PASS "<<checks<<" native resample backend checks on "<<compute_backend()<<'\n';return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
