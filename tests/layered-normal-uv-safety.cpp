// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-uv-bake.hpp"
#include "baked-normal-field.hpp"
#include "canonical-normal-uv.hpp"
#include <iostream>
#include <numeric>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool yes,const char *label){if(!yes)throw std::runtime_error(label);++checks;std::cout<<"PASS "<<label<<'\n';}
}
int main(int argc,char **argv)try{
  if(argc!=2)return 2;const auto scene=Scene::load(argv[1]);const auto exact=scene.encode();const auto &o=scene.active();
  SourceUvBakeInput in;in.paint=o.painted;in.current_material=o.material_bindings;in.current_deformation=o.deformation;in.appearance=o.appearance;in.target=o.painted->mesh();
  in.source_vertices.resize(in.target.positions.size());std::iota(in.source_vertices.begin(),in.source_vertices.end(),0);
  for(auto &uv:in.target.uv){uv[0]=uv[0]*.8f+.1f;uv[1]=uv[1]*.8f+.1f;}
  const auto adopted=adopt_source_uv_bake(bake_source_uv(in));
  check(adopted.material_bindings->canonical_uv&&adopted.paint->baked()->normal_field==o.painted->baked()->normal_field&&scene.encode()==exact,
    "Changed UV producer preserves independent normal field and authentic new correspondence");
  Document doc;doc.replace(scene);check(doc.wait_mesh(),"Actual converted native worker is ready");doc.begin_uv();check(doc.wait_uv(),"Actual converted mesh opens its existing UV workspace");
  doc.end_uv(true);check(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty()&&!doc.frame().state->uv,
    "Finishing untouched converted UV workspace succeeds as an actual no-op");
  check(doc.copy_scene().encode()==exact,"Untouched UV Apply preserves complete normal field/material/source and native data");
  doc.begin_uv();check(doc.wait_uv(),"Second UV workspace opens normally");
  doc.edit_uv(UvAction::Scale,.8f);doc.end_uv(true);check(doc.wait_asset()&&doc.wait_mesh(),"Actual changed UV worker reaches a finite endpoint");
  const auto changed=doc.copy_scene().encode();
  check(doc.frame().state->error.empty()&&changed!=exact&&doc.copy_scene().active().material_bindings->canonical_uv,
    "Actual UV Apply retains authentic original normal coordinates");
  check(doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==exact,"One UV Undo restores original whole Scene without no-op history");
  check(!doc.undo()&&doc.copy_scene().encode()==exact,"Untouched UV Done did not create false global history");
  std::cout<<"PASS "<<checks<<" actual independent normal UV preservation checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
