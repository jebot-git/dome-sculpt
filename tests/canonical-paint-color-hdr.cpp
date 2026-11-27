// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/emission-native.hpp"
#include "dome/import.hpp"
#include "dome/native-authoring.hpp"
#include "dome/projection.hpp"
#include "baked-normal-field.hpp"
#include "canonical-paint-sampling.hpp"
#include "emission-paint-access.hpp"
#include "humanoid-sha256.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>

using namespace dome;
namespace {
void check(bool condition, const char *description) {
  if (!condition) throw std::runtime_error(description);
  std::cout << "PASS " << description << std::endl;
}

void settle(Document &doc) {
  if (!doc.wait_asset() || !doc.wait_mesh() || !doc.frame().state->error.empty())
    throw std::runtime_error("Document worker: " + doc.frame().state->error);
}

std::vector<uint8_t> bytes(const std::filesystem::path &file) {
  std::ifstream stream(file,std::ios::binary);
  return {std::istreambuf_iterator<char>(stream),{}};
}

void compare_portable(const std::filesystem::path &path,const SculptObject &source) {
  ImportLimits limits;
  limits.mode=ImportMode::PreserveGltf;
  const auto asset=import_mesh(path,limits);
  check(asset.primitives.size()==1 && asset.primitives.front().colors.empty(),
        "GLB has one visible patch and no duplicate COLOR_0 multiplier");
  const auto &primitive=asset.primitives.front();
  const auto &material=asset.materials.at(primitive.material);
  auto read=[&](int slot) {
    const auto &texture=asset.textures.at(size_t(slot));
    check(texture.wrap_s==33071 && texture.wrap_t==33071 && texture.min_filter==9729 && texture.mag_filter==9729,
          "Portable material role uses its clamp-linear sampler");
    return decode_projection_png(asset.images.at(size_t(texture.image)).bytes);
  };
  const auto base=read(material.color_texture), normal=read(material.normal_texture), emission=read(material.emissive_texture);
  const auto maps=detail::EmissionExportAccess::other_channels(*source.painted,source.material,{});
  check(base.width==maps->pbr_color.width && base.height==maps->pbr_color.height && base.rgba==maps->pbr_color.rgba &&
        base.rgba!=maps->color.rgba && normal.rgba==source.painted->baked()->normal_field->pbr()->rgba,
        "Independent GLB reader gets the exact resolved colour and normal pixels from the native patch");
  const auto writable=source.painted->paint_sampling_domain()->corner_coordinates(source.painted->mesh());
  const auto &domain=*source.material_bindings->canonical_uv;
  check(material.color_texcoord && material.normal_texcoord && material.emissive_texcoord &&
        writable.size()==primitive.mesh.indices.size() && primitive.tangents.size()==primitive.mesh.positions.size(),
        "GLB carries explicit tangents and separate retained coordinates beside editable UV0");
  auto uv=[&](int set,uint32_t vertex) {
    return set==1?primitive.uv1.at(vertex):primitive.extra_uv_sets.at(uint32_t(set)).at(vertex);
  };
  for (size_t c=0;c<writable.size();++c) {
    const auto v=primitive.mesh.indices[c];
    if (uv(material.color_texcoord,v)!=writable[c] || uv(material.emissive_texcoord,v)!=writable[c] ||
        uv(material.normal_texcoord,v)!=domain.coordinates().at(domain.mesh()->indices.at(c)))
      throw std::runtime_error("Portable colour, normal or radiance texture samples a foreign atlas");
  }
  check(material.emissive_strength>2 && std::ranges::any_of(emission.rgba,[](uint8_t value){return value>0 && value<255;}),
        "Portable emission carries measured HDR strength and a varying texture");
}

enum class ForgedColor { Coordinates, Texture };

std::vector<uint8_t> with_forged_color(const std::vector<uint8_t> &original, ForgedColor forgery) {
  using Json=nlohmann::json;
  auto read=[](const uint8_t *p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);};
  const auto json_size=read(original.data()+12);
  auto document=Json::parse(original.begin()+20,original.begin()+20+json_size);
  auto &package=document.at("extras").at("domeSculptEmission");
  const auto material=package.at("images").front().at("material").get<size_t>();
  auto &entry=document.at("materials").at(material);
  auto &base=entry.at("pbrMetallicRoughness").at("baseColorTexture");
  if(forgery==ForgedColor::Coordinates) base["texCoord"]=0;
  else base["index"]=entry.at("normalTexture").at("index");
  package.erase("documentSha256");
  package["documentSha256"]=creator_detail::sha256(Json::to_cbor(document));
  auto json=document.dump();
  while(json.size()%4) json.push_back(' ');
  const size_t binary=20+json_size;
  std::vector<uint8_t> result;
  for (auto word : {0x46546c67u,2u,uint32_t(20+json.size()+original.size()-binary),uint32_t(json.size()),0x4e4f534au})
    for (size_t k=0;k<4;++k) result.push_back(uint8_t(word>>(8*k)));
  result.insert(result.end(),json.begin(),json.end());
  result.insert(result.end(),original.begin()+binary,original.end());
  return result;
}

