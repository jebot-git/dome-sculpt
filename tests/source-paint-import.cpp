// SPDX-License-Identifier: GPL-3.0-or-later
#define main established_graphics_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/source-paint.hpp"
#include "dome/document.hpp"
#include "dome/material-bindings.hpp"
#include "import-authoring-fixtures.hpp"
#include "paint-overlay-packet.hpp"
#include "paint-archive-shape.hpp"
namespace {
using Json=nlohmann::json;
import_fixture::Fixture input(int kind){
 import_fixture::Fixture f;while(f.bin.size()%4)f.bin.push_back(0);auto &p=f.j["meshes"][0]["primitives"][0];p["attributes"]["TEXCOORD_0"]=f.values({-1.25f,-.5f,1.75f,-.5f,1.75f,2.5f,-1.25f,2.5f},2,"VEC2");
 f.j["nodes"][0]["translation"]={0,0,0};f.j["nodes"][0]["children"]={1};f.j["nodes"][1]["translation"]={0,0,0};f.j["nodes"][1]["scale"]={5,5,5};
 auto &m=f.j["materials"][0];
 if(kind!=0&&kind!=2)m["pbrMetallicRoughness"].erase("baseColorTexture");
 if(kind>=1&&kind<=3){m["extensions"]["VRMC_materials_mtoon"]={{"specVersion","1.0"}};f.j["extensionsUsed"].push_back("VRMC_materials_mtoon");
  if(kind==1||kind==2){
   auto &mt=m["extensions"]["VRMC_materials_mtoon"];mt["shadeMultiplyTexture"]={{"index",0},{"texCoord",1},{"extensions",{{"KHR_texture_transform",{{"offset",{.13f,-.27f}},{"scale",{.8f,1.1f}},{"rotation",.17f},{"texCoord",1}}}}}};
   mt["shadeColorFactor"]={.4f,.6f,.8f};mt["shadingShiftFactor"]=-1;mt["shadingToonyFactor"]=0;
   f.j["extensionsUsed"].push_back("KHR_texture_transform");
  }
  if(kind==3){m["extensions"]["VRMC_materials_mtoon"]["matcapTexture"]={{"index",0}};m["extensions"]["VRMC_materials_mtoon"]["matcapFactor"]={.4f,.3f,.2f};}
 }
 if(kind>=4){m["alphaMode"]="OPAQUE";m.erase("alphaCutoff");}
 if(kind==5){p["attributes"].erase("TEXCOORD_0");p["attributes"].erase("TEXCOORD_1");}
 return f;
}
void settle(Document &d){if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error("Actual native source import/paint worker: "+d.frame().state->error);}
void zero_emission(Scene &scene){for(const auto &o:scene.objects())if(o.mesh_active&&o.painted&&o.painted->source_sampling_prepared()){
 auto paint=o.painted->snapshot();PaintSettings ink;ink.channel=PaintChannel::Emission;ink.emission={0,0,0};ink.front_only=false;Brush b;b.from=b.to={0,0,0};b.normal={0,0,1};b.radius=1;b.strength=.5f;paint->begin_stroke();paint->stamp(b,ink);check(paint->end_stroke(),"Actual sparse paint activates the native imported source overlay");scene.object(o.id).painted=std::move(paint);
}}
}
int main(int argc,char **argv)try{
 if(argc<2||argc>3)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<'\n';check(argc<3||std::string_view(argv[2])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Requested import hardware proof requires actual A770");Fbo fbo;fbo.bind();SurfaceRenderer renderer;renderer.set_frame_time(0);int maximum=0;
 Scene original;original.active().visible=false;const auto original_bytes=original.encode();ImportAuthoringOptions raw_options;raw_options.paint_resolution=64;auto atlas_options=raw_options;atlas_options.prepare_texture_atlas=true;
 for(int kind=0;kind<6;++kind){auto source=input(kind);const auto path=out/("case"+std::to_string(kind)+".glb");source.save(path);auto raw=prepare_authoring_import(original,path,raw_options),prepared=prepare_authoring_import(original,path,atlas_options);
  check(original.encode()==original_bytes&&prepared.objects.size()==1,"Actual initial source-aware import preserves caller and selected hierarchy");const auto id=prepared.objects[0];const auto &object=prepared.scene.object(id);const auto &paint=*object.painted;
  check(paint.source_sampling_prepared()&&paint.surface_diagnostics().policy==PaintUvPolicy::Strict&&*object.appearance->original_bytes==source.bytes(),"Actual Import uses independent writable authority and keeps exact original source bytes");
  check(paint.source_maps()->empty()==(kind!=0&&kind!=2),"Untextured and typed-only imports contain no invented PBR image roles");
  check(object.material_bindings->has_mtoon==(kind>=1&&kind<=3),"Standalone glTF MToon roles are adopted independently of a VRM humanoid root");
  const auto baseline=prepared.scene.encode();check(baseline.at("version")==28&&Scene::decode(baseline).encode()==baseline,"Owning Scene28 restores actual native imported source and supplemental logical UV1");
  const auto authored_uv1=paint.source_sampling_authored_uv1();
  check(kind==5?!authored_uv1:authored_uv1&&authored_uv1->size()==paint.source_sampling_original()->mesh().positions.size(),"Only genuine unused-by-PBR authored UV1 is retained as a before-domain field");
  auto active=prepared.scene.snapshot();zero_emission(active);const auto saved=active.encode();auto restart=Scene::decode(saved);check(restart.encode()==saved,"Real painted source-aware import preserves exact metadata through native restart");
  Document a,b;a.replace(raw.scene);b.replace(restart);settle(a);settle(b);
  for(bool pbr:{false,true}){if(pbr){a.shading(false);b.shading(false);}else{a.authored_shading();b.authored_shading();}settle(a);settle(b);
   const auto wanted=draw(renderer,a.frame().surface),actual=draw(renderer,b.frame().surface);const int error=delta(wanted,actual);maximum=std::max(maximum,error);size_t visible=0;for(size_t n=3;n<wanted.size();n+=4)visible+=wanted[n]>0;
   std::cout<<"IMPORT kind="<<kind<<" pbr="<<pbr<<" covered="<<visible<<" max="<<error<<'\n';check(visible>20&&error<=2,"Actual imported original/painted/reopened authored and PBR pixels retain source sampling and role frames");
   if(!pbr&&(kind==1||kind==2)){
    auto wrong=restart.snapshot();auto material=std::make_shared<SourceMaterialBindings>(*wrong.active().material_bindings);material->material.textures[1].texcoord=0;material->material.textures[1].transform_texcoord=0;wrong.active().material_bindings=std::move(material);Document control;control.replace(wrong);settle(control);
    check(delta(actual,draw(renderer,control.frame().surface))>4,"Independent UV0 substitution visibly changes the actual UV1 shade role");
    draw(renderer,b.frame().surface);
   }
   auto uploads=renderer.buffer_stats();check(draw(renderer,b.frame().surface)==actual&&renderer.buffer_stats().geometry_uploads==uploads.geometry_uploads,"Actual source-aware import second eye reuses immutable geometry");
  }
  if(kind==2){const auto packet=paint.preview(object.material)->overlay;check(packet->original_uv0()==paint.source_maps()->uv1&&packet->original_uv1()!=paint.source_maps()->uv1,"Generated UV0 carrier1 stays distinct from actual authored logical UV1");}
  if(kind==4){
   Document native;native.replace(original);ImportSettings settings;settings.paint_resolution=64;settings.prepare_texture_atlas=true;native.import_asset(path,settings);auto file=out/"native-import-queued.dsculpt";native.save_async(file).get();settle(native);const auto imported=native.copy_scene().encode();check(Scene::load(file).encode()==imported,"Actual native Import and immediately queued Save retain complete source ownership");
   check(native.undo()&&native.copy_scene().encode()==original_bytes&&native.redo()&&native.copy_scene().encode()==imported,"Actual native source-aware Import has one exact global Undo/Redo boundary");settle(native);
   PaintSettings ink;ink.color={.1f,.8f,.2f,.8f};native.paint_settings(ink);NativeStrokeSample sample;sample.origin={0,.06f,2};sample.direction={0,0,-1};sample.brush.radius=.2f;
   check(native.begin_native_stroke()&&native.sample_native_stroke(sample),"Actual native input paints the untextured source-aware imported grid");native.end_native_stroke();file=out/"native-painted-queued.dsculpt";native.save_async(file).get();check(native.wait_native_stroke(),"Actual native source paint transaction retires");settle(native);const auto painted=native.copy_scene().encode();check(painted!=imported&&Scene::load(file).encode()==painted,"Immediately queued Save follows actual imported sparse painting exactly");check(native.undo()&&native.copy_scene().encode()==imported&&native.redo()&&native.copy_scene().encode()==painted,"Paint and Import retain separate exact history boundaries");
  }
 }
 // Independent logical-coordinate ownership, limits and malformed-reader controls.
 auto mesh=quad();auto uv1=std::make_shared<std::vector<std::array<float,2>>>(mesh.uv);auto before=capture_source_sampling(mesh,{},64);const auto initial=before->encode();const std::array<uint32_t,4> vertices{0,1,2,3};auto paint=prepare_source_paint(before,mesh,vertices,uv1,SourcePaintOptions{});const auto state=paint->encode();(*uv1)[0][0]=.25f;
 check(paint->encode()==state&&before->encode()==initial,"Factory freezes the caller's independent authored coordinate buffer");
 auto bad=state;bad["authored_uv1"].get_binary().pop_back();check(rejects([&]{PaintedMesh::decode(bad,nullptr,false,nullptr,nullptr,SourcePaintDecodeContext{true});}),"Malformed authored UV1 byte count refuses before ownership allocation");
 bad=state;auto &bytes=bad["authored_uv1"].get_binary();bytes[0]=0;bytes[1]=0;bytes[2]=128;bytes[3]=127;check(rejects([&]{PaintedMesh::decode(bad,nullptr,false,nullptr,nullptr,SourcePaintDecodeContext{true});}),"Nonfinite authored logical UV1 cannot mint sampling authority");
 check(rejects([&]{PaintedMesh::decode(state);})&&rejects([&]{auto limits=SourcePaintOptions{};limits.max_coordinate_bytes=8;prepare_source_paint(before,mesh,vertices,uv1,limits);})&&paint->encode()==state,"Legacy reader and coordinate quota refusals preserve exact accepted source state");
 auto empty_roles=capture_source_sampling(mesh,{},512);auto plain=std::make_shared<PaintedMesh>(mesh,512);PaintSettings ink;ink.color={.2f,.7f,.4f,.5f};ink.front_only=false;Brush brush;brush.from=brush.to={0,0,0};brush.normal={0,0,1};brush.radius=.3f;
 for(auto &p:{empty_roles,plain}){p->begin_stroke();p->stamp(brush,ink);check(p->end_stroke(),"Actual ordinary no-image paint allocates a writable plain field");}
 const auto captured=empty_roles->composite(white()),ordinary=plain->composite(white());check(captured->color.rgba==ordinary->color.rgba&&captured->metal_rough.rgba==ordinary->metal_rough.rgba&&captured->emission.rgba==ordinary->emission.rgba,"Empty original role bundles preserve ordinary composite semantics and selected-backend eligibility");
 std::cout<<"PASS "<<checks<<" source-aware native import checks maxerror="<<maximum<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
