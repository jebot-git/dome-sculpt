// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/mesh-authoring.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
void check(bool ok, const char *message) {
  if (!ok) throw std::runtime_error(message);
  std::cout << "PASS " << message << '\n';
}
Scene source_scene() {
  Mesh m;
  constexpr float r = .1f;
  m.positions = {{-r,-r,-r},{r,-r,-r},{r,r,-r},{-r,r,-r},
                 {-r,-r,r},{r,-r,r},{r,r,r},{-r,r,r}};
  for (auto p : m.positions) m.normals.push_back(normalized(p));
  m.indices = {0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,
               1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};
  unwrap(m, 128);
  Scene scene;
  auto &o = scene.active();
  o.name = "Painted source";
  o.transform.position = {1,2,3};
  o.transform.scale = 2;
  o.material.color = {.2f,.3f,.8f,1};
  o.volume->add_layer("Source details");
  o.painted = std::make_shared<PaintedMesh>(m,128);
  o.mesh_active = true;
  PaintSettings ink;
  ink.color = {.05f,.1f,.9f,.7f};
  ink.front_only = false;
  Brush b; b.from = b.to = {0,0,0}; b.radius = .5f;
  o.painted->begin_stroke(); o.painted->stamp(b,ink); o.painted->end_stroke();
  o.painted->add_layer("Hidden mask");
  ink.channel = PaintChannel::Mask; ink.mask = .2f;
  o.painted->begin_stroke(); o.painted->stamp(b,ink); o.painted->end_stroke();
  o.painted->layer_properties("Hidden mask",false,.35f);
  DeformationState rig(m);
  rig.skeleton = Skeleton{{{"root",-1,{{0,0,0},{},{1,1,1}},.2f}}};
  rig.skin = SkinWeights{};
  rig.skin->vertices.assign(m.positions.size(),{{0,1}});
  rig.pose = {rig.skeleton->bones[0].rest};
  rig.pose[0].translation.x = .15f;
  scene.set_deformation(o.id, std::move(rig));
  return scene;
}
struct Driver {
  Document &doc; SpatialFrame f;
  Driver(Document &d):doc(d) {
    f.focused=f.head_valid=true; f.head.position={0,1.6f,0};
    f.hands[1].tracked.valid=true; f.hands[1].tracked.grip.position={0,1.48f,-.5f}; send();
  }
  void send(float x=0,float y=0,bool click=false,float pressure=0) {
    ++f.sequence; f.time+=.05; auto &h=f.hands[1];
    h.x=x;h.y=y;h.joystick=click;h.tracked.trigger=pressure;
    doc.submit(f); if(!doc.wait_input())throw std::runtime_error("Input timeout");
  }
  void sector(int i) {float a=i*3.14159265358979323846f/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}
  void open() {send();send(0,0,true);send();sector(8);sector(9);sector(1);}
  void size(float mm) {
    auto s=doc.frame().state;auto c=s->controls;
    float t=std::log(mm/c.minimum)/std::log(c.maximum/c.minimum);
    float u=(100+420*t)/640.f,v=300.f/640;
    auto &h=f.hands[1];h.tracked.aimValid=true;
    h.tracked.aim.position=ob::transform(s->panel,{(u-.5f)*.6f,(.5f-v)*.6f,.25f});
    h.tracked.aim.orientation=s->panel.orientation;
    send(0,0,false,.8f);send();
  }
  void confirm() {send(0,0,true);send();}
};
}
int main() try {
  auto scene=source_scene();auto original=scene.encode();auto id=scene.active().id;
  Document doc;doc.replace(scene);
  check(doc.frame().state->can_voxel_remesh,"A painted posed source exposes voxel remeshing");
  doc.begin_voxel_remesh();doc.edit_voxel_size(50);
  check(!doc.begin_desktop() && doc.copy_scene().encode()==original && !doc.frame().state->dirty,
        "Voxel size previews own input without changing the scene or history");
  doc.end_voxel_remesh(false);
  check(doc.frame().state->voxel_size_mm==30 && doc.copy_scene().encode()==original,
        "Cancel restores the previous size and exact source");
  doc.remesh_to_sculpt(50);
  check(doc.wait_asset() && doc.frame().state->error.empty(),"Native voxel remeshing completes on the asset worker");
  auto after=doc.copy_scene();auto encoded_after=after.encode();
  check(after.objects().size()==2 && after.active().id!=id && !after.object(id).visible &&
        !after.active().mesh_active && !after.active().deformation,
        "Remeshing creates a separate editable sculpt and retains a hidden source");
  check(after.object(id).painted->encode()==scene.object(id).painted->encode() &&
        after.object(id).deformation->encode(scene.object(id).painted->mesh())==
          scene.object(id).deformation->encode(scene.object(id).painted->mesh()) &&
        after.object(id).volume->encode()==scene.object(id).volume->encode(),
        "All original paint layers, rig, pose and sculpt layers remain exact");
  check(std::abs(after.active().volume->voxel_size()-.025f)<1e-7f &&
        length(after.active().transform.position-scene.active().transform.position)<1e-6f &&
        after.active().transform.scale==2 && after.active().material.color==scene.active().material.color,
        "World-millimetre size respects object scale and preserves transform/material factors");
  check(after.active().volume->sample({.2f,0,0})<0 && after.active().volume->sample({-.1f,0,0})>0,
        "The new sculpt samples the displayed posed shape rather than the rest mesh");
  check(doc.undo() && doc.copy_scene().encode()==original && !doc.frame().state->dirty,
        "One undo restores source visibility, selection and the exact original scene");
  check(doc.redo() && doc.copy_scene().encode()==encoded_after,"Redo restores the exact voxel field and object identity");
  check(doc.begin_desktop(),"The resulting scalar field accepts sculpt input immediately");
  Brush brush;brush.tool=Tool::Carve;brush.from=brush.to={1.3f,2,3.2f};brush.radius=.1f;
  doc.stamp_desktop(brush);doc.end_desktop();
  check(doc.copy_volume().voxels()!=after.active().volume->voxels(),"The remeshed result supports a real new sculpt stroke");
  check(doc.undo() && doc.copy_scene().encode()==encoded_after,"Undoing that stroke retains the remesh transaction");
  doc.replace(scene);Driver driver(doc);driver.open();
  check(doc.frame().state->controls.parameter==VrParameter::VoxelSize,
        "Joystick Objects/Mesh tools opens the continuous voxel-size panel");
  driver.send(1);float coarse=doc.frame().state->voxel_size_mm;
  driver.send(0,1);float fine=doc.frame().state->voxel_size_mm;
  check(coarse>30 && fine>coarse && fine-coarse<coarse-30,
        "Horizontal stick and vertical fine wheel adjust the logarithmic size continuously");
  driver.f.focused=false;driver.send();driver.f.focused=true;driver.send();
  check(doc.copy_scene().encode()==original && doc.frame().state->voxel_size_mm==30,
        "Focus loss cancels controller options without creating geometry");
  driver.open();driver.size(50);driver.confirm();
  check(doc.wait_asset() && doc.frame().state->error.empty() && doc.copy_scene().objects().size()==2,
        "An XR aim slider and joystick confirmation execute the native remesh operation");
  check(doc.undo() && doc.copy_scene().encode()==original,"Controller-created remesh is one exact scene undo");
  auto reject=[&](Scene source,float mm,const char *message) {
    auto before=source.encode();doc.replace(source);doc.remesh_to_sculpt(mm);
    check(doc.wait_asset() && !doc.frame().state->error.empty() &&
          doc.copy_scene().encode()==before && !doc.frame().state->dirty && !doc.frame().state->can_undo,message);
  };
  reject(scene,1000,"A size too coarse to capture the shape fails without hiding or deleting the source");
  reject(scene,.1f,"Excessive voxel grids fail without partial geometry or history");
  auto open=scene;auto plane=open.active().painted->mesh();plane.indices.resize(3);
  open.active().deformation.reset();open.active().painted=std::make_shared<PaintedMesh>(plane,128);
  reject(open,50,"Open source surfaces fail with an intact editable source");
  auto cancelled=scene;bool stopped=false;
  try {create_voxel_sculpt(cancelled,50,[]{return true;});}catch(const std::exception&){stopped=true;}
  check(stopped && cancelled.encode()==original,"Cancellation never publishes a partial remesh");
  doc.replace(scene);auto s=doc.frame().state;AssetSource stale{s->generation,s->revision,s->active_object};
  doc.replace(scene);bool refused=false;
  try {doc.remesh_to_sculpt(50,stale);}catch(const std::logic_error&){refused=true;}
  check(refused && !doc.frame().state->asset_busy && doc.copy_scene().encode()==original,
        "Reused source object IDs cannot bypass generation guards");
  return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
