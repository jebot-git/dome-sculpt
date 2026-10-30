// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/normal-preparation.hpp"
#define main legacy_projection_policy_suite
#include "bake-projection-policy.cpp"
#undef main
int main()try{
  Fixture fixture;Document document;document.replace(fixture.scene);ready(document);const auto original=document.copy_scene().encode();Driver hand(document);hand.open();hand.confirm(false);hand.sector(9);
  check(document.frame().state->controls.parameter==VrParameter::BakeColors&&document.frame().state->controls.components==4,"Actual controller preparation policy exposes independent color/error and normal/error rows");
  check(!document.frame().state->bake_settings.prepare_normals&&document.frame().state->controls.value[3]==.08f,"Portable normal authoring is explicit opt-in with the declared default angular tolerance");
  hand.point(520,350);hand.point(200,420);hand.confirm(false);
  check(!document.frame().state->bake_settings.prepare_normals&&document.frame().state->bake_settings.normal_error==.02f&&document.copy_scene().encode()==original,"Controller Cancel discards normal interpretation/quality choices without mutating scene or history");
  hand.sector(9);hand.point(520,350);hand.point(200,420);hand.confirm(true);auto settings=document.frame().state->bake_settings;
  check(settings.prepare_normals&&!settings.prepare_colors&&settings.normal_error>.02f&&!settings.allow_partial&&!settings.allow_backfaces&&!document.frame().state->dirty&&!document.frame().state->can_undo,"Controller confirmation stages the independent normal quality policy while preserving color/projection defaults and history");
  hand.sector(9);hand.point(100,350);hand.f.focused=false;hand.send();
  check(document.frame().state->controls.parameter==VrParameter::None&&document.frame().state->bake_settings.prepare_normals&&document.copy_scene().encode()==original,"Controller focus loss discards only the unconfirmed normal policy");
  document.bake_from_object(fixture.full,settings);ready(document);check(document.frame().state->error.empty()&&document.copy_scene().active().material_bindings->normal_preparation&&document.copy_scene().active().painted->baked()->normal_interpretation==BakedNormalInterpretation::PreparedAttributes,"Actual accepted controller normal choices feed native preparation and same-hit dual normal bake");
  check(document.frame().state->message.find("normal attributes prepared")!=std::string::npos,"Native completion reports the explicit new normal interpretation and initial measured quantization");
  check(document.undo()&&document.copy_scene().encode()==original&&document.redo(),"Controller-configured normal authoring and bake remain one exact reversible action");
  std::cout<<checks<<" controller normal preparation checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
