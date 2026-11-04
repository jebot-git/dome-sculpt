// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_mtoon_graphical_fixture
#include "mtoon-render.cpp"
#undef main
#include "prepared-normal-consumer.hpp"
#include "dome/document.hpp"
#include "dome/scene-file.hpp"
#include "dome/native-authoring.hpp"
namespace {
void finish_conversion_frame(Document &d){if(!d.wait_asset(std::chrono::seconds(120))||!d.wait_mesh(std::chrono::seconds(120))||!d.frame().state->error.empty())throw std::runtime_error("Converted normal render worker: "+d.frame().state->error);}
void png(const std::filesystem::path &path,const std::vector<uint8_t> &pixels){const auto data=Image{96,96,pixels}.png();std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(data.data()),data.size());}
}
int main(int argc,char**argv)try{
 if(argc<3||argc>4||(argc==4&&std::string_view(argv[3])!="--hardware"))return 2;
 const std::filesystem::path source=argv[1],out=argv[2];std::filesystem::create_directories(out);
 NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
 std::cout<<"GL_VENDOR "<<vendor<<"\nGL_RENDERER "<<device<<std::endl;
 if(argc==4)check(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos&&device.find("llvmpipe")==std::string::npos&&device.find("UHD")==std::string::npos,"Actual strict Intel Arc A770 device executes layered normal consumer images");
 Fbo fbo;SurfaceRenderer native;prepared_normal_consumer::Renderer independent;
 int variant=0;
 for(const char *name:{"converted-0","converted-7","partial-rebake","clear-then-rebake","cleared-0","cleared-7","legacy-upgrade","legacy-clear"}){
  auto converted=Scene::load(source/(std::string(name)+".dsculpt"));converted.active().transform.scale=5;
  for(int pose=0;pose<3;++pose)for(float weight:{0.f,.35f,1.f}){
   auto scene=converted;auto &o=scene.active();const bool has_detail=o.painted->baked()&&bool(o.painted->baked()->normal_field);if(o.deformation){auto d=*o.deformation;if(d.morphs)d.morph_weights.assign(d.morphs->targets.size(),weight);
    if(d.skeleton&&pose){for(const auto &bone:d.skeleton->bones)d.pose.push_back(bone.rest);
     d.pose[0].scale=pose==1?Vec3{1.1f,.85f,.55f}:Vec3{.8f,1.05f,1.6f};
     const float angle=pose==1?.2f:-.25f;d.pose[1].rotation={std::cos(angle/2),0,std::sin(angle/2),0};d.pose[1].scale=pose==1?Vec3{.9f,1.1f,1.5f}:Vec3{1.15f,.8f,.7f};}
    scene.set_deformation(o.id,d);}
   Document doc;doc.replace(scene);doc.shading(false);finish_conversion_frame(doc);
   const auto path=out/("view-"+std::to_string(variant)+"-"+std::to_string(pose)+"-"+std::to_string(int(weight*100))+".glb");
   doc.export_asset(path,AssetExportFormat::EditableGlb256).get();prepared_normal_consumer::Model file(path);
   fbo.bind();const auto wanted=draw(native,doc.frame().surface);const auto stats=native.texture_stats();
   fbo.bind();const auto repeated=draw(native,doc.frame().surface);
   check(repeated==wanted&&native.texture_stats().uploads==stats.uploads&&native.texture_stats().pack_builds==stats.pack_builds,"Converted native second eye reuses immutable normal/source texture packs");
   fbo.bind();independent.render(file.data);const auto actual=read();const auto error=delta(wanted,actual);
   size_t visible=0;for(size_t at=3;at<wanted.size();at+=4)visible+=wanted[at]&&actual[at];
   std::cout<<"CANONICAL_CONVERTED_EXPORT variant="<<variant<<" pose="<<pose<<" weight="<<weight<<" visible="<<visible<<" max="<<error<<std::endl;
   if(error>4){png(out/"failed-native.png",wanted);png(out/"failed-consumer.png",actual);}
   check(visible>200&&error<=4,"Independent parsed GLB/PNG skin/morph normal consumer matches the actual converted native model");
   if(pose==0&&weight==0){auto &normal=file.data->meshes[0].primitives[0].material->normal_texture;const auto saved=normal.texture;normal.texture=nullptr;fbo.bind();independent.render(file.data);const auto absent=read();normal.texture=saved;
    check(has_detail?saved&&delta(absent,actual)>2:!saved,"Independent emitted normal pixels follow the actual active or cleared baked role");
    std::ifstream input(path,std::ios::binary);std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};auto embedded=extract_native_authoring(bytes);
    check(embedded&&embedded->encode()==scene.encode(),"Actual editable GLB recovers complete Scene33 layers, original source and current pose");
   }
  }
  ++variant;
 }
 std::cout<<"PASS "<<checks<<" independent layered normal image checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
