// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_CANONICAL_CONVERSION_HELPERS_ONLY
#include "canonical-normal-conversion.cpp"
#include "canonical-normal-owner.hpp"
#include "dome/shapekey-authoring.hpp"
#include "dome/scene-file.hpp"
namespace {
Scene legacy_conversion_model(int variant){
 auto s=baked_fan(variant&1,variant&2,variant&4);
 if(variant&8){auto paint=s.active().painted->snapshot();auto baked=*paint->baked();
  for(size_t at=0;at<baked.coverage.size();++at)if(baked.coverage[at])baked.coverage[at]=at%3==0?0:at%3==1?127:255;
  paint->clear_bake();paint->install_bake(std::move(baked));s.active().painted=std::move(paint);
 }
 auto paint=s.active().painted->snapshot();Brush brush;brush.from=brush.to={.025f,.025f,0};brush.normal={0,0,1};brush.radius=.09f;brush.strength=.4f;
 PaintSettings ink;ink.color={.12f,.4f,.75f,.8f};paint->begin_stroke();paint->stamp(brush,ink);
 require_cage(paint->end_stroke(),"Actual native conversion source includes editable sparse paint");
 paint->add_layer("Hidden source-preserving layer");paint->layer_properties("Hidden source-preserving layer",false,.37f);s.active().painted=std::move(paint);s.validate();return s;
}
void settled(Document &doc){
 if(!doc.wait_asset(std::chrono::seconds(120))||!doc.wait_mesh(std::chrono::seconds(120))||!doc.frame().state->error.empty())
  throw std::runtime_error("Canonical conversion native worker: "+doc.frame().state->error);
}
void unchanged_paint_channels(const PaintedMesh &before,const PaintedMesh &after){
 const auto a=before.baked(),b=after.baked();
 require_cage(after.mesh_owner()==before.mesh_owner()&&after.source_maps()==before.source_maps()&&
   a->coverage==b->coverage&&a->maps.color.rgba==b->maps.color.rgba&&a->maps.metal_rough.rgba==b->maps.metal_rough.rgba&&
   a->maps.emission.rgba==b->maps.emission.rgba&&a->maps.pbr_color.rgba==b->maps.pbr_color.rgba&&a->color_interpretation==b->color_interpretation,
   "Explicit conversion preserves exact mesh/source owners, baked material channels and fractional/protected coverage");
 bool same=before.layers().size()==after.layers().size()&&before.active_layer()==after.active_layer();
 for(size_t l=0;l<before.layers().size()&&same;++l){const auto &x=before.layers()[l],&y=after.layers()[l];
  same=x.id==y.id&&x.name==y.name&&x.visible==y.visible&&x.opacity==y.opacity&&x.tiles==y.tiles&&x.pbr_tiles==y.pbr_tiles&&x.emission_tiles==y.emission_tiles;
 }
 require_cage(same,"Conversion shares every visible/hidden editable sparse tile and all layer properties without flattening");
 require_cage(b->maps.normal.rgba.empty()&&b->normal_interpretation==BakedNormalInterpretation::PreparedAttributes&&after.normal_projection(),
   "Only the historical normal role moves into the explicit prepared attribute model");
}
void converted_model(Scene &scene){
 auto &o=scene.active();auto r=convert_canonical_normals({o.painted,o.appearance,o.material_bindings,o.deformation,o.material,{}});
 o.painted=std::move(r.paint);o.material_bindings=std::move(r.material_bindings);scene.validate();
}
void assert_owned(const Scene &before,const Scene &after){
 const auto &o=after.active();const auto p=o.material_bindings->normal_preparation;const auto c=p->canonical_conversion;
 require_cage(c&&c->valid_owner(c)&&p->original_paint==c->before_paint()&&p->original_deformation==c->before_deformation()&&
   p->original_paint->encode()==before.active().painted->encode(),"Opaque conversion lease retains exact complete historical canonical paint rather than relabeling its bytes");
 require_cage(after.active().deformation==before.active().deformation&&after.active().bake_recipe==before.active().bake_recipe&&
   cage_sources(after)==cage_sources(before),"Conversion preserves current native skin/morph, saved bake recipe and every hidden source exactly");
 unchanged_paint_channels(*before.active().painted,*after.active().painted);
}
}
#ifndef DOME_CANONICAL_OWNING_HELPERS_ONLY
int main(int argc,char **argv)try{
 if(argc==3&&std::string_view(argv[1])=="--ui-fixture"){legacy_conversion_model(7).save(argv[2]);return 0;}
 if(argc==3&&std::string_view(argv[1])=="--inspect"){
  auto s=Scene::load(argv[2]);auto p=s.active().material_bindings?s.active().material_bindings->normal_preparation:nullptr;
  std::cout<<nlohmann::json{{"scene",s.encode()},{"converted",bool(p&&p->canonical_conversion)}}.dump()<<'\n';return 0;
 }
 const std::filesystem::path out=argc>1?argv[1]:"canonical-normal-owning";std::filesystem::create_directories(out);
 for(int variant=0;variant<16;++variant){auto before=legacy_conversion_model(variant);const auto exact=before.encode();auto scene=before;converted_model(scene);assert_owned(before,scene);
  const auto encoded=scene.encode();const auto &materials=encoded.at("source_materials");
  require_cage(encoded.at("version")==31&&materials.at("version")==13&&materials.at("base_version")==4&&materials.at("canonical_conversions").size()==1&&
    materials.at("normal_preparations")[0].at("version")==7&&materials.at("normal_preparations")[0].at("base_version")==1,
    "Actual writer emits Scene31/material13/normal-entry7 only with a genuine replayable conversion anchor");
  auto recovered=Scene::decode(encoded);require_cage(recovered.encode()==encoded,"Whole owning Scene decoder replays conversion before restoring exact shared normal authority");
  before.save(out/("original-"+std::to_string(variant)+".dsculpt"));scene.save(out/("converted-"+std::to_string(variant)+".dsculpt"));
  require_cage(Scene::load(out/("converted-"+std::to_string(variant)+".dsculpt")).encode()==encoded&&before.encode()==exact,
    "Actual native Save/restart preserves converted and complete original data with the input Scene unchanged");
  if(variant==3){auto mirrored=recovered;ShapeKeyEdit mirror;mirror.action=ShapeKeyAction::Mirror;mirror.target=0;mirror.name="Converted explicit Mirror";mirror.mirror_frames=ShapeKeyMirrorFrames::BaseRelative;
   author_shapekey(mirrored,mirrored.active().id,mirror);const auto wire=mirrored.encode();
   require_cage(wire.at("version")==31&&wire.at("source_materials").at("version")==13&&wire.at("source_materials").at("explicit_mirror")==true&&Scene::decode(wire).encode()==wire,
     "Explicit Mirror and canonical conversion compose actual material capabilities and shared source leases through restart");
   mirrored.save(out/"converted-mirror.dsculpt");
  }
  if(variant==7){
   auto bad=encoded;bad["version"]=30;require_cage(failure([&]{Scene::decode(bad);},"Scene31"),"Scene30 cannot acquire conversion authority by relabeling outer data");
   bad=encoded;bad["source_materials"]["canonical_conversions"][0]["receipt"]["observed_center_error"]=.2;
   require_cage(failure([&]{Scene::decode(bad);},"replay"),"Forged conversion quality data fails actual retained-source replay");
   bad=encoded;bad["source_materials"]["canonical_conversions"][0]["anchor"]=123;
   require_cage(failure([&]{Scene::decode(bad);},"identity"),"Foreign conversion anchor is rejected before publication");
   bad=encoded;const auto pid=materials.at("normal_preparations")[0].at("paint").get<size_t>();
   const auto raw=scene.active().material_bindings->normal_preparation->original_paint;
   const auto &coverage=raw->baked()->coverage;const auto at=size_t(std::find(coverage.begin(),coverage.end(),255)-coverage.begin());
   bad["source_materials"]["paints"][pid]["baked"]["normal"].get_binary().at(at*4)^=64;
   require_cage(failure([&]{Scene::decode(bad);},"replay"),"Changed real historical normal pixels cannot borrow the earlier converted output");
   auto forged=scene;auto binding=std::make_shared<SourceMaterialBindings>(*forged.active().material_bindings);auto prep=std::make_shared<SourceNormalPreparation>(*binding->normal_preparation);prep->canonical_conversion.reset();binding->normal_preparation=std::move(prep);forged.active().material_bindings=std::move(binding);
   require_cage(failure([&]{forged.validate();},"Historical normal"),"Removing opaque conversion ownership restores the original historical-normal refusal");
   ShapeKeyEdit edit;edit.action=ShapeKeyAction::Create;edit.name="Converted new key";author_shapekey(recovered,recovered.active().id,edit);
   edit.action=ShapeKeyAction::Replace;edit.target=recovered.active().deformation->morphs->targets.size()-1;edit.positions=recovered.active().painted->mesh().positions;edit.positions[1].z+=.007f;edit.positions[3].z-=.006f;author_shapekey(recovered,recovered.active().id,edit);
   const auto shaped=recovered.encode();require_cage(Scene::decode(shaped).encode()==shaped&&recovered.active().material_bindings->normal_preparation->canonical_conversion,
     "New/Replace shapekeys retain explicit converted planes and actual owning frame derivations through restart");
   recovered.save(out/"converted-key.dsculpt");
  }
 }
 auto before=legacy_conversion_model(7);const auto exact=before.encode();Document doc;doc.replace(before);settled(doc);
 require_cage(doc.frame().state->can_convert_bake,"Actual native snapshot enables explicit conversion for its eligible target");
 doc.convert_baked_normals();auto save=doc.save_async(out/"worker.dsculpt");save.get();settled(doc);const auto accepted=doc.copy_scene().encode();
 require_cage(!doc.frame().state->can_convert_bake&&Scene::load(out/"worker.dsculpt").encode()==accepted,
   "Actual Document conversion worker publishes once and ordered Save waits for the complete owning result");
 require_cage(doc.undo()&&doc.copy_scene().encode()==exact&&doc.redo()&&doc.copy_scene().encode()==accepted,
   "Public conversion is one exact Undo/Redo transaction including historical data and bake recipes");
 doc.replace(before);settled(doc);Driver driver(doc);driver.open();driver.sector(0);
 require_cage(doc.frame().state->controls.parameter==VrParameter::BakeSource,"Actual controller reaches the existing source/cage panel");
 driver.ray(.5f,492.f/640);settled(doc);
 require_cage(doc.frame().state->controls.parameter==VrParameter::None&&doc.copy_scene().encode()==accepted,
   "Direct controller conversion uses the same native worker and closes its staged source controls");
 const auto controller=doc.copy_scene().encode();driver.send(0,0,false,.8f);driver.send(0,0,false,.8f);driver.send();settled(doc);
 require_cage(doc.copy_scene().encode()==controller,"Held trigger after controller conversion cannot leak a paint or sculpt stroke");
 doc.replace(before);settled(doc);doc.save(out/"late-cancel.dsculpt");
 auto entered=std::make_shared<std::promise<void>>();auto ready=entered->get_future();auto release=std::make_shared<std::atomic<bool>>(false);
 doc.edit_scene("Convert with actual prepublication gate",[entered,release](Scene &s,const auto &cancelled){auto &o=s.active();NormalPreparationOptions options;options.cancelled=cancelled;auto result=convert_canonical_normals({o.painted,o.appearance,o.material_bindings,o.deformation,o.material,{}},options);o.painted=std::move(result.paint);o.material_bindings=std::move(result.material_bindings);entered->set_value();while(!release->load()&&!cancelled())std::this_thread::sleep_for(std::chrono::milliseconds(1));});
 require_cage(ready.wait_for(std::chrono::seconds(30))==std::future_status::ready,"Real conversion completes before controlled native publication boundary");
 auto queued=doc.save_async(out/"late-cancel.dsculpt");doc.cancel_background_work();release->store(true);
 require_cage(queued.wait_for(std::chrono::seconds(30))==std::future_status::ready&&failure([&]{queued.get();},"cancel")&&doc.wait_asset(std::chrono::seconds(30))&&doc.wait_mesh(std::chrono::seconds(30))&&doc.copy_scene().encode()==exact&&Scene::load(out/"late-cancel.dsculpt").encode()==exact,
   "Late conversion Cancel preserves exact original Scene and existing file and cancels its dependent Save");
 doc.convert_baked_normals();settled(doc);require_cage(doc.copy_scene().encode()==accepted,"Actual native conversion retries exactly after late cancellation");
 auto solo=before;solo.erase(2);solo.active().bake_recipe.reset();doc.replace(solo);settled(doc);Driver independent_source(doc);independent_source.open();independent_source.sector(0);
 require_cage(doc.frame().state->controls.parameter==VrParameter::BakeSource&&doc.frame().state->bake_sources.empty(),
   "Historical project with no recipe or source mesh still exposes normal conversion through controllers");
 independent_source.ray(.5f,492.f/640);settled(doc);
 require_cage(doc.copy_scene().active().material_bindings->normal_preparation->canonical_conversion&&doc.copy_scene().objects().size()==1,
   "Controller conversion uses the original baked data without inventing or requiring a new source mesh");
 std::cout<<"PASS "<<cage_checks<<" owning canonical conversion checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
#endif
