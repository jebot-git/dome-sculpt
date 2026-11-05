// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_native_key_graphics_fixture
#include "mtoon-render.cpp"
#undef main
#include "prepared-normal-consumer.hpp"
#include "dome/document.hpp"
#include "dome/rig-edit.hpp"
#include "dome/native-authoring.hpp"
#include "normal-frame-export.hpp"
#include <fstream>
#include <set>
namespace {
std::vector<uint8_t> key_file_bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void finish(Document&d){check(d.wait_asset(std::chrono::seconds(120))&&d.wait_mesh(std::chrono::seconds(120))&&d.frame().state->error.empty(),"Actual native key authoring and immutable render worker settle");}
void capture(const std::filesystem::path&path,const Pixels&p){auto png=Image{96,96,p}.png();std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(png.data()),png.size());}
const cgltf_accessor* field(const cgltf_attribute*a,size_t n,cgltf_attribute_type t,int set=0){for(size_t i=0;i<n;++i)if(a[i].type==t&&a[i].index==set)return a[i].data;return nullptr;}
}
int main(int argc,char**argv)try{
 if(argc<3||argc>4||(argc==4&&std::string_view(argv[3])!="--hardware"))return 2;const bool strict=argc==4;const auto source=Scene::load(argv[1]);const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);setenv("XDG_STATE_HOME",(out/"state").c_str(),1);setenv("XDG_CONFIG_HOME",(out/"config").c_str(),1);NativeGraphics graphics;const std::string gpu=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<gpu<<"\nGL_VENDOR "<<reinterpret_cast<const char*>(glGetString(GL_VENDOR))<<"\nGL_VERSION "<<reinterpret_cast<const char*>(glGetString(GL_VERSION))<<std::endl;check(!strict||(gpu.find("Intel")!=std::string::npos&&gpu.find("Arc")!=std::string::npos&&gpu.find("A770")!=std::string::npos&&gpu.find("llvmpipe")==std::string::npos),"Strict native key export consumer requires actual Intel Arc A770");Fbo fbo;SurfaceRenderer native;prepared_normal_consumer::Renderer consumer;unsigned number=0;Pixels zero;
 for(bool posed:{false,true})for(float weight:{0.f,.37f,1.f}){
  auto scene=source;scene.active().transform.scale=5;RigEdit pose;pose.linked_pose=false;pose.morph=MorphWeightEdit{0,weight};if(posed)pose.joint=JointAdjustment{1,{0,0,25},{}};edit_rig(scene,scene.active().id,pose);const auto exact=scene.encode();Document doc;doc.replace(scene);doc.shading(false);finish(doc);fbo.bind();const auto wanted=draw(native,doc.frame().surface);size_t visible=0;std::set<std::array<uint8_t,4>> colours;for(size_t p=0;p<wanted.size();p+=4)if(wanted[p+3]){++visible;colours.insert({wanted[p],wanted[p+1],wanted[p+2],wanted[p+3]});}check(visible>200&&colours.size()>8,"Actual framed posed key image has substantial varied visible paint");
  if(!posed&&weight==0)zero=wanted;if(!posed&&weight==1)check(delta(zero,wanted)>2,"Actual authored current morph visibly changes geometry and shading");
  auto dir=out/std::to_string(number++);std::filesystem::create_directories(dir);capture(dir/"native.png",wanted);
  for(bool editable:{false,true}){
   const auto file=dir/(editable?"editable.glb":"standard.glb");doc.export_asset(file,editable?AssetExportFormat::EditableGlb256:AssetExportFormat::Glb).get();check(doc.copy_scene().encode()==exact,"Actual ordinary/editable native key worker preserves complete source, paint, skin and current frame owner");prepared_normal_consumer::Model model(file);check(model.data->meshes_count==1&&model.data->meshes[0].primitives_count==1,"Current key emits only its actual visible native subject");const auto&primitive=model.data->meshes[0].primitives[0];const auto&mesh=scene.active().painted->mesh();const auto corners=scene.active().painted->paint_sampling_domain()->corner_coordinates(mesh);const auto uv=field(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_texcoord,1);check(uv&&primitive.targets_count==1&&field(primitive.targets[0].attributes,primitive.targets[0].attributes_count,cgltf_attribute_type_normal),"Emitted current normal morph and independent paint coordinate arrays are present");
   for(size_t c=0;c<mesh.indices.size();++c){const auto vertex=cgltf_accessor_read_index(primitive.indices,c);std::array<float,2> coord{};check(cgltf_accessor_read_float(uv,vertex,coord.data(),2)&&coord==corners[c],"Independent parser preserves every genuine corner paint coordinate through the normal/morph split");}
   fbo.bind();consumer.render(model.data,nullptr);const auto pixels=read();const auto error=delta(wanted,pixels);std::cout<<"NATIVE36_KEY_EXPORT posed="<<posed<<" weight="<<weight<<" editable="<<editable<<" max="<<error<<std::endl;capture(dir/(editable?"editable.png":"standard.png"),pixels);check(error<=4,"Independent skin/morph/normal/paint consumer meets unchanged4 image bound");
   if(editable){auto recovered=extract_native_authoring(key_file_bytes(file));check(recovered&&recovered->encode()==exact,"Actual editable native key GLB recovers every original authoring owner and current value");}
   if(!posed&&weight==0&&!editable){auto&view=primitive.material->pbr_metallic_roughness.base_color_texture;const auto set=view.texcoord;view.texcoord=0;fbo.bind();consumer.render(model.data,nullptr);const auto wrong=read();view.texcoord=set;capture(dir/"wrong-authored-uv.png",wrong);check(delta(wanted,wrong)>4,"Independent wrong authored-UV sampling remains visibly sensitive");}
  }
  if(!posed&&weight==0){auto surface=scene.preview_surfaces().front();const auto correct=surface;surface.paint_sampling_attachment=std::shared_ptr<const PaintSamplingAttachment>(surface.paint_sampling_attachment.get(),[](const PaintSamplingAttachment*){});check(rejects([&]{detail::normal_frame_export(surface,detail::SourceMorphSpace::Native,{},true);}),"Same-address foreign attachment control block cannot borrow native key paint/frame authority");surface=correct;surface.paint_corner_coordinates[0][0]+=.125f;check(rejects([&]{detail::normal_frame_export(surface,detail::SourceMorphSpace::Native,{},true);}),"Copied corner coordinates cannot borrow the authentic native key paint domain");surface=correct;auto b=std::make_shared<SourceMaterialBindings>(*surface.material_bindings);b->native_paint_frames.reset();surface.material_bindings=b;check(rejects([&]{detail::normal_frame_export(surface,detail::SourceMorphSpace::Native,{},true);}),"A missing native frame owner cannot acquire normal export authority from a version or counts");}
  if(!posed&&weight==0){auto surface=scene.preview_surfaces().front();surface.paint_sampling_attachment.reset();surface.paint_corner_coordinates.clear();check(rejects([&]{detail::normal_frame_export(surface,detail::SourceMorphSpace::Native,{},true);}),"Missing complete native key paint stream cannot silently fall back to authored UV0");}
  doc.save_async(dir/"following.dsculpt").get();check(Scene::load(dir/"following.dsculpt").encode()==exact,"Following native Save remains exact after current key export");
 }
 std::cout<<"PASS "<<checks<<" actual native36 key export graphical controls"<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
