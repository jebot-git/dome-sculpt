// SPDX-License-Identifier: GPL-3.0-or-later
#include "key-derivation-codec.hpp"
#include "key-paint-adoption.hpp"
namespace dome::detail {
namespace {
uint64_t number(const nlohmann::json &j,bool positive=false){if(!j.is_number_unsigned()&&(!j.is_number_integer()||j.get<int64_t>()<0))throw std::invalid_argument("Invalid owning key identity");auto n=j.get<uint64_t>();if(positive&&!n)throw std::invalid_argument("Missing owning key target identity");return n;}
void stop(const std::function<bool()> &c){if(c&&c())throw std::runtime_error("Owning native key record cancelled");}
}
nlohmann::json KeyDerivationCodec::encode(const SourceKeyDerivation &node,NormalFrameArchive &frames,
    uint32_t paint,uint32_t before,CreatorAssetArchive &models,PaintImageArchive &images){
  node.validate();nlohmann::json output={{"version",1},{"paint",paint},{"before_binding",before},{"frames",frames.add(node.frames())},{"deformation",node.before_deformation()?node.before_deformation()->encode(node.before_paint()->mesh()):nlohmann::json(nullptr)},{"appearance",node.before_appearance()?encode_creator_appearance(*node.before_appearance(),models,images):nlohmann::json(nullptr)},{"targets",nlohmann::json::array()}};
  for(const auto &t:node.targets()){
    if(t.mirror_tolerance){if(output["version"]!=4)output["version"]=3;output["targets"].push_back({{"id",t.id},{"operation","mirror"},{"reference",*t.reference},{"tolerance",*t.mirror_tolerance}});if(t.mirror_conversion){output["version"]=4;auto &v=output["targets"].back();v["plane"]=t.mirror_conversion->plane;v["frame_policy"]=t.mirror_conversion->frames==ShapeKeyMirrorFrames::ExactAuthored?"exact-authored-v1":"base-relative-linear-v1";}}
    else if(t.reference&&*t.reference!=t.id){if(output["version"]!=3&&output["version"]!=4)output["version"]=2;
      if(*t.reference)output["targets"].push_back({{"id",t.id},{"operation","copy"},{"reference",*t.reference}});
      else output["targets"].push_back({{"id",t.id},{"operation","mix"},{"targets",node.before_material()->normal_frames->current_targets},{"weights",node.before_deformation()->morph_weights}});
    }else output["targets"].push_back({{"id",t.id},{"reference",t.reference?nlohmann::json(*t.reference):nlohmann::json(nullptr)}});
  }return output;
}
std::shared_ptr<const SourceKeyDerivation> KeyDerivationCodec::decode(const nlohmann::json &input,
    uint32_t current,const std::vector<std::shared_ptr<const SourceMaterialBindings>> &earlier,
    const PaintResolver &paint,KeyFrameArchive &frames,const CreatorAssetArchive &models,
    const std::function<bool()> &cancelled,bool explicit_mirror_capability){
  stop(cancelled);if(!input.is_object()||input.size()!=7||!input.at("version").is_number_integer()||(input.at("version")!=1&&input.at("version")!=2&&input.at("version")!=3&&!(explicit_mirror_capability&&input.at("version")==4))||!input.at("targets").is_array()||input.at("targets").empty()||input.at("targets").size()>128)throw std::invalid_argument("Unsupported owning native key record");
  const auto version=input.at("version").get<uint32_t>();const bool named=version>=2;bool has_operation=false,has_mirror=false,has_explicit_mirror=false;
  const auto before=number(input.at("before_binding"));const auto paint_id=number(input.at("paint"));const auto frame_id=number(input.at("frames"));if(before>=current||before>=earlier.size()||paint_id>=256||frame_id>=256||!paint)throw std::invalid_argument("Owning native key record must reference an actual earlier model");
  for(const auto&t:input.at("targets")){
    if(!t.is_object())throw std::invalid_argument("Invalid owning key target reference shape");number(t.at("id"),true);
    if(t.contains("operation")){has_operation=true;if(!named||!t.at("operation").is_string())throw std::invalid_argument("Named key operation requires owning record2");const auto &op=t.at("operation").get_ref<const std::string&>();
      if(op=="mirror"){has_mirror=true;const bool expanded=t.contains("plane")||t.contains("frame_policy");if((version!=3&&version!=4)||t.size()!=(expanded?6u:4u)||(expanded&&version!=4)||number(t.at("reference"),true)==number(t.at("id"),true)||!t.at("tolerance").is_number())throw std::invalid_argument("Mirror key requires owning record3 and a genuine different seed");const auto tolerance=t.at("tolerance").get<float>();if(!std::isfinite(tolerance)||tolerance<=0||tolerance>1)throw std::invalid_argument("Invalid explicit mirror local tolerance");if(expanded){if(!t.at("plane").is_number()||!std::isfinite(t.at("plane").get<double>())||std::abs(t.at("plane").get<double>())>1e6||!t.at("frame_policy").is_string()||(t.at("frame_policy")!="exact-authored-v1"&&t.at("frame_policy")!="base-relative-linear-v1")||(t.at("frame_policy")=="exact-authored-v1"&&t.at("plane").get<double>()==0))throw std::invalid_argument("Invalid explicit Mirror record4 plane or conversion");has_explicit_mirror=true;}}
      else if(op=="copy"){if(t.size()!=3||number(t.at("reference"),true)==number(t.at("id"),true))throw std::invalid_argument("Copy key record requires a different genuine seed target");}
      else if(op=="mix"){if(t.size()!=4||!t.at("targets").is_array()||!t.at("weights").is_array()||t.at("targets").size()>128||t.at("weights").size()>128)throw std::invalid_argument("Invalid bounded captured mix record");for(const auto &id:t.at("targets"))number(id,true);for(const auto &w:t.at("weights"))if(!w.is_number()||!std::isfinite(w.get<float>()))throw std::invalid_argument("Invalid finite captured mix weight");}
      else throw std::invalid_argument("Unsupported named key operation");
    }else{if(t.size()!=2)throw std::invalid_argument("Invalid owning key target reference shape");if(!t.at("reference").is_null()&&number(t.at("reference"),true)!=number(t.at("id"),true))throw std::invalid_argument("Copy key seed requires an explicit record2 operation");}
  }
  if(named&&!has_operation)throw std::invalid_argument("Record2 requires an actual named key operation");if(version==3&&!has_mirror)throw std::invalid_argument("Record3 requires an actual mirrored key operation");if(version==4&&!has_explicit_mirror)throw std::invalid_argument("Record4 requires actual explicit plane or converted artist-frame semantics");
  auto node=std::shared_ptr<SourceKeyDerivation>(new SourceKeyDerivation);node->owner_=node;node->material_=earlier.at(before);if(!node->material_)throw std::invalid_argument("Owning key record has an absent before material");
  const auto source=paint(uint32_t(paint_id),node->material_);if(!source)throw std::invalid_argument("Owning key record has an absent before Paint");node->paint_=source->snapshot();
  if(!input.at("deformation").is_null())node->deformation_=std::make_shared<const DeformationState>(DeformationState::decode(input.at("deformation"),node->paint_->mesh()));
  if(!input.at("appearance").is_null())node->appearance_=decode_creator_appearance(input.at("appearance"),models);
  if(node->material_->key_derivation)node->paint_=adopt_key_paint_geometry(*node->paint_,*node->material_,node->appearance_.get(),node->deformation_.get(),cancelled);
  if(const auto domain=frames.references_.at(size_t(frame_id));domain&&frames.input_.at("position_domains").at(*domain).at("version")==2)
    node->paint_=adopt_key_source_geometry(*node->paint_,*node->material_,node->appearance_.get(),node->deformation_.get(),cancelled);
  auto staged=frames;node->frames_=staged.bind(uint32_t(frame_id),node->paint_->mesh_owner(),*node->material_,cancelled);node->domain_=node->frames_->position_domain;node->current_targets_=node->frames_->current_targets;node->next_target_=node->frames_->next_target;
  for(const auto&t:input.at("targets")){
    std::optional<uint64_t> reference;
    if(t.contains("operation")&&t.at("operation").get_ref<const std::string&>()=="mix"){
      if(!node->deformation_||t.at("targets").get<std::vector<uint64_t>>()!=node->material_->normal_frames->current_targets||t.at("weights").get<std::vector<float>>()!=node->deformation_->morph_weights)throw std::invalid_argument("Captured mix differs from actual before target/weight authority");reference=0;
    }else if(!t.at("reference").is_null())reference=number(t.at("reference"),true);
    node->targets_.push_back({number(t.at("id"),true),reference});if(t.contains("operation")&&t.at("operation").get_ref<const std::string&>()=="mirror"){node->targets_.back().mirror_tolerance=t.at("tolerance").get<float>();if(t.contains("plane"))node->targets_.back().mirror_conversion=DerivedMirrorConversion{t.at("plane").get<double>(),t.at("frame_policy")=="exact-authored-v1"?ShapeKeyMirrorFrames::ExactAuthored:ShapeKeyMirrorFrames::BaseRelative};}
  }
  node->validate(true,cancelled);stop(cancelled);std::swap(frames.frames_,staged.frames_);frames.domains_.swap(staged.domains_);frames.used_.swap(staged.used_);return node;
}
} // namespace dome::detail
