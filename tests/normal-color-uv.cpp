// SPDX-License-Identifier: GPL-3.0-or-later
#include "current-color-target-fixture.hpp"
#include "dome/document.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/normal-sampling.hpp"
#include "dome/source-uv-bake.hpp"
#include "dome/uv.hpp"
#include "polygon-color-target.hpp"
#include <set>
#include <iostream>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception &e){std::cout<<"REFUSAL "<<e.what()<<std::endl;return true;}}
void ready(Document &d){if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error("Normal/colour native worker: "+d.frame().state->error);}
Scene prepared(bool seam){
 auto scene=current_color_target_fixture::fixture(seam,256);const auto source=scene.objects().back().id;
 cut_color_fixture::pose(scene);auto deformation=*scene.active().deformation;deformation.pose.clear();deformation.morph_weights.assign(deformation.morphs->targets.size(),0);scene.set_deformation(scene.active().id,std::move(deformation));
 auto &high=scene.object(source);auto mesh=high.painted->mesh();for(size_t v=0;v<mesh.positions.size();++v){auto p=mesh.positions[v];mesh.normals[v]=normalized(mesh.normals[v]+Vec3{.19f*p.y,.23f*p.x,.11f*p.z});}high.painted=std::make_shared<PaintedMesh>(std::move(mesh),256);
 const auto original=scene.encode();Document doc;doc.replace(scene);ready(doc);ObjectBakeSettings settings;settings.prepare_colors=settings.prepare_normals=true;settings.ao_samples=0;
 doc.bake_from_object(source,settings);ready(doc);auto result=doc.copy_scene();
 check(result.active().painted->normal_projection()&&result.active().painted->color_projection(),"Actual native Bake prepares two independently owned normal and current-corner colour fields");
 check(doc.undo()&&doc.copy_scene().encode()==original&&doc.redo()&&doc.copy_scene().encode()==result.encode(),"Joint native preparation has one exact whole-source Undo/Redo boundary");
 const auto &bake=*result.active().painted->baked();std::set<std::array<uint8_t,3>> normals;for(size_t p=0;p<bake.coverage.size();++p)if(bake.coverage[p])normals.insert({bake.maps.normal.rgba[p*4],bake.maps.normal.rgba[p*4+1],bake.maps.normal.rgba[p*4+2]});check(normals.size()>4,"Actual Bake supplies varying normal detail rather than a constant substitute");return result;
}
std::array<float,2> front(const Mesh &mesh){size_t f=0;while(f<mesh.indices.size()/3&&mesh.normals[mesh.indices[f*3]].z<.9f)++f;if(f==mesh.indices.size()/3)throw std::runtime_error("Front chart missing");std::array<float,2> p{};for(size_t c=0;c<3;++c){auto uv=mesh.uv[mesh.indices[f*3+c]];for(size_t d=0;d<2;++d)p[d]+=uv[d]/3;}return p;}
Scene transfer(const Scene &scene,UvAction action,float value){
 const auto &object=scene.active();UvWorkspace workspace(object.painted->mesh(),object.painted->resolution());auto p=front(object.painted->mesh());workspace.edit(UvAction::Pick,p[0],p[1]);workspace.edit(action,value);auto view=workspace.view();check(view.valid,"Real selected-chart scale/turn workspace validates");auto derivative=prepare_polygon_uv_derivative(object,*view.mesh,*view.source_vertices,{});
 SourceUvBakeInput input;input.paint=object.painted;input.current_material=object.material_bindings;input.current_deformation=object.deformation;input.polygon_authority=object.polygon;input.target_polygon=derivative.authority;input.target=derivative.mesh;input.source_vertices=derivative.source_vertices;
 check(refuses([&]{bake_source_uv(input);}),"Combined current-corner normals require explicit source-domain retention instead of silently changing canonical axes");
 SourceUvBakeOptions options;options.normal_sampling=SourceNormalUvMethod::RetainSource;
 const auto encoded=scene.encode();auto result=bake_source_uv(input,options);auto adopted=adopt_source_uv_bake(result);
 check(result.normal_sampling&&result.color_reprojection&&result.color_reprojection->uv_reprojection,"One real transfer produces independent exact normal correspondence and newly measured colour quality");
 check(result.normal_sampling->before_material==object.material_bindings&&result.color_reprojection->uv_reprojection->before_material==object.material_bindings&&result.normal_sampling->before_paint==object.painted&&result.color_reprojection->uv_reprojection->before_paint==object.painted,"Both recipes retain the same exact actual before-state without borrowing one another's receipt");
 auto changed=scene.snapshot();auto &target=changed.active();target.painted=adopted.paint;target.material_bindings=adopted.material_bindings;target.deformation=adopted.deformation;target.appearance=adopted.appearance;target.polygon=derivative.authority;changed.validate();auto saved=changed.encode();
 check(saved.at("version")==23&&saved.at("source_materials").at("version")==8&&saved.contains("normal_sampling"),"Joint Scene23/material8 explicitly carries both colour3 and normal3 feature pools");
 auto loaded=Scene::decode(saved);check(loaded.encode()==saved,"Joint field pools and full owning before-graphs restart exactly");
 const auto &current=loaded.active();auto normal=current.material_bindings->normal_preparation->sampling_domain;auto colour=current.material_bindings->color_preparation->uv_reprojection;
 check(normal->before_material==colour->before_material&&normal->before_paint==colour->before_paint,"Archive restart retains exact shared before-state aliases across both independently owned recipes");
 check(current.painted->normal_projection()->sampling_domain->detail==current.painted->baked()->normal_authority,"Normal-specific event identity survives beside independent current RGBA coverage");
 check(current.material_bindings->color_preparation->current_descriptor()->field==current.polygon->colors,"Restarted current descriptor owns the actual stable corner field");
 auto bad=saved;bad.erase("normal_sampling");check(refuses([&]{Scene::decode(bad);}),"Colour material8 cannot substitute for a missing required normal pool");bad=saved;bad["source_materials"]["version"]=7;check(refuses([&]{Scene::decode(bad);}),"Normal material7 cannot silently admit current-colour UV3 records");bad=saved;bad["normal_sampling"]["details"].push_back(bad["normal_sampling"]["details"][0]);check(refuses([&]{Scene::decode(bad);}),"Unreachable duplicated normal event remains an aggregate authority error in a colour archive");
 check(scene.encode()==encoded,"Detached composition preserves exact caller geometry, source capsules, pose, all paint and original receipts");return loaded;
}
}
int main(int argc,char **argv)try{
 const auto directory=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/"normal-colour-uv";std::filesystem::create_directories(directory);
 for(bool seam:{false,true}){auto source=prepared(seam);auto live=*source.active().deformation;live.morph_weights[0]=.6f;live.pose.resize(2);live.pose[0].scale={1.2f,.8f,.7f};live.pose[1].translation={0,.1f,0};live.pose[1].rotation={.99875026f,0,0,.04997917f};source.set_deformation(source.active().id,std::move(live));
 auto first=transfer(source,UvAction::Scale,.7f);auto second=transfer(first,UvAction::Rotate,1.57079632679f);second=transfer(second,UvAction::Conformal,0);auto path=directory/(seam?"seam.dsculpt":"gradient.dsculpt");second.save(path);check(Scene::load(path).encode()==second.encode(),"Repeated deformed scale, turn and conformal unwrap persist the same normal/source/colour/shape and affine capsules");
 Document native;native.replace(source);ready(native);auto before=native.copy_scene().encode();native.begin_uv();check(native.wait_uv(),"Actual native UV loader initializes the joint prepared source");native.edit_uv(UvAction::RetainNormals);auto p=front(source.active().painted->mesh());native.edit_uv(UvAction::Pick,p[0],p[1]);native.edit_uv(UvAction::Scale,.7f);check(native.wait_uv()&&native.frame().state->uv->valid,"Actual native chart scale validates under explicit Keep-detail");native.end_uv(true);auto save=native.save_async(directory/(seam?"native-seam.dsculpt":"native-gradient.dsculpt"));save.get();ready(native);auto accepted=native.copy_scene().encode();check(accepted.at("version")==23&&accepted.contains("normal_sampling"),"Actual native worker commits both required owning recipes");check(native.undo()&&native.copy_scene().encode()==before&&native.redo()&&native.copy_scene().encode()==accepted,"Actual combined UV Apply has one exact global Undo/Redo, preserving deformed frames and all corner state");
 }
 std::cout<<"PASS "<<checks<<" normal/current-colour composition checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
