// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NATIVE_KEY_EXPORT_NO_MAIN
#include "native-paint-key-export-render.cpp"
#undef DOME_NATIVE_KEY_EXPORT_NO_MAIN
#include "dome/emission-native.hpp"
#include "emission-export.hpp"
#include "native-paint-frame-source.hpp"
#include <bit>
namespace {
Scene paint_physical_key(Scene scene){
 scene.active().material.emissive={2.4f,.015f,.8f};Document doc;doc.replace(scene);finish(doc);
 const auto frame=doc.frame().surface;const auto &range=frame->ranges.front();
 const auto a=frame->mesh.positions.at(frame->mesh.indices.at(range.first)),b=frame->mesh.positions.at(frame->mesh.indices.at(range.first+1)),c=frame->mesh.positions.at(frame->mesh.indices.at(range.first+2));
 const auto p=a*.3f+b*.3f+c*.4f,n=normalized(cross(b-a,c-a));
 PaintSettings ink;ink.front_only=false;ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={8,.016f,.16f};doc.paint_settings(ink);
 NativeStrokeSample sample;sample.origin=p+n;sample.direction=n*-1;sample.brush.radius=.055f;sample.brush.strength=.7f;sample.brush.blend=.4f;
 check(doc.begin_native_stroke()&&doc.sample_native_stroke(sample),"Actual physical emission gesture enters the current key authoring worker");doc.end_native_stroke();check(doc.wait_native_stroke(std::chrono::seconds(120)),"Actual keyed physical paint gesture retires");finish(doc);
 auto result=doc.copy_scene();check(result.active().painted->has_physical_emission(),"Actual current key contains genuine FLOAT emission paint");
 check(result.active().material_bindings&&result.active().material_bindings->native_paint_frames&&result.active().material_bindings->native_paint_sampling,"Current physical paint retains its distinct real key-frame and paint-domain owners");
 result.active().material_bindings->native_paint_frames->validate_binding(*result.active().material_bindings,true);return result;
}
}
int main(int argc,char **argv)try{
 if(argc<3||argc>4)return 2;const bool counter=argc==4&&std::string_view(argv[3])=="--expect-refusal",strict=argc==4&&std::string_view(argv[3])=="--hardware";if(argc==4&&!counter&&!strict)return 2;
 const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);setenv("XDG_STATE_HOME",(out/"state").c_str(),1);setenv("XDG_CONFIG_HOME",(out/"config").c_str(),1);
 NativeGraphics graphics;const std::string gpu=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<gpu<<std::endl;check(!strict||(gpu.find("Intel")!=std::string::npos&&gpu.find("Arc")!=std::string::npos&&gpu.find("A770")!=std::string::npos),"Physical key export executes on requested actual Intel Arc A770");
 const auto source=paint_physical_key(Scene::load(argv[1]));Fbo fbo;SurfaceRenderer native;prepared_normal_consumer::Renderer independent;unsigned number=0;
 for(bool posed:{false,true})for(float weight:{0.f,.37f,1.f}){
  auto scene=source;scene.active().transform.scale=5;RigEdit pose;pose.linked_pose=false;pose.morph=MorphWeightEdit{0,weight};if(posed)pose.joint=JointAdjustment{1,{0,0,25},{}};edit_rig(scene,scene.active().id,pose);const auto exact=scene.encode();Document doc;doc.replace(scene);doc.shading(false);finish(doc);
  const auto dir=out/std::to_string(number++);std::filesystem::create_directories(dir);const auto file=dir/"physical.glb";
  if(counter){const std::string prior="prior genuine keyed HDR output";{std::ofstream f(file);f<<prior;}std::string reason;try{doc.export_asset(file,AssetExportFormat::Glb).get();}catch(const std::exception&e){reason=e.what();}std::cout<<"KEY_HDR_ORIGINAL posed="<<posed<<" weight="<<weight<<" reason="<<reason<<std::endl;check(reason=="Native HDR field requires its own supported native paint owner","Actual genuine keyed FLOAT paint reaches the missing ordinary HDR frame adapter");auto current=key_file_bytes(file);check(std::string(current.begin(),current.end())==prior&&doc.copy_scene().encode()==exact,"Original keyed HDR refusal preserves prior output and exact caller Scene");doc.save_async(dir/"following.dsculpt").get();check(Scene::load(dir/"following.dsculpt").encode()==exact,"Following ordered native Save succeeds after the keyed HDR refusal");continue;}
  fbo.bind();const auto wanted=draw(native,doc.frame().surface);size_t visible=0;std::set<std::array<uint8_t,4>> colors;for(size_t i=0;i<wanted.size();i+=4)if(wanted[i+3]){++visible;colors.insert({wanted[i],wanted[i+1],wanted[i+2],wanted[i+3]});}check(visible>200&&colors.size()>8,"Actual physical key native image has nonblank varied coverage");capture(dir/"native.png",wanted);
  for(bool editable:{false,true}){
   const auto target=editable?dir/"editable.glb":file;doc.export_asset(target,editable?AssetExportFormat::EditableGlb256:AssetExportFormat::Glb).get();check(doc.copy_scene().encode()==exact,"Ordinary and editable physical-key workers retain exact native source and paint");prepared_normal_consumer::Model model(target);const auto &primitive=model.data->meshes[0].primitives[0];size_t skins=0;for(size_t n=0;n<model.data->nodes_count;++n)if(model.data->nodes[n].mesh&&model.data->nodes[n].skin&&model.data->nodes[n].skin->inverse_bind_matrices)++skins;check(primitive.targets_count==1&&field(primitive.targets[0].attributes,primitive.targets[0].attributes_count,cgltf_attribute_type_normal)&&skins==1,"Actual physical key output retains current normal target and real skin graph");
   fbo.bind();independent.render(model.data,nullptr);auto pixels=read();const auto error=delta(wanted,pixels);std::cout<<"PHYSICAL_KEY posed="<<posed<<" weight="<<weight<<" editable="<<editable<<" max="<<error<<std::endl;check(error<=4,"Independent physical key skin/normal/morph/emission consumer meets existing key bound4");capture(dir/(editable?"editable.png":"ordinary.png"),pixels);
   const auto recovered=editable?extract_native_authoring(key_file_bytes(target)):extract_native_emission(key_file_bytes(target));check(recovered&&recovered->encode()==exact,"Both physical-key containers recover exact current FLOAT paint, rig, morph and source");
   if(!editable&&!posed&&weight==0){
    auto damaged=key_file_bytes(target);auto word=[&](size_t at){uint32_t value=0;for(size_t k=0;k<4;++k)value|=uint32_t(damaged.at(at+k))<<(8*k);return value;};
    const auto &accessor=model.json.at("accessors").at(model.json.at("meshes").at(0).at("primitives").at(0).at("targets").at(0).at("NORMAL").get<size_t>());const auto &view=model.json.at("bufferViews").at(accessor.at("bufferView").get<size_t>());
    const auto offset=size_t(28)+word(12)+view.value("byteOffset",size_t(0))+accessor.value("byteOffset",size_t(0));const auto changed=std::bit_cast<uint32_t>(std::bit_cast<float>(word(offset))+.125f);for(size_t k=0;k<4;++k)damaged.at(offset+k)=uint8_t(changed>>(8*k));
    check(rejects([&]{extract_native_emission(damaged);}),"Damaged real emitted NORMAL morph bytes cannot borrow an otherwise valid native HDR package");
   }
   Document restored;restored.replace(*recovered);restored.shading(false);finish(restored);fbo.bind();check(draw(native,restored.frame().surface)==wanted,"Recovered physical key renders every native pixel exactly");
   const auto second=dir/(editable?"editable-second.glb":"ordinary-second.glb");restored.export_asset(second,editable?AssetExportFormat::EditableGlb256:AssetExportFormat::Glb).get();check(key_file_bytes(second)==key_file_bytes(target),"Physical-key second export retains exact bytes and bounded container graph");
  }
  if(!posed&&weight==0){
   const auto &original=scene.active();auto foreign=original;auto binding=std::make_shared<SourceMaterialBindings>(*original.material_bindings);binding->native_paint_frames.reset();foreign.material_bindings=binding;check(rejects([&]{detail::native_paint_emission_field(foreign,{});}),"Missing key-frame owner cannot authorize the physical morph field");
   binding=std::make_shared<SourceMaterialBindings>(*original.material_bindings);binding->native_paint_frames=std::shared_ptr<const detail::NativePaintFrameSource>(binding->native_paint_frames.get(),[](const detail::NativePaintFrameSource*){});foreign.material_bindings=binding;check(rejects([&]{detail::native_paint_emission_field(foreign,{});}),"Same-address foreign frame control block cannot borrow physical-key authority");
   foreign=original;auto changed=std::make_shared<DeformationState>(*original.deformation);changed->morphs->targets[0].delta[0].z+=.02f;foreign.deformation=changed;check(rejects([&]{detail::native_paint_emission_field(foreign,{});}),"Changed current morph cannot borrow the sealed physical-key shading source");
   check(rejects([&]{detail::native_paint_emission_field(original,[]{return true;});}),"Physical-key field cancellation is reachable before dense radiance storage");check(scene.encode()==exact,"Physical-key authority and cancellation probes preserve exact source and FLOAT data");
  }
  doc.save_async(dir/"following.dsculpt").get();check(Scene::load(dir/"following.dsculpt").encode()==exact,"Following native Save remains exact after physical-key exports");
 }
 std::cout<<"PASS "<<checks<<" actual physical key "<<(counter?"refusal":"export image")<<" controls"<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
