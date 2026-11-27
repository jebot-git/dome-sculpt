// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/render.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/mtoon.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/source-maps.hpp"
#include "dome/material-visibility.hpp"
#include "dome/material-bindings.hpp"
#include "dome/native-authoring.hpp"
#include "dome/emission-native.hpp"
#include "dome/texture-sets.hpp"
#include "dome/import-image.hpp"
#include "../src/gltf-document.hpp"
#include "../src/texture-set-paint-access.hpp"
#include "mtoon-painted-set-fixture.hpp"
#include "graphics.h"
#include <fstream>
#include <iostream>
using namespace dome;
namespace {
int checks=0;
void check(bool v,const char*s){if(!v)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<'\n';}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
const Mat4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
Mesh quad(){Mesh m;m.positions={{-.75f,-.75f,0},{.75f,-.75f,0},{.75f,.75f,0},{-.75f,.75f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
std::shared_ptr<const Image> image(std::array<uint8_t,4> p){return std::make_shared<const Image>(Image{1,1,{p[0],p[1],p[2],p[3]}});}
Material white(){Material m;m.color={1,1,1,1};m.unlit=true;m.alpha=Material::AlphaMode::Opaque;return m;}
struct Fbo {
 static constexpr int size=96;GLuint f=0,c=0,d=0;
 explicit Fbo(bool srgb=false){glGenFramebuffers(1,&f);glBindFramebuffer(GL_FRAMEBUFFER,f);glGenTextures(1,&c);glBindTexture(GL_TEXTURE_2D,c);glTexImage2D(GL_TEXTURE_2D,0,srgb?GL_SRGB8_ALPHA8:GL_RGBA8,size,size,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,c,0);glGenRenderbuffers(1,&d);glBindRenderbuffer(GL_RENDERBUFFER,d);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,size,size);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,d);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Incomplete fixture FBO");}
 ~Fbo(){glDeleteRenderbuffers(1,&d);glDeleteTextures(1,&c);glDeleteFramebuffers(1,&f);}
 void bind(){glBindFramebuffer(GL_FRAMEBUFFER,f);}
};
using Pixels=std::vector<uint8_t>;
Pixels read(){Pixels p(Fbo::size*Fbo::size*4);glReadPixels(0,0,Fbo::size,Fbo::size,GL_RGBA,GL_UNSIGNED_BYTE,p.data());if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Fixture GPU read failed");return p;}
std::array<int,4> at(const Pixels&p,int x=48,int y=48){size_t i=(y*Fbo::size+x)*4;return {p[i],p[i+1],p[i+2],p[i+3]};}
int delta(const Pixels&a,const Pixels&b){int n=0;for(size_t i=0;i<a.size();++i)n=std::max(n,std::abs(int(a[i])-int(b[i])));return n;}
std::shared_ptr<MeshSnapshot> snapshot(Material m=white()){auto s=std::make_shared<MeshSnapshot>();s->mesh=quad();s->ranges.push_back({1,0,6,m,{},{},0});return s;}
Pixels draw(SurfaceRenderer&r,std::shared_ptr<const MeshSnapshot>s,bool transparent=true){r.draw(std::move(s),{},identity,{0,0,2},Fbo::size,Fbo::size,transparent);return read();}
MToonMaterial typed(bool legacy){return legacy?MToonMaterial::from_vrm0({{"shader","VRM/MToon"}}):MToonMaterial::from_vrm1({{"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"}}}}}});}
std::shared_ptr<CreatorAppearance> appearance(const MToonMaterial&m,const std::vector<std::shared_ptr<const Image>>&images={}){
 auto a=std::make_shared<CreatorAppearance>();auto asset=std::make_shared<ImportedAsset>();asset->materials.emplace_back();asset->materials[0].unlit=true;asset->images.resize(images.size());for(size_t i=0;i<images.size();++i){ImportedTexture t;t.image=int(i);t.min_filter=t.mag_filter=GL_NEAREST;asset->textures.push_back(t);}ImportedPrimitive p;p.mesh=quad();p.source_mesh=p.source_primitive=0;p.uv1={{{.75f,.5f}},{{.75f,.5f}},{{.75f,.5f}},{{.75f,.5f}}};asset->primitives.push_back(p);a->asset=asset;a->decoded_images=std::make_shared<const std::vector<std::shared_ptr<const Image>>>(images);a->material=a->source_mesh=a->source_primitive=0;a->source_vertices={0,1,2,3};if(m.version()==MToonVersion::V0)a->mtoon_json=m.source_json().dump();else a->material_json=m.source_json().dump();return a;
}
std::array<int,4> expected(const MToonMaterial&m,const MToonSamples&s,Vec3 normal={0,0,1}){
 MToonLighting q;q.normal=normal;q.view={0,0,1};q.light_direction=normalized(Vec3{-.4f,.8f,.7f});q.light_color={1,1,1};q.gi_up={.25f,.25f,.25f};q.gi_down={.1f,.1f,.1f};q.gi=q.gi_down*(.5f-.5f*normal.y)+q.gi_up*(.5f+.5f*normal.y);q.normal_oriented=true;auto c=mtoon_shade(m,s,q);std::array<int,4> p{};for(int k=0;k<3;++k)p[k]=int(std::lround(std::clamp(mtoon_linear_to_srgb(c.color[k]),0.f,1.f)*255));p[3]=int(std::lround(c.color[3]*255));return p;
}
bool close(std::array<int,4>a,std::array<int,4>b,int tolerance=2){for(int k=0;k<4;++k)if(std::abs(a[k]-b[k])>tolerance)return false;return true;}
Mesh set_triangles(){
 Mesh mesh;
 mesh.positions={{-.85f,-.65f,0},{-.10f,-.65f,0},{-.475f,.65f,0},
                 {.10f,-.65f,0},{.85f,-.65f,0},{.475f,.65f,0}};
 mesh.normals.assign(6,{0,0,1});
 mesh.uv={{{0,0}},{{1,0}},{{.5f,1}},{{0,0}},{{1,0}},{{.5f,1}}};
 mesh.indices={0,1,2,3,4,5};
 return mesh;
}
void native_texture_sets(SurfaceRenderer &renderer){
 Scene scene;
 std::array<TextureSetInitialField,2> fields;
 for(auto &field:fields){field.resolution=64;field.material=white();}
 fields[0].material.color={0,0,1,1};
 fields[1].material.color={1,0,0,1};
 const auto object=scene.active().id;
 initialize_texture_sets(scene,object,set_triangles(),fields,std::array<TextureSetId,2>{0,1});
 SourceMaterialBindings binding;
 binding.material=MToonMaterial::from_vrm1({
     {"alphaMode","MASK"},{"emissiveFactor",{0,1,0}},
     {"pbrMetallicRoughness",{{"baseColorFactor",{0,0,0,1}},{"baseColorTexture",{{"index",0}}}}},
     {"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"},{"shadeColorFactor",{0,0,0}}}}}}
 },1);
 const auto &material=fields[1].material;
 binding.native_reference={material.color,material.emissive,int(material.alpha),
                           material.alpha_cutoff,material.double_sided};
 auto &base=binding.roles[size_t(MToonSlot::Base)];
 base.image=std::make_shared<const Image>(Image{2,1,{255,255,255,0,255,255,255,255}});
 base.wrap_s=base.wrap_t=GL_CLAMP_TO_EDGE;
 base.min_filter=base.mag_filter=GL_NEAREST;
 check(bind_texture_set_mtoon(scene,object,1,binding),"Native material partition attaches its typed shader");

 Document doc;
 doc.replace(Scene::decode(scene.encode()));
 check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Cold Scene40 MToon set publishes real render ranges");
 const auto authored=draw(renderer,doc.frame().surface);
 check(close(at(authored,23,33),{0,0,255,255}),"MToon leaves the neighboring PBR range blue");
 check(at(authored,60,33)[3]==0,"MToon mask leaves its transparent atlas half empty");
 check(close(at(authored,77,33),{0,255,0,255}),"MToon draws the opaque half using its independent emission factor");
 doc.shading(false);
 check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Core PBR view republishes the same material partition");
 const auto pbr=draw(renderer,doc.frame().surface);
 check(close(at(pbr,23,33),{0,0,255,255})&&close(at(pbr,77,33),{255,0,0,255}),
       "Core PBR preview switches only the typed range's visible result");
 check(doc.copy_scene().encode()==scene.encode(),"Preview selection preserves exact authored MToon data");
}

