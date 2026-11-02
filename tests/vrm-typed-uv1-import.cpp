// SPDX-License-Identifier: GPL-3.0-or-later
#define main matcap_fixture_suite
#include "vrm-matcap-import.cpp"
#undef main
#include "dome/document.hpp"
#include <algorithm>
int main(int argc,char **argv)try{
 if(argc!=2)return 2;std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 auto fixture=source(false);import_fixture::Fixture original;
 fixture.j["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_1"]=original.j["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_1"];
 fixture.j["materials"][0]["extensions"]["VRMC_materials_mtoon"]["shadeMultiplyTexture"]={{"index",0},{"texCoord",1}};
 fixture.j["samplers"][0]={{"wrapS",33071},{"wrapT",33071},{"magFilter",9729},{"minFilter",9729}};
 auto path=out/"toon-shade-only-uv1.vrm";fixture.save(path);Scene empty;const auto before=empty.encode();
 ImportAuthoringOptions options;options.paint_resolution=64;options.prepare_texture_atlas=true;
 auto imported=prepare_authoring_import(empty,path,options);
 check(imported.objects.size()==2&&empty.encode()==before,"Typed-only shade UV1 imports both real source instances with explicit atlas choice");
 double max_error=0;
 for(auto id:imported.objects){const auto &object=imported.scene.object(id);const auto &binding=*object.material_bindings;const auto &origin=*binding.uv_origin;
  check(binding.sampling_reparameterized&&binding.material.textures[size_t(MToonSlot::Shade)].texcoord==0&&origin.material->textures[size_t(MToonSlot::Shade)].texcoord==1,"Current atlas resolves the typed role while first-source descriptor retains original UV1 selection");
  check(!origin.paint->source_maps()&&object.appearance->asset->primitives[0].mesh.uv.empty(),"No fabricated PBR map or original UV0 stream is required to own typed-only UV1 sampling");
  const auto &raw=object.appearance->asset->primitives[0];const auto &mesh=object.painted->mesh();const auto &view=binding.roles[size_t(MToonSlot::Shade)];
  for(size_t c=0;c<mesh.indices.size();c+=3){std::array<float,2> target{},uv{};for(size_t k=0;k<3;++k){auto v=mesh.indices[c+k];auto original_vertex=object.appearance->source_vertices[v];for(size_t axis=0;axis<2;++axis){target[axis]+=mesh.uv[v][axis]/3;uv[axis]+=raw.uv1[original_vertex][axis]/3;}}
   // Independent analytic bilinear oracle for the fixture's 2x2 red/green/blue/white image.
   double x=std::clamp(double(uv[0])*2-.5,0.,1.),y=std::clamp(double(uv[1])*2-.5,0.,1.);
   std::array<double,4> expected{(1-x)*(1-y)+x*y,x,(y),((1-x)*(1-y)*32+x*(1-y)*96+(1-x)*y*160+x*y*255)/255};
   auto sampled=sample_source_texture(view,target[0],target[1],true);for(size_t channel=0;channel<4;++channel)max_error=std::max(max_error,std::abs(sampled[channel]-expected[channel]));
  }
 }
 check(max_error<=2./255,"Independent original-image/UV1 bilinear oracle matches the prepared shade atlas within two linear channel steps");
 auto encoded=imported.scene.encode();auto saved=out/"typed-uv1.dsculpt";imported.scene.save(saved);
 check(Scene::decode(encoded).encode()==encoded&&Scene::load(saved).encode()==encoded,"Typed-only source UV1, role descriptors and exact source graph survive native restart");
 Document document;ImportSettings settings;settings.paint_resolution=64;settings.prepare_texture_atlas=true;document.import_asset(path,settings);
 check(document.wait_asset()&&document.wait_mesh()&&document.frame().state->error.empty(),"Actual native import worker accepts the typed-only UV1 source");
 auto accepted=document.copy_scene().encode();auto current=document.copy_scene();const auto &object=current.active();auto geometry=object.deformation?object.deformation->preview(object.painted->mesh()):object.painted->mesh();
 auto local=geometry.positions[geometry.indices[0]]*.15f+geometry.positions[geometry.indices[1]]*.1f+geometry.positions[geometry.indices[2]]*.75f;auto world=current.world_matrix(object.id).point({local.x,local.y,local.z});Vec3 point{float(world[0]),float(world[1]),float(world[2])};auto normal=document.gradient(point);
 PaintSettings ink;ink.color={.3f,.9f,.2f,1};document.paint_settings(ink);NativeStrokeSample stroke;stroke.origin=point+normal;stroke.direction=normal*-1.f;stroke.brush.radius=.05f;
 check(document.begin_native_stroke()&&document.sample_native_stroke(stroke),"Real native paint worker accepts the prepared typed-only UV1 geometry");document.end_native_stroke();document.save_async(out/"worker.dsculpt").get();
 check(document.wait_native_stroke()&&document.wait_mesh()&&document.frame().state->error.empty(),"Immediately ordered Save follows the actual typed-only UV1 paint stroke");
 auto painted=document.copy_scene().encode();check(painted!=accepted&&Scene::load(out/"worker.dsculpt").encode()==painted,"Native Save writes the complete painted current role/source endpoint");
 check(document.undo()&&document.copy_scene().encode()==accepted&&document.redo()&&document.copy_scene().encode()==painted,"Native paint has its own exact Undo/Redo endpoint after typed-only import");
 check(document.undo()&&document.undo()&&document.copy_scene().encode()==before&&document.redo()&&document.copy_scene().encode()==accepted,"One separate import Undo/Redo retains exact typed UV1 identity and both source instances");
 std::cout<<"Typed-only UV1 checks "<<checks<<" max linear error "<<max_error<<'\n';return 0;
}catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
