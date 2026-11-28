// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
#include "dome/bake-recipe.hpp"
#include <algorithm>

namespace dome::detail {
inline std::vector<uint64_t> recipe_bake_donors(const ObjectBakeRecipe &recipe){
  std::vector<uint64_t> ids{recipe.source.object};
  for(const auto &donor:recipe.additional_sources)ids.push_back(donor.object);
  return ids;
}
inline std::span<const uint64_t> current_bake_donors(const EditorState &s){
  return s.bake_donors.empty()?std::span(&s.bake_source,1):std::span(s.bake_donors);
}
inline size_t bake_source_choice_count(const EditorState &s){
  return s.bake_sources.size()+!s.bake_selected_donors.empty()+!s.bake_saved_donors.empty()+
      (!s.bake_donors.empty()&&s.bake_donors!=s.bake_saved_donors&&s.bake_donors!=s.bake_selected_donors);
}
inline std::span<const uint64_t> bake_source_choice(const EditorState &s,size_t index){
  if(index<s.bake_sources.size())return std::span(&s.bake_sources[index],1);
  index-=s.bake_sources.size();
  if(!s.bake_selected_donors.empty()){
    if(index==0)return s.bake_selected_donors;
    --index;
  }
  if(!s.bake_saved_donors.empty()){
    if(index==0)return s.bake_saved_donors;
    --index;
  }
  if(index==0&&!s.bake_donors.empty()&&s.bake_donors!=s.bake_saved_donors&&s.bake_donors!=s.bake_selected_donors)
    return s.bake_donors;
  return {};
}
inline size_t bake_source_choice_index(const EditorState &s,std::span<const uint64_t> donors){
  const size_t count=bake_source_choice_count(s);
  for(size_t i=0;i<count;++i)if(std::ranges::equal(bake_source_choice(s,i),donors))return i;
  return count;
}
inline bool missing_bake_donor(std::span<const uint64_t> donors){
  return std::ranges::find(donors,unavailable_bake_object)!=donors.end();
}
inline std::string bake_source_choice_label(const EditorState &s,size_t index){
  const auto donors=bake_source_choice(s,index);
  if(donors.empty())return "Choose a different source";
  if(donors.size()>1){
    const auto count=" ("+std::to_string(donors.size())+")";
    if(missing_bake_donor(donors))return "Saved sources"+count+" - missing mesh";
    if(donors.data()==s.bake_selected_donors.data())return "Selected objects"+count;
    if(donors.data()==s.bake_saved_donors.data())return "Saved sources"+count;
    return "Previous selection"+count;
  }
  const auto found=std::ranges::find(s.objects,donors.front(),&ObjectInfo::id);
  return found==s.objects.end()?"Unavailable source - reselect":found->name+(found->visible?"":" [hidden]");
}
}
