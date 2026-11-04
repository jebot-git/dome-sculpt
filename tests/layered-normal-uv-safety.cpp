// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-uv-bake.hpp"
#include "baked-normal-field.hpp"
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
  bool refused=false;try{bake_source_uv(in);}catch(const std::exception &e){refused=std::string_view(e.what()).find("converted normal detail")!=std::string_view::npos;}
  check(refused&&scene.encode()==exact,"Changed UV producer cannot silently discard independent normal field and provenance");
  Document doc;doc.replace(scene);check(doc.wait_mesh(),"Actual converted native worker is ready");doc.begin_uv();check(doc.wait_uv(),"Actual converted mesh opens its existing UV workspace");
  doc.end_uv(true);check(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty()&&!doc.frame().state->uv,
    "Finishing untouched converted UV workspace succeeds as an actual no-op");
  check(doc.copy_scene().encode()==exact,"Untouched UV Apply preserves complete normal field/material/source and native data");
  doc.begin_uv();check(doc.wait_uv(),"Second UV workspace opens normally");
  doc.edit_uv(UvAction::Scale,.8f);doc.end_uv(true);check(doc.wait_asset(),"Unsupported changed UV request reaches a finite worker endpoint");
  check(!doc.frame().state->error.empty()&&doc.copy_scene().encode()==exact,"Actual changed UV worker refuses atomically instead of losing normal detail");
  doc.end_uv(false);check(!doc.frame().state->uv&&doc.copy_scene().encode()==exact,"Cancel after refused transfer closes the draft with original scene exact");
  check(!doc.undo()&&doc.copy_scene().encode()==exact,"UV no-op/refusal creates no false global history entry");
  std::cout<<"PASS "<<checks<<" actual independent normal UV preservation checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
