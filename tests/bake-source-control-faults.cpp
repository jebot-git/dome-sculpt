// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_bake_controls_main
#include "bake-controls.cpp"
#undef main
#include <cstdlib>
#include <new>
namespace {thread_local bool inject=false,denied=false;thread_local size_t calls=0,boundary=SIZE_MAX;}
[[gnu::noinline]] void *operator new(size_t n){if(inject&&++calls>=boundary){denied=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);}void operator delete[](void *p)noexcept{std::free(p);}void operator delete(void *p,size_t)noexcept{std::free(p);}void operator delete[](void *p,size_t)noexcept{std::free(p);}
namespace dome {struct DocumentTransactionTestAccess {
 static auto tokens(const Document &d){std::lock_guard lock(d.model_mutex_);return std::tuple{d.content_,d.next_content_,d.saved_content_,d.undo_.size(),d.redo_.size(),d.state_.generation,d.state_.revision,d.state_.edit_authority,d.asset_work_.load(),d.native_source_scene_,d.input_.owns_control(VrParameter::BakeSource),d.input_.menu_page(),d.state_.bake_source,d.state_.bake_settings.cage_object,bool(d.control_before_)};}
 static void confirm(Document &d){std::lock_guard lock(d.model_mutex_);d.input_.complete_control();d.control_event_locked({VrControlEventType::Confirm,VrParameter::BakeSource});}
};}
namespace {
void prepared_source(Document &d){d.replace(fixture());check(d.wait_mesh(),"Native source/cage fixture publishes");auto o=d.copy_scene().active();d.object_properties(o.id,"Keep A",o.visible,o.locked,o.transform,o.material);d.object_properties(o.id,"Keep B",o.visible,o.locked,o.transform,o.material);check(d.undo()&&d.wait_mesh(),"Source/cage admission retains real Undo and Redo");Driver h(d);h.open();h.sector(0);h.value(1,0);h.value(.5f,1);check(d.frame().state->controls.parameter==VrParameter::BakeSource&&d.frame().state->controls.components==2,"Actual controller stages both changed source and cage before confirmation faults");}
bool same_control(const VrControlState&a,const VrControlState&b){return a.parameter==b.parameter&&a.title==b.title&&a.value==b.value&&a.components==b.components&&a.selected==b.selected&&a.minimum==b.minimum&&a.maximum==b.maximum&&a.logarithmic==b.logarithmic;}
}
int main()try{
 Document measured;prepared_source(measured);calls=0;boundary=SIZE_MAX;inject=true;try{DocumentTransactionTestAccess::confirm(measured);}catch(...){inject=false;throw;}inject=false;const auto count=calls;check(count>0&&count<4096,"Actual source/cage confirmation allocation boundaries measured");
 for(size_t at=1;at<=count;++at){Document d;prepared_source(d);auto original=d.copy_scene().encode();auto before=DocumentTransactionTestAccess::tokens(d);auto control=d.frame().state->controls;auto publication=d.frame().state;calls=0;boundary=at;denied=false;bool caught=false;inject=true;try{DocumentTransactionTestAccess::confirm(d);}catch(const std::bad_alloc&){caught=true;}inject=false;
  if(!caught||!denied||d.copy_scene().encode()!=original||DocumentTransactionTestAccess::tokens(d)!=before||!same_control(d.frame().state->controls,control)||d.frame().state!=publication){std::cerr<<"FAILED_PREFIX "<<at<<" caught="<<caught<<" denied="<<denied<<'\n';throw std::runtime_error("Source/cage confirmation lost draft input ownership, configuration or exact scene/history");}
  DocumentTransactionTestAccess::confirm(d);check(d.frame().state->bake_source==3&&d.frame().state->bake_settings.cage_object==2&&d.frame().state->controls.parameter==VrParameter::None&&d.copy_scene().encode()==original,"Same native draft retries to the exact chosen source/cage with no scene/history mutation");
 }
 std::cout<<"PASS persistent_source_cage_confirmation_prefixes="<<count<<'\n';return 0;
}catch(const std::exception &e){inject=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
