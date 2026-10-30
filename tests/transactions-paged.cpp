// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/sculpt-resolution.hpp"
#include "dome/sculpt-storage.hpp"
#include "ray-index.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool value,const char *text){if(!value)throw std::runtime_error(text);++checks;std::cout<<"PASS "<<text<<std::endl;}
nlohmann::json ordered(const Volume &v){auto j=v.encode();std::sort(j["samples"].begin(),j["samples"].end());for(auto &l:j["sculpt_layers"])std::sort(l["delta"].begin(),l["delta"].end());if(j.contains("sculpt_mask"))std::sort(j["sculpt_mask"]["values"].begin(),j["sculpt_mask"]["values"].end());return j;}
Volume source(){
  Volume v(.04f);v.begin_stroke();Brush b;b.radius=.237f;b.blend=0;v.stamp(b);v.end_stroke();
  auto detail=v.add_layer("Editable detail");auto hidden=v.add_layer("Retained hidden locked detail");v.layer_properties(hidden,"Retained hidden locked detail",false,true,.25f);v.select_layer(detail);v.layer_properties(detail,"Editable detail",true,false,.75f);
  v.fill_mask(.25f);auto settings=v.sculpt_settings();settings.mask_edit=true;settings.mask_strength=.4f;settings.symmetry_axes=5;settings.symmetry_origin={.01f,-.02f,.03f};v.sculpt_settings(settings);
  v.begin_stroke();b.from=b.to={0,0,0};b.radius=.1f;v.stamp(b);v.end_stroke();settings.mask_edit=false;v.sculpt_settings(settings);
  SculptResolutionOptions options;options.voxel_size=.02f;
  auto result=resample_sculpt(v,options);check(result.report.changed&&bool(result.replacement),"Physical-band fixture is produced by real bounded resolution conversion");return std::move(*result.replacement);
}
}
int main()try{
  Document doc;doc.replace(source());check(doc.wait_mesh(),"Paged Volume4 authoring baseline is ready");
  auto before=doc.copy_volume();auto original=ordered(before);auto owner=before.geometry_owner();auto mask_owner=before.mask_values().owner();
  check(original["version"]==4&&before.band()==.16f&&before.voxel_size()==.02f,"Converted physical band and local grid spacing remain independent");
  check(before.layers().size()==3&&!before.layers()[2].visible&&before.layers()[2].locked&&before.layers()[2].opacity==.25f&&before.active_layer()==before.layers()[1].id,"Hidden/locked/strength/active displacement identities are retained");
  check(before.mask_default()==.25f&&!before.mask_values().empty()&&before.sculpt_settings().symmetry_axes==5,"Independent global/sparse protection and object-local symmetry are present");
  check(doc.begin_desktop(),"Explicit stroke targets the selected unlocked displacement layer");Brush b;b.from=b.to={0,0,.23f};b.radius=.08f;b.tool=Tool::Carve;b.pressure=.6f;b.blend=0;doc.stamp_desktop(b);doc.end_desktop();check(doc.wait_mesh(),"Edited paged geometry becomes available");
  auto changed=doc.copy_volume();auto edited=ordered(changed);auto changed_owner=changed.geometry_owner();
  check(edited!=original&&changed_owner!=owner,"Real protected mirrored sculpting creates a new immutable paged root");
  check(ordered(before)==original&&before.geometry_owner()==owner,"Held original root and pages are unchanged by later writes");
  check(doc.undo(),"Document transactional Undo succeeds over paged state");auto restored=doc.copy_volume();
  check(ordered(restored)==original&&restored.mask_values().owner()==mask_owner,"Undo restores exact samples, physical band, all layers, protection/settings and independent mask owner");
  check(restored.geometry_change(changed_owner).kind==Volume::GeometryChange::Kind::Local,"Undo swaps the authenticated geometry journal with its new owner");
  check(ordered(changed)==edited&&changed.geometry_owner()==changed_owner,"Undo cannot mutate a retained edited owner");
  check(doc.redo(),"Document transactional Redo succeeds over paged state");auto redone=doc.copy_volume();
  check(ordered(redone)==edited&&redone.geometry_change(restored.geometry_owner()).kind==Volume::GeometryChange::Kind::Local,"Redo restores exact data and authenticated ancestry rather than a stale journal");
  auto stable=redone.geometry_owner();check(doc.begin_desktop(),"A subsequent gesture uses the swapped empty/before-page providers");b.from=b.to={.1f,0,.2f};doc.stamp_desktop(b);doc.cancel_desktop();auto cancelled=doc.copy_volume();
  check(ordered(cancelled)==edited&&cancelled.geometry_owner()==stable,"Cancellation after Undo/Redo restores the original paged root allocation-free");
  auto settings=doc.frame().state->sculpt_settings;settings.mask_edit=true;doc.sculpt_settings(settings);
  auto mask_before=doc.copy_volume();auto mask_json=ordered(mask_before);auto mask_before_owner=mask_before.mask_values().owner();auto sdf_owner=mask_before.geometry_owner();
  check(doc.begin_desktop(),"Protection gesture uses the independently swapped mask page provider");b.from=b.to={.15f,0,0};b.radius=.1f;b.pressure=1;doc.stamp_desktop(b);doc.end_desktop();
  auto mask_after=doc.copy_volume();auto masked=ordered(mask_after);auto mask_after_owner=mask_after.mask_values().owner();
  check(masked!=mask_json&&mask_after_owner!=mask_before_owner&&mask_after.geometry_owner()==sdf_owner,"Protection creates an independent COW mask root without changing SDF ownership");
  check(doc.undo()&&ordered(doc.copy_volume())==mask_json&&doc.copy_volume().mask_values().owner()==mask_before_owner,"Transactional mask Undo restores exact independent owner/settings/layers");
  check(doc.redo()&&ordered(doc.copy_volume())==masked&&doc.copy_volume().mask_values().owner()==mask_after_owner,"Transactional mask Redo preserves exact original mask page identity");
  check(ordered(mask_before)==mask_json&&mask_before.geometry_owner()==sdf_owner&&ordered(redone)==edited,"Retained SDF and mask snapshots stay immutable across both histories");
  check(doc.copy_volume().geometry_change(sdf_owner).kind==Volume::GeometryChange::Kind::Unchanged,"Mask-only history cannot manufacture SDF dirty ancestry");
  check(doc.begin_desktop(),"Further mask gesture starts after swapped history providers");b.from=b.to={-.15f,0,0};doc.stamp_desktop(b);doc.cancel_desktop();
  check(ordered(doc.copy_volume())==masked&&doc.copy_volume().mask_values().owner()==mask_after_owner,"Mask cancellation restores exact pre-gesture pages after Undo/Redo");
  edited=std::move(masked);
  auto path=std::filesystem::temp_directory_path()/("dome-transactions-paged-"+std::to_string(getpid())+".dsculpt");doc.save(path);auto loaded=Scene::load(path);
  check(ordered(*loaded.active().volume)==edited,"Native save/reload preserves the complete swapped Volume4 payload");std::filesystem::remove(path);
  using Legacy=std::optional<Vec3>(Volume::*)(Vec3,Vec3,float)const;Legacy legacy=static_cast<Legacy>(&Volume::raycast);
  auto hit=(redone.*legacy)({0,0,1},{0,0,-1},2);check(bool(hit),"The preserved three-argument picker remains usable after transactional paged edits");
  sculpt_detail::clear_ray_index_cache();std::cout<<checks<<" paged transaction state/owner/journal checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
