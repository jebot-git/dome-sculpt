// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-anatomical-weights.hpp"
#include <iostream>
using namespace dome;using namespace dome::detail;
int main(int argc,char**argv)try{
 if(argc!=3)throw std::invalid_argument("authentic scene lease probe");auto scene=Scene::load(argv[1]);uint64_t reference=0,target=0;
 for(auto&o:scene.objects())if(o.deformation&&o.deformation->skeleton)reference=o.id;
 for(auto&o:scene.objects())if(o.id!=reference&&o.mesh_active&&o.painted&&o.appearance){target=o.id;break;}
 if(!reference||!target)throw std::invalid_argument("authentic current source required");auto&o=scene.object(target);const std::string test=argv[2];
 if(test=="paint-layer"){
  o.painted=o.painted->snapshot();const auto first=o.painted->active_layer();o.painted->add_layer("Lease counterfactual");const auto second=o.painted->active_layer();o.painted->select_layer(first);
  auto r=capture_native_anatomical(scene,std::span<const uint64_t>(&target,1),reference);if(!r.matches(scene))throw std::runtime_error("Initial genuine lease mismatch");const auto revision=o.painted->revision();o.painted->select_layer(second);
  if(o.painted->revision()!=revision)throw std::runtime_error("Counterfactual did not preserve local paint revision");if(r.matches(scene))throw std::runtime_error("Lease accepted changed current paint-layer selection at the same geometry/revision");
 }else if(test=="volume-layer"){
  o.volume=std::make_shared<Volume>(o.volume->geometry_snapshot());const auto first=o.volume->active_layer();const auto second=o.volume->add_layer("Lease counterfactual");o.volume->select_layer(first);
  auto r=capture_native_anatomical(scene,std::span<const uint64_t>(&target,1),reference);if(!r.matches(scene))throw std::runtime_error("Initial genuine lease mismatch");const auto revision=o.volume->revision();o.volume->select_layer(second);
  if(o.volume->revision()!=revision)throw std::runtime_error("Counterfactual did not preserve local sculpt revision");if(r.matches(scene))throw std::runtime_error("Lease accepted changed current sculpt-layer selection at the same geometry/revision");
 }else if(test=="mask-fork"){
  auto a=o.volume->geometry_snapshot(),b=o.volume->geometry_snapshot();a.fill_mask(1);b.fill_mask(.5f);o.volume=std::make_shared<Volume>(a);
  if(a.revision()!=b.revision()||a.geometry_owner()!=b.geometry_owner())throw std::runtime_error("Actual independent mask forks need equal geometry/revision");
  auto r=capture_native_anatomical(scene,std::span<const uint64_t>(&target,1),reference);if(!r.matches(scene))throw std::runtime_error("Initial genuine lease mismatch");*o.volume=b;
  if(r.matches(scene))throw std::runtime_error("Lease accepted different real mask fork at the same wrapper/geometry/revision");
 }else if(test=="signed-zero"){
  o.material.metallic=0;auto r=capture_native_anatomical(scene,std::span<const uint64_t>(&target,1),reference);if(!r.matches(scene))throw std::runtime_error("Initial genuine lease mismatch");o.material.metallic=-0.0f;
  if(r.matches(scene))throw std::runtime_error("Lease accepted changed retained material scalar bits");
 }else if(test=="hidden-fork"){
  o.painted=o.painted->snapshot();o.painted->add_layer("Hidden counterfactual");
  auto a=o.painted->snapshot(),b=o.painted->snapshot();Brush brush;brush.from=brush.to={0,0,0};brush.radius=2;PaintSettings settings;settings.front_only=false;
  a->begin_stroke();settings.color={1,0,0,1};a->stamp(brush,settings);if(!a->end_stroke())throw std::runtime_error("No real first hidden paint change");
  b->begin_stroke();settings.color={0,0,1,1};b->stamp(brush,settings);if(!b->end_stroke())throw std::runtime_error("No real second hidden paint change");
  a->layer_properties("Hidden counterfactual",false,1);b->layer_properties("Hidden counterfactual",false,1);
  if(a->revision()!=b->revision()||a->mesh_owner()!=b->mesh_owner())throw std::runtime_error("Actual independent hidden forks need equal geometry/revision");o.painted=a;
  auto r=capture_native_anatomical(scene,std::span<const uint64_t>(&target,1),reference);if(!r.matches(scene))throw std::runtime_error("Initial genuine lease mismatch");*a=*b;
  if(r.matches(scene))throw std::runtime_error("Lease accepted different real hidden paint fork at the same wrapper/geometry/revision");
 }else throw std::invalid_argument("unknown probe");
 std::cout<<"PASS authentic lease "<<test<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
