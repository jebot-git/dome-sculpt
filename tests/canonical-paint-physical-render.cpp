// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_CANONICAL_PAINT_RENDER_NO_MAIN
#include "canonical-paint-key-uv-render.cpp"
#undef DOME_CANONICAL_PAINT_RENDER_NO_MAIN
int main(int argc,char **argv)try{
 if(argc!=3&&argc!=4)return 2;NativeGraphics graphics;device(argc==4&&std::string_view(argv[3])=="--hardware");const std::filesystem::path inputs=argv[1],out=argv[2];std::filesystem::create_directories(out);Fbo fbo;SurfaceRenderer renderer;renderer.set_frame_time(0);size_t number=0;
 for(const auto stem:{"0-0","7-0","0-3","7-4"})for(auto alpha:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend}){
  auto source=Scene::load(inputs/(std::string(stem)+".dsculpt"));source.active().material.alpha=alpha;source.active().material.color[3]=.85f;source.active().material.alpha_cutoff=.4f;source.active().transform.scale=5;
  check(source.active().painted->has_physical_emission(),"Actual source contains genuine writable physical float values before UV");const auto original=source.encode();auto changed=keep(keep(source));const auto path=out/(std::to_string(number)+".dsculpt");changed.save(path);auto reopened=Scene::load(path);check(reopened.encode()==changed.encode(),"Actual repeated native physical UV saves and reopens exact field/paint/model owners");
  const auto deformation=source.active().deformation;const int poses=deformation&&deformation->skeleton?3:1;const bool morphs=deformation&&deformation->morphs&&!deformation->morphs->targets.empty();
  for(int pose=0;pose<poses;++pose)for(float weight:{0.f,.35f,1.f}){
   if(!morphs&&weight!=0)continue;auto before=source,after=reopened;
   auto apply=[&](Scene &s){if(!s.active().deformation)return;auto d=*s.active().deformation;if(d.morphs)d.morph_weights.assign(d.morphs->targets.size(),weight);if(d.skeleton){d.pose.clear();for(const auto &bone:d.skeleton->bones)d.pose.push_back(bone.rest);if(pose){d.pose[0].scale=pose==1?Vec3{1.1f,.85f,.65f}:Vec3{.85f,1.05f,1.4f};if(d.pose.size()>1){const float a=pose==1?.2f:-.25f;d.pose[1].rotation={std::cos(a/2),0,std::sin(a/2),0};}}}s.set_deformation(s.active().id,std::move(d));};apply(before);apply(after);
   Document b,a;b.replace(before);a.replace(after);b.shading(false);a.shading(false);settled(b);settled(a);fbo.bind();auto bp=draw(renderer,b.frame().surface);fbo.bind();auto ap=draw(renderer,a.frame().surface);auto prefix=out/(std::to_string(number)+"-"+std::to_string(pose)+"-"+std::to_string(int(weight*100)));joint::capture(prefix.string()+"-before.png",bp);joint::capture(prefix.string()+"-after.png",ap);size_t covered=0;std::set<std::array<uint8_t,4>> samples;for(size_t p=0;p<bp.size();p+=4)if(bp[p+3]){++covered;samples.insert({bp[p],bp[p+1],bp[p+2],bp[p+3]});}check(covered>32&&samples.size()>1,"Actual physical source has nonblank varying visible pixels");const auto error=delta(bp,ap);std::cout<<"PHYSICAL_BEFORE_UV input="<<stem<<" alpha="<<int(alpha)<<" pose="<<pose<<" morph="<<weight<<" max="<<error<<std::endl;check(error<=2,"Physical-before-UV native pixels preserve unchanged strict2 bound with genuine independent paint/normal domains");
   const auto t=renderer.texture_stats();const auto g=renderer.buffer_stats();fbo.bind();check(draw(renderer,a.frame().surface)==ap&&renderer.texture_stats().uploads==t.uploads&&renderer.texture_stats().pack_builds==t.pack_builds&&renderer.buffer_stats().geometry_uploads==g.geometry_uploads,"Actual second physical eye reuses exact streams and float/normal packets");
  }
  check(source.encode()==original,"Physical pose and renderer consumers preserve complete original authoring source");++number;
 }
 std::cout<<"PASS "<<checks<<" native physical-before-UV image controls\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