void verify_float_filter(const EmissionInterchangeImage &exr,const Image &portable,
                         const SourceMaterialBindings &source,const MToonMaterial &exported,double bound){
 const auto &original=source.roles[size_t(MToonSlot::Emission)].image;
 const auto &before=source.material.parameters;
 const auto &after=exported.parameters;
 const std::array native_factor{before.emission.x,before.emission.y,before.emission.z};
 const std::array export_factor{after.emission.x,after.emission.y,after.emission.z};
 const auto linear=[](uint8_t q){
  const double x=q/255.;
  return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);
 };
 std::array<double,256> srgb{};
 for(size_t i=0;i<srgb.size();++i)srgb[i]=linear(uint8_t(i));
 double worst=0;
 size_t count=0,changing_coverage=0;
 for(size_t y=0;y+1<exr.height;++y)for(size_t x=0;x+1<exr.width;++x){
  const auto i=y*exr.width+x;
  const std::array taps{i,i+1,i+exr.width,i+exr.width+1};
  const bool transition=std::any_of(taps.begin()+1,taps.end(),[&](size_t at){
   return exr.pixels[at][9]!=exr.pixels[i][9];
  });
  if(!transition&&(y%7||x%7))continue;
  for(const auto &f:{std::array{.27,.72},std::array{.84,.41}}){
   const auto filter=[&](const auto &sample){
    return std::lerp(std::lerp(sample(taps[0]),sample(taps[1]),f[0]),
                     std::lerp(sample(taps[2]),sample(taps[3]),f[0]),f[1]);
   };
   const auto coefficient=[&](size_t component){
    return filter([&](size_t at){return double(exr.pixels[at][component]);});
   };
   const auto coverage=coefficient(9);
   changing_coverage+=coverage>0&&coverage<1;
   for(size_t k=0;k<3;++k){
    const auto original_texel=[&](size_t at){return srgb[original->rgba[at*4+k]];};
    const double base=original?filter(original_texel):1.;
    const double native=base*native_factor[k]*before.emission_strength*(1-coverage)+
                        coefficient(k+3)*before.emission_strength+coefficient(k+6);
    const auto encoded=[&](size_t at){return srgb[portable.rgba[at*4+k]];};
    const double gltf=filter(encoded)*export_factor[k]*after.emission_strength;
    worst=std::max(worst,std::abs(native-gltf));
    ++count;
   }
  }
 }
 std::cout<<"FLOAT_FILTER samples="<<count<<" fractional="<<changing_coverage
          <<" maximum_error="<<worst<<" bound="<<bound<<'\n';
 check(count>1000&&changing_coverage>0&&worst<=bound,
       "Independent fractional taps match the original typed emission and FLOAT coefficients within the bound");
}

