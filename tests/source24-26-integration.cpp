// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_PAINT_SAMPLING_RENDER_NO_MAIN
#include "paint-sampling-render.cpp"
#undef DOME_PAINT_SAMPLING_RENDER_NO_MAIN
#include "dome/import-authoring.hpp"
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
 auto original=joint::prepare(true,Material::AlphaMode::Mask);auto retained=retain_writable(original);Document document;document.replace(retained);endpoint(document);document.shading(false);endpoint(document);
 auto source=root/"current-extra-coordinates.glb";document.export_asset(source).get();prepared_normal_consumer::Model raw(source);
 check(prepared_normal_consumer::attr(raw.data->meshes[0].primitives[0].attributes,raw.data->meshes[0].primitives[0].attributes_count,cgltf_attribute_type_texcoord,2)!=nullptr,"Actual current-domain compiler supplies genuine contiguous UV0/UV1/UV2");
 const auto retained_id=retained.active().id;const auto retained_paint=retained.active().painted;
 ImportSettings settings;settings.paint_resolution=64;document.import_settings(settings);document.import_asset(source);endpoint(document);auto combined=document.copy_scene();
 check(combined.encode().at("version")==26&&combined.encode().at("source_materials").at("version")==9,"Single native project writes actual Source26 ownership with material9 retained paint/normal context");
 check(combined.object(retained_id).painted==retained_paint&&combined.active().painted->source_maps()->uses_alternate_uv(),"Raw extra-coordinate import retains the earlier owning writable-paint object");
 const auto image=Image{1,1,{255,0,0,255}}.png();const auto image_path=root/"projection.png";{std::ofstream out(image_path,std::ios::binary);out.write(reinterpret_cast<const char*>(image.data()),image.size());}
 ProjectionCamera camera;camera.pose.position={0,0,2};camera.width=camera.height=2;document.load_projection_image(image_path,camera);check(document.wait_projection_image(),"Actual current-main projection image worker loads on the mixed-context project");
 auto before=document.copy_scene().encode();check(!document.begin_native_stroke()&&document.frame().state->message==detail::editable_texture_atlas_notice&&document.copy_scene().encode()==before,"Projection trigger on raw extra coordinates refuses before any stroke/history mutation");
 document.begin_uv();check(document.wait_uv(),"Mixed-context raw object starts its real UV worker");document.edit_uv(UvAction::Pack);check(document.wait_uv()&&document.frame().state->uv->valid,"Actual Pack produces a validated editable atlas");document.end_uv(true);endpoint(document);
 auto atlas=document.copy_scene();check(!atlas.active().painted->source_maps()->uses_alternate_uv()&&atlas.active().material_bindings->uv_origin->paint->source_maps()->extra_uv_sets.contains(2),"Native UV Apply compiles current roles but retains the exact original UV2 owner");
 ink(document,atlas);const auto saved=root/"projected.dsculpt";document.save_async(saved).get();endpoint(document);auto painted=document.copy_scene();auto restored=Scene::load(saved);
 check(restored.encode()==painted.encode()&&restored.encode().at("version")==26,"Projection gesture and ordered Save/reopen retain both paint/normal pools and original extra coordinates");
 check(document.undo()&&document.copy_scene().encode()==atlas.encode()&&document.redo()&&document.copy_scene().encode()==painted.encode(),"Actual projected paint has one exact Undo/Redo with current PBR counters");endpoint(document);
 Fbo fbo;fbo.bind();SurfaceRenderer renderer;prepared_normal_consumer::Renderer independent;auto native=draw(renderer,document.frame().surface);
 const auto output=root/"projected.glb";document.export_asset(output).get();prepared_normal_consumer::Model emitted(output);fbo.bind();independent.render(emitted.data);check(delta(native,read())<=3,"Actual combined native PBR and independently parsed final GLB agree after UV preparation and projection");
 auto lowered=restored.encode();lowered["version"]=24;check(rejects([&]{Scene::decode(lowered);}),"Retained-only original UV2 cannot be smuggled into Scene24 through the paint-domain history");
 std::cout<<"PASS "<<checks<<" combined Source24/26/current-main workflow checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
