// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/rig.hpp"
#ifdef DOME_TEST_WEIGHT_TRANSFER_GPU
#include "weight-transfer-device.hpp"
#endif
#include <iostream>

using namespace dome;
namespace {
void expect(bool result,const char *message){if(!result)throw std::runtime_error(message);}
void append_triangle(Mesh &mesh,Vec3 a,Vec3 b,Vec3 c) {
  const auto first=uint32_t(mesh.positions.size());
  mesh.positions.insert(mesh.positions.end(),{a,b,c});
  mesh.normals.insert(mesh.normals.end(),3,{0,0,1});
  mesh.indices.insert(mesh.indices.end(),{first,first+1,first+2});
}

void transfer_many_points(){
  // Identical triangles have different joints. More than eight source
  // triangles force ties to resolve across independent BVH leaves.
  Mesh source;
  SkinWeights weights;
  for(size_t i=0;i<1024;++i){
    append_triangle(source,{0,0,0},{1,0,0},{0,1,0});
    if(!i)weights.vertices.insert(weights.vertices.end(),{{{0,1}},{{1,1}},{{2,1}}});
    else for(size_t k=0;k<3;++k)weights.vertices.push_back({{3,1}});
  }
  Mesh target;
  constexpr float radius=.03125f;
  for(size_t i=0;i<350;++i)
    append_triangle(target,{.125f,.125f,radius},{.5f,.125f,radius},{.125f,.5f,radius});
  const std::array<VertexWeights,3> expected{{
    {{0,.75f},{1,.125f},{2,.125f}},{{0,.375f},{1,.5f},{2,.125f}},
    {{0,.375f},{1,.125f},{2,.5f}}}};
  auto transferred=transfer_weights(source,weights,target,4,radius);
  for(size_t row=0;row<target.positions.size();++row)
    expect(transferred.vertices[row]==expected[row%3],"Nearest triangle must retain exact tie and radius boundaries across batches");

  target.positions.back().z=.125f;
  bool rejected=false;
  try{(void)transfer_weights(source,weights,target,4,radius);}
  catch(const std::runtime_error &error){
    rejected=std::string_view(error.what()).ends_with("vertex 1049");
  }
  expect(rejected,"A late recipient outside the radius must report its native vertex index");
  std::cout<<"PASS exact 1050-row transfer, coincident source tie, inclusive radius and late refusal\n";
}

#ifdef DOME_TEST_WEIGHT_TRANSFER_GPU
void shared_work_budget() {
  using namespace detail::weight_transfer;
  const std::array nodes{Node{{0,0,0},{1,1,0},0,1,0,0}};
  const std::array<uint32_t,1> order{0};
  const std::array<uint32_t,3> indices{0,1,2};
  const std::array<Vec3,3> vertices{{{0,0,0},{1,0,0},{0,1,0}}};
  auto device=upload({nodes,order,indices,vertices});
  const std::array<Vec3,4> queries{{{.25f,.25f,0},{.5f,.25f,0},{.25f,.5f,0},{.125f,.125f,0}}};
  std::array<Hit,4> results;
  for(auto &hit:results)hit.triangle=99;
  auto visits=device->search(queries,.1f,7,results);
  expect(visits>7,"Shared work must refuse a batch that costs one visit beyond the limit");
  for(auto hit:results)expect(hit.triangle==99,"Refused search changed the caller's result buffer");
  visits=device->search(queries,.1f,8,results);
  expect(visits==8,"Every node and eligible triangle visit must be charged");
  for(size_t i=0;i<results.size();++i)
    expect(results[i].triangle==0&&results[i].weight.b==queries[i].x&&results[i].weight.c==queries[i].y,
           "Retry after device budget exhaustion must recover exact hits");
  std::cout<<"PASS shared GPU visit budget and exact retry\n";
}
#endif
} // namespace

int main() try {
  transfer_many_points();
#ifdef DOME_TEST_WEIGHT_TRANSFER_GPU
  shared_work_budget();
#endif
  return 0;
} catch(const std::exception &error) {
  std::cerr<<"FAIL "<<error.what()<<'\n';
  return 1;
}