void baked_texture_set(SurfaceRenderer &renderer, const std::filesystem::path &directory,
                       TextureSetId set, uint32_t resolution, float gain, bool hdr_donor,
                       bool varying_normal=false, bool full_coverage=false) {
 const auto right = set != 0;
 const auto prefix = directory / (full_coverage ? "full-ldr" : varying_normal ? "varying-ldr" :
                                   hdr_donor ? "hdr" : gain == 1 ? "ldr" : "gain");
 std::filesystem::create_directories(prefix);
 auto mesh = set_triangles();
 std::vector<TextureSetId> membership{0,1};
 if(full_coverage){
  mesh=quad();
  for(auto &p:mesh.positions)p.x=.5f*p.x-.475f;
  const auto left=mesh.positions;
  for(auto p:left){p.x+=.95f;mesh.positions.push_back(p);}
  mesh.normals.resize(8,{0,0,1});
  mesh.uv.insert(mesh.uv.end(),{{0,0},{1,0},{1,1},{0,1}});
  mesh.indices.insert(mesh.indices.end(),{4,5,6,4,6,7});
  membership={0,0,1,1};
 }
 Scene scene;
 const auto object = scene.active().id;
 std::array<TextureSetInitialField,2> fields;
 for (auto &field : fields) {
  field.resolution = 64;
  field.material = white();
  field.material.color = {0,0,1,1};
 }
 if(hdr_donor){
  auto &sibling=fields[1-set].material;
  sibling.unlit=false;
  sibling.emissive={.024f,1.15f,.12f};
 }
 auto &current = fields[set].material;
 fields[set].resolution = resolution;
 current.color = {.31f,.67f,.83f,.75f};
 current.unlit = false;
 current.alpha = Material::AlphaMode::Mask;
 current.alpha_cutoff = .4f;
 current.emissive = {.012f,.024f,.03f};
 initialize_texture_sets(scene,object,mesh,fields,membership);
 select_texture_set(scene,object,set);
 const bool physical=hdr_donor||gain!=1;
 check(edit_texture_set(scene,object,set,[right,physical,hdr_donor](PaintedMesh &paint,Material &) {
  PaintSettings ink;
  ink.front_only = false;
  ink.color = {.85f,.23f,.1f,.03f};
  Brush brush;
  brush.from = brush.to = {right?.46f:-.46f,.22f,0};
  brush.normal = {0,0,1};
  brush.radius = .13f;
  brush.strength = .81f;
  paint.add_layer("Ink over the bake");
  paint.begin_stroke();
  paint.stamp(brush,ink);
  if(physical){
   ink.channel=PaintChannel::Emission;
   ink.emission_units=hdr_donor?PaintEmissionUnits::Physical:PaintEmissionUnits::MaterialRelative;
   ink.emission=hdr_donor?std::array{.43f,.02f,.59f}:std::array{.073f,.02f,.19f};
   brush.from=brush.to={right?.5f:-.5f,-.48f,0};
   brush.radius=.075f;
   paint.stamp(brush,ink);
  }
  return paint.end_stroke();
 }),"A real editable paint layer lies above the selected set's Bake");

 SourceMaterialBindings binding;
 binding.material = MToonMaterial::from_vrm1({
     {"alphaMode","MASK"},{"alphaCutoff",current.alpha_cutoff},{"emissiveFactor",current.emissive},
     {"pbrMetallicRoughness",{{"baseColorFactor",current.color},{"baseColorTexture",{{"index",0}}}}},
     {"normalTexture",{{"index",1}}},
     {"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"},
       {"shadingShiftFactor",-.3f},{"shadeColorFactor",{.09f,.04f,.2f}}}}}}
 },2);
 if(hdr_donor){
  auto authored=binding.material.source_json();
  authored["emissiveTexture"]={{"index",2}};
  binding.material=MToonMaterial::from_vrm1(authored,3);
  auto &emission=binding.roles[size_t(MToonSlot::Emission)];
  Image gradient{resolution,resolution,std::vector<uint8_t>(size_t(resolution)*resolution*4,255)};
  for(uint32_t y=0;y<resolution;++y)for(uint32_t x=0;x<resolution;++x){
   const auto at=(size_t(y)*resolution+x)*4;
   gradient.rgba[at]=uint8_t(60+78*x/resolution);
   gradient.rgba[at+1]=uint8_t(115+100*y/resolution);
   gradient.rgba[at+2]=uint8_t(150+80*x/resolution);
  }
  emission.image=std::make_shared<const Image>(std::move(gradient));
  emission.wrap_s=emission.wrap_t=GL_CLAMP_TO_EDGE;
  emission.min_filter=emission.mag_filter=GL_LINEAR;
 }
 binding.material.parameters.emission_strength = gain;
 binding.native_reference = {current.color,current.emissive,int(current.alpha),
                             current.alpha_cutoff,current.double_sided};
 binding.roles[size_t(MToonSlot::Base)].image = image({204,173,118,255});
 auto &normal_role=binding.roles[size_t(MToonSlot::Normal)];
 normal_role.image = image({136,180,240,255});
 if(full_coverage)normal_role.image=std::make_shared<const Image>(Image{2,1,{250,135,220,255,40,185,220,255}});
 if(varying_normal){
  Image normal{resolution,resolution,std::vector<uint8_t>(size_t(resolution)*resolution*4,255)};
  for(uint32_t y=0;y<resolution;++y)for(uint32_t x=0;x<resolution;++x){
   const float u=.075f+.03f*float(x)/resolution,v=.36f+.028f*float(y)/resolution;
   const std::array direction{u,v,std::sqrt(1-u*u-v*v)};
   for(size_t k=0;k<3;++k)normal.rgba[(size_t(y)*resolution+x)*4+k]=uint8_t(std::lround((direction[k]+1)*127.5f));
  }
  normal_role.image=std::make_shared<const Image>(std::move(normal));
  normal_role.wrap_s=normal_role.wrap_t=GL_CLAMP_TO_EDGE;
  normal_role.min_filter=normal_role.mag_filter=GL_LINEAR;
 }
 check(bind_texture_set_mtoon(scene,object,set,binding),"The selected partition has a typed base and original normal role");

 const auto donor = scene.add("Hidden bake donor",Volume{});
 auto &source = scene.object(donor);
 source.visible = false;
 source.locked = true;
 source.mesh_active = true;
 source.material.color = {.57f,.19f,.39f,.91f};
 source.material.emissive = {.06f,hdr_donor?1.4f:.11f,.023f};
 auto donor_mesh = mesh;
 const auto donor_normal = normalized(Vec3{.45f,-.42f,1.f});
 const auto half=donor_mesh.positions.size()/2;
 for (size_t i=0;i<donor_mesh.positions.size();++i) {
  auto &position = donor_mesh.positions[i];
  if(!full_coverage)position.x += i < half ? -.14f : .14f;
  position.z += .01f;
  donor_mesh.uv[i][0] = donor_mesh.uv[i][0]*.44f + (i>=half?.52f:0.f);
  donor_mesh.normals[i] = donor_normal;
 }
 source.painted = std::make_shared<PaintedMesh>(std::move(donor_mesh),64);
 scene.select(object);

 Document doc;
 doc.replace(scene);
 check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Selected native MToon material renders before Bake");
 check(doc.frame().state->can_bake,"Shared Bake controls admit the selected MToon recipient");
 const auto before = doc.copy_scene();
 const auto field = texture_set_view(before.object(object),set);
 const auto sibling = texture_set_view(before.object(object),right?0:1);
 const auto authored = draw(renderer,doc.frame().surface);
 ObjectBakeSettings settings;
 settings.allow_partial = true;
 settings.ao_samples = 3;
 settings.padding = 0;
 doc.bake_from_object(donor,settings);
 const auto native_path = prefix / "baked.dsculpt";
 doc.save_async(native_path).get();
 check(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty(),
       "Bake and immediately queued Save publish the typed viewport");
 const auto after = doc.copy_scene();
 const auto selected = texture_set_view(after.object(object),set);
 const auto untouched = texture_set_view(after.object(object),right?0:1);
 const auto &bake = selected.painted->baked();
 check(selected.material_bindings==field.material_bindings&&selected.domain==field.domain&&
       selected.painted->layers().back().tiles==field.painted->layers().back().tiles,
       "Bake preserves the exact MToon binding, face domain and editable layer tiles");
 check(detail::TextureSetPaintAccess::same_current(*untouched.painted,*sibling.painted)&&
       untouched.domain==sibling.domain,
       "The neighboring PBR set keeps its exact paint and face domain");
 check(bake&&bake->normal_interpretation==BakedNormalInterpretation::CanonicalMikk&&
       !bake->maps.normal.rgba.empty()&&bool(bake->emission_field)==(hdr_donor||gain!=1),
       "Selected Bake owns canonical normals and the appropriate emission representation");
 check(selected.bake_recipe&&selected.bake_recipe->source.object==donor&&
       selected.bake_recipe->settings.ao_samples==3&&selected.bake_recipe->settings.allow_partial,
       "The selected field owns the actual donor and Bake settings");
 check(Scene::load(native_path).encode()==after.encode(),"Ordered native Save restores every baked and authored field");

 const auto pixels = draw(renderer,doc.frame().surface);
 const int baked_x = right?79:16, kept_x = right?59:36, other_x = right?25:70, y = 32;
 const auto original = at(authored,kept_x,y);
 if(full_coverage)
  check(std::all_of(bake->coverage.begin(),bake->coverage.end(),[](auto value){return value==255;}),
        "Full coverage replaces every tap of an independently sampled original normal map");
 else check(original[3]==255&&at(pixels,kept_x,y)==original,"A missed texel keeps original typed shading and normal");
 check(at(pixels,other_x,y)==at(authored,other_x,y),"The adjacent PBR field keeps its exact visible pixels");
 const auto srgb_round_trip = [](float x) {
  return mtoon_srgb_to_linear(std::round(mtoon_linear_to_srgb(x)*255.f)/255.f);
 };
 auto resolved = binding.material;
 resolved.parameters.base_color = {1,1,1,1};
 resolved.parameters.emission = {1,1,1};
 resolved.parameters.emission_strength = 1;
 MToonSamples material_samples;
 const auto emission_sample = [&](size_t component) {
  return hdr_donor||gain!=1 ? source.material.emissive[component]
                           : srgb_round_trip(source.material.emissive[component]);
 };
 for (size_t component=0;component<3;++component)
  material_samples.base[component] = srgb_round_trip(source.material.color[component]);
 material_samples.emission = {emission_sample(0),emission_sample(1),emission_sample(2)};
 material_samples.base[3] = source.material.color[3];
 const auto projected = at(pixels,baked_x,y);
 const auto want = expected(resolved,material_samples,donor_normal);
 std::cout<<"BAKED_MTOON grid="<<resolution<<" gain="<<gain<<" HDR="<<hdr_donor
          <<" actual="<<projected[0]<<','<<projected[1]<<','<<projected[2]<<','<<projected[3]
          <<" expected="<<want[0]<<','<<want[1]<<','<<want[2]<<','<<want[3]<<'\n';
 check(close(projected,want,3)&&projected!=at(authored,baked_x,y),
       "The typed shader reads donor color, canonical normal and absolute brightness at the hit");

 const auto path = prefix / "editable.glb";
 doc.export_asset(path,AssetExportFormat::EditableGlb256).get();
 std::ifstream input(path,std::ios::binary);
 const std::vector<uint8_t> bytes(std::istreambuf_iterator<char>{input},{});
 const auto editable = extract_native_authoring(bytes);
 check(editable&&editable->encode()==after.encode(),"Editable GLB restores the typed Bake and paint exactly");
 const auto json_length = uint32_t(bytes.at(12))|(uint32_t(bytes.at(13))<<8)|
     (uint32_t(bytes.at(14))<<16)|(uint32_t(bytes.at(15))<<24);
 const auto gltf = nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+json_length);
 const auto &primitive = gltf.at("meshes").at(0).at("primitives").at(set);
 const auto &material = gltf.at("materials").at(primitive.at("material").get<size_t>());
 check(material.contains("normalTexture")&&material.contains("emissiveTexture")&&
       (physical?material.at("extras").contains("domeSculptNativeMaterial"):
                 material["extensions"].contains("VRMC_materials_mtoon")),
       "Editable GLB carries the typed material or its labeled core fallback plus native state");
 {
  const auto file=prefix / "standard.glb";
  doc.export_asset(file,AssetExportFormat::Glb).get();
  std::ifstream stream(file,std::ios::binary);
  const std::vector<uint8_t> portable(std::istreambuf_iterator<char>{stream},{});
  check(!extract_native_authoring(portable),"Standard GLB carries an interoperable material independently of Editable GLB data");
  if(physical){
   const auto recovered=extract_native_emission(portable);
   check(recovered&&recovered->encode()==after.encode(),"Standard FLOAT GLB exactly restores every native paint, Bake and recipe");
  }
  const auto length=uint32_t(portable.at(12))|(uint32_t(portable.at(13))<<8)|
      (uint32_t(portable.at(14))<<16)|(uint32_t(portable.at(15))<<24);
  const auto json=nlohmann::json::parse(portable.begin()+20,portable.begin()+20+length);
  const auto imported=import_glb(portable,{},ImportMode::PreserveGltf);
  const auto primitive=std::find_if(imported.primitives.begin(),imported.primitives.end(),
      [set](const auto &p){return p.source_primitive==int(set);});
  check(primitive!=imported.primitives.end()&&primitive->tangents.size()==primitive->mesh.positions.size(),
        "Independent glTF import recovers the explicit canonical tangent frame");
  const auto &source_primitive=json.at("meshes").at(primitive->source_mesh).at("primitives").at(primitive->source_primitive);
  const auto &exported=json.at("materials").at(source_primitive.at("material").get<size_t>());
  const auto bound=exported.at("extras").at("domeSculptMToonPaint").at("maximumNormalErrorRadians").get<double>();
  check(bound>0&&bound<=(full_coverage?.00011:.02),"Standard MToon records a bounded normal filtering error");
  EmissionInterchangeImage exr;
  double between=0;
  if(physical){
   const auto &receipt=exported.at("extras").at("domeSculptPortableEmission");
   const auto &package=json.at("extras").at("domeSculptEmission");
   const auto &images=package.at("images");
   check(images.size()==(hdr_donor?2u:1u),"FLOAT export carries a separate resource for each physical member");
   const auto selected_exr=std::find_if(images.begin(),images.end(),[&](const auto &entry){
    return entry.at("textureSet").at("id")==set;
   });
   check(selected_exr!=images.end(),"The exact package identifies the selected member independently of table order");
   const auto &entry=*selected_exr;
   if(hdr_donor)
    check(std::any_of(images.begin(),images.end(),[&](const auto &other){
     return other.at("textureSet").at("id")==1-set&&other.at("view")=="pbr";
    }),"A physical PBR sibling keeps its own model and resource in the same package");
   check(entry.at("view")=="authored-mtoon"&&entry.at("textureSet").at("id")==set&&
       entry.at("fallback").at("view")=="authored-mtoon",
       "The exact FLOAT resource is explicitly bound to authored MToon and its face partition");
   const auto &buffer=json.at("bufferViews").at(entry.at("bufferView").get<size_t>());
   exr=decode_emission_exr(std::span(portable).subspan(28+length+buffer.at("byteOffset").get<size_t>(),
       buffer.at("byteLength").get<size_t>()));
   bool has_overlay=false,has_bake=false;
   const auto emission=source.material.emissive[1];
   const auto donor_green=hdr_donor?emission:srgb_round_trip(emission);
   for(const auto &q:exr.pixels){
    has_overlay|=hdr_donor?q[9]>0&&q[8]>.06f:q[5]>.01f;
    has_bake|=q[9]==1&&q[4]==0&&q[1]==q[7]&&std::abs(q[7]-donor_green)<=1e-6f;
   }
   check(exr.version==2&&exr.width==resolution&&has_overlay&&has_bake,
        "Decoded FLOAT channels preserve legacy or physical strokes and donor radiance independently");
   between=receipt.at("betweenTexelErrorBound").get<double>();
   check(between>receipt.at("maximumAbsoluteError").get<double>()&&
         between<=.005*entry.at("fallback").at("strength").get<float>(),
         "The emitted texture records an absolute bound including between-texel filtering");
  }
  SourceMaterialBindings decoded;
  decoded.material=MToonMaterial::from_vrm1(exported,imported.textures.size());
  for(size_t i=0;i<decoded.roles.size();++i){
   const auto index=decoded.material.textures[i].index;
   if(index<0)continue;
   const auto &slot=imported.textures.at(size_t(index));
   auto &view=decoded.roles[i];
   view.image=std::make_shared<const Image>(decode_import_image(imported.images.at(size_t(slot.image))));
   view.wrap_s=slot.wrap_s;view.wrap_t=slot.wrap_t;
   view.min_filter=slot.min_filter;view.mag_filter=slot.mag_filter;
  }
  if(physical){
   const auto &emission=decoded.roles[size_t(MToonSlot::Emission)].image;
   check(emission&&emission->width==exr.width&&emission->height==exr.height,
         "The typed baked emissive image matches its FLOAT grid");
   verify_float_filter(exr,*emission,binding,decoded.material,between);
  }
  const auto &normal=decoded.roles[size_t(MToonSlot::Normal)];
  check(normal.image&&normal.image->width==resolution&&normal.wrap_s==GL_CLAMP_TO_EDGE&&
        normal.min_filter==GL_LINEAR&&decoded.material.parameters.normal_scale==1,
        "The exported MToon normal slot uses its own linear clamp grid and unit scale");
  auto view=std::make_shared<MeshSnapshot>();
  view->mesh=primitive->mesh;
  view->source_normals=view->mesh.normals;
  view->source_tangents=primitive->tangents;
  view->ranges.push_back({100,0,view->mesh.indices.size(),{},{},{},0});
  view->ranges.back().source_frame=true;
  view->ranges.back().material_bindings=std::make_shared<const SourceMaterialBindings>(std::move(decoded));
  const auto actual=draw(renderer,view);
  check(at(pixels,right?70:25,58)[3]==0&&at(actual,right?70:25,58)[3]==0,
        "Baked MToon and standalone GLB both preserve the painted cutout");
  int delta=0;
  size_t visible=0,alpha_differences=0;
  for(int y=0;y<Fbo::size;++y)for(int x=right?48:0;x<(right?96:48);++x){
   const auto a=at(actual,x,y),b=at(pixels,x,y);
   alpha_differences+=a[3]!=b[3];
   visible+=a[3]!=0;
   for(size_t k=0;k<3;++k)delta=std::max(delta,std::abs(a[k]-b[k]));
  }
  std::cout<<"PORTABLE_BAKED_MTOON grid="<<resolution<<" normal_bound="<<bound
           <<" visible="<<visible<<" max_rgb="<<delta<<" alpha_differences="<<alpha_differences<<'\n';
  check(alpha_differences==0,"The standalone GLB preserves native masked visibility");
  check(visible>700&&delta<=3,"Parsed MToon with exported tangents matches native filtered and painted shading");
 }
 const auto standard = prefix / "preexisting.glb";
 std::ofstream(standard)<<"previous export";
 if(physical){
  auto unsupported=after.snapshot();
  auto adjusted=*selected.material_bindings;
  const auto role=size_t(MToonSlot::Emission);
  auto authored=adjusted.material.source_json();
  authored["emissiveTexture"]={{"index",2}};
  adjusted.material=MToonMaterial::from_vrm1(authored,3);
  adjusted.roles[role].image=std::make_shared<const Image>(Image{2,1,{0,255,15,255,255,0,45,255}});
  adjusted.roles[role].wrap_s=GL_REPEAT;
  check(bind_texture_set_mtoon(unsupported,object,set,adjusted),"A nonaligned source remains valid native authoring data");
  auto options=GltfExportOptions{};
  auto emission=std::make_shared<NativeEmissionExport>();
  emission->scene=std::make_shared<const Scene>(std::move(unsupported));
  emission->allow_measured_core_fallback=true;
  options.emission=emission;
  bool refused=false;
  try{
   export_glb(emission_export_surfaces(*emission->scene),standard,options);
  }catch(const std::invalid_argument &error){
   refused=std::string_view(error.what()).find("constant or aligned clamp-linear")!=std::string_view::npos;
  }
  check(refused,"Typed FLOAT GLB refuses a varying emission source with unrelated sampling");
 }else if(!full_coverage){
  auto sharp=after.snapshot();
  auto adjusted=*selected.material_bindings;
  auto &source=adjusted.roles[size_t(MToonSlot::Normal)];
  Image alternating{resolution,resolution,std::vector<uint8_t>(size_t(resolution)*resolution*4,255)};
  for(size_t i=0;i<alternating.rgba.size();i+=4){
   alternating.rgba[i]=(i/4)%2?255:0;
   alternating.rgba[i+1]=128;
   alternating.rgba[i+2]=145;
  }
  source.image=std::make_shared<const Image>(std::move(alternating));
  source.wrap_s=source.wrap_t=GL_CLAMP_TO_EDGE;
  source.min_filter=source.mag_filter=GL_LINEAR;
  check(bind_texture_set_mtoon(sharp,object,set,adjusted)&&
        rejects([&]{export_glb(sharp.export_surfaces(),standard);}),
        "A sharp alternating original normal field fails the portable filter bound");
 }
 std::ifstream file(standard);
 check(std::string(std::istreambuf_iterator<char>{file},{})=="previous export", "Refused Standard GLB preserves its destination");

 check(doc.undo()&&doc.copy_scene().encode()==before.encode(),"Undo restores the exact unbaked typed field");
 check(doc.redo()&&doc.copy_scene().encode()==after.encode(),"Redo restores the exact selected Bake");
 Document reopened;
 reopened.open(native_path);
 check(reopened.wait_mesh()&&draw(renderer,reopened.frame().surface)==pixels,
       "Cold Open restores identical typed viewport pixels");
 if(physical){
  const auto second=prefix/"reopened-standard.glb";
  reopened.export_asset(second,AssetExportFormat::Glb).get();
  std::ifstream original(prefix/"standard.glb",std::ios::binary),again(second,std::ios::binary);
  const std::vector<uint8_t> expected(std::istreambuf_iterator<char>{original},{});
  const std::vector<uint8_t> actual(std::istreambuf_iterator<char>{again},{});
  check(!actual.empty()&&actual==expected,"Cold Open reproduces the exact typed FLOAT GLB bytes");
 }
 check(reopened.frame().state->can_clear_bake,"Clear Bake is available on the reopened selected field");
 reopened.clear_bake();
 check(reopened.wait_asset()&&reopened.wait_mesh()&&reopened.frame().state->error.empty()&&
       draw(renderer,reopened.frame().surface)==authored,"Clear Bake recovers the original typed rendering and layers");
 const auto cleared = reopened.copy_scene();
 const auto field_cleared = texture_set_view(cleared.object(object),set);
 check(!field_cleared.painted->baked()&&field_cleared.bake_recipe&&
       field_cleared.bake_recipe->encode()==selected.bake_recipe->encode(),
       "Clear Bake retains the exact per-field recipe for Recall");
 reopened.save(prefix / "recall.dsculpt");
}

