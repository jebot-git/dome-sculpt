// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/source-maps.hpp"
#include "dome/paint.hpp"
#include "dome/scene.hpp"
#include "dome/source-uv-bake.hpp"
#include "dome/document.hpp"
#include "cgltf.h"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
namespace {
int checks=0;
void check(bool ok,const char*text){if(!ok)throw std::runtime_error(text);++checks;std::cout<<"PASS "<<text<<'\n';}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
Mesh triangle(){Mesh m;m.positions={{0,0,0},{1,0,0},{0,1,0}};m.normals.assign(3,{0,0,1});m.uv={{{0,0}},{{1,0}},{{0,1}}};m.indices={0,1,2};return m;}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;
 auto mesh=triangle();auto image=std::make_shared<const Image>(Image{2,1,{255,0,0,255,0,0,255,255}});
 auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image;maps->color.wrap_s=maps->color.wrap_t=33071;maps->color.mag_filter=9728;maps->color.texcoord=1;
 check(rejects([&]{maps->validate();}),"UV1 sampling cannot enter storage without an owned coordinate domain");
 maps->uv1=std::make_shared<const std::vector<std::array<float,2>>>(std::vector<std::array<float,2>>{{0,0},{0,1},{1,0}});maps->emission=maps->color;maps->emission.texcoord=0;
 maps->normal.image=std::make_shared<const Image>(Image{1,1,{255,128,128,255}});maps->normal.texcoord=1;
 maps->validate_surface(mesh);check(maps->uses_uv1(),"Paired immutable UV1 domain is independently validated against current mesh vertices");
 check(rejects([&]{sample_source_texture(maps->color,1,0,false);}),"Unresolved UV1 image views retain the existing sampler refusal");
 auto a=sample_surface_texture(mesh,*maps,maps->color,0,{0,1,0},false),b=sample_surface_texture(mesh,*maps,maps->emission,0,{0,1,0},false);
 check(a[0]==1&&a[2]==0&&b[0]==0&&b[2]==1,"Per-role sampling chooses UV1 and UV0 independently instead of relabeling mesh UVs");
 auto normal=source_texture_normal(mesh,*maps,0,{.25f,.5f,.25f});
 check(normal.y>.999f&&std::abs(normal.x)<.005f&&std::abs(normal.z)<.005f,"UV1 normal derivation uses the independent swapped-coordinate tangent basis");
 auto wrong=mesh;wrong.positions.push_back({0,0,1});check(rejects([&]{maps->validate_surface(wrong);}),"UV1 authority rejects a different current vertex domain");
 check(rejects([&]{source_texture_uv(mesh,*maps,1,1,{1,0,0});})&&rejects([&]{source_texture_uv(mesh,*maps,1,0,{2,0,0});}),"Source triangle identity and normalized barycentric inputs remain validated");
 PaintImageArchive images;auto encoded=encode_source_maps(*maps,images);auto restored=decode_source_maps(encoded,images);
 check(*restored->uv1==*maps->uv1&&restored->color.image==restored->emission.image,"Versioned UV1 bytes and shared original image identities roundtrip exactly");
 auto bad=encoded;bad["source_uv1"]["vertices"]=2'000'001;check(rejects([&]{decode_source_maps(bad,images);}),"Oversized coordinate metadata is rejected before allocation");
 bad=encoded;bad["source_uv1"]["data"]=nlohmann::json::binary({1,2,3});check(rejects([&]{decode_source_maps(bad,images);}),"Partial coordinate payloads cannot be accepted as UV1 authority");
 bad=encoded;bad["source_uv1"]["version"]=2;check(rejects([&]{decode_source_maps(bad,images);}),"Unknown source-coordinate codec versions fail explicitly");
 auto paint=PaintedMesh::from_maps(mesh,maps,64);auto owner=paint->source_maps()->uv1;maps->uv1=std::make_shared<const std::vector<std::array<float,2>>>(3,std::array<float,2>{0,0});
 check(paint->source_maps()->uv1==owner&&paint->snapshot()->source_maps()->uv1==owner,"Published source coordinates retain immutable ownership through snapshots and caller bundle replacement");
 check(paint->storage_bytes()>=24,"Coordinate storage participates in native paint budgets");
 auto round=PaintedMesh::decode(paint->encode());check(*round->source_maps()->uv1==*owner,"Standalone painted payload preserves the exact source coordinate domain");
 check(paint->preview(Material{})->source_only&&rejects([&]{paint->composite(Material{});}),"Unimplemented atlas compositing remains guarded while raw source ownership is preserved");
 auto permuted=mesh;std::swap(permuted.positions[0],permuted.positions[1]);std::swap(permuted.normals[0],permuted.normals[1]);std::swap(permuted.uv[0],permuted.uv[1]);for(auto &id:permuted.indices)if(id<2)id=1-id;
 check(rejects([&]{paint->reparameterize(permuted);}),"Vertex permutation cannot silently lose source UV1 identities");
 SourceUvBakeInput input;input.paint=paint;input.target=mesh;input.source_vertices={0,1,2};SourceUvBakeOptions options;options.resolution=64;
 const auto retained=paint->encode();auto baked=bake_source_uv(input,options);auto at=(size_t(8)*64+48)*4;
 check(baked.maps->color.image->rgba[at]==255&&baked.maps->color.image->rgba[at+2]==0&&baked.maps->emission.image->rgba[at]==0&&baked.maps->emission.image->rgba[at+2]==255,"Actual atlas baker independently samples swapped UV1 color and original UV0 emission");
 check(baked.maps->normal.image->rgba[at+1]>=254&&std::abs(int(baked.maps->normal.image->rgba[at])-128)<=1,"Atlas normal conversion follows the UV1 tangent basis into the destination UV0 frame");
 check(!baked.maps->uv1&&!baked.maps->uses_uv1()&&baked.original_paint==paint&&paint->encode()==retained,"Derived atlas resolves coordinate indices while retaining exact immutable original UV1 paint");
 auto adopted=adopt_source_uv_bake(baked);check(adopted.paint->composite(Material{})->color.width==64&&adopted.material_bindings->uv_origin->paint->source_maps()->uv1==owner,"Explicit bake adoption produces editable compositing with retained source-coordinate authority");
 Scene prepared;prepared.active().mesh_active=true;prepared.active().painted=adopted.paint;prepared.active().material_bindings=adopted.material_bindings;
 auto saved_prepared=prepared.encode();check(saved_prepared["version"]==18&&*Scene::decode(saved_prepared).active().material_bindings->uv_origin->paint->source_maps()->uv1==*owner,"UV1 retained only inside a resolved source capsule still requires Scene18 and survives restart");
 saved_prepared["version"]=16;check(rejects([&]{Scene::decode(saved_prepared);}),"An older scene version cannot hide UV1 inside resolved material history");
 int polls=0;auto cancelled=options;cancelled.cancelled=[&]{return ++polls>8;};check(rejects([&]{bake_source_uv(input,cancelled);})&&paint->encode()==retained,"Cancelled UV1 atlas work preserves the original coordinates, source images and paint exactly");
 Scene scene;scene.active().mesh_active=true;scene.active().painted=paint;auto native=scene.encode();check(native["version"]==18,"Actual source-coordinate payload raises the scene feature minimum");
 auto reopened=Scene::decode(native);check(*reopened.active().painted->source_maps()->uv1==*owner,"Native scene restart retains UV1 authority");native["version"]=16;check(rejects([&]{Scene::decode(native);}),"A downgraded scene cannot smuggle source UV1 payloads into an older feature version");
 auto output=std::filesystem::path(argv[1]);output+=".glb";export_glb(scene.preview_surfaces(),output);cgltf_options parse_options{};cgltf_data *parsed=nullptr;
 check(cgltf_parse_file(&parse_options,output.c_str(),&parsed)==cgltf_result_success,"Independent cgltf parser opens the actual source UV1 GLB");
 std::unique_ptr<cgltf_data,decltype(&cgltf_free)> parsed_owner(parsed,cgltf_free);
 check(cgltf_load_buffers(&parse_options,parsed,output.c_str())==cgltf_result_success&&cgltf_validate(parsed)==cgltf_result_success,"Independent consumer loads and validates emitted binary resources");
 const auto &primitive=parsed->meshes[0].primitives[0];const cgltf_accessor *secondary=nullptr;for(size_t i=0;i<primitive.attributes_count;++i)if(primitive.attributes[i].type==cgltf_attribute_type_texcoord&&primitive.attributes[i].index==1)secondary=primitive.attributes[i].data;
 bool coordinates=secondary&&secondary->count==owner->size();if(coordinates)for(size_t vertex=0;vertex<owner->size();++vertex){float values[2]{};coordinates&=cgltf_accessor_read_float(secondary,vertex,values,2)&&values[0]==owner->at(vertex)[0]&&values[1]==owner->at(vertex)[1];}
 check(coordinates,"GLB owns the complete exact TEXCOORD_1 accessor independently of retained asset metadata");
 check(primitive.material->pbr_metallic_roughness.base_color_texture.texcoord==1&&primitive.material->normal_texture.texcoord==1&&primitive.material->emissive_texture.texcoord==0,"Each emitted material role retains its independently selected coordinate index");
 Document document;document.replace(scene);auto original_scene=document.copy_scene().encode();document.begin_uv();check(document.wait_uv(),"Actual native UV draft initializes from owned source UV1");
 document.edit_uv(UvAction::Scale,.7f);check(document.wait_uv()&&document.frame().state->uv->valid,"Native UV edit creates a valid explicit source-coordinate preparation target");
 document.end_uv(false);check(document.copy_scene().encode()==original_scene,"Cancelling the actual native draft retains the complete original UV1 scene");
 document.begin_uv();check(document.wait_uv(),"A new native UV session retains the original coordinate authority after Cancel");document.edit_uv(UvAction::Scale,.7f);check(document.wait_uv()&&document.frame().state->uv->valid,"Native UV preparation validates before acceptance");
 document.end_uv(true);check(document.wait_asset()&&document.frame().state->error.empty(),"Actual native worker accepts explicit UV1 preparation before Save becomes available");
 auto project=std::filesystem::path(argv[1]);project+="-prepared.dsculpt";document.save_async(project).get();
 auto accepted=document.copy_scene().encode();auto prepared_paint=document.copy_scene().active().painted;
 check(!prepared_paint->source_maps()->uv1&&document.copy_scene().active().material_bindings->uv_origin->paint->source_maps()->uv1==owner,"Actual native preparation resolves active maps and retains the exact original coordinate owner");
 check(Scene::load(project).encode()==accepted,"Native Save after accepted preparation reopens with prepared maps and retained UV1 history");
 check(document.undo()&&document.copy_scene().encode()==original_scene&&document.redo()&&document.copy_scene().encode()==accepted,"Source UV1 preparation is one exact native Undo and Redo transaction");
 auto ordinary=std::make_shared<PaintSourceMaps>(*paint->source_maps());ordinary->color.texcoord=ordinary->normal.texcoord=0;ordinary->uv1.reset();scene.active().painted=PaintedMesh::from_maps(mesh,ordinary,64);check(scene.encode()["version"]<18,"Ordinary UV0 sources keep their historical scene format");
 std::cout<<"Source UV1 authority checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
