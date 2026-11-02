// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/rig-fit-tool.hpp"
namespace dome {
std::string rig_fit_role_label(size_t i){const auto &r=human_roles().at(i);return r.name+(r.required?" (required)":"");}
std::string rig_fit_tool_label(const RigFitView *v,int sector){
 static const std::array<const char *,12> labels={"Previous landmark","Next landmark","Place landmark","Fitting options","X -","X +","Y -","Y +","Z -","Z +","Fit and bind","Cancel fitting"};
 if(sector<0||sector>=12)return {};
 if(v&&v->settings){if(sector==0)return "Previous option";if(sector==1)return "Next option";if(sector==2||sector==3)return "Landmarks";if(sector==4)return "Decrease";if(sector==5)return "Increase";if(sector>=6&&sector<=9)return "";}
 if(sector==2&&v&&v->placing)return "Finish placement";
 if(sector==10&&v&&!v->options.bind)return "Fit skeleton";
 return labels[size_t(sector)];
}
std::string rig_fit_parameter_text(const RigFitView &v){
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
