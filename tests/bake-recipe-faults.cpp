// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_recipe_fixture
#include "bake-recipes.cpp"
#undef main
#include <cstdlib>
#include <new>
namespace {thread_local bool inject=false,denied=false;thread_local size_t calls=0,boundary=SIZE_MAX;}
[[gnu::noinline]] void *operator new(size_t n){if(inject&&++calls>=boundary){denied=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);}void operator delete[](void *p)noexcept{std::free(p);}void operator delete(void *p,size_t)noexcept{std::free(p);}void operator delete[](void *p,size_t)noexcept{std::free(p);}
namespace dome {struct DocumentTransactionTestAccess {
 static auto tokens(const Document &d){std::lock_guard lock(d.model_mutex_);const auto &c=d.state_.controls;const auto &s=d.state_.bake_settings;
  return std::tuple{d.content_,d.next_content_,d.saved_content_,d.undo_.size(),d.redo_.size(),d.state_.generation,d.state_.revision,d.state_.edit_authority,d.asset_work_.load(),d.native_source_scene_,d.input_.owns_control(VrParameter::BakeSource),d.input_.menu_page(),d.state_.bake_source,d.state_.bake_recipe,d.state_.bake_settings_object,bool(d.control_before_),c.parameter,c.title,c.value,c.bake_preset,c.components,c.selected,c.minimum,c.maximum,c.logarithmic,s.front_mm,s.back_mm,s.ao_distance_mm,s.ao_samples,s.padding,s.allow_partial,s.allow_backfaces,s.prepare_colors,s.color_error,s.prepare_normals,s.normal_error,s.cage_object};
 }
 static void event(Document &d,bool confirm){std::lock_guard lock(d.model_mutex_);if(confirm)d.input_.complete_control();d.control_event_locked({confirm?VrControlEventType::Confirm:VrControlEventType::RecallBakeRecipe,VrParameter::BakeSource});}
};}
namespace {
const Scene &original(){static const auto scene=[] {auto s=cage_scene();bake_object(s,2,cage_settings());return s;}();return scene;}
std::unique_ptr<Document> prepared(bool confirm){
 auto d=std::make_unique<Document>();d->replace(original());ready_cage(*d);
 for(auto name:{"Retained target A","Retained target B"}){auto object=d->copy_scene().active();d->object_properties(object.id,name,object.visible,object.locked,object.transform,object.material);}
 require_cage(d->undo()&&d->wait_mesh(),"Recipe admission retains actual Undo and Redo");
 Driver hand(*d);hand.open();hand.sector(1);hand.value(.2f,0);hand.confirm();hand.sector(0);
 require_cage(d->frame().state->bake_settings.front_mm!=100,"Actual controller stages changed settings before recipe events");
 if(confirm)DocumentTransactionTestAccess::event(*d,false);
 return d;
}
}
int main()try{
 for(bool confirm:{false,true}){
  auto measured=prepared(confirm);calls=0;boundary=SIZE_MAX;inject=true;try{DocumentTransactionTestAccess::event(*measured,confirm);}catch(...){inject=false;throw;}inject=false;const auto count=calls;
  require_cage(count&&count<4096,"Actual recipe event allocation boundaries measured");
  for(size_t at=1;at<=count;++at){auto d=prepared(confirm);const auto before=d->copy_scene().encode();const auto tokens=DocumentTransactionTestAccess::tokens(*d);const auto frame=d->frame();calls=0;boundary=at;denied=false;bool caught=false;inject=true;
   try{DocumentTransactionTestAccess::event(*d,confirm);}catch(const std::bad_alloc&){caught=true;}inject=false;
   if(!caught||!denied||d->copy_scene().encode()!=before||DocumentTransactionTestAccess::tokens(*d)!=tokens||d->frame().state!=frame.state||d->frame().surface!=frame.surface){std::cerr<<"FAILED_RECIPE_PREFIX "<<(confirm?"confirm":"recall")<<' '<<at<<" caught="<<caught<<" denied="<<denied<<'\n';throw std::runtime_error("Recipe event lost exact draft/settings/input/history/publication");}
   DocumentTransactionTestAccess::event(*d,confirm);
   require_cage(d->copy_scene().encode()==before&&
    (confirm?(d->frame().state->bake_settings.front_mm==100&&d->frame().state->controls.parameter==VrParameter::None):bool(d->frame().state->controls.bake_preset)),
    "Same native recipe draft retries without changing Scene or history");
  }
  std::cout<<"PASS recipe_"<<(confirm?"confirm":"recall")<<"_persistent_prefixes="<<count<<'\n';
 }
 auto base=original();base.active().shape_source=capture_polygon_origin(base,base.active().id);
 const auto group=base.add_group("Recipe dependencies");base.object(2).group=base.object(3).group=group;
 for(bool grouped:{false,true}){
  const auto erase=[&](Scene &s){if(grouped)s.erase_group(group,true);else s.erase(2);};
  auto measured=base;calls=0;boundary=SIZE_MAX;inject=true;try{erase(measured);}catch(...){inject=false;throw;}inject=false;const auto count=calls;const auto expected=measured.encode();
  require_cage(count&&count<4096,"Actual recipe dependency deletion allocations measured");
  for(size_t at=1;at<=count;++at){auto candidate=base;const auto before=candidate.encode();const auto recipe=candidate.active().bake_recipe;const auto history=candidate.active().shape_source;calls=0;boundary=at;denied=false;bool caught=false;inject=true;
   try{erase(candidate);}catch(const std::bad_alloc&){caught=true;}inject=false;
   if(!caught||!denied||candidate.encode()!=before||candidate.active().bake_recipe!=recipe||candidate.active().shape_source!=history)throw std::runtime_error("Recipe dependency deletion changed Scene or immutable history after allocation denial");
   erase(candidate);if(candidate.encode()!=expected)throw std::runtime_error("Recipe dependency deletion retry differs from positive endpoint");
  }
  std::cout<<"PASS recipe_"<<(grouped?"group":"object")<<"_delete_persistent_prefixes="<<count<<'\n';
 }
 return 0;
}catch(const std::exception &e){inject=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
