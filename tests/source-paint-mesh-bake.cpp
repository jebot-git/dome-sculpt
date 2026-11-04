// SPDX-License-Identifier: GPL-3.0-or-later
#define main established_mtoon_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/source-paint.hpp"
#include "dome/bake.hpp"
#include "dome/tangents.hpp"
#include "dome/bake-authoring.hpp"
#include "dome/document.hpp"
#include "paint-overlay-packet.hpp"
#include "source-color-consumer.hpp"
#include <numeric>
namespace {
std::shared_ptr<MeshSnapshot> view(const Mesh &mesh,Material material,
    std::shared_ptr<const TextureMaps> maps,std::vector<std::array<float,4>> colors={}){
 auto result=std::make_shared<MeshSnapshot>();result->mesh=mesh;result->source_colors=std::move(colors);
 const auto corners=tangent_corners(mesh);result->tangents.resize(mesh.positions.size());
 for(size_t c=0;c<mesh.indices.size();++c)result->tangents[mesh.indices[c]]=corners[c];
 result->ranges.push_back({1,0,mesh.indices.size(),material,std::move(maps),{},0});return result;
}
void stamp(PaintedMesh &paint,PaintSettings settings){
 Brush brush;brush.from=brush.to={0,0,0};brush.normal={0,0,1};brush.radius=2;brush.strength=.8f;settings.front_only=false;
 paint.begin_stroke();paint.stamp(brush,settings);check(paint.end_stroke(),"Real source atlas stroke publishes a sparse contribution");
}
}
int main(int argc,char **argv)try{
 if(argc<2||argc>3)return 2;const auto out=std::filesystem::path(argv[1]);std::filesystem::create_directories(out);
 NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
 std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<std::endl;
 check(argc<3||std::string_view(argv[2])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Requested hardware requires actual Intel Arc A770");
 Fbo fbo;fbo.bind();SurfaceRenderer renderer;color_consumer::Renderer independent;
 size_t number=0;
 for(bool textured:{false,true})for(bool painted:{false,true}){
  auto raw=quad();raw.uv={{{-1,-1}},{{2,-1}},{{2,2}},{{-1,2}}};auto roles=std::make_shared<PaintSourceMaps>();
  if(textured){
   roles->color.image=std::make_shared<const Image>(Image{2,1,{255,12,4,255,30,180,90,170}});
   roles->color.texcoord=1;roles->color.min_filter=roles->color.mag_filter=9728;
   roles->uv1=std::make_shared<const std::vector<std::array<float,2>>>(4,std::array<float,2>{.75f,.5f});
   roles->metal_rough.image=image({150,190,120,255});roles->emission.image=image({90,70,50,255});roles->occlusion.image=image({160,160,160,255});roles->occlusion_strength=.4f;
  }
  PaintSurfaceOptions policy;policy.policy=PaintUvPolicy::SourceAuthored;
  const auto original=capture_source_sampling(raw,roles,64,policy);const auto source_value=original->encode();
  const std::array<uint32_t,4> vertices{0,1,2,3};auto paint=prepare_source_paint(original,quad(),vertices);
  if(painted){
   PaintSettings ink;ink.color={.18f,.58f,.9f,.32f};stamp(*paint,ink);
   ink.channel=PaintChannel::Metallic;ink.metallic=.75f;stamp(*paint,ink);
   ink.channel=PaintChannel::Roughness;ink.roughness=.3f;stamp(*paint,ink);
   ink.channel=PaintChannel::Emission;ink.emission={.1f,.035f,.08f};stamp(*paint,ink);
   paint->layer_properties("Fractional original paint",true,.63f);
   paint->add_layer("Hidden preserved paint");ink.channel=PaintChannel::Color;ink.color={1,0,1,1};stamp(*paint,ink);paint->layer_properties("Hidden preserved paint",false,.4f);
  }
  const auto before=paint->encode();auto material=white();material.color={.71f,.83f,.62f,.81f};material.metallic=.25f;material.roughness=.8f;material.emissive={.12f,.2f,.09f};
  auto marker=paint->preview(material);check(marker->source_only&&marker->overlay&&marker->source_maps==paint->source_maps(),"Genuine factory provides the independent source and paint owner");
  BakeSource high;high.mesh=paint->mesh();high.colors.assign(4,{.8f,.7f,.9f,.75f});high.materials[0].material=material;high.materials[0].maps=marker;
  BakeOptions settings;settings.resolution=256;settings.ao_samples=0;settings.padding=4;
  auto result=bake_mesh(quad(),high,settings);check(result.projected==256*256&&!result.missed,"Every target texel projects onto the actual source");
  auto maps=std::make_shared<TextureMaps>(result.maps);maps->normal=result.normal;
  for(bool unlit:{true,false})for(auto alpha:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend}){
   material.unlit=unlit;material.alpha=alpha;material.alpha_cutoff=.3f;auto reference=view(high.mesh,material,marker,high.colors);
   auto target=white();target.unlit=unlit;target.alpha=alpha;target.alpha_cutoff=.3f;
   const auto wanted=draw(renderer,reference),actual=draw(renderer,view(quad(),target,maps));const auto error=delta(wanted,actual);
   std::cout<<"SOURCE4_MESH_BAKE case="<<number<<" texture="<<textured<<" painted="<<painted<<" unlit="<<unlit<<" alpha="<<int(alpha)<<" max="<<error<<std::endl;
   check(error<=2,"Source-aware mesh baking matches actual native source shading within two codes");
   const auto file=out/("case"+std::to_string(number++)+".glb");ObjectSurface surface;surface.id=1;surface.mesh=quad();surface.material=target;surface.textures=maps;export_glb({surface},file);
   color_consumer::Model model(file);fbo.bind();independent.render(model.data,nullptr,{},true);check(delta(actual,read())<=2,"Actual baked GLB matches an independent parsed PBR consumer");
  }
  check(paint->encode()==before&&original->encode()==source_value,"Baking retains exact source, editable layers and original coordinate owner");
  auto foreign=std::make_shared<TextureMaps>(*marker);foreign->source_maps=std::make_shared<const PaintSourceMaps>(*marker->source_maps);auto wrong=high;wrong.materials[0].maps=foreign;
  check(rejects([&]{bake_mesh(quad(),wrong,settings);})&&paint->encode()==before,"Equal-valued foreign source owner refuses before any output publication");
  bool stop=false;auto cancelled=settings;cancelled.cancelled=[&]{return stop;};cancelled.progress=[&](float p){if(p>.05f)stop=true;};
  check(rejects([&]{bake_mesh(quad(),high,cancelled);})&&stop&&paint->encode()==before,"Actual source-aware projection cancellation preserves all original paint");
  if(painted){
   auto hidden=paint->snapshot();hidden->layer_properties("Hidden preserved paint",false,.4f);hidden->select_layer(hidden->layers().front().id);hidden->layer_properties("Fractional original paint",false,.63f);
   auto control=high;control.materials[0].maps=hidden->preview(material);auto changed=bake_mesh(quad(),control,settings);
   check(changed.maps.color.rgba!=result.maps.color.rgba&&changed.maps.emission.rgba!=result.maps.emission.rgba,"Independent no-paint control detects removal of color and emission contributions");
   auto physical=paint->snapshot();physical->add_layer("Physical radiance");PaintSettings ink;ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={10,.005f,0};stamp(*physical,ink);auto hdr=high;hdr.materials[0].maps=physical->preview(material);
   check(rejects([&]{bake_mesh(quad(),hdr,settings);}),"Unsupported physical radiance refuses before byte output can clip it");
  }
 }
 std::cout<<"PASS "<<checks<<" source-aware mesh bake checks in "<<number<<" graphical exports\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
