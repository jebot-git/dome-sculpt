// SPDX-License-Identifier: GPL-3.0-or-later
#include "cgltf.h"
#include "dome/material-bindings.hpp"
#include "dome/scene.hpp"
#include "dome/humanoid-creator.hpp"
#include "../src/gltf-document.hpp"
#include <iostream>
#include <memory>
using namespace dome;
namespace {int checks=0;void check(bool v,const char*s){if(!v)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<'\n';}bool near(Vec3 a,Vec3 b){return length(a-b)<1e-6f;}
const cgltf_accessor *attribute(const cgltf_attribute *a,size_t count,cgltf_attribute_type type,int index=0){for(size_t n=0;n<count;++n)if(a[n].type==type && a[n].index==index)return a[n].data;return nullptr;}
Vec3 value(const cgltf_accessor *a,size_t index){float v[4]{};if(!a || !cgltf_accessor_read_float(a,index,v,4))throw std::runtime_error("Missing parsed vector");return {v[0],v[1],v[2]};}
}
int main(int argc,char**argv)try {
 if(argc!=2)return 2;std::filesystem::path path=argv[1];
 Mesh m;m.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};m.normals.assign(4,{0,0,1});m.indices={0,1,2,0,2,3};m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};
 auto raw=std::make_shared<ImportedAsset>();raw->mode=ImportMode::PreserveGltf;raw->materials.emplace_back();ImportedPrimitive p;p.mesh=m;p.source_mesh=p.source_primitive=0;p.uv1={{{.1f,.2f}},{{.3f,.4f}},{{.5f,.6f}},{{.7f,.8f}}};p.colors={{{.1f,.2f,.3f,1}},{{.2f,.3f,.4f,1}},{{.3f,.4f,.5f,1}},{{.4f,.5f,.6f,1}}};p.morph_targets.resize(1);raw->primitives.push_back(p);
 auto appearance=std::make_shared<CreatorAppearance>();appearance->asset=raw;appearance->source_mesh=appearance->source_primitive=0;appearance->source_vertices={0,1,2,3};appearance->source_morph_targets={0};appearance->source_basis_quaternion=Quat{};
 auto maps=std::make_shared<PaintSourceMaps>();maps->normal.image=std::make_shared<const Image>(Image{1,1,{255,128,128,255}});auto paint=PaintedMesh::from_maps(m,maps,64);
 auto state=std::make_shared<DeformationState>(m);state->morphs=MorphSet(m);auto moved=m.positions;for(auto &v:moved)v.z+=.1f;state->morphs->set_target(m,"native",moved);
 auto origin=std::make_shared<SourceUvOrigin>();origin->paint=paint->snapshot();origin->deformation=state;origin->appearance=appearance;
 auto f=std::make_shared<NormalFrames>();f->origin_vertex_count=f->source_vertex_count=4;f->source_corner_count=6;f->origin_vertices=f->source_vertices=m.indices;f->source_corners={0,1,2,3,4,5};f->normals.assign(6,{0,0,1});f->tangents.assign(6,{1,0,0,1});f->targets.push_back({1,std::vector<Vec3>(6,Vec3{.1f,0,-.01f}),std::vector<Vec3>(6,Vec3{0,.2f,0})});f->targets[0].tangents[3].y=.4f;
 auto registry=std::make_shared<NormalFrameSet>();registry->pbr=f;registry->targets={{1,0,0,{}}};registry->current_targets={1};registry->next_target=2;
 auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=false;binding->uv_origin=origin;binding->normal_frames=registry;binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;binding->validate();
 auto current=std::make_shared<DeformationState>(*state);current->skeleton=Skeleton{{{"root",-1,{},1}}};current->skin=SkinWeights{};current->skin->vertices.assign(4,{{0,1}});current->pose={{{},{},{2,1,.5f}}};current->morph_weights={.35f};
 ObjectSurface object;object.id=1;object.name="Explicit native frames";object.mesh=m;object.textures=paint->preview(object.material);object.deformation=current;object.appearance=appearance;object.material_bindings=binding;
 export_glb({object},path);cgltf_options options{};cgltf_data *pointer=nullptr;check(cgltf_parse_file(&options,path.c_str(),&pointer)==cgltf_result_success,"Independent cgltf parses the frame-aware exported GLB");std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(pointer,cgltf_free);
 check(cgltf_load_buffers(&options,pointer,path.c_str())==cgltf_result_success && cgltf_validate(pointer)==cgltf_result_success,"Exported frame geometry, skin, morphs and images pass independent cgltf validation");
 const auto &primitive=pointer->meshes[0].primitives[0];auto position=attribute(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_position),normal=attribute(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_normal),tangent=attribute(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_tangent);auto uv1=attribute(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_texcoord,1),color=attribute(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_color);
 check(position && position->count==5 && tangent && tangent->count==5,"A differing target tangent splits the shared atlas vertex before all attributes are emitted");
 bool exact=true;for(size_t c=0;c<m.indices.size();++c){auto v=cgltf_accessor_read_index(primitive.indices,c);auto original=m.indices[c];exact&=near(value(position,v),m.positions[original])&&near(value(normal,v),f->normals[c]);auto t=value(tangent,v);exact&=near(t,{1,0,0});float q[4]{};cgltf_accessor_read_float(uv1,v,q,2);exact&=q[0]==p.uv1[original][0] && q[1]==p.uv1[original][1];cgltf_accessor_read_float(color,v,q,4);for(size_t k=0;k<4;++k)exact&=q[k]==p.colors[original][k];auto dn=attribute(primitive.targets[0].attributes,primitive.targets[0].attributes_count,cgltf_attribute_type_normal),dt=attribute(primitive.targets[0].attributes,primitive.targets[0].attributes_count,cgltf_attribute_type_tangent),dp=attribute(primitive.targets[0].attributes,primitive.targets[0].attributes_count,cgltf_attribute_type_position);exact&=near(value(dn,v),f->targets[0].normals[c])&&near(value(dt,v),f->targets[0].tangents[c])&&near(value(dp,v),current->morphs->targets[0].delta[original]);}
 check(exact,"Every parsed corner retains exact POSITION/NORMAL/TANGENT targets, UV1 and COLOR0 through the split mapping");
 auto joints=attribute(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_joints),weights=attribute(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_weights);bool rig=true;for(size_t v=0;v<position->count;++v){float j[4]{},w[4]{};cgltf_accessor_read_float(joints,v,j,4);cgltf_accessor_read_float(weights,v,w,4);rig&=j[0]==0 && w[0]==1;}check(rig && pointer->skins_count==1 && pointer->meshes[0].weights_count==1 && pointer->meshes[0].weights[0]==.35f,"The same parsed split vertices retain current skin influences and morph preview weights");
 check(binding->normal_frames==registry && paint->mesh().positions.size()==4 && current->morphs->targets[0].delta.size()==4,"Export leaves native rest geometry, authoring frames and current deformation immutable");
 {
  auto nframe=std::make_shared<NormalFrames>(*f);nframe->tangents.clear();for(auto &target:nframe->targets)target.tangents.clear();
  auto nset=std::make_shared<NormalFrameSet>(*registry);nset->pbr=nframe;auto nbinding=std::make_shared<SourceMaterialBindings>(*binding);nbinding->normal_frames=nset;
  auto nmaps=std::make_shared<TextureMaps>(*object.textures);auto nsources=std::make_shared<PaintSourceMaps>(*nmaps->source_maps);nsources->normal={};nmaps->source_maps=nsources;
  auto nobject=object;nobject.material_bindings=nbinding;nobject.textures=nmaps;auto out=detail::make_gltf({nobject});const auto &attrs=out.json["meshes"][0]["primitives"][0]["attributes"];
  check(attrs.contains("NORMAL") && !attrs.contains("TANGENT") && out.json["meshes"][0]["primitives"][0]["targets"][0].contains("NORMAL"),"N-only exports preserve authored normal keys without inventing a tangent or UV authority");
  auto npath=path.parent_path()/(path.stem().string()+"-normal-only.glb");nobject.textures.reset();export_glb({nobject},npath);cgltf_data *nparsed=nullptr;bool valid=cgltf_parse_file(&options,npath.c_str(),&nparsed)==cgltf_result_success;std::unique_ptr<cgltf_data,decltype(&cgltf_free)> nfile(nparsed,cgltf_free);valid=valid && cgltf_load_buffers(&options,nparsed,npath.c_str())==cgltf_result_success && cgltf_validate(nparsed)==cgltf_result_success;
  check(valid && !attribute(nparsed->meshes[0].primitives[0].attributes,nparsed->meshes[0].primitives[0].attributes_count,cgltf_attribute_type_tangent) && attribute(nparsed->meshes[0].primitives[0].targets[0].attributes,nparsed->meshes[0].primitives[0].targets[0].attributes_count,cgltf_attribute_type_normal),"Independent cgltf validates an N-only GLB with its original normal morph and no dummy tangent accessor");
  auto mixedset=std::make_shared<NormalFrameSet>(*registry);mixedset->mtoon=nframe;auto mixedbinding=std::make_shared<SourceMaterialBindings>(*binding);mixedbinding->has_mtoon=true;mixedbinding->material=MToonMaterial::from_vrm0({{"shader","VRM/MToon"}});mixedbinding->normal_frames=mixedset;auto mixed=object;mixed.material_bindings=mixedbinding;auto compatible=detail::make_gltf({mixed},detail::SourceMorphSpace::Original);
  check(compatible.json["meshes"][0]["primitives"][0]["attributes"].contains("TANGENT"),"N-only MToon can share the exact normal channels and retain a PBR-only required tangent authority");
  auto different=std::make_shared<NormalFrames>(*nframe);different->targets[0].normals[0].x+=.02f;mixedset=std::make_shared<NormalFrameSet>(*mixedset);mixedset->mtoon=different;mixedbinding=std::make_shared<SourceMaterialBindings>(*mixedbinding);mixedbinding->normal_frames=mixedset;mixed.material_bindings=mixedbinding;bool guarded=false;try{detail::make_gltf({mixed},detail::SourceMorphSpace::Original);}catch(const std::invalid_argument &e){guarded=std::string(e.what()).find("PBR fallback and MToon require different retained normal attributes")!=std::string::npos;}
  check(guarded,"A differing normal target channel refuses incompatible PBR and MToon primitive sharing even without a MToon normal texture");
 }
 {
  auto flat=std::make_shared<NormalFrames>(*f);flat->targets[0].normals.clear();flat->targets[0].tangents.clear();
  auto canonical=std::make_shared<NormalFrames>(*flat);canonical->transport=NormalFrameTransport::CanonicalMikk;canonical->generated_tangents=true;canonical->generated_uv=0;
  auto flatset=std::make_shared<NormalFrameSet>(*registry);flatset->pbr=flat;flatset->canonical=canonical;auto flatbinding=std::make_shared<SourceMaterialBindings>(*binding);flatbinding->normal_frames=flatset;
  auto bakedpaint=paint->snapshot();BakedTextures bake;auto solid=[](std::array<uint8_t,4>pixel){Image out{64,64,std::vector<uint8_t>(64*64*4)};for(size_t n=0;n<out.rgba.size();++n)out.rgba[n]=pixel[n%4];return out;};bake.maps.color=solid({170,149,124,255});bake.maps.metal_rough=solid({255,204,0,255});bake.maps.emission=solid({0,0,0,255});bake.maps.normal=solid({128,204,230,255});bake.coverage.assign(64*64,255);bakedpaint->install_bake(bake);
  auto rigid=object;rigid.material_bindings=flatbinding;rigid.textures=bakedpaint->preview(rigid.material);auto exact=detail::make_gltf({rigid});
  check(exact.objects[0].canonical_normal.image.get()==&rigid.textures->baked_normal && exact.json["materials"][0]["normalTexture"]["scale"]==1,"Full-coverage matching flat frames with rigid anisotropic skinning export the original raw canonical image at unit strength");
  auto refuses=[&](const ObjectSurface &candidate){try{detail::make_gltf({candidate});return false;}catch(const std::invalid_argument &e){return std::string(e.what()).find("standard representable target basis")!=std::string::npos;}};
  auto fractional=std::make_shared<TextureMaps>(*rigid.textures);fractional->baked_coverage.rgba[0]=128;auto partial=rigid;partial.textures=fractional;
  check(refuses(partial),"Fractional canonical coverage refuses an unsupported material flatten instead of changing normal evaluation");
  auto island=rigid;for(auto &uv:island.mesh.uv){uv[0]=.25f+.5f*uv[0];uv[1]=.25f+.5f*uv[1];}island.textures=fractional;auto sparse=detail::make_gltf({island});
  check(sparse.objects[0].canonical_normal.image!=nullptr,"Unused atlas gaps are permitted after the continuous triangle sampling footprint is proven fully opaque");
  auto edge=std::make_shared<TextureMaps>(*fractional);edge->baked_coverage.rgba[(32*64+15)*4]=128;island.textures=edge;
  check(refuses(island),"Coverage proof includes bilinear texel support outside triangle centers rather than testing only covered centers");
  auto curved=std::make_shared<NormalFrames>(*flat);curved->normals[1]=normalized(Vec3{.2f,0,1});auto curvedcanonical=std::make_shared<NormalFrames>(*curved);curvedcanonical->transport=NormalFrameTransport::CanonicalMikk;curvedcanonical->generated_tangents=true;curvedcanonical->generated_uv=0;auto curvedset=std::make_shared<NormalFrameSet>(*flatset);curvedset->pbr=curved;curvedset->canonical=curvedcanonical;auto curvedbinding=std::make_shared<SourceMaterialBindings>(*flatbinding);curvedbinding->normal_frames=curvedset;auto unsupported=rigid;unsupported.material_bindings=curvedbinding;
  check(refuses(unsupported),"Curved raw canonical interpolation refuses silently normalized standard columns");
  auto skinned=std::make_shared<DeformationState>(*current);skinned->skeleton->bones.push_back({"second",-1,{},1});skinned->pose.push_back({});skinned->skin->vertices[0]={{0,.5f},{1,.5f}};unsupported=rigid;unsupported.deformation=skinned;
  check(refuses(unsupported),"Blended joint weights cannot pass the rigid per-triangle canonical export proof");
 }
 std::cout<<"Frame export checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
