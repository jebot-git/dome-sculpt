// SPDX-License-Identifier: GPL-3.0-or-later
#include "canonical-paint-native.hpp"
#include "dome/document.hpp"
#include <iostream>
#include <filesystem>
using namespace dome;
namespace {size_t checks=0;void check(bool b,const char *s){++checks;if(!b)throw std::runtime_error(s);}void settle(Document &d){if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error("Native37: "+d.frame().state->error);}template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}}
int main(int argc,char **argv)try{
 if(argc!=3)return 2;const auto source=Scene::load(argv[1]);const auto original=source.encode();const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);Document d;d.replace(source);settle(d);
 check(detail::canonical_paint_retention_supported(source.active()),"Actual canonical actor has a supported owning paint transition");
 const auto revision=d.frame().state->revision;d.begin_uv();check(d.wait_uv(),"Initial actual UV workspace settles");auto view=d.frame().state->uv;
 check(view&&view->retain_paint_sampling&&view->retain_normal_sampling&&view->paint_sampling_required,"Supported canonical authoring explicitly keeps both fields by default");
 check(refuses([&]{d.edit_uv(UvAction::RetainPaint);}),"Required converted-normal writable basis cannot silently switch to lossy raster");d.end_uv(true);settle(d);check(d.copy_scene().encode()==original&&d.frame().state->revision==revision,"Untouched Accept is a genuine Scene/history no-op");
 nlohmann::json prior=original,current;for(int pass=0;pass<2;++pass){
  d.begin_uv();check(d.wait_uv(),"Actual repeated UV workspace is ready");d.edit_uv(UvAction::Scale,.8f);check(d.wait_uv(),"Actual scaled canonical UV draft validates");d.end_uv(true);auto saved=d.save_async(out/(std::to_string(pass)+".dsculpt"));saved.get();settle(d);current=d.copy_scene().encode();
  check(current.at("version")==37&&current.at("source_materials").at("version")==19,"Actual native UV worker atomically publishes genuine Scene37/material19");
  const auto o=d.copy_scene().active();const auto p=o.material_bindings->canonical_paint_sampling;check(p&&p->valid_owner(p)&&p->matches_current(*o.painted),"Actual current native holder has its factory-owned writable attachment");
  check(o.material_bindings->normal_frames==source.active().material_bindings->normal_frames&&o.painted->baked()==source.active().painted->baked()&&o.rig_source==source.active().rig_source&&o.shape_source==source.active().shape_source&&o.bake_recipe==source.active().bake_recipe,"Native UV changes neither current frames/normal field nor actual first rig/shape/recipe owners");
  check(Scene::load(out/(std::to_string(pass)+".dsculpt")).encode()==current,"Immediately queued Save waits for the actual accepted owning endpoint");
  d.undo();settle(d);check(d.copy_scene().encode()==prior,"Native UV Undo restores exact prior source/control/paint graph");d.redo();settle(d);check(d.copy_scene().encode()==current,"Native UV Redo restores exact genuine retained endpoint");prior=current;
 }
 d.begin_uv();check(d.wait_uv(),"Cancel starts from actual repeated retained domain");d.edit_uv(UvAction::Scale,.9f);check(d.wait_uv(),"Cancel uses a real changed workspace");d.end_uv(false);settle(d);check(d.copy_scene().encode()==current,"Changed workspace Cancel preserves exact source/normal/paint graph");
 Document reopened;reopened.replace(Scene::load(out/"1.dsculpt"));settle(reopened);reopened.begin_uv();check(reopened.wait_uv(),"Reopened owning UV workspace validates");check(reopened.frame().state->uv->can_restore_paint_source,"Real complete before-state exposes source Restore after restart");reopened.edit_uv(UvAction::RestorePaintSource);settle(reopened);check(reopened.copy_scene().encode()==original,"Actual source Restore returns complete original paint/model/material/rig/key/control graph");reopened.undo();settle(reopened);check(reopened.copy_scene().encode()==current,"Restore Undo recovers exact final retained graph");reopened.redo();settle(reopened);check(reopened.copy_scene().encode()==original,"Restore Redo recovers exact native original");
 check(source.encode()==original,"Actual native workflow never mutates caller's original source");std::cout<<"PASS "<<checks<<" actual canonical paint37 native controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
