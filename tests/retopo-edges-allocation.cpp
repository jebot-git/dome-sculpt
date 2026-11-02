// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_retopo_edges_test
#include "retopo-edges.cpp"
#undef main
#include <cstdlib>
#include <new>
namespace allocation {thread_local bool armed=false,hit=false;thread_local size_t calls=0,fail=0;}
void *operator new(size_t n){if(allocation::armed&&++allocation::calls>=allocation::fail&&allocation::fail){allocation::hit=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);}
void operator delete[](void *p)noexcept{std::free(p);}
void operator delete(void *p,size_t)noexcept{std::free(p);}
void operator delete[](void *p,size_t)noexcept{std::free(p);}
int main()try{
 auto endpoints=patches();auto source=reference(endpoints.mesh);size_t total=0;
 for(unsigned mode=0;mode<6;++mode){
  auto setup=[&]{RetopoWorkspace w(source);
   if(mode==2){for(auto p:std::array<Vec3,4>{{{-.1f,-.1f,0},{-.1f,.1f,0},{.1f,.1f,0},{.1f,-.1f,0}}})point(w,p);}
   else{point(w,{-.3f,-.1f,0});point(w,{-.1f,-.1f,0});}
   w.edit(RetopoAction::Pin);w.edit(RetopoAction::Undo);
   if(mode>=3&&mode<=4){w.edit(RetopoAction::ExtrudeMode);w.edit(RetopoAction::BeginGesture);w.edit(RetopoAction::Sample,{-.2f,-.1f,0});if(mode==4)w.edit(RetopoAction::Sample,{-.2f,-.2f,0});}
   return w;};
  auto run=[&](RetopoMesh &m,RetopoWorkspace &w){
   if(mode==0){m.bridge_edges({endpoints.left[2],endpoints.left[1],endpoints.right[0],endpoints.right[3]});}
   else if(mode==1){auto e=m.edges().at(edge(m,endpoints.left[0],endpoints.left[1]));m.extrude_edge(edge(m,e.a,e.b),{m.vertices().at(e.a).position+Vec3{0,-.1f,0},m.vertices().at(e.b).position+Vec3{0,-.1f,0}});}
   else w.edit(mode==2?RetopoAction::BridgeEdges:mode==3?RetopoAction::Sample:mode==4?RetopoAction::EndGesture:RetopoAction::Redo,{-.2f,-.2f,0});
  };
  auto measured_mesh=patches().mesh;auto measured=setup();allocation::calls=allocation::fail=0;allocation::armed=true;run(measured_mesh,measured);allocation::armed=false;auto count=allocation::calls;
  require(count>0,"Real topology/history allocation boundaries measured");auto expected=mode<2?measured_mesh.encode():measured.view().topology->encode();
  for(size_t at=1;at<=count;++at){
   auto mesh=patches().mesh;auto mesh_before=mesh.encode();auto w=setup();auto before=w.view();allocation::calls=0;allocation::fail=at;allocation::hit=false;bool refused=false;
   allocation::armed=true;try{run(mesh,w);}catch(const std::bad_alloc&){refused=true;}allocation::armed=false;
   require(allocation::hit&&refused,"Every persistent allocation denial propagates");
   require(mesh.encode()==mesh_before&&w.view().topology==before.topology&&w.view().selected==before.selected&&w.view().revision==before.revision&&w.view().extruding==before.extruding,"Failed operation preserves exact geometry, IDs, selection and visible revision");
   // Failed previews and accepted-End keep the gesture cancellable. Cancellation
   // itself must not allocate even under persistent allocator denial.
   if(mode>=3&&mode<=4){allocation::calls=0;allocation::fail=1;allocation::armed=true;w.edit(RetopoAction::CancelGesture);allocation::armed=false;}
   auto baseline=w.view().topology;w.edit(RetopoAction::Redo);require(w.view().topology!=baseline,"Earlier Redo survives rejected edit");w.edit(RetopoAction::Undo);require(w.view().topology==baseline,"Earlier Undo returns exact preexisting endpoint");
   auto retry_mesh=patches().mesh;auto retry=setup();run(retry_mesh,retry);require((mode<2?retry_mesh.encode():retry.view().topology->encode())==expected,"Actual rejected operation retries to identical stable topology");++total;
  }
  std::cout<<"PASS retopo edge operation "<<mode<<": "<<count<<" persistent allocation boundaries\n";
 }
 std::cout<<"PASS "<<total<<" persistent allocation boundaries\n";return 0;
}catch(const std::exception &e){allocation::armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
