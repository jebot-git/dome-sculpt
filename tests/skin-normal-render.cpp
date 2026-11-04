// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_native_material_fixture
#include "mtoon-render.cpp"
#undef main
#include "prepared-normal-consumer.hpp"
#include "dome/document.hpp"
#include "dome/scene-file.hpp"
#include <cstring>
namespace {
Scene authored_skin(int variant){Mesh m;m.positions={{-.1f,-.1f,0},{.1f,-.1f,.015f},{.1f,.1f,-.01f},{-.1f,.1f,.01f}};m.normals={normalized(Vec3{.1f,.1f,1}),normalized(Vec3{-.6f,.2f,1}),normalized(Vec3{.3f,.7f,1}),normalized(Vec3{.6f,-.4f,1})};m.indices={0,1,2,0,2,3};m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};Scene scene;scene.active().mesh_active=true;scene.active().transform.scale=5;scene.active().painted=std::make_shared<PaintedMesh>(m,64);DeformationState d(m);d.skeleton=Skeleton{{{"root",-1,{},.2f},{"tip",0,{},.1f}}};d.skeleton->bones[1].rest.translation={0,.04f,0};d.skin=SkinWeights{};for(size_t v=0;v<4;++v)d.skin->vertices.push_back(v%2?VertexWeights{{1,1}}:VertexWeights{{0,.7f},{1,.3f}});for(auto b:d.skeleton->bones)d.pose.push_back(b.rest);
 if(variant==1)d.pose[0].rotation={std::cos(.2f),0,0,std::sin(.2f)};
 if(variant==2){d.pose[0].rotation={std::cos(.15f),std::sin(.15f),0,0};d.pose[1].rotation={std::cos(-.25f),0,std::sin(-.25f),0};}
 if(variant==3){d.pose[0].scale={1.4f,.75f,1.7f};d.pose[1].scale={.8f,1.3f,.7f};}
 if(variant==4)d.pose[1].full_local_matrix=RigMatrix{1,0,0,0,.35f,1,0,0,.1f,.2f,1,0,0,.04f,0,1};
 if(variant==5){d.skeleton->inverse_bind_override=d.skeleton->inverse_bind();d.pose[1].rotation={std::cos(.2f),std::sin(.2f),0,0};}
 scene.set_deformation(1,d);return scene;}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);NativeGraphics graphics;Fbo fbo;SurfaceRenderer native;native.set_frame_time(0);prepared_normal_consumer::Renderer consumer;
 for(int variant=0;variant<6;++variant){auto initial=authored_skin(variant);auto before=initial.encode();Document document;document.replace(initial.snapshot());document.shading(false);check(document.wait_asset()&&document.wait_mesh()&&document.frame().state->error.empty(),"Native authored-normal skin publishes immutable posed geometry");const auto file=out/("skin-normal-"+std::to_string(variant)+".glb");document.export_asset(file,AssetExportFormat::EditableGlb256).get();prepared_normal_consumer::Model model(file);fbo.bind();auto expected=draw(native,document.frame().surface);fbo.bind();consumer.render(model.data);auto actual=read();size_t visible=0;for(size_t i=3;i<actual.size();i+=4)visible+=actual[i]&&expected[i];const auto error=delta(expected,actual);std::cout<<"AUTHORED_SKIN_IMAGE variant="<<variant<<" visible="<<visible<<" max="<<error<<'\n';check(visible>500,"Independent GLB skin consumer and native view draw the genuine curved model");check(error<=4,"Authored joint normals match independent standard GLB rendering at unchanged four-level bound");check(document.copy_scene().encode()==before&&load_scene_file(file).scene.encode()==before,"Real native export preserves exact rig, skin, authored normals and source scene through recovery");
  if(variant==0){auto &p=model.data->meshes[0].primitives[0];auto a=prepared_normal_consumer::attr(p.attributes,p.attributes_count,cgltf_attribute_type_normal);check(a&&a->component_type==cgltf_component_type_r_32f,"Sensitivity probe uses actual emitted float normals");for(size_t v=0;v<a->count;++v){auto *destination=static_cast<uint8_t*>(a->buffer_view->buffer->data)+a->buffer_view->offset+a->offset+v*a->stride;const float replacement[3]{0,0,1};std::memcpy(destination,replacement,sizeof(replacement));}fbo.bind();consumer.render(model.data);check(delta(actual,read())>4,"Replacing authored normals with flat normals changes independently rendered pixels beyond the fixed bound");}
 }
 std::cout<<"PASS "<<checks<<" authored skin-normal image checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