bool forgery_rejected(const std::vector<uint8_t> &original, ForgedColor forgery, std::string_view diagnostic) {
  try { (void)extract_native_emission(with_forged_color(original,forgery)); }
  catch (const std::invalid_argument &error) { return std::string_view(error.what()).find(diagnostic)!=std::string_view::npos; }
  return false;
}
}

int main(int argc,char **argv) try {
  if(argc!=3)return 2;
  const std::filesystem::path out=argv[2];
  std::filesystem::create_directories(out);
  setenv("XDG_STATE_HOME",(out/"state").c_str(),1);
  setenv("XDG_CONFIG_HOME",(out/"config").c_str(),1);
  const auto source=Scene::load(argv[1]);
  check(bool(source.active().retopology),"Fixture reopens its actual retained retopology patch");
  const auto guide=source.active().retopology->source;
  Document doc;
  doc.replace(source);
  settle(doc);
  doc.edit_scene("Set physical lighting on the hidden Bake source",[guide](Scene &scene,const auto &){
    scene.object(guide).material.emissive={27,6,14};
  });
  settle(doc);
  ObjectBakeSettings bake;
  bake.front_mm=bake.back_mm=35;
  bake.ao_samples=0;
  bake.allow_backfaces=true;
  doc.bake_from_object(guide,bake);
  settle(doc);
  doc.edit_scene("Paint physical emission on the retained grid",[](Scene &scene,const auto &){
    auto &paint=*scene.active().painted;
    paint.add_layer("Physical rim");
    PaintSettings ink;
    ink.channel=PaintChannel::Emission;
    ink.emission={12,24,4};
    ink.emission_units=PaintEmissionUnits::Physical;
    ink.front_only=false;
    Brush brush;
    const auto &mesh=paint.mesh();
    brush.from=brush.to=(mesh.positions[mesh.indices[0]]+mesh.positions[mesh.indices[1]]+mesh.positions[mesh.indices[2]])/3.f;
    brush.normal={0,0,1};
    brush.radius=.19f;
    brush.strength=.8f;
    paint.begin_stroke();
    paint.stamp(brush,ink);
    if(!paint.end_stroke())throw std::runtime_error("Physical layer needs actual writable coverage");
  });
  settle(doc);
  const auto scene=doc.copy_scene();
  const auto &paint=*scene.active().painted;
  check(scene.active().material_bindings->canonical_paint_sampling && paint.color_projection() &&
        paint.baked() && paint.baked()->normal_field && paint.baked()->emission_field && paint.has_physical_emission(),
        "Real Bake and physical paint create FLOAT emission beside prepared colour and independent normals");
  const auto exact=scene.encode();
  auto standard=doc.export_asset(out/"standard.glb",AssetExportFormat::Glb);
  auto editable=doc.export_asset(out/"editable.glb",AssetExportFormat::EditableGlb256);
  auto saved=doc.save_async(out/"original.dsculpt");
  standard.get();
  editable.get();
  saved.get();
  const auto standard_bytes=bytes(out/"standard.glb");
  auto portable=extract_native_emission(standard_bytes);
  auto restored=extract_native_authoring(bytes(out/"editable.glb"));
  check(portable && portable->encode()==exact && restored && restored->encode()==exact &&
        Scene::load(out/"original.dsculpt").encode()==exact && doc.copy_scene().encode()==exact,
        "Standard HDR package, Editable GLB and ordered Save restore the entire exact native authoring state");
  compare_portable(out/"standard.glb",scene.active());
  compare_portable(out/"editable.glb",scene.active());
  check(forgery_rejected(standard_bytes,ForgedColor::Coordinates,"resolved color differs"),
        "Rehashed package with a foreign base-colour UV is rejected by native role replay");
  check(forgery_rejected(standard_bytes,ForgedColor::Texture,"image differs from its independently owned material field"),
        "Rehashed package borrowing the normal PNG as base colour is rejected by native role replay");
  check(doc.undo() && doc.copy_scene().active().painted->layers().size()+1==paint.layers().size() &&
        doc.redo() && doc.copy_scene().encode()==exact,"Both exports preserve existing native Undo and Redo endpoints");
} catch(const std::exception &error) {
  std::cerr << "FAIL " << error.what() << '\n';
  return 1;
}
