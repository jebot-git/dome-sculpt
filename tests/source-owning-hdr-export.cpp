// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/gltf-export.hpp"
#include "dome/emission-native.hpp"
#include "baked-emission-domain.hpp"
#include "paint-overlay-sample.hpp"
#include "gltf-document.hpp"
#include "humanoid-sha256.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace {
using namespace dome;
using Json=nlohmann::json;
size_t checks=0;
void check(bool value,const char *why){if(!value)throw std::runtime_error(why);++checks;}
std::vector<uint8_t> read(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
uint32_t word(std::span<const uint8_t>b,size_t p){return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;}
detail::GltfDocument document(const std::vector<uint8_t>&b){detail::GltfDocument d;const auto n=word(b,12);d.json=Json::parse(b.begin()+20,b.begin()+20+n);d.binary.assign(b.begin()+28+n,b.end());return d;}
std::span<const uint8_t> view(const detail::GltfDocument&d,const Json&r){const auto &v=d.json.at("bufferViews").at(r.at("bufferView").get<size_t>());return std::span<const uint8_t>(d.binary).subspan(v.at("byteOffset").get<size_t>(),v.at("byteLength").get<size_t>());}
void resign(detail::GltfDocument&d){auto &p=d.json["extras"]["domeSculptEmission"];p["binarySha256"]=creator_detail::sha256(d.binary);auto copy=d.json;copy["extras"]["domeSculptEmission"].erase("documentSha256");p["documentSha256"]=creator_detail::sha256(Json::to_cbor(copy));}
void replace(detail::GltfDocument &d,Json&record,const EmissionInterchangeImage&image){const auto bytes=encode_emission_exr(image);d.binary.insert(d.binary.end(),(4-d.binary.size()%4)%4,0);const auto begin=d.binary.size();d.binary.insert(d.binary.end(),bytes.begin(),bytes.end());d.binary.insert(d.binary.end(),(4-d.binary.size()%4)%4,0);const auto index=d.json["bufferViews"].size();d.json["bufferViews"].push_back({{"buffer",0},{"byteOffset",begin},{"byteLength",bytes.size()}});d.json["buffers"][0]["byteLength"]=d.binary.size();record["bufferView"]=index;record["sha256"]=creator_detail::sha256(bytes);}
void refuses(detail::GltfDocument d,const std::filesystem::path&p,const char*why){resign(d);detail::write_glb(std::move(d),p);bool refused=false;try{(void)extract_native_emission(read(p));}catch(const std::exception&e){std::cout<<"EXPECTED_REFUSAL "<<e.what()<<'\n';refused=true;}check(refused,why);}
struct Coeff {std::array<double,3> legacy{},physical{};double coverage=0;};
Coeff texel(const PaintedMesh&p,int x,int y){Coeff value;const int r=p.resolution();x=std::clamp(x,0,r-1);y=std::clamp(y,0,r-1);for(const auto &layer:p.layers())if(layer.visible&&layer.opacity){const auto common=p.pixel(layer.id,x,y);const auto e=p.emission_pixel(layer.id,x,y);const double a=double(e.coverage)*common.data[13]/255.*layer.opacity;for(size_t k=0;k<3;++k){value.legacy[k]=value.legacy[k]*(1-a)+e.legacy[k]*a;value.physical[k]=value.physical[k]*(1-a)+e.physical[k]*a;}value.coverage=value.coverage*(1-a)+a;}return value;}
Coeff sample(const PaintedMesh&p,double u,double v){const double x=u*p.resolution()-.5,y=v*p.resolution()-.5;const int ix=std::floor(x),iy=std::floor(y);Coeff out;for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const auto q=texel(p,ix+dx,iy+dy);const double w=(dx?x-ix:1-x+ix)*(dy?y-iy:1-y+iy);out.coverage+=q.coverage*w;for(size_t k=0;k<3;++k){out.legacy[k]+=q.legacy[k]*w;out.physical[k]+=q.physical[k]*w;}}return out;}
void stroke(Scene&s,PaintEmissionUnits units,std::array<float,3>color,const char*name){auto p=s.active().painted->snapshot();p->add_layer(name);PaintSettings ink;ink.front_only=false;ink.channel=PaintChannel::Emission;ink.emission_units=units;ink.emission=color;Brush b;b.from=b.to={0,0,.2f};b.normal={0,0,1};b.radius=.13f;b.strength=.65f;p->begin_stroke();p->stamp(b,ink);check(p->end_stroke(),"Actual current512 editable emission stroke");s.active().painted=p;s.validate();}
}
int main(int argc,char**argv)try{
 if(argc!=3)return 2;const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);auto scene=Scene::load(argv[1]);const auto target=scene.active().id;const auto domain=scene.active().painted->baked()->emission_domain;check(domain&&domain->field()->resolution()==64&&scene.active().painted->resolution()==512,"Actual native source64/current512 owning seed");
 stroke(scene,PaintEmissionUnits::Physical,{4,.2f,1},"New current512 physical contribution");stroke(scene,PaintEmissionUnits::MaterialRelative,{.2f,.45f,.6f},"New current512 relative contribution");const auto exact=scene.encode();
 auto native=std::make_shared<NativeEmissionExport>();native->scene=std::make_shared<const Scene>(scene.snapshot());native->scope=NativeEmissionExport::Scope::WholeScene;native->allow_measured_core_fallback=true;GltfExportOptions options;options.emission=native;
 const auto file=out/"retained-source-current.glb";export_glb(emission_export_surfaces(*native->scene),file,options);const auto bytes=read(file);auto recovered=extract_native_emission(bytes);check(recovered&&recovered->encode()==exact,"Package5 restores complete native Scene43/hidden owners/editable layers exactly");auto d=document(bytes);const auto &package=d.json.at("extras").at("domeSculptEmission");check(package.at("version")==5,"Independent source grid uses explicit package5");size_t subject=package.at("images").size();for(size_t i=0;i<package.at("images").size();++i)if(package.at("images")[i].at("object")==target)subject=i;check(subject<package.at("images").size(),"Actual emitted target owns a unique emission entry");const auto &entry=package.at("images")[subject];const auto source_image=decode_emission_exr(view(d,entry.at("retainedEmission"))),current_image=decode_emission_exr(view(d,entry));check(source_image.width==64&&source_image.height==64&&current_image.width==512&&current_image.height==512&&source_image.version==2&&current_image.version==2,"Two independent FLOAT coefficient witnesses retain their actual source/current dimensions");
 for(size_t i=0;i<source_image.pixels.size();++i){const auto &p=source_image.pixels[i];const auto &wanted=domain->field()->values()[i];for(size_t k=0;k<3;++k)check(p[3+k]==wanted.legacy[k]&&p[6+k]==wanted.absolute[k],"Source EXR retains exact original FLOAT coefficients");check(p[9]==wanted.coverage,"Source EXR retains exact source coverage");}
 const auto &original_source=*scene.object(domain->source_id()).painted;const auto &current=*scene.active().painted;const auto packet=detail::PaintOverlayFactory::capture(*recovered->active().painted,{});double maximum=0;size_t probes=0;for(size_t y=0;y<151;++y)for(size_t x=0;x<157;++x){const double u=(x+.271)/157.,v=(y+.619)/151.;const auto before=sample(original_source,u,v),above=sample(current,u,v);
  // Compare the native continuous function without sampling a flattened512 image.
  const auto shown=detail::sample_overlay_values(*packet,float(u),float(v),false);
  for(size_t k=0;k<3;++k){const double expected=before.physical[k]*(1-above.coverage)+above.legacy[k]+above.physical[k];const double actual=double(shown[2][k])+shown[4][k];maximum=std::max(maximum,std::abs(expected-actual));}++probes;}
 check(probes==23707&&maximum<2e-4,"Dense independently reconstructed source/current functions retain original2e-4 fidelity bound after export/reopen");std::cout<<"INDEPENDENT_MULTI_GRID probes="<<probes<<" maximum="<<maximum<<'\n';
 auto bad=d;bad.json["extras"]["domeSculptEmission"]["images"][subject].erase("retainedEmission");refuses(std::move(bad),out/"missing-source.glb","Rehashed omitted source witness refuses");
 bad=d;bad.json["extras"]["domeSculptEmission"]["images"][subject]["retainedEmission"]["sourceResolution"]=128;refuses(std::move(bad),out/"wrong-source-grid.glb","Rehashed source-grid substitution refuses");
 bad=d;auto forged=source_image;forged.pixels[0][0]+=1;forged.pixels[0][6]+=1;replace(bad,bad.json["extras"]["domeSculptEmission"]["images"][subject]["retainedEmission"],forged);refuses(std::move(bad),out/"wrong-source-float.glb","Reencoded/resigned source coefficients must match exact native provenance replay");
 bad=d;forged=current_image;forged.pixels[0][9]=.5f;forged.pixels[0][6]=.25f;replace(bad,bad.json["extras"]["domeSculptEmission"]["images"][subject],forged);refuses(std::move(bad),out/"wrong-current-float.glb","Reencoded/resigned current coefficients must match exact native replay independently of resolved fallback metrics");
 auto again=std::make_shared<NativeEmissionExport>(*native);again->scene=std::make_shared<const Scene>(recovered->snapshot());options.emission=again;export_glb(emission_export_surfaces(*again->scene),out/"second.glb",options);check(read(out/"second.glb")==bytes,"Second native package5 cycle is byte exact");
 Document doc;doc.replace(scene.snapshot());check(doc.wait_mesh(),"Current native document worker accepts exact retained owner");doc.export_asset(out/"worker.glb",AssetExportFormat::Glb).get();auto worker=extract_native_emission(read(out/"worker.glb"));check(worker&&worker->encode()==exact,"Actual file worker preserves native multi-grid authoring state");check(scene.encode()==exact,"All export/refusal/consumer operations preserve original source");std::cout<<"PASS "<<checks<<" retained HDR export controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
