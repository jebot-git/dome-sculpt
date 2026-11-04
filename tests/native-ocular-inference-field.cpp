// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit private candidate producer. No original artist BIN/weights input.
#include "dome/rig-rest.hpp"
#include "geometry-weight-authoring.hpp"
#include "native-head-geometry.hpp"
#include "rotary-cap-geometry.hpp"
#include "weight-boundary-sewing.hpp"
#include "native-carrier-continuation.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
int main(int argc, char **argv) try {
  if (argc != 3)
    throw std::runtime_error("Choose authentic fitted unskinned Scene and "
                             "automatic geometry-candidate output directory");
  const std::filesystem::path out = argv[2];
  std::filesystem::create_directories(out);
  auto scene = Scene::load(argv[1]);
  const auto before = scene.encode();
  uint64_t reference = 0;
  std::vector<uint64_t> targets;
  for (const auto &o : scene.objects()) {
    if (o.deformation && o.deformation->skeleton)
      reference = o.id;
    if (o.mesh_active && o.painted && o.appearance) {
      if (!o.appearance->asset || !o.appearance->asset->skins.empty())
        throw std::runtime_error("Not a genuine unskinned input");
      for (const auto &p : o.appearance->asset->primitives)
        if (!p.influences.empty())
          throw std::runtime_error("Artist influences in input");
      targets.push_back(o.id);
    }
  }
  if (!reference || targets.size() != 19)
    throw std::runtime_error("Complete genuine19-part native input required");
  detail::DeclaredWeightSupport support;
  support.policy = detail::DeclaredSupportPolicy::UniqueSourceWitnesses;
  support.world_metric = true;
  support.reference = reference;
  support.row_anchors = detail::DeclaredRowAnchors::NearestPermitted;
  support.source_requirements =
      detail::DeclaredSourceRequirements::GeometryEvidence;
  support.source_span = detail::DeclaredSourceSpan::AuthoredFinite;
  support.origin_boundary =
      detail::DeclaredOriginBoundary::NativeFiniteLineIntegral;
  support.contact_support = detail::DeclaredContactSupport::CommonMask;
  support.rig_geometry = detail::DeclaredRigGeometry::NativeBoneAxes;
  const auto analysis = detail::analyze_native_head_geometry(scene, reference);
  if (scene.encode() != before)
    throw std::runtime_error("Automatic analysis changed its captured source");
  // Research candidate only: the same exact boundary algebra applies these
  // GEOMETRY-GENERATED constraints, with no manual target IDs or role palette.
  // They are not represented as user provenance or original-source authority.
  const auto &reference_object = scene.object(reference);
  const auto &rig = *reference_object.deformation->skeleton;
  const auto &roles = reference_object.deformation->humanoid->joints;
  const auto head_joint = roles.at("head");
  detail::HeatJointMask eyes{};
  for (auto role : {"leftEye","rightEye"}) {auto j=roles.at(role);eyes[j/64]|=uint64_t(1)<<(j%64);}
  const std::function<bool()> never = [] { return false; };
  detail::weight_surface::Budget construction{0, 500000000-analysis.work, never};
  auto shift=AffineTransform{}.matrix();auto refworld=scene.world_matrix(reference);
  for(size_t k=0;k<3;++k)shift[12+k]=-refworld.matrix()[12+k];
  const AffineTransform origin(shift);
  std::vector<detail::HeatSourceSegment> eye_points;
  const auto segments=rig.segments();
  for(auto role:{"leftEye","rightEye"}){
    auto j=roles.at(role);auto p=segments[j][0];auto q=(origin*refworld).point({p.x,p.y,p.z});
    const std::array<double,3> point{float(q[0]),float(q[1]),float(q[2])};
    if(rig.bones[j].deform)eye_points.push_back({point,point,j});
  }
  detail::NearestWeightSources nearest_eye(eye_points,construction);
  nlohmann::json inferred;inferred["geometry_generated_not_user_or_source_authority"]=true;inferred["parts"]=nlohmann::json::array();
  size_t temporary_peak=analysis.bytes;
  for(auto id:targets){
    detail::DeclaredWeightPart part{id,{}};
    const auto at=std::find_if(analysis.parts.begin(),analysis.parts.end(),[&](const auto&p){return p.object==id;});
    if(at==analysis.parts.end())throw std::runtime_error("Automatic source part lost its analysis");
    bool ocular=!at->head_region&&at->geometric_controls!=detail::HeatJointMask{};
    for(size_t k=0;k<4;++k)ocular&=(at->geometric_controls[k]&~eyes[k])==0;
    nlohmann::json choices=nlohmann::json::array();
    if(at->head_region||ocular){
      const auto&o=scene.object(id);const auto&m=o.painted->mesh();size_t local=0;
      auto components=detail::heat_intent_components(m,{},local,construction.maximum-construction.used,never);construction.step(local);
      std::map<uint32_t,std::vector<uint32_t>>vertices;
      for(uint32_t i=0;i<components.size();++i){construction.step(16);vertices[components[i]].push_back(i);}
      std::optional<Mesh> metric;
      if(ocular){detail::SkinGeometryWork geom(construction.maximum-construction.used,never);auto available=geom.remaining();metric=detail::skin_world_mesh(m,origin*scene.world_matrix(id),geom);construction.step(available-geom.remaining());}
      for(const auto&[root,rows]:vertices){
        uint32_t joint=head_joint;bool convex=false;
        if(ocular){
          Mesh subset;std::vector<uint32_t>map(m.positions.size(),UINT32_MAX);
          for(auto i:rows){construction.step(8);map[i]=subset.positions.size();subset.positions.push_back(metric->positions[i]);}
          for(size_t c=0;c<m.indices.size();c+=3){construction.step(8);if(components[m.indices[c]]!=root)continue;for(size_t k=0;k<3;++k)subset.indices.push_back(map[m.indices[c+k]]);}
          auto patch=detail::analyze_projected_ocular_patch(subset,128u*1024u*1024u,construction);
          convex=patch.disk&&patch.convex_boundary;
          temporary_peak=std::max(temporary_peak,subset.positions.size()*112+subset.indices.size()*160+32768+m.positions.size()*32);
          if(convex){
            uint32_t selected=UINT32_MAX;
            for(auto p:subset.positions){
              const auto mask=nearest_eye.nearest({p.x,p.y,p.z},construction);size_t count=0;uint32_t candidate=0;
              for(uint32_t j=0;j<rig.bones.size();++j)if(mask[j/64]&(uint64_t(1)<<(j%64))){++count;candidate=j;}
              if(count!=1||(selected!=UINT32_MAX&&selected!=candidate))throw std::runtime_error("Convex ocular patch has ambiguous/cross-eye control support");
              selected=candidate;
            }
            joint=selected;
          }
        }
        part.declarations.push_back({root,{joint}});
        choices.push_back({{"component_root",root},{"control",joint},{"projected_convex_patch",convex},{"head_region_seed",at->head_region}});
      }
      // Same exact algebraic primitive as the explicit-boundary oracle; this
      // research request is generated by the declared geometry rule ABOVE.
      part.user_singleton_boundary=true;
    }else part.user_excluded_joints.push_back(head_joint);
    inferred["parts"].push_back({{"object",id},{"choices",choices},{"head_excluded",!at->head_region&&!ocular}});
    const auto &own=scene.object(id);
    std::vector<std::vector<Vec3>> deltas;
    if(own.deformation&&own.deformation->morphs)
      for(const auto&t:own.deformation->morphs->targets)deltas.push_back(t.delta);
    const auto sewn=detail::geometric_boundary_sewing(own.painted->mesh(),deltas,
                          128u*1024u*1024u,construction);
    temporary_peak=std::max(temporary_peak,sewn.bytes+analysis.bytes);
    part.geometric_row_groups=sewn.groups;
    support.parts.push_back(std::move(part));
  }
  std::ofstream(out/"automatic-constraints.json")<<inferred.dump(2);
  support.hierarchy_joint=head_joint;
  auto head=[&](uint64_t id){for(const auto&p:analysis.parts)if(p.object==id)return p.head_region;throw std::logic_error("Unknown captured part");};
  for(const auto&e:analysis.seam_endpoints) {
    if(head(e.first)!=head(e.second)) {
      const auto other=head(e.first)?e.second:e.first;
      const auto vertex=head(e.first)?e.second_vertex:e.first_vertex;
      if(std::find(support.hierarchy_transition_parts.begin(),support.hierarchy_transition_parts.end(),other)==support.hierarchy_transition_parts.end())
        support.hierarchy_transition_parts.push_back(other);
      support.hierarchy_one_boundary.push_back({other,vertex});
    }else support.geometric_equalities.push_back({e.first,e.second,e.first_vertex,e.second_vertex});
  }
  if(support.hierarchy_transition_parts.empty())support.hierarchy_joint=UINT32_MAX;
  size_t declaration_bytes = support.geometric_equalities.capacity()*sizeof(detail::DeclaredWeightSupport::Equality)+
    support.hierarchy_transition_parts.capacity()*sizeof(uint64_t)+support.hierarchy_one_boundary.capacity()*sizeof(detail::DeclaredWeightSupport::Boundary);
  for (const auto &part : support.parts) {
    declaration_bytes += part.user_excluded_joints.capacity() * sizeof(uint32_t);
    declaration_bytes += part.geometric_row_groups.capacity()*sizeof(uint32_t);
    declaration_bytes += part.declarations.capacity() * sizeof(detail::HeatComponentDeclaration);
    for (const auto &entry : part.declarations)
      declaration_bytes += entry.joints.capacity() * sizeof(uint32_t) + 64;
  }
  const auto carrier=detail::native_carrier_plan(analysis,support,*reference_object.deformation->humanoid,construction);
  std::ofstream(out/"carrier-plan.json")<<nlohmann::json{{"carrier",carrier.carrier},{"targets",carrier.targets},{"policy","geometry-derived carrier plus bounded generated-field continuation"}}.dump(2);
  const size_t continuation_allowance=20'000'000;
  const size_t wrapper_work = analysis.work + construction.used + 4096 + targets.size() * 32,
               wrapper_bytes =
                   sizeof(support) + sizeof(analysis) + analysis.parts.capacity()*sizeof(detail::NativeHeadGeometryReport::Part) + analysis.contacts.capacity()*sizeof(detail::NativeHeadGeometryReport::Contact)+analysis.seam_endpoints.capacity()*sizeof(detail::NativeHeadGeometryReport::Seam) + declaration_bytes +
                   targets.size() * sizeof(detail::DeclaredWeightPart) +
                   scene.objects().size()*(sizeof(SculptObject)+512)+scene.groups().size()*(sizeof(SceneGroup)+512)+65536;
  if(wrapper_work>=500000000||wrapper_bytes>=256u*1024u*1024u)
    throw std::length_error("Automatic request staging exhausted original resources");
  detail::GeometryWeightBindOptions options;
  options.limits = {4, 1e-5f};
  options.maximum_work = 500000000 - wrapper_work-continuation_allowance;
  options.maximum_workspace = 256u * 1024u * 1024u - wrapper_bytes;
  detail::GeometryWeightBindReport report;
  const auto start = std::chrono::steady_clock::now();
  scene.save(out / "fitted-unskinned.dsculpt");
  Scene prepared=scene;
  if (!detail::author_geometry_weight_skin(prepared, targets, support, options, {},
                                           &report))
    throw std::runtime_error("No actual changes");
  const auto seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
          .count();
  const size_t continuation_bytes=detail::continue_native_carrier(prepared,carrier,reference,
                 continuation_allowance,256u*1024u*1024u-wrapper_bytes);
  if(scene.encode()!=before)throw std::runtime_error("Owning anatomical continuation modified its immutable input before adoption");
  scene=std::move(prepared);
  scene.save(out / "bound.dsculpt");
  std::ofstream(out / "report.json")
      << nlohmann::json{{"work", report.work + wrapper_work+continuation_allowance},
                        {"workspace", std::max({report.workspace + wrapper_bytes, temporary_peak,continuation_bytes+wrapper_bytes})},
                        {"seconds", seconds},
                        {"analysis_work", analysis.work},
                        {"analysis_workspace", analysis.bytes},
                        {"contacts", report.field.contacts},
                        {"rows", report.field.rows},
                        {"changed", report.changed},
                        {"partition", report.field.partition},
                        {"linear_residual", report.field.residual},
                        {"linear_iterations", report.field.iterations},
                        {"policy", "private AUTOMATIC source-morph Head plus projected ocular-patch candidate with "
                                   "geometry-derived boundary hypothesis, original finite axes, no fallback"}}
             .dump(2);
  std::cout << "SAVED candidate contacts=" << report.field.contacts
            << " work=" << report.work + wrapper_work+continuation_allowance
            << " bytes=" << report.workspace + wrapper_bytes
            << " seconds=" << seconds << std::endl;
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << std::endl;
  return 1;
}
