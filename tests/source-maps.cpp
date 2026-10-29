// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/paint.hpp"
#include "dome/source-maps.hpp"
#include "dome/scene.hpp"
#include "dome/projection.hpp"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);std::cout<<"PASS "<<m<<'\n';}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
bool near(float a,float b,float e=.006f){return std::abs(a-b)<e;}
std::shared_ptr<const Image> flat(std::array<uint8_t,4> rgba,uint32_t w=1,uint32_t h=1){auto i=std::make_shared<Image>();i->width=w;i->height=h;for(size_t n=0;n<size_t(w)*h;++n)i->rgba.insert(i->rgba.end(),rgba.begin(),rgba.end());return i;}
Mesh quad(){Mesh m;m.positions={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
uint8_t pixel(const Image&i,uint32_t x,uint32_t y,int c){return i.rgba[(size_t(y)*i.width+x)*4+c];}
}
int main(int argc,char **argv)try{
  auto image=std::make_shared<Image>();image->width=128;image->height=64;
  for(uint32_t y=0;y<64;++y)for(uint32_t x=0;x<128;++x){image->rgba.push_back(uint8_t(x*2));image->rgba.push_back(uint8_t(y*4));image->rgba.push_back(195);image->rgba.push_back(x%3?128:0);}
  auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image;maps->color.wrap_s=maps->color.wrap_t=33071;
  maps->normal.image=flat({128,128,255,255},5,3);maps->occlusion.image=flat({128,0,0,255},3,2);
  maps->metal_rough.image=flat({17,128,64,255});maps->emission.image=flat({128,64,0,255});
  maps->normal_scale=.6f;maps->occlusion_strength=.25f;
  auto paint=PaintedMesh::from_maps(quad(),maps,64);Material material;material.color={1,1,1,1};material.metallic=material.roughness=1;material.emissive={1,1,1};
  check(paint->has_paint_data()&&paint->layers()[0].tiles[0]==nullptr,"Original source maps protect replacement without allocating paint tiles");
  check(paint->source_maps().get()!=maps.get()&&paint->source_maps()->color.image.get()==image.get(),"Source bundle metadata freezes while original decoded image storage is shared");
  auto composite=paint->composite(material);
  check(composite->color.width==128&&composite->color.height==64&&composite->color.rgba==image->rgba,"Rectangular high-resolution original RGBA survives unpainted compositing exactly including transparent RGB");
  check(composite->transparent&&composite->source_maps->normal.image->width==5&&composite->source_maps->normal.image->height==3,"Normal and AO source views retain independent original dimensions and alpha metadata");
  check(pixel(composite->metal_rough,32,32,0)==223&&pixel(composite->metal_rough,32,32,1)==128&&pixel(composite->metal_rough,32,32,2)==64,"Original linear ORM channels and independent occlusion strength compose in the documented channels");
  check(pixel(composite->emission,32,32,0)==128&&pixel(composite->emission,32,32,1)==64,"Emission textures decode and encode sRGB without double material factors");
  material.color={.5f,.25f,1,.5f};material.metallic=.5f;material.roughness=.25f;material.emissive={.5f,1,1};
  auto factored=paint->composite(material);
  check(near(srgb_to_linear(pixel(factored->color,64,32,0)/255.f),srgb_to_linear(image->rgba[(32*128+64)*4]/255.f)*.5f)&&pixel(factored->color,64,32,3)==64,"Material linear color and alpha factors apply exactly once above the authored texture");
  check(pixel(factored->metal_rough,32,32,1)==32&&pixel(factored->metal_rough,32,32,2)==32,"Metallic and roughness factors modulate the original linear texture once");
  maps->color.uv_transform[4]=.5f;maps->normal_scale=9;
  check(paint->source_maps()->normal_scale==.6f&&paint->source_maps()->color.uv_transform[4]==0,"Mutating caller bundle metadata cannot change the published source");
  auto held=paint->snapshot();check(held->source_maps()==paint->source_maps(),"Paint snapshots share immutable source ownership without copying original pixels");
  PaintSourceTexture sampler;sampler.image=std::make_shared<Image>(Image{2,1,{255,0,0,255,0,0,255,0}});sampler.mag_filter=9728;
  check(sample_source_texture(sampler,1.1f,.5f,false)[0]==1&&sample_source_texture(sampler,-.1f,.5f,false)[2]==1,"Original repeat addressing handles positive and negative UVs");
  sampler.wrap_s=33648;check(sample_source_texture(sampler,1.1f,.5f,false)[2]==1&&sample_source_texture(sampler,-.1f,.5f,false)[0]==1,"Mirrored repeat reflects both sides at authored texture boundaries");
  sampler.wrap_s=33071;check(sample_source_texture(sampler,4,.5f,false)[2]==1&&sample_source_texture(sampler,-4,.5f,false)[0]==1,"Clamp addressing retains edge texels outside the image");
  sampler.mag_filter=9729;auto middle=sample_source_texture(sampler,.5f,.5f,true);
  check(near(middle[0],.5f)&&near(middle[2],.5f)&&near(middle[3],.5f),"Bilinear original texture filtering uses linear RGB and independent linear alpha");
  sampler.uv_transform={-1,0,0,1,1,0};sampler.mag_filter=9728;
  check(sample_source_texture(sampler,.1f,.5f,false)[2]==1,"Affine source UV mirroring is applied before original sampler addressing");
  sampler.uv_transform={0,0,0,0,.1f,.5f};
  check(sample_source_texture(sampler,.9f,.8f,false)[0]==1,"Valid constant source UV transforms preserve authored constant texture lookups");
  sampler.texcoord=1;check(rejects([&]{sampler.validate();}),"Unmapped secondary source UV sets fail explicitly rather than changing appearance");sampler.texcoord=0;
  sampler.wrap_s=0;check(rejects([&]{sampler.validate();}),"Unsupported source samplers cannot silently enter native paint storage");
  PaintImageArchive archive;auto id=archive.add(image);check(archive.add(image)==id&&archive.size()==1&&archive.pixels()==8192,"Shared source images count once per document archive identity");
  auto encoded=paint->encode(&archive);auto second=held->encode(&archive);
  check(archive.size()==5&&!encoded.contains("source_images")&&!second.contains("source_images"),"Multiple material objects refer to one shared source image archive without inline duplication");
  auto restored_archive=PaintImageArchive::decode(archive.encode());auto restored=PaintedMesh::decode(encoded,&restored_archive),other=PaintedMesh::decode(second,&restored_archive);
  check(restored->source_maps()->color.image==other->source_maps()->color.image&&restored->source_maps()->color.image->rgba==image->rgba,"Document restart reuses shared exact source image storage across objects");
  check(restored->source_maps()->normal_scale==.6f&&restored->source_maps()->normal.image->width==5&&restored->source_maps()->occlusion_strength==.25f,"Source channel interpretation and normal/AO parameters roundtrip without flattening");
  auto standalone=PaintedMesh::decode(paint->encode());check(standalone->composite(material)->color.rgba==factored->color.rgba,"Standalone native painted assets keep self-contained exact source maps");
  check(rejects([&]{PaintedMesh::decode(paint->encode(),&archive);}),"Ambiguous embedded and document-wide image registries are rejected");
  auto missing=encoded;missing["source_maps"]["color"]["image"]=999;check(rejects([&]{PaintedMesh::decode(missing,&archive);}),"Missing original image references fail instead of losing authored texture data");
  auto huge_metadata=encoded;huge_metadata["source_maps"]["color"]["texcoord"]=uint64_t(1)<<32;
  check(rejects([&]{PaintedMesh::decode(huge_metadata,&archive);}),"Large source sampler integers cannot truncate into accepted metadata");
  auto broad_normal=*paint->source_maps();broad_normal.normal_scale=1000;broad_normal.validate();
  check(decode_source_maps(encode_source_maps(broad_normal,archive),archive)->normal_scale==1000,"Finite authored normal scales retain their value without an arbitrary presentation clamp");
  auto malformed=archive.encode();malformed[0]["rgba"]=nlohmann::json::binary({1,2,3});check(rejects([&]{PaintImageArchive::decode(malformed);}),"Restart validates exact RGBA dimensions before copying image bytes");
  PaintImageLimits limit;limit.max_image_pixels=4;PaintImageArchive small(limit);
  check(rejects([&]{small.add(image);})&&small.size()==0,"Original per-image quota failures preserve an unchanged archive");
  limit={};limit.max_unique_pixels=2;PaintImageArchive bounded(limit);auto one=flat({1,2,3,4});bounded.add(one);bounded.add(one);bounded.add(flat({5,6,7,8}));
  check(bounded.pixels()==2&&rejects([&]{bounded.add(flat({9,10,11,12}));})&&bounded.size()==2,"Shared source pixel quota counts unique images and rejects excess atomically");
  limit={};limit.max_images=1;check(rejects([&]{PaintImageArchive::decode(archive.encode(),limit);}),"Original image-count resource limits apply on restart before pixel allocation");
  check(rejects([&]{PaintedMesh::from_maps(quad(),std::make_shared<PaintSourceMaps>(),64);}),"An empty source bundle cannot create misleading protected data");
  auto baseline=paint->composite(material)->color.rgba;Brush b;b.from=b.to={0,0,0};b.normal={0,0,1};b.radius=.6f;PaintSettings settings;settings.color={0,1,0,1};
  paint->begin_stroke();paint->stamp(b,settings);check(paint->end_stroke(),"Source-backed meshes use the existing sparse editable layer transaction");auto after=paint->composite(material)->color.rgba;
  check(after!=baseline&&held->composite(material)->color.rgba==baseline,"Painting an overlay preserves the immutable baseline and prior snapshots");
  check(paint->undo()&&paint->composite(material)->color.rgba==baseline&&paint->has_paint_data(),"Undo restores exact original pixels while original appearance stays protected");
  check(paint->redo()&&paint->composite(material)->color.rgba==after,"Redo restores original-map overlay without resampling source pixels");
  paint->begin_stroke();settings.color={0,0,1,1};paint->stamp(b,settings);paint->cancel_stroke();check(paint->composite(material)->color.rgba==after,"Cancelling source-map painting restores the complete previous overlay exactly");
  paint->layer_properties("Hidden",false,0);check(paint->composite(material)->color.rgba==baseline&&paint->has_paint_data(),"Hidden zero-opacity paint leaves original appearance and protection intact");
  auto same=paint->reparameterize(quad());check(same->source_maps()==paint->source_maps(),"Unchanged UV topology preserves shared original source ownership");
  auto changed=quad();for(auto &uv:changed.uv)uv[0]=1-uv[0];
  check(rejects([&]{paint->reparameterize(changed);})&&paint->source_maps()->color.image->rgba==image->rgba,"UV changes requiring a source bake fail safely while keeping exact original textures");
  auto empty=PaintedMesh(quad(),64);auto old=empty.encode();check(!old.contains("source_maps")&&!PaintedMesh::decode(old)->has_paint_data(),"Legacy native painted assets remain readable without a source archive");
  Scene scene;scene.active().painted=held;scene.active().mesh_active=true;
  auto duplicate=scene.duplicate(scene.active().id);scene.validate();auto document=scene.encode();
  check(document.at("source_images").size()==5&&!document.at("objects")[0].at("painted").contains("source_images"),"Native whole-Scene persistence stores each original image once across duplicated objects");
  auto document_copy=Scene::decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(document)));
  auto &first=document_copy.objects()[0];auto &other_object=document_copy.object(duplicate);
  check(first.painted->source_maps()->color.image==other_object.painted->source_maps()->color.image&&first.painted->source_maps()->color.image->rgba==image->rgba,
    "Scene CBOR restart restores exact original source images with shared ownership");
  for(int arg=1;arg<argc;++arg){
    // Bounded fixture extraction tests the original embedded source images.
    // It deliberately does not relax product importer's material checks.
    std::ifstream file(argv[arg],std::ios::binary|std::ios::ate);auto length=file.tellg();
    if(!file||length<28||length>128*1024*1024)throw std::runtime_error("Invalid VRoid GLB fixture size");
    std::vector<uint8_t> bytes(static_cast<size_t>(length));file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),length);
    auto word=[&](size_t at){if(at+4>bytes.size())throw std::runtime_error("Truncated GLB fixture");uint32_t v=0;for(int k=0;k<4;++k)v|=uint32_t(bytes[at+k])<<(k*8);return v;};
    if(!file||word(0)!=0x46546c67||word(4)!=2||word(8)!=bytes.size()||word(16)!=0x4e4f534a)throw std::runtime_error("Invalid GLB fixture header");
    size_t json_size=word(12),bin_header=20+json_size;
    if(bin_header+8>bytes.size()||word(bin_header+4)!=0x004e4942||word(bin_header)>bytes.size()-bin_header-8)throw std::runtime_error("Invalid GLB fixture chunks");
    auto asset=nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+json_size);size_t bin_start=bin_header+8,bin_size=word(bin_header);
    PaintImageArchive original;std::vector<std::shared_ptr<const Image>> images;
    for(auto &encoded_image:asset.at("images")){
      if(encoded_image.at("mimeType")!="image/png")throw std::runtime_error("Baseline fixture needs explicit supported PNG images");
      auto &view=asset.at("bufferViews").at(encoded_image.at("bufferView").get<size_t>());
      size_t offset=view.value("byteOffset",size_t(0)),count=view.at("byteLength").get<size_t>();
      if(view.value("buffer",0)!=0||offset>bin_size||count>bin_size-offset)throw std::runtime_error("Invalid fixture image buffer view");
      auto decoded=std::make_shared<const Image>(decode_projection_png(std::span<const uint8_t>(bytes.data()+bin_start+offset,count)));
      images.push_back(decoded);original.add(decoded);
    }
    auto saved=original.encode();auto restored=PaintImageArchive::decode(saved);
    bool exact=original.size()==restored.size();for(size_t i=0;i<original.size();++i)
      exact&=original.at(i)->width==restored.at(i)->width&&original.at(i)->height==restored.at(i)->height&&original.at(i)->rgba==restored.at(i)->rgba;
    check(exact&&original.size()==25,"Actual CC0 VRoid baseline preserves all25 decoded original PNGs exactly across restart");
    for(auto &material:asset.at("materials")){
      PaintSourceMaps views;
      auto resolve=[&](PaintSourceTexture &view,const nlohmann::json &info){if(!info.contains("index"))return;
        auto &texture=asset.at("textures").at(info.at("index").get<size_t>());view.image=images.at(texture.at("source").get<size_t>());
        if(texture.contains("sampler")){auto &sampler=asset.at("samplers").at(texture.at("sampler").get<size_t>());
          view.wrap_s=sampler.value("wrapS",10497);view.wrap_t=sampler.value("wrapT",10497);view.min_filter=sampler.value("minFilter",0);view.mag_filter=sampler.value("magFilter",0);}
        view.texcoord=info.value("texCoord",0);
      };
      auto pbr=material.value("pbrMetallicRoughness",nlohmann::json::object());
      resolve(views.color,pbr.value("baseColorTexture",nlohmann::json::object()));resolve(views.metal_rough,pbr.value("metallicRoughnessTexture",nlohmann::json::object()));
      resolve(views.emission,material.value("emissiveTexture",nlohmann::json::object()));resolve(views.normal,material.value("normalTexture",nlohmann::json::object()));
      resolve(views.occlusion,material.value("occlusionTexture",nlohmann::json::object()));
      views.normal_scale=material.value("normalTexture",nlohmann::json::object()).value("scale",1.f);
      views.occlusion_strength=material.value("occlusionTexture",nlohmann::json::object()).value("strength",1.f);
      views.validate();encode_source_maps(views,original);
    }
    check(original.size()==25,"All actual baseline materials reuse the25-image source cache without duplicate storage");
    std::cout<<"BASELINE "<<argv[arg]<<" images="<<original.size()<<" unique_pixels="<<original.pixels()<<" materials="<<asset.at("materials").size()<<'\n';
  }
  std::cout<<"Native original source-map tests complete\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
