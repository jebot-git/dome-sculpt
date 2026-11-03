// SPDX-License-Identifier: GPL-3.0-or-later
#include "import-authoring-fixtures.hpp"
#include "dome/polygon-scene.hpp"
#include "polygon-conversion.hpp"
#include "polygon-scene-budget.hpp"
#include "scene-feature-version.hpp"
#include <iostream>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);++checks;}
bool refuses(auto operation,std::string_view diagnostic){try{operation();}catch(const std::exception&e){return std::string_view(e.what()).find(diagnostic)!=std::string_view::npos;}return false;}
DeformationState exact_pose(const Mesh &mesh){
 DeformationState d(mesh);d.skeleton=Skeleton{{{"root",-1,{},.2f}}};d.pose.push_back(d.skeleton->bones[0].rest);
 auto matrix=AffineTransform{}.float_matrix();matrix[4]=.25f;matrix[12]=.03f;d.pose[0].full_local_matrix=matrix;return d;
}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;const std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);
 import_fixture::Fixture fixture;while(fixture.bin.size()%4)fixture.bin.push_back(0);const auto uv=fixture.values({0,0,0,1,1,1,1,0},2,"VEC2");
 fixture.j["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_2"]=uv;
 const auto path=directory/"raw-uv2.glb";fixture.save(path);ImportAuthoringOptions options;options.paint_resolution=64;
 auto imported=prepare_authoring_import(Scene{},path,options).scene;uint64_t selected=0;
 for(const auto &object:imported.objects())if(object.appearance){selected=object.id;break;}
 check(selected!=0,"Real native source contains unused authenticated UV2");auto &part=imported.object(selected);
 auto legacy_null=Scene{}.encode();const auto legacy=legacy_null;legacy_null["objects"][0]["rig_source"]=nullptr;
 check(Scene::decode(legacy_null).encode()==legacy,"Legacy nullable rig marker remains no history rather than forcing a newer format");
 legacy_null["objects"][0]["rig_source"]=0;
 check(refuses([&]{Scene::decode(legacy_null);},"Scene25"),"A non-null rig marker still requires its actual source history version");
 const auto old26=imported.encode();
 check(old26.at("version")==26&&std::none_of(old26.at("objects").begin(),old26.at("objects").end(),[](const auto &o){return o.contains("rig_source");}),"UV-only Scene26 retains its original shape without fabricated rig-history fields");
 check(Scene::decode(old26).encode()==old26,"Original UV-only Scene26 records restart exactly in the combined decoder");
 const auto exact=exact_pose(part.painted->mesh());const auto encoded=exact.encode(part.painted->mesh());
 check(encoded.at("version")==2&&DeformationState::decode(encoded,part.painted->mesh()).encode(part.painted->mesh())==encoded,"Exact pose version2 survives native decode without the obsolete unsupported-adapter guard");
 auto malformed=encoded;malformed["version"]=1;
 check(refuses([&]{DeformationState::decode(malformed,part.painted->mesh());},"version two"),"A pose-local marker cannot masquerade as legacy deformation1");
 malformed=encoded;malformed["pose"][0].erase("full_local_matrix");
 check(refuses([&]{DeformationState::decode(malformed,part.painted->mesh());},"requires exact"),"Deformation2 without real absolute local authority refuses");
 detail::PolygonSceneBudget budget;budget.deformation(encoded,25);
 check(budget.deformations==17&&exact.resource_entries()==17,"Retained exact pose charges its sixteen matrix entries and one joint before allocation");
 check(refuses([&]{detail::PolygonSceneBudget old;old.deformation(encoded,24);},"Scene25"),"A down-version retained before-state refuses in aggregate preflight");
 budget.deformations=8'000'000-16;
 check(refuses([&]{budget.deformation(encoded,26);},"budget"),"Scene26 does not exempt exact before-state matrices from the aggregate budget");
 auto materials=nlohmann::json{{"version",2},{"origins",nlohmann::json::array({{{"deformation",encoded}}})}};
 check(SourceMaterialArchive::deformation_entries(materials)==17,"Retained material deformation counting uses the same exact-pose charge");
 // The current object is plain SDF. Both new features exist only in its one
 // immutable rig source, including the original appearance/model/paint tables.
 auto source_deform=part.deformation?*part.deformation:DeformationState(part.painted->mesh());
 source_deform.skeleton=exact.skeleton;source_deform.pose=exact.pose;part.deformation=std::make_shared<const DeformationState>(std::move(source_deform));
 const auto posed26=imported.encode();
 check(posed26.at("version")==26&&std::none_of(posed26.at("objects").begin(),posed26.at("objects").end(),[](const auto &o){return o.contains("rig_source");}),"Current exact pose under Scene26 does not fabricate an unrelated history pool");
 check(Scene::decode(posed26).encode()==posed26,"Scene26 combines current exact poses with raw UV2 through actual Deformation2 decoding");
 Scene retained;retained.active().rig_source=capture_polygon_origin(imported,selected);
 check(detail::minimum_scene_version(retained.active().rig_source.get())==26,"Rig capsule recursively retains both exact pose25 and raw source UV26");
 const auto saved=retained.encode();
 check(saved.at("version")==26&&saved.at("polygon_sources").at("version")==6,"Retained-only actual rig history selects Scene26 and polygon source archive6");
 auto reopened=Scene::decode(saved);
 check(reopened.encode()==saved&&reopened.active().rig_source->deformation->pose[0].full_local_matrix==exact.pose[0].full_local_matrix,"Scene26 decodes actual rig source and exact locals without dropping its before-state");
 check(reopened.active().rig_source->appearance->asset->primitives[0].extra_uv_sets.size()==1,"The same rig source replays its unused raw UV accessor rather than inferring a sampling role");
 retained.save(directory/"retained-rig.dsculpt");
 check(Scene::load(directory/"retained-rig.dsculpt").encode()==saved,"Actual CBOR Save and restart preserve the mixed retained source closure");
 auto downgraded=saved;downgraded["version"]=24;
 check(refuses([&]{Scene::decode(downgraded);},"Scene26")||refuses([&]{Scene::decode(downgraded);},"Scene25"),"Lower outer versions cannot smuggle retained UV26/rig25 authority");
 auto no_rig=Scene{};no_rig.active().painted=part.painted->snapshot();no_rig.active().mesh_active=true;no_rig.active().deformation=std::make_shared<const DeformationState>(exact);
 const auto pose_only=no_rig.encode();
 check(pose_only.at("version")==25&&Scene::decode(pose_only).encode()==pose_only,"Legacy Scene25 exact-pose-only shape remains compatible without actual rig history");
 std::cout<<"PASS "<<checks<<" semantic source25/26 version and before-state checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
