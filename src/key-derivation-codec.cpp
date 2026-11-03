// SPDX-License-Identifier: GPL-3.0-or-later
#include "key-derivation-codec.hpp"
namespace dome::detail {
namespace {
uint64_t number(const nlohmann::json &j,bool positive=false){if(!j.is_number_unsigned()&&(!j.is_number_integer()||j.get<int64_t>()<0))throw std::invalid_argument("Invalid owning key identity");auto n=j.get<uint64_t>();if(positive&&!n)throw std::invalid_argument("Missing owning key target identity");return n;}
void stop(const std::function<bool()> &c){if(c&&c())throw std::runtime_error("Owning native key record cancelled");}
}
nlohmann::json KeyDerivationCodec::encode(const SourceKeyDerivation &node,NormalFrameArchive &frames,
    uint32_t paint,uint32_t before,CreatorAssetArchive &models,PaintImageArchive &images){
  node.validate();nlohmann::json output={{"version",1},{"paint",paint},{"before_binding",before},{"frames",frames.add(node.frames())},{"deformation",node.before_deformation()?node.before_deformation()->encode(node.before_paint()->mesh()):nlohmann::json(nullptr)},{"appearance",node.before_appearance()?encode_creator_appearance(*node.before_appearance(),models,images):nlohmann::json(nullptr)},{"targets",nlohmann::json::array()}};
  for(const auto &t:node.targets())output["targets"].push_back({{"id",t.id},{"reference",t.reference?nlohmann::json(*t.reference):nlohmann::json(nullptr)}});return output;
}
std::shared_ptr<const SourceKeyDerivation> KeyDerivationCodec::decode(const nlohmann::json &input,
    uint32_t current,const std::vector<std::shared_ptr<const SourceMaterialBindings>> &earlier,
    const PaintResolver &paint,KeyFrameArchive &frames,const CreatorAssetArchive &models,
    const std::function<bool()> &cancelled){
  stop(cancelled);if(!input.is_object()||input.size()!=7||!input.at("version").is_number_integer()||input.at("version")!=1||!input.at("targets").is_array()||input.at("targets").size()>128)throw std::invalid_argument("Unsupported owning native key record");
  const auto before=number(input.at("before_binding"));const auto paint_id=number(input.at("paint"));const auto frame_id=number(input.at("frames"));if(before>=current||before>=earlier.size()||paint_id>=256||frame_id>=256||!paint)throw std::invalid_argument("Owning native key record must reference an actual earlier model");
  for(const auto&t:input.at("targets")){if(!t.is_object()||t.size()!=2)throw std::invalid_argument("Invalid owning key target reference shape");number(t.at("id"),true);if(!t.at("reference").is_null())number(t.at("reference"),true);}
  auto node=std::shared_ptr<SourceKeyDerivation>(new SourceKeyDerivation);node->owner_=node;node->material_=earlier.at(before);if(!node->material_)throw std::invalid_argument("Owning key record has an absent before material");
  const auto source=paint(uint32_t(paint_id),node->material_);if(!source)throw std::invalid_argument("Owning key record has an absent before Paint");node->paint_=source->snapshot();
  if(!input.at("deformation").is_null())node->deformation_=std::make_shared<const DeformationState>(DeformationState::decode(input.at("deformation"),node->paint_->mesh()));
  if(!input.at("appearance").is_null())node->appearance_=decode_creator_appearance(input.at("appearance"),models);
  auto staged=frames;node->frames_=staged.bind(uint32_t(frame_id),node->paint_->mesh_owner(),*node->material_,cancelled);node->domain_=node->frames_->position_domain;node->current_targets_=node->frames_->current_targets;node->next_target_=node->frames_->next_target;
  for(const auto&t:input.at("targets"))node->targets_.push_back({number(t.at("id"),true),t.at("reference").is_null()?std::optional<uint64_t>{}:std::optional<uint64_t>{number(t.at("reference"),true)}});
  node->validate(true,cancelled);stop(cancelled);std::swap(frames.frames_,staged.frames_);frames.domains_.swap(staged.domains_);frames.used_.swap(staged.used_);return node;
}
} // namespace dome::detail
