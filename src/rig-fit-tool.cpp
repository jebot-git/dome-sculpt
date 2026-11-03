// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/rig-fit-tool.hpp"
#include "rig-fit-metric.hpp"
namespace dome {
namespace { Vec3 world_point(const AffineTransform &world,Vec3 point){auto p=world.point({point.x,point.y,point.z});return {float(p[0]),float(p[1]),float(p[2])};} }
size_t rig_fit_selection_count(const RigFitView &v){if(v.rest_source&&!v.rest_source->skeleton)throw std::invalid_argument("Missing original fitting skeleton");return v.rest_source?v.rest_source->skeleton->bones.size():human_roles().size();}
size_t rig_fit_options_count(const RigFitView &v){return v.rest_source?5:rig_fit_parameter_count;}
std::string rig_fit_selection_label(const RigFitView &v){return v.rest_source?v.rest_source->skeleton->bones.at(v.role).name:rig_fit_role_label(v.role);}
std::optional<Vec3> rig_fit_selection_point(const RigFitView &v){if(v.rest_source)return v.bone_heads.at(v.role);const auto &name=human_roles().at(v.role).name;auto at=v.landmarks.points.find(name);return at==v.landmarks.points.end()?std::optional<Vec3>{}:at->second;}
void rig_fit_selection_point(RigFitView &v,std::optional<Vec3> p){if(v.rest_source){if(!p)throw std::logic_error("Existing bone heads cannot be removed; use role assignment separately");v.bone_heads.at(v.role)=*p;return;}const auto &name=human_roles().at(v.role).name;if(p)v.landmarks.points[name]=*p;else v.landmarks.points.erase(name);}
std::vector<RigRestJointEdit> rig_fit_rest_edits(const RigFitView &v){
 if(!v.rest_source||!v.rest_source->skeleton)throw std::invalid_argument("This fitting tool has no original REST skeleton");
 const auto &s=*v.rest_source->skeleton;const auto segments=s.segments();const auto rest=s.world();std::vector<RigRestJointEdit> edits;
 if(v.bone_heads.size()!=s.bones.size()||v.bone_parents.size()!=s.bones.size()||v.bone_lengths.size()!=s.bones.size())throw std::invalid_argument("Incomplete fitting bone identities");
 for(size_t j=0;j<s.bones.size();++j){RigRestJointEdit edit;edit.joint=uint32_t(j);
  if(v.bone_heads[j].x!=segments[j][0].x||v.bone_heads[j].y!=segments[j][0].y||v.bone_heads[j].z!=segments[j][0].z)edit.world_position=world_point(v.world,v.bone_heads[j]);
  if(v.bone_parents[j]!=s.bones[j].parent)edit.parent=v.bone_parents[j];
  const float original=detail::rig_fit_world_length(v.world,rest[j],s.bones[j].length);
  if(v.bone_lengths[j]!=original)edit.world_length=v.bone_lengths[j];
  if(edit.world_position||edit.parent||edit.world_length)edits.push_back(edit);
 }return edits;
}

std::string rig_fit_role_label(size_t i){const auto &r=human_roles().at(i);return r.name+(r.required?" (required)":"");}
std::string rig_fit_tool_label(const RigFitView *v,int sector){
 static const std::array<const char *,12> labels={"Previous landmark","Next landmark","Place landmark","Fitting options","X -","X +","Y -","Y +","Z -","Z +","Fit and bind","Cancel fitting"};
 if(sector<0||sector>=12)return {};
 if(v&&v->settings){if(sector==0)return "Previous option";if(sector==1)return "Next option";if(sector==2||sector==3)return "Landmarks";if(sector==4)return "Decrease";if(sector==5)return "Increase";if(sector>=6&&sector<=9)return "";}
 if(v&&v->rest_source){if(sector==0&&!v->settings)return "Previous bone";if(sector==1&&!v->settings)return "Next bone";if(sector==2&&!v->settings&&!v->placing)return "Place bone head";if(sector==10)return v->restore_source?"Restore original rig":"Apply rest fitting";}
 if(sector==2&&v&&v->placing)return "Finish placement";
 if(sector==10&&v&&!v->options.bind)return "Fit skeleton";
 return labels[size_t(sector)];
}
std::string rig_fit_parameter_text(const RigFitView &v){
 if(v.rest_source){switch(v.parameter){case 0:return "Step (world m) "+std::to_string(v.step);case 1:{const auto parent=v.bone_parents.at(v.role);return "Parent: "+(parent<0?std::string("Root"):v.rest_source->skeleton->bones.at(size_t(parent)).name);}case 2:return "Bone length (world m) "+std::to_string(v.bone_lengths.at(v.role));case 3:return v.linked_rest?"Fit all linked rig parts":"Fit selected rig part";case 4:return v.restore_source?"Restore original rig data":"Keep fitted rig data";default:throw std::invalid_argument("Unknown REST fitting option");}}
 auto number=[](const char *name,float value){return std::string(name)+std::to_string(value);};
 const auto &o=v.options;
 switch(v.parameter){case 0:return number("Step (world m) ",v.step);case 1:return o.bind?"Bind skin weights":"Fit skeleton only";
 case 2:return number("Surface distance (m) ",o.fit.maximum_surface_distance);case 3:return number("Minimum segment (m) ",o.fit.minimum_segment_length);case 4:return number("Terminal length (m) ",o.fit.terminal_length);
 case 5:return number("Skin seed distance (m) ",o.binding.maximum_seed_distance);case 6:return number("Skin seed margin (m) ",o.binding.seed_margin);case 7:return number("Skin falloff ",o.binding.falloff);case 8:return "Skin influences "+std::to_string(o.binding.limits.max_influences);
 case 9:return v.landmarks.points.contains(human_roles().at(v.role).name)?"Selected landmark placed":"Selected landmark missing";
 case 10:return "Rotate model body frame X";case 11:return "Rotate model body frame Y";case 12:return "Rotate model body frame Z";case 13:return "Reset model body frame (+Y up, +Z front)";
 default:throw std::invalid_argument("Unknown fitting option");}
}
void adjust_rig_fit_parameter(RigFitView &v,int direction){
 if(direction!=1&&direction!=-1)throw std::invalid_argument("Fitting adjustment needs a direction");const float factor=direction>0?1.1f:1/1.1f;auto scaled=[&](float &f,float lo,float hi){f=std::clamp(f*factor,lo,hi);};auto &o=v.options;
 if(v.rest_source){switch(v.parameter){case 0:scaled(v.step,.00001f,10);break;case 1:{const auto n=int(v.rest_source->skeleton->bones.size());int candidate=v.bone_parents.at(v.role);for(int attempt=0;attempt<=n;++attempt){candidate=(candidate+1+direction+n+1)%(n+1)-1;if(candidate==int(v.role))continue;auto graph=*v.rest_source->skeleton;for(size_t j=0;j<graph.bones.size();++j)graph.bones[j].parent=v.bone_parents[j];graph.bones[v.role].parent=candidate;try{graph.validate();}catch(const std::invalid_argument&){continue;}v.bone_parents.at(v.role)=candidate;break;}break;}case 2:scaled(v.bone_lengths.at(v.role),.000001f,10000);break;case 3:v.linked_rest=!v.linked_rest;break;case 4:v.restore_source=!v.restore_source;break;default:throw std::invalid_argument("Unknown REST fitting option");}return;}
 switch(v.parameter){case 0:scaled(v.step,.00001f,10);break;case 1:o.bind=!o.bind;break;
 case 2:scaled(o.fit.maximum_surface_distance,.00001f,10);break;case 3:scaled(o.fit.minimum_segment_length,.000001f,o.fit.terminal_length);break;case 4:scaled(o.fit.terminal_length,o.fit.minimum_segment_length,10);break;
 case 5:scaled(o.binding.maximum_seed_distance,std::max(.00001f,o.binding.seed_margin),10000);break;case 6:o.binding.seed_margin=std::clamp(o.binding.seed_margin+direction*.001f,0.f,o.binding.maximum_seed_distance);break;case 7:o.binding.falloff=std::clamp(o.binding.falloff+direction*.1f,1.f,8.f);break;case 8:o.binding.limits.max_influences=uint32_t(std::clamp(int(o.binding.limits.max_influences)+direction,1,32));break;
 case 9:{const auto &name=human_roles().at(v.role).name;if(v.landmarks.points.contains(name))v.landmarks.points.erase(name);else throw std::logic_error("Place the missing landmark with the controller or world coordinates");break;}
 case 10:case 11:case 12:{Vec3 axis{};if(v.parameter==10)axis.x=1;else if(v.parameter==11)axis.y=1;else axis.z=1;const float angle=direction*.0436332313f;Quat q{std::cos(angle),axis.x*std::sin(angle),axis.y*std::sin(angle),axis.z*std::sin(angle)};v.landmarks.left=q.rotate(v.landmarks.left);v.landmarks.up=q.rotate(v.landmarks.up);v.landmarks.front=q.rotate(v.landmarks.front);break;}
 case 13:v.landmarks.left={1,0,0};v.landmarks.up={0,1,0};v.landmarks.front={0,0,1};break;
 default:throw std::invalid_argument("Unknown fitting option");}
 o.validate();
}
} // namespace dome
