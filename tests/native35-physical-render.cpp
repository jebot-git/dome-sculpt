// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NATIVE_PAINT32_RENDER_NO_MAIN
#include "native-paint32-render.cpp"
#undef DOME_NATIVE_PAINT32_RENDER_NO_MAIN
#include "dome/emission-native.hpp"
#include <fstream>
namespace {
std::vector<uint8_t> physical_bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
}
int main(int argc,char**argv)try{
 if(argc!=3)return 2;const std::filesystem::path input=argv[1],out=argv[2];std::filesystem::create_directories(out);NativeGraphics graphics;std::cout<<"ACTUAL_RENDERER "<<glGetString(GL_RENDERER)<<std::endl;Fbo fbo;SurfaceRenderer renderer;renderer.set_frame_time(0);prepared_normal_consumer::Renderer independent;
 for(int variant:{0,7})for(int operation:{0,1,3,4}){
  const auto path=input/(std::to_string(variant)+"-"+std::to_string(operation)+".dsculpt");if(!std::filesystem::exists(path)){std::cout<<"MISSING_CURRENT_OWNER "<<path<<std::endl;continue;}const auto source=Scene::load(path);const auto source_exact=source.encode();
  for(int pose=0;pose<3;++pose){auto current=source;if(current.active().deformation){auto d=*current.active().deformation;if(d.morphs){d.morph_weights.assign(d.morphs->targets.size(),0);if(!d.morph_weights.empty())d.morph_weights[0]=.17f;if(operation>=3)d.morph_weights.back()=pose==0?0:pose==1?.35f:1.f;else if(!d.morph_weights.empty())d.morph_weights.back()=pose==0?0:pose==1?.35f:1.f;}if(d.skeleton&&pose){d.pose.clear();for(const auto &bone:d.skeleton->bones)d.pose.push_back(bone.rest);d.pose[0].scale=pose==1?Vec3{1.1f,.85f,.55f}:Vec3{.8f,1.05f,1.6f};const float a=pose==1?.2f:-.25f;d.pose[1].rotation={std::cos(a/2),0,std::sin(a/2),0};}current.set_deformation(current.active().id,d);}
   Document doc;doc.replace(current);doc.shading(false);settled(doc);fbo.bind();const auto native=draw(renderer,doc.frame().surface);const auto prefix=out/(std::to_string(variant)+"-"+std::to_string(operation)+"-"+std::to_string(pose));const auto exact=doc.copy_scene().encode();const auto file=prefix.string()+".glb";doc.export_asset(file,AssetExportFormat::Glb).get();settled(doc);auto restored=extract_native_emission(physical_bytes(file));check(restored&&restored->encode()==exact&&doc.copy_scene().encode()==exact,"Same-object physical/normal export recovers exact native graph without changing current authoring");
   prepared_normal_consumer::Model model(file);fbo.bind();independent.render(model.data);const auto portable=read();const auto error=delta(native,portable);std::cout<<"CANONICAL_PHYSICAL_IMAGE variant="<<variant<<" operation="<<operation<<" pose="<<pose<<" max="<<error<<std::endl;joint::capture(prefix.string()+"-native.png",native);joint::capture(prefix.string()+"-portable.png",portable);check(error<=4,"Independent PNG/factor/strength +true normal/skin/morph consumer preserves unchanged4 image bound");
   Document reopened;reopened.replace(*restored);reopened.shading(false);settled(reopened);fbo.bind();check(draw(renderer,reopened.frame().surface)==native,"Recovered current physical/normal project restores all native pixels exactly");const auto stats=renderer.texture_stats();fbo.bind();check(draw(renderer,reopened.frame().surface)==native&&renderer.texture_stats().uploads==stats.uploads&&renderer.texture_stats().pack_builds==stats.pack_builds,"Second current physical/normal eye reuses exact float and normal packets");
  }
  check(source.encode()==source_exact,"Mixed physical/normal pose and export controls preserve immutable actual source");
 }
 std::cout<<"PASS "<<checks<<" current physical/normal independent consumer controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
