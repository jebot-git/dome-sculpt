// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_PAINT_SAMPLING_RENDER_NO_MAIN
#include "paint-sampling-render.cpp"
#undef DOME_PAINT_SAMPLING_RENDER_NO_MAIN
#include "dome/import-authoring.hpp"
#include "import-authoring-fixtures.hpp"
#include "paint-source-admission.hpp"
#include <fstream>
namespace {
void endpoint(Document &d){if(!d.wait_native_stroke()||!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error("Combined native endpoint: "+d.frame().state->error);}
void ink(Document &d,const Scene&s){
 const auto &o=s.active();auto geometry=o.deformation?o.deformation->preview(o.painted->mesh()):o.painted->mesh();
 const auto &indices=geometry.indices;size_t corner=0;for(size_t c=0;c<indices.size();c+=3)if(geometry.normals[indices[c]].z>.9f){corner=c;break;}
 Vec3 point{};for(size_t k=0;k<3;++k)point+=geometry.positions[indices[corner+k]]/3.f;
 auto n=normalized(geometry.normals[indices[corner]]);auto world=s.world_matrix(o.id).point({point.x,point.y,point.z});point={float(world[0]),float(world[1]),float(world[2])};
 NativeStrokeSample sample;sample.origin=point+n;sample.direction=n*-1;sample.brush.radius=.18f;sample.brush.blend=0;
 check(d.begin_native_stroke()&&d.sample_native_stroke(sample),"Actual combined native projection/paint gesture is admitted");d.end_native_stroke(true);
}
}
int main(int argc,char **argv)try{
 if(argc<2)return 2;const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);
 NativeGraphics graphics;const std::string gpu=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<gpu<<std::endl;
 check(argc<3||std::string_view(argv[2])!="--hardware"||(gpu.find("Intel")!=std::string::npos&&gpu.find("Arc")!=std::string::npos&&gpu.find("A770")!=std::string::npos),"Combined fixture uses its requested actual graphics device");
 const auto image=Image{1,1,{255,0,0,255}}.png();const auto image_path=root/"projection.png";{std::ofstream out(image_path,std::ios::binary);out.write(reinterpret_cast<const char*>(image.data()),image.size());}
 Fbo fbo;fbo.bind();SurfaceRenderer renderer;prepared_normal_consumer::Renderer independent;
 auto original=joint::prepare(true,Material::AlphaMode::Opaque);auto retained=retain_writable(original);
 auto unsupported=retained.encode();unsupported["version"]=26;unsupported["objects"][0]["rig_source"]=0;
 check(rejects([&]{Scene::decode(unsupported);}),"Outer Scene26 cannot silently discard an unsupported actual Source25 rig capsule");
 unsupported=retained.encode();unsupported["objects"][0]["rig_source"]=nullptr;
 check(Scene::decode(unsupported).encode()==retained.encode(),"Null future rig capsule field does not fabricate a Source25 feature");
 unsupported=retained.encode();check(!unsupported["polygon_sources"]["origins"].empty(),"Retained-capsule negative uses an actual owning polygon-origin entry");unsupported["polygon_sources"]["origins"][0]["rig_source"]=0;
 check(rejects([&]{Scene::decode(unsupported);}),"Retained original graph cannot smuggle an unsupported actual Source25 rig capsule");
 auto deform=*retained.active().deformation;if(deform.pose.empty())deform.pose.resize(deform.skeleton->bones.size());auto encoded_deform=deform.encode(retained.active().painted->mesh());encoded_deform["pose"][0]["full_local_matrix"]=RigMatrix{};
 check(rejects([&]{DeformationState::decode(encoded_deform,retained.active().painted->mesh());}),"Bare and recursively retained Deformation1 cannot ignore an exact-pose matrix marker");
 Document document;document.replace(retained);endpoint(document);document.shading(false);endpoint(document);
 ProjectionCamera retained_camera;retained_camera.pose.position={0,0,2};retained_camera.width=retained_camera.height=2;
 document.load_projection_image(image_path,retained_camera);check(document.wait_projection_image(),"Retained paint-domain project loads the actual native projector");
 auto retained_before=document.copy_scene();ink(document,retained_before);const auto retained_file=root/"retained-projected.dsculpt";document.save_async(retained_file).get();endpoint(document);auto retained_after=document.copy_scene();
 check(retained_after.active().painted->preview(retained_after.active().material)->color.rgba!=retained_before.active().painted->preview(retained_before.active().material)->color.rgba,"Known opaque sampled surface produces actual projected pixels after retained UV editing");
 check(retained_after.active().painted->normal_projection()==retained_before.active().painted->normal_projection()&&retained_after.active().painted->baked()->normal_authority==retained_before.active().painted->baked()->normal_authority,"Actual post-UV projection preserves independently captured normal field and event authority");
 check(Scene::load(retained_file).encode()==retained_after.encode()&&document.undo()&&document.copy_scene().encode()==retained_before.encode()&&document.redo()&&document.copy_scene().encode()==retained_after.encode(),"Retained-domain projected paint has exact native Save/reopen and one Undo/Redo");endpoint(document);retained=document.copy_scene();
 const auto current_file=root/"current-paint-normal.glb";document.export_asset(current_file).get();prepared_normal_consumer::Model current_glb(current_file);
 check(prepared_normal_consumer::attr(current_glb.data->meshes[0].primitives[0].attributes,current_glb.data->meshes[0].primitives[0].attributes_count,cgltf_attribute_type_texcoord,1)!=nullptr,"Current paint/normal compiler emits its actual retained coordinate accessor without inventing a redundant UV2");
 auto retained_pixels=draw(renderer,document.frame().surface);fbo.bind();independent.render(current_glb.data);auto retained_error=delta(retained_pixels,read());std::cout<<"RETAINED_POST_PROJECTION_GLB_MAX "<<retained_error<<std::endl;
 check(retained_error<=3,"New native post-UV projection on retained paint domain matches independent current GLB pixels");
 const auto retained_id=retained.active().id;const auto retained_paint=retained.active().painted;
 for(bool extra:{false,true}){
  PaintImageArchive images;auto encoded=original.active().painted->encode(&images);PaintSourceMaps maps;
  auto values=std::make_shared<const std::vector<std::array<float,2>>>(original.active().painted->mesh().uv);
  maps.color.image=std::make_shared<const Image>(Image{1,1,{255,255,255,255}});maps.color.texcoord=extra?2:1;
  if(extra)maps.extra_uv_sets[2]=values;else maps.uv1=values;encoded["source_maps"]=encode_source_maps(maps,images);
  auto before=PaintedMesh::decode(encoded,&images);auto snapshot=before->encode(&images);std::vector<uint32_t> remap(before->mesh().positions.size());std::iota(remap.begin(),remap.end(),0u);
  bool refused=false;try{retain_source_corner_paint(before,original.active().material_bindings,original.active().deformation,original.active().material_bindings->color_preparation->current_descriptor(),before->mesh(),remap);}catch(const std::exception&e){refused=std::string_view(e.what())=="Retained corner paint requires explicit atlas preparation of active source coordinates";}
  check(refused&&before->encode(&images)==snapshot,"Detached KeepPaint factory refuses active original coordinate owners before remapping or mutation");
 }
 auto mixed=retained.snapshot();for(const auto &object:mixed.objects())mixed.object(object.id).visible=false;mixed.object(retained_id).visible=true;mixed.object(retained_id).transform.position.x=-3;document.replace(mixed);endpoint(document);document.shading(false);endpoint(document);
 import_fixture::Fixture input;while(input.bin.size()%4)input.bin.push_back(0);const auto uv2=input.values({0,1,0,0,1,0,1,1},2,"VEC2");input.j["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_2"]=uv2;input.j["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]["texCoord"]=2;input.j["materials"][0]["alphaMode"]="OPAQUE";input.j["materials"][0].erase("alphaCutoff");
 const auto source=root/"genuine-source-uv2.glb";input.save(source);
 ImportSettings settings;settings.paint_resolution=64;document.configure_import(settings);document.import_asset(source);endpoint(document);auto combined=document.copy_scene();
 check(combined.encode().at("version")==26&&combined.encode().at("source_materials").at("version")==9,"Single native project writes actual Source26 ownership with material9 retained paint/normal context");
 check(combined.object(retained_id).painted->mesh_owner()==retained_paint->mesh_owner()&&combined.object(retained_id).painted->paint_sampling_attachment()==retained_paint->paint_sampling_attachment()&&combined.active().painted->source_maps()->uses_alternate_uv(),"Raw extra-coordinate import retains the earlier owning writable-paint object");
 ProjectionCamera camera;camera.pose.position={0,0,2};camera.width=camera.height=2;document.load_projection_image(image_path,camera);check(document.wait_projection_image(),"Actual current-main projection image worker loads on the mixed-context project");
 auto before=document.copy_scene().encode();check(!document.begin_native_stroke()&&document.frame().state->message==detail::editable_texture_atlas_notice&&document.copy_scene().encode()==before,"Projection trigger on raw extra coordinates refuses before any stroke/history mutation");
 document.begin_uv();check(document.wait_uv(),"Mixed-context raw object starts its real UV worker");document.edit_uv(UvAction::Pack);check(document.wait_uv()&&document.frame().state->uv->valid,"Actual Pack produces a validated editable atlas");document.end_uv(true);endpoint(document);
 auto atlas=document.copy_scene();check(!atlas.active().painted->source_maps()->uses_alternate_uv()&&atlas.active().material_bindings->uv_origin->paint->source_maps()->extra_uv_sets.contains(2),"Native UV Apply compiles current roles but retains the exact original UV2 owner");
 ink(document,atlas);const auto saved=root/"projected.dsculpt";document.save_async(saved).get();endpoint(document);auto painted=document.copy_scene();auto restored=Scene::load(saved);
 check(restored.encode()==painted.encode()&&restored.encode().at("version")==26,"Projection gesture and ordered Save/reopen retain both paint/normal pools and original extra coordinates");
 check(painted.active().painted->encode()!=atlas.active().painted->encode(),"Known opaque imported texel produces a real projected paint transaction");
 std::cout<<"PROJECTED_CHANGED "<<(painted.active().painted->encode()!=atlas.active().painted->encode())<<" CAN_UNDO "<<document.frame().state->can_undo<<std::endl;
 const bool undone=document.undo();auto undone_scene=document.copy_scene().encode();auto difference=nlohmann::json::diff(atlas.encode(),undone_scene);std::ofstream(root/"undo-difference.json")<<difference.dump(2);
 check(undone&&undone_scene==atlas.encode()&&document.redo()&&document.copy_scene().encode()==painted.encode(),"Actual projected paint has one exact Undo/Redo with current PBR counters");endpoint(document);
 auto native=draw(renderer,document.frame().surface);
 const auto output=root/"projected.glb";document.export_asset(output).get();prepared_normal_consumer::Model emitted(output);fbo.bind();independent.render(emitted.data);auto final_error=delta(native,read());std::cout<<"COMBINED_GLB_MAX "<<final_error<<std::endl;check(final_error<=3,"Actual combined native PBR and independently parsed final GLB agree after UV preparation and projection");
 cgltf_texture_view *wrong=nullptr;for(size_t m=0;m<emitted.data->meshes_count&&!wrong;++m)for(size_t p=0;p<emitted.data->meshes[m].primitives_count;++p){auto &primitive=emitted.data->meshes[m].primitives[p];if(primitive.material&&primitive.material->pbr_metallic_roughness.base_color_texture.texture&&primitive.material->pbr_metallic_roughness.base_color_texture.texcoord==2)wrong=&primitive.material->pbr_metallic_roughness.base_color_texture;}
 check(wrong!=nullptr,"Mixed current output retains an actual visible original UV2 material role");wrong->texcoord=0;fbo.bind();independent.render(emitted.data);auto wrong_error=delta(native,read());wrong->texcoord=2;std::cout<<"COMBINED_WRONG_UV_MAX "<<wrong_error<<std::endl;check(wrong_error>3,"Independent mixed-project comparison detects wrong original UV2 role sampling");
 auto lowered=restored.encode();lowered["version"]=24;check(rejects([&]{Scene::decode(lowered);}),"Retained-only original UV2 cannot be smuggled into Scene24 through the paint-domain history");
 std::cout<<"PASS "<<checks<<" combined Source24/26/current-main workflow checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
