// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "dome/color-preparation.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/texture-sets.hpp"
#include "dome/source-maps.hpp"
#include "baked-emission-access.hpp"
#include "baked-emission-write-scope.hpp"
#include "native-read-scope.hpp"
#include "paint-overlay-sample.hpp"
#include <bit>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace {
using namespace dome;
size_t checks=0;
void check(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);}
template<class F>void refuses(F f,const char *message){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,message);}
void exact(float a,float b,const char *message){check(std::bit_cast<uint32_t>(a)==std::bit_cast<uint32_t>(b),message);}
Mesh quad(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
Image image(std::array<uint8_t,4> value){Image i{64,64,std::vector<uint8_t>(64*64*4)};for(size_t k=0;k<i.rgba.size();++k)i.rgba[k]=value[k%4];return i;}
BakedTextures legacy(){BakedTextures b;b.maps.color=image({22,47,92,255});b.maps.metal_rough=image({113,62,31,255});b.maps.emission=image({180,92,33,255});b.maps.normal=image({128,128,255,255});b.coverage.assign(4096,255);return b;}
BakedTextures floating(BakedEmissionSample value,uint8_t common=255){auto b=legacy();b.maps.emission={};b.coverage.assign(4096,common);b.emission_field=BakedEmissionField::create(64,std::vector<BakedEmissionSample>(4096,value));return b;}
Scene scene(std::shared_ptr<PaintedMesh> p){Scene s;s.active().mesh_active=true;s.active().painted=std::move(p);return s;}
void writer_controls(){
  for(bool forged:{false,true}){
    auto raw=MToonMaterial{}.source_json();
    raw["extras"]["opaque"]={{"baked",{{"version",6}}}};
    if(forged){
      const auto field=BakedEmissionField::create(8,std::vector<BakedEmissionSample>(64,BakedEmissionSample{{},{50000,0,0},1}));
      raw["extras"]["opaque"]={{"resolution",64},{"active",1},{"next_id",2},{"layers",nlohmann::json::array()},
        {"baked",{{"version",6},{"base_version",1},{"float_emission",nlohmann::json::binary(field->encode())},{"emission",nlohmann::json::binary(std::vector<uint8_t>{})}}}};
    }
    auto binding=std::make_shared<SourceMaterialBindings>();binding->material=MToonMaterial::from_vrm1(raw);binding->validate();
    auto value=scene(std::make_shared<PaintedMesh>(quad(),64));value.active().material_bindings=binding;
    const auto encoded=value.encode();
    check(encoded.at("version")!=41,"retained opaque MToon JSON cannot fabricate a FLOAT writer capability");
    check(Scene::decode(encoded).encode()==encoded,"opaque bare/forged-paint/binary material metadata preserves exact old wire");
  }
  auto paint=std::make_shared<PaintedMesh>(quad(),64);paint->install_bake(floating({{},{50000,.00001f,2},1}));
  {
    PaintImageArchive real,foreign;detail::BakedEmissionWriteScope outer;outer.begin(real);
    (void)paint->encode();(void)paint->encode(&foreign);
    check(!outer.emitted(),"discarded/null/foreign-archive hash encodes cannot manufacture writer feature");
    const auto nested=scene(paint).encode();
    check(nested.at("version")==41&&!outer.emitted(),"actual nested Scene encoder binds its own archive without contaminating parent");
    bool child_emitted=false;
    try{
      PaintImageArchive child;detail::BakedEmissionWriteScope inner;inner.begin(child);
      (void)paint->encode(&child);child_emitted=inner.emitted();
      throw std::runtime_error("intentional writer unwind");
    }catch(const std::runtime_error&){}
    check(child_emitted,"nested scope recognizes actual child field bytes");
    check(!outer.emitted(),"writer exception unwind restores clean outer scope");
    (void)paint->encode(&real);check(outer.emitted(),"only actual successfully emitted owning archive grants writer feature");
  }
  ColorPreparationOptions options;options.resolve_identity=true;
  const auto prepared=prepare_source_colors({paint,{},{},{},Material{}},options);
  auto retained=scene(prepared.paint->snapshot());retained.active().material_bindings=prepared.material_bindings;
  retained.active().painted->clear_bake();
  check(!retained.active().painted->baked()&&retained.active().material_bindings->color_preparation->original_paint->baked()->emission_field,"actual persisted source history retains FLOAT after current base is cleared");
  const auto history=retained.encode();check(history.at("version")==41&&Scene::decode(history).encode()==history,"writer feature follows actual emitted history owner rather than only current paint");
  Scene sets;std::array<TextureSetInitialField,2> fields;for(auto &field:fields)field.resolution=64;
  initialize_texture_sets(sets,sets.active().id,quad(),fields,std::array<TextureSetId,2>{0,1});
  check(edit_texture_set(sets,sets.active().id,1,[](PaintedMesh &paint,Material &){paint.install_bake(floating({{},{50000,.00001f,2},1}));return true;}),"actual public set mutation adopts independent FLOAT owner");
  check(!sets.active().painted->baked()&&sets.active().texture_sets->at(1).painted->baked()->emission_field,"additional set is the genuine FLOAT owner, primary remains legacy");
  const auto encoded=sets.encode();check(encoded.at("version")==41&&encoded.at("payload").at("version")==40,"Scene41 retains actual Scene40 set ownership beneath its envelope");
  check(Scene::decode(encoded).encode()==encoded,"nonprimary shared-geometry field and Scene40/Paint7 capabilities restart exactly");
  refuses([&]{Scene::decode(encoded.at("payload"));},"old Scene40 cannot claim new baked data without its explicit envelope");
}
} // namespace
int main(int argc,char **argv)try{
  using namespace dome;
  writer_controls();
  if(argc==2&&std::string_view(argv[1])=="--writer-controls"){
    std::cout<<"Baked emission writer "<<checks<<" checks PASS\n";return 0;
  }
  if(argc!=1)throw std::invalid_argument("baked-emission-native [--writer-controls]");
  auto paint=std::make_shared<PaintedMesh>(quad(),64);
  paint->install_bake(legacy());
  const auto old_scene=scene(paint).encode();
  check(old_scene.at("version")!=41,"legacy Scene wire is not promoted without a genuine FLOAT base");
  check(Scene::decode(old_scene).encode()==old_scene,"old RGB8 base roundtrip unchanged");
  paint->add_layer("Hidden physical metadata");
  Brush brush;brush.from=brush.to={};brush.normal={0,0,1};brush.radius=2;brush.strength=.8f;
  PaintSettings settings;settings.channel=PaintChannel::Emission;settings.emission_units=PaintEmissionUnits::Physical;settings.emission={9,3,.0001f};settings.front_only=false;
  paint->begin_stroke();paint->stamp(brush,settings);check(paint->end_stroke(),"genuine editable physical layer authored");
  paint->layer_properties("Hidden physical metadata",false,.375f);
  const auto layers=paint->layers();
  const auto held=paint->snapshot();
  const auto held_value=held->encode();
  const auto old=paint->baked();
  const size_t center=32*64+32;
  const auto prior=detail::baked_emission_texel(*old,center);
  const float tiny=0x1p-24f;
  const BakedEmissionSample incoming{{},{50000*tiny,2*tiny,.25f*tiny},tiny};
  auto next=floating(incoming,0); // Independent emission coverage despite common R8 zero.
  const auto expected=BakedEmissionField::over(prior,incoming);
  paint->install_bake(std::move(next));
  check(paint->has_physical_emission()&&paint->baked()->emission_field,"actual FLOAT base retains its interpretation");
  check(paint->baked()->maps.emission.rgba.empty(),"RGB8 emission is not a duplicate FLOAT authority");
  check(paint->baked()->maps.color.rgba==old->maps.color.rgba&&paint->baked()->maps.metal_rough.rgba==old->maps.metal_rough.rgba&&paint->baked()->maps.normal.rgba==old->maps.normal.rgba&&paint->baked()->coverage==old->coverage,"zero common coverage preserves unrelated channels and normal bytes");
  const auto actual=paint->baked()->emission_field->values()[center];
  for(size_t k=0;k<3;++k){exact(actual.legacy[k],expected.legacy[k],"old RGB8 relative coefficients retain factor semantics");exact(actual.absolute[k],expected.absolute[k],"tiny HDR absolute coefficients retain energy");}
  exact(actual.coverage,expected.coverage,"independent FLOAT base-over coverage");
  check(held->encode()==held_value&&held->baked()==old,"immutable prior snapshot and its base unchanged");
  check(paint->layers().size()==layers.size(),"layer count retained");
  for(size_t i=0;i<layers.size();++i){const auto &a=layers[i],&b=paint->layers()[i];check(a.tiles==b.tiles&&a.pbr_tiles==b.pbr_tiles&&a.emission_tiles==b.emission_tiles&&a.name==b.name&&a.id==b.id&&a.visible==b.visible&&a.opacity==b.opacity,"hidden and visible editable channel owners retained exactly");}
  auto packet=detail::PaintOverlayFactory::capture_native_emission(*paint);
  check(detail::overlay_uses_float_emission(*packet),"FLOAT base selects actual linear preview path");
  const auto sample=detail::sample_overlay_texel_values(*packet,32,32,false);
  for(size_t k=0;k<3;++k){exact(sample[2][k],actual.legacy[k],"CPU preview legacy base matches native");exact(sample[4][k],actual.absolute[k],"hidden physical layers do not alter visible base");}
  exact(sample[3][3],actual.coverage,"CPU preview independent emission coverage");
  auto visible=paint->snapshot();visible->layer_properties("Hidden physical metadata",true,.375f);
  auto visible_packet=detail::PaintOverlayFactory::capture_native_emission(*visible);
  const auto visible_sample=detail::sample_overlay_texel_values(*visible_packet,32,32,false);
  const auto pixel=visible->emission_pixel(visible->active_layer(),32,32);
  const float a=pixel.coverage*.375f*visible->pixel(visible->active_layer(),32,32).data[13]/255.f;
  for(size_t k=0;k<3;++k){exact(visible_sample[4][k],actual.absolute[k]*(1-a)+pixel.physical[k]*a,"actual editable mask/opacity composes above immutable FLOAT base");}
  const auto encoded=scene(paint).encode();
  check(encoded.size()==3&&encoded.at("version")==41&&encoded.at("payload").at("version")!=41,"genuine Scene41 envelope retains prior exact scene capability");
  auto bytes=nlohmann::json::to_cbor(encoded);
  auto restarted=Scene::decode(nlohmann::json::from_cbor(bytes));
  check(restarted.encode()==encoded,"native CBOR preserves FLOAT base, original channels, hidden layers and metadata exactly");
  check(restarted.active().painted->baked()->emission_field->encode()==paint->baked()->emission_field->encode(),"native FLOAT bits survive actual owning decode");
  refuses([&]{Scene::decode(encoded.at("payload"));},"stripping Scene41 cannot retain its new payload capability");
  refuses([&]{PaintedMesh::decode(paint->encode());},"standalone legacy paint reader cannot manufacture Scene41 authority");
  auto fake=nlohmann::json{{"format","dome-sculpt"},{"version",41},{"payload",old_scene}};
  refuses([&]{Scene::decode(fake);},"empty/fake Scene41 requires a genuinely decoded FLOAT owner");
  fake["payload"]=encoded;refuses([&]{Scene::decode(fake);},"direct nested Scene41 envelopes refused");
  fake=encoded;fake["version"]=41.;refuses([&]{Scene::decode(fake);},"Scene41 version must be an integer");
  fake=encoded;fake["extra"]=1;refuses([&]{Scene::decode(fake);},"Scene41 exact wrapper shape");
  fake=encoded;fake["payload"]["objects"][0]["painted"]["payload"]["baked"]["absolute_units"]="half";
  refuses([&]{Scene::decode(fake);},"unknown new FLOAT interpretation metadata cannot be silently discarded");
  auto unbaked=std::make_shared<PaintedMesh>(quad(),64);
  auto independent=floating(incoming,0);unbaked->install_bake(independent);
  auto independent_packet=detail::PaintOverlayFactory::capture_native_emission(*unbaked);
  auto independent_sample=detail::sample_overlay_texel_values(*independent_packet,32,32,false);
  exact(independent_sample[3][0],0,"common color coverage remains zero");
  exact(independent_sample[3][3],tiny,"emission FLOAT coverage not replaced by common R8 coverage");
  check(independent_sample[4][0]>0,"tiny HDR energy survives native preview sampling");
  auto malformed=independent;malformed.maps.emission=image({0,0,0,255});
  refuses([&]{unbaked->install_bake(malformed);},"duplicate RGB8 authority refused");
  check(unbaked->baked()->emission_field==independent.emission_field,"refused install leaves prior native owner unchanged");
  {
    size_t calls=0;
    detail::NativeReadScope stop([&]{return ++calls>=4;});
    refuses([&]{Scene::decode(encoded);},"actual owning read cancellation interrupts new FLOAT decode");
  }
  check(Scene::decode(encoded).encode()==encoded,"failed/cancelled scoped read does not poison next decode");
  ColorPreparationOptions color_options;color_options.resolve_identity=true;
  const auto color=prepare_source_colors({paint,{},{},{},Material{}},color_options);
  check(color.converted&&color.paint->baked()->emission_field==paint->baked()->emission_field&&color.paint->baked()->maps.normal.rgba==paint->baked()->maps.normal.rgba,"actual colour preparation retains exact FLOAT owner and canonical normal channel");
  auto colored=scene(color.paint);colored.active().material_bindings=color.material_bindings;
  const auto colored_value=colored.encode();check(Scene::decode(colored_value).encode()==colored_value,"prepared colour's owning source archive retains original FLOAT and hidden layers exactly");
  const auto normal=convert_canonical_normals({paint,{},{},{},Material{}});
  check(normal.converted&&normal.paint->baked()->emission_field==paint->baked()->emission_field&&normal.paint->baked()->maps.color.rgba==paint->baked()->maps.color.rgba&&normal.paint->baked()->maps.metal_rough.rgba==paint->baked()->maps.metal_rough.rgba,"actual canonical normal conversion retains exact FLOAT and other material channels");
  auto normals=scene(normal.paint);normals.active().material_bindings=normal.material_bindings;
  const auto normal_value=normals.encode();check(Scene::decode(normal_value).encode()==normal_value,"independent normal-field source archive preserves genuine FLOAT native ownership");
  std::cout<<"Native baked emission "<<checks<<" checks PASS\n";
  return 0;
}catch(const std::exception &error){std::cerr<<"Native baked emission FAIL after "<<checks<<": "<<error.what()<<'\n';return 1;}
