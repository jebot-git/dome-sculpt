// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-fixture.hpp"
#include "polygon-conversion.hpp"
#include <iostream>
#include <filesystem>
#include <unistd.h>
using namespace dome;
namespace {unsigned checks=0;void check(bool v,const char*m){if(!v)throw std::runtime_error(m);++checks;std::cout<<"PASS "<<m<<'\n';}template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}}
int main()try{
 auto scene=polygon_test::fixture();auto &source=scene.active();auto before=scene.encode();const auto &rest=source.painted->mesh();Mesh target;std::vector<uint32_t> mapping;
 for(auto index:rest.indices){target.positions.push_back(rest.positions[index]);target.normals.push_back(rest.normals[index]);target.uv.push_back({.1f+.8f*rest.uv[index][0],.1f+.8f*rest.uv[index][1]});target.indices.push_back(uint32_t(mapping.size()));target.uv_pins.push_back(index%2);target.uv_seams.push_back(index%3==0);mapping.push_back(index);}
 auto derivative=prepare_polygon_uv_derivative(source,target,mapping);
 check(derivative.mesh.positions.size()==rest.positions.size()&&derivative.mesh.indices==rest.indices,"Split display triangles canonicalize back to exact original N-gon corner domain");
 check(derivative.authority->topology.faces().size()==6&&derivative.authority->topology.next_id()==source.polygon->topology.next_id()&&derivative.authority->topology.encode()["vertices"]==source.polygon->topology.encode()["vertices"]&&derivative.authority->topology.encode()["edges"]==source.polygon->topology.encode()["edges"],"UV editing retains every vertex, edge, corner and face identity without triangulating stored authority");
 for(size_t i=0;i<rest.positions.size();++i){check(derivative.source_vertices[i]==i,"Canonical deformation correspondence remains the exact original packed vertex");if(derivative.mesh.uv[i]!=std::array<float,2>{.1f+.8f*rest.uv[i][0],.1f+.8f*rest.uv[i][1]}||derivative.mesh.uv_pins[i]!=i%2)throw std::runtime_error("UV/pin witness lost exact corner attributes");}
 check(derivative.mesh.uv_seams==target.uv_seams,"Ordered triangle-edge seam flags remain exact after canonicalization");
 check(scene.encode()==before,"Detached UV authority preparation leaves full original paint and scene exact");
 auto repeated=std::find_if(mapping.begin()+1,mapping.end(),[&](auto v){return v==mapping[0];});if(repeated==mapping.end())throw std::runtime_error("Fixture lacks actual shared N-gon triangle corner");auto corner=size_t(repeated-mapping.begin());
 auto malformed=target;malformed.uv[corner][0]+=.005f;check(refuses([&]{prepare_polygon_uv_derivative(source,malformed,mapping);}),"One N-gon corner split into two UV charts refuses explicit face-cut correspondence loss");
 malformed=target;malformed.uv_pins[corner]^=1;check(refuses([&]{prepare_polygon_uv_derivative(source,malformed,mapping);}),"Conflicting pin identity for one N-gon corner is refused rather than guessed");
 malformed=target;malformed.positions[0].x+=.001f;check(refuses([&]{prepare_polygon_uv_derivative(source,malformed,mapping);}),"Changed geometry cannot masquerade as a UV-only edit");
 malformed=target;malformed.normals[0]={0,1,0};check(refuses([&]{prepare_polygon_uv_derivative(source,malformed,mapping);}),"Original canonical/custom normal identity remains guarded");
 auto wrong=mapping;wrong[0]=(wrong[0]+1)%rest.positions.size();check(refuses([&]{prepare_polygon_uv_derivative(source,target,wrong);}),"Wrong source vertex mapping is rejected before publication");
 size_t checkpoints=0;auto measured=prepare_polygon_uv_derivative(source,target,mapping,[&]{++checkpoints;return false;});
 for(size_t boundary=1;boundary<=checkpoints;++boundary){size_t called=0;if(!refuses([&]{prepare_polygon_uv_derivative(source,target,mapping,[&]{return ++called==boundary;});})||scene.encode()!=before)throw std::runtime_error("Cancelled polygon UV preparation changed source");}
 check(true,"Every measured cancellation checkpoint preserves exact full source and local authority");
 auto candidate=scene.snapshot();candidate.active().polygon=derivative.authority;candidate.active().painted=source.painted->reparameterize(derivative.mesh);candidate.validate();
 validate_polygon_derivative(*candidate.active().polygon,candidate.active().painted.get(),true);
 check(candidate.active().polygon->topology.encode()!=source.polygon->topology.encode()&&scene.encode()==before,"Changed UVs and transferred paint share an authenticated canonical N-gon derivative");
 auto path=std::filesystem::temp_directory_path()/("dome-polygon-uv-"+std::to_string(getpid())+".dsculpt");candidate.save(path);check(Scene::load(path).encode()==candidate.encode(),"Scene12 restart retains edited N-gons, UV pins/seams and transferred paint");std::filesystem::remove(path);
 std::cout<<"PASS "<<checks<<" polygon UV checks / "<<checkpoints<<" cancellation boundaries\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
