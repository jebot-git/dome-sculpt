// SPDX-License-Identifier: GPL-3.0-or-later
#include "bake-fixture-identity.hpp"
#define main prior_material_suite
#include "mtoon-render.cpp"
#undef main
#include "current-color-target-fixture.hpp"
#include "dome/uv.hpp"
#include "source-color-consumer.hpp"
#include <set>
namespace {
void ready(Document &doc){if(!doc.wait_asset()||!doc.wait_mesh()||!doc.frame().state->error.empty())throw std::runtime_error("Current colour UV consumer: "+doc.frame().state->error);}
void move_front(Document &doc){doc.begin_uv();if(!doc.wait_uv())throw std::runtime_error("Current colour UV init timeout");const auto mesh=doc.frame().state->uv->mesh;size_t face=0;while(face<mesh->indices.size()/3&&mesh->normals[mesh->indices[face*3]].z<.9f)++face;
 std::array<float,2> cursor{};for(size_t k=0;k<3;++k){auto q=mesh->uv[mesh->indices[face*3+k]];for(size_t d=0;d<2;++d)cursor[d]+=q[d]/3;}doc.edit_uv(UvAction::Pick,cursor[0],cursor[1]);if(!doc.wait_uv())throw std::runtime_error("Current colour UV Pick timeout");doc.edit_uv(UvAction::Move,1.f/256,0);if(!doc.wait_uv()||!doc.frame().state->uv->valid)throw std::runtime_error("Current colour UV draft invalid");doc.end_uv(true);ready(doc);
}
}
int main(int argc,char **argv)try{
 if(argc<2)return 2;const auto directory=std::filesystem::path(argv[1]);std::filesystem::create_directories(directory);
 NativeGraphics context;const auto device=std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));std::cout<<"GPU "<<device<<std::endl;check(argc<3||std::string_view(argv[2])!="--hardware"||device.find("A770")!=std::string::npos,"Actual colour UV consumer uses the requested device");Fbo fbo;fbo.bind();SurfaceRenderer native;color_consumer::Renderer independent;size_t number=0;
 for(bool seam:{false,true})for(bool posed:{false,true})for(auto alpha:{dome::Material::AlphaMode::Opaque,dome::Material::AlphaMode::Mask,dome::Material::AlphaMode::Blend}){
  auto original=current_color_target_fixture::fixture(seam,256);const auto source=original.objects().back().id,target=original.active().id;
  for(auto id:{source,target}){auto &o=original.object(id);o.material.unlit=false;o.material.alpha=alpha;o.material.color={.72f,.83f,.61f,.7f};}
  auto &o=original.object(source);auto mesh=o.painted->mesh();for(size_t v=0;v<mesh.positions.size();++v){auto p=mesh.positions[v];mesh.normals[v]=normalized(mesh.normals[v]+dome::Vec3{.19f*p.y,.23f*p.x,.11f*p.z});}o.painted=std::make_shared<dome::PaintedMesh>(mesh,256);
  Document baked;baked.replace(original);ObjectBakeSettings settings;settings.prepare_colors=true;settings.ao_samples=0;baked.bake_from_object(source,settings);ready(baked);auto scene=baked.copy_scene();
  const auto pixels=scene.object(target).painted->baked();std::set<std::array<uint8_t,3>> normals;for(size_t at=0;at<pixels->coverage.size();++at)if(pixels->coverage[at])normals.insert({pixels->maps.normal.rgba[at*4],pixels->maps.normal.rgba[at*4+1],pixels->maps.normal.rgba[at*4+2]});check(normals.size()>4,"Actual native Bake supplies a nonconstant authored normal field for the lit UV test");
  if(posed)cut_color_fixture::pose(scene);const auto before=scene.encode();Document document;document.replace(scene);ready(document);auto old=draw(native,document.frame().surface);move_front(document);auto changed=document.copy_scene();check(changed.encode().at("version")==30&&changed.active().bake_recipe==scene.active().bake_recipe&&bool(changed.active().bake_recipe)&&document.frame().surface->source_colors.empty(),"Actual native UV publishes its own current projection without duplicate corner modulation");auto value=draw(native,document.frame().surface);const auto native_error=delta(old,value);std::cout<<"UV_NATIVE case="<<number<<" seam="<<seam<<" pose="<<posed<<" maxerror="<<native_error<<std::endl;check(native_error<=2,"Independent full-image before/after lit native UV remains within two encoded levels, including skin/morph and alpha");
  auto path=directory/(std::to_string(number)+".dsculpt");document.save(path);Document reopened;reopened.open(path);ready(reopened);check(reopened.copy_scene().encode()==changed.encode(),"Actual Save/reopen preserves the complete colour UV source DAG and current descriptor");auto glb=path;glb.replace_extension(".glb");reopened.export_asset(glb).get();color_consumer::Model model(glb);auto &primitive=model.data->meshes[0].primitives[0];check(!color_consumer::attr(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_color)&&primitive.material->pbr_metallic_roughness.base_color_texture.texture,"Export transports the actual resolved current PBR colour plane without doubled COLOR_0");fbo.bind();independent.render(model.data,nullptr,{},true);auto exported=read();const auto export_error=delta(value,exported);std::cout<<"UV_GLB case="<<number<<" maxerror="<<export_error<<std::endl;check(export_error<=1,"Independent cgltf/PNG/GLSL consumer matches the native current-colour UV model, lighting, normal, pose and alpha");
  document.restore_polygon_source();ready(document);check(document.copy_scene().encode()==before,"Actual saved UV origin Restore recovers exact prior baked normals, paint, colour and pose");++number;
 }
 std::cout<<checks<<" current-colour UV actual consumers in "<<number<<" cases\n";return 0;
}catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
