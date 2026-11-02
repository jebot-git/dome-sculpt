// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import-native.hpp"
#include "dome/import-authoring.hpp"
#include "dome/humanoid-creator.hpp"
#include <ufbx.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using namespace dome;
void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(int argc,char **argv)try {
 check(argc==3,"Provide fixture and private output directory");
 const auto output=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(output);
 const auto path=output/"named.fbx";std::ifstream original(argv[1],std::ios::binary);const std::string input{std::istreambuf_iterator<char>(original),{}};check(!input.empty(),"Read generated conformance fixture");
 auto write=[&](const auto &file,const auto &data){std::ofstream f(file,std::ios::binary);f.write(reinterpret_cast<const char*>(data.data()),data.size());check(bool(f),"Write private fixture");};
 write(path,input);write(output/"color.png",Image{2,2,{255,0,0,255,0,255,0,255,0,0,255,255,255,255,0,255}}.png());write(output/"normal.png",Image{1,1,{230,153,204,255}}.png());
 ufbx_load_opts load{};load.strict=true;load.geometry_transform_handling=UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES;load.inherit_mode_handling=UFBX_INHERIT_MODE_HANDLING_HELPER_NODES;load.target_axes=ufbx_axes_right_handed_y_up;load.target_unit_meters=1;load.space_conversion=UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
 std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)> raw(ufbx_load_file(path.c_str(),&load,nullptr),ufbx_free_scene);check(bool(raw),"Independent raw parse");
 auto capture=capture_native_source(path);ImportLimits limits;limits.mode=ImportMode::PreserveGltf;auto asset=import_glb(*capture.canonical_glb,limits);std::vector<uint32_t> materials;size_t compared=0,meshes=0;
 const ufbx_material *shared=nullptr;
 for(size_t i=0;i<asset.nodes.size();++i){const auto &n=asset.nodes[i];if(n.mesh<0)continue;auto *node=raw->nodes[i];check(node->mesh&&node->materials.count==1,"Actual raw mesh/material identity");if(shared)check(shared==node->materials[0],"The real raw meshes share exactly one material");shared=node->materials[0];
  auto primitive=asset.meshes[size_t(n.mesh)].primitives[0];const auto &p=asset.primitives[primitive];const auto &material=asset.materials[p.material];const int uv=n.name=="One"?1:0;materials.push_back(p.material);
  check(material.color_texcoord==uv&&material.normal_texcoord==uv,"Every actual material role resolves its own per-mesh named UV identity");check(material.normal_texture>=0&&material.color_texture>=0,"Both colour and normal images remain active");
  check(node->mesh->uv_sets.count==2&&std::string(node->mesh->uv_sets[size_t(uv)].name.data)=="UVMap","Independent source declares the named UVMap at different indices");
  const auto corners=nlohmann::json::parse(p.extras_json).at("dome_source_corners");check(p.tangents.size()==p.mesh.positions.size(),"Normal UV set retains its corresponding authored tangent stream");
  for(size_t v=0;v<p.mesh.positions.size();++v){auto c=corners.at(v).get<uint32_t>();auto expected=ufbx_get_vertex_vec2(&node->mesh->uv_sets[size_t(uv)].vertex_uv,c);auto actual=uv?p.uv1.at(v):p.mesh.uv.at(v);check(std::abs(actual[0]-expected.x)<1e-6&&std::abs(actual[1]-expected.y)<1e-6,"Actual canonical UV matches independent named-set source corners");auto tangent=ufbx_get_vertex_vec3(&node->mesh->uv_sets[size_t(uv)].vertex_tangent,c);check(std::abs(p.tangents[v][0]-tangent.x)<1e-6&&std::abs(p.tangents[v][1]-tangent.y)<1e-6&&std::abs(p.tangents[v][2]-tangent.z)<1e-6,"Actual canonical tangent uses the selected normal-texture UV basis");++compared;}
  ++meshes;
 }
 check(meshes==2&&materials[0]!=materials[1],"Canonical per-primitive material variants keep both distinct meanings");check(asset.images.size()==2,"Material variants do not duplicate or lose original image resources");
 ImportAuthoringOptions options;options.paint_resolution=64;options.prepare_texture_atlas=true;auto authored=prepare_authoring_import(Scene{},path,options);check(authored.objects.size()==2,"Real native authoring adopts both textured instances");auto saved=path;saved.replace_extension(".dsculpt");authored.scene.save(saved);check(Scene::load(saved).encode()==authored.scene.encode(),"Native restart preserves complete original resources/coordinates/material mappings");
 auto refused=[&](const std::string &text,const char *expected){auto p=output/"invalid.fbx";write(p,text);bool bad=false;try{capture_native_source(p);}catch(const std::exception &e){bad=std::string_view(e.what()).find(expected)!=std::string_view::npos;}check(bad,"Invalid per-primitive UV authority refuses with its exact diagnosis");};
 auto missing=input;const std::string property="P:\"UVSet\",\"KString\",\"\",\"\",\"UVMap\"";size_t at=0;while((at=missing.find(property,at))!=std::string::npos){missing.replace(at,property.size(),"P:\"UVSet\",\"KString\",\"\",\"\",\"Missing\"");++at;}refused(missing,"UV set is missing");
 auto ambiguous=input;const std::string named="Name:\"Default\"";at=0;while((at=ambiguous.find(named,at))!=std::string::npos){ambiguous.replace(at,named.size(),"Name:\"UVMap\"");++at;}refused(ambiguous,"UV set name is ambiguous");
 ImportOptions no_materials;no_materials.materials=false;auto absent=output/"invalid.fbx";write(absent,missing);auto retained=capture_native_source(absent,no_materials);auto plain=import_glb(*retained.canonical_glb,limits);check(plain.images.empty(),"Explicit disabled materials do not require or fabricate active UV bindings");
 std::cout<<"PASS "<<compared<<" independent UV/tangent corners, exact shared-material variants/images and native authoring restart\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
