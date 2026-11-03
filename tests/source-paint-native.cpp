// SPDX-License-Identifier: GPL-3.0-or-later
#define main established_graphics_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "dome/source-paint.hpp"
#include <filesystem>
#include "render-paint-overlay.hpp"
int main(int argc,char **argv)try{
 if(argc!=2&&(argc!=3||std::string_view(argv[2])!="--hardware"))return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 auto raw=quad();raw.uv={{-1,-1},{2,-1},{2,2},{-1,2}};auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image({180,120,60,255});
 PaintSurfaceOptions policy;policy.policy=PaintUvPolicy::SourceAuthored;auto before=PaintedMesh::from_maps(raw,maps,64,policy);
 Scene scene;scene.active().painted=prepare_source_paint(before,quad(),std::array<uint32_t,4>{0,1,2,3});scene.active().mesh_active=true;scene.active().material=white();
 const auto baseline=scene.encode();Document doc;doc.replace(scene);check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Actual native worker publishes original-source paint mesh");
 check(doc.copy_scene().encode()==baseline,"Native initial publication preserves exact source owner");
 PaintSettings ink;ink.color={.1f,.8f,.3f,.6f};doc.paint_settings(ink);NativeStrokeSample sample;sample.origin={0,0,1};sample.direction={0,0,-1};sample.brush.radius=.35f;
 check(doc.begin_native_stroke()&&doc.sample_native_stroke(sample),"Actual native input reaches source-preserving writable grid");doc.end_native_stroke();const auto file=out/"native-source-paint.dsculpt";doc.save_async(file).get();
 check(doc.wait_native_stroke()&&doc.wait_mesh()&&doc.frame().state->error.empty(),"Accepted source paint and immediately queued native Save finish without error");
 const auto painted=doc.copy_scene().encode();check(painted!=baseline&&Scene::load(file).encode()==painted,"Actual queued Save reopens exact accepted source paint");
 auto frame=doc.frame();check(frame.surface&&!frame.surface->ranges.empty()&&frame.surface->ranges[0].textures&&frame.surface->ranges[0].textures->overlay,"Native immutable mesh snapshot contains the sparse original-source overlay");
 NativeGraphics graphics;
 const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
 std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<'\n';
 check(argc<3||(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos&&device.find("llvmpipe")==std::string::npos),"Requested native hardware run requires actual A770");
 Fbo fbo;fbo.bind();SurfaceRenderer renderer;renderer.set_output_encoding(SurfaceOutputEncoding::Linear);const auto pixels=draw(renderer,frame.surface);
 check(at(pixels)[3]>0,"Actual worker snapshot reaches production EGL SurfaceRenderer");
 auto owner=doc.copy_scene().active().painted->preview(scene.active().material)->overlay;
 const std::array<uint32_t,6> corners{0,1,2,0,2,3};auto packed_maps=std::make_shared<PaintSourceMaps>(*owner->source_maps());
 auto coordinates=std::make_shared<std::vector<std::array<float,2>>>();for(auto id:corners)coordinates->push_back(owner->source_maps()->uv1->at(id));packed_maps->uv1=coordinates;
 auto packed=detail::PaintOverlayFactory::pack_source_maps(owner,packed_maps,corners);
 check(owner->original_uv0()&&*owner->original_uv0()==raw.uv&&!owner->original_uv1(),"Actual native source packet retains logical UV0 without fabricating authored UV1");
 std::vector<std::array<float,2>> expected_uv;for(auto id:corners)expected_uv.push_back(raw.uv[id]);
 check(packed->original_uv0()&&*packed->original_uv0()==expected_uv&&!packed->original_uv1(),"Actual render expansion preserves exact logical UV0 in its own six-vertex domain");
 check(packed->source_maps()==packed_maps&&packed->contribution_owner()==owner&&packed->layers().data()==owner->layers().data(),"Validated packed metadata shares its real immutable contribution owner without copying layers");
 detail::PaintOverlayCompiler compiler;compiler.prepare({{owner,true},{packed,true}});
 check(compiler.size()==1&&compiler.at(owner,true).planes==compiler.at(packed,true).planes,"Packed render metadata reuses all five actual GPU contribution planes");
 auto bad=std::make_shared<PaintSourceMaps>(*packed_maps);bad->color.wrap_s=33648;
 check(rejects([&]{detail::PaintOverlayFactory::pack_source_maps(owner,bad,corners);}),"Packed metadata cannot change the actual source sampler");
 bad=std::make_shared<PaintSourceMaps>(*packed_maps);bad->color.image=std::make_shared<const Image>(*bad->color.image);
 check(rejects([&]{detail::PaintOverlayFactory::pack_source_maps(owner,bad,corners);}),"Equal-valued foreign images cannot borrow original packed sampling ownership");
 bad=std::make_shared<PaintSourceMaps>(*packed_maps);auto wrong=std::make_shared<std::vector<std::array<float,2>>>(*coordinates);wrong->at(1)[0]+=.125f;bad->uv1=wrong;
 check(rejects([&]{detail::PaintOverlayFactory::pack_source_maps(owner,bad,corners);}),"Packed coordinates require bit-exact ordered source correspondence");
 check(rejects([&]{detail::PaintOverlayFactory::pack_source_maps(owner,packed_maps,corners,[]{return true;});})&&doc.copy_scene().encode()==painted,"Packed-owner cancellation preserves exact source paint and native scene");

 check(doc.undo()&&doc.copy_scene().encode()==baseline&&doc.redo()&&doc.copy_scene().encode()==painted,"Actual native stroke has exact global Undo and Redo endpoints");
 check(doc.wait_mesh()&&doc.begin_native_stroke()&&doc.sample_native_stroke(sample),"Second actual source gesture starts after redo");doc.end_native_stroke(false);check(doc.wait_native_stroke()&&doc.wait_mesh()&&doc.copy_scene().encode()==painted,"Cancelled native gesture preserves exact accepted source/history state");
 Document restarted;restarted.replace(Scene::load(file));check(restarted.wait_mesh()&&restarted.frame().state->error.empty(),"Fresh native Document reopens source ownership and overlay");
 check(draw(renderer,restarted.frame().surface)==pixels,"Fresh native restart renders exact accepted worker frame");
 std::cout<<"PASS "<<checks<<" actual native source-paint transaction checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
