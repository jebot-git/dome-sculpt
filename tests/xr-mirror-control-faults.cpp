// SPDX-License-Identifier: GPL-3.0-or-later
#define main mirror_controller_workflow_main
#include "xr-mirror-controls.cpp"
#undef main
#include <cstdlib>
#include <new>
namespace {thread_local bool inject=false,denied=false;thread_local size_t allocations=0,boundary=SIZE_MAX;}
[[gnu::noinline]] void *operator new(size_t n){if(inject&&++allocations>=boundary){denied=true;throw std::bad_alloc();}if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);}void operator delete[](void *p)noexcept{std::free(p);}void operator delete(void *p,size_t)noexcept{std::free(p);}void operator delete[](void *p,size_t)noexcept{std::free(p);}
namespace dome {struct DocumentTransactionTestAccess{
 static auto tokens(const Document &d){std::lock_guard lock(d.model_mutex_);return std::tuple{d.content_,d.next_content_,d.saved_content_,d.undo_.size(),d.redo_.size(),d.state_.generation,d.state_.revision,d.state_.edit_authority,d.asset_work_.load(),d.native_source_scene_,d.input_.owns_control(VrParameter::ShapeMirror),d.input_.menu_page()};}
 static void confirm(Document &d){std::lock_guard lock(d.model_mutex_);d.input_.complete_control();d.control_event_locked({VrControlEventType::Confirm,VrParameter::ShapeMirror});}
};}
namespace {
void prepare(Document &d){d.replace(fixture());ready(d);d.select_shape_target(0);auto o=d.copy_scene().active();d.object_properties(o.id,"Retained A",o.visible,o.locked,o.transform,o.material);d.object_properties(o.id,"Retained B",o.visible,o.locked,o.transform,o.material);check(d.undo()&&d.wait_mesh(),"Admission fixture owns retained Undo and Redo");Hand hand(d);hand.shapes();hand.sector(3);hand.row(1,detail::mirror_plane_slider(.07));check(d.frame().state->controls.parameter==VrParameter::ShapeMirror,"Real controller prepares immutable Mirror draft before native confirmation faults");}
bool same(const VrControlState&a,const VrControlState&b){return a.parameter==b.parameter&&a.title==b.title&&a.value==b.value&&a.components==b.components&&a.selected==b.selected&&a.minimum==b.minimum&&a.maximum==b.maximum&&a.logarithmic==b.logarithmic;}
}
int main()try{
 Document measured;prepare(measured);boundary=SIZE_MAX;allocations=0;inject=true;try{DocumentTransactionTestAccess::confirm(measured);}catch(...){inject=false;throw;}inject=false;const auto count=allocations;ready(measured);check(count>10,"Actual native Mirror admission has measured allocation boundaries");
 for(size_t at=1;at<=count;++at){Document d;prepare(d);const auto original=d.copy_scene().encode();const auto before=DocumentTransactionTestAccess::tokens(d);const auto controls=d.frame().state->controls;const auto publication=d.frame().state;boundary=at;allocations=0;denied=false;bool caught=false;inject=true;try{DocumentTransactionTestAccess::confirm(d);}catch(const std::bad_alloc&){caught=true;}inject=false;
  if(!caught||!denied||d.copy_scene().encode()!=original||DocumentTransactionTestAccess::tokens(d)!=before||!same(d.frame().state->controls,controls)||d.frame().state!=publication){std::cerr<<"FAILED_PREFIX "<<at<<" captured="<<caught<<" denied="<<denied<<"\n";throw std::runtime_error("Persistent admission failure lost exact Mirror draft, source, history or control ownership");}
  DocumentTransactionTestAccess::confirm(d);ready(d);check(d.undo()&&d.copy_scene().encode()==original,"Every failed Mirror admission retries to one exact undoable native operation");
 }
 std::cout<<"PASS persistent_xr_mirror_admission_prefixes="<<count<<'\n';return 0;
}catch(const std::exception&e){inject=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
