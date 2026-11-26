// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/emission-native.hpp"
#include "dome/source-maps.hpp"
#include "emission-bake-source.hpp"
#include "gltf-document.hpp"
#include "humanoid-sha256.hpp"
#include "paint-overlay-sample.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string_view>

namespace {
using namespace dome;
using Json=nlohmann::json;
size_t checks=0;
void check(bool value,const char *message){
 if(!value)throw std::runtime_error(message);
 ++checks;
}
void refuses(auto action,std::string_view message){
 try{action();}
 catch(const std::exception &e){
  check(std::string_view(e.what()).find(message)!=std::string_view::npos,e.what());
  return;
 }
 throw std::runtime_error("Expected refusal");
}
Mesh quad(){
 Mesh m;
 m.positions={{-.75f,-.75f,0},{.75f,-.75f,0},{.75f,.75f,0},{-.75f,.75f,0}};
 m.normals.assign(4,{0,0,1});
 m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};
 m.indices={0,1,2,0,2,3};
 return m;
}
void stamp(PaintedMesh &paint,Vec3 position,std::array<float,3> emission){
 PaintSettings ink;
 ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;
 ink.emission=emission;ink.front_only=false;
 Brush brush;
 brush.from=brush.to=position;brush.normal={0,0,1};brush.radius=.32f;brush.strength=.83f;
 paint.begin_stroke();paint.stamp(brush,ink);
 check(paint.end_stroke(),"Real sparse source paint stroke commits");
}
std::vector<uint8_t> read(const std::filesystem::path &path){
 std::ifstream f(path,std::ios::binary);
 if(!f)throw std::runtime_error("Could not read exported GLB");
 return {std::istreambuf_iterator<char>(f),{}};
}
detail::GltfDocument document(const std::vector<uint8_t> &bytes){
 const size_t n=uint32_t(bytes.at(12))|uint32_t(bytes.at(13))<<8|
                uint32_t(bytes.at(14))<<16|uint32_t(bytes.at(15))<<24;
 return {Json::parse(bytes.begin()+20,bytes.begin()+20+n),{bytes.begin()+28+n,bytes.end()}};
}
void check_samples(const Scene &scene,uint64_t source){
 const auto original=detail::emission_bake_source(scene.object(source));
 const auto packet=detail::PaintOverlayFactory::capture(*scene.active().painted,{});
 const auto domain=packet->baked()->emission_domain;
 float max_mapped=0,max_identity=0;
 for(int y=1;y<64;++y)for(int x=1;x<64;++x){
  const float u=float(x*4+1)/256,v=float(y*4+3)/256;
  const auto expected=v<=u?original(0,{1-u,u-v,v}):original(1,{1-v,u,v-u});
  const float tu=.625f-.5f*u+.125f*v,tv=.25f+.5f*v;
  const auto actual=detail::sample_overlay_values(*packet,tu,tv,false);
  const auto wrong=domain->field()->sample(tu,tv).absolute;
  for(size_t channel=0;channel<3;++channel){
   max_mapped=std::max(max_mapped,std::abs(expected[channel]-actual[4][channel]));
   max_identity=std::max(max_identity,std::abs(expected[channel]-wrong[channel]));
  }
 }
 std::cout<<"MAPPED source oracle probes=3969 maximum="<<max_mapped<<" identity_error="<<max_identity<<'\n';
 check(max_mapped<=2e-5f&&max_identity>.5f,"Exact source function survives off-grid mapped sampling with a sensitive wrong-coordinate control");
}
} // namespace

