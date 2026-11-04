// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NATIVE_PAINT32_RENDER_NO_MAIN
#include "native-paint32-render.cpp"
#undef DOME_NATIVE_PAINT32_RENDER_NO_MAIN
#include "emission-paint-access.hpp"
#include "dome/native-authoring.hpp"
#include <fstream>
namespace {
Scene skinned_native32(bool posed) {
 auto source=native_grid(Material::AlphaMode::Opaque);
 source.active().painted=detail::EmissionExportAccess::without_emission(*source.active().painted);
 source.active().material.emissive={};
 // Reorder the UV-edited vertices. The exporter must actually remap skin
 // arrays, rather than pass solely because both layouts happen to be identity.
 const auto &m=source.active().painted->mesh();Mesh target;std::vector<uint32_t> map;
 const std::array<size_t,6> order{3,4,5,0,1,2};
 for(auto c:order){auto v=m.indices[c];map.push_back(v);target.positions.push_back(m.positions[v]);target.normals.push_back(m.normals[v]);auto uv=m.uv[v];target.uv.push_back({.5f-.7f*(uv[1]-.5f),.5f+.7f*(uv[0]-.5f)});}
 target.indices={3,4,5,0,1,2};
 auto retained=detail::retain_native_paint(source.active(),target,map);auto result=source;
 result.active().painted=retained.paint;result.active().material_bindings=detail::bind_native_paint(retained.owner,{});
 DeformationState d(target);d.skeleton=Skeleton{{{"root",-1,{},.4f},{"tip",0,{{0,.3f,0}},.2f}}};
 d.skin=SkinWeights{};
 for(auto p:target.positions){float w=std::clamp(p.y+.5f,.1f,.9f);d.skin->vertices.push_back({{0,1-w},{1,w}});}
 d.pose={d.skeleton->bones[0].rest,d.skeleton->bones[1].rest};
 if(posed){d.pose[0].translation={.08f,.015f,0};d.pose[1].rotation={.98472655f,0,0,.17410814f};}
 result.set_deformation(result.active().id,d);result.validate();return result;
}
Vec3 parsed_point(const float *m,Vec3 p){return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};}
std::vector<uint8_t> bytes(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
}
int main(int argc,char **argv)try{
 if(argc<2||argc>3)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 if(argc==3&&std::string_view(argv[2])!="--expect-refusal"&&std::string_view(argv[2])!="--hardware")return 2;
 const bool negative=argc==3&&std::string_view(argv[2])=="--expect-refusal";
 NativeGraphics graphics;const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<std::endl;
 const bool hardware=device.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos&&device.find("llvmpipe")==std::string::npos;
 check(argc<3||std::string_view(argv[2])!="--hardware"||hardware,"Strict native paint skin hardware consumer requires actual Intel Arc A770");
 Fbo fbo;SurfaceRenderer renderer;prepared_normal_consumer::Renderer independent;
 for(bool posed:{false,true}){
  auto scene=skinned_native32(posed);const auto before=scene.encode();Document d;d.replace(scene);
  const auto path=out/(posed?"posed.glb":"rest.glb");
  if(negative){std::string reason;try{d.export_asset(path).get();}catch(const std::exception &e){reason=e.what();}std::cout<<"ORIGINAL_NEGATIVE "<<reason<<'\n';check(reason.find("Native paint export requires")!=std::string::npos,"Old actual public export refuses genuine native32 skin");check(scene.encode()==before&&d.copy_scene().encode()==before,"Original refusal preserves real native32/skin source");continue;}
  d.shading(false);settled(d);fbo.bind();const auto native_pixels=draw(renderer,d.frame().surface);
  d.export_asset(path).get();check(d.copy_scene().encode()==before,"Actual public skin export retains every original native paint and bind byte");
  prepared_normal_consumer::Model model(path);auto *raw=model.data;check(cgltf_validate(raw)==cgltf_result_success,"Independent emitted skin/accessor hierarchy validation");
  const auto expected=scene.active().deformation->preview(scene.active().painted->mesh());const auto &original=scene.active().painted->mesh();auto coordinates=scene.active().painted->paint_sampling_domain()->corner_coordinates(original);size_t corners=0;float maximum=0,wrong_maximum=0;
  for(size_t n=0;n<raw->nodes_count;++n){const auto &node=raw->nodes[n];if(!node.mesh)continue;check(node.skin&&node.skin->inverse_bind_matrices,"Actual native paint export retains real skin/inverse binds");
   for(size_t part=0;part<node.mesh->primitives_count;++part){const auto &p=node.mesh->primitives[part];auto positions=cgltf_find_accessor(&p,cgltf_attribute_type_position,0);auto joints=cgltf_find_accessor(&p,cgltf_attribute_type_joints,0);auto weights=cgltf_find_accessor(&p,cgltf_attribute_type_weights,0);auto paint=cgltf_find_accessor(&p,cgltf_attribute_type_texcoord,1);check(positions&&joints&&weights&&paint&&p.indices,"Actual emitted geometry/skin/paint coordinate arrays exist");
    for(size_t c=0;c<p.indices->count;++c){auto v=cgltf_accessor_read_index(p.indices,c);float pos[3]{},uv[2]{},weight[4]{};unsigned joint[4]{};check(cgltf_accessor_read_float(positions,v,pos,3)&&cgltf_accessor_read_float(paint,v,uv,2)&&cgltf_accessor_read_uint(joints,v,joint,4)&&cgltf_accessor_read_float(weights,v,weight,4),"Independent actual corner accessor reads");Vec3 actual{},wrong{};
     for(size_t k=0;k<4;++k)if(weight[k]){check(joint[k]<node.skin->joints_count,"Emitted joint indices are actual skin members");float bind[16]{},world[16]{};check(cgltf_accessor_read_float(node.skin->inverse_bind_matrices,joint[k],bind,16),"Independent authored inverse bind read");cgltf_node_transform_world(node.skin->joints[joint[k]],world);auto p=Vec3{pos[0],pos[1],pos[2]};actual+=parsed_point(world,parsed_point(bind,p))*weight[k];wrong+=parsed_point(world,p)*weight[k];}
     auto source=original.indices.at(c);auto error=length(actual-expected.positions[source]);maximum=std::max(maximum,error);wrong_maximum=std::max(wrong_maximum,length(wrong-expected.positions[source]));check(error<3e-5f,"Independent joint/IBM products reproduce exact native posed corner positions");check(std::array<float,2>{uv[0],uv[1]}==coordinates.at(c),"Independent paint coordinates follow the same reordered corner as skin weights");++corners;
    }
    check(!p.extras.data||std::string_view(p.extras.data).find("domeSculptNormalTransport")==std::string_view::npos,"Coordinate-only skin export never fabricates artist normal receipts");
   }
  }
  check(corners==6&&wrong_maximum>.01f,"All corners checked and missing-IBM negative is sensitive");
  fbo.bind();independent.render(model.data,nullptr);const auto portable_pixels=read();const auto image_error=delta(native_pixels,portable_pixels);check(image_error<=4,"Independent actual skinned native paint pixels meet unchanged4 bound");
  size_t visible=0;std::set<std::array<uint8_t,4>> colours;for(size_t p=0;p<native_pixels.size();p+=4)if(native_pixels[p+3]){++visible;colours.insert({native_pixels[p],native_pixels[p+1],native_pixels[p+2],native_pixels[p+3]});}check(visible>1000&&colours.size()>8,"Actual skin render has substantial varied visible paint coverage");
  joint::capture(out/(posed?"posed-native.png":"rest-native.png"),native_pixels);joint::capture(out/(posed?"posed-portable.png":"rest-portable.png"),portable_pixels);
  auto editable=out/(posed?"posed-editable.glb":"rest-editable.glb");d.export_asset(editable,AssetExportFormat::EditableGlb256).get();auto recovered=extract_native_authoring(bytes(editable));check(recovered&&recovered->encode()==before,"Actual editable GLB recovers exact original skin/native paint graph");
  auto native=out/(posed?"posed.dsculpt":"rest.dsculpt");d.save_async(native).get();check(Scene::load(native).encode()==before,"Following Save/reopen preserves exact native paint and deformation owners");
  std::cout<<"NATIVE32_SKIN posed="<<posed<<" corners="<<corners<<" max="<<maximum<<" image_error="<<image_error<<" wrong_missing_ibm="<<wrong_maximum<<'\n';
 }
 if(!negative){auto scene=skinned_native32(false);auto state=*scene.active().deformation;const auto &mesh=scene.active().painted->mesh();state.morphs=MorphSet(mesh);auto positions=mesh.positions;positions[0].z+=.1f;state.morphs->set_target(mesh,"Actual curved key",positions);state.morph_weights={.35f};scene.set_deformation(scene.active().id,std::move(state));const auto before=scene.encode();Document d;d.replace(scene);auto dir=out/"refusal";std::filesystem::create_directories(dir);auto file=dir/"morph.glb";{std::ofstream f(file);f<<"prior output";}std::string reason;try{d.export_asset(file).get();}catch(const std::exception &e){reason=e.what();}check(reason.find("complete shading adapter")!=std::string::npos,"Actual curved native paint morph retains its genuine unadapted shading refusal");const auto sentinel=bytes(file);check(std::string(sentinel.begin(),sentinel.end())=="prior output"&&d.copy_scene().encode()==before,"Morph refusal preserves exact prior output and complete skin/morph/paint graph");auto save=dir/"following.dsculpt";d.save_async(save).get();check(Scene::load(save).encode()==before,"Following native Save succeeds and retains all unsupported-but-editable morph data");}
 std::cout<<"PASS "<<checks<<" native paint skin export checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
