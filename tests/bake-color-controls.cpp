// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/material-bindings.hpp"
#define main existing_projection_policy_suite
#include "bake-projection-policy.cpp"
#undef main
int main()try{
 Fixture f;Document doc;doc.replace(f.scene);ready(doc);auto original=doc.copy_scene().encode();Driver hand(doc);hand.open();hand.confirm(false);hand.sector(9);
 check(doc.frame().state->controls.parameter==VrParameter::BakeColors&&doc.frame().state->controls.components==2,"Actual controller Baking color-policy cell opens two explicit independent rows");
 check(!doc.frame().state->bake_settings.prepare_colors&&doc.frame().state->controls.value[1]==.04f,"Source-color preparation is opt-in with explicit default error tolerance");
 hand.point(520,240);hand.point(300,320);hand.confirm(false);check(!doc.frame().state->bake_settings.prepare_colors&&doc.frame().state->bake_settings.color_error==.04f&&doc.copy_scene().encode()==original,"Cancel discards preparation and error choices without touching retained data");
 hand.sector(9);hand.point(520,240);hand.point(300,320);hand.confirm(true);auto settings=doc.frame().state->bake_settings;
 check(settings.prepare_colors&&settings.color_error>.1f&&!settings.allow_partial&&!settings.allow_backfaces&&!doc.frame().state->dirty&&!doc.frame().state->can_undo,"Confirmation stages only explicit color configuration while preserving projection defaults and document history");
 hand.sector(9);hand.point(100,240);hand.f.focused=false;hand.send();check(doc.frame().state->controls.parameter==VrParameter::None&&doc.frame().state->bake_settings.prepare_colors&&doc.frame().state->bake_settings.color_error==settings.color_error&&doc.copy_scene().encode()==original,"Focus loss discards only unconfirmed color policy and preserves previously accepted settings");
 doc.bake_from_object(f.full,settings);ready(doc);check(doc.frame().state->error.empty()&&doc.copy_scene().active().material_bindings->color_preparation&&doc.copy_scene().active().painted->baked()->color_interpretation==BakedColorInterpretation::ResolvedPbr,"Actual accepted controller color choices feed native preparation and same-hit separate-PBR bake");
 check(doc.frame().state->message.find("color error bound")!=std::string::npos,"Native completion reports measured projection quality instead of hiding the color interpretation");
 check(doc.undo()&&doc.copy_scene().encode()==original&&doc.redo(),"Controller-configured color preparation and bake remain one reversible document action");
 std::cout<<checks<<" bake color controller checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
