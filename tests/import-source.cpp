// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import-native.hpp"
#include "dome/import-authoring.hpp"
#include "dome/import-image.hpp"
#include "dome/humanoid-creator.hpp"
#include <cstdio>
#include <jpeglib.h>
#include "import-authoring-fixtures.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
namespace fs=std::filesystem;
namespace {
size_t checks=0;
void check(bool ok,const char *what){if(!ok)throw std::runtime_error(what);++checks;std::cout<<"PASS "<<what<<'\n';}
template<class F>void refuses(F f,const char *what){bool failed=false;try{f();}catch(const std::exception &){failed=true;}check(failed,what);}
void write(const fs::path &p,std::span<const uint8_t> b){std::ofstream out(p,std::ios::binary);out.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));if(!out)throw std::runtime_error("fixture write");}
void write(const fs::path &p,std::string s){write(p,std::span(reinterpret_cast<const uint8_t*>(s.data()),s.size()));}
uint32_t word(const std::vector<uint8_t>&v,size_t at){return uint32_t(v.at(at))|uint32_t(v.at(at+1))<<8|uint32_t(v.at(at+2))<<16|uint32_t(v.at(at+3))<<24;}
}
int main(int argc,char **argv){try{
  fs::path root=argc>1?argv[1]:"import-source";fs::create_directories(root);
  ImportOptions options;check(ImportOptions::decode(options.encode())==options,"shared import options exact codec");
  options.custom_metres_per_unit=0;refuses([&]{options.validate();},"zero unit scale refused");options={};
  auto bad=options.encode();bad["normals"]=UINT64_MAX;refuses([&]{ImportOptions::decode(bad);},"malformed option enum refused");
  write(root/"asset.obj","o triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
  write(root/"raw.dat","old");
  ImportSourceCapture capture(root/"asset.obj");auto old=capture.resource("raw.dat");
  write(root/"new.dat","new");fs::rename(root/"new.dat",root/"raw.dat");
  check(capture.resource("./raw.dat")==old && std::string(old->begin(),old->end())=="old","renamed resource never reread; immutable snapshot retained");
  refuses([&]{capture.resource("../outside");},"parent resource escape refused");
  fs::remove(root/"escape");fs::create_symlink("/etc/passwd",root/"escape");
  refuses([&]{capture.resource("escape");},"symlink outside captured directory refused");
  write(root/"internal.dat","ok");fs::remove(root/"internal-link");fs::create_symlink("internal.dat",root/"internal-link");
  check(capture.resource("internal-link")->size()==2,"confined symlink capture allowed on openat2 kernel");
  import_fixture::Fixture receipt_fixture;
  auto done=capture.finish(std::make_shared<const std::vector<uint8_t>>(receipt_fixture.bytes()),"test adapter");
  check(done.original->format==ImportSourceFormat::Obj && done.original->root.bytes->size()>0,"raw format tag distinct from canonical adapter");
  auto receipt=ImportSourceReceipt::decode(done.original->encode());
  check(receipt->encode()==done.original->encode(),"root/resources/options exact binary receipt roundtrip");
  bad=done.original->encode();bad["resources"].push_back(bad["root"]);refuses([&]{ImportSourceReceipt::decode(bad);},"duplicate archived resource identity refused");
  ImportLimits small;small.max_file_bytes=4;refuses([&]{ImportSourceCapture c(root/"asset.obj",{},small);},"captured root byte quota enforced");
  small={};small.cancelled=[]{return true;};refuses([&]{ImportSourceCapture c(root/"asset.obj",{},small);},"capture cancellation before publication");
  ImportSourceCapture canceled(root/"asset.obj",{},ImportLimits{});refuses([&]{canceled.resource("missing");},"missing dependency explicit error");
  // Separate source buffers are rebased without changing primitive/accessor IDs.
  import_fixture::Fixture fixture;auto glb=fixture.bytes();
  auto json=nlohmann::json::parse(glb.begin()+20,glb.begin()+20+word(glb,12));
  auto bin_at=20+word(glb,12);std::vector<uint8_t> binary(glb.begin()+bin_at+8,glb.end());
  json["buffers"][0]["uri"]="geometry%20data.bin";
  write(root/"geometry data.bin",binary);auto original_json=json.dump(2);write(root/"scene.gltf",original_json);
  auto converted=capture_gltf_source(root/"scene.gltf");
  auto asset=import_glb(*converted.canonical_glb);
  check(asset.primitives.size()==1 && asset.nodes.size()==3,"external glTF captured and parsed with source identities");
  check(std::string(converted.original->root.bytes->begin(),converted.original->root.bytes->end())==original_json && converted.original->resources[0].bytes->size()==binary.size(),"external glTF exact root and binary resources retained");
  check(converted.original->format==ImportSourceFormat::Gltf && converted.canonical_glb!=converted.original->root.bytes,"canonical GLB never labelled original JSON");
  write(root/"passthrough.glb",glb);auto passthrough=capture_gltf_source(root/"passthrough.glb");
  check(passthrough.canonical_glb==passthrough.original->root.bytes,"self contained GLB aliases single captured original");
  json["buffers"][0]["uri"]="../bad.bin";write(root/"escape.gltf",json.dump());refuses([&]{capture_gltf_source(root/"escape.gltf");},"escaped glTF URI refused before parser adoption");
  auto native=capture_native_source(root/"asset.obj");auto obj=import_glb(*native.canonical_glb);
  check(obj.primitives.size()==1 && obj.primitives[0].mesh.positions.size()==3,"native OBJ polygon converted in C++ worker module");
  check(obj.primitives[0].normals_authored && obj.primitives[0].mesh.normals[0].z==1,"native OBJ missing normal flat policy explicit in canonical adapter");
  check(native.original->format==ImportSourceFormat::Obj && native.original->root.bytes!=native.canonical_glb && !native.diagnostics.empty(),"unitless OBJ assumed metre choice diagnosed and raw exact");
  options.units=ImportUnits::Millimetres;auto metric=capture_native_source(root/"asset.obj",options);auto metric_asset=import_glb(*metric.canonical_glb);
  bool scaled=false;for(const auto &n:metric_asset.nodes)scaled|=std::abs(n.local[0]-.001f)<1e-9;
  check(scaled,"explicit OBJ millimetre units applied as hierarchy conversion");
  write(root/"material.mtl","newmtl Red\nKd 1 0 0\n");write(root/"material.obj","mtllib material.mtl\no material\nv 0 0 0\nv 1 0 0\nv 0 1 0\nusemtl Red\nf 1 2 3\n");
  auto material=capture_native_source(root/"material.obj");auto ma=import_glb(*material.canonical_glb);
  check(material.original->resources.size()==1 && material.original->resources[0].name=="material.mtl","OBJ MTL captured through confined native callback");
  check(ma.materials.at(ma.primitives[0].material).color[0]==1&&ma.materials.at(ma.primitives[0].material).color[1]==0,"native OBJ material factor conversion");
  fs::create_directories(root/"materials");fs::create_directories(root/"maps");
  Image native_color{2,2,{255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255}};
  write(root/"maps"/"red.png",native_color.png());
  write(root/"materials"/"nested.mtl","newmtl Painted\nKd 1 1 1\nmap_Kd ../maps/red.png\n");
  write(root/"textured.obj","mtllib materials/nested.mtl\no textured\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nusemtl Painted\nf 1/1 2/2 3/3\n");
  auto textured=capture_native_source(root/"textured.obj");auto ta=import_glb(*textured.canonical_glb);
  check(textured.original->resources.size()==2 && ta.images.size()==1 && ta.materials.at(ta.primitives[0].material).color_texture>=0,
      "nested OBJ MTL texture relative paths captured and embedded without filesystem reread");
  auto textured_scene=prepare_authoring_import(Scene{},root/"textured.obj",{64,{},{}});
  auto textured_restart=Scene::decode(textured_scene.scene.encode());
  check(textured_restart.object(textured_scene.objects[0]).appearance->decoded_images->at(0)->rgba==native_color.rgba,
      "textured OBJ native painting source and raw bundle survive restart");
  write(root/"second.mtl","newmtl Second\nKd 0 1 0\n");
  write(root/"multi.obj","mtllib materials/nested.mtl second.mtl\no multi\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nusemtl Painted\nf 1/1 2/2 3/3\nusemtl Second\nf 1/1 2/2 3/3\n");
  auto multi=capture_native_source(root/"multi.obj");auto multiple=import_glb(*multi.canonical_glb);
  check(multi.original->resources.size()==3 && multiple.primitives.size()==2,"all OBJ mtllib files captured with their distinct material directories");
  auto authored=prepare_authoring_import(Scene{},root/"scene.gltf",{64,{},{}});
  auto scene_json=authored.scene.encode();check(scene_json.at("version")==11,"converted original-source receipt selects Scene11");
  auto restored=Scene::decode(scene_json);const auto &appearance=*restored.object(authored.objects.front()).appearance;
  check(appearance.source_receipt && appearance.source_receipt->root.bytes &&
      std::string(appearance.source_receipt->root.bytes->begin(),appearance.source_receipt->root.bytes->end())==original_json,
      "Scene11 restart preserves exact glTF JSON separately from canonical authority");
  check(appearance.source_receipt->resources[0].bytes->size()==binary.size() && *appearance.original_bytes==*converted.canonical_glb,
      "Scene11 restart retains canonical and binary resource identities");
  auto down=scene_json;down["version"]=10;refuses([&]{Scene::decode(down);},"older Scene versions reject raw-source receipts");
  auto native_authored=prepare_authoring_import(Scene{},root/"asset.obj",{64,{},{}});
  auto native_restored=Scene::decode(native_authored.scene.encode());
  check(native_restored.object(native_authored.objects[0]).appearance->source_receipt->format==ImportSourceFormat::Obj,
      "actual native OBJ authoring and save restart retain original format");
  // Authored fixture, no external FBX SDK or fixture runtime dependency.
  write(root/"triangle.fbx",R"FBX(; FBX 7.4.0 project file
FBXHeaderExtension: { FBXHeaderVersion: 1003
 FBXVersion: 7400
}
GlobalSettings: {
 Version: 1000
 Properties70: {
  P: "UpAxis", "int", "Integer", "",1
  P: "UpAxisSign", "int", "Integer", "",1
  P: "FrontAxis", "int", "Integer", "",2
  P: "FrontAxisSign", "int", "Integer", "",1
  P: "CoordAxis", "int", "Integer", "",0
  P: "CoordAxisSign", "int", "Integer", "",1
  P: "UnitScaleFactor", "double", "Number", "",100
 }
}
Objects: {
 Geometry: 10, "Geometry::Triangle", "Mesh" {
  Vertices: *9 { a: 0,0,0,1,0,0,0,1,0 }
  PolygonVertexIndex: *3 { a: 0,1,-3 }
 }
 Model: 20, "Model::Parent", "Null" { Version: 232 }
 Model: 21, "Model::One", "Mesh" { Version: 232 }
 Model: 22, "Model::Two", "Mesh" {
  Version: 232
  Properties70: { P: "Lcl Translation", "Lcl Translation", "", "A",2,0,0 }
 }
}
Connections: {
 C: "OO",20,0
 C: "OO",21,20
 C: "OO",22,20
 C: "OO",10,21
 C: "OO",10,22
}
)FBX");
  auto fbx=capture_native_source(root/"triangle.fbx");auto fa=import_glb(*fbx.canonical_glb);
  check(fbx.original->format==ImportSourceFormat::Fbx && fa.meshes.size()==1 && fa.nodes.size()==4,
      "native ASCII FBX retains parent hierarchy and mesh instances");
  size_t attached=0;bool translated=false;for(const auto &n:fa.nodes){attached+=n.mesh>=0;translated|=std::abs(n.world[12]-2)<1e-6;}
  check(attached==2 && translated && fa.primitives[0].mesh.positions[1].x==1,"FBX authored metre units and translated instance retained");
  auto fbx_authoring=prepare_authoring_import(Scene{},root/"triangle.fbx",{64,{},{}});
  check(fbx_authoring.objects.size()==2 && Scene::decode(fbx_authoring.scene.encode()).objects().size()==3,
      "actual native FBX detached authoring/save restart");
  if(argc>2){auto binary_source=capture_native_source(argv[2]);auto ba=import_glb(*binary_source.canonical_glb);
    check(!ba.primitives.empty() && binary_source.original->format==ImportSourceFormat::Fbx,"pinned upstream binary FBX smoke conversion");}
  options={};options.units=ImportUnits::Centimetres;
  auto centimetres=import_glb(*capture_native_source(root/"triangle.fbx",options).canonical_glb);
  bool translated_cm=false;for(const auto &n:centimetres.nodes)translated_cm|=std::abs(n.world[12]-.02f)<1e-7;
  check(translated_cm,"explicit FBX centimetre unit override applies to hierarchy once");
  options={};options.axes=ImportAxes::ZUp;
  auto zup=import_glb(*capture_native_source(root/"triangle.fbx",options).canonical_glb);
  bool rotated=false;for(const auto &n:zup.nodes)if(n.mesh>=0)rotated|=std::abs(n.world[6]+1)<1e-6&&std::abs(n.world[9]-1)<1e-6;
  check(rotated,"explicit FBX Z-up override rotates source axes without changing raw geometry");
  write(root/"fold.obj","o fold\nv 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\nf 1 2 3\nf 1 4 2\n");
  options={};options.normals=ImportNormals::GenerateSmooth;
  auto smooth=import_glb(*capture_native_source(root/"fold.obj",options).canonical_glb);
  auto normal=smooth.primitives[0].mesh.normals[0];
  check(std::abs(normal.y-std::sqrt(.5f))<1e-6&&std::abs(normal.z-std::sqrt(.5f))<1e-6,"explicit smooth normal option area-weights shared original control points");
  // Standard native JPEG decode, bounded output and strict truncated-input error.
  jpeg_compress_struct encoder{};jpeg_error_mgr errors{};encoder.err=jpeg_std_error(&errors);jpeg_create_compress(&encoder);
  unsigned char *jpeg_bytes=nullptr;unsigned long jpeg_size=0;jpeg_mem_dest(&encoder,&jpeg_bytes,&jpeg_size);
  encoder.image_width=2;encoder.image_height=1;encoder.input_components=3;encoder.in_color_space=JCS_RGB;
  jpeg_set_defaults(&encoder);jpeg_set_quality(&encoder,100,TRUE);jpeg_start_compress(&encoder,TRUE);
  unsigned char row_bytes[]={220,40,10,220,40,10};JSAMPROW row=row_bytes;jpeg_write_scanlines(&encoder,&row,1);jpeg_finish_compress(&encoder);jpeg_destroy_compress(&encoder);
  ImportedImage ji;ji.mime_type="image/jpeg";ji.bytes.assign(jpeg_bytes,jpeg_bytes+jpeg_size);std::free(jpeg_bytes);
  auto image=decode_import_image(ji);
  check(image.width==2 && image.height==1 && image.rgba[3]==255 && std::abs(int(image.rgba[0])-220)<=2 && std::abs(int(image.rgba[1])-40)<=2,
      "native standard JPEG RGB/opaque decode within codec quantization");
  refuses([&]{decode_import_image(ji,1);},"JPEG pixel quota checked before image allocation");
  auto jpeg_fixture=fixture;auto image_at=jpeg_fixture.bin.size();
  while(image_at%4){jpeg_fixture.bin.push_back(0);++image_at;}
  jpeg_fixture.bin.insert(jpeg_fixture.bin.end(),ji.bytes.begin(),ji.bytes.end());
  auto jpeg_view=jpeg_fixture.j["bufferViews"].size();jpeg_fixture.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",image_at},{"byteLength",ji.bytes.size()}});
  jpeg_fixture.j["images"][0]["bufferView"]=jpeg_view;jpeg_fixture.j["images"][0]["mimeType"]="image/jpeg";
  jpeg_fixture.save(root/"jpeg.glb");auto jpeg_scene=prepare_authoring_import(Scene{},root/"jpeg.glb",{64,{},{}});
  auto jpeg_saved=Scene::decode(jpeg_scene.scene.encode());const auto &jpeg_appearance=*jpeg_saved.object(jpeg_scene.objects[0]).appearance;
  check(jpeg_appearance.asset->images[0].bytes==ji.bytes && jpeg_appearance.decoded_images->at(0)->rgba==image.rgba,
      "actual JPEG glTF authoring/save restart retains encoded and decoded texture");
  refuses([&]{decode_import_image(ji,64*1024*1024,[]{return true;});},"JPEG cancellation refuses before output publication");
  ji.bytes.resize(ji.bytes.size()/2);refuses([&]{decode_import_image(ji);},"truncated JPEG refuses without publishing partial image");
  // A concurrent in-place write is distinguished from an atomic name replace:
  // the stable descriptor may retain an old inode, but mixed bytes are rejected.
  write(root/"changing.obj",std::string(200000,'a'));size_t callbacks=0;ImportLimits changing;
  changing.cancelled=[&]{if(++callbacks==4)write(root/"changing.obj",std::string(200000,'b'));return false;};
  refuses([&]{ImportSourceCapture capture_changed(root/"changing.obj",{},changing);},"in-place source mutation during capture refuses atomic import");
  std::cout<<checks<<" source capture checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
