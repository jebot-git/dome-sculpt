// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NATIVE_PAINT32_RENDER_NO_MAIN
#include "native-paint32-render.cpp"
#undef DOME_NATIVE_PAINT32_RENDER_NO_MAIN
#include "dome/emission-native.hpp"
#include "dome/gltf-export.hpp"
#include "emission-export.hpp"
#include "gltf-document.hpp"
#include "humanoid-sha256.hpp"
#include <fstream>
namespace {
std::vector<uint8_t> bytes32(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
uint32_t word32(std::span<const uint8_t> b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
detail::GltfDocument document32(const std::vector<uint8_t> &bytes){detail::GltfDocument d;auto n=word32(bytes,12);d.json=nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+n);d.binary.assign(bytes.begin()+28+n,bytes.end());return d;}
EmissionInterchangeImage field32(const detail::GltfDocument &d){const auto &record=d.json.at("extras").at("domeSculptEmission").at("images").at(0);const auto &v=d.json.at("bufferViews").at(record.at("bufferView").get<size_t>());return decode_emission_exr(std::span<const uint8_t>(d.binary).subspan(v.at("byteOffset").get<size_t>(),v.at("byteLength").get<size_t>()));}
void resign32(detail::GltfDocument &d){auto &marker=d.json["extras"]["domeSculptEmission"];marker["binarySha256"]=creator_detail::sha256(d.binary);auto copy=d.json;copy["extras"]["domeSculptEmission"].erase("documentSha256");marker["documentSha256"]=creator_detail::sha256(nlohmann::json::to_cbor(copy));}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);NativeGraphics graphics;Fbo fbo;fbo.bind();SurfaceRenderer renderer;renderer.set_frame_time(0);prepared_normal_consumer::Renderer independent;unsigned number=0;
 for(auto alpha:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend})for(bool hdr:{false,true}){
  auto source=native_grid(alpha);if(hdr)source.active().material.emissive={2.4f,.015f,.8f};auto scene=native_split(source);const auto exact=scene.encode();
  auto owner=std::make_shared<NativeEmissionExport>();owner->scene=std::make_shared<const Scene>(scene.snapshot());owner->scope=NativeEmissionExport::Scope::WholeScene;owner->allow_measured_core_fallback=true;GltfExportOptions options;options.emission=owner;
  const auto file=out/(std::to_string(number)+".glb");auto surfaces=emission_export_surfaces(*owner->scene);check(surfaces.size()==1,"Native32 physical package has its genuine current subject");auto report=export_glb(surfaces,file,options);check(!report.diagnostics.empty(),"Native32 physical package reports finite fallback error");const auto bytes=bytes32(file);auto d=document32(bytes);const auto &package=d.json.at("extras").at("domeSculptEmission");const auto &record=package.at("images").at(0);check(package.at("version")==3&&record.at("domain").at("kind")=="native-paint32"&&record.at("domain").at("texCoord")==1,"Package3 explicitly owns the true retained paint coordinate meaning");auto restored=extract_native_emission(bytes);check(restored&&restored->encode()==exact,"Package3 independently replays coordinates/float planes and recovers exact Scene32");
  const auto expected=detail::native_paint_emission_field(scene.active(),{}),actual=field32(d);check(actual.width==expected.width&&actual.height==expected.height&&actual.pixels==expected.pixels,"Embedded FLOAT32 EXR preserves all actual radiance/relative/physical/coverage texels exactly");
  Document doc;doc.replace(scene);doc.shading(false);settled(doc);fbo.bind();const auto native=draw(renderer,doc.frame().surface);prepared_normal_consumer::Model model(file);fbo.bind();independent.render(model.data,nullptr);auto portable=read();std::cout<<"NATIVE32_HDR alpha="<<int(alpha)<<" hdr="<<hdr<<" max="<<delta(native,portable)<<std::endl;check(delta(native,portable)<=2,"Independent package3 PNG/factor/strength consumer keeps unchanged2 bound");
  Document opened;opened.replace(*restored);opened.shading(false);settled(opened);fbo.bind();check(draw(renderer,opened.frame().surface)==native,"Restored package3 native renderer retains exact physical pixels");
  const auto actual_worker=out/(std::to_string(number)+"-worker.glb");doc.export_asset(actual_worker,AssetExportFormat::Glb).get();auto worker_restored=extract_native_emission(bytes32(actual_worker));check(worker_restored&&worker_restored->encode()==exact,"Actual ordinary physical Document worker now emits/reopens genuine native32 package");
  auto next=std::make_shared<NativeEmissionExport>(*owner);next->scene=std::make_shared<const Scene>(restored->snapshot());options.emission=next;const auto second=out/(std::to_string(number)+"-second.glb");export_glb(emission_export_surfaces(*next->scene),second,options);check(bytes32(second)==bytes,"Native32 physical package second cycle preserves exact bytes and graph size");
  if(number==0){
   options.emission=owner;auto foreign=surfaces;foreign[0].material_bindings=std::shared_ptr<const SourceMaterialBindings>(surfaces[0].material_bindings.get(),[](const SourceMaterialBindings*){});check(rejects([&]{export_glb(foreign,out/"foreign-controlblock.glb",options);}),"Same-address foreign material control block cannot borrow physical Scene authority");foreign=surfaces;foreign[0].paint_corner_coordinates[0][0]+=.01f;check(rejects([&]{export_glb(foreign,out/"wrong-corner.glb",options);}),"Foreign current paint corners cannot borrow physical Scene authority");
   auto changed=d;auto &bad=changed.json["extras"]["domeSculptEmission"]["images"][0];bad["domain"]["texCoord"]=0;changed.json["materials"][bad["material"].get<size_t>()]["emissiveTexture"]["texCoord"]=0;resign32(changed);detail::write_glb(std::move(changed),out/"wrong-basis.glb");check(rejects([&]{extract_native_emission(bytes32(out/"wrong-basis.glb"));}),"Rehashed wrong UV0 basis refuses actual owning corner replay");
   changed=d;changed.json["extras"]["domeSculptEmission"]["images"][0].erase("domain");resign32(changed);detail::write_glb(std::move(changed),out/"missing-domain.glb");check(rejects([&]{extract_native_emission(bytes32(out/"missing-domain.glb"));}),"Package3 without actual domain witness fails closed");
  }
  check(scene.encode()==exact,"Physical export/negative/worker/recovery never changes current native source");++number;
 }
 std::cout<<"PASS "<<checks<<" native32 owning physical emission checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
