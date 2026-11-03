// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdlib>
#include <new>
namespace allocation { thread_local long remaining=-1; thread_local bool count=false;thread_local size_t calls=0; }
void *operator new(size_t n){if(allocation::count)++allocation::calls;if(allocation::remaining>=0&&allocation::remaining--==0){allocation::remaining=0;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
#include "key-mirror-relative.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;using namespace dome::detail;
namespace {
size_t checks=0;void check(bool value,const char*message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<std::endl;}
bool refuses(auto f,std::string_view message){try{f();return false;}catch(const std::exception&e){std::cout<<"REFUSAL "<<e.what()<<std::endl;return std::string_view(e.what()).find(message)!=std::string_view::npos;}}
bool near(Vec3 a,Vec3 b){return length(a-b)<1e-7f;}
Mesh mesh(){Mesh m;m.positions={{-1,0,0},{-.5f,0,0},{-.5f,1,0},{1,0,0},{.5f,1,0},{.5f,0,0}};m.normals.assign(6,{0,0,1});m.indices={0,1,2,3,4,5};for(auto p:m.positions)m.uv.push_back({(p.x+1)/2,p.y});return m;}
NormalFrames frames(const Mesh&m){NormalFrames f;f.origin_vertex_count=6;f.origin_vertices=m.indices;f.normals={{0,1,0},{0,1,0},{0,1,0},{0,0,1},{0,0,1},{0,0,1}};f.tangents.assign(6,{1,0,0,1});f.targets.push_back({1,std::vector<Vec3>(6,{.1f,.2f,.3f}),std::vector<Vec3>(6,{.04f,.05f,.06f})});return f;}
}
int main()try{
 const auto m=mesh();const auto f=frames(m);const auto original=f.encode();const auto map=reflected_key_correspondence(m,1e-6f);const std::vector<uint32_t> expected={3,5,4,0,2,1};check(map.corners==expected&&map.vertices==expected,"Strict logical corner bijection matches independently enumerated reflected triangles");
 const auto zero_plane=reflected_key_correspondence(m,KeyMirrorPlane{},1e-6f);check(zero_plane.corners==map.corners&&zero_plane.vertices==map.vertices&&zero_plane.maximum_rest_error==map.maximum_rest_error,"Historical zero-plane overload keeps exact correspondence and error");
 auto shifted=m;for(auto&p:shifted.positions)p.x+=.125f;check(reflected_key_correspondence(shifted,KeyMirrorPlane{.125},1e-6f).corners==expected&&refuses([&]{reflected_key_correspondence(shifted,1e-6f);},"triangle refinement"),"Explicit nonzero local plane succeeds without silently shifting the default operation");
 check(refuses([&]{reflected_key_role_signs(f,m.uv,map);},"base normal parity"),"Original exact artist reflection refuses actual asymmetric base directions");
 const auto result=relative_mirror_attributes(m,f,f.targets[0],2,map,m.uv);
 for(size_t c=0;c<6;++c){const Vec3 n=c<3?Vec3{-.1f,.3f,-.2f}:Vec3{-.1f,-.3f,.2f};const Vec3 t=c<3?Vec3{.04f,-.06f,.05f}:Vec3{.04f,.06f,-.05f};check(near(result.target.normals[c],n)&&near(result.target.tangents[c],t),"Independent signed-axis artist bases derive actual converted N/T deltas");}
 check(result.target.id==2&&f.encode()==original&&result.report.maximum_normal_base_difference>1&&result.report.maximum_tangent_base_difference==0,"Conversion declares base mismatch, new target identity, and unchanged complete artist source");
 auto zero=f;for(auto&v:zero.targets[0].normals)v={};for(auto&v:zero.targets[0].tangents)v={};const auto z=relative_mirror_attributes(m,zero,zero.targets[0],2,map,m.uv);for(size_t c=0;c<6;++c)check(near(z.target.normals[c],{})&&near(z.target.tangents[c],{}),"Zero authored deltas stay zero under explicit base-relative interpretation");
 for(float weight:{-.3f,.35f,1.f}){auto scaled=f;for(auto&v:scaled.targets[0].normals)v=v*weight;for(auto&v:scaled.targets[0].tangents)v=v*weight;const auto r=relative_mirror_attributes(m,scaled,scaled.targets[0],2,map,m.uv);bool linear=true;for(size_t c=0;c<6;++c)linear&=near(r.target.normals[c],result.target.normals[c]*weight)&&near(r.target.tangents[c],result.target.tangents[c]*weight);check(linear,"Actual conversion is linear over independently authored morph weights");}
 auto wrong_uv=m.uv;for(auto&uv:wrong_uv)std::swap(uv[0],uv[1]);const auto swapped=relative_mirror_attributes(m,f,f.targets[0],2,map,wrong_uv);check(!near(swapped.target.normals[0],result.target.normals[0])&&!near(swapped.target.tangents[0],result.target.tangents[0]),"Different actual tangent chart is sensitive and never treated as the original role basis");
 auto wrong_w=f;wrong_w.tangents[0][3]=-1;check(refuses([&]{relative_mirror_attributes(m,wrong_w,wrong_w.targets[0],2,map,m.uv);},"handedness"),"Conversion cannot invent missing tangent handedness morphs");
 const auto copied=f.targets[0];check(refuses([&]{relative_mirror_attributes(m,f,copied,2,map,m.uv);},"actual named role target"),"An unrelated equal-valued target is not silently borrowed as a role owner");
 auto badmap=map;badmap.corners[0]=999;check(refuses([&]{relative_mirror_attributes(m,f,f.targets[0],2,badmap,m.uv);},"corner involution"),"Malformed detached corner proof fails before indexing another field");
 auto n_only=f;n_only.tangents.clear();n_only.targets[0].tangents.clear();const auto n=relative_mirror_attributes(m,n_only,n_only.targets[0],2,map,{});check(n.target.tangents.empty()&&near(n.target.normals[0],{-.1f,.3f,-.2f}),"Genuine N-only role uses independent shortest rotation without a manufactured tangent");
 KeyMirrorLimits tiny;tiny.max_bytes=143;check(refuses([&]{relative_mirror_attributes(m,f,f.targets[0],2,map,m.uv,tiny);},"preflight"),"Output-byte bound refuses before target allocation");tiny={};tiny.max_work=23;check(refuses([&]{relative_mirror_attributes(m,f,f.targets[0],2,map,m.uv,tiny);},"preflight"),"Declared work bound refuses before target allocation");
 size_t polls=0;KeyMirrorLimits limits;limits.cancelled=[&]{++polls;return false;};relative_mirror_attributes(m,f,f.targets[0],2,map,m.uv,limits);for(size_t cut=1;cut<=polls;++cut){size_t p=0;limits.cancelled=[&]{return ++p>=cut;};check(refuses([&]{relative_mirror_attributes(m,f,f.targets[0],2,map,m.uv,limits);},"cancel")&&f.encode()==original,"Every actual cancellation poll preserves immutable original role data");}
 allocation::calls=0;allocation::count=true;relative_mirror_attributes(m,f,f.targets[0],2,map,m.uv);allocation::count=false;const auto calls=allocation::calls;for(size_t cut=0;cut<calls;++cut){allocation::remaining=long(cut);bool failed=false;try{relative_mirror_attributes(m,f,f.targets[0],2,map,m.uv);}catch(const std::bad_alloc&){failed=true;}allocation::remaining=-1;check(failed&&f.encode()==original,"Each persistent allocation prefix rolls back detached output without mutating the source");}
 std::cout<<"PASS "<<checks<<" explicit frame-relative checks; allocations "<<calls<<" cancellation "<<polls<<std::endl;return 0;
}catch(const std::exception&e){allocation::remaining=-1;allocation::count=false;std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
