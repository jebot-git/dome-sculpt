// SPDX-License-Identifier: GPL-3.0-or-later
#define main previous_key_mirror_tests
#include "independent-seam-key-mirror.cpp"
#undef main
#include "dome/source-paint.hpp"
#include "dome/source-maps.hpp"
#include "normal-frame-runtime.hpp"
#include "material-archive-version.hpp"
#include "source-paint-pairing.hpp"
int main(int argc,char**argv)try{
 if(argc!=2)return 2;std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 auto scene=fixture(false);auto &o=scene.active();const auto current=o.painted->mesh();auto raw=current;for(auto&uv:raw.uv){uv[0]=3*uv[0]-1;uv[1]=2*uv[1]-.5f;}
 auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=std::make_shared<const Image>(Image{2,2,{120,30,90,255,30,170,70,255,80,120,220,255,230,190,20,255}});maps->color.min_filter=maps->color.mag_filter=9728;
 PaintSurfaceOptions options;options.policy=PaintUvPolicy::SourceAuthored;auto before=PaintedMesh::from_maps(raw,maps,64,options);std::vector<uint32_t> vertices(current.positions.size());for(size_t v=0;v<vertices.size();++v)vertices[v]=uint32_t(v);o.painted=prepare_source_paint(before,current,vertices);scene.validate();
 check(o.painted->source_sampling_prepared()&&o.painted->valid_source_sampling_attachment(),"Actual original-source sampling4 remains independently attached to the existing native provided-frame source");
 o.painted->add_layer("Physical emission before Mirror");PaintSettings settings;settings.channel=PaintChannel::Emission;settings.emission_units=PaintEmissionUnits::Physical;settings.emission={3,.25f,.05f};settings.front_only=false;Brush brush;brush.from=brush.to={0,0,0};brush.normal={0,0,1};brush.radius=.4f;brush.strength=.7f;o.painted->begin_stroke();o.painted->stamp(brush,settings);check(o.painted->end_stroke()&&o.painted->has_physical_emission(),"Real Source4 writable grid owns physical Paint3 emission before named key authoring");
 const auto original=scene.encode();const auto target=o.id;const auto original_uv=o.painted->source_sampling_original()->mesh().uv;ShapeKeyEdit mirror;mirror.action=ShapeKeyAction::Mirror;mirror.target=0;mirror.name="Physical original-source Mirror";author_shapekey(scene,target,mirror);check(scene.active().painted->valid_source_sampling_attachment(),"Real named key authoring preserves original sampling and physical layer ownership");
 auto changed=scene.active().painted->snapshot();changed->delete_layer();scene.active().painted=changed;scene.validate();check(!changed->has_physical_emission()&&scene.active().material_bindings->key_derivation->before_paint()->has_physical_emission(),"Current layer removal leaves physical emission only in the immutable key before-paint receipt");
 const auto encoded=scene.encode();check(encoded.at("version")==28&&encoded.at("source_materials").at("version")==11&&encoded.at("source_materials").at("base_version")==10,"Writer composes actual28 sampling and physical11 over genuine owning key table10");
 scene.save(out/"source4-key-hdr.dsculpt");auto read=Scene::load(out/"source4-key-hdr.dsculpt");check(read.encode()==encoded,"Scene28 source/physical/key closure Save/reopen is exact");
 check(read.active().painted->valid_source_sampling_attachment()&&read.active().painted->source_sampling_original()->mesh().uv==original_uv&&!read.active().painted->has_physical_emission()&&read.active().material_bindings->key_derivation->before_paint()->has_physical_emission(),"Owning alias decode retains genuine original coordinates/proof and separate current/before physical planes");
 auto bad=encoded;bad["source_materials"]["base_version"]=9;check(refuses([&]{Scene::decode(bad);},"key"),"HDR wrapper cannot relabel genuine key records as unsupported old base9");
 bad=encoded;bad["version"]=27;check(refuses([&]{Scene::decode(bad);},"source")&&scene.encode()==encoded,"Source4 remains an actual feature requirement in every key before-owner");
 ShapeKeyEdit restore;restore.action=ShapeKeyAction::Restore;author_shapekey(read,target,restore);check(read.encode()==original,"Complete key Restore recovers original sampling, physical emission and all old artist/rig state");
 std::cout<<"PASS "<<checks<<" key/HDR/original-source integration checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
