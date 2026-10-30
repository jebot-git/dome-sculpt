// SPDX-License-Identifier: GPL-3.0-or-later
// Actual public Document workflow across separately developed subsystems.
#include "dome/document.hpp"
#include "dome/import.hpp"
#include "dome/humanoid-creator.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<std::endl;}
void ready(Document &d){if(!d.wait_asset()||!d.wait_mesh())throw std::runtime_error("Authoring worker failed to become ready: "+d.frame().state->error);if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
nlohmann::json canonical(Scene scene){auto j=scene.encode();for(auto &o:j["objects"]){auto &v=o["volume"];std::sort(v["samples"].begin(),v["samples"].end());for(auto &l:v["sculpt_layers"])std::sort(l["delta"].begin(),l["delta"].end());if(v.contains("sculpt_mask"))std::sort(v["sculpt_mask"]["values"].begin(),v["sculpt_mask"]["values"].end());}return j;}
nlohmann::json source_record(Scene scene,uint64_t id){auto j=canonical(std::move(scene));for(auto o:j["objects"])if(o["id"]==id){o.erase("visible");return o;}throw std::runtime_error("Retained source disappeared");}
std::vector<uint8_t> bytes(const std::filesystem::path &p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
Scene painted_cube(){Mesh m;constexpr float r=.12f;m.positions={{-r,-r,-r},{r,-r,-r},{r,r,-r},{-r,r,-r},{-r,-r,r},{r,-r,r},{r,r,r},{-r,r,r}};for(auto p:m.positions)m.normals.push_back(normalized(p));m.indices={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};unwrap(m,128);
  Scene scene;auto &o=scene.active();o.name="Source 日本語";o.painted=std::make_shared<PaintedMesh>(m,128);o.mesh_active=true;o.transform.position={.3f,.1f,-.2f};o.transform.scale=1.6f;
  auto group=scene.add_group("Scaled hierarchy",{{.1f,0,0},{},1.25f});o.group=group;o.material.color={.7f,.8f,.9f,1};
  PaintSettings ink;ink.color={.8f,.05f,.2f,1};ink.front_only=false;Brush brush;brush.from=brush.to={0,0,0};brush.radius=1;o.painted->begin_stroke();o.painted->stamp(brush,ink);o.painted->end_stroke();return scene;
}
}
int main(int argc,char **argv)try {
  auto directory=argc==2?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/("dome-authoring-journey-"+std::to_string(getpid()));std::filesystem::create_directories(directory);
  auto input=directory/"painted-source.glb",project=directory/"authored.dsculpt",output=directory/"authored.glb";
  auto native=painted_cube();export_glb(native.surfaces(),input);auto original_bytes=bytes(input);
  Document d;auto initial_endpoint=canonical(d.copy_scene());d.import_asset(input,128);ready(d);auto imported=d.copy_scene();auto source=imported.active().id;auto retained=source_record(imported,source);
  check(imported.active().mesh_active&&imported.active().appearance&&*imported.active().appearance->original_bytes==original_bytes,"Native import retains exact painted source GLB and authored hierarchy");
  check(std::abs(imported.world_transform(source).scale-2)<1e-6f,"Import preserves complete nested world scale");
  auto import_endpoint=canonical(imported);d.remesh_to_sculpt(40);ready(d);auto remeshed=d.copy_scene();auto target=remeshed.active().id;auto placement=remeshed.world_transform(target);
  check(target!=source&&!remeshed.object(source).visible&&!remeshed.active().mesh_active,"Explicit remesh creates an editable sculpt and retains the original mesh");
  check(source_record(remeshed,source)==retained&&std::abs(remeshed.active().volume->voxel_size()-.02f)<1e-6f,"Remeshing preserves exact imported source and uses world-mm spacing");
  auto remesh_endpoint=canonical(remeshed);ProceduralSculptOptions detail;detail.rows=detail.columns=2;detail.density=1;detail.depth=.06f;
  d.apply_procedural_sculpt(detail);ready(d);auto detailed=d.copy_scene();
  check(detailed.active().volume->layers().size()==2&&source_record(detailed,source)==retained,"Procedural detail changes the new sculpt while original source paint and provenance remain exact");
  auto detail_endpoint=canonical(detailed);check(d.undo()&&canonical(d.copy_scene())==remesh_endpoint,"One detail Undo restores the remeshed endpoint exactly");ready(d);check(d.redo()&&canonical(d.copy_scene())==detail_endpoint,"Detail Redo restores exact generated geometry");ready(d);
  d.prepare_mesh(128);ready(d);check(d.copy_scene().active().mesh_active&&d.copy_scene().active().painted,"Detailed sculpt becomes a writable textured mesh through the normal native workflow");
  d.copy_scene().save(directory/"before-bake.dsculpt");
  ObjectBakeSettings bake;bake.front_mm=bake.back_mm=250;bake.ao_samples=4;bake.padding=2;auto before_bake=canonical(d.copy_scene());d.bake_from_object(source,bake);check(d.wait_asset()&&d.wait_mesh(),"Strict projection refusal drains the worker");
  check(d.frame().state->error.find("Projection missed")!=std::string::npos&&canonical(d.copy_scene())==before_bake,"Strict bake still refuses real unprojected detail without changing any source or target");
  bake.allow_partial=true;d.bake_from_object(source,bake);ready(d);auto baked=d.copy_scene();
  check(baked.active().painted->baked()&&source_record(baked,source)==retained,"Original imported painted source bakes onto new topology without changing the retained source");
  auto bake_endpoint=canonical(baked);PaintSettings ink;ink.color={.05f,.8f,.1f,1};ink.front_only=false;d.paint_settings(ink);
  check(d.begin_desktop(),"Native painting starts on the newly baked target");Brush brush;auto p=ob::transform(placement,{0,.15f,0});brush.from=brush.to={p.x,p.y,p.z};brush.radius=.2f;brush.pressure=1;d.stamp_desktop(brush);d.end_desktop();ready(d);
  check(canonical(d.copy_scene())!=bake_endpoint&&source_record(d.copy_scene(),source)==retained,"Painting edits the baked target and keeps original imported textures exact");
  auto paint_endpoint=canonical(d.copy_scene());d.begin_uv();check(d.wait_uv(),"Native UV topology initializes after procedural bake and painting");check(d.frame().state->uv->valid,"Generated atlas is valid before manual UV edits");
  // Select an independently padded chart. Shrinking an arbitrary concave
  // island can legitimately collide with an island nested in its empty area,
  // or collapse an already sub-ULP sliver; validation must keep refusing those.
  auto uv_view=d.frame().state->uv;auto island=std::find_if(uv_view->topology->islands.begin(),uv_view->topology->islands.end(),[](const auto &faces){return faces.size()==1;});
  check(island!=uv_view->topology->islands.end(),"Generated slivers remain editable independent charts with real face selection");
  auto face=island->front();std::array<float,2> cursor{};for(size_t k=0;k<3;++k){auto uv=uv_view->mesh->uv[uv_view->mesh->indices[face*3+k]];cursor[0]+=uv[0]/3;cursor[1]+=uv[1]/3;}
  d.edit_uv(UvAction::Pick,cursor[0],cursor[1]);d.edit_uv(UvAction::Scale,.9f);check(d.wait_uv(),"UV scale validation completes");
  if(!d.frame().state->uv->valid)throw std::runtime_error("Scaled UV validation: "+d.frame().state->uv->error);
  check(d.frame().state->uv->valid,"Selected padded chart scales under unchanged strict overlap tolerance");d.end_uv(true);check(d.wait_uv(),"Accepted UV transfer completes");ready(d);
  auto uv_endpoint=canonical(d.copy_scene());check(uv_endpoint!=paint_endpoint&&source_record(d.copy_scene(),source)==retained,"UV editing transfers editable paint and baked data while retaining exact original source");
  check(d.undo()&&canonical(d.copy_scene())==paint_endpoint,"UV transfer is one exact global Undo");ready(d);check(d.redo()&&canonical(d.copy_scene())==uv_endpoint,"UV transfer Redo restores its exact endpoint");ready(d);
  d.save(project);Document reopened;reopened.open(project);ready(reopened);check(canonical(reopened.copy_scene())==uv_endpoint,"Native restart restores the entire imported/remeshed/detailed/baked/painted/UV scene");
  reopened.export_asset(output).get();ImportLimits limits;limits.mode=ImportMode::PreserveGltf;auto parsed=import_mesh(output,limits);
  check(!parsed.primitives.empty()&&!parsed.images.empty()&&!parsed.materials.empty(),"Background glTF export interoperably reimports final geometry, materials and embedded textures");
  check(bytes(input)==original_bytes,"Complete authoring journey leaves source file bytes unchanged");
  check(d.undo()&&d.undo()&&d.undo()&&d.undo()&&d.undo(),"Independent accepted authoring operations remain independently undoable");ready(d);check(canonical(d.copy_scene())==remesh_endpoint,"Undoing all five target-authoring operations restores exact remesh endpoint");check(d.undo()&&canonical(d.copy_scene())==import_endpoint,"Undoing voxel conversion restores exact imported scene");ready(d);check(d.undo()&&canonical(d.copy_scene())==initial_endpoint,"Final import Undo restores initial document exactly");ready(d);
  std::cout<<checks<<" complete native authoring-journey checks passed; artifacts="<<directory<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
