// SPDX-License-Identifier: GPL-3.0-or-later
#include "normal-bake-encoding.hpp"
#include "portable-normal-raster.hpp"
#include "polygon-surface-transfer.hpp"
#include "dome/brush.hpp"
#include "dome/paint.hpp"
#include "dome/source-maps.hpp"
#include <iostream>
#include <numbers>
using namespace dome;
using namespace dome::detail;
namespace {
unsigned checks=0;
void check(bool v,const char *message){if(!v)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
template<class F> bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
// Independent Gauss-Jordan oracle; never calls the production inverse/decoder.
Vec3 solve(NormalBakeBasis f,Vec3 d) {
  double a[3][4]={{f.t.x,f.b.x,f.n.x,d.x},{f.t.y,f.b.y,f.n.y,d.y},{f.t.z,f.b.z,f.n.z,d.z}};
  for(int c=0;c<3;++c){int p=c;for(int r=c+1;r<3;++r)if(std::abs(a[r][c])>std::abs(a[p][c]))p=r;for(int k=0;k<4;++k)std::swap(a[p][k],a[c][k]);auto div=a[c][c];if(std::abs(div)<1e-12)throw std::runtime_error("Oracle singular basis");for(int k=0;k<4;++k)a[c][k]/=div;for(int r=0;r<3;++r)if(r!=c){auto v=a[r][c];for(int k=0;k<4;++k)a[r][k]-=a[c][k]*v;}}
  return normalized({float(a[0][3]),float(a[1][3]),float(a[2][3])});
}
Vec3 forward(NormalBakeBasis b,Vec3 q){return normalized(b.t*q.x+b.b*q.y+b.n*q.z);}
double angle(Vec3 a,Vec3 b){a=normalized(a);b=normalized(b);return std::atan2(length(cross(a,b)),std::clamp(double(dot(a,b)),-1.,1.));}

void device_normals(){
 using namespace portable_normal;
 auto image=std::make_shared<Image>(Image{73,37,std::vector<uint8_t>(73*37*4)});
 for(size_t i=0;i<image->rgba.size();i+=4){
  image->rgba[i]=uint8_t(92+(i*23)%77);
  image->rgba[i+1]=uint8_t(87+(i*53)%83);
  image->rgba[i+2]=uint8_t(224+i%23);
  image->rgba[i+3]=(i/4)%11?255:0;
 }
 PaintSourceTexture source;
 source.image=image;
 source.wrap_s=source.wrap_t=33071;
 source.min_filter=source.mag_filter=9729;
 auto device=upload(*image,{});
 std::vector<portable_normal::Sample> samples(batch_pixels);
 for(size_t i=0;i<samples.size();++i){
  const float a=float(i%691)/690,b=float(i%353)/352*(1-a);
  samples[i]={{1-a-b,a,b},{float(i%293)/267-.049f,float(i%593)/557-.031f}};
 }
 std::vector<uint32_t> actual(samples.size());
 std::array<Chart,2> charts{};
 std::array<std::vector<uint32_t>,2> individual;
 for(bool native:{true,false}){
  const float hand=native?1.f:-1.f;
  const NormalBakeBasis before{{1,0,0},{0,hand,0},{0,0,1}};
  const NormalBakeBasis after{{1,0,0},{0,.8f*hand,.6f*hand},{0,-.6f,.8f}};
  auto &chart=charts[native?0:1];
  chart.source.before=before;
  chart.source.current=after;
  chart.source.native=native;
  chart.source.handedness=hand;
  const std::array<Vec3,3> tips{Vec3{.27f,.05f,1},Vec3{-.19f,-.18f,1},Vec3{.01f,.21f,1}};
  std::array<NormalBakeVertex,3> placed,emitted;
  for(size_t k=0;k<3;++k){
   placed[k]=normal_bake_vertex(tips[k],{1,.08f*float(k),0},hand,NormalVertexBitangent::UnitCross);
   emitted[k]=normal_bake_vertex(forward(after,tips[k]),{1,0,0},hand,NormalVertexBitangent::UnitCross);
   chart.source.vertices[k]=placed[k];
   chart.destination[k]=emitted[k];
  }
  device->encode(std::span(&chart,1),samples,actual,{});
  individual[native?0:1]=actual;
  size_t device_texels=0;
  for(size_t i=0;i<samples.size();++i)if(actual[i]!=needs_reference){
   const auto &p=samples[i];
   const std::array<double,3> b{p.barycentric[0],p.barycentric[1],p.barycentric[2]};
   auto basis=normal_bake_basis(placed,b,NormalFragmentBasis::UnitColumns);
   if(native){
    const auto n=normal_bake_math::unit(placed[0].n*float(b[0])+placed[1].n*float(b[1])+placed[2].n*float(b[2]));
    const auto t=normal_bake_math::unit(before.t-n*dot(before.t,n));
    basis={t,cross(n,t)*hand,n};
   }
   const auto e=sample_source_texture(source,p.uv[0],p.uv[1],false);
   const Vec3 detail{2*e[0]-1,2*e[1]-1,2*e[2]-1};
   const auto input=!native||e[3]>0?basis.t*detail.x+basis.b*detail.y+basis.n*detail.z:basis.n;
   const auto desired=transport_polygon_direction(input,{before,hand},{after,hand});
   const auto reference=encode_normal_bake(normal_bake_basis(emitted,b,NormalFragmentBasis::UnitColumns),desired);
   if(!reference||reference->quantized_angle_radians>.02||reference->rgba[2]<128)
    throw std::runtime_error("Device admitted an unqualified portable normal");
   const auto word=actual[i];
   if(reference->rgba!=std::array<uint8_t,4>{uint8_t(word),uint8_t(word>>8),uint8_t(word>>16),255})
    throw std::runtime_error("Device changed the rounded normal byte outside its precision guard");
   ++device_texels;
  }
  check(device_texels>samples.size()*9/10,"Device reproduces the exact native or reflected prepared encoding outside its guard");
  std::cout<<"DEVICE_TEXELS native="<<native<<" encoded="<<device_texels<<" total="<<samples.size()<<'\n';
  unsigned polls=0;
  check(refuses([&]{device->encode(std::span(&chart,1),std::span(samples).first(32),std::span(actual).first(32),
       [&]{return ++polls==2;});}),"Cancellation after device dispatch refuses the detached batch");
 }
 for(size_t i=0;i<samples.size();++i)samples[i].chart=uint32_t(i%charts.size());
 device->encode(charts,samples,actual,{});
 for(size_t i=0;i<samples.size();++i)if(actual[i]!=individual[samples[i].chart][i])
  throw std::runtime_error("A batched chart produced another chart's normal result");
 check(true,"Combined native and reflected prepared charts match their separate dispatches");
 samples[0].chart=uint32_t(charts.size());
 check(refuses([&]{device->encode(charts,samples,actual,{});}),"A normal sample cannot address a foreign chart");
}

void device_certificate(){
 using namespace portable_normal;
 const auto original=std::make_shared<const Image>(Image{1,1,{128,128,255,255}});
 const auto emitted=std::make_shared<const Image>(Image{2,1,{128,128,255,255,160,128,255,255}});
 auto device=upload(*original,{});
 std::array<Chart,2> charts{};
 for(size_t i=0;i<charts.size();++i){
  const float hand=i?-1.f:1.f;
  auto &chart=charts[i];
  chart.source.native=i==0;
  chart.source.handedness=hand;
  chart.source.before=chart.source.current={{1,0,0},{0,hand,0},{0,0,1}};
  for(size_t k=0;k<3;++k)
   chart.source.vertices[k]=chart.destination[k]=normal_bake_vertex({0,0,1},{1,0,0},hand,NormalVertexBitangent::UnitCross);
 }
 PaintSourceTexture lookup;
 lookup.image=emitted;
 lookup.wrap_s=lookup.wrap_t=33071;
 const auto exact=[&](float u){
  const auto e=sample_source_texture(lookup,u,.5f,false);
  const auto z=normal_bake_math::unit(Vec3{2*e[0]-1,2*e[1]-1,2*e[2]-1});
  const auto n=normal_bake_math::unit(Vec3{2.f*(128.f/255.f)-1,2.f*(128.f/255.f)-1,1});
  return std::atan2(double(length(cross(n,z))),std::clamp(double(dot(n,z)),-1.,1.))<=.02;
 };
 float below=.25f,above=.75f;
 while(std::nextafter(below,above)!=above){
  const auto middle=(below+above)*.5f;
  if(exact(middle))below=middle;
  else above=middle;
 }
 std::vector<Probe> probes;
 for(uint32_t i=0;i<charts.size();++i){
  for(size_t j=0;j<513;++j)probes.push_back({{{.25f,.25f,.5f},{.5f,.5f},i},{float(j)/512.f,.5f}});
  for(float u:{below,above})probes.push_back({{{.25f,.25f,.5f},{.5f,.5f},i},{u,.5f}});
 }
 std::vector<Verdict> output(probes.size());
 check(refuses([&]{device->certify(charts,probes,output,{});}),"Certification needs its separately uploaded emitted atlas");
 device->set_emitted_image(*emitted,{});
 device->certify(charts,probes,output,{});
 size_t passed=0,failed=0,rechecked=0;
 for(size_t i=0;i<probes.size();++i){
  if(i%515>=513&&output[i]!=Verdict::Reference)
   throw std::runtime_error("A probe adjacent to the angular boundary did not retain the precision check");
  if(output[i]==Verdict::Reference){++rechecked;continue;}
  const bool accepted=output[i]==Verdict::Pass;
  if(accepted!=exact(probes[i].emitted_uv[0]))throw std::runtime_error("A definite device verdict differs from the angular reference");
  if(accepted)++passed;
  else ++failed;
 }
 check(passed&&failed&&(passed+failed)>9*probes.size()/10,"Native and reflected atlas verdicts agree on definite passes and failures");
 std::cout<<"CERTIFIED_PROBES accepted="<<passed<<" rejected="<<failed<<" precision="<<rechecked<<'\n';
 unsigned polls=0;
 check(refuses([&]{device->certify(charts,probes,output,[&]{return ++polls==2;});}),
       "Cancellation after the certificate dispatch refuses the detached decision batch");
 const Image replacement{1,1,{255,128,128,255}};
 device->set_emitted_image(replacement,{});
 device->certify(charts,probes,output,{});
 check(std::all_of(output.begin(),output.end(),[](auto verdict){return verdict==Verdict::Fail;}),
       "A replacement atlas invalidates all verdicts for its preceding image");
}
}
int main(int argc,char **argv)try {
  if(argc>1&&std::string_view(argv[1])=="--missing"){
   try{(void)portable_normal::upload(Image{1,1,{128,128,255,255}},{});}
   catch(const std::exception &e){std::cout<<"REFUSED "<<e.what()<<'\n';return 0;}
   throw std::runtime_error("Missing device returned a portable normal raster");
  }
  NormalBakeBasis identity{{1,0,0},{0,1,0},{0,0,1}};
  for(auto desired:{Vec3{0,0,1},Vec3{.3f,-.2f,1},Vec3{-.7f,.5f,-.4f}}){auto encoded=encode_normal_bake(identity,desired);check(encoded&&angle(encoded->tangent,solve(identity,desired))<1e-6&&encoded->quantized_angle_radians<.01,"Independent inverse and bounded RGB8 reconstruction agree for a full-rank frame");}
  std::array<NormalBakeVertex,3> raw,unit;
  // Vertex1 is a .45 glTF morph between two valid orthogonal endpoint
  // frames: base N=Z/T=X, target N=X/T=Z. Morph addition precedes vertex
  // normalization; the intermediate N/T angle need not stay orthogonal.
  const Vec3 normals[]={{0,0,1},{.45f,0,.55f},{0,0,1}};
  const Vec3 tangents[]={{1,0,0},{.55f,0,.45f},{0,1,0}};
  for(int i=0;i<3;++i){raw[i]=normal_bake_vertex(normals[i],tangents[i],1,NormalVertexBitangent::RawCross);unit[i]=normal_bake_vertex(normals[i],tangents[i],1,NormalVertexBitangent::UnitCross);}
  const std::array<double,3> bary{.2,.35,.45};
  auto native=normal_bake_basis(raw,bary,NormalFragmentBasis::UnitColumns);
  auto reference=normal_bake_basis(unit,bary,NormalFragmentBasis::UnitColumns);
  auto legacy=normal_bake_basis(raw,bary,NormalFragmentBasis::RawColumns);
  check(angle(native.b,reference.b)>.01,"Varying N/T angles expose vertex-bitangent normalization before versus after interpolation");
  auto desired=normalized(Vec3{.35f,.2f,1});
  auto pbr=encode_normal_bake(reference,desired),toon=encode_normal_bake(legacy,desired);
  check(pbr&&toon&&angle(pbr->tangent,solve(reference,desired))<1e-6&&angle(toon->tangent,solve(legacy,desired))<1e-6,"Declared PBR and legacy MToon basis inverses match the independent oracle");
  check(length(pbr->tangent-toon->tangent)>.02f&&pbr->rgba!=toon->rgba,"One physical baked direction can require different PBR and MToon normal planes");
  check(angle(forward(reference,pbr->tangent),desired)<1e-6&&angle(forward(legacy,toon->tangent),desired)<1e-6,"Separate planes recover the same desired normal in their declared rest evaluations");
  const auto old=normalized(Vec3{-.6f,.2f,1}),detail=normalized(Vec3{.4f,-.5f,1});const float coverage=.37f;
  auto world_mix=normalized(forward(legacy,old)*(1-coverage)+forward(legacy,detail)*coverage);
  auto tangent_mix=forward(legacy,normalized(old*(1-coverage)+detail*coverage));
  check(angle(world_mix,tangent_mix)>.001,"Normalized world-space coverage does not commute with tangent-space mixing in a non-orthonormal frame");
  auto resolved=encode_normal_bake(legacy,world_mix);
  check(resolved&&angle(forward(legacy,resolved->tangent),world_mix)<1e-6,"A resolved normal can encode the chosen current-pose coverage result without division by coverage");
  auto reflected=reference;reflected.b=reflected.b*-1;
  auto mirrored=encode_normal_bake(reflected,desired);
  check(mirrored&&angle(mirrored->tangent,solve(reflected,desired))<1e-6,"A reflected full-rank basis retains explicit handedness in its inverse");
  check(!encode_normal_bake({{1,0,0},{2,0,0},{0,0,1}},desired),"Singular frame encoding refuses instead of inventing a rest basis");
  check(!encode_normal_bake({{1,0,0},{1,1e-8f,0},{0,0,1}},desired),"Relative conditioning detects a near-singular basis independently of absolute scale");
  check(!encode_normal_bake(identity,{1,0,0},0)&&encode_normal_bake(identity,{0,0,1},0),"Zero normal strength admits only directions its declared decoder can represent");
  check(refuses([&]{normal_bake_basis(raw,{.2,.2,.2},NormalFragmentBasis::RawColumns);}),"Malformed barycentric authority is rejected");
  auto mixed=raw;mixed[1].w=-1;check(refuses([&]{normal_bake_basis(mixed,bary,NormalFragmentBasis::RawColumns);}),"A triangle cannot silently mix tangent handedness");
  check(refuses([&]{encode_normal_bake(identity,{NAN,0,1});}),"Nonfinite input does not publish encoded normals");
  if(compute_backend()!="CPU"){device_normals();device_certificate();}
  std::cout<<"PASS "<<checks<<" normal-bake encoding foundation checks\n";return 0;
} catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
