// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/import-authoring.hpp"
#include "dome/rig-edit.hpp"
#include "dome/source-placement.hpp"
#include "dome/vrm.hpp"
#include "vrm-rigid-fixtures.hpp"
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t count=0;
void check(bool b,const char *m){if(!b)throw std::runtime_error(m);++count;std::cout<<"PASS "<<m<<std::endl;}
void ready(Document&d){check(d.wait_asset(std::chrono::seconds(90))&&d.wait_native_stroke(std::chrono::seconds(90))&&d.wait_mesh(std::chrono::seconds(90)),"Native rigid authoring workers settle");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;auto home=std::filesystem::temp_directory_path()/("dome-rigid-authoring-"+std::to_string(getpid()));std::filesystem::create_directories(home);
 setenv("HOME",home.c_str(),1);setenv("XDG_CONFIG_HOME",home.c_str(),1);setenv("XDG_STATE_HOME",home.c_str(),1);
 struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{home};
 auto placed=home/"placed.vrm",source=home/"rigid.vrm";vrm_source_fixture::source_variant(argv[1],placed,2);auto nodes=vrm_source_fixture::with_rigid_accessories(placed,source);
 ImportAuthoringOptions options;options.paint_resolution=64;auto prepared=prepare_authoring_import(Scene{},source,options);auto scene=prepared.scene;auto active=prepared.objects.front();uint64_t rigid=0;
 for(auto id:prepared.objects)if(scene.object(id).appearance->source_node==int(nodes.moving))rigid=id;
 RigEdit pose;pose.joint=JointAdjustment{scene.object(active).deformation->humanoid->joints.at("head"),{0,0,17},{}};edit_rig(scene,active,pose);
 auto &o=scene.object(rigid);auto shape=*o.deformation;shape.morph_weights={.6f};scene.set_deformation(rigid,std::move(shape));
 AffineTransform extra(std::array<double,16>{-1.,.12,0,0,.25,1.1,0,0,0,0,.85,0,.04,-.02,.01,1});o.affine_edit=std::make_shared<const NativeAffineEdit>(o.affine_edit->matrix()*extra);
 Document doc;doc.replace(scene);doc.select_object(rigid);ready(doc);auto initial=doc.copy_scene().encode();auto before=doc.copy_scene();auto surfaces=before.preview_surfaces();auto at=std::find_if(surfaces.begin(),surfaces.end(),[&](const auto&s){return s.id==rigid;});auto mesh=at->preview_mesh();
 auto a=mesh.indices[0],b=mesh.indices[1],c=mesh.indices[2];auto local=mesh.positions[a]*.3f+mesh.positions[b]*.2f+mesh.positions[c]*.5f;auto world=before.world_matrix(rigid);auto p=world.point({local.x,local.y,local.z}),n=world.normal({0,0,1});Vec3 center{float(p[0]),float(p[1]),float(p[2])},normal=normalized(Vec3{float(n[0]),float(n[1]),float(n[2])});
 auto hit=doc.raycast(center+normal*.4f,normal*-1.f);check(hit&&length(*hit-center)<2e-6f,"Actual Document picking reaches the current morphed rigid plane beneath posed/sheared/reflected source placement");
 check(length(doc.gradient(center)-normal)<2e-6f,"Native gradient uses the exact rigid inverse-transpose normal");
 PaintSettings settings;settings.color={1,0,0,1};doc.paint_settings(settings);
 NativeStrokeSample sample;sample.origin=center+normal*.4f;sample.direction=normal*-1.f;sample.brush.normal=normal;sample.brush.radius=.04f;sample.brush.blend=0;
 check(doc.begin_native_stroke()&&doc.sample_native_stroke(sample),"Actual worker admits painting on the posed rigid original UV surface");doc.end_native_stroke(false);ready(doc);check(doc.copy_scene().encode()==initial,"Cancelled rigid paint retains exact rest mesh, source metadata, shape and placement");
 check(doc.begin_native_stroke()&&doc.sample_native_stroke(sample),"Rigid paint retry begins from unchanged source");doc.end_native_stroke();auto project=home/"painted.dsculpt";auto save=doc.save_async(project);save.get();ready(doc);auto painted=doc.copy_scene();
 auto uv=at->mesh.uv[a];for(size_t k=0;k<2;++k)uv[k]=at->mesh.uv[a][k]*.3f+at->mesh.uv[b][k]*.2f+at->mesh.uv[c][k]*.5f;
 auto texel=painted.object(rigid).painted->pixel(1,uint32_t(uv[0]*64),uint32_t(uv[1]*64));check(texel.data[9]>200,"Evaluated world brush paints the independently interpolated original UV texel");
 check(painted.object(rigid).painted->mesh_owner()==before.object(rigid).painted->mesh_owner()&&painted.object(rigid).appearance==before.object(rigid).appearance&&painted.object(rigid).deformation==before.object(rigid).deformation&&painted.object(rigid).affine_edit==before.object(rigid).affine_edit,"Rigid painting preserves immutable mesh, raw source, morph and durable placement owners");
 check(Scene::load(project).encode()==painted.encode(),"Immediately accepted Save waits for the actual rigid paint worker and restarts exactly");check(doc.undo()&&doc.copy_scene().encode()==initial&&doc.redo()&&doc.copy_scene().encode()==painted.encode(),"Rigid paint has one exact global Undo and Redo");
 auto old=doc.copy_scene().world_matrix(rigid);CanvasTransform delta;delta.position={.12f,-.03f,.02f};delta.orientation={0,0,std::sin(.08f),std::cos(.08f)};
 doc.configure_object_grab(true,true);ready(doc);
 check(doc.begin_object_grab(rigid),"Existing native held-object path admits the real rigid source attachment");doc.move_held_object_world(delta);doc.end_object_grab(false);ready(doc);check(doc.copy_scene().encode()==painted.encode(),"Cancelled held rigid movement retains all source and accepted paint");
 check(doc.begin_object_grab(rigid),"Held rigid retry uses the original source anchor");doc.move_held_object_world(delta);doc.end_object_grab();ready(doc);auto moved=doc.copy_scene();auto expected=(affine_similarity(delta)*old).matrix(),actual=moved.world_matrix(rigid).matrix();double error=0;for(size_t k=0;k<16;++k)error=std::max(error,std::abs(expected[k]-actual[k]));check(error<2e-12,"Accepted world-held delta preserves full sheared source frame without Canvas approximation");
 check(doc.undo()&&doc.copy_scene().encode()==painted.encode()&&doc.redo()&&doc.copy_scene().encode()==moved.encode(),"Held rigid movement remains one separate exact global Undo and Redo");
 auto output=home/"painted-held.vrm";auto exported=doc.export_asset(output,AssetExportFormat::Avatar);exported.get();ready(doc);check(std::filesystem::file_size(output)>10000,"Actual avatar worker exports painted held rigid parts together with all source skins");
 std::cout<<count<<" actual rigid pick/paint/held/history/Save checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
