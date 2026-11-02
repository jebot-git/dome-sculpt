// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "dome/source-maps.hpp"
#include "dome/normal-frames.hpp"
#include "source-uv-archive.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <limits>
using namespace dome;
namespace {unsigned checks=0;void check(bool v,const char *s){if(!v)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<'\n';}bool refuses(auto f){try{f();return false;}catch(const std::exception&){return true;}}}
int main(int argc,char **argv)try{
 Mesh mesh;mesh.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};mesh.normals.assign(4,{0,0,1});mesh.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};mesh.indices={0,1,2,0,2,3};
 auto image=std::make_shared<const Image>(Image{2,2,{255,0,0,0,0,255,0,255,0,0,255,255,255,255,255,255}});
 auto coordinates=std::make_shared<std::vector<std::array<float,2>>>(mesh.uv);for(auto &uv:*coordinates)std::swap(uv[0],uv[1]);
 PaintSourceMaps maps;maps.color.image=image;maps.color.texcoord=2;maps.color.wrap_s=maps.color.wrap_t=33071;maps.color.min_filter=maps.color.mag_filter=9728;maps.extra_uv_sets[2]=coordinates;maps.validate_surface(mesh);
 check(maps.uses_alternate_uv()&&!maps.uses_uv1()&&maps.coordinate_count()==8,"Additional logical source UV ownership does not masquerade as UV1");
 const auto sampled=sample_surface_texture(mesh,maps,maps.color,0,{.1f,.8f,.1f},false);auto primary=maps.color;primary.texcoord=0;const auto wrong=sample_surface_texture(mesh,maps,primary,0,{.1f,.8f,.1f},false);
 check(sampled[2]==1&&sampled[1]==0&&wrong[1]==1&&wrong[2]==0,"Independent UV2 corner/barycentric sampling is visibly distinct from authored UV0");
 check(refuses([&]{sample_source_texture(maps.color,.9f,.1f,false);}),"Bare texture sampling still refuses unresolved UV2 instead of guessing a source set");
 PaintImageArchive images;const auto encoded=encode_source_maps(maps,images);check(encoded.at("version")==2&&encoded.at("payload").at("color").at("texcoord")==2,"Conditional source-map envelope preserves the genuine role UV2 ID");
 const auto decoded=decode_source_maps(encoded,images);check(decoded->color.image==image&&*decoded->extra_uv_sets.at(2)==*coordinates&&decoded->color.texcoord==2,"Source-map restart retains exact images, coordinate arrays and per-role identities");
 check(refuses([&]{detail::source_uv_archive_measure(encoded,25);})&&detail::source_uv_archive_measure(encoded,26).bytes==32,"Scene26 preflight rejects older outer contexts before coordinate allocation");
 auto legacy=maps;legacy.color.texcoord=0;legacy.extra_uv_sets.clear();PaintImageArchive plain_images;auto plain=encode_source_maps(legacy,plain_images);check(!plain.contains("version")&&!plain.contains("extra_uv_sets")&&plain.size()==3,"Ordinary UV0 codec retains its exact historical unwrapped shape");
 auto secondary=legacy;secondary.color.texcoord=1;secondary.uv1=coordinates;PaintImageArchive second_images;auto old_uv1=encode_source_maps(secondary,second_images);check(!old_uv1.contains("version")&&old_uv1.at("source_uv1").at("version")==1&&decode_source_maps(old_uv1,second_images)->extra_uv_sets.empty(),"Historical UV1 record1 remains independent and unchanged");
 Scene scene;scene.active().painted=PaintedMesh::from_maps(mesh,std::make_shared<const PaintSourceMaps>(maps),64);scene.active().mesh_active=true;const auto stored=scene.encode();check(stored.at("version")==26&&Scene::decode(stored).encode()==stored,"Actual native Scene26 preserves a complete owned additional-UV source surface");
 for(int version:{18,19,20,21,22,23,24,25}){auto bad=stored;bad["version"]=version;check(refuses([&]{Scene::decode(bad);}),"Older or unsupported Scene tags cannot smuggle extra source-coordinate authority");}
 auto bad=maps;bad.extra_uv_sets.clear();check(refuses([&]{bad.validate_surface(mesh);}),"Referenced UV2 without a coordinate owner refuses");
 bad=maps;bad.extra_uv_sets[3]=coordinates;check(refuses([&]{bad.validate_surface(mesh);}),"Unreferenced extra-coordinate owner cannot hide in an active map record");
 auto short_values=std::make_shared<std::vector<std::array<float,2>>>(*coordinates);short_values->pop_back();bad=maps;bad.extra_uv_sets[2]=short_values;check(refuses([&]{bad.validate_surface(mesh);}),"Additional coordinates must match the actual complete REST vertex domain");
 auto invalid=std::make_shared<std::vector<std::array<float,2>>>(*coordinates);invalid->front()[0]=std::numeric_limits<float>::quiet_NaN();bad=maps;bad.extra_uv_sets[2]=invalid;check(refuses([&]{bad.validate_surface(mesh);}),"Nonfinite additional source coordinates refuse");
 auto malformed=encoded;malformed["extra_uv_sets"].push_back(malformed["extra_uv_sets"][0]);check(refuses([&]{decode_source_maps(malformed,images);}),"Duplicate persisted logical source UV identities refuse");
 malformed=encoded;malformed["extra_uv_sets"][0]["vertices"]=2'000'001;check(refuses([&]{decode_source_maps(malformed,images);}),"Oversized declared additional source-coordinate storage refuses before allocation");
 malformed=encoded;malformed["version"]=3;check(refuses([&]{decode_source_maps(malformed,images);}),"Unknown source-map interpretation cannot be silently adopted");
 bad=maps;bad.color.texcoord=7;bad.extra_uv_sets.clear();bad.extra_uv_sets[7]=coordinates;bad.validate_surface(mesh);check(source_texture_coordinates(mesh,bad,7).data()==coordinates->data(),"Sparse genuine UV7 identity resolves through its own owner without renumbering");
 NormalFrames frame;frame.origin_vertex_count=4;frame.origin_vertices=mesh.indices;frame.normals.assign(6,{0,0,1});frame.tangents.assign(6,{1,0,0,1});frame.generated_tangents=true;frame.generated_uv=2;const auto frame2=frame.encode();check(frame2.at("version")==2&&NormalFrames::decode(frame2).generated_uv==2,"Generated tangent provenance uses conditional normal-frame record2 for the genuine UV2 basis");
 auto mislabeled=frame2;mislabeled["version"]=1;check(refuses([&]{NormalFrames::decode(mislabeled);}),"Legacy frame1 cannot relabel generated UV2 tangent authority");frame.generated_uv=1;check(frame.encode().at("version")==1,"Historical UV0/1 tangent provenance continues to emit frame record1");
 check(scene.encode()==stored,"All malformed and sampling probes preserve the complete original native source");
 if(argc==2){auto bundle=nlohmann::json{{"images",images.encode()},{"maps",encoded},{"legacy_maps",plain},{"uv1_maps",old_uv1},{"frames",frame2},{"legacy_frames",frame.encode()},{"scene",stored}};const auto bytes=nlohmann::json::to_cbor(bundle);std::ofstream file(argv[1],std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));if(!file)throw std::runtime_error("Legacy-reader fixture write failed");}
 std::cout<<"PASS "<<checks<<" genuine extra source UV foundation checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
