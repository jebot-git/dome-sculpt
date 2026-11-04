// SPDX-License-Identifier: GPL-3.0-or-later
#include "bake-recipe-capabilities.hpp"
#define DOME_PAINT_SAMPLING_DOCUMENT_NO_MAIN
#include "paint-sampling-document.cpp"
#undef DOME_PAINT_SAMPLING_DOCUMENT_NO_MAIN
namespace {
void lossless_draft(Document &doc){
 begin(doc);doc.edit_uv(UvAction::RetainNormals);
 check(doc.frame().state->uv->retain_normal_sampling&&!doc.frame().state->uv->retain_paint_sampling&&doc.frame().state->message.find("Paint and normal detail")!=std::string::npos,"Existing Keep normal detail action explicitly announces lossless paint preservation without changing the separate draft option");
 auto p=front(doc.copy_scene().active().painted->mesh());doc.edit_uv(UvAction::Pick,p[0],p[1]);doc.edit_uv(UvAction::Scale,.7f);doc.edit_uv(UvAction::Rotate,1.57079632679f);
 check(doc.wait_uv()&&doc.frame().state->uv->valid,"Original Keep-normal command validates its actual fractional scale/turn draft");
}
void lossless_accept(Document &doc,bool fail=false){lossless_draft(doc);paint_gate::arm(fail);doc.end_uv(true);paint_gate::wait();}
SourceUvBakeInput detached(const Scene &scene){
 const auto &o=scene.active();UvWorkspace uv(o.painted->mesh(),256);auto p=front(o.painted->mesh());uv.edit(UvAction::Pick,p[0],p[1]);uv.edit(UvAction::Scale,.7f);uv.edit(UvAction::Rotate,1.57079632679f);
 auto v=uv.view();auto derivative=prepare_polygon_uv_derivative(o,*v.mesh,*v.source_vertices,{});
 SourceUvBakeInput input;input.paint=o.painted;input.current_material=o.material_bindings;input.current_deformation=o.deformation;input.polygon_authority=o.polygon;input.target_polygon=derivative.authority;input.target=derivative.mesh;input.source_vertices=derivative.source_vertices;return input;
}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;const std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);
 auto source=prepared(true,true);source.active().material.alpha=Material::AlphaMode::Opaque;
 auto d=*source.active().deformation;d.pose.resize(2);d.pose[0].scale={1.2f,.8f,.7f};d.pose[1].translation={0,.1f,0};d.morph_weights[0]=.35f;source.set_deformation(source.active().id,std::move(d));
 const auto exact=source.encode();const auto pixels=rgba(*source.active().painted);
 auto input=detached(source);SourceUvBakeOptions options;options.normal_sampling=SourceNormalUvMethod::RetainSource;
 auto result=bake_source_uv(input,options);auto adopted=adopt_source_uv_bake(result);
 check(adopted.paint->paint_sampling_domain()&&result.color_reprojection->paint_sampling&&result.normal_sampling->metadata_sampling_domain==adopted.paint->paint_sampling_domain(),"Original default-resolution transfer owns independent exact paint and normal sampling proofs");
 check(rgba(*adopted.paint)==pixels,"Partial Bake and every editable colour/coverage/ORM/emission grid remain byte exact");
 options.resolution=256;auto explicit_raster=bake_source_uv(input,options);
 check(explicit_raster.color_reprojection->uv_reprojection&&!explicit_raster.color_reprojection->paint_sampling,"Explicit resolution preserves genuine raster Scene23 transfer semantics");
 options.resolution=0;options.max_layer_bytes=1;check(refuses([&]{bake_source_uv(input,options);}),"Lossless default respects the existing caller storage quota");options.max_layer_bytes=128*1024*1024;
 for(int quota=0;quota<4;++quota){auto bounded=options;switch(quota){case 0:bounded.max_raster_samples=1;break;case 1:bounded.max_sample_operations=1;break;case 2:bounded.max_working_bytes=1;break;case 3:bounded.max_output_bytes=1;break;}check(refuses([&]{bake_source_uv(input,bounded);}),"Lossless source transfer respects caller raster/work/scratch/image quotas before publication");}
 options.cancelled=[]{return true;};check(refuses([&]{bake_source_uv(input,options);}),"Cancelled lossless preparation publishes no detached result");options.cancelled={};
 auto bad=result;auto maps=std::make_shared<PaintSourceMaps>(*bad.maps);bad.maps=maps;
 check(result.maps&&result.maps->empty()&&refuses([&]{adopt_source_uv_bake(bad);}),"An equal-valued foreign empty role table cannot replace the authentic retained source owner");
 maps->color.image=std::make_shared<const Image>(Image{1,1,{255,0,0,255}});
 check(refuses([&]{adopt_source_uv_bake(bad);}),"A newly supplied source image cannot borrow retained coordinate authority");
 bad=result;auto baked=std::make_shared<BakedTextures>(*bad.baked);baked->maps.color.rgba[0]^=1;bad.baked=baked;
 check(refuses([&]{adopt_source_uv_bake(bad);}),"Modified baked pixels cannot borrow a lossless source receipt");
 auto kept=source.snapshot();kept.active().painted=adopted.paint;kept.active().material_bindings=adopted.material_bindings;kept.active().deformation=adopted.deformation;kept.active().polygon=input.target_polygon;kept.validate();
 auto kept_input=detached(kept);auto resize=options;resize.resolution=256;
 check(refuses([&]{bake_source_uv(kept_input,resize);}),"Existing retained detail cannot be silently rasterized by a resolution request without its required compiler");
 check(source.encode()==exact,"Detached positive, quota, cancellation and tamper attempts preserve the complete input");
 {
  Document doc;paint_gate::Release release;doc.replace(source);ready(doc);lossless_accept(doc);auto path=directory/"accepted.dsculpt";auto f=doc.save_async(path);
  check(f.wait_for(std::chrono::milliseconds(20))!=std::future_status::ready&&refuses([&]{doc.edit_uv(UvAction::RetainPaint);}),"Accepted old command seals its transfer policy and exact ordered Save endpoint");paint_gate::release();saved(f);ready(doc);
  auto accepted=doc.copy_scene();auto encoded=accepted.encode();
  check(bake_recipe_fixture::scene30(accepted,24,encoded)&&Scene::load(path).encode()==encoded&&rgba(*accepted.active().painted)==pixels,"Actual Keep-normal Apply and immediate Save/restart retain the unchanged writable paint basis");
  check(accepted.active().deformation->encode(accepted.active().painted->mesh())==source.active().deformation->encode(source.active().painted->mesh()),"Lossless native operation preserves current pose, skin and morph state");
  check(doc.undo()&&doc.copy_scene().encode()==exact&&doc.redo()&&doc.copy_scene().encode()==encoded,"Original command has one exact source/paint/normal/history Undo and Redo");
  ready(doc);doc.export_asset(directory/"accepted.glb").get();check(std::filesystem::file_size(directory/"accepted.glb")>100,"Actual restarted retained-grid state emits its current GLB through the cancellable file worker");
 }
 for(int mode=0;mode<3;++mode){
  Document doc;paint_gate::Release release;doc.replace(source);ready(doc);auto path=directory/("failure-"+std::to_string(mode)+".dsculpt");source.save(path);const auto bytes=file_bytes(path);
  lossless_accept(doc,mode==1);auto f=doc.save_async(path);Scene replacement;
  if(mode==0)doc.cancel_background_work();if(mode==2){replacement=source.snapshot();replacement.active().name="Replacement";doc.replace(replacement);}paint_gate::release();
  check(f.wait_for(std::chrono::seconds(60))==std::future_status::ready,"Cancelled, stale or failed original-command worker releases its dependent Save");bool refused=false,typed=false;try{f.get();}catch(const ExactPaintFailure&){refused=typed=true;}catch(const std::exception&){refused=true;}
  check(refused&&(mode!=1||typed)&&file_bytes(path)==bytes&&doc.wait_asset()&&doc.copy_scene().encode()==(mode==2?replacement.encode():exact),"Exact worker failure/cancel/stale generation preserves original output, owner graph and history");
  if(mode==1){check(doc.frame().state->uv&&!doc.frame().state->uv->committing,"Factory failure retains the same ready draft");doc.end_uv(true);ready(doc);check(bake_recipe_fixture::scene30(doc.copy_scene(),24)&&doc.undo()&&doc.copy_scene().encode()==exact,"Same native Keep-normal draft retries successfully with one Undo");}
 }
 {
  Document doc;doc.replace(source);ready(doc);doc.configure_selection(true);Hand hand(doc);hand.open();check(doc.wait_uv()&&doc.frame().state->uv,"Actual selection-on joystick opens original prepared UV workflow");
  hand.point(55,270);check(doc.frame().state->uv->retain_normal_sampling&&!doc.frame().state->uv->retain_paint_sampling,"Actual controller ray chooses original Keep-normal action");
  hand.point(178,85);auto before=doc.frame().state->uv->mesh->uv;hand.send(-1,0);hand.send();check(doc.wait_uv()&&doc.frame().state->uv->valid&&doc.frame().state->uv->mesh->uv!=before,"Real joystick edits UVs under captured lossless policy");
  hand.send(0,0,true);hand.send();ready(doc);auto changed=doc.copy_scene();check(!doc.frame().state->uv&&changed.active().painted->paint_sampling_domain()&&rgba(*changed.active().painted)==pixels,"Controller Confirm preserves actual partial-Bake paint and independent normal detail");
  auto path=directory/"controller.dsculpt";doc.save_async(path).get();check(Scene::load(path).encode()==changed.encode()&&doc.undo()&&doc.copy_scene().encode()==exact,"Controller result survives restart and exact one-step Undo");
 }
 check(source.encode()==exact,"Every actual worker/controller attempt retains original source data");std::cout<<"PASS "<<checks<<" lossless original-command checks\n";return 0;
}catch(const std::exception&e){paint_gate::release();std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
