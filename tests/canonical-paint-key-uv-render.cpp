// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NATIVE_PAINT32_RENDER_NO_MAIN
#include "native-paint32-render.cpp"
#undef DOME_NATIVE_PAINT32_RENDER_NO_MAIN
#include "canonical-paint-sampling.hpp"
namespace {
Scene keep(const Scene &source){
 const auto &o=source.active();UvWorkspace uv(o.painted->mesh(),o.painted->resolution());uv.edit(UvAction::Scale,.8f);const auto view=uv.view();
 check(view.valid&&view.mesh&&view.source_vertices,"Actual shared UV workspace produces a valid .8 scale/map");
 const auto result=detail::CanonicalPaintSampling::create(source,o.id,*view.mesh,*view.source_vertices);
 auto next=source;next.active().painted=result.paint;next.active().material_bindings=result.binding;next.active().deformation=result.deformation;next.validate();return next;
}
}
int main(int argc,char **argv)try{
 if(argc!=4)return 2;const auto original=Scene::load(argv[1]);const auto raster=Scene::load(argv[2]);const std::filesystem::path out=argv[3];std::filesystem::create_directories(out);const auto exact=original.encode();
 auto once=keep(original);auto twice=keep(once);check(twice.active().painted->mesh().uv==raster.active().painted->mesh().uv&&twice.active().painted->mesh().indices==raster.active().painted->mesh().indices,"Two actual .8 workspace scales match the original failed raster layout exactly");
 twice.save(out/"retained.dsculpt");auto restored=Scene::load(out/"retained.dsculpt");check(restored.encode()==twice.encode(),"Actual repeated writable/key owners persist exactly before graphical consumption");
 NativeGraphics graphics;std::cout<<"ACTUAL_RENDERER "<<glGetString(GL_RENDERER)<<std::endl;Fbo fbo;SurfaceRenderer renderer;renderer.set_frame_time(0);prepared_normal_consumer::Renderer independent;
 std::vector<Scene> exports;std::vector<Pixels> references;unsigned n=0;for(int pose=0;pose<3;++pose)for(float weight:{0.f,.35f,1.f}){
  auto before=original,after=restored;before.active().transform.scale=after.active().transform.scale=5;posed(before,pose,weight);posed(after,pose,weight);
  Document b,a;b.replace(before);a.replace(after);b.shading(false);a.shading(false);settled(b);settled(a);fbo.bind();auto bp=draw(renderer,b.frame().surface);fbo.bind();auto ap=draw(renderer,a.frame().surface);
  auto prefix=out/std::to_string(n++);joint::capture(prefix.string()+"-before.png",bp);joint::capture(prefix.string()+"-retained.png",ap);const auto error=delta(bp,ap);std::cout<<"RETAINED_GRID pose="<<pose<<" morph="<<weight<<" max="<<error<<std::endl;check(error<=2,"Identical original colour counterexample retains unchanged strict2 gate");
  const auto stats=renderer.texture_stats();const auto buffers=renderer.buffer_stats();fbo.bind();check(draw(renderer,a.frame().surface)==ap&&renderer.texture_stats().uploads==stats.uploads&&renderer.texture_stats().pack_builds==stats.pack_builds&&renderer.buffer_stats().geometry_uploads==buffers.geometry_uploads,"Second eye reuses genuine writable/normal owner streams exactly");
  exports.push_back(after);references.push_back(ap);
 }
 for(size_t i=0;i<exports.size();++i){
  Document a;a.replace(exports[i]);a.shading(false);settled(a);auto prefix=out/std::to_string(i);auto path=prefix.string()+".glb";a.export_asset(path,AssetExportFormat::Glb).get();settled(a);prepared_normal_consumer::Model compiled(path);fbo.bind();independent.render(compiled.data);auto parsed=read();joint::capture(prefix.string()+"-portable.png",parsed);const auto pe=delta(references[i],parsed);std::cout<<"PARSED_GLB image="<<i<<" max="<<pe<<std::endl;check(pe<=2,"Actual native fileworker GLB independently preserves unchanged strict2 current-image gate");
 }
 check(original.encode()==exact,"All pose/UV/current/export work leaves exact saved source unchanged");std::cout<<"PASS "<<checks<<" exact-input retained key/UV graphical controls\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
