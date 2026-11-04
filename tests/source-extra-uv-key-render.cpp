// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_EXTRA_UV_ROLE_NO_MAIN
#include "source-extra-uv-roles.cpp"
#undef DOME_EXTRA_UV_ROLE_NO_MAIN
#include "import-skin-fixtures.hpp"
#include "dome/shapekey-authoring.hpp"
namespace {
import_fixture::Fixture key_source(int coordinate,bool generated){
 auto f=role_fixture(3,coordinate);auto &p=f.j["meshes"][0]["primitives"][0];
 if(generated){p["attributes"].erase("TANGENT");p["targets"][0].erase("TANGENT");}
 const auto joints=import_fixture::joint_data(f);
 const auto weights=f.values({.8f,.2f,0,0,.5f,.5f,0,0,.2f,.8f,0,0,.65f,.35f,0,0},4,"VEC4");
 p["attributes"]["JOINTS_0"]=joints;p["attributes"]["WEIGHTS_0"]=weights;
 f.j["nodes"][0]["children"].push_back(3);f.j["nodes"][1]["skin"]=0;f.j["nodes"][2]["skin"]=0;
 f.j["nodes"].push_back({{"name","source root"},{"translation",{0,0,0}},{"children",{4}}});
 f.j["nodes"].push_back({{"name","source child"},{"translation",{.05f,0,0}}});
 f.j["skins"]={{{"joints",{3,4}},{"skeleton",3}}};return f;
}
void native_capture(const std::filesystem::path &root,SurfaceRenderer &renderer,Fbo &fbo,prepared_normal_consumer::Renderer &consumer,bool stress=false){
 for(bool curved:{false,true}){
  auto mesh=quad();if(curved){mesh.positions[2].z=.65f;MorphSet geometry(mesh);mesh=geometry.evaluate(mesh,{});}
  auto maps=std::make_shared<PaintSourceMaps>();maps->normal.image=std::make_shared<const Image>(Image{2,2,{180,140,235,255,100,160,235,255,145,90,235,255,170,185,235,255}});maps->normal.texcoord=2;
  maps->color.image=std::make_shared<const Image>(Image{1,1,{220,180,150,255}});maps->color.texcoord=1;maps->uv1=std::make_shared<const std::vector<std::array<float,2>>>(mesh.uv);
  auto coordinates=std::make_shared<std::vector<std::array<float,2>>>();for(auto q:mesh.uv)coordinates->push_back({q[1],q[0]});if(stress)(*coordinates)[2]={.4f,.7f};maps->extra_uv_sets[2]=coordinates;maps->validate_surface(mesh);
  Scene initial;initial.active().mesh_active=true;initial.active().material=white();initial.active().material.unlit=false;initial.active().painted=PaintedMesh::from_maps(mesh,maps,64);const auto exact=initial.encode();Document before;before.replace(initial);settled(before);before.shading(false);settled(before);const auto original=draw(renderer,before.frame().surface);
  before.author_shape({});const auto scene_file=root/(curved?"native-curved.dsculpt":"native-plane.dsculpt");before.save_async(scene_file).get();settled(before);auto captured=before.copy_scene();
  check(Scene::load(scene_file).encode()==captured.encode()&&before.undo()&&before.copy_scene().encode()==exact&&before.redo()&&before.copy_scene().encode()==captured.encode(),"Native worker frame capture supports ordered Save/reopen and exact one Undo/Redo");
  const auto frames=captured.active().material_bindings->normal_frames;
  check(frames->pbr->generated_tangents&&frames->pbr->generated_uv==2&&frames->pbr->source_vertices.empty(),"Native owned UV2 capture mints genuine native frame provenance without fake raw artist identities");
  Document current;current.replace(captured);settled(current);current.shading(false);settled(current);const auto native=draw(renderer,current.frame().surface);const auto capture_error=delta(original,native);std::cout<<"NATIVE_UV2_CAPTURE curved="<<curved<<" current_error="<<capture_error<<std::endl;
  check(capture_error<=2,"A neutral geometry-key capture preserves the genuine current native normal-map appearance");
  auto path=root/(curved?"native-curved.glb":"native-plane.glb");current.export_asset(path,AssetExportFormat::Glb).get();prepared_normal_consumer::Model model(path);fbo.bind();consumer.render(model.data);check(delta(native,read())<=2,"Native UV2 frame capture exports independent standard normal-map pixels");
  ShapeKeyEdit restore;restore.action=ShapeKeyAction::Restore;author_shapekey(captured,captured.active().id,restore);check(captured.encode()==exact,"Native frame capture Restore preserves the complete original no-frame paint source");
 }
}
void pose_keys(Scene &scene,int pose,float weight){
 for(const auto &object:scene.objects())if(object.deformation){auto d=*object.deformation;d.morph_weights.assign(d.morphs->targets.size(),weight);d.pose.clear();for(const auto &bone:d.skeleton->bones)d.pose.push_back(bone.rest);
  if(pose){auto &a=d.pose[0].scale;a={a.x*1.1f,a.y*.85f,a.z*.65f};float angle=.2f;d.pose[1].rotation={std::cos(angle/2),0,std::sin(angle/2),0};auto &b=d.pose[1].scale;b={b.x*.9f,b.y*1.1f,b.z*1.4f};}scene.set_deformation(object.id,d);
 }
}
}
int main(int argc,char**argv)try{
 if(argc<2)return 2;bool hardware=false,stress=false;for(int i=2;i<argc;++i){auto arg=std::string_view(argv[i]);if(arg=="--hardware")hardware=true;else if(arg=="--skew")stress=true;else throw std::invalid_argument("Unknown key consumer option");}const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);
 NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GL_VENDOR "<<vendor<<"\nGL_RENDERER "<<device<<std::endl;
 check(!hardware||(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos&&device.find("llvmpipe")==std::string::npos),"New key consumer runs on the exact requested graphics device");
 Fbo fbo;fbo.bind();SurfaceRenderer renderer;prepared_normal_consumer::Renderer consumer;
 native_capture(root,renderer,fbo,consumer,stress);
 for(int coordinate=2;coordinate<=7;++coordinate)for(bool generated:{false,true}){
  const auto stem="uv"+std::to_string(coordinate)+(generated?"-generated":"-artist");const auto input=root/(stem+"-source.glb");key_source(coordinate,generated).save(input);
  Scene empty;empty.active().visible=false;ImportAuthoringOptions options;options.paint_resolution=64;auto scene=prepare_authoring_import(empty,input,options).scene;const auto before=scene.encode();std::vector<uint64_t> ids;for(const auto &object:scene.objects())if(object.appearance)ids.push_back(object.id);
  for(auto id:ids){const auto &object=scene.object(id);ShapeKeyEdit edit;edit.action=ShapeKeyAction::Replace;edit.target=0;const auto &rest=object.painted->mesh();for(size_t v=0;v<rest.positions.size();++v){auto p=rest.positions[v]+object.deformation->morphs->targets[0].delta[v];edit.positions.push_back({1.3f*p.x+.25f*p.y,.15f*p.x+.8f*p.y,.12f*p.x+.22f*p.y});}author_shapekey(scene,id,edit);ShapeKeyEdit mirror;mirror.action=ShapeKeyAction::Mirror;mirror.mirror_frames=ShapeKeyMirrorFrames::VertexPositions;mirror.target=0;mirror.name="Coordinate-aware mirror";author_shapekey(scene,id,mirror);}
  const auto saved=root/(stem+".dsculpt");scene.save(saved);check(Scene::load(saved).encode()==scene.encode(),"Current UV2+ keys and immutable raw/frame sources survive restart");
  for(int pose=0;pose<2;++pose)for(float weight:{0.f,.35f,1.f}){
   auto current=scene;pose_keys(current,pose,weight);Document document;document.replace(current);settled(document);document.shading(false);settled(document);
   const auto surface=document.frame().surface;const auto native=draw(renderer,surface);const auto stats=renderer.buffer_stats();check(native==draw(renderer,surface)&&renderer.buffer_stats().geometry_uploads==stats.geometry_uploads,"Static/second-eye key preview reuses source/frame/UV geometry without another upload");
   const auto path=root/(stem+"-p"+std::to_string(pose)+"-w"+std::to_string(int(weight*100))+".glb");document.export_asset(path,AssetExportFormat::Glb).get();prepared_normal_consumer::Model model(path);fbo.bind();consumer.render(model.data);const auto independent=read();const auto error=delta(native,independent);size_t covered=0;for(size_t i=3;i<independent.size();i+=4)covered+=independent[i]!=0;
   std::cout<<"KEY_SOURCE_UV set="<<coordinate<<" generated="<<generated<<" pose="<<pose<<" weight="<<weight<<" error="<<error<<" covered="<<covered<<std::endl;
   check(covered>150&&error<=2,"Independent parsed GLB matches current authored key pixels through morph weights and mixed-joint pose");
   for(size_t n=0;n<model.data->materials_count;++n){check(model.data->materials[n].normal_texture.texcoord==coordinate,"Emitted normal role preserves its genuine source UV set ID");model.data->materials[n].normal_texture.texcoord=0;}
   fbo.bind();consumer.render(model.data);auto wrong=delta(independent,read());check(wrong>2,"Wrong UV0 normal sampling visibly fails on the newly authored key output");
  }
  for(auto id:ids){ShapeKeyEdit restore;restore.action=ShapeKeyAction::Restore;author_shapekey(scene,id,restore);}check(scene.encode()==before,"All newly authored source-UV keys restore their complete exact original project graph");
 }
 std::cout<<"PASS "<<checks<<" genuine coordinate-aware key graphical checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
