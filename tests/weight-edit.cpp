// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/weight-edit.hpp"
#include "dome/humanoid-creator.hpp"
#include "weight-brush-kernel.hpp"
#include "weight-preview.hpp"
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
float reference_capsule(Vec3 p,const WeightBrush &brush) {
  const auto direction=brush.to-brush.from;
  const float squared=dot(direction,direction);
  const float t=squared>1e-20f?std::clamp(dot(p-brush.from,direction)/squared,0.f,1.f):0;
  const float u=std::clamp(1-length(p-(brush.from+direction*t))/brush.radius,0.f,1.f);
  return u*u*(3-2*u)*brush.strength*brush.pressure;
}
void check_brush_batches() {
  namespace field=detail::weight_brush;
  std::vector<WeightBrush> direct(257),mirrored;
  std::vector<Vec3> points(16391);
  for(size_t i=0;i<direct.size();++i){
    auto &brush=direct[i];
    brush.from={float(int(i%31)-15)*.217f,float(i%13)*.116f,float(i%5)*.105f};
    brush.to=brush.from+Vec3{.27f,float(int(i%7)-3)*.017f,-.135f};
    brush.radius=.014f+float(i%19)*.015f;
    brush.pressure=float(i%11)/10;
    brush.strength=float(i%7)/6;
    auto &reflection=mirrored.emplace_back(brush);
    reflection.from.x=-brush.from.x;
    reflection.to.x=-brush.to.x;
    points[i]=brush.from;
  }
  direct[2].radius=mirrored[2].radius=std::numeric_limits<float>::denorm_min();
  for(size_t i=direct.size();i<points.size();++i)
    points[i]={float(int(i%269)-134)*.027f,float(i%109)*.017f,float(int(i%41)-7)*.013f};
  auto pack=[](const auto &brushes){
    std::vector<field::Capsule> result;
    for(const auto &b:brushes)result.push_back(field::capsule(b.from,b.to,b.radius,b.strength,b.pressure));
    return result;
  };
  const auto a=pack(direct),b=pack(mirrored);
  auto query=field::upload(a,b);
  size_t samples=0,hits=0;
  for(const unsigned sides:{unsigned(field::Direct),unsigned(field::Reflected),field::Direct|field::Reflected}){
    const size_t batch=field::batch_capsules/(direct.size()*(sides==(field::Direct|field::Reflected)?2:1));
    std::vector<field::Hit> output(batch);
    for(size_t first=0;first<points.size();first+=batch){
      const auto count=std::min(batch,points.size()-first);
      const auto touched=query->search(std::span(points).subspan(first,count),sides,std::span(output).first(count));
      size_t matched=0;
      for(size_t i=0;i<count;++i){
        const auto p=points[first+i];
        float d=0,m=0;
        for(size_t j=0;j<direct.size();++j){
          if(sides&field::Direct)d=std::max(d,reference_capsule(p,direct[j]));
          if(sides&field::Reflected)m=std::max(m,reference_capsule(p,mirrored[j]));
        }
        const float expected=d>=m?d:-m;
        if(expected!=0){
          if(matched>=touched||output[matched].vertex!=i||output[matched].alpha!=expected)
            throw std::runtime_error("Compacted weight hit differs from the original capsule oracle at "+
                std::to_string(first+i)+" expected "+std::to_string(expected));
          ++matched;
        }
      }
      if(matched!=touched)throw std::runtime_error("Compacted weight hits contain unexpected vertices");
      samples+=count;
      hits+=touched;
    }
  }
  std::cout<<"WEIGHT_BRUSH samples="<<samples<<" compact_hits="<<hits<<'\n';
  require(hits>1000&&hits<samples,"Batched direct, reflected and combined strengths match the original capsule oracle exactly");
}
void check_incremental_preview(){
  const auto mesh=plane();
  SkinWeights skin;
  skin.vertices={{{0,.5f},{1,.5f}},{{0,1}},{{0,.8f},{2,.2f}},{{0,1}}};
  const std::array surfaces{WeightPaintSurface{9,&mesh,&skin,{}}};
  WeightEditOptions options;
  options.symmetry=WeightBrushSymmetry{0,{0,2,1}};
  std::array<WeightBrush,3> stroke{};
  stroke[0].from=stroke[0].to={-1,0,0};
  stroke[1].from=stroke[1].to={1,0,0};
  stroke[2].from=stroke[2].to={-1,1,0};
  for(auto &brush:stroke){brush.joint=1;brush.value=.9f;brush.strength=.65f;}
  for(auto mode:{WeightBrushMode::Add,WeightBrushMode::Subtract,WeightBrushMode::Replace,WeightBrushMode::Smooth}){
    for(auto &brush:stroke)brush.mode=mode;
    detail::WeightFalloffPreview cache;
    const auto exact=[&](std::span<const WeightBrush> samples){
      const auto incremental=detail::preview_weights(surfaces,3,samples,options,cache,{});
      const auto replay=edit_weights(surfaces,3,samples,options);
      return cache.prefix.size()==samples.size()&&incremental.affected==replay.affected&&
          incremental.changed==replay.changed&&incremental.constrained==replay.constrained&&
          incremental.diagnostics==replay.diagnostics&&
          incremental.surfaces.at(0).weights.vertices==replay.surfaces.at(0).weights.vertices;
    };
    bool matched=true;
    for(size_t length:{1,2,3,3,1,2})matched&=exact(std::span(stroke).first(length));
    auto changed=stroke;
    changed[0].pressure=.25f;
    matched&=exact(changed);
    require(matched,"Appended, repeated, shortened and changed previews match complete replays including mirrored direct ties");
    unsigned polls=0;
    rejects([&]{detail::preview_weights(surfaces,3,stroke,options,cache,[&]{return ++polls>4;});},
            "Interrupted prefix evaluation discards the partial weight cache");
    require(cache.prefix.empty()&&cache.maxima.empty()&&!cache.query&&exact(stroke),
            "After cancellation a fresh preview reproduces exact weights from the immutable base");
  }
}
void check_resident_preview(){
  namespace field=detail::weight_brush;
  const std::array<Vec3,2> points{{{-1,0,0},{1,0,0}}};
  const auto source=std::span<const Vec3>(points);
  const std::array spans{source.first(1),std::span<const Vec3>{},source.last(1)};
  const std::array left{field::capsule(points[0],points[0],.2f,.6f,1)};
  const std::array right{field::capsule(points[1],points[1],.2f,.6f,1)};
  const std::array weaker{field::capsule(points[1],points[1],.2f,.3f,1)};
  auto query=field::upload(left,right,true);
  unsigned polls=0;
  rejects([&]{query->cache_points(spans,[&]{return ++polls==2;});},
          "An interrupted posed upload leaves no partial resident geometry");
  require(query->cache_points(spans,{})&&polls==2,
          "A resident brush can retry the same immutable surface list after cancellation");
  std::array<field::Hit,2> hits;
  constexpr auto both=field::Direct|field::Reflected;
  require(query->search_cached(0,both,hits)==2&&hits[0].vertex==0&&hits[0].alpha==.6f&&
          hits[1].vertex==1&&hits[1].alpha==-.6f,
          "Compacted resident hits preserve order across separate linked surfaces");
  require(query->search_cached(0,both,hits)==0,
          "Repeated samples do not return unchanged resident falloff");
  query->set_capsules(weaker,left);
  require(query->search_cached(1,both,std::span(hits).first(1))==0,
          "Weaker direct samples preserve the stronger reflected winner without a readback payload");
  query->set_capsules(right,left);
  require(query->search_cached(1,both,std::span(hits).first(1))==1&&hits[0].vertex==0&&hits[0].alpha==.6f,
          "A resident direct sample replaces a tied reflected winner at the correct batch offset");
  rejects([&]{query->search_cached(1,both,hits);},"Resident query rejects an overflow of its posed range");
  query->clear_cached_hits();
  require(query->search_cached(0,both,hits)==2&&hits[0].vertex==0&&hits[0].alpha==-.6f&&
          hits[1].vertex==1&&hits[1].alpha==.6f,
          "Clearing the falloff lets edited prefixes replay against the same resident geometry");
}
void check_linked_resident_ranges(){
  auto mesh=plane();
  const auto part_size=detail::weight_brush::batch_capacity/2+4;
  mesh.positions.resize(part_size,{8,8,8});
  mesh.positions.back()=mesh.positions.front();
  mesh.normals.resize(part_size,{0,0,1});
  mesh.uv.clear();
  SkinWeights skin;
  skin.vertices.assign(mesh.positions.size(),{{0,1}});
  const std::array surfaces{
    WeightPaintSurface{7,&mesh,&skin,{},WeightSurfaceJoints{3,1,2,{}}},
    WeightPaintSurface{3,&mesh,&skin,{},WeightSurfaceJoints{3,{},{},{}}},
    WeightPaintSurface{9,&mesh,&skin,{},WeightSurfaceJoints{3,{},1,{}}},
    WeightPaintSurface{1,&mesh,&skin,{},WeightSurfaceJoints{3,2,{},{}}},
    WeightPaintSurface{5,&mesh,&skin,{},WeightSurfaceJoints{3,2,1,{}}}};
  WeightEditOptions options;
  options.symmetry=WeightBrushSymmetry{0,{0,2,1}};
  WeightBrush left;
  left.from=left.to={-1,0,0};
  left.joint=1;left.strength=.5f;
  auto right=left;
  right.from=right.to={1,0,0};
  const std::array stroke{left,right};
  detail::WeightFalloffPreview cache;
  const auto prefix=detail::preview_weights(surfaces,3,std::span(stroke).first(1),options,cache,{});
  require(cache.resident&&cache.ranges.size()==3&&cache.ranges[0].count==part_size&&
          cache.ranges[1].count==part_size&&cache.ranges[2].count==2*part_size&&prefix.surfaces[1].affected==0,
          "Resident linked ranges group only eligible vertices by direct and reflected brush sides");
  const auto combined=detail::preview_weights(surfaces,3,stroke,options,cache,{});
  const auto replay=edit_weights(surfaces,3,stroke,options);
  bool matches=combined.affected==replay.affected&&combined.changed==replay.changed&&
      combined.diagnostics==replay.diagnostics;
  for(size_t i=0;i<surfaces.size();++i)
    matches&=combined.surfaces[i].object==surfaces[i].object&&
        combined.surfaces[i].weights.vertices==replay.surfaces[i].weights.vertices;
  require(matches&&weight(combined.surfaces[4].weights.vertices.back(),2)==.5f,
          "Batches spanning linked parts preserve original ordering, per-part slots and hits beyond the batch boundary");
}
int main() try {
  check_brush_batches();
  check_resident_preview();
  check_linked_resident_ranges();
  check_incremental_preview();
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
  WeightEditOptions exact_budget;exact_budget.maximum_work=22;
  require(edit_weights(surfaces,3,brush,exact_budget).changed==2,
          "A batched stroke fits the same exact work budget as the original capsule loop");
  --exact_budget.maximum_work;
  rejects([&]{edit_weights(surfaces,3,brush,exact_budget);},
          "One work unit below the complete stroke preserves the input skin");
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
  {
    auto shear = AffineTransform{}.matrix();
    shear[0] = -1.5;
    shear[1] = .35;
    shear[5] = .4;
    shear[12] = .7;
    const AffineTransform placement(shear);
    auto place = [&](Vec3 p) {
      const auto q = placement.point({p.x,p.y,p.z});
      return Vec3{float(q[0]),float(q[1]),float(q[2])};
    };
    auto posed = mesh.positions;
    for (auto &p : posed) p = place(p);
    auto placed = brush;
    placed.from = place(brush.from);
    placed.to = place(brush.to);
    auto placed_mirror = mirrored;
    placed_mirror.symmetry->object_to_brush = placement;
    SkinWeights reordered;
    reordered.vertices.assign(4,{{1,1}});
    std::array<uint8_t, 4> locks{};
    std::array bodies{
      WeightPaintSurface{1,&mesh,&skin,posed,{}},
      WeightPaintSurface{2,&mesh,&reordered,posed,WeightSurfaceJoints{4,3,0,locks}},
      WeightPaintSurface{3,&mesh,&reordered,posed,WeightSurfaceJoints{4,{},0,locks}}};
    const auto solved = edit_weights(bodies,3,placed,placed_mirror);
    require(solved.changed == 10 &&
        weight(solved.surfaces[0].weights.vertices[1],2) == .5f &&
        weight(solved.surfaces[1].weights.vertices[0],3) == .5f &&
        weight(solved.surfaces[1].weights.vertices[1],0) == .5f &&
        weight(solved.surfaces[2].weights.vertices[0],3) == 0 &&
        weight(solved.surfaces[2].weights.vertices[1],0) == .5f,
        "World-metric mirroring follows the authored plane under reflected shear and maps differing skin slot tables");
  }
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
