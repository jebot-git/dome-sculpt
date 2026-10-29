// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/weight-edit.hpp"
#include "dome/humanoid-creator.hpp"
#include <iostream>
#include <limits>
using namespace dome;
void require(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
  std::cout << "PASS " << message << '\n';
}
void rejects(auto operation, const char *message) {
  bool failed = false;
  try { operation(); } catch (const std::exception &) { failed = true; }
  require(failed, message);
}
Mesh plane() {
  Mesh m;
  m.positions = {{-1,0,0},{1,0,0},{-1,1,0},{1,1,0}};
  m.normals.assign(4,{0,0,1}); m.indices={0,1,2,1,3,2};
  m.uv={{0,0},{1,0},{0,1},{1,1}}; return m;
}
float weight(const VertexWeights &values, uint32_t joint) {
  for (auto influence : values) if (influence.joint == joint) return influence.weight;
  return 0;
}
int main() try {
  auto mesh=plane(); SkinWeights skin; skin.vertices.assign(4,{{0,1}});
  auto original=skin.vertices; auto topology=morph_topology_id(mesh);
  std::array surfaces{WeightPaintSurface{7,&mesh,&skin,{}}};
  WeightBrush brush; brush.from={-1,0,0};brush.to={-1,1,0};brush.radius=.2f;
  brush.strength=.5f;brush.joint=1;
  auto result=edit_weights(surfaces,3,brush);
  require(result.changed==2 && result.affected==2 && result.constrained==0 &&
          result.surfaces[0].object==7 && result.surfaces[0].topology==topology &&
          weight(result.surfaces[0].weights.vertices[0],1)==.5f &&
          weight(result.surfaces[0].weights.vertices[1],1)==0,
          "Swept capsule edits only geometric hits and retains exact object/topology identity");
  require(skin.vertices==original && morph_topology_id(mesh)==topology,
          "Detached stroke leaves input rest mesh/skin untouched");
  brush.pressure=0;require(edit_weights(surfaces,3,brush).changed==0,
                          "Zero pressure has no effect");brush.pressure=1;
  auto preview=mesh.positions;for(auto&p:preview)p.x+=10;
  surfaces[0].positions=preview;
  require(edit_weights(surfaces,3,brush).changed==0,
          "Explicit posed/world preview coordinates determine brush selection");
  brush.from.x+=10;brush.to.x+=10;
  require(edit_weights(surfaces,3,brush).changed==2,
          "Translated preview brush hits without altering rest topology");
  surfaces[0].positions={};brush.from.x-=10;brush.to.x-=10;
  WeightEditOptions mirrored;mirrored.symmetry=WeightBrushSymmetry{0,{0,2,1}};
  auto symmetric=edit_weights(surfaces,3,brush,mirrored);
  require(symmetric.changed==4 &&
          weight(symmetric.surfaces[0].weights.vertices[0],1)==.5f &&
          weight(symmetric.surfaces[0].weights.vertices[1],2)==.5f,
          "Explicit mirrored capsule and involutive joints paint opposite limbs");
  auto self_mirror=mirrored;self_mirror.symmetry->joints={0,1,2};
  auto wide=brush;wide.radius=3;
  auto once=edit_weights(surfaces,3,wide,self_mirror);
  require(weight(once.surfaces[0].weights.vertices[0],1)==.5f,
          "Overlapping self-mirrored capsule applies maximum falloff once");
  auto bad=mirrored;bad.symmetry->joints={0,2,2};
  rejects([&]{edit_weights(surfaces,3,brush,bad);},"Noninvolutive symmetry is rejected");
  auto shifted=mesh;for(auto&p:shifted.positions)p.x+=.25f;
  surfaces[0].rest=&shifted;mirrored.symmetry->plane_x=.25f;
  brush.from.x+=.25f;brush.to.x+=.25f;
  require(edit_weights(surfaces,3,brush,mirrored).changed==4,
          "Explicit nonzero mirror plane handles offset authored characters");
  surfaces[0].rest=&mesh;brush.from.x-=.25f;brush.to.x-=.25f;
  WeightEditOptions locked;locked.locks={1,0,0};
  SkinWeights constrained;constrained.vertices.assign(4,{{0,.7f},{1,.3f}});
  surfaces[0].weights=&constrained;
  auto blocked=edit_weights(surfaces,3,brush,locked);
  require(blocked.changed==0 && blocked.constrained==2 && !blocked.diagnostics.empty() &&
          blocked.surfaces[0].weights.vertices==constrained.vertices,
          "Impossible locked normalization is reported without clipping or dropping locks");
  std::vector<WeightPaintSurface> many;
  for(uint64_t id=1;id<=12;++id)many.push_back({id,&mesh,&constrained,{}});
  auto aggregate=edit_weights(many,3,brush,locked);
  require(aggregate.constrained==24 && aggregate.diagnostics.size()==8,
          "Aggregate constraint counts remain exact while diagnostics are bounded");
  brush.strength=.1f;brush.mode=WeightBrushMode::Subtract;
  auto reduced=edit_weights(surfaces,3,brush,locked);
  require(reduced.changed==0 && reduced.constrained==2,
          "Remainder cannot move to a hard lock when no other unlocked group exists");
  constrained.vertices.assign(4,{{0,.6f},{1,.2f},{2,.2f}});
  auto valid=edit_weights(surfaces,3,brush,locked);
  require(valid.changed==2 && weight(valid.surfaces[0].weights.vertices[0],0)==.6f &&
          std::abs(weight(valid.surfaces[0].weights.vertices[0],1)-.1f)<1e-6f,
          "Feasible stroke preserves locked float values and redistributes unlocked mass");
  surfaces[0].weights=&skin;brush.mode=WeightBrushMode::Replace;brush.value=.75f;
  brush.strength=1;
  require(std::abs(weight(edit_weights(surfaces,3,brush).surfaces[0].weights.vertices[0],1)-.75f)<1e-6f,
          "Replace mode honors pressure-weighted explicit target weight");
  SkinWeights unsmooth;unsmooth.vertices={{{0,1}},{{0,.5f},{1,.5f}},{{0,.5f},{1,.5f}},{{0,1}}};
  surfaces[0].weights=&unsmooth;brush.mode=WeightBrushMode::Smooth;
  auto smooth=edit_weights(surfaces,3,brush);
  require(smooth.changed==2 && weight(smooth.surfaces[0].weights.vertices[0],1)==.5f &&
          std::abs(weight(smooth.surfaces[0].weights.vertices[2],1)-1.f/6)<1e-6f,
          "Smooth uses immutable triangle adjacency without sequential stroke feedback");
  WeightEditOptions bounded;bounded.maximum_work=1;
  rejects([&]{edit_weights(surfaces,3,brush,bounded);},"Work budget aborts detached stroke");
  bounded={};bounded.maximum_vertices=3;
  rejects([&]{edit_weights(surfaces,3,brush,bounded);},"Vertex budget is enforced before output");
  size_t calls=0;
  rejects([&]{edit_weights(surfaces,3,brush,{},[&]{return ++calls>3;});},
          "Cancellation discards all detached outputs");
  auto duplicate=std::array{surfaces[0],surfaces[0]};
  rejects([&]{edit_weights(duplicate,3,brush);},"Duplicate linked object IDs reject ambiguous publication");
  std::vector<Vec3> nonfinite=mesh.positions;nonfinite[0].x=std::numeric_limits<float>::infinity();
  surfaces[0].positions=nonfinite;
  rejects([&]{edit_weights(surfaces,3,brush);},"Nonfinite posed coordinates are rejected");
  surfaces[0].positions={};
  auto huge = brush;huge.to.x=1e30f;
  rejects([&]{edit_weights(surfaces,3,huge);},
          "Finite but overflowing capsule coordinates are rejected");
  surfaces[0].weights=&skin;
  auto capped = WeightEditOptions{};capped.limits.max_influences=1;
  auto single=brush;single.mode=WeightBrushMode::Replace;single.value=.5f;
  auto cannot_cap=edit_weights(surfaces,3,single,capped);
  require(cannot_cap.constrained==2 && cannot_cap.changed==0,
          "Influence caps report impossible fractional assignments explicitly");
  auto avatar=create_humanoid();std::vector<WeightPaintSurface> linked;
  uint64_t object=1;
  for(const auto&part:avatar.parts)linked.push_back({object++,&part.mesh,&*part.deformation.skin,{}});
  const auto&state=avatar.parts.front().deformation;auto world=state.skeleton->world();
  auto arm=state.humanoid->joints.at("leftUpperArm");
  WeightBrush actual;actual.from=actual.to={world[arm][12],world[arm][13],world[arm][14]};
  actual.radius=.16f;actual.joint=arm;actual.strength=.05f;
  auto painted=edit_weights(linked,state.skeleton->bones.size(),actual);
  require(painted.surfaces.size()==19 && painted.changed>0,
          "Actual CC0 VRoid all nineteen authored parts accept one linked capsule stroke");
  for(size_t i=0;i<19;++i){
    const auto&part=avatar.parts[i];
    painted.surfaces[i].weights.validate(part.mesh.positions.size(),state.skeleton->bones.size());
    require(painted.surfaces[i].topology==morph_topology_id(part.mesh) &&
            part.appearance->source_joint_nodes.size()==state.skeleton->bones.size(),
            "Actual source topology/morph/joint provenance survives detached weight authoring");
  }
  return 0;
}catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
