// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NATIVE_PAINT32_RENDER_NO_MAIN
#include "native-paint32-render.cpp"
#undef DOME_NATIVE_PAINT32_RENDER_NO_MAIN
#include "dome/native-scene-append.hpp"
#include "dome/native-authoring.hpp"
#include "native-weight-retention.hpp"
#include <fstream>
namespace {
std::vector<uint8_t> union_bytes(const std::filesystem::path &path){
 std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Missing union output");
 return {std::istreambuf_iterator<char>(f),{}};
}
}
int main(int argc,char **argv)try{
 if(argc!=5&&(argc!=6||std::string_view(argv[5])!="--hardware"))return 2;
 const std::filesystem::path normal_dir=argv[1],uv_dir=argv[2],native_file=argv[3],out=argv[4];
 std::filesystem::create_directories(out);NativeGraphics graphics;
 const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<std::endl;
 const bool actual=device.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos&&device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos;
 check(argc!=6||actual,"Mixed authoring strict hardware request requires actual Intel Arc A770");
 Fbo fbo;SurfaceRenderer renderer;prepared_normal_consumer::Renderer independent;
 auto native=Scene::load(native_file);native.active().transform.position.x+=.65f;
 const auto native_exact=native.encode();check(native_exact.at("version")==32,"Native side is a genuine Scene32 factory/worker fixture");
 const std::array<std::filesystem::path,4> normal_files{normal_dir/"converted-7.dsculpt",uv_dir/"queued-7-1.dsculpt",uv_dir/"cleared-7-1.dsculpt",uv_dir/"rebaked-7-1.dsculpt"};
 unsigned number=0;
 for(const auto &normal_file:normal_files){
  const auto normals=Scene::load(normal_file);const auto normal_exact=normals.encode();
  check(rejects([&]{detail::native_weight_retention_impl::Counter budget(512*1024*1024,500'000'000);budget.material(normals.active().material_bindings);}),
    "Anatomical ownership budget refuses unrepresented authentic canonical roots, including after Clear");
  check(normal_exact.at("version")==33||normal_exact.at("version")==34,"Normal side owns actual converted/UV/Clear/rebake history");
  for(bool reverse:{false,true}){
   const auto &destination=reverse?native:normals,&package=reverse?normals:native;
   const auto dst=destination.encode(),src=package.encode();
   auto appended=append_native_scene(destination,package);auto combined=std::move(appended.scene);const auto exact=combined.encode();
   const auto &materials=exact.at("source_materials");
   check(exact.at("version")==35&&materials.at("version")==17&&materials.at("native_paint_sampling")==true&&materials.at("layered_normals")==true,
    "Actual native append owns both bounded material graphs in Scene35/material17");
   check(Scene::decode(exact).encode()==exact,"Independent owning decoder replays all combined paint/normal roots exactly");
   check(destination.encode()==dst&&package.encode()==src,"Successful append leaves both original projects exact");
   const auto prefix=out/std::to_string(number);combined.save(prefix.string()+".dsculpt");
   check(Scene::load(prefix.string()+".dsculpt").encode()==exact,"Combined native file saves and restarts every capability exactly");
   for(int fault=0;fault<7;++fault){auto bad=exact;auto &m=bad["source_materials"];
    if(fault==0)bad["version"]=34;
    if(fault==1)m["version"]=16;
    if(fault==2)m["native_paint_sampling"]=false;
    if(fault==3)m["layered_normals"]=false;
    if(fault==4)m["native_paints"]=nlohmann::json::array();
    if(fault==5)bad.erase("baked_normal_fields");
    if(fault==6)bad.erase("paint_sampling");
    check(rejects([&]{Scene::decode(bad);}),"Missing, downgraded or forged combined capability cannot authorize source data");
   }
   Document doc;doc.replace(combined);doc.shading(false);settled(doc);fbo.bind();const auto original=draw(renderer,doc.frame().surface);
   const auto glb=prefix.string()+".glb";doc.export_asset(glb,AssetExportFormat::EditableGlb256).get();
   check(doc.copy_scene().encode()==exact,"Actual combined editable export preserves the complete native project");
   const auto recovered=extract_native_authoring(union_bytes(glb));
   check(recovered&&recovered->encode()==exact,"Real emitted GLB embeds and recovers the whole combined Scene35");
   prepared_normal_consumer::Model model(glb);fbo.bind();independent.render(model.data);const auto portable=read();
   const auto error=delta(original,portable);std::cout<<"UNION_IMAGE state="<<normal_file.filename()<<" reverse="<<reverse<<" max="<<error<<std::endl;
   joint::capture(prefix.string()+"-native.png",original);joint::capture(prefix.string()+"-portable.png",portable);
   check(error<=4,"Independent mixed GLB/PNG consumer preserves both normal and native paint coordinates within unchanged4");
   Document reopened;reopened.replace(*recovered);reopened.shading(false);settled(reopened);fbo.bind();check(draw(renderer,reopened.frame().surface)==original,"Recovered combined scene renders every original pixel exactly");
   auto stripped=combined;
   for(auto id:appended.objects)stripped.erase(id);
   check(stripped.encode().at("version")==dst.at("version"),"Removing the imported capability restores the genuine minimal original Scene format");
   if(number==0){
    auto fake=normal_exact;fake["version"]=35;check(rejects([&]{Scene::decode(fake);}),"A Scene35 number alone cannot manufacture native ownership");
    unsigned polls=0;check(rejects([&]{append_native_scene(destination,package,[&]{return ++polls>=3;});})&&destination.encode()==dst&&package.encode()==src,
      "Cancelled actual append preserves both complete owning scenes");
    const auto native_id=reverse?destination.active().id:appended.objects.front();
    doc.select_object(native_id);doc.begin_uv();check(doc.wait_uv(),"Actual native UV worker opens inside the combined scene");
    doc.edit_uv(UvAction::Pick,.5f,.5f);doc.edit_uv(UvAction::Scale,.8f);check(doc.wait_uv()&&doc.frame().state->uv->valid,"Combined native UV draft remains genuinely editable");
    doc.end_uv(true);const auto save=out/"queued-UV35.dsculpt";doc.save_async(save).get();settled(doc);const auto changed=doc.copy_scene().encode();
    check(changed!=exact&&changed.at("version")==35&&Scene::load(save).encode()==changed,"Ordered Save waits for actual mixed-scene UV Apply and exact restart");
    check(doc.undo(),"Mixed-scene UV Apply has one global Undo");settled(doc);check(doc.copy_scene().encode()==exact,"Mixed-scene Undo retains original normal history and native source");
    check(doc.redo(),"Mixed-scene UV Apply has one global Redo");settled(doc);check(doc.copy_scene().encode()==changed,"Mixed-scene Redo restores exact combined authority");
   }
   ++number;
  }
  check(normals.encode()==normal_exact&&native.encode()==native_exact,"All export/history/counterfactual work leaves the actual source files' decoded contents exact");
 }
 std::cout<<"PASS "<<checks<<" mixed native paint and normal authoring controls\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
