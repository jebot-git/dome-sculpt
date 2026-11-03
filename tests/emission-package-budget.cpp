// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/emission-native.hpp"
#include "dome/material-bindings.hpp"
#include "emission-package.hpp"
#include "gltf-document.hpp"
#include "humanoid-sha256.hpp"
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
namespace allocation_probe {
std::atomic_bool armed=false;
std::atomic_size_t large=0;
constexpr size_t field_bytes=1024*1024*sizeof(std::array<float,10>);
}
void *operator new(size_t count){if(allocation_probe::armed&&count>=allocation_probe::field_bytes)++allocation_probe::large;if(auto p=std::malloc(count?count:1))return p;throw std::bad_alloc();}
void *operator new[](size_t count){return ::operator new(count);}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}
void operator delete[](void*p,size_t)noexcept{std::free(p);}
namespace {
using namespace dome;
using J=nlohmann::json;
unsigned checks=0;
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<std::endl;}
Mesh quad(){Mesh m;m.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};m.normals.assign(4,{0,0,1});m.indices={0,1,2,0,2,3};m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};return m;}
std::vector<uint8_t> bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
EmissionInterchangeImage constant(uint32_t resolution){EmissionInterchangeImage field;field.width=field.height=resolution;field.pixels.assign(size_t(resolution)*resolution,{10,.005f,.0003f,0,0,0,0,0,0,0});return field;}
std::string hash(std::span<const uint8_t> v){return creator_detail::sha256(v);}
void rebind(detail::GltfDocument &doc){while(doc.binary.size()%4)doc.binary.push_back(0);doc.json["buffers"]={{{"byteLength",doc.binary.size()}}};auto &package=doc.json["extras"]["domeSculptEmission"];package["binarySha256"]=hash(doc.binary);package.erase("documentSha256");package["documentSha256"]=hash(J::to_cbor(doc.json));}
}
int main(int argc,char**argv)try{
 using namespace dome;if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 Scene scene;auto &first=scene.active();first.mesh_active=true;first.painted=std::make_shared<PaintedMesh>(quad(),64);first.material.emissive={10,.005f,.0003f};auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=false;binding->physical_emission=first.material.emissive;first.material_bindings=binding;const auto id=first.id;for(unsigned i=1;i<4;++i)scene.duplicate(id);scene.validate();
 NativeEmissionExport owner;owner.scene=std::make_shared<const Scene>(scene.snapshot());owner.scope=NativeEmissionExport::Scope::WholeScene;owner.allow_measured_core_fallback=true;std::vector<ObjectSurface> surfaces;std::vector<uint64_t> ids;for(const auto &object:scene.objects()){ObjectSurface s;s.id=object.id;s.name=object.name;s.transform=object.transform;s.material=object.material;s.material.emissive={};s.mesh=object.painted->mesh();surfaces.push_back(std::move(s));ids.push_back(object.id);}
 auto doc=detail::make_gltf(surfaces);std::vector<detail::EmissionExportImage> fields;for(size_t i=0;i<ids.size();++i){detail::EmissionExportImage field;field.object=ids[i];field.node=doc.objects[i].node;field.material=doc.objects[i].material;field.image=constant(64);field.fallback_factor={0,0,0};field.maximum_absolute_error=10;field.maximum_relative_error=1;field.erased_nonzero_samples=64*64*3;fields.push_back(std::move(field));}
 detail::attach_native_emission(doc,owner,ids,fields,256*1024*1024,{});const auto small=out/"small-valid.glb";detail::write_glb(doc,small);auto restored=extract_native_emission(bytes(small));check(restored&&restored->encode()==scene.encode(),"Genuine constant physical background and four native owners restore exactly below aggregate field quota");
 // Adversarial but internally digest-bound input: these are exact constant
 // radiance fields sampled more densely, not fabricated source-authority seals.
 auto large=constant(1024);const auto encoded=encode_emission_exr(large);while(doc.binary.size()%4)doc.binary.push_back(0);const size_t offset=doc.binary.size(),view=doc.json["bufferViews"].size();doc.binary.insert(doc.binary.end(),encoded.begin(),encoded.end());doc.json["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",encoded.size()}});{auto &entry=doc.json["extras"]["domeSculptEmission"]["images"][0];entry["bufferView"]=view;entry["sha256"]=hash(encoded);entry["fallback"]["erasedNonzeroSamples"]=1024*1024*3;}rebind(doc);const auto below=out/"large-valid.glb";detail::write_glb(doc,below);allocation_probe::large=0;allocation_probe::armed=true;auto below_restored=extract_native_emission(bytes(below));allocation_probe::armed=false;check(below_restored&&below_restored->encode()==scene.encode()&&allocation_probe::large==1,"Genuine mixed-resolution constant fields below decoded aggregate cap still restore exactly with one expected pixel allocation");for(auto &entry:doc.json["extras"]["domeSculptEmission"]["images"]){entry["bufferView"]=view;entry["sha256"]=hash(encoded);entry["fallback"]["erasedNonzeroSamples"]=1024*1024*3;}rebind(doc);const auto oversized=out/"aggregate-rebound.glb";detail::write_glb(doc,oversized);const auto captured=bytes(oversized);
 allocation_probe::large=0;allocation_probe::armed=true;std::string refusal;try{auto result=extract_native_emission(captured);allocation_probe::armed=false;check(result&&result->encode()==scene.encode(),"Old reader's over-budget decoded fields still returned the exact genuine native owner");}catch(const std::exception&e){allocation_probe::armed=false;refusal=e.what();}
 std::cout<<"DECODED_FIELD_BYTES="<<4*allocation_probe::field_bytes<<" ENCODED_FIELD_BYTES="<<encoded.size()*4<<" LARGE_PIXEL_ALLOCATIONS="<<allocation_probe::large<<" REFUSAL="<<refusal<<std::endl;
 check(refusal.find("aggregate decoded EXR")!=std::string::npos&&allocation_probe::large==0,"Aggregate decoded EXR quota rejects before any full pixel allocation");
 auto fresh=detail::make_gltf(surfaces);auto oversized_fields=fields;for(auto &field:oversized_fields){field.image=large;field.erased_nonzero_samples=1024*1024*3;}bool writer_refused=false;try{detail::attach_native_emission(fresh,owner,ids,oversized_fields,256*1024*1024,{});}catch(const std::length_error&e){writer_refused=std::string_view(e.what()).find("aggregate decoded EXR")!=std::string_view::npos;}check(writer_refused&&scene.encode()==owner.scene->encode(),"Private writer mirrors public decoded-field forecast before native/EXR serialization and preserves exact owner");
 std::cout<<"Native HDR aggregate field budget: "<<checks<<" checks\n";return 0;
}catch(const std::exception&e){allocation_probe::armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
