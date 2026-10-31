// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/material-visibility.hpp"
#include "document-transaction.hpp"
#include <algorithm>
namespace dome {
namespace {
bool contains(std::span<const uint64_t> ids,uint64_t id){return std::find(ids.begin(),ids.end(),id)!=ids.end();}
bool descendant(const Scene &scene,const SculptObject &o,uint64_t group){
  auto id=o.group;size_t depth=0;
  while(id){if(id==group)return true;if(++depth>128)throw std::length_error("Selection hierarchy is too deep");id=scene.group(id).parent;}
  return false;
}
}
std::vector<ObjectHierarchyRow> object_hierarchy(const Scene &scene,std::span<const uint64_t> selected,std::span<const uint64_t> collapsed){
  std::vector<ObjectHierarchyRow> rows;
  if(scene.objects().size()>256||scene.groups().size()>128)throw std::length_error("Selection hierarchy exceeds scene limits");
  const auto &groups=scene.groups();
  std::array<std::pair<uint64_t,size_t>,128> index{};
  struct Count{size_t total=0,marked=0;bool visible=false;};
  std::array<Count,128> counts{};
  for(size_t i=0;i<groups.size();++i)index[i]={groups[i].id,i};
  std::sort(index.begin(),index.begin()+groups.size());
  for(size_t i=0;i<groups.size();++i)if(!index[i].first||(i&&index[i].first==index[i-1].first))throw std::invalid_argument("Invalid selection group identity");
  auto locate=[&](uint64_t id){
    auto end=index.begin()+groups.size();
    auto found=std::lower_bound(index.begin(),end,id,[](auto pair,uint64_t key){return pair.first<key;});
    if(found==end||found->first!=id)throw std::invalid_argument("Missing selection group parent");
    return found->second;
  };
  // Visit each object's ancestry once, rather than traversing it again for
  // every folder on every input publication. Scratch storage is bounded.
  for(const auto &g:groups){auto id=g.id;size_t depth=0;while(id){if(++depth>128)throw std::length_error("Selection hierarchy is too deep or cyclic");id=groups[locate(id)].parent;}}
  for(const auto &o:scene.objects()){
    auto id=o.group;bool marked=contains(selected,o.id);
    while(id){auto at=locate(id);auto &count=counts[at];++count.total;count.marked+=marked;count.visible|=o.visible;id=groups[at].parent;}
  }
  rows.reserve(groups.size()+scene.objects().size());
  auto append=[&](auto &&self,uint64_t parent,unsigned depth)->void{
    if(depth>128)throw std::length_error("Selection hierarchy is too deep");
    for(size_t i=0;i<groups.size();++i)if(groups[i].parent==parent){
      const auto &g=groups[i];const auto &count=counts[i];bool open=!contains(collapsed,g.id);
      rows.push_back({g.id,depth,g.name,true,open,count.total&&count.marked==count.total,count.marked>0&&count.marked<count.total,false,count.visible,false});
      if(open)self(self,g.id,depth+1);
    }
    for(const auto &o:scene.objects())if(o.group==parent)
      rows.push_back({o.id,depth,o.name,false,false,contains(selected,o.id),false,contains(selected,o.id)&&o.id==scene.active().id,o.visible,o.locked});
  };
  append(append,0,0);return rows;
}
bool Document::selection_ready_locked() const {
  return !state_.asset_busy&&!state_.recovery_busy&&!state_.object_grabbing&&!desktop_&&!native_stroke_&&!action_active_locked()&&!state_.polygon&&!state_.cut&&!state_.retopology&&!uv_&&!control_before_&&!import_view_;
}
void Document::selection_hierarchy_locked(){
  std::erase_if(state_.selected_objects,[&](uint64_t id){return std::none_of(scene_.objects().begin(),scene_.objects().end(),[&](const auto&o){return o.id==id;});});
  std::erase_if(state_.collapsed_groups,[&](uint64_t id){return std::none_of(scene_.groups().begin(),scene_.groups().end(),[&](const auto&g){return g.id==id;});});
  if(!state_.hierarchy_open){state_.hierarchy.clear();return;}
  state_.hierarchy=object_hierarchy(scene_,state_.selected_objects,state_.collapsed_groups);
  const auto last=state_.hierarchy.empty()?0:((state_.hierarchy.size()-1)/object_outliner::page_rows)*object_outliner::page_rows;
  state_.hierarchy_offset=std::min(state_.hierarchy_offset,last);
}
void Document::configure_selection(bool on){std::lock_guard lock(model_mutex_);configure_selection_locked(on);}
void Document::configure_selection_locked(bool on){
  if(state_.selection_mode==on)return;
  if(!selection_ready_locked())throw std::logic_error("Finish or cancel the current edit before changing selection mode");
  Transaction transaction(*this);invalidate_object_authority_locked();state_.held_object=0;
  state_.selection_mode=on;
  if(on){
    state_.object_grab_mode=false;state_.selection_focus=true;state_.hierarchy_open=true;state_.hierarchy_anchored=false;
    if(state_.selected_objects.empty())state_.selected_objects.push_back(scene_.active().id);
  }
  state_.selection_hover={};state_.selection_pressed={true,true};state_.selection_release={true,true};
  state_.message=on?"Selection: touch or point + trigger; grip + trigger toggles membership":"Editing active object; unselected objects remain transparent";
  auto mesh=mesh_.load();
  const bool picking=on&&(!mesh->picking||mesh->generation!=state_.generation||mesh->revision!=state_.revision);
  publish_locked(picking,false);transaction.commit(false);
}
void Document::select_objects(std::span<const uint64_t> ids,ObjectSelection operation,AssetSource expected){std::lock_guard lock(model_mutex_);select_objects_locked(ids,operation,expected);}
void Document::select_objects_locked(std::span<const uint64_t> ids,ObjectSelection operation,AssetSource expected){
  if(!selection_ready_locked())throw std::logic_error("Finish or cancel the current edit before selecting objects");
  if(expected.generation&&expected.generation!=state_.generation)throw std::logic_error("The document changed; reopen the hierarchy");
  if(expected.revision&&expected.revision!=state_.revision)throw std::logic_error("The scene changed; reopen the hierarchy");
  if(expected.edit_authority&&expected.edit_authority!=state_.edit_authority)throw std::logic_error("Selection authority changed; reopen the hierarchy");
  if(ids.size()>256)throw std::length_error("Too many selected objects");
  std::vector<uint64_t> next=operation==ObjectSelection::Replace?std::vector<uint64_t>{}:state_.selected_objects;
  std::vector<uint64_t> unique;
  for(auto id:ids){(void)scene_.object(id);if(contains(unique,id))continue;unique.push_back(id);
    auto found=std::find(next.begin(),next.end(),id);
    if(operation==ObjectSelection::Toggle&&found!=next.end())next.erase(found);
    else if(found==next.end())next.push_back(id);
  }
  uint64_t active=scene_.active().id;
  // Last added member becomes active, matching a viewport/Outliner extend.
  for(auto id:unique)if(contains(next,id))active=id;
  if(!next.empty()&&!contains(next,active))active=next.back();
  if(next==state_.selected_objects&&active==scene_.active().id&&state_.selection_focus)return;
  Transaction transaction(*this);invalidate_object_authority_locked();state_.held_object=0;
  const bool changed=scene_.active().id!=active;
  state_.selected_objects=std::move(next);state_.selection_focus=true;
  scene_.select(active);input_.refresh_volume(volume());bound_volume_=scene_.active().volume;
  last_volume_=volume().revision();last_paint_=scene_.active().mesh_active?scene_.active().painted->revision():0;
  // Selection itself never meshes. Only existing selection-dependent preview
  // modes (weight heatmap / shape / mask) require a new worker snapshot.
  publish_locked(changed&&(state_.weights.enabled||state_.shape_sculpt||state_.sculpt_mask_visible),false);transaction.commit(false);
}
void Document::select_group(uint64_t group,ObjectSelection operation,AssetSource expected){std::lock_guard lock(model_mutex_);select_group_locked(group,operation,expected);}
void Document::select_group_locked(uint64_t group,ObjectSelection operation,AssetSource expected){
  (void)scene_.group(group);std::vector<uint64_t> ids;
  for(const auto &o:scene_.objects())if(descendant(scene_,o,group))ids.push_back(o.id);
  // A collection keeps hidden members but gives a visible unlocked child the
  // active editing role whenever possible.
  std::stable_partition(ids.begin(),ids.end(),[&](auto id){const auto&o=scene_.object(id);return !o.visible||o.locked;});
  // Toggle a collection as one set, including descendants of collapsed rows.
  if(operation==ObjectSelection::Toggle){
    bool all=!ids.empty()&&std::all_of(ids.begin(),ids.end(),[&](auto id){return contains(state_.selected_objects,id);});
    if(!all)operation=ObjectSelection::Extend;
  }
  select_objects_locked(ids,operation,expected);
}
void Document::show_object_hierarchy(bool open){
  std::lock_guard lock(model_mutex_);Transaction transaction(*this);
  state_.hierarchy_open=open;if(open){state_.hierarchy_anchored=false;if(selection_ready_locked())input_.cancel();}else state_.hierarchy_hover={-1,-1};
  publish_locked(false,false);transaction.commit(false);
}
void Document::focus_selection(bool on){
  std::lock_guard lock(model_mutex_);if(state_.selection_focus==on)return;
  if(!selection_ready_locked())throw std::logic_error("Finish the current edit before changing selection focus");
  Transaction transaction(*this);invalidate_object_authority_locked();state_.selection_focus=on;
  publish_locked(false,false);transaction.commit(false);
}
bool Document::process_object_selection_locked(const SpatialFrame &f){
  if(state_.hierarchy_open&&!state_.hierarchy_anchored){
    state_.hierarchy_panel={f.head.position+ob::rotate(f.head.orientation,{.35f,-.02f,-1.1f}),f.head.orientation,1};state_.hierarchy_anchored=true;
  }
  const bool menu=input_.menu_open();
  state_.selection_hover={};state_.hierarchy_hover={-1,-1};
  bool captured=false;
  const bool additive=f.hands[0].tracked.squeeze>.55f||f.hands[1].tracked.squeeze>.55f;
  for(size_t i=0;i<2;++i){
    const auto &h=f.hands[i];
    if(!h.tracked.valid){state_.selection_pressed[i]=true;continue;}
    const bool pressed=h.tracked.trigger>.55f&&!state_.selection_pressed[i];
    if(h.tracked.trigger<=.25f)state_.selection_pressed[i]=false;
    if(h.tracked.trigger>.55f)state_.selection_pressed[i]=true;
    auto panel=h.tracked.aimValid?panel_ray(menu?state_.panel:state_.hierarchy_panel,h.tracked.aim):std::nullopt;
    if(panel&&((menu&&input_.menu_page()==0)||(!menu&&state_.hierarchy_open))){
      const float u=panel->u,v=panel->v;
      if(menu){
        if(v>=.02f&&v<.10f&&(u<.32f||u>.68f)){
          captured=true;if(pressed)state_.selection_release[i]=true;
          if(pressed&&selection_ready_locked()){
            if(u<.32f)configure_selection_locked(!state_.selection_mode);
            else{Transaction transaction(*this);state_.hierarchy_open=!state_.hierarchy_open;state_.hierarchy_anchored=false;input_.cancel();publish_locked(false,false);transaction.commit(false);}
          }
        }
      }else{
        captured=true;if(pressed)state_.selection_release[i]=true;
        if(v>=object_outliner::first&&v<object_outliner::first+object_outliner::page_rows*object_outliner::row_height){
          size_t row=state_.hierarchy_offset+size_t((v-object_outliner::first)/object_outliner::row_height);
          if(row<state_.hierarchy.size()){
            state_.hierarchy_hover[i]=int(row);
            if(pressed&&selection_ready_locked()){
              // Copy identity before a selection publication replaces rows.
              auto item=state_.hierarchy[row];
              if(item.group&&u<.13f){
                Transaction transaction(*this);auto &ids=state_.collapsed_groups;auto pos=std::find(ids.begin(),ids.end(),item.id);
                if(pos==ids.end())ids.push_back(item.id);else ids.erase(pos);
                publish_locked(false,false);transaction.commit(false);
              }else if(!item.group&&u>.87f){
                Transaction transaction(*this);invalidate_object_authority_locked();Scene before=scene_;
                scene_.object(item.id).visible=!item.visible;
                if(item.visible&&state_.held_object==item.id)state_.held_object=0;
                record_locked({{},std::move(before),scene_});publish_locked(true,false);transaction.commit();
              }else if(item.group)select_group_locked(item.id,additive?ObjectSelection::Toggle:ObjectSelection::Replace);
              else select_objects_locked(std::span<const uint64_t>(&item.id,1),additive?ObjectSelection::Toggle:ObjectSelection::Replace);
            }
          }
        }else if(pressed&&selection_ready_locked()){
          if(v>=.02f&&v<.10f){
            if(u<.34f)configure_selection_locked(!state_.selection_mode);
            else if(u<.84f){Transaction transaction(*this);invalidate_object_authority_locked();state_.selection_focus=!state_.selection_focus;publish_locked(false,false);transaction.commit(false);}
            else{Transaction transaction(*this);state_.hierarchy_open=false;publish_locked(false,false);transaction.commit(false);}
          }else if(v>=.12f&&v<.21f){
            if(u<.5f){std::vector<uint64_t> ids;for(const auto &o:scene_.objects())if(o.visible)ids.push_back(o.id);select_objects_locked(ids,ObjectSelection::Replace);}
            else select_objects_locked({},ObjectSelection::Replace);
          }else if(v>=.84f&&v<.92f){
            Transaction transaction(*this);
            if(u<.5f)state_.hierarchy_offset=state_.hierarchy_offset>=object_outliner::page_rows?state_.hierarchy_offset-object_outliner::page_rows:0;
            else if(state_.hierarchy_offset+object_outliner::page_rows<state_.hierarchy.size())state_.hierarchy_offset+=object_outliner::page_rows;
            publish_locked(false,false);transaction.commit(false);
          }
        }
      }
      if(captured)continue;
    }
    if(menu||!state_.selection_mode||h.joystick)continue;
    auto snapshot=mesh_.load();
    if(!snapshot->picking||snapshot->generation!=state_.generation||snapshot->revision!=state_.revision)continue;
    const auto origin=world_to_model(state_.canvas,{h.tracked.grip.position.x,h.tracked.grip.position.y,h.tracked.grip.position.z});
    auto hit=snapshot->picking->nearby(origin,.04f/state_.canvas.scale,f.time);
    if(!hit&&h.tracked.aimValid){
      const auto &aim=h.tracked.aim;auto from=world_to_model(state_.canvas,{aim.position.x,aim.position.y,aim.position.z});
      auto d=ob::rotate(ob::conjugate(state_.canvas.orientation),ob::rotate(aim.orientation,{0,0,-1}));
      hit=snapshot->picking->raycast(from,{d.x,d.y,d.z},5/state_.canvas.scale,f.time);
    }
    if(hit){
      const size_t corner=size_t(hit->triangle)*3;
      for(const auto &range:snapshot->ranges)if(corner>=range.first&&corner-range.first<range.count){state_.selection_hover[i]=range.object;break;}
      state_.selection_hit[i]=hit->position;
    }
    if(pressed&&selection_ready_locked()){
      if(auto id=state_.selection_hover[i])select_objects_locked(std::span<const uint64_t>(&id,1),additive?ObjectSelection::Toggle:ObjectSelection::Replace);
      else if(!additive)select_objects_locked({},ObjectSelection::Replace);
    }
  }
  // Let an actual menu-button edge enter the ordinary shared menu pipeline.
  const bool menu_press=std::any_of(f.hands.begin(),f.hands.end(),[](const auto&h){return h.joystick;});
  return captured&&!menu_press;
}
} // namespace dome
