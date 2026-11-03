// SPDX-License-Identifier: GPL-3.0-or-later
#include "export-corner-coordinates.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
using namespace dome;
namespace fault {bool armed=false,hit=false;size_t calls=0,cut=0;}
void *operator new(size_t n){if(fault::armed&&++fault::calls>=fault::cut&&fault::cut){fault::hit=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);}void operator delete[](void *p)noexcept{std::free(p);}
void operator delete(void *p,size_t)noexcept{std::free(p);}void operator delete[](void *p,size_t)noexcept{std::free(p);}
void *operator new(size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void *operator new[](size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void operator delete(void *p,const std::nothrow_t&)noexcept{std::free(p);}void operator delete[](void *p,const std::nothrow_t&)noexcept{std::free(p);}
namespace {unsigned checks=0;void check(bool v,const char *why){if(!v)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<'\n';}bool refuses(auto fn){try{fn();return false;}catch(const std::exception&){return true;}}
bool same(const detail::CoordinateFrameLayout&a,const detail::CoordinateFrameLayout&b){return a.frames.vertices==b.frames.vertices&&a.frames.corners==b.frames.corners&&a.frames.indices==b.frames.indices&&a.coordinates==b.coordinates;}}
int main()try{
 Mesh mesh;mesh.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};mesh.normals.assign(4,{0,0,1});mesh.uv={{{.1f,.1f}},{{.9f,.1f}},{{.9f,.9f}},{{.1f,.9f}}};mesh.indices={0,1,2,0,2,3};
 auto primary=std::make_shared<NormalFrames>();primary->origin_vertex_count=4;primary->origin_vertices=mesh.indices;primary->normals.assign(6,{0,0,1});primary->tangents.assign(6,{1,0,0,1});primary->targets.push_back({1,std::vector<Vec3>(6,Vec3{1,0,0}),std::vector<Vec3>(6,Vec3{0,1,0})});
 NormalFrameSet set;set.pbr=set.mtoon=primary;set.targets={{1,0,std::nullopt,{}}};set.current_targets={1};set.next_target=2;
 auto base=detail::normal_frame_layout(mesh,set);std::vector<std::array<float,2>> coordinates;for(auto v:mesh.indices)coordinates.push_back(mesh.uv[v]);
 auto identity=detail::split_frame_corner_coordinates(mesh,base,coordinates);
 check(identity.frames.indices==base.indices&&identity.frames.vertices==base.vertices,"Identical corner coordinates preserve the complete existing frame split and order");
 auto seam=coordinates;seam[3]={.25f,.75f};auto result=detail::split_frame_corner_coordinates(mesh,base,seam);
 check(result.frames.vertices.size()==5&&result.frames.indices[0]!=result.frames.indices[3]&&result.frames.indices[2]==result.frames.indices[4],"Independent paint UV seam splits only its contradictory ordered corners without averaging");
 bool exact=true;for(size_t c=0;c<mesh.indices.size();++c){const auto v=result.frames.indices[c];exact&=result.frames.vertices[v]==mesh.indices[c]&&result.coordinates[v]==seam[c];}
 check(exact,"Every triangle corner maps to its exact REST vertex and independent paint coordinate");
 const std::array<std::array<float,2>,4> uv1{{{.2f,.3f},{.4f,.3f},{.4f,.6f},{.2f,.6f}}};
 for(size_t c=0;c<mesh.indices.size();++c){const auto v=result.frames.indices[c],rest=result.frames.vertices[v];check(mesh.uv[rest]==coordinates[c]&&uv1[rest]==uv1[mesh.indices[c]],"Explicit REST remap preserves authored UV0 and separate original UV1 through paint seams");}
 auto secondary=std::make_shared<NormalFrames>(*primary);secondary->targets[0].normals[3].y=.2f;set.mtoon=secondary;const auto full=detail::normal_frame_layout(mesh,set);auto both=seam;both[4]={.3f,.7f};const auto split=detail::split_frame_corner_coordinates(mesh,full,both);
 check(full.vertices.size()==5&&split.frames.vertices.size()==6&&split.frames.indices[0]!=split.frames.indices[3]&&split.frames.indices[2]!=split.frames.indices[4],"Secondary role target-frame identity and independent paint seams both participate without collapsing either");
 DeformationState deformation(mesh);deformation.skeleton=Skeleton{{{"root",-1,{},1}}};deformation.skin=SkinWeights{};deformation.skin->vertices.assign(4,{{0,1}});deformation.pose={{{},{},{2,1,.5f}}};deformation.morph_weights={.35f};
 const auto posed=detail::normal_frame_preview(set,split.frames,&deformation);bool analytic=true;
 for(size_t c=0;c<mesh.indices.size();++c){const auto v=split.frames.indices[c];analytic&=length(posed.normals[0][v]-Vec3{.175f,0,2})<1e-6f;analytic&=length(posed.normals[1][v]-Vec3{.175f,c==3?.07f:0.f,2})<1e-6f;const auto t=posed.tangents[0][v];analytic&=length(Vec3{t[0],t[1],t[2]}-Vec3{2,.35f,0})<1e-6f;}
 check(analytic,"Independent anisotropic joint/morph formulas verify complete PBR and distinct MToon N/T target transport after coordinate splitting");
 auto invalid=base;invalid.vertices[0]=3;check(refuses([&]{detail::split_frame_corner_coordinates(mesh,invalid,seam);}),"Forged representative REST correspondence refuses");
 invalid=base;invalid.indices[0]=99;check(refuses([&]{detail::split_frame_corner_coordinates(mesh,invalid,seam);}),"Out-of-range complete-frame index refuses");
 auto bad=seam;bad.pop_back();check(refuses([&]{detail::split_frame_corner_coordinates(mesh,base,bad);}),"Partial corner coordinates cannot be guessed from vertex count");
 bad=seam;bad[0][1]=std::numeric_limits<float>::quiet_NaN();check(refuses([&]{detail::split_frame_corner_coordinates(mesh,base,bad);}),"Nonfinite sampling coordinates refuse before layout publication");
 check(refuses([&]{detail::split_frame_corner_coordinates(mesh,base,seam,[]{return true;});}),"Immediate cancellation publishes no detached coordinate layout");
 unsigned polls=0;check(refuses([&]{detail::split_frame_corner_coordinates(mesh,base,seam,[&]{return ++polls==2;});})&&polls==2,"Final cancellation refuses even a completely built detached result");
 check(refuses([&]{detail::split_frame_corner_coordinates(mesh,base,seam,{},3);}),"Explicit work quota is enforced independently of data validity");
 check(refuses([&]{detail::split_frame_corner_coordinates(mesh,base,seam,{},128*1024*1024,4);}),"Retained-frame vertex quota cannot be bypassed by additional paint seams");
 fault::armed=true;fault::cut=0;fault::calls=0;auto measured=detail::split_frame_corner_coordinates(mesh,base,seam);fault::armed=false;const auto count=fault::calls;size_t denied=0;
 for(size_t cut=1;cut<=count;++cut){fault::calls=0;fault::cut=cut;fault::hit=false;fault::armed=true;bool failed=false;try{detail::split_frame_corner_coordinates(mesh,base,seam);}catch(const std::bad_alloc&){failed=true;}fault::armed=false;if(fault::hit){if(!failed)throw std::runtime_error("Persistent coordinate allocation denial was hidden");++denied;}check(same(measured,detail::split_frame_corner_coordinates(mesh,base,seam)),"Persistent allocation failure leaves the input exact and permits deterministic retry");}
 check(denied==count&&count>5,"Every measured detached coordinate allocation boundary is persistently denied and recovered");
 std::cout<<"PASS "<<checks<<" coordinate export layout checks, "<<denied<<" allocation boundaries\n";return 0;
}catch(const std::exception&e){fault::armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
