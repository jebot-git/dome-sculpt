// SPDX-License-Identifier: GPL-3.0-or-later
#include "baked-normal-export.hpp"
#include "dome/scene-file.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <new>
#include <iostream>
namespace fault {thread_local bool enabled=false,hit=false;thread_local size_t calls=0,at=SIZE_MAX;void visit(){if(enabled&&++calls>=at){hit=true;throw std::bad_alloc();}}}
[[gnu::noinline]] void *operator new(size_t n){fault::visit();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);}void operator delete[](void *p)noexcept{std::free(p);}void operator delete(void *p,size_t)noexcept{std::free(p);}void operator delete[](void *p,size_t)noexcept{std::free(p);}
void *operator new(size_t n,std::align_val_t a){fault::visit();void *p=nullptr;if(posix_memalign(&p,size_t(a),n?n:1))throw std::bad_alloc();return p;}
void *operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}void operator delete(void*p,size_t,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,size_t,std::align_val_t)noexcept{std::free(p);}
using namespace dome;
namespace {
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
nlohmann::json signature(const detail::GeneratedBakedNormalExport &result){
 const auto &f=result.frame;auto vectors=[](const auto &values){auto a=nlohmann::json::array();for(auto v:values)a.push_back({v.x,v.y,v.z});return a;};
 auto normals=nlohmann::json::array(),tangents=nlohmann::json::array();for(const auto &v:f.normal_targets)normals.push_back(vectors(v));for(const auto &v:f.tangent_targets)tangents.push_back(vectors(v));
 return {{"vertices",f.rest_vertices},{"positions",vectors(f.surface.mesh.positions)},{"normals",vectors(f.surface.mesh.normals)},{"uv",f.surface.mesh.uv},{"pins",f.surface.mesh.uv_pins},{"seams",f.surface.mesh.uv_seams},{"indices",f.surface.mesh.indices},{"tangents",f.tangents},{"normal_targets",normals},{"tangent_targets",tangents},{"deformation",f.surface.deformation->encode(f.surface.mesh)},{"recomputed",result.recomputed_normals}};
}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;const auto scene=load_scene_file(argv[1]).scene;const auto before=scene.encode();const auto surfaces=scene.preview_surfaces();check(surfaces.size()==1,"Actual mirrored skin/morph bake supplies one immutable native surface");const auto &input=surfaces.front();
 auto generate=[&]{auto value=detail::baked_normal_export(input,detail::SourceMorphSpace::Native,false);if(!value)throw std::runtime_error("Actual canonical normal surface was not admitted");return *value;};
 fault::calls=0;fault::at=SIZE_MAX;fault::enabled=true;auto positive=generate();fault::enabled=false;const auto count=fault::calls;const auto expected=signature(positive);
 check(count&&count<4096,"Actual new tangent staging allocation boundaries measured");
 for(size_t at=1;at<=count;++at){fault::calls=0;fault::at=at;fault::hit=false;bool caught=false;fault::enabled=true;
  try{generate();}catch(const std::bad_alloc&){caught=true;}fault::enabled=false;
  if(!caught||!fault::hit||scene.encode()!=before)throw std::runtime_error("Persistent tangent allocation refusal changed source or missed its actual boundary");
  if(signature(generate())!=expected)throw std::runtime_error("Tangent retry differs from the complete positive geometry/skin/morph endpoint");
 }
 size_t polls=0;auto full=detail::baked_normal_export(input,detail::SourceMorphSpace::Native,false,[&]{++polls;return false;});check(full&&polls>5,"Actual tangent conversion exposes bounded cancellation checks");
 for(size_t cutoff:{size_t(1),polls/2,polls}){size_t at=0;bool refused=false;try{detail::baked_normal_export(input,detail::SourceMorphSpace::Native,false,[&]{return ++at>=cutoff;});}catch(const std::runtime_error &e){refused=std::string_view(e.what()).find("cancelled")!=std::string_view::npos;}check(refused&&scene.encode()==before,"Early middle and final tangent cancellation preserve the entire native source");}
 auto large=input;large.deformation.reset();const size_t corners=3*(128*1024*1024/192/3+2);large.mesh.indices.resize(corners);for(size_t c=0;c<corners;++c)large.mesh.indices[c]=uint32_t(c%3);
 bool bounded=false;try{detail::baked_normal_export(large,detail::SourceMorphSpace::Native,false);}catch(const std::length_error&e){bounded=std::string_view(e.what()).find("staging")!=std::string_view::npos;}
 check(bounded&&scene.encode()==before&&signature(generate())==expected,"Prospective oversized tangent staging refuses before Mikk allocation and the same source retries");
 std::cout<<"PASS tangent_persistent_cpp_prefixes="<<count<<" cancellation_polls="<<polls<<'\n';return 0;
}catch(const std::exception &e){fault::enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
