// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NORMAL_COLOR_RENDER_NO_MAIN
#include "normal-color-uv-render.cpp"
#undef DOME_NORMAL_COLOR_RENDER_NO_MAIN
#include "dome/native-authoring.hpp"
#include "dome/emission-native.hpp"
#include "dome/bake.hpp"
#include <numeric>
#include "native-authoring-compiler.hpp"
#include "paint-overlay-sample.hpp"
#include <fstream>
namespace {
std::vector<uint8_t> bytes(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
Scene plain(){Scene scene;scene.active().visible=false;const auto id=scene.add("Native physical paint",Volume{});scene.select(id);auto &o=scene.active();o.mesh_active=true;o.painted=std::make_shared<PaintedMesh>(quad(),64);o.material.unlit=false;o.material.color={.7f,.8f,.6f,1};return scene;}
void ink(Scene &scene){auto &o=scene.active();o.material.emissive={8,.016f,.16f};o.painted=o.painted->snapshot();auto &p=*o.painted;p.add_layer("Float partial mask");PaintSettings s;s.front_only=false;s.channel=PaintChannel::Emission;s.emission_units=PaintEmissionUnits::Physical;s.emission={12,.002f,3};Brush b;b.from=b.to={-.02f,.015f,0};b.normal={0,0,1};
 const auto &mesh=p.mesh();for(size_t f=0;f<mesh.indices.size()/3;++f)if(mesh.normals.at(mesh.indices[f*3]).z>.9f){b.from=b.to=(mesh.positions.at(mesh.indices[f*3])+mesh.positions.at(mesh.indices[f*3+1])+mesh.positions.at(mesh.indices[f*3+2]))/3;break;}
 b.radius=.19f;b.strength=.73f;p.begin_stroke();p.stamp(b,s);check(p.end_stroke(),"Actual native/retained physical brush changes float detail");p.layer_properties("Float partial mask",true,.63f);s.channel=PaintChannel::Mask;s.mask=.4f;b.radius=.1f;p.begin_stroke();p.stamp(b,s);check(p.end_stroke(),"Physical float detail uses the actual editable common mask");p.add_layer("Hidden high radiance");s.channel=PaintChannel::Emission;s.emission={10000,10000,10000};p.begin_stroke();p.stamp(b,s);p.end_stroke();p.layer_properties("Hidden high radiance",false,1);}
}
int main(int argc,char **argv)try{
 if(argc<2||argc>3)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 NativeGraphics graphics;const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER)),vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR));
 std::cout<<"GPU "<<vendor<<" / "<<device<<std::endl;
 check(argc<3||std::string_view(argv[2])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Hardware mode requires actual Intel Arc A770");
 Fbo fbo;SurfaceRenderer native;native.set_frame_time(0);prepared_normal_consumer::Renderer independent;
 for(int kind=0;kind<4;++kind){
  std::cout<<"STAGE kind="<<kind<<" prepare"<<std::endl;
  auto scene=kind==1?joint::transfer(joint::prepare(false,Material::AlphaMode::Opaque)):plain();
  if(kind==2){auto &o=scene.active();o.mesh_active=false;o.material.emissive={8,.016f,.16f};Brush sphere;sphere.radius=.3f;sphere.blend=0;o.volume->begin_stroke();o.volume->stamp(sphere);check(o.volume->end_stroke(),"Native constant test uses a genuine sculpted SDF surface");}else ink(scene);
  if(kind==3)scene.active().material.unlit=true;
  const auto exact=scene.encode();const auto paint=scene.active().painted;const auto binding=scene.active().material_bindings;
  NativeAuthoringExport options{std::make_shared<const Scene>(scene.snapshot()),NativeAuthoringExport::Scope::WholeScene,{},true};
  const auto capture=detail::NativeAuthoringCapture::create(options,{});
  auto compiled=detail::compile_native_authoring(*capture,{},256*1024*1024,{});
  check(compiled.emission.size()==1,"Only the actual visible native physical object owns compiled emission");
  const auto &field=compiled.emission.front();
  if(kind==3)check(field.unlit&&field.unlit_ignored_samples>100&&field.strength<30,"Unlit native view reports ignored actual radiance and excludes the hidden 10000-unit layer");
  if(kind!=2){check(field.image.width==paint->resolution(),"Float view preserves its actual writable-grid resolution");
    auto packet=detail::PaintOverlayFactory::capture(*paint);double max_error=0;size_t nonzero=0;
    auto lin=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
    // Independent coefficient/coverage equation at actual texel centres. No
    // generated source image or colour receipt is used as normal authority.
    for(size_t at=0;at<field.image.rgba.size()/4;++at){const auto owner=packet->padding()->at(at);std::array<double,3> legacy{},physical{};double coverage=0;
      if(owner>=0){const size_t x=size_t(owner)%field.image.width,y=size_t(owner)/field.image.width;
        if(auto baked=paint->baked()){coverage=baked->coverage.at(size_t(owner))/255.;for(size_t k=0;k<3;++k)legacy[k]=lin(baked->maps.emission.rgba.at(size_t(owner)*4+k)/255.)*coverage;}
        for(const auto &layer:paint->layers())if(layer.visible&&layer.opacity>0){const auto &common=paint->pixel(layer.id,x,y);const auto e=paint->emission_pixel(layer.id,x,y);const double a=double(layer.opacity)*common.data[13]/255.*e.coverage;
          for(size_t k=0;k<3;++k){legacy[k]=legacy[k]*(1-a)+e.legacy[k]*a;physical[k]=physical[k]*(1-a)+e.physical[k]*a;}coverage=coverage*(1-a)+a;}}
      for(size_t k=0;k<3;++k){const double expected=scene.active().material.emissive[k]*(1-coverage)+legacy[k]+physical[k];const double actual=lin(field.image.rgba[at*4+k]/255.)*field.factor[k]*field.strength;max_error=std::max(max_error,std::abs(expected-actual));nonzero+=expected>0;}}
    check(nonzero>100&&max_error<=field.maximum_absolute_error+1e-5,"Actual physical radiance/factor/strength obeys its measured texel quantization bound");
    if(kind==1)check(compiled.surfaces.front().paint_sampling_attachment==paint->paint_sampling_attachment(),"Retained emission preserves genuine paint coordinates separately from authored UV0 and normal authority");
  }else check(!field.image.width&&field.factor[0]*field.strength==8,"Native constant radiance needs no invented UV/image authority");
  const auto path=out/("native"+std::to_string(kind)+".glb");
  std::cout<<"STAGE kind="<<kind<<" export"<<std::endl;
  export_glb_authoring(options,path);
  auto restored=extract_native_authoring(bytes(path));check(restored&&restored->encode()==exact,"Native physical project recovers every float plane/owner/history exactly");
  restored->save(out/("native"+std::to_string(kind)+".dsculpt"));
  export_glb_authoring(NativeAuthoringExport{std::make_shared<const Scene>(restored->snapshot()),NativeAuthoringExport::Scope::WholeScene,{},true},out/("cycle"+std::to_string(kind)+".glb"));
  check(extract_native_authoring(bytes(out/("cycle"+std::to_string(kind)+".glb")))->encode()==exact,"Second physical native production retains the same exact project");
  check(scene.encode()==exact&&scene.active().painted==paint&&scene.active().material_bindings==binding,"Physical compiler never mutates original paint/material control owners");
  if(kind==1){prepared_normal_consumer::Model file(path);const auto &m=file.data->materials[0];check(m.emissive_texture.texcoord>0,"Retained emission binds its genuinely emitted independent paint coordinate accessor");}
  Document document;document.replace(scene.snapshot());document.shading(false);settled(document);
  const auto snapshot=document.frame().surface;fbo.bind();const auto original=draw(native,snapshot);
  prepared_normal_consumer::Model exported(path);fbo.bind();independent.render(exported.data,nullptr);const auto portable=read();
  std::cout<<"NATIVE_PHYSICAL_PIXELS kind="<<kind<<" max="<<delta(original,portable)<<std::endl;
  joint::capture(out/("native"+std::to_string(kind)+".png"),original);joint::capture(out/("portable"+std::to_string(kind)+".png"),portable);
  check(delta(original,portable)<=2,"Actual Document Core PBR matches independently parsed finite emission/normal/alpha images within two pixels");
  Document reopened;reopened.replace(restored->snapshot());reopened.shading(false);settled(reopened);fbo.bind();
  check(delta(original,draw(native,reopened.frame().surface))==0,"Recovered native float project renders exactly the original full frame");
  const auto texture_uploads=native.texture_stats().uploads;const auto geometry_uploads=native.buffer_stats().geometry_uploads;
  fbo.bind();draw(native,reopened.frame().surface);
  check(native.texture_stats().uploads==texture_uploads&&native.buffer_stats().geometry_uploads==geometry_uploads,"Second eye/no-op reuses actual native float planes and geometry without uploads");
  if(kind==1){auto &emission=exported.data->materials[0].emissive_texture;const auto coordinate=emission.texcoord;emission.texcoord=0;fbo.bind();independent.render(exported.data,nullptr);const auto wrong=read();emission.texcoord=coordinate;
    check(delta(portable,wrong)>2,"Independent wrong authored-UV emission counterfactual detects the genuine retained paint coordinate");}
  if(kind==0){
    for(const auto [format,resolution]:{std::pair{AssetExportFormat::EditableGlb256,256},std::pair{AssetExportFormat::EditableGlb512,512},std::pair{AssetExportFormat::EditableGlb1024,1024}}){
      const auto worker_file=out/("worker-"+std::to_string(resolution)+".glb");document.export_asset(worker_file,format).get();
      prepared_normal_consumer::Model emitted(worker_file);const auto image=prepared_normal_consumer::image(emitted.data->materials[0].emissive_texture.texture);
      check(image.width==uint32_t(resolution)&&image.height==uint32_t(resolution),"Actual native file worker honors the selected256/512/1024 finite-emission resolution");
      fbo.bind();independent.render(emitted.data,nullptr);check(delta(original,read())<=2,"Actual worker finite resolution preserves the sensitive native physical PBR view within two pixels");
      Document recovered;recovered.open_async(worker_file).get();recovered.shading(false);settled(recovered);
      check(recovered.copy_scene().encode()==exact&&recovered.frame().state->dirty&&recovered.frame().state->project_path.empty(),"Actual Open worker restores the exact float project and unsaved native identity");
      const auto saved=out/("worker-"+std::to_string(resolution)+".dsculpt");recovered.save_async(saved).get();
      const auto again=out/("worker-cycle-"+std::to_string(resolution)+".glb");recovered.export_asset(again,format).get();
      check(Scene::load(saved).encode()==exact&&bytes(again)==bytes(worker_file),"Native worker Save and second editable-export cycle preserve exact project and deterministic portable bytes");
    }
  }
  if(kind==1){auto wrong=std::make_shared<MeshSnapshot>(*snapshot);wrong->ranges.front().textures=reopened.frame().surface->ranges.front().textures;bool refused=false;
    try{draw(native,wrong);}catch(const std::exception&e){refused=std::string(e.what()).find("Native emission packet lost")!=std::string::npos;}
    check(refused&&scene.encode()==exact,"A genuinely decoded foreign native packet cannot borrow another retained paint-domain control owner");
    fbo.bind();check(delta(original,draw(native,snapshot))==0,"Renderer retries cleanly after a foreign retained emission packet refusal");}
  if(kind==0){BakeSource high;high.mesh=paint->mesh();high.materials.front().material=scene.active().material;high.materials.front().material.emissive={};high.materials.front().maps=paint->preview(scene.active().material);BakeOptions bake;bake.resolution=64;bake.ao_samples=0;
    check(rejects([&]{bake_mesh(paint->mesh(),high,bake);})&&scene.encode()==exact,"Native float preview cannot silently enter the legacy RGB Bake source adapter");}
  const auto before=bytes(path);SourceUvBakeOptions budget;budget.max_sample_operations=1;
  if(kind!=2)check(rejects([&]{export_glb_authoring(options,path,{},budget);})&&bytes(path)==before&&scene.encode()==exact,"Physical image/sample quota refuses before publication with exact old destination and native Scene");
  if(kind!=2){budget={};budget.max_working_bytes=1;
    check(rejects([&]{export_glb_authoring(options,path,{},budget);})&&bytes(path)==before,"Native nonemission staging preflights its requested working cap before publication");
    SourceUvBakeInput uv;uv.paint=paint;uv.current_material=binding;uv.current_deformation=scene.active().deformation;uv.target=paint->mesh();uv.source_vertices.resize(uv.target.positions.size());std::iota(uv.source_vertices.begin(),uv.source_vertices.end(),0);bool physical_refused=false;
    try{bake_source_uv(uv);}catch(const std::exception&e){physical_refused=std::string(e.what()).find("Physical emission")!=std::string::npos;}
    check(physical_refused&&scene.encode()==exact,"Unsupported float UV transfer retains its explicit source-preserving refusal rather than RGB clipping");}
  check(rejects([&]{export_glb(scene.preview_surfaces(),out/"strict-refusal.glb");})&&scene.encode()==exact,"Ordinary strict GLB cannot borrow native physical capture authorization");
  GltfExportOptions cancel;cancel.before_commit=[]{throw std::runtime_error("actual precommit cancellation");};
  check(rejects([&]{export_glb_authoring(options,path,cancel);})&&bytes(path)==before,"Physical native precommit cancellation preserves the completed old destination");
 }
 std::cout<<"PASS "<<checks<<" native/retained physical authoring checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
