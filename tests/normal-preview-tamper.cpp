// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_PAINT_SAMPLING_RENDER_NO_MAIN
#include "paint-sampling-render.cpp"
#undef DOME_PAINT_SAMPLING_RENDER_NO_MAIN
#include "dome/normal-preview.hpp"
int main()try {
 auto scene=retain_writable(joint::prepare(true,Material::AlphaMode::Mask));const auto original=scene.encode();const auto &object=scene.active();
 const auto &source=*object.material_bindings->normal_preparation->sampling_domain;
 // This actual allocation is mutable T, not const T. Keep its mutable alias
 // while independently authenticating the public fields through the real
 // worker factory. Do not copy the other node's private address-bound seal.
 auto node=std::make_shared<SourceNormalSampling>();node->receipt=source.receipt;node->layout=source.layout;node->projection=source.projection;node->before_paint=source.before_paint;node->before_appearance=source.before_appearance;node->before_deformation=source.before_deformation;node->before_material=source.before_material;node->sampling_material=source.sampling_material;node->current_sampling_reference=source.current_sampling_reference;node->metadata_sampling_domain=source.metadata_sampling_domain;
 detail::authenticate_normal_sampling_metadata(node);
 auto recipe=std::make_shared<SourceNormalPreparation>(*object.material_bindings->normal_preparation);recipe->sampling_domain=node;
 auto binding=std::make_shared<SourceMaterialBindings>(*object.material_bindings);binding->normal_preparation=recipe;
 const auto maps=object.painted->preview(object.material);auto proof=prepare_normal_preview(*object.painted,binding,object.material);
 check(node->has_sealed_metadata()&&proof&&proof->matches(binding,maps),"Actual mutable sampler allocation acquires its own independently replayed seal and cached preview proof");
 auto probe=[&](auto mutation){
  struct Restore{SourceNormalSampling &node;SourceNormalSampling value;~Restore(){node=std::move(value);}}restore{*node,*node};
  mutation(*node);check(!node->has_sealed_metadata()&&!proof->matches(binding,maps),"Same-address sampler scalar/owner mutation invalidates its independent seal and normal preview");
 };
 probe([](auto &n){++n.receipt.version;});probe([](auto &n){n.receipt.resolution*=2;});probe([](auto &n){++n.receipt.vertices;});probe([](auto &n){n.receipt.indices+=3;});probe([](auto &n){++n.receipt.correspondence_bytes;});probe([](auto &n){++n.receipt.max_working_bytes;});
 probe([](auto &n){n.layout=std::make_shared<const SourceNormalUvLayout>(*n.layout);});probe([](auto &n){n.projection=std::make_shared<const PaintNormalProjection>(*n.projection);});probe([](auto &n){n.before_paint=n.before_paint->snapshot();});probe([](auto &n){n.before_material=std::make_shared<const SourceMaterialBindings>(*n.before_material);});probe([](auto &n){n.sampling_material=std::make_shared<const SourceMaterialBindings>(*n.sampling_material);});probe([](auto &n){n.before_deformation=std::make_shared<const DeformationState>(*n.before_deformation);});probe([](auto &n){n.before_appearance=std::make_shared<const CreatorAppearance>();});probe([](auto &n){n.current_sampling_reference.emplace();});probe([](auto &n){n.metadata_sampling_domain.reset();});
 check(node->has_sealed_metadata()&&proof->matches(binding,maps),"Restoring the exact sampler fields restores its original actual proof without reminting");
 auto recipe_probe=[&](auto mutation){
  struct Restore{SourceNormalPreparation &recipe;SourceNormalPreparation value;~Restore(){recipe=std::move(value);}}restore{*recipe,*recipe};
  mutation(*recipe);check(!proof->matches(binding,maps),"Same-address recipe scalar/owner mutation invalidates its independently captured preview proof");
 };
 recipe_probe([](auto &r){++r.receipt.version;});recipe_probe([](auto &r){r.receipt.resolution*=2;});recipe_probe([](auto &r){r.receipt.authored_native_normals=!r.receipt.authored_native_normals;});recipe_probe([](auto &r){r.receipt.authored_native_tangents=!r.receipt.authored_native_tangents;});recipe_probe([](auto &r){r.receipt.accepted_angular_error+=.0001;});recipe_probe([](auto &r){r.receipt.observed_initial_error+=.0001;});recipe_probe([](auto &r){++r.receipt.sampled_texels;});
 recipe_probe([](auto &r){r.projection=std::make_shared<const PaintNormalProjection>(*r.projection);});recipe_probe([](auto &r){r.initial_projection=std::make_shared<const PaintNormalProjection>(*r.initial_projection);});recipe_probe([](auto &r){r.target_frames=std::make_shared<const NormalFrameSet>(*r.target_frames);});recipe_probe([](auto &r){r.original_paint=r.original_paint->snapshot();});recipe_probe([](auto &r){r.original_deformation=std::make_shared<const DeformationState>(*r.original_deformation);});recipe_probe([](auto &r){r.original_material=std::make_shared<const SourceMaterialBindings>(*r.original_material);});recipe_probe([](auto &r){r.original_appearance=std::make_shared<const CreatorAppearance>();});recipe_probe([](auto &r){r.uv_reprojection=std::make_shared<const SourceNormalUvReprojection>();});recipe_probe([](auto &r){r.sampling_domain.reset();});
 check(node->has_sealed_metadata()&&proof->matches(binding,maps)&&scene.encode()==original,"All same-address tamper attempts restore exact owners, cached preview and complete original Scene");
 std::cout<<"PASS "<<checks<<" independent same-address normal proof mutation checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
