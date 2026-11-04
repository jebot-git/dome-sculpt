// SPDX-License-Identifier: GPL-3.0-or-later
#define main previous_mtoon_fixture
#include "mtoon-render.cpp"
#undef main
#include "prepared-normal-consumer.hpp"
#include "dome/document.hpp"
#include "dome/source-uv-bake.hpp"
#include "dome/native-authoring.hpp"
#include "canonical-normal-uv.hpp"
#include "baked-normal-field.hpp"
#include <numeric>
namespace {
void uv_settle(Document &d){if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error("Normal UV worker: "+d.frame().state->error);}
bool uv_refuses(auto call){try{call();return false;}catch(const std::exception&){return true;}}
Scene uv_edit(Scene scene,bool split){
 auto &o=scene.active();SourceUvBakeInput in;in.paint=o.painted;in.current_material=o.material_bindings;in.current_deformation=o.deformation;
 const auto &before=o.painted->mesh();
 if(split){for(auto v:before.indices){in.target.positions.push_back(before.positions[v]);in.target.normals.push_back(before.normals[v]);auto q=before.uv[v];in.target.uv.push_back({.1f+.8f*(1-q[1]),.1f+.8f*q[0]});in.source_vertices.push_back(v);in.target.indices.push_back(uint32_t(in.target.indices.size()));}}
 else{in.target=before;in.source_vertices.resize(before.positions.size());std::iota(in.source_vertices.begin(),in.source_vertices.end(),0);for(auto &q:in.target.uv){q[0]=.1f+.8f*q[0];q[1]=.1f+.8f*q[1];}}
 auto produced=bake_source_uv(in);auto adopted=adopt_source_uv_bake(produced);const auto original=o.painted->baked()?o.painted->baked()->normal_field:nullptr;
 o.painted=std::move(adopted.paint);o.material_bindings=std::move(adopted.material_bindings);o.deformation=std::move(adopted.deformation);o.appearance=std::move(adopted.appearance);
 check(o.material_bindings->canonical_uv&&o.material_bindings->canonical_uv->origin()==o.material_bindings->canonical_attributes&&
   (o.painted->baked()?o.painted->baked()->normal_field:nullptr)==original&&!o.painted->normal_projection(),"Actual UV producer/adopter preserves exact independent normal field and original source");
 scene.validate();return scene;
}
}
int main(int argc,char **argv)try{
 if(argc!=3)return 2;const std::filesystem::path source=argv[1],out=argv[2];std::filesystem::create_directories(out);
 NativeGraphics gl;
 const auto device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"ACTUAL_RENDERER "<<(device?device:"unavailable")<<std::endl;
 if(const auto expected=std::getenv("DOME_EXPECT_RENDERER"))check(device&&std::string_view(device).find(expected)!=std::string_view::npos,
   "Actual retained normal UV consumer runs on the explicitly required renderer");
 Fbo fbo;SurfaceRenderer renderer;prepared_normal_consumer::Renderer independent;
 for(int variant:{0,7}){
  auto initial=Scene::load(source/("converted-"+std::to_string(variant)+".dsculpt"));initial.active().transform.scale=5;
  for(bool split:{false,true}){
   auto edited=uv_edit(initial,split);const auto wire=edited.encode();
   check(wire.at("version")==34&&wire.at("source_materials").at("version")==16&&wire.at("baked_normal_fields").at("version")==2,
     "Actual owning writer declares Scene34/material16/UV-pool2 only for genuine retained correspondence");
   auto restored=Scene::decode(wire);check(restored.encode()==wire,"Scene34 decode replays exact root/corner correspondence and full native data");
   auto &r=restored.active();check(r.material_bindings->canonical_uv->origin()==r.material_bindings->canonical_attributes&&r.painted->baked()->normal_field->origin()==r.material_bindings->canonical_attributes,
     "Restart retains one exact shared original root across UV material and active field");
   auto bad=wire;bad["baked_normal_fields"]["uv"][0]["version"]=1.0;
   check(uv_refuses([&]{Scene::decode(bad);}),"UV archive version must be a real bounded integer capability");
   bad=wire;bad["version"]=33;check(uv_refuses([&]{Scene::decode(bad);}),"Scene33 cannot smuggle retained UV capability");
   bad=wire;bad["baked_normal_fields"]["uv"].push_back(bad["baked_normal_fields"]["uv"][0]);check(uv_refuses([&]{Scene::decode(bad);}),"Unreachable UV correspondence records are refused");
   bad=wire;bad["source_materials"]["bindings"][0]["canonical_uv"]=uint64_t(1)<<32;check(uv_refuses([&]{Scene::decode(bad);}),"Oversized UV identity cannot wrap into a genuine pool owner");
   for(int pose=0;pose<3;++pose)for(float weight:{0.f,.35f,1.f}){
    auto original=initial,current=restored;
    for(auto *scene:{&original,&current}){auto &o=scene->active();if(o.deformation){auto d=*o.deformation;
      if(d.morphs)d.morph_weights.assign(d.morphs->targets.size(),weight);
      if(d.skeleton&&pose){d.pose.clear();for(const auto &bone:d.skeleton->bones)d.pose.push_back(bone.rest);
       d.pose[0].scale=pose==1?Vec3{1.1f,.85f,.55f}:Vec3{.8f,1.05f,1.6f};const float a=pose==1?.2f:-.25f;d.pose[1].rotation={std::cos(a/2),0,std::sin(a/2),0};}
      scene->set_deformation(o.id,d);}}
    Document before,after;before.replace(original);after.replace(current);before.shading(false);after.shading(false);uv_settle(before);uv_settle(after);
    fbo.bind();auto expected=draw(renderer,before.frame().surface);fbo.bind();auto actual=draw(renderer,after.frame().surface);const auto atlas_error=delta(expected,actual);
    std::cout<<"NORMAL_UV variant="<<variant<<" split="<<split<<" pose="<<pose<<" weight="<<weight<<" atlas_error="<<atlas_error<<std::endl;
    size_t visible=0;for(size_t i=3;i<actual.size();i+=4)visible+=actual[i]&&expected[i];
    check(visible>200,"Actual original and UV-edited images contain the rendered surface");
    check(atlas_error<=4,"Actual posed/morphed native OpenGL shading retains original normal field after UV changes");
    const auto stats=renderer.texture_stats();fbo.bind();check(draw(renderer,after.frame().surface)==actual&&renderer.texture_stats().uploads==stats.uploads&&renderer.texture_stats().pack_builds==stats.pack_builds,
      "Second eye adds no new normal texture uploads or pack builds");
    const auto path=out/("uv-"+std::to_string(variant)+"-"+std::to_string(split)+"-"+std::to_string(pose)+"-"+std::to_string(int(weight*100))+".glb");
    after.export_asset(path,AssetExportFormat::EditableGlb256).get();prepared_normal_consumer::Model file(path);fbo.bind();independent.render(file.data);const auto independent_error=delta(actual,read());
    std::cout<<"NORMAL_UV_EXPORT max="<<independent_error<<std::endl;check(independent_error<=4,"Independent GLB/PNG skin/morph consumer uses emitted original normal UV channel");
    if(variant==0&&split&&pose==0&&weight==0){
     auto &normal=file.data->meshes[0].primitives[0].material->normal_texture;const auto coordinate=normal.texcoord;
     normal.texcoord=0;fbo.bind();independent.render(file.data);const auto wrong=read();normal.texcoord=coordinate;
     check(coordinate>0&&delta(actual,wrong)>4,"Independent wrong-UV counterfactual exceeds the unchanged image bound");
    }
    if(pose==0&&weight==0){std::ifstream input(path,std::ios::binary);std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};auto embedded=extract_native_authoring(bytes);
     check(embedded&&embedded->encode()==current.encode(),"Actual editable GLB restores complete Scene34 and current normal UV correspondence");}
   }
   Document doc;doc.replace(restored);uv_settle(doc);doc.shading(false);doc.begin_uv();check(doc.wait_uv(),"Actual existing UV editor opens retained converted-normal workspace");
   check(doc.frame().state->uv->normal_sampling_required&&doc.frame().state->uv->retain_normal_sampling,"Shared desktop and XR UV view shows original normal retention selected and required");
   doc.edit_uv(UvAction::Scale,.8f);doc.end_uv(true);auto saved=doc.save_async(out/("queued-"+std::to_string(variant)+"-"+std::to_string(split)+".dsculpt"));saved.get();uv_settle(doc);
   const auto changed=doc.copy_scene().encode();check(changed!=wire&&Scene::load(out/("queued-"+std::to_string(variant)+"-"+std::to_string(split)+".dsculpt")).encode()==changed,"Real UV Apply and ordered queued Save publish the same complete retained-normal scene");
   check(doc.undo(),"Actual UV edit has one global Undo");uv_settle(doc);check(doc.copy_scene().encode()==wire,"UV Undo restores exact whole original scene");check(doc.redo(),"Actual UV edit has one global Redo");uv_settle(doc);check(doc.copy_scene().encode()==changed,"UV Redo restores exact correspondence and field");
   Document baseline;baseline.replace(initial);baseline.shading(false);uv_settle(baseline);doc.clear_bake();baseline.clear_bake();uv_settle(doc);uv_settle(baseline);
   auto cleared=doc.copy_scene();check(!cleared.active().painted->baked()&&cleared.active().material_bindings->canonical_uv&&Scene::decode(cleared.encode()).encode()==cleared.encode(),"Clear drops active detail while retained UV correspondence survives native restart");
   doc.save(out/("cleared-"+std::to_string(variant)+"-"+std::to_string(split)+".dsculpt"));
   fbo.bind();auto clear_original=draw(renderer,baseline.frame().surface);fbo.bind();check(delta(clear_original,draw(renderer,doc.frame().surface))<=4,"Clear restores original artist shading after repeated UV editing");
   ObjectBakeSettings settings;settings.ao_samples=0;settings.padding=0;settings.allow_partial=true;settings.front_mm=settings.back_mm=1000;
   doc.bake_from_object(2,settings);baseline.bake_from_object(2,settings);uv_settle(doc);uv_settle(baseline);
   const auto baked=doc.copy_scene(),base=baseline.copy_scene();check(baked.active().painted->baked()->normal_field->pbr()->rgba==base.active().painted->baked()->normal_field->pbr()->rgba,
     "Actual post-Clear rebake projects independent normal writes into original root UV rather than current material atlas");
   check(Scene::decode(baked.encode()).encode()==baked.encode(),"Rebaked normal history and retained UV remain exact through owning codec");
   doc.save(out/("rebaked-"+std::to_string(variant)+"-"+std::to_string(split)+".dsculpt"));
  }
 }
 std::cout<<"PASS "<<checks<<" native retained normal UV checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
