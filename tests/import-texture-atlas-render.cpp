// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/import-authoring.hpp"
#include "dome/material-bindings.hpp"
#include "import-authoring-fixtures.hpp"
#include "source-color-consumer.hpp"
#include "prepared-normal-consumer.hpp"
#include "dome/native-authoring.hpp"
#include "dome/source-paint-bake.hpp"
#include <numeric>
namespace {
// Resampling can move a discontinuous texel/alpha edge inside its two-pixel
// filtering footprint. Evaluate every visible pixel for mean error, and demand
// the same two-level bound as export for all visible samples outside that
// explicit footprint. Empty background never improves these statistics.
struct Fidelity {double mean=0;size_t visible=0,interior=0;int maximum=0,interior_maximum=0;};
Fidelity fidelity(const Pixels &reference,const Pixels &prepared){
 constexpr int side=Fbo::size;std::vector<bool> edge(side*side),footprint(side*side);
 for(int y=0;y<side;++y)for(int x=0;x<side;++x){auto at=y*side+x;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){auto nx=x+dx,ny=y+dy;if(nx<0||nx>=side||ny<0||ny>=side)continue;for(int c=0;c<4;++c)if(std::abs(int(reference[at*4+c])-int(reference[(ny*side+nx)*4+c]))>16)edge[at]=true;}}
 for(int y=0;y<side;++y)for(int x=0;x<side;++x)if(edge[y*side+x])for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){auto nx=x+dx,ny=y+dy;if(nx>=0&&nx<side&&ny>=0&&ny<side)footprint[ny*side+nx]=true;}
 Fidelity out;double total=0;
 for(size_t at=0;at<edge.size();++at){if(!reference[at*4+3]&&!prepared[at*4+3])continue;++out.visible;if(!footprint[at])++out.interior;for(int c=0;c<4;++c){auto error=std::abs(int(reference[at*4+c])-int(prepared[at*4+c]));total+=error;out.maximum=std::max(out.maximum,error);if(!footprint[at])out.interior_maximum=std::max(out.interior_maximum,error);}}
 if(out.visible)out.mean=total/(out.visible*4);return out;
}
void save_image(const std::filesystem::path &path,const Pixels &pixels){auto bytes=Image{Fbo::size,Fbo::size,pixels}.png();std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!file)throw std::runtime_error("Source fidelity capture failed");}
import_fixture::Fixture fixture(bool flat,const char *alpha,float weight){
 import_fixture::Fixture f;f.j["nodes"][0]["translation"]={0,0,0};f.j["nodes"][0]["rotation"]={0,0,0,1};f.j["nodes"][0]["scale"]={3,3,3};f.j["nodes"][1]["translation"]={-.14f,0,0};f.j["nodes"][1]["scale"]={1,1,1};f.j["nodes"][2]["translation"]={.14f,0,0};f.j["nodes"][1]["weights"]={weight};f.j["nodes"][2]["weights"]={weight};
 auto &m=f.j["materials"][0];m.erase("extensions");m["alphaMode"]=alpha;m["pbrMetallicRoughness"]["baseColorFactor"][3]=.75f;m["pbrMetallicRoughness"]["baseColorTexture"]["texCoord"]=1;
 auto png=image({230,153,204,255})->png();while(f.bin.size()%4)f.bin.push_back(0);int view=int(f.j["bufferViews"].size());f.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",f.bin.size()},{"byteLength",png.size()}});f.bin.insert(f.bin.end(),png.begin(),png.end());int im=int(f.j["images"].size());f.j["images"].push_back({{"bufferView",view},{"mimeType","image/png"}});int texture=int(f.j["textures"].size());f.j["textures"].push_back({{"source",im},{"sampler",0}});m["normalTexture"]={{"index",texture},{"texCoord",1},{"scale",.6f}};
 if(flat){auto &p=f.j["meshes"][0]["primitives"][0];p["attributes"].erase("NORMAL");p["targets"][0].erase("NORMAL");}return f;
}
void paint(dome::Document &document){auto scene=document.copy_scene();auto &o=scene.active();auto mesh=o.deformation?o.deformation->preview(o.painted->mesh()):o.painted->mesh();auto local=mesh.positions[mesh.indices[0]]*.05f+mesh.positions[mesh.indices[1]]*.05f+mesh.positions[mesh.indices[2]]*.9f;auto world=scene.world_matrix(o.id).point({local.x,local.y,local.z});Vec3 p{float(world[0]),float(world[1]),float(world[2])};auto n=document.gradient(p);PaintSettings ink;ink.color={.9f,.2f,.4f,.65f};document.paint_settings(ink);NativeStrokeSample sample;sample.origin=p+n;sample.direction=n*-1.f;sample.brush.radius=.13f;check(document.begin_native_stroke()&&document.sample_native_stroke(sample),"Actual native worker paints an explicitly prepared UV1 source in its current morph pose");document.end_native_stroke();check(document.wait_native_stroke()&&document.wait_mesh()&&document.frame().state->error.empty(),"Prepared UV1 paint commits through the real worker before independent rendering");}
}
int main(int argc,char **argv)try{
 if(argc<2)return 2;auto directory=std::filesystem::path(argv[1]);std::filesystem::create_directories(directory);NativeGraphics context;std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),gpu=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GL_VENDOR "<<vendor<<"\nGL_RENDERER "<<gpu<<'\n';check(argc<3||std::string_view(argv[2])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&gpu.find("A770")!=std::string::npos),"Requested independent atlas consumer runs on the actual Arc device");Fbo fbo;fbo.bind();SurfaceRenderer native;prepared_normal_consumer::Renderer independent;
 for(bool flat:{false,true})for(const char *alpha:{"OPAQUE","MASK","BLEND"})for(float weight:{0.f,.65f}){
  auto f=fixture(flat,alpha,weight);auto prefix=std::string(flat?"flat-":"artist-")+alpha+"-"+std::to_string(weight);auto path=directory/(prefix+".glb");f.save(path);Scene before;before.active().visible=false;ImportAuthoringOptions options;options.paint_resolution=512;options.prepare_texture_atlas=true;auto imported=prepare_authoring_import(before,path,options);
  auto raw_options=options;raw_options.prepare_texture_atlas=false;
  auto original=prepare_authoring_import(before,path,raw_options).scene;
  Document raw;raw.replace(original);check(raw.wait_mesh()&&raw.frame().state->error.empty(),"Exact retained original UV1 source renders independently of its prepared atlas");
  auto reference=draw(native,raw.frame().surface);
  Document document;document.replace(imported.scene);check(document.wait_mesh()&&document.frame().state->error.empty(),"Actual explicitly prepared UV1 import publishes immutable current render geometry");
  auto prepared=draw(native,document.frame().surface);auto quality=fidelity(reference,prepared);
  save_image(directory/(prefix+"-original.png"),reference);save_image(directory/(prefix+"-prepared.png"),prepared);
  check(quality.visible>128&&quality.interior>128,"Source-fidelity comparison covers actual visible surfaces and substantial non-edge interiors");
  std::cout<<"SOURCE_FIDELITY flat="<<flat<<" alpha="<<alpha<<" weight="<<weight<<" maxerror="<<quality.maximum<<" visible_mean="<<quality.mean<<" interior_max="<<quality.interior_maximum<<" interior_pixels="<<quality.interior<<'\n';
  check(quality.maximum==0,"Independent writable atlas retains every visible original-source pixel exactly, including filtered alpha boundaries");
  if(!flat&&std::string_view(alpha)=="OPAQUE"&&weight==0){
    auto wrong=original.snapshot();for(auto id:imported.objects){auto &object=wrong.object(id);auto maps=std::make_shared<PaintSourceMaps>(*object.painted->source_maps());auto uv=std::make_shared<std::vector<std::array<float,2>>>(*maps->uv1);for(auto &q:*uv)q[0]=1-q[0];maps->uv1=uv;PaintSurfaceOptions keep;keep.policy=PaintUvPolicy::SourceAuthored;object.painted=PaintedMesh::from_maps(object.painted->mesh(),maps,512,keep);}
    Document changed;changed.replace(wrong);check(changed.wait_mesh()&&changed.frame().state->error.empty(),"Deliberately wrong UV1 source publishes an independent sensitivity reference");auto sensitivity=fidelity(reference,draw(native,changed.frame().surface));
    check(sensitivity.interior_maximum>32&&sensitivity.mean>2,"Source-fidelity oracle rejects a wrong coordinate domain in visible interiors");
  }
  Pixels previous;
  for(int stage=0;stage<2;++stage){
   if(stage)paint(document);
   const auto state=document.copy_scene().encode();auto pixels=draw(native,document.frame().surface);size_t visible=0;for(size_t i=3;i<pixels.size();i+=4)visible+=pixels[i]!=0;
   check(visible>128,"Source-aware imported material has substantial visible coverage");
   auto exported=directory/(prefix+"-"+std::to_string(stage)+".glb");
   if(!stage){
    document.export_asset(exported).get();color_consumer::Model model(exported);fbo.bind();independent.render(model.data);auto error=delta(pixels,read());
    check(error<=2,"Independent parsed ordinary GLB retains original UV1 image sampling, frames, colors, alpha and morphs before painting");
   }else{
    document.export_asset(exported,AssetExportFormat::EditableGlb512).get();color_consumer::Model model(exported);fbo.bind();independent.render(model.data);const auto file_pixels=read();const auto loss=fidelity(pixels,file_pixels);
    std::cout<<"EXPLICIT_FINITE_VIEW "<<prefix<<" mean="<<loss.mean<<" max="<<loss.maximum<<" interior="<<loss.interior_maximum<<'\n';
    check(loss.visible>128&&loss.mean<=2&&loss.interior_maximum<=2,"Explicit finite output retains the nominal-view bounds on these twelve fixtures; minification counterexamples remain separate");
    // The portable view has a deliberately finite sampling domain. Qualify its
    // actual emitted pixels against a separately installed detached bake, and
    // qualify exact native recovery separately; never label it lossless.
    auto converted=document.copy_scene();std::map<uint64_t,SourcePaintBakeResult> views;
    for(auto id:imported.objects){auto &o=converted.object(id);SourceUvBakeInput input;input.paint=o.painted;input.appearance=o.appearance;input.current_material=o.material_bindings;input.current_deformation=o.deformation;input.target=o.painted->mesh();input.source_vertices.resize(input.target.positions.size());std::iota(input.source_vertices.begin(),input.source_vertices.end(),0);SourceUvBakeOptions policy;policy.resolution=512;auto view=bake_source_paint(input,o.material,policy);auto adopted=adopt_source_uv_bake(view.sampling);o.painted=adopted.paint;o.material_bindings=adopted.material_bindings;o.deformation=adopted.deformation;o.appearance=adopted.appearance;views.emplace(id,std::move(view));}
    Document finite;finite.replace(converted);finite.shading(false);check(finite.wait_mesh()&&finite.frame().state->error.empty(),"Detached explicit finite material view publishes on its own immutable document");
    auto compiled=std::make_shared<MeshSnapshot>(*finite.frame().surface);for(auto &range:compiled->ranges)if(auto found=views.find(range.object);found!=views.end()){range.textures=found->second.textures;range.material_bindings=found->second.material_bindings;}
    check(delta(file_pixels,draw(native,compiled))<=2,"Independent actual-worker GLB matches its explicitly compiled finite PBR material view");
    Document recovered;recovered.open_async(exported).get();recovered.shading(false);check(recovered.wait_mesh()&&recovered.frame().state->error.empty()&&recovered.copy_scene().encode()==state,"Actual package Open recovers exact current layers, source images and coordinate owners");
    check(delta(pixels,draw(native,recovered.frame().surface))==0,"Actual native recovery restores every original painted preview pixel exactly");
    check(pixels!=previous,"Native atlas painting produces a visible material change");
   }
   check(document.copy_scene().encode()==state,"Actual ordinary or explicit finite export leaves the complete authoring document exact");previous=std::move(pixels);
  }
 }
 std::cout<<"Editable atlas render checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