void painted_texture_sets(SurfaceRenderer &renderer,const std::filesystem::path &output,
                          mtoon_paint_fixture::Background background){
 using mtoon_paint_fixture::Background;
 const bool varying=background==Background::Varying;
 const bool shared=background==Background::SharedShade||varying;
 std::filesystem::create_directories(output);
 Scene scene;
 std::array<TextureSetInitialField,2> fields;
 for(auto &field:fields){field.resolution=64;field.material=white();}
 fields[0].material.color={0,0,1,1};
 initialize_texture_sets(scene,scene.active().id,set_triangles(),fields,
                         std::array<TextureSetId,2>{0,1});
 mtoon_paint_fixture::author(scene,1,{.44f,-.19f,0},false,background);
 Document document;
 document.replace(scene);
 check(document.wait_mesh()&&document.frame().state->error.empty(),
       "The painted material and adjacent PBR mesh render through the native document");
 const auto frame=document.frame().surface;
 const auto native=draw(renderer,frame);

 const auto path=output/(varying?"rendered-varying-mtoon.glb":shared?"rendered-shared-shade-mtoon.glb":
     background==Background::Uniform?"rendered-uniform-painted-mtoon.glb":"rendered-painted-mtoon.glb");
 export_glb(scene.export_surfaces(),path);
 std::ifstream file(path,std::ios::binary);
 const std::vector<uint8_t> bytes(std::istreambuf_iterator<char>{file},{});
 check(bytes.size()>20,"Standard GLB is available as a standalone file");
 const auto length=uint32_t(bytes[12])|(uint32_t(bytes[13])<<8)|
                   (uint32_t(bytes[14])<<16)|(uint32_t(bytes[15])<<24);
 check(length<bytes.size()-20,"The emitted GLB has a bounded material JSON chunk");
 const auto json=nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+length);
 const auto &primitive=json.at("meshes").at(0).at("primitives").at(1);
 const auto &material=json.at("materials").at(primitive.at("material").get<size_t>());
 auto decoded=import_glb(bytes,{},ImportMode::PreserveGltf);
 const auto target=std::find_if(decoded.primitives.begin(),decoded.primitives.end(),
      [](const auto &surface){return surface.source_primitive==1;});
 check(target!=decoded.primitives.end()&&target->mesh.indices.size()==3,
       "A fresh consumer recovers the actual second primitive and its UVs");
 SourceMaterialBindings binding;
 binding.material=MToonMaterial::from_vrm1(material,decoded.textures.size());
 for(size_t role=0;role<binding.roles.size();++role){
  const auto index=binding.material.textures[role].index;
  if(index<0)continue;
  const auto &slot=decoded.textures.at(size_t(index));
  auto &view=binding.roles[role];
  view.image=std::make_shared<const Image>(decode_import_image(decoded.images.at(size_t(slot.image))));
  view.wrap_s=slot.wrap_s;view.wrap_t=slot.wrap_t;
  view.min_filter=slot.min_filter;view.mag_filter=slot.mag_filter;
 }
 check(binding.roles[size_t(MToonSlot::Base)].image->width==64&&
       binding.roles[size_t(MToonSlot::Emission)].image->width==64&&
       binding.roles[size_t(MToonSlot::Shade)].image->width==(shared?64u:2u),
       "The independent file read resolves base, glow and the actual shade image");
 check(binding.material.textures[size_t(MToonSlot::Base)].offset==std::array<float,2>{},
       "A generated paint grid uses its current UVs and clamp sampler without the old transform");
 auto portable=std::make_shared<MeshSnapshot>();
 portable->mesh=target->mesh;
 Material fallback;
 fallback.color=binding.native_reference.color;
 fallback.alpha=Material::AlphaMode(binding.native_reference.alpha);
 portable->ranges.push_back({100,0,portable->mesh.indices.size(),fallback,{},{},0});
 portable->ranges.back().material_bindings=std::make_shared<const SourceMaterialBindings>(std::move(binding));
 const auto pixels=draw(renderer,portable);
 int error=0;
 size_t visible=0,masked=0,alpha_differences=0;
 for(int y=0;y<Fbo::size;++y)for(int x=Fbo::size/2;x<Fbo::size;++x){
  const auto expect=at(native,x,y),actual=at(pixels,x,y);
  alpha_differences+=expect[3]!=actual[3];
  visible+=expect[3]!=0;
  if(x>66&&x<72&&y>31&&y<40)masked+=expect[3]==0;
  for(size_t c=0;c<3;++c)error=std::max(error,std::abs(expect[c]-actual[c]));
 }
 std::cout<<"PAINTED_MTOON background="<<int(background)<<" visible="<<visible<<" hole_probes="<<masked
          <<" max_rgb="<<error<<" alpha_differences="<<alpha_differences<<'\n';
 check(visible>700&&masked>10,"The fixture visibly exercises unpainted surface and a painted cutout");
 check(!alpha_differences&&error<=3,"OpenGL output from the parsed GLB agrees at painted, filtered and masked pixels");
 if(shared){
  auto untouched=std::make_shared<MeshSnapshot>(*portable);
  auto alternate=std::make_shared<SourceMaterialBindings>(*portable->ranges.back().material_bindings);
  const auto source=texture_set_view(scene.active(),1).material_bindings;
  const auto shade=size_t(MToonSlot::Shade);
  alternate->roles[shade]=source->roles[shade];
  alternate->material.parameters.shade_color=source->material.parameters.shade_color;
  untouched->ranges.back().material_bindings=std::move(alternate);
  check(delta(draw(renderer,untouched),pixels)>8,
        "The painted shade changes visible shadow pixels compared with the original shade alone");
 }
 MaterialSurfaceQuery imported(portable->mesh,portable->ranges);
 size_t rays=0;
 for(float y:{-.5f,-.35f,-.2f,.05f,.22f})for(float x:{.26f,.34f,.42f,.5f,.58f,.66f,.74f}){
  const Vec3 origin{x,y,2};
  check(bool(imported.raycast(origin,{0,0,-1}))==bool(frame->picking->raycast(origin,{0,0,-1})),
        "Parsed Standard GLB agrees with native material visibility at the paint boundary");
  ++rays;
 }
 std::cout<<"PAINTED_MTOON rays="<<rays<<'\n';
}
}
int main(int argc,char**argv)try{
 NativeGraphics graphics;std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<'\n';check(argc<2||std::string(argv[1])!="--hardware"||(device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos),"Production renderer runs on requested hardware");
 if(argc>=2&&std::string(argv[1])=="--texture-sets"){Fbo test;SurfaceRenderer renderer;native_texture_sets(renderer);std::cout<<"PASS "<<checks<<" native MToon texture-set graphics checks\n";return 0;}
 if(argc==3&&std::string(argv[1])=="--baked-texture-sets"){
  Fbo test;
  SurfaceRenderer renderer;
  baked_texture_set(renderer,argv[2],1,64,1,false);
  baked_texture_set(renderer,argv[2],1,512,1,false,true);
  baked_texture_set(renderer,argv[2],1,64,1,false,false,true);
  baked_texture_set(renderer,argv[2],1,512,2,false);
  baked_texture_set(renderer,argv[2],0,512,1,true);
  std::cout<<"PASS "<<checks<<" baked MToon texture-set graphics checks\n";
  return 0;
 }
 if(argc==3&&std::string(argv[1])=="--painted-texture-sets"){
  Fbo test;SurfaceRenderer renderer;
  using mtoon_paint_fixture::Background;
  for(auto background:{Background::Default,Background::Uniform,Background::SharedShade,Background::Varying})
   painted_texture_sets(renderer,argv[2],background);
  std::cout<<"PASS "<<checks<<" painted MToon texture-set graphics checks\n";
  return 0;
 }
 Fbo f;SurfaceRenderer renderer;renderer.set_frame_time(0);glBindFramebuffer(GL_FRAMEBUFFER,0);renderer.draw(std::make_shared<MeshSnapshot>(),{},identity,{0,0,2},1,1,false);std::array<uint8_t,4> desktop{};glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,desktop.data());check(desktop[0]==48&&desktop[1]==48&&desktop[2]==48,"Default desktop framebuffer attachment encoding and depth queries resolve correctly");f.bind();
 auto blended=white();blended.color={.25f,.5f,1,.5f};blended.alpha=Material::AlphaMode::Blend;
 auto result=at(draw(renderer,snapshot(blended)));check(close(result,{99,137,188,128}),"Transparent source RGB blends in linear floating point before sRGB transfer; alpha stays linear");
 float copied_depth=0;auto opaque_depth=snapshot();draw(renderer,opaque_depth);glReadPixels(48,48,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&copied_depth);check(std::abs(copied_depth-.5f)<1e-6f,"Resolved native depth is preserved for subsequent desktop overlays");check(rejects([&]{renderer.draw(opaque_depth,{},identity,{0,0,2},8192,8192,true);}),"Linear scene pixel budgets reject oversized views before allocation");auto encoded=draw(renderer,snapshot(blended));{Fbo srgb(true);auto automatic=draw(renderer,snapshot(blended));check(delta(encoded,automatic)<=1,"sRGB destination hardware transfer and UNORM explicit transfer agree without double gamma");}f.bind();
 renderer.set_output_encoding(SurfaceOutputEncoding::Linear);check(close(at(draw(renderer,snapshot(blended))),{32,64,128,128}),"Linear OpenXR UNORM output contains premultiplied linear RGB and unmodified alpha");renderer.set_output_encoding(SurfaceOutputEncoding::Srgb);
 renderer.set_deferred_resolve(true);renderer.draw(snapshot(blended),{},identity,{0,0,2},96,96,true);GLint bound=0;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&bound);check(GLuint(bound)!=f.f,"Deferred stereo scene leaves its private linear target available for UI overlays");check(rejects([&]{renderer.set_output_encoding(SurfaceOutputEncoding::Linear);}),"Pending stereo view rejects transfer changes before resolve");glEnable(GL_SCISSOR_TEST);glScissor(45,45,6,6);glClearColor(.5f,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glDisable(GL_SCISSOR_TEST);renderer.resolve();check(close(at(read()),{188,0,0,255}),"Deferred UI overlay shares linear scene and gets exactly one final transfer");glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&bound);check(GLuint(bound)==f.f,"Resolve restores caller framebuffer for OpenXR release");renderer.set_deferred_resolve(false);
 auto no_mesh=std::make_shared<MeshSnapshot>();auto background=at(draw(renderer,no_mesh,false));check(close(background,{48,48,48,255}),"Desktop background survives linear decode/encode unchanged");
 for(bool legacy:{false,true}){
  auto full=snapshot();full->ranges[0].appearance=appearance(typed(legacy));
  auto original=draw(renderer,full);auto clipped=std::make_shared<MeshSnapshot>(*full);
  clipped->clipped_object=1;clipped->clip_plane={{1,0,0},0};auto pixels=draw(renderer,clipped);
  check(at(pixels,24,48)[3]==0&&at(pixels,72,48)[3]==255,"Native plane clipping applies to retained V0/V1 MToon surfaces");
  check(at(pixels,72,48)==at(original,72,48),"Retained MToon kept-side shading remains exact under clipping");
  auto other=std::make_shared<MeshSnapshot>(*clipped);other->clipped_object=2;
  check(delta(draw(renderer,other),original)==0,"Cut clipping remains isolated to the selected object identity");
 }
 for(bool legacy:{false,true}){
  auto m=typed(legacy);m.parameters.base_color={.6f,.4f,.2f,1};m.parameters.shade_color={.2f,.3f,.4f};m.parameters.cull=MToonCull::Off;auto s=snapshot();s->ranges[0].appearance=appearance(m);auto pixel=at(draw(renderer,s));check(close(pixel,expected(m,{})),"Production V0/V1 dispatch precedes retained unlit and agrees with independent CPU MToon lighting");
  auto maps=std::vector<std::shared_ptr<const Image>>{image({128,96,64,255}),image({96,160,224,255}),image({32,16,8,255}),image({48,80,112,255}),image({64,128,192,255})};
  m.textures[0].index=0;m.textures[1].index=1;m.textures[3].index=2;m.textures[4].index=3;m.textures[5].index=4;m.parameters.emission={.25f,.5f,.75f};m.parameters.rim_color={.2f,.3f,.4f};m.parameters.matcap_color=legacy?Vec3{1,1,1}:Vec3{.1f,.2f,.3f};auto a=appearance(m,maps);s->ranges[0].appearance=a;auto actual=at(draw(renderer,std::make_shared<MeshSnapshot>(*s)));MToonSamples samples;auto decoded=[](const Image&i){return Vec3{mtoon_srgb_to_linear(i.rgba[0]/255.f),mtoon_srgb_to_linear(i.rgba[1]/255.f),mtoon_srgb_to_linear(i.rgba[2]/255.f)};};for(int k=0;k<3;++k)samples.base[k]=mtoon_srgb_to_linear(maps[0]->rgba[k]/255.f);samples.shade=decoded(*maps[1]);samples.emission=decoded(*maps[2]);samples.rim=decoded(*maps[3]);samples.matcap=decoded(*maps[4]);if(!close(actual,expected(m,samples),3)){auto want=expected(m,samples);std::cerr<<"legacy="<<legacy<<" actual="<<actual[0]<<","<<actual[1]<<","<<actual[2]<<","<<actual[3]<<" expected="<<want[0]<<","<<want[1]<<","<<want[2]<<","<<want[3]<<"\n";}check(close(actual,expected(m,samples),3),"Original base/shade/emission/rim/matcap roles use independent sRGB images and authored factors once");
  for(int role:{6,9,10}){if((legacy&&role==6)||(!legacy&&role>=9))continue;auto scalar=m;scalar.parameters.shift=-.65f;scalar.parameters.toony=.25f;scalar.textures[role].index=5;auto views=maps;views.push_back(image({64,128,192,255}));auto ss=std::make_shared<MeshSnapshot>(*s);ss->ranges[0].appearance=appearance(scalar,views);auto sample=samples;float value=scalar.textures[role].encoding==MToonEncoding::Srgb?mtoon_srgb_to_linear(64/255.f):64/255.f;if(role==6)sample.shift=value;else if(role==9)sample.receive_shadow=value;else sample.shading_grade=value;check(close(at(draw(renderer,ss)),expected(scalar,sample),3),"Original shift/receive-shadow/shading-grade role uses its version-specific linear or gamma channel contract");}
  auto baseline=draw(renderer,std::make_shared<MeshSnapshot>(*s));maps.push_back(image({255,128,128,255}));m.textures[2].index=5;m.parameters.normal_enabled=false;if(legacy){s->ranges[0].appearance=appearance(m,maps);check(delta(baseline,draw(renderer,std::make_shared<MeshSnapshot>(*s)))<=1,"Authored disabled normal keyword does not accidentally apply a present normal texture");}m.parameters.normal_enabled=true;s->ranges[0].appearance=appearance(m,maps);check(delta(baseline,draw(renderer,std::make_shared<MeshSnapshot>(*s)))>8,"Enabled original normal role affects current-geometry derivative tangent basis");
 }
 for(bool legacy:{false,true}){auto m=typed(legacy);m.parameters.base_color[3]=.5f;m.parameters.alpha=MToonAlpha::Blend;m.parameters.depth_write=false;m.parameters.source_blend=5;m.parameters.destination_blend=10;m.parameters.render_queue=3000;auto two=snapshot();two->mesh.indices.insert(two->mesh.indices.end(),{0,1,2,0,2,3});two->ranges[0].appearance=appearance(m);two->ranges.push_back(two->ranges[0]);two->ranges.back().object=2;two->ranges.back().first=6;auto c=at(draw(renderer,two));check(std::abs(c[3]-(legacy?128:191))<=1,"Original V0 MAX alpha and V1 source-over alpha remain distinct across transparent surfaces");}
 auto pair=std::make_shared<const Image>(Image{2,1,{255,0,0,255,0,0,255,255}});auto uv=typed(false);uv.parameters.cull=MToonCull::Off;uv.parameters.base_color={0,0,0,1};uv.parameters.shade_color={0,0,0};uv.parameters.emission={1,1,1};uv.textures[3].index=0;uv.textures[3].texcoord=1;auto uvscene=snapshot();uvscene->ranges[0].appearance=appearance(uv,{pair});check(close(at(draw(renderer,uvscene)),{0,0,255,255}),"Original UV1 uses explicit source primitive/remap rather than packed UV0 or guessed offsets");auto invalid=std::make_shared<CreatorAppearance>(*uvscene->ranges[0].appearance);invalid->source_vertices[0]=999;uvscene->ranges[0].appearance=invalid;check(rejects([&]{draw(renderer,std::make_shared<MeshSnapshot>(*uvscene));}),"Missing or invalid authored UV1 remaps fail explicitly before drawing");
 uv.textures[3].texcoord=0;uv.parameters.scroll={.5f,0};auto animated=snapshot();animated->ranges[0].appearance=appearance(uv,{pair});renderer.set_frame_time(0);auto first=at(draw(renderer,animated));renderer.set_frame_time(1);auto next=at(draw(renderer,std::make_shared<MeshSnapshot>(*animated)));check(first!=next,"Deterministic shared stereo frame time drives authored MToon UV animation");renderer.set_frame_time(0);
 auto masked_animation=uv;masked_animation.textures[8].index=1;auto ms=snapshot();ms->ranges[0].appearance=appearance(masked_animation,{pair,image({255,255,0,255})});renderer.set_frame_time(0);auto stopped=draw(renderer,ms);renderer.set_frame_time(1);check(delta(stopped,draw(renderer,std::make_shared<MeshSnapshot>(*ms)))<=1,"MToon1 UV animation mask samples linear blue independently of red and green");renderer.set_frame_time(0);
 auto outline=typed(false);outline.parameters.cull=MToonCull::Off;outline.parameters.outline_cull=MToonCull::Front;outline.parameters.outline=MToonOutline::World;outline.parameters.outline_width=.1f;outline.parameters.outline_color={1,0,0};outline.parameters.outline_mix=0;auto outlined=snapshot();outlined->mesh.indices.insert(outlined->mesh.indices.end(),{2,1,0,3,2,0});outlined->ranges[0].count=12;for(size_t i=0;i<4;++i){auto p=outlined->mesh.positions[i];outlined->mesh.normals[i]=normalized(Vec3{p.x,p.y,0});}outlined->ranges[0].appearance=appearance(outline);auto border=draw(renderer,outlined);check(at(border,9,48)[0]>200&&at(border,9,48)[3]==255,"World-space outline extrudes already transformed native geometry on the production draw route");
 auto widthmasked=outline;widthmasked.textures[7].index=0;auto ws=std::make_shared<MeshSnapshot>(*outlined);ws->ranges[0].appearance=appearance(widthmasked,{image({255,0,255,255})});check(at(draw(renderer,ws),9,48)[3]==0,"MToon1 outline-width role samples linear green and suppresses extrusion at zero");
 for(auto [w,color]:std::vector<std::pair<float,std::array<int,4>>>{{0,{0,0,255,255}},{.25f,{0,255,255,255}},{.5f,{0,255,0,255}},{.75f,{255,255,0,255}},{1,{255,0,0,255}}}){auto weights=std::make_shared<MeshSnapshot>(*outlined);weights->selected_weights.assign(4,w);check(close(at(draw(renderer,weights)),color),"Selected joint weights bypass authored MToon and draw the unlit blue-to-red ramp");check(at(read(),9,48)[3]==0,"Selected weight surfaces suppress their material outlines");}
 auto ordinary=std::make_shared<MeshSnapshot>(*outlined);ordinary->selected_weights.assign(4,-1);check(delta(border,draw(renderer,ordinary))<=1,"Negative weight sentinel preserves ordinary material and outline rendering");ordinary->selected_weights[0]=1.1f;check(rejects([&]{draw(renderer,std::make_shared<MeshSnapshot>(*ordinary));}),"Weight visualization rejects out-of-range values");
 for(bool legacy:{false,true}){
  auto alpha=legacy?MToonMaterial::from_vrm0({{"shader","VRM/MToon"},{"floatProperties",{{"_BlendMode",1},{"_CullMode",0}}}}):MToonMaterial::from_vrm1({{"alphaMode","MASK"},{"doubleSided",true},{"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"}}}}}});
  alpha.textures[0].index=0;
  auto transparent=image({255,255,255,0});auto weighted=snapshot();weighted->ranges[0].appearance=appearance(alpha,{transparent});weighted->selected_weights.assign(4,.5f);
  check(at(draw(renderer,weighted))[3]==0,"V0/V1 selected-joint heatmaps preserve authored alpha-mask holes");
  MaterialSurfaceQuery picking(weighted->mesh,weighted->ranges);
  check(!picking.raycast({0,0,2},{0,0,-1},4),"CPU material picking and the weight viewport agree on transparent MToon coverage");
  auto animated=alpha;animated.parameters.scroll={.5f,0};
  auto split=std::make_shared<const Image>(Image{2,1,{255,255,255,0,255,255,255,255}});
  auto animation=snapshot();animation->ranges[0].appearance=appearance(animated,{split});animation->selected_weights.assign(4,.5f);
  MaterialSurfaceQuery moving(animation->mesh,animation->ranges);
  renderer.set_frame_time(0);auto shown=at(draw(renderer,animation));
  check(shown[3]>0&&moving.raycast({.01f,.01f,2},{0,0,-1},4,0).has_value(),"The displayed material frame time selects visible animated heatmap coverage");
  renderer.set_frame_time(1);shown=at(draw(renderer,animation));
  check(shown[3]==0&&!moving.raycast({.01f,.01f,2},{0,0,-1},4,1),"GPU and picking share the same animated alpha holes when the frame time advances");
  renderer.set_frame_time(0);
  alpha.parameters.alpha=MToonAlpha::Blend;alpha.parameters.alpha_to_coverage=false;alpha.parameters.render_queue=3000;alpha.parameters.source_blend=5;alpha.parameters.destination_blend=10;alpha.parameters.depth_write=false;
  weighted=snapshot();weighted->ranges[0].appearance=appearance(alpha,{image({255,255,255,128})});
  auto original_alpha=at(draw(renderer,weighted))[3];weighted=std::make_shared<MeshSnapshot>(*weighted);weighted->selected_weights.assign(4,.5f);
  check(at(draw(renderer,weighted))[3]==original_alpha&&original_alpha>0&&original_alpha<255,"V0/V1 weight heatmaps retain the material's actual blend-alpha state");
 }
 {
  auto surface=snapshot();auto maps=std::make_shared<TextureMaps>();maps->source_only=true;auto source=std::make_shared<PaintSourceMaps>();source->color.image=image({255,255,255,0});maps->source_maps=source;
  surface->ranges[0].material.alpha=Material::AlphaMode::Mask;surface->ranges[0].textures=maps;surface->selected_weights.assign(4,1);
  auto back=quad();for(auto p:back.positions){p.z=.2f;surface->mesh.positions.push_back(p);}surface->mesh.normals.insert(surface->mesh.normals.end(),back.normals.begin(),back.normals.end());surface->mesh.uv.insert(surface->mesh.uv.end(),back.uv.begin(),back.uv.end());for(auto id:back.indices)surface->mesh.indices.push_back(id+4);surface->selected_weights.insert(surface->selected_weights.end(),4,.5f);surface->ranges.push_back({2,6,6,white()});
  check(close(at(draw(renderer,surface)),{0,255,0,255}),"Separate weighted objects bind their own alpha textures and retain visible geometry behind cutouts");
 }
 auto source=std::make_shared<PaintSourceMaps>();source->color.image=image({128,192,64,255});auto paint=PaintedMesh::from_maps(quad(),source,64);auto original=paint->composite(white());check(original->color_coverage.width==1&&original->color_coverage.rgba==std::vector<uint8_t>({0,0,0,255}),"Untouched source coverage uses an exact bounded zero sentinel");Brush b;b.from=b.to={0,0,0};b.normal={0,0,1};b.radius=.6f;PaintSettings p;p.color={0,0,0,1};paint->begin_stroke();paint->stamp(b,p);paint->end_stroke();auto full=paint->composite(white());auto center=[](const Image&i){return i.rgba[(i.height/2*i.width+i.width/2)*4];};check(full->color_coverage.width==full->color.width&&center(full->color_coverage)==255,"True black paint retains full independent linear coverage");paint->layer_properties("Half",true,.5f);auto half=paint->composite(white());check(center(half->color_coverage)==128,"Layer opacity scales exact color coverage independently of black RGB");paint->layer_properties("Hidden",false,1);check(paint->composite(white())->color_coverage.width==1,"Hidden paint returns zero-coverage sentinel without discarding retained tiles");paint->layer_properties("Mask",true,1);p.channel=PaintChannel::Mask;p.mask=.5f;paint->begin_stroke();paint->stamp(b,p);paint->end_stroke();auto masked=paint->composite(white());check(center(masked->color_coverage)==128,"Linear painted masks contribute once to combined color coverage");auto held=paint->snapshot();p.channel=PaintChannel::Color;p.erase=true;paint->begin_stroke();paint->stamp(b,p);paint->cancel_stroke();check(paint->composite(white())==masked&&held->composite(white())==masked,"Cancelled erase preserves immutable source coverage snapshots and cache identity");check(PaintedMesh::decode(paint->encode())->composite(white())->color_coverage.rgba==masked->color_coverage.rgba,"Native source/layer restart reconstructs identical coverage without serializing derived caches");

 auto zero=white();zero.color={0,0,0,1};auto shade=typed(false);shade.parameters.base_color={0,0,0,1};shade.parameters.shade_color={1,1,1};shade.parameters.cull=MToonCull::Off;shade.textures[0].index=shade.textures[1].index=0;auto za=appearance(shade,{source->color.image});auto zasset=std::make_shared<ImportedAsset>(*za->asset);zasset->materials[0].color=zero.color;za->asset=zasset;auto zp=PaintedMesh::from_maps(quad(),source,64);auto zs=snapshot(zero);zs->mesh.normals.assign(4,{0,0,-1});zs->ranges[0].appearance=za;zs->ranges[0].textures=zp->preview(zero);auto untouched=draw(renderer,zs);zs->ranges[0].textures=zp->composite(zero);check(delta(untouched,draw(renderer,std::make_shared<MeshSnapshot>(*zs)))<=3,"Derived zero-base-factor MToon shade restores untouched original shade RGB using zero coverage");p.channel=PaintChannel::Color;p.erase=false;p.color={0,0,0,1};zp->begin_stroke();zp->stamp(b,p);zp->end_stroke();zs->ranges[0].textures=zp->composite(zero);check(close(at(draw(renderer,std::make_shared<MeshSnapshot>(*zs))),{0,0,0,255}),"Fully covered true black paint removes aliased shade even when original base factor is zero");zp->layer_properties("Half black",true,.5f);zs->ranges[0].textures=zp->composite(zero);auto halfshade=at(draw(renderer,std::make_shared<MeshSnapshot>(*zs)));check(halfshade[0]>10&&halfshade[1]>halfshade[0]&&halfshade[2]>0,"Partially covered black paint preserves remaining original zero-factor shade channels");
 auto low=zero;low.color={.0001f,.001f,.01f,1};auto lowtoon=shade;lowtoon.parameters.base_color=low.color;auto la=appearance(lowtoon,{source->color.image});auto las=std::make_shared<ImportedAsset>(*la->asset);las->materials[0].color=low.color;la->asset=las;auto lp=PaintedMesh::from_maps(quad(),source,64);auto ls=snapshot(low);ls->mesh.normals.assign(4,{0,0,-1});ls->ranges[0].appearance=la;ls->ranges[0].textures=lp->preview(low);auto lr=draw(renderer,ls);lp->begin_stroke();lp->stamp(b,p);lp->end_stroke();ls->ranges[0].textures=lp->composite(low);check(close(at(lr,20,20),at(draw(renderer,std::make_shared<MeshSnapshot>(*ls)),20,20)),"Near-zero nonzero base factors retain original shade outside painted coverage without quantization amplification");
 paint->add_layer("Second half");p.channel=PaintChannel::Color;p.color={1,0,0,1};paint->begin_stroke();paint->stamp(b,p);paint->end_stroke();paint->layer_properties("Second half",true,.5f);check(std::abs(int(center(paint->composite(white())->color_coverage))-192)<=1,"Multiple visible color layers union coverage in order through independent masks and opacity");

 for(bool legacy:{false,true}){
 auto raw=std::make_shared<PaintSourceMaps>();raw->color.image=image({255,255,255,255});raw->normal.image=image({255,128,128,255});raw->normal_scale=.75f;
 auto painter=PaintedMesh::from_maps(quad(),raw,64);auto mt=typed(legacy);mt.parameters.shade_color={.1f,.2f,.3f};mt.parameters.toony=0;mt.textures[2].index=0;mt.parameters.normal_enabled=!legacy;if(!legacy)mt.textures[2].texcoord=1;auto app=appearance(mt,{raw->normal.image});
 BakedTextures base;base.maps.color={64,64,std::vector<uint8_t>(64*64*4)};base.maps.metal_rough=base.maps.emission=base.maps.normal=base.maps.color;base.coverage.resize(64*64);
 for(int y=0;y<64;++y)for(int x=0;x<64;++x){size_t at=size_t(y)*64+x;base.coverage[at]=x<22?0:x<43?128:255;for(int k=0;k<4;++k){base.maps.color.rgba[at*4+k]=255;base.maps.metal_rough.rgba[at*4+k]=std::array<uint8_t,4>{255,255,0,255}[k];base.maps.emission.rgba[at*4+k]=k==3?255:0;base.maps.normal.rgba[at*4+k]=std::array<uint8_t,4>{128,204,230,255}[k];}}
 painter->install_bake(base);auto scene=snapshot();scene->ranges[0].textures=painter->composite(white());scene->ranges[0].appearance=app;scene->tangents.assign(4,{1,0,0,1});auto pixels=draw(renderer,scene);Vec3 canonical=normalized(Vec3{128/255.f*2-1,204/255.f*2-1,230/255.f*2-1});
 for(auto [x,cov]:std::array<std::pair<int,float>,3>{{{24,0},{48,128/255.f},{72,1}}}){auto N=normalized(Vec3{0,0,1}*(1-cov)+canonical*cov);check(close(at(pixels,x,48),expected(mt,{},N),3),"Merged MToon protects original keyword/UV1 normal and blends partial/full canonical bake in world space");}
 for(int y=0;y<64;++y)for(int x=0;x<64;++x){size_t pos=size_t(y)*64+x;base.coverage[pos]=x<28?0:255;if(x<28)for(int k=0;k<4;++k)base.maps.normal.rgba[pos*4+k]=std::array<uint8_t,4>{255,128,128,255}[k];}
 auto boundary_paint=PaintedMesh::from_maps(quad(),raw,64);boundary_paint->install_bake(base);scene->ranges[0].textures=boundary_paint->composite(white());auto boundary=draw(renderer,std::make_shared<MeshSnapshot>(*scene));auto half=normalized(Vec3{0,0,1}*.5f+canonical*.5f);check(close(at(boundary,43,48),expected(mt,{},half),3),"Canonical normal filtering premultiplies coverage so arbitrary protected texels cannot bleed across bake boundaries");
 check(painter->composite(white())->baked_base==painter->baked(),"Merged composite retains exact immutable raw baked payload for typed normal reconstruction");
 }
 std::cout<<"Production MToon/linear target tests complete: "<<checks<<" checks\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
