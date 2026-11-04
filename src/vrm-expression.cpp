// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/vrm-expression.hpp"
#include "vrm1-metadata.hpp"
#include <algorithm>
#include <set>
#include <limits>
namespace dome {
namespace {
int category(std::string_view name){
 if(name=="aa"||name=="ih"||name=="ou"||name=="ee"||name=="oh")return 0;
 if(name=="blink"||name=="blinkLeft"||name=="blinkRight")return 1;
 if(name=="lookUp"||name=="lookDown"||name=="lookLeft"||name=="lookRight")return 2;
 return -1;
}
}
Vrm1ExpressionEvaluation evaluate_vrm1_expressions(const nlohmann::json &gltf,
 const Vrm1ExpressionWeights &requested,const std::function<bool()> &cancelled){
 auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("VRM1 expression evaluation cancelled");};stop();
 detail::validate_vrm1_metadata(gltf,cancelled);
 const auto &vrm=gltf.at("extensions").at("VRMC_vrm");
 const auto expressions=vrm.value("expressions",nlohmann::json::object());
 const std::array<const char*,2> tables{"preset","custom"};
 const std::array<const std::map<std::string,float>*,2> input{&requested.preset,&requested.custom};
 Vrm1ExpressionEvaluation result;
 const std::array<std::map<std::string,float>*,2> output{&result.effective.preset,&result.effective.custom};
 struct Entry{const nlohmann::json *definition;float *weight;bool binary;int group;};
 std::vector<Entry> entries;size_t count=0;
 for(size_t table=0;table<2;++table){
  const auto definitions=expressions.value(tables[table],nlohmann::json::object());
  for(const auto &[name,value]:*input[table]){
   stop();if(!std::isfinite(value))throw std::invalid_argument("VRM1 expression input must be finite");
   if(!definitions.contains(name))throw std::invalid_argument("VRM1 expression input has no actual source definition: "+name);
  }
  if(!expressions.contains(tables[table]))continue;
  for(const auto &entry:expressions.at(tables[table]).items()){
   stop();if(++count>100000)throw std::length_error("VRM1 expression count exceeds native work bound");
   if(table==1&&std::set<std::string>{"happy","angry","sad","relaxed","surprised","aa","ih","ou","ee","oh","blink","blinkLeft","blinkRight","lookUp","lookDown","lookLeft","lookRight","neutral"}.contains(entry.key()))throw std::invalid_argument("VRM1 custom expression collides with a preset name");
   const auto at=input[table]->find(entry.key());float value=at==input[table]->end()?0:std::clamp(at->second,0.f,1.f);
   const bool binary=entry.value().value("isBinary",false);if(binary)value=value>.5f?1:0;
   auto &weight=(*output[table])[entry.key()];weight=value;
   entries.push_back({&entry.value(),&weight,binary,table==0?category(entry.key()):-1});
  }
 }
 // All overrides use binary-normalized input, before any category attenuation.
 // Same-category overrides are invalid/ignored per the pinned specification.
 std::array<double,3> attenuation{};const std::array<const char*,3> modes{"overrideMouth","overrideBlink","overrideLookAt"};
 for(const auto &e:entries)for(size_t group=0;group<3;++group){
  stop();if(e.group==int(group)||!*e.weight)continue;
  const auto mode=e.definition->value(modes[group],std::string("none"));
  if(mode=="block")attenuation[group]+=1;else if(mode=="blend")attenuation[group]+=*e.weight;
 }
 for(const auto &e:entries){
  stop();if(e.group>=0){const double effect=std::clamp(attenuation[size_t(e.group)],0.,1.);*e.weight=e.binary&&effect>0?0:float(*e.weight*(1-effect));}
 }
 for(const auto &e:entries){
  const auto &expression=*e.definition;
  for(const auto &bind:expression.value("morphTargetBinds",nlohmann::json::array())){
   stop();auto &value=result.morphs[{bind.at("node").get<uint32_t>(),bind.at("index").get<uint32_t>()}];
   value+=bind.at("weight").get<float>()**e.weight;
   if(!std::isfinite(value))throw std::invalid_argument("VRM1 accumulated morph is nonfinite");
  }
  for(const auto &bind:expression.value("materialColorBinds",nlohmann::json::array())){
   stop();result.colors.push_back({bind.at("material").get<uint32_t>(),bind.at("type").get<std::string>(),bind.at("targetValue").get<std::array<float,4>>(),*e.weight});
  }
  for(const auto &bind:expression.value("textureTransformBinds",nlohmann::json::array())){
   stop();result.textures.push_back({bind.at("material").get<uint32_t>(),bind.value("scale",std::array<float,2>{1,1}),bind.value("offset",std::array<float,2>{0,0}),*e.weight});
  }
 }
 stop();return result;
}
}
