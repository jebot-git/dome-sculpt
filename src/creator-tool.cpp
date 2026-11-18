// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creator-tool.hpp"
#include "text-utf8.hpp"
#include <cstdio>
#include <nlohmann/json.hpp>
namespace dome {
namespace {
using Member=float HumanoidControls::*;
constexpr Member fields[]={&HumanoidControls::height,&HumanoidControls::anime,
 &HumanoidControls::build,&HumanoidControls::shoulder_width,&HumanoidControls::waist_width,
 &HumanoidControls::leg_length,&HumanoidControls::arm_length,&HumanoidControls::head_width,
 &HumanoidControls::head_height,&HumanoidControls::eye_size,&HumanoidControls::eye_spacing,
 &HumanoidControls::nose_size,&HumanoidControls::mouth_width,&HumanoidControls::jaw_width,
 &HumanoidControls::hair_length,&HumanoidControls::hair_volume,&HumanoidControls::fringe};
constexpr const char *labels[]={"Height (m)","Anime proportions","Build","Shoulder width",
 "Waist width","Leg length","Arm length","Head width","Head height","Eye size",
 "Eye spacing","Nose size","Mouth width","Jaw width","Hair length","Hair volume","Fringe",
 "Skin red","Skin green","Skin blue","Iris red","Iris green","Iris blue",
 "Hair red","Hair green","Hair blue"};
bool same(const HumanoidCreatorSettings&a,const HumanoidCreatorSettings&b){
 return a.name==b.name&&a.preset==b.preset&&a.controls.encode()==b.controls.encode();
}
}
void HumanoidCreatorSettings::validate() const {
 controls.validate();
 if(controls.baseline!="vroid-beta-female"&&controls.baseline!="vroid-beta-male")
  throw std::invalid_argument("Creator offers only the verified CC0 VRoid female and male bases");
 if(name.empty()||name.size()>128||name.find('\0')!=std::string::npos||!text::valid_utf8(name))
  throw std::invalid_argument("Avatar name must contain 1 to 128 valid UTF-8 bytes");
 if(preset&&*preset!=HumanoidPreset::Neutral&&*preset!=HumanoidPreset::Toon&&*preset!=HumanoidPreset::Anime)
  throw std::invalid_argument("Unknown humanoid creator preset");
}
HumanoidCreatorParameter humanoid_creator_parameter(const HumanoidControls&c,size_t index){
 if(index>=humanoid_creator_parameter_count)throw std::out_of_range("Unknown humanoid creator parameter");
 float value=0,lo=0,hi=1;
 if(index<std::size(fields)){
  value=c.*fields[index];
  if(index==0){lo=1.2f;hi=2.2f;}
  else if(index!=1&&index!=9&&index<14)lo=-1;
 }else{
  const auto &color=index<20?c.skin_color:index<23?c.iris_color:c.hair_color;
  value=color[(index-17)%3];
 }
 return {labels[index],lo,hi,.01f,value};
}
void set_humanoid_creator_parameter(HumanoidControls&controls,size_t index,float value){
 const auto parameter=humanoid_creator_parameter(controls,index);
 if(!std::isfinite(value)||value<parameter.minimum||value>parameter.maximum)
  throw std::invalid_argument("Creator parameter is outside its native bounds");
 auto next=controls;
 if(index<std::size(fields))next.*fields[index]=value;
 else {auto &color=index<20?next.skin_color:index<23?next.iris_color:next.hair_color;color[(index-17)%3]=value;}
 next.validate();controls=std::move(next);
}
std::string humanoid_creator_parameter_text(const HumanoidCreatorView&view){
 const auto p=humanoid_creator_parameter(view.settings.controls,view.parameter);
 char value[48];std::snprintf(value,sizeof(value),"%.3f",double(p.value));
 return std::string(p.label)+": "+value;
}
void update_humanoid_creator(HumanoidCreatorView&view,HumanoidCreatorSettings next){
 next.validate();if(view.slider_before)throw std::logic_error("Finish the creator slider first");
 if(same(view.settings,next))return;
 view.undo.push_back(view.settings);if(view.undo.size()>128)view.undo.erase(view.undo.begin());
 view.redo.clear();view.settings=std::move(next);
}
bool humanoid_creator_history(HumanoidCreatorView&view,bool redo){
 if(view.slider_before){if(redo)return false;view.settings=*view.slider_before;view.slider_before.reset();return true;}
 auto &from=redo?view.redo:view.undo;auto &to=redo?view.undo:view.redo;if(from.empty())return false;
 to.push_back(view.settings);view.settings=std::move(from.back());from.pop_back();return true;
}
} // namespace dome
