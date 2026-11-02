// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_normal_uv_document_suite
#include "normal-uv-document.cpp"
#undef main
namespace {
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
void begin(Document &doc){doc.begin_uv();check(doc.wait_uv(),"Keep-detail UV workspace initializes");}
void point(Hand &hand,float x,float y){auto state=hand.d.frame().state;auto &tracked=hand.f.hands[1].tracked;tracked.aimValid=true;tracked.aim.position=ob::transform(state->panel,{(x/640-.5f)*.6f,(.5f-y/640)*.6f,.25f});tracked.aim.orientation=state->panel.orientation;tracked.trigger=.8f;hand.send();tracked.trigger=0;hand.send();}
}
int main(int argc,char **argv)try{
 if(argc==3&&std::string_view(argv[1])=="--ui-fixture"){prepared_shape_fixture::model(true).save(argv[2]);return 0;}
 if(argc==3&&std::string_view(argv[1])=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
 if(argc!=2)return 2;const std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);const auto source=prepared_shape_fixture::model(true);const auto original=source.encode();
 Document doc;doc.replace(source);ready(doc);begin(doc);
 check(doc.frame().state->uv->normal_sampling_available&&!doc.frame().state->uv->retain_normal_sampling&&!doc.frame().state->uv->normal_sampling_required,"Historical prepared-normal projects keep raster sampling as the default");
 doc.edit_uv(UvAction::RetainNormals);check(doc.frame().state->uv->retain_normal_sampling&&doc.frame().state->message.find("native project")!=std::string::npos,"Explicit native Keep-detail choice publishes its actionable export limitation");
 doc.edit_uv(UvAction::Scale,.7f);check(doc.wait_uv(),"Keep-detail chart scale validates");doc.edit_uv(UvAction::Undo);check(doc.wait_uv()&&doc.frame().state->uv->retain_normal_sampling,"UV draft Undo preserves the explicit sampling choice");doc.edit_uv(UvAction::Redo);check(doc.wait_uv()&&doc.frame().state->uv->retain_normal_sampling,"UV draft Redo preserves the explicit sampling choice");
 doc.end_uv(false);check(doc.copy_scene().encode()==original,"Cancel discards Keep-detail draft configuration without changing the original model");
 begin(doc);check(!doc.frame().state->uv->retain_normal_sampling,"A cancelled choice does not silently change later default authoring");doc.edit_uv(UvAction::RetainNormals);doc.edit_uv(UvAction::Scale,.7f);check(doc.wait_uv(),"Native Keep-detail Scale finishes validation");
 ReleaseGate release;gate::arm();doc.end_uv(true);gate::wait();
 check(refuses([&]{doc.edit_uv(UvAction::RetainNormals);}),"Accepted UV job cannot race a changed normal sampling choice");
 auto saved=doc.save_async(directory/"native.dsculpt");gate::release();saved.get();ready(doc);const auto accepted=doc.copy_scene().encode();
 check(accepted.at("version")==21&&doc.copy_scene().active().material_bindings->normal_preparation->sampling_domain,"Actual native Apply captures the selected mode in the exact UV worker result");
 check(Scene::load(directory/"native.dsculpt").encode()==accepted,"Save queued behind Keep-detail Apply stores the complete owning result");
 check(doc.undo()&&doc.copy_scene().encode()==original&&doc.redo()&&doc.copy_scene().encode()==accepted,"Keep-detail Apply and Save have one exact global Undo/Redo boundary");ready(doc);
 Document restarted;restarted.open(directory/"native.dsculpt");ready(restarted);begin(restarted);check(restarted.frame().state->uv->retain_normal_sampling&&restarted.frame().state->uv->normal_sampling_required,"Restarted retained domain remains retained instead of silently reverting to raster semantics");
 check(refuses([&]{restarted.edit_uv(UvAction::RetainNormals);})&&restarted.frame().state->uv->retain_normal_sampling,"A later incompatible raster choice refuses atomically and retains its exact field");restarted.end_uv(false);
 {
  Document conversion;conversion.replace(source);ready(conversion);begin(conversion);conversion.edit_uv(UvAction::RetainNormals);conversion.end_uv(true);ready(conversion);
  check(conversion.copy_scene().encode().at("version")==21&&conversion.copy_scene().active().painted->mesh().uv==source.active().painted->mesh().uv,"An explicit Keep-detail conversion without chart movement is accepted as one source-retaining edit");
  check(conversion.undo()&&conversion.copy_scene().encode()==original,"Keep-detail-only conversion has an exact single Undo");
 }
 {
  Document controller;controller.replace(source);ready(controller);controller.configure_selection(true);Hand hand(controller);hand.open();check(controller.wait_uv(),"Actual joystick opens the shared Keep-detail UV workspace");point(hand,55,270);
  check(controller.frame().state->uv->retain_normal_sampling,"Actual controller ray activates the Keep-detail UV tool");point(hand,178,85);const auto before=controller.frame().state->uv->mesh->uv;hand.send(-1,0);hand.send();
  check(controller.wait_uv()&&controller.frame().state->uv->valid&&controller.frame().state->uv->mesh->uv!=before,"Controller axes scale the actual retained-normal UV draft");hand.send(0,0,true);hand.send();ready(controller);
  check(controller.copy_scene().encode().at("version")==21&&!controller.frame().state->uv,"Controller Confirm commits the same owning exact normal field");controller.save(directory/"controller.dsculpt");
  check(Scene::load(directory/"controller.dsculpt").encode()==controller.copy_scene().encode(),"Controller-authored Keep-detail state saves and restarts exactly");
 }
 {
  Document focus;focus.replace(source);ready(focus);Hand hand(focus);hand.open();check(focus.wait_uv(),"Focus-loss fixture initializes actual UV controls");point(hand,55,270);hand.f.focused=false;hand.send();
  check(!focus.frame().state->uv&&focus.copy_scene().encode()==original,"Controller focus loss cancels Keep-detail configuration and retains the complete original source");
 }
 check(source.encode()==original,"Every native/controller mode operation preserves immutable original paint/frame/pose/weights");std::cout<<"PASS "<<checks<<" Keep-detail native/controller checks\n";return 0;
}catch(const std::exception &error){gate::release();std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