int main(int argc,char **argv)try{
 if(argc!=2)return 2;
 const std::filesystem::path output=argv[1];
 std::filesystem::create_directories(output);
 Scene scene;
 const auto source=scene.active().id;
 auto &from=scene.active();
 from.mesh_active=true;from.material.emissive={.07f,.12f,.01f};
 from.painted=std::make_shared<PaintedMesh>(quad(),64);
 stamp(*from.painted,{-.32f,0,0},{4,.06f,.2f});
 stamp(*from.painted,{.21f,.16f,0},{.3f,1.9f,.08f});
 auto target_mesh=quad();
 for(auto &uv:target_mesh.uv)uv={.625f-.5f*uv[0]+.125f*uv[1],.25f+.5f*uv[1]};
 const auto target=scene.add("Mirrored inset atlas",Volume{});
 scene.select(target);
 scene.active().mesh_active=true;
 scene.active().painted=std::make_shared<PaintedMesh>(target_mesh,128);
 ObjectBakeSettings settings;
 settings.front_mm=settings.back_mm=2;settings.ao_samples=0;settings.padding=0;
 const auto baseline=scene.encode();
 auto incompatible=target_mesh;
 incompatible.uv[3][0]+=.015625f;
 refuses([&]{detail::BakedEmissionDomain::capture(scene,source,std::make_shared<const Mesh>(incompatible),128,
                 scene.world_transform(target));},"lack one exact target-to-source paint mapping");
 Document app;
 app.replace(scene.snapshot());
 check(app.wait_mesh(),"Actual Document publishes the initial mesh");
 app.bake_from_object(source,settings);
 const auto file=output/"mapped.dsculpt";
 app.save_async(file).get();
 check(app.wait_asset()&&app.wait_mesh()&&app.frame().state->error.empty(),"Actual Bake worker and following ordered Save complete");
 const auto baked=app.copy_scene();
 const auto domain=baked.active().painted->baked()->emission_domain;
 const detail::BakedEmissionDomain::Mapping expected{{{-2,.5f,1.125f},{0,2,-.5f}}};
 check(domain&&domain->mapping()==expected&&domain->field()->resolution()==64&&domain->target_resolution()==128,
       "Bake retains the true mirrored source64 grid under the current128 paint grid");
 check_samples(baked,source);
 const auto exact=baked.encode();
 check(app.undo()&&app.wait_mesh()&&app.copy_scene().encode()==baseline,"Bake is exactly one Undo");
 check(app.redo()&&app.wait_mesh()&&app.copy_scene().encode()==exact,"Redo restores exact certified owner");
 const auto reopened=Scene::load(file);
 check(reopened.encode()==exact,"Native reopen preserves exact independent coordinates and owners");
 check_samples(reopened,source);
 auto record=domain->encode();
 check(record.at("version")==2&&record.at("coordinates")==Json(expected),"Mapped native source carries explicit coordinate version");
 auto malformed=record;
 malformed["coordinates"][0].push_back(17);
 refuses([&]{detail::BakedEmissionDomain::preflight_archive(malformed);},"Invalid retained emission coordinate record");
 record["coordinates"][0][2]=1.f;
 refuses([&]{detail::BakedEmissionDomain::decode(record,baked.active().painted->mesh_owner(),128);},
         "changed its certified coordinates");
 const auto glb=output/"mapped.glb";
 app.export_asset(glb,AssetExportFormat::Glb).get();
 const auto bytes=read(glb);
 const auto imported=extract_native_emission(bytes);
 check(imported&&imported->encode()==exact,"Native GLB replay restores the exact mapped project");
 check_samples(*imported,source);
 auto package=document(bytes);
 auto &marker=package.json.at("extras").at("domeSculptEmission");
 bool found=false;
 for(auto &entry:marker.at("images"))if(entry.at("object")==target){
  auto &witness=entry.at("retainedEmission");
  check(witness.at("version")==2&&witness.at("coordinates")==Json(expected),"GLB witnesses its native mapping separately from target UV0");
  witness["coordinates"][0][2]=1.f;
  found=true;
 }
 check(found,"Actual retained target exported");
 auto digest=package.json;
 digest["extras"]["domeSculptEmission"].erase("documentSha256");
 marker["documentSha256"]=creator_detail::sha256(Json::to_cbor(digest));
 const auto forged=output/"forged.glb";
 detail::write_glb(std::move(package),forged);
 refuses([&]{extract_native_emission(read(forged));},"witness changed its certified coordinates");
 check(app.copy_scene().encode()==exact,"All replay/refusal checks preserve the live project");
 std::cout<<"PASS "<<checks<<" mapped HDR emission controls\n";
 return 0;
}catch(const std::exception &e){
 std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';
 return 1;
}
