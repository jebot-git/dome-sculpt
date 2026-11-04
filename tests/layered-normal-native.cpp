// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_render_main
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "baked-normal-field.hpp"
#include <fstream>
namespace {
void settled_layers(Document &d){
  if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error("Layered native worker: "+d.frame().state->error);
}
void capture_layers(const std::filesystem::path &p,const std::vector<uint8_t> &v){
  auto bytes=Image{96,96,v}.png();std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
}
void roundtrip_layers(Document &document,const std::filesystem::path &path){
  auto scene=document.copy_scene();const auto wire=scene.encode();
  check(wire.at("version")==33&&wire.at("source_materials").at("version")==15,"Actual owning writer declares separate Scene33/material15 capabilities");
  auto decoded=Scene::decode(wire);check(decoded.encode()==wire,"Owning decoder replays exact normal history and preserves complete native Scene");
  const auto &o=decoded.active();const auto c=o.material_bindings->canonical_attributes;
  check(c&&c->before_paint()==o.material_bindings->uv_origin->paint&&(!o.painted->baked()||o.painted->baked()->normal_field->origin()==c),
    "Restart preserves one genuine shared original paint/frame origin across current material and baked field");
  document.save(path);check(Scene::load(path).encode()==wire,"Actual ordered Document Save/restart retains exact layered normals and material/source data");
}
}
int main(int argc,char **argv)try{
  if(argc==3&&std::string_view(argv[1])=="--inspect"){
    const auto scene=Scene::load(argv[2]);const auto &o=scene.active();
    std::cout<<nlohmann::json{{"scene",scene.encode()},{"converted",bool(o.material_bindings&&o.material_bindings->canonical_attributes)},
      {"active_detail",bool(o.painted&&o.painted->baked()&&o.painted->baked()->normal_field)}}.dump()<<'\n';return 0;
  }
  if(argc==3&&(std::string_view(argv[1])=="--ui-fixture"||std::string_view(argv[1])=="--ui-solo-fixture"||
      std::string_view(argv[1])=="--ui-converted"||std::string_view(argv[1])=="--ui-solo-converted")){
    auto scene=Scene::load(std::filesystem::path(__FILE__).parent_path()/"fixtures/layered-normal/original-7.dsculpt");
    if(std::string_view(argv[1]).find("solo")!=std::string_view::npos){scene.erase(2);scene.active().bake_recipe.reset();}
    if(std::string_view(argv[1]).find("converted")!=std::string_view::npos){Document doc;doc.replace(scene);settled_layers(doc);doc.convert_baked_normals();settled_layers(doc);doc.save(argv[2]);}
    else scene.save(argv[2]);return 0;
  }
  if(argc!=3)return 2;const std::filesystem::path source=argv[1],out=argv[2];std::filesystem::create_directories(out);
  NativeGraphics graphics;Fbo fbo;SurfaceRenderer renderer;
  const auto actual_renderer=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  std::cout<<"ACTUAL_RENDERER "<<(actual_renderer?actual_renderer:"unavailable")<<'\n';
  if(const auto wanted=std::getenv("DOME_EXPECT_RENDERER"))
    check(actual_renderer&&std::string_view(actual_renderer).find(wanted)!=std::string_view::npos,
      "Actual native worker/render consumer uses the explicitly required renderer");
  for(int variant:{0,7}){
    auto raw=Scene::load(source/("original-"+std::to_string(variant)+".dsculpt"));raw.active().transform.scale=5;
    Document before,after;before.replace(raw);after.replace(raw);before.shading(false);after.shading(false);settled_layers(before);settled_layers(after);
    fbo.bind();const auto original=draw(renderer,before.frame().surface);
    after.convert_baked_normals();settled_layers(after);auto converted=after.copy_scene();
    const auto origin=converted.active().material_bindings->canonical_attributes;
    roundtrip_layers(after,out/("converted-"+std::to_string(variant)+".dsculpt"));
    check(origin&&!converted.active().painted->normal_projection()&&!converted.active().material_bindings->normal_preparation&&
      converted.active().painted->baked()->normal_field->origin()==origin,"Real conversion keeps baked detail separate from intrinsic planes and authentic attribute provenance");
    fbo.bind();const auto current=draw(renderer,after.frame().surface);const auto convert_error=delta(original,current);
    capture_layers(out/("converted-"+std::to_string(variant)+".png"),current);
    check(convert_error<=4,"Actual converted native frame remains within unchanged image bound");
    before.clear_bake();after.clear_bake();settled_layers(before);settled_layers(after);
    fbo.bind();const auto a=draw(renderer,before.frame().surface);fbo.bind();const auto b=draw(renderer,after.frame().surface);
    capture_layers(out/("raw-clear-"+std::to_string(variant)+".png"),a);capture_layers(out/("converted-clear-"+std::to_string(variant)+".png"),b);
    check(delta(a,b)<=4,"Actual worker Clear Bake removes converted detail and restores genuine original shading");
    auto cleared=after.copy_scene();check(!cleared.active().painted->normal_projection()&&!cleared.active().painted->baked()&&
      cleared.active().material_bindings->canonical_attributes==origin&&origin->before_paint()->baked(),"Clear drops active field but retains complete original normal/material/deformation provenance");
    roundtrip_layers(after,out/("cleared-"+std::to_string(variant)+".dsculpt"));
    after.undo();settled_layers(after);fbo.bind();check(delta(current,draw(renderer,after.frame().surface))==0,"Clear Undo restores exact converted shading");
    after.redo();settled_layers(after);fbo.bind();check(delta(b,draw(renderer,after.frame().surface))==0,"Clear Redo restores exact intrinsic shading");
    std::cout<<"CLEAR variant="<<variant<<" conversion_error="<<convert_error<<" clear_error="<<delta(a,b)<<'\n';
  }
  for(bool clear_first:{false,true}){
    auto raw=Scene::load(source/"original-0.dsculpt");raw.active().transform.scale=raw.object(2).transform.scale=5;
    Mesh high;high.positions={{0,0,.02f},{.3f,0,.02f},{0,.3f,.02f}};high.normals.assign(3,{.3f,.4f,std::sqrt(.75f)});
    high.uv={{{0,0}},{{1,0}},{{0,1}}};high.indices={0,1,2};raw.object(2).painted=std::make_shared<PaintedMesh>(high,64);
    Document legacy,converted;legacy.replace(raw);converted.replace(raw);legacy.shading(false);converted.shading(false);settled_layers(legacy);settled_layers(converted);
    converted.convert_baked_normals();settled_layers(converted);
    if(clear_first){legacy.clear_bake();converted.clear_bake();settled_layers(legacy);settled_layers(converted);}
    fbo.bind();const auto prior=draw(renderer,converted.frame().surface);
    ObjectBakeSettings settings;settings.ao_samples=0;settings.padding=0;settings.allow_partial=true;settings.front_mm=settings.back_mm=1000;
    legacy.bake_from_object(2,settings);converted.bake_from_object(2,settings);settled_layers(legacy);settled_layers(converted);
    fbo.bind();const auto a=draw(renderer,legacy.frame().surface);fbo.bind();const auto b=draw(renderer,converted.frame().surface);
    capture_layers(out/(clear_first?"clear-then-rebake.png":"same-normal-partial-rebake.png"),b);
    check(delta(a,b)<=4,"Actual partial Bake agrees with legacy original in projected and missed regions, including after Clear");
    if(!clear_first)check(delta(prior,b)<=4,"Partial rebake of the same direction preserves the actual previous field");
    const auto saved=converted.copy_scene();const auto &field=saved.active().painted->baked()->normal_field;
    check(field&&field->intrinsic_baseline()==clear_first&&field->replacements()==1,"Rebake uses current resolved detail or genuine cleared intrinsic baseline without resurrecting old bake");
    roundtrip_layers(converted,out/(clear_first?"clear-then-rebake.dsculpt":"partial-rebake.dsculpt"));
    converted.clear_bake();settled_layers(converted);
    std::cout<<"REBAKE clear_first="<<clear_first<<" legacy_error="<<delta(a,b)<<" prior_error="<<delta(prior,b)<<'\n';
  }
  // Genuine old31 files are decoded under their old contract. A real explicit
  // editing command performs the owning migration; loading alone never does.
  for(bool clear:{false,true}){
    auto old=Scene::load(source/"converted-7.dsculpt");check(old.encode().at("version")==31,"Existing Scene31 remains readable without silent reinterpretation");
    Document document;document.replace(old);document.shading(false);settled_layers(document);
    if(clear)document.clear_bake();else document.convert_baked_normals();settled_layers(document);
    roundtrip_layers(document,out/(clear?"legacy-clear.dsculpt":"legacy-upgrade.dsculpt"));
    check(!document.copy_scene().active().painted->normal_projection(),"Explicit legacy command removes false intrinsic ownership while retaining original source");
  }
  {
    auto source_scene=Scene::load(source/"original-0.dsculpt");Document document;document.replace(source_scene);settled_layers(document);
    document.convert_baked_normals();settled_layers(document);const auto valid=document.copy_scene().encode();
    auto refuses=[&](nlohmann::json bad){try{Scene::decode(bad);return false;}catch(const std::exception&){return true;}};
    auto bad=valid;bad["version"]=32;check(refuses(bad),"Relabeling the outer Scene cannot grant independent normal authority");
    bad=valid;bad["source_materials"]["version"]=2;check(refuses(bad),"Historical material cannot acquire layered capabilities by relabeling");
    bad=valid;bad["source_materials"]["bindings"][0]["canonical_attributes"]=uint64_t(1)<<32;check(refuses(bad),"Oversized original-owner references cannot wrap to a valid identity");
    bad=valid;bad["baked_normal_fields"]["origins"][0]["receipt"]["observed_center_error"]=.2;check(refuses(bad),"Forged canonical quality receipt fails actual original-source replay");
    bad=valid;bad["baked_normal_fields"]["fields"][0]["prior"]=0;check(refuses(bad),"Cyclic normal-field history fails preflight");
    bad=valid;bad["baked_normal_fields"]["fields"][0]["baseline"]=true;check(refuses(bad),"Historical detail cannot be relabeled as a cleared intrinsic baseline");
    bad=valid;bad["baked_normal_fields"]["fields"].push_back(bad["baked_normal_fields"]["fields"][0]);check(refuses(bad),"Unreachable owning field records are rejected");
    check(document.copy_scene().encode()==valid,"Malformed independent graphs preserve complete live document and exact retry");
  }
  {
    auto raw=Scene::load(source/"original-7.dsculpt");Document document;document.replace(raw);settled_layers(document);
    document.convert_baked_normals();settled_layers(document);auto scene=document.copy_scene();const auto mesh=scene.active().painted->mesh();
    scene.add("Prepared source guard target",Volume{});scene.active().mesh_active=true;scene.active().painted=std::make_shared<PaintedMesh>(mesh,64);
    const auto exact=scene.encode();ObjectBakeSettings options;options.ao_samples=0;options.allow_partial=true;options.front_mm=options.back_mm=1000;
    bool refused=false;try{bake_object(scene,1,options);}catch(const std::exception &e){refused=std::string_view(e.what()).find("source-frame transport")!=std::string_view::npos;}
    check(refused&&scene.encode()==exact,"Independent prepared source cannot bypass genuine source-frame transport or mutate either object");
  }
  return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
