// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake-authoring.hpp"
#include "dome/bake-distance.hpp"
#include "dome/material-bindings.hpp"
#include "dome/document.hpp"
#include "dome/gltf-export.hpp"
#include "emission-bake-source.hpp"
#include "emission-export.hpp"
#include "humanoid-sha256.hpp"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <limits>
#include <string_view>
#include <iostream>
#include <nlohmann/json.hpp>
#include <png.h>

namespace {
using namespace dome;
size_t checks=0;
void check(bool v,const char *message){++checks;if(!v)throw std::runtime_error(message);}
Mesh quad(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
Image image(std::array<uint8_t,4> v){Image i{64,64,std::vector<uint8_t>(4096*4)};for(size_t k=0;k<i.rgba.size();++k)i.rgba[k]=v[k%4];return i;}
struct Fixture{Scene scene;uint64_t target,source;std::vector<BakedEmissionSample> radiance;};
Fixture fixture(){
  Fixture f;f.target=f.scene.active().id;f.scene.active().mesh_active=true;f.scene.active().painted=std::make_shared<PaintedMesh>(quad(),64);
  f.scene.active().painted->add_layer("Retained hidden target");f.scene.active().painted->layer_properties("Retained hidden target",false,.25f);
  f.source=f.scene.add("Actual retained FLOAT source",Volume{});auto &o=f.scene.active();o.mesh_active=true;o.visible=false;o.locked=true;o.material.emissive={0,0,0};
  auto mesh=quad();for(auto &p:mesh.positions)p.z=.005f;o.painted=std::make_shared<PaintedMesh>(mesh,64);
  BakedTextures b;b.maps.color=image({24,65,117,255});b.maps.metal_rough=image({177,42,99,255});b.coverage.assign(4096,255);
  f.radiance.resize(4096);
  for(size_t i=0;i<4096;++i)f.radiance[i]={{},{50000.f+float(i%64)*32,.00001f*float(i/64+1),2},1};
  b.emission_field=BakedEmissionField::create(64,f.radiance);o.painted->install_bake(std::move(b));
  f.scene.select(f.target);f.scene.validate();return f;
}
void finish(Document &doc){check(doc.wait_asset(std::chrono::seconds(120)),"actual worker finishes within original120s bound");if(!doc.frame().state->error.empty())throw std::runtime_error(doc.frame().state->error);}
void saved(std::future<void> f){check(f.wait_for(std::chrono::seconds(120))==std::future_status::ready,"ordered Save finishes within120s");f.get();}
uint32_t word(std::span<const uint8_t> b,size_t p){return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;}
detail::GltfDocument gltf(const std::vector<uint8_t> &bytes){detail::GltfDocument d;const size_t n=word(bytes,12);d.json=nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+n);d.binary.assign(bytes.begin()+28+n,bytes.end());return d;}
std::vector<uint8_t> read(const std::filesystem::path &path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
Image png(std::span<const uint8_t> bytes){png_image p{};p.version=PNG_IMAGE_VERSION;struct Cleanup{png_image *p;~Cleanup(){png_image_free(p);}}cleanup{&p};if(!png_image_begin_read_from_memory(&p,bytes.data(),bytes.size()))throw std::runtime_error("Independent PNG header decode failed");p.format=PNG_FORMAT_RGBA;Image image{p.width,p.height,std::vector<uint8_t>(size_t(p.width)*p.height*4)};if(!png_image_finish_read(&p,nullptr,image.rgba.data(),0,nullptr))throw std::runtime_error("Independent PNG decode failed");return image;}
std::span<const uint8_t> view(const detail::GltfDocument &d,size_t id){const auto &v=d.json.at("bufferViews").at(id);return std::span<const uint8_t>(d.binary).subspan(v.at("byteOffset").get<size_t>(),v.at("byteLength").get<size_t>());}
void resign(detail::GltfDocument &d){auto &p=d.json["extras"]["domeSculptEmission"];p["binarySha256"]=creator_detail::sha256(d.binary);auto copy=d.json;copy["extras"]["domeSculptEmission"].erase("documentSha256");p["documentSha256"]=creator_detail::sha256(nlohmann::json::to_cbor(copy));}
size_t append(detail::GltfDocument &d,std::span<const uint8_t> bytes){d.binary.insert(d.binary.end(),(4-d.binary.size()%4)%4,0);const auto offset=d.binary.size();d.binary.insert(d.binary.end(),bytes.begin(),bytes.end());d.binary.insert(d.binary.end(),(4-d.binary.size()%4)%4,0);const auto id=d.json["bufferViews"].size();d.json["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",bytes.size()}});d.json["buffers"][0]["byteLength"]=d.binary.size();return id;}
void refuses_package(detail::GltfDocument d,const std::filesystem::path &path,const char *message){resign(d);detail::write_glb(std::move(d),path);bool caught=false;try{extract_native_emission(read(path));}catch(const std::exception&){caught=true;}check(caught,message);}
void quality_oracle(const detail::GltfDocument &d,const EmissionInterchangeImage &source){
 const auto &record=d.json.at("extras").at("domeSculptEmission").at("images").at(0),&f=record.at("fallback");
 check(f.at("version")==2&&f.at("view")=="pbr"&&f.at("units")=="strength:linear-cd/m2;maximumAbsoluteError:linear-cd/m2;maximumRelativeError:dimensionless","actual fallback2 declares exact typed view and metric units");
 const auto &material=d.json.at("materials").at(record.at("material").get<size_t>());
 const auto texture=material.at("emissiveTexture").at("index").get<size_t>();const auto image_id=d.json.at("textures").at(texture).at("source").get<size_t>();
 const auto image=png(view(d,d.json.at("images").at(image_id).at("bufferView").get<size_t>()));
 check(image.width==source.width&&image.height==source.height,"independent standard PNG decoder sees genuine current raster dimensions");
 const auto factors=f.at("emissiveFactor").get<std::array<double,3>>();const double strength=f.at("strength").get<double>();
 std::array<double,3> peak{};for(const auto &p:source.pixels)for(size_t k=0;k<3;++k)peak[k]=std::max(peak[k],double(p[k]));
 check(strength==std::max({1.,peak[0],peak[1],peak[2]})&&strength==52016,"actual FLOAT normalization preserves independently authored52016 peak above old30000 guard");
 long double maximum_absolute=0,maximum_relative=0;size_t erased=0;
 for(size_t i=0;i<source.pixels.size();++i)for(size_t k=0;k<3;++k){
  const long double x=static_cast<long double>(image.rgba[i*4+k])/255;
  const long double linear=x<=.04045L?x/12.92L:std::pow((x+.055L)/1.055L,2.4L);
  const long double reconstructed=linear*factors[k]*strength,original=source.pixels[i][k];
  const long double error=std::abs(original-reconstructed);maximum_absolute=std::max(maximum_absolute,error);
  if(original>0){maximum_relative=std::max(maximum_relative,error/original);erased+=reconstructed==0;}else check(reconstructed==0,"zero source has zero reconstructed energy");
 }
 const double precision=64*std::numeric_limits<double>::epsilon();
 check(std::abs(static_cast<long double>(f.at("maximumAbsoluteError").get<double>())-maximum_absolute)<=precision*strength,"reported physical absolute error agrees with independent extended-precision PNG reconstruction");
 check(std::abs(static_cast<long double>(f.at("maximumRelativeError").get<double>())-maximum_relative)<=precision*std::max(1.L,maximum_relative),"reported dimensionless ratio agrees with independent extended-precision oracle");
 check(f.at("erasedNonzeroSamples")==erased&&erased==0,"measured current fallback retains every nonzero sample in this actual HDR/tiny-value fixture");
}
void source_controls(){
  Scene scene;auto &source=scene.active();source.mesh_active=true;
  auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=std::make_shared<const Image>(image({255,255,255,255}));
  maps->uv1=std::make_shared<const std::vector<std::array<float,2>>>(std::vector<std::array<float,2>>{{1,0},{0,0},{0,1},{1,1}});
  source.painted=PaintedMesh::from_maps(quad(),maps,64);
  const float coverage=.25f;
  BakedTextures baked;baked.maps.color=image({24,65,117,255});baked.maps.metal_rough=image({177,42,99,255});baked.coverage.assign(4096,255);
  std::vector<BakedEmissionSample> field(4096);
  for(size_t i=0;i<field.size();++i)field[i]={{coverage*.25f,coverage*.5f,0},{coverage*(50000+32*float(i%64)),coverage*.00001f*float(i/64+1),coverage*2},coverage};
  baked.emission_field=BakedEmissionField::create(64,std::move(field));
  source.painted->install_bake(std::move(baked));
  auto texture=std::make_shared<const Image>(Image{2,2,{255,0,255,255,0,255,0,255,0,0,0,255,255,255,255,255}});
  auto binding=std::make_shared<SourceMaterialBindings>();
  binding->material.parameters.emission={.25f,.5f,.75f};
  binding->native_reference.emission={.2f,.3f,.4f};source.material.emissive={.4f,.15f,.4f};
  auto &slot=binding->material.textures[size_t(MToonSlot::Emission)];slot.index=0;slot.texcoord=1;slot.offset={-.5f,0};slot.encoding=MToonEncoding::Srgb;
  auto &role=binding->roles[size_t(MToonSlot::Emission)];role.image=texture;role.mag_filter=role.min_filter=9728;role.wrap_s=role.wrap_t=10497;
  const std::array<float,3> bary{.75f,.125f,.125f};
  auto expected=[&](float strength,std::array<double,3> background){
    std::array<double,3> result;const std::array<double,3> legacy{coverage*.25f,coverage*.5f,0},absolute{12624,coverage*.00001f*8.5,coverage*2};
    // Actual writable UV0=(.25,.125) bilinearly averages x15/16,y7/8.
    // The authored affine source field therefore gives red12624 and green
    // coverage*1e-5*8.5 independently from the alternate image's UV1.
    for(size_t k=0;k<3;++k)result[k]=background[k]*(1-coverage)+legacy[k]*strength+absolute[k];
    return result;
  };
  auto matches=[&](const auto &actual,const auto &wanted){for(size_t k=0;k<3;++k)check(std::isfinite(actual[k])&&std::abs(double(actual[k])-wanted[k])<=std::max(1e-10,std::abs(wanted[k])*2e-6),"actual immutable source sampler agrees with independent typed/absolute/coverage oracle");};
  const auto held_paint=source.painted;
  auto original_roles=std::make_shared<PaintSourceMaps>(*maps);auto &original_emission=original_roles->emission;
  original_emission.image=texture;original_emission.texcoord=1;original_emission.uv_transform={1,0,0,1,-.5f,0};original_emission.mag_filter=original_emission.min_filter=9728;original_emission.wrap_s=original_emission.wrap_t=10497;
  source.painted=PaintedMesh::from_maps(quad(),original_roles,64);source.painted->install_bake(*held_paint->baked());source.material.emissive={2,4,8};
  const auto original=detail::emission_bake_source(source);check(bool(original),"PBR source samples its actual independently owned alternate-UV texture role");
  matches(original(0,bary),expected(1,{2,0,8}));
  source.painted=held_paint;source.material.emissive={.4f,.15f,.4f};
  for(const float strength:{0.f,.5f,3.5f}){
    auto current=std::make_shared<SourceMaterialBindings>(*binding);current->material.parameters.emission_strength=strength;source.material_bindings=current;
    const auto sampler=detail::emission_bake_source(source);check(bool(sampler),"genuine FLOAT source remains sampleable at zero and nonunit typed strength");
    // Authored UV1 at this exact triangle barycentric is(.75,.125); the typed
    // offset selects image texel(0,0)=(1,0,1). UV0 would select green texel(1,0).
    // Typed/current native correction is(.5,.25,.75), before strength.
    matches(sampler(0,bary),expected(strength,{.5*strength,0,.75*strength}));
  }
  auto current=std::make_shared<SourceMaterialBindings>(*binding);current->material.parameters.emission_strength=3.5f;current->physical_emission=std::array<float,3>{3,4,5};source.material_bindings=current;
  const auto fixed=detail::emission_bake_source(source);matches(fixed(0,bary),expected(3.5f,{3,4,5}));
  auto zero=std::make_shared<SourceMaterialBindings>(*current);zero->material.parameters.emission_strength=0;zero->physical_emission=std::array<float,3>{0,0,0};source.material_bindings=zero;
  const auto zero_sample=detail::emission_bake_source(source);matches(zero_sample(0,bary),expected(0,{0,0,0}));
  const auto held=fixed(0,bary);source.material.emissive={};source.painted->clear_bake();source.material_bindings={};
  check(fixed(0,bary)==held,"captured source material/field/UV owners remain immutable after original source replacement");
  bool caught=false;try{fixed(2,bary);}catch(const std::out_of_range&){caught=true;}
  check(caught,"source sampler refuses an actual out-of-range source triangle");
  bool cancelled=false;source.material_bindings=current;
  const auto cancellable=detail::emission_bake_source(source,[&]{return cancelled;});cancelled=true;caught=false;
  try{cancellable(0,bary);}catch(const std::exception&){caught=true;}
  check(caught,"late cancellation is polled by the owning source sampler");
  auto animated=std::make_shared<SourceMaterialBindings>(*current);animated->material.parameters.scroll={.1f,0};source.material_bindings=animated;caught=false;
  try{detail::emission_bake_source(source);}catch(const std::invalid_argument&){caught=true;}
  check(caught,"animated typed source still requires explicit time freezing, not implicit time zero");
}
// New producer modes supply real native files and independent numeric oracles
// for ordinary app tests. They never observe or manipulate the application.
void public_endpoint(const Fixture &f,const ObjectBakeSettings &settings,
                     const std::filesystem::path &directory,const std::string &name){
  auto independent=f.scene;
  const auto report=bake_object(independent,f.source,settings);
  check(report.projected==4096&&report.missed==0,"public fixture has actual full source-to-target projection");
  const auto before=f.scene.encode(),expected=independent.encode();
  Document doc;doc.replace(f.scene);
  check(doc.frame().state->can_bake&&doc.frame().state->bake_sources==std::vector<uint64_t>{f.source}&&doc.frame().state->bake_source==f.source,"actual fresh document selects the sole genuine hidden source by default");
  doc.bake_from_object(f.source,settings);
  auto save=doc.save_async(directory/(name+"-accepted.dsculpt"));finish(doc);saved(std::move(save));
  const auto actual=doc.copy_scene();
  check(actual.encode()==expected,"actual public fixture worker matches complete independent synchronous native endpoint");
  check(actual.object(f.source).painted->encode()==f.scene.object(f.source).painted->encode(),"public fixture preserves full hidden locked source owner");
  const auto &paint=*actual.active().painted;
  check(paint.baked()&&paint.baked()->emission_field&&paint.baked()->maps.emission.rgba.empty(),"public fixture owns actual FLOAT emission and no RGB8 emission substitute");
  const auto values=paint.baked()->emission_field->values();
  check(values.size()==f.radiance.size(),"actual public endpoint keeps exact independent authoring grid");
  for(size_t i=0;i<values.size();++i){
    check(values[i].coverage==1&&values[i].legacy==std::array<float,3>{},"public endpoint is an absolute fully covered FLOAT seed");
    for(size_t k=0;k<3;++k)check(std::abs(double(values[i].absolute[k])-f.radiance[i].absolute[k])<=std::max(1e-10,double(f.radiance[i].absolute[k])*2e-6),"public endpoint projection matches independent authored HDR/tiny-value samples");
  }
  Document reopened;reopened.open(directory/(name+"-accepted.dsculpt"));
  check(reopened.copy_scene().encode()==expected,"public producer fresh Document reopen preserves entire exact accepted native payload");
  check(doc.undo()&&doc.copy_scene().encode()==before,"public producer has one exact complete Undo boundary");
  saved(doc.save_async(directory/(name+"-undone.dsculpt")));
  check(doc.redo()&&doc.copy_scene().encode()==expected,"public producer has one exact complete Redo boundary");
  saved(doc.save_async(directory/(name+"-redone.dsculpt")));
}
void public_fixtures(const std::filesystem::path &directory){
  std::filesystem::create_directories(directory);auto f=fixture();f.scene.save(directory/"original.dsculpt");
  ObjectBakeSettings defaults;
  // Source-derived BakeSlider physical initialization and callback arithmetic
  // from src/bake-editor.cpp. This is expected-data construction, not app state.
  auto desktop=defaults;
  auto distance=[](float value){return float(bake_distance_from_unit(bake_distance_to_unit(value)));};
  desktop.front_mm=distance(defaults.front_mm);desktop.back_mm=distance(defaults.back_mm);desktop.ao_distance_mm=distance(defaults.ao_distance_mm);
  constexpr double degrees=57.29577951308232;
  const double units=.25*degrees;
  desktop.normal_error=float(((double(defaults.normal_error)*degrees/units)*units)/degrees);
  public_endpoint(f,defaults,directory,"xr");public_endpoint(f,desktop,directory,"desktop");
  auto settings=[](const ObjectBakeSettings &v){return nlohmann::json{{"front_mm",v.front_mm},{"back_mm",v.back_mm},{"ao_distance_mm",v.ao_distance_mm},{"ao_samples",v.ao_samples},{"padding",v.padding},{"allow_partial",v.allow_partial},{"allow_backfaces",v.allow_backfaces},{"prepare_colors",v.prepare_colors},{"color_error",v.color_error},{"prepare_normals",v.prepare_normals},{"normal_error",v.normal_error},{"cage_object",v.cage_object}};};
  const nlohmann::json manifest{{"version",1},{"resolution",64},{"independent_pixel_oracle",true},{"source",f.source},{"target",f.target},{"peak",52016},{"tiny_nonzero",.00001f},{"hidden_locked_source",true},{"default_settings",settings(defaults)},{"desktop_slider_settings",settings(desktop)},{"files",{{"original","original.dsculpt"},{"xr-accepted","xr-accepted.dsculpt"},{"xr-undone","xr-undone.dsculpt"},{"xr-redone","xr-redone.dsculpt"},{"desktop-accepted","desktop-accepted.dsculpt"},{"desktop-undone","desktop-undone.dsculpt"},{"desktop-redone","desktop-redone.dsculpt"}}}};
  std::ofstream out(directory/"manifest.json");out<<manifest.dump(2)<<'\n';
  if(!out)throw std::runtime_error("Cannot publish public fixture manifest");
}
} // namespace
int main(int argc,char **argv)try{
  using namespace dome;
  if(argc==2&&std::string_view(argv[1])=="--source-controls"){
    source_controls();std::cout<<"Baked emission source "<<checks<<" checks PASS\n";return 0;
  }
  if(argc==3&&std::string_view(argv[1])=="--inspect"){
    std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;
  }
  if(argc==3&&std::string_view(argv[1])=="--public-fixtures"){
    public_fixtures(argv[2]);std::cout<<"Baked emission public producer "<<checks<<" checks PASS\n";return 0;
  }
  if(argc!=2)throw std::invalid_argument("baked-emission-workflow output-directory | --inspect FILE | --public-fixtures DIRECTORY | --source-controls");
  const auto directory=std::filesystem::path(argv[1]);std::filesystem::create_directories(directory);
  auto f=fixture();const auto before=f.scene.encode();const auto source=f.scene.object(f.source).painted;
  const auto source_value=source->encode();const auto target_layers=f.scene.active().painted->layers();
  auto sampler=detail::emission_bake_source(f.scene.object(f.source));
  check(bool(sampler),"source sampler is derived from a genuine owning native FLOAT field");
  auto sample=sampler(0,{.25f,.25f,.5f});
  check(sample[0]>50000&&sample[1]>0,"actual immutable projection-source sampler retains HDR and tiny terms");
  auto cancelled=f.scene;bool stop=false;ObjectBakeSettings settings;settings.front_mm=10;settings.back_mm=2;settings.padding=2;settings.ao_samples=0;
  bool rejected=false;try{bake_object(cancelled,f.source,settings,[&]{return stop;},[&](float progress){if(progress>=.8f)stop=true;});}catch(const std::exception &){rejected=true;}
  check(rejected&&stop&&cancelled.encode()==before,"late actual Bake cancellation retains full source/target/history owners");
  Document document;document.replace(f.scene);document.bake_from_object(f.source,settings);
  auto output=document.save_async(directory/"accepted.dsculpt");
  finish(document);saved(std::move(output));
  const auto accepted=document.copy_scene();const auto after=accepted.encode();const auto paint=accepted.active().painted;
  check(after!=before&&paint->baked()&&paint->baked()->emission_field&&paint->baked()->maps.emission.rgba.empty(),"actual worker adopts a distinct FLOAT Bake beneath editable paint");
  check(accepted.object(f.source).painted->encode()==source_value&&source->encode()==source_value,"hidden locked source and immutable held source unchanged");
  check(paint->layers().size()==target_layers.size(),"all target editable layers retained");
  for(size_t i=0;i<target_layers.size();++i){const auto &a=target_layers[i],&b=paint->layers()[i];check(a.tiles==b.tiles&&a.emission_tiles==b.emission_tiles&&a.pbr_tiles==b.pbr_tiles&&a.visible==b.visible&&a.opacity==b.opacity&&a.name==b.name,"hidden target ownership/opacity/name survive worker adoption");}
  const auto values=paint->baked()->emission_field->values();
  for(size_t i=0;i<values.size();++i){
    check(values[i].coverage==1&&values[i].legacy==std::array<float,3>{},"projected Bake is explicitly resolved absolute replacement");
    for(size_t k=0;k<3;++k)check(std::abs(double(values[i].absolute[k])-f.radiance[i].absolute[k])<=std::max(1e-10,double(f.radiance[i].absolute[k])*2e-6),"actual projection agrees with independent source field oracle");
  }
  check(document.undo()&&document.copy_scene().encode()==before,"actual global Undo restores exact native before-state");
  saved(document.save_async(directory/"undone.dsculpt"));
  check(document.redo()&&document.copy_scene().encode()==after,"actual global Redo restores exact native FLOAT Bake");
  saved(document.save_async(directory/"redone.dsculpt"));
  Document reopened;reopened.open(directory/"accepted.dsculpt");
  check(reopened.copy_scene().encode()==after,"fresh actual Document publication/reopen retains exact complete native state");
  check(Scene::load(directory/"undone.dsculpt").encode()==before&&Scene::load(directory/"redone.dsculpt").encode()==after,"ordered Save history endpoints retain exact native state");
  const auto owner=std::make_shared<const Scene>(accepted);
  const NativeEmissionExport export_owner{owner,NativeEmissionExport::Scope::WholeScene,true};
  const auto surfaces=emission_export_surfaces(*owner);
  const auto prepared=detail::prepare_emission_export(surfaces,export_owner,{});
  check(prepared.images.size()==1,"actual export evaluates only the visible target while owning the complete hidden source");
  const auto &field=prepared.images.front().image;
  check(field.version==2,"actual export field declares resolved FLOAT interpretation");
  auto exr=encode_emission_exr(field);auto restored=decode_emission_exr(exr);
  check(restored.version==2&&restored.width==field.width&&restored.height==field.height&&restored.pixels==field.pixels,"actual export FLOAT32 EXR preserves every radiance/contribution/coverage bit");
  GltfExportOptions export_options;export_options.emission=std::make_shared<const NativeEmissionExport>(export_owner);
  const auto glb=directory/"resolved-bake.glb";export_glb(surfaces,glb,export_options);
  std::ifstream stream(glb,std::ios::binary);std::vector<uint8_t> payload((std::istreambuf_iterator<char>(stream)),{});
  const auto imported=extract_native_emission(payload);
  check(imported&&imported->encode()==after,"actual GLB native/FLOAT export restores full source/target/current material/history recipe state");
  const auto document_gltf=gltf(payload);quality_oracle(document_gltf,field);
  auto bad=document_gltf;bad.json["extras"]["domeSculptEmission"]["images"][0]["fallback"]["maximumAbsoluteError"]=0;
  refuses_package(std::move(bad),directory/"forged-metric.glb","rehashed fake version2 quality metric refuses independently of document checksums");
  bad=document_gltf;bad.json["extras"]["domeSculptEmission"]["images"][0]["fallback"]["units"]="half";
  refuses_package(std::move(bad),directory/"forged-units.glb","rehashed unsupported FLOAT fallback units refuse");
  bad=document_gltf;auto &downgraded=bad.json["extras"]["domeSculptEmission"]["images"][0]["fallback"];downgraded.erase("version");downgraded.erase("units");downgraded.erase("view");downgraded["strength"]=1;downgraded["maximumAbsoluteError"]=0;downgraded["maximumRelativeError"]=0;
  refuses_package(std::move(bad),directory/"stripped-fallback2.glb","actual EXR2 cannot masquerade as legacy fallback metadata");
  bad=document_gltf;EmissionInterchangeImage legacy_image;legacy_image.width=legacy_image.height=64;legacy_image.pixels.resize(4096);const auto legacy_exr=encode_emission_exr(legacy_image);const auto legacy_view=append(bad,legacy_exr);auto &legacy_record=bad.json["extras"]["domeSculptEmission"]["images"][0];legacy_record["bufferView"]=legacy_view;legacy_record["sha256"]=creator_detail::sha256(legacy_exr);
  refuses_package(std::move(bad),directory/"fake-fallback2.glb","version2 metadata cannot raise bounds or reinterpret an actual legacy EXR1");
  bad=document_gltf;auto altered=field;altered.pixels[0][3]=.5f;const auto altered_exr=encode_emission_exr(altered);const auto altered_view=append(bad,altered_exr);auto &altered_record=bad.json["extras"]["domeSculptEmission"]["images"][0];altered_record["bufferView"]=altered_view;altered_record["sha256"]=creator_detail::sha256(altered_exr);
  refuses_package(std::move(bad),directory/"foreign-float-contribution.glb","rehashed FLOAT coefficients with unchanged RGB/quality cannot borrow actual native field authority");
  bad=document_gltf;auto missing=accepted;missing.active().painted=missing.active().painted->snapshot();missing.active().painted->clear_bake();const auto missing_value=missing.encode();check(missing_value.at("version")==41,"counterexample still owns a genuine hidden FLOAT source");const auto missing_native=nlohmann::json::to_cbor(missing_value);const auto missing_view=append(bad,missing_native);auto &package=bad.json["extras"]["domeSculptEmission"];package["version"]=1;package["encoding"]="scene-cbor+openexr-float32-zip";package["native"]={{"bufferView",missing_view},{"sha256",creator_detail::sha256(missing_native)}};
  refuses_package(std::move(bad),directory/"foreign-current-owner.glb","real unrelated Scene41 hidden source cannot authorize a missing current FLOAT field");
  std::cout<<"Baked emission worker "<<checks<<" checks PASS\n";return 0;
}catch(const std::exception &e){std::cerr<<"Baked emission worker FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
