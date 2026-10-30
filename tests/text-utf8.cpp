// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/retopo-authoring.hpp"
#include "../src/text-utf8.hpp"
#include "cgltf.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
template<class F>bool rejects(F f){try{f();return false;}catch(...){return true;}}
}
int main()try {
  for(auto text:{std::string("ASCII / Česky / 中文 / 👩‍🔬 / é"),std::string("\xf4\x8f\xbf\xbf")}) {
    check(dome::text::valid_utf8(text),"Valid scalar text including multibyte and supplementary characters is retained");
    for(size_t bytes=0;bytes<=text.size()+1;++bytes) {
      auto prefix=dome::text::utf8_prefix(text,bytes);
      if(prefix.size()>bytes || !dome::text::valid_utf8(prefix) || text.compare(0,prefix.size(),prefix))
        throw std::runtime_error("Generated-name prefix split a scalar or altered its bytes");
    }
  }
  for(auto invalid:{"\xc0\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\xe2\x82","\x80","\xc2\x41"})
    check(!dome::text::valid_utf8(invalid),"Overlong, surrogate, out-of-range, truncated and stray UTF-8 forms are rejected");
  std::string name="A";for(int n=0;n<81;++n)name+="界";
  Mesh m;m.positions={{0,0,0},{1,0,0},{0,1,0}};m.normals.assign(3,{0,0,1});m.uv={{{0,0}},{{1,0}},{{0,1}}};m.indices={0,1,2};
  Scene scene;scene.active().name=name;scene.active().painted=std::make_shared<PaintedMesh>(m,64);scene.active().mesh_active=true;
  auto source=scene.active().id;auto duplicate=scene.duplicate(source);
  check(dome::text::valid_utf8(scene.object(duplicate).name) && scene.object(duplicate).name.ends_with(" copy"),
        "Duplicating a valid CJK name at the byte budget preserves complete UTF-8 scalars");
  scene.select(source);RetopoMesh topology;std::array<TopologyID,3> ids;
  for(size_t i=0;i<3;++i)ids[i]=topology.add_vertex({m.positions[i],m.uv[i],false});topology.add_face(ids);
  auto target=publish_retopology(scene,topology,source,{});
  check(dome::text::valid_utf8(scene.object(target).name)&&scene.object(target).name.ends_with(" retopo"),
        "Retopology-generated names preserve valid CJK text at the suffix byte boundary");
  auto encoded=scene.encode();check(Scene::decode(encoded).encode()==encoded,"Unicode generated names round-trip with exact native metadata");
  auto path=std::filesystem::temp_directory_path()/("dome-utf8-"+std::to_string(getpid())+".glb");export_glb(scene.surfaces(),path);
  cgltf_options options{};cgltf_data *raw=nullptr;
  check(cgltf_parse_file(&options,path.c_str(),&raw)==cgltf_result_success,"Independent glTF parser accepts Unicode retopology names");
  std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
  check(cgltf_load_buffers(&options,raw,path.c_str())==cgltf_result_success&&cgltf_validate(raw)==cgltf_result_success,
        "Independent glTF validation retains generated scene structure with UTF-8 metadata");
  std::filesystem::remove(path);
  auto bad=encoded;bad["objects"][0]["name"]=std::string("valid prefix")+"\xed\xa0\x80";
  check(rejects([&]{Scene::decode(bad);}),"Native scene input refuses invalid Unicode names rather than publishing corrupt metadata");
  return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
