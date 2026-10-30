// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_shapekey_kernel_test
#include "shapekey-authoring.cpp"
#undef main
#include <cstdlib>
#include <new>
namespace allocation {
thread_local bool armed=false,hit=false;
thread_local size_t calls=0,failure=0;
}
void *operator new(size_t n){
 if(allocation::armed&&++allocation::calls>=allocation::failure&&allocation::failure){allocation::hit=true;throw std::bad_alloc();}
 if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}
void operator delete[](void*p,size_t)noexcept{std::free(p);}
int main()try{
 auto source=fixture();author_shapekey(source,source.active().id,{});
 source.duplicate(source.active().id);auto before=source.encode();auto wire=nlohmann::json::to_cbor(before);
 ShapeKeyEdit edit;edit.action=ShapeKeyAction::Replace;edit.positions=source.active().painted->mesh().positions;edit.positions[0].z=.1f;
 size_t total=0;
 for(int mode=0;mode<3;++mode){
  auto attempt=source;
  auto run=[&]{if(mode==0)(void)source.encode();else if(mode==1)(void)Scene::decode(before);else author_shapekey(attempt,attempt.active().id,edit);};
  allocation::calls=allocation::failure=0;allocation::armed=true;run();allocation::armed=false;auto measured=allocation::calls;
  check(measured>50,"Scene16 codec and shape edits exercise real allocation boundaries");
  for(size_t boundary=1;boundary<=measured;++boundary){
   attempt=source;allocation::calls=0;allocation::failure=boundary;allocation::hit=false;bool refused=false;
   allocation::armed=true;try{run();}catch(const std::bad_alloc&){refused=true;}allocation::armed=false;
   if(!allocation::hit||!refused)throw std::runtime_error("A measured persistent-denial boundary did not refuse");
   if(nlohmann::json::to_cbor(source.encode())!=wire||nlohmann::json::to_cbor(before)!=wire||
      (mode==2&&attempt.encode()!=before))throw std::runtime_error("Persistent denial changed native shape/source authority");
   ++total;
  }
  auto retry=Scene::decode(before);if(retry.encode()!=before||retry.objects()[0].shape_source!=retry.objects()[1].shape_source)throw std::runtime_error("Retry lost exact source state or shared source identity");
  if(mode==2){author_shapekey(retry,retry.active().id,edit);if(retry.active().deformation->morphs->targets[0].delta[0].z!=.1f)throw std::runtime_error("Failed shape edit cannot be retried");}
  std::cout<<"PASS Scene16 "<<(mode==0?"encode":mode==1?"decode":"shape replacement")<<" "<<measured<<" persistent allocation boundaries\n";
 }
 std::cout<<"PASS "<<total<<" Scene16 persistent-denial boundaries, exact source/keys and retries\n";return 0;
}catch(const std::exception&e){allocation::armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
