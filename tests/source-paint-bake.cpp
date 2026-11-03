// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_mtoon_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "dome/source-uv-bake.hpp"
#include "dome/source-paint-bake.hpp"
#include "source-paint-fixtures.hpp"
#include "source-color-consumer.hpp"
#include <numeric>
namespace {
struct Fidelity {double mean=0;size_t visible=0,interior=0;int maximum=0,interior_maximum=0;};
Fidelity fidelity(const Pixels &reference,const Pixels &actual){
 constexpr int side=Fbo::size;std::vector<bool> edge(side*side),footprint(side*side);
 for(int y=0;y<side;++y)for(int x=0;x<side;++x){auto at=y*side+x;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){auto nx=x+dx,ny=y+dy;if(nx<0||nx>=side||ny<0||ny>=side)continue;for(int c=0;c<4;++c)if(std::abs(int(reference[at*4+c])-int(reference[(ny*side+nx)*4+c]))>16)edge[at]=true;}}
 for(int y=0;y<side;++y)for(int x=0;x<side;++x)if(edge[y*side+x])for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){auto nx=x+dx,ny=y+dy;if(nx>=0&&nx<side&&ny>=0&&ny<side)footprint[ny*side+nx]=true;}
 Fidelity out;double total=0;for(size_t at=0;at<edge.size();++at){if(!reference[at*4+3]&&!actual[at*4+3])continue;++out.visible;if(!footprint[at])++out.interior;for(int c=0;c<4;++c){auto error=std::abs(int(reference[at*4+c])-int(actual[at*4+c]));total+=error;out.maximum=std::max(out.maximum,error);if(!footprint[at])out.interior_maximum=std::max(out.interior_maximum,error);}}
 if(out.visible)out.mean=total/(out.visible*4);return out;
}
void settle(Document &d){check(d.wait_mesh()&&d.frame().state->error.empty(),"Actual source-aware bake project reaches native rendering");}
void ink(Scene &scene){auto &o=scene.active();auto p=o.painted->snapshot();
 for(auto channel:{PaintChannel::Color,PaintChannel::Metallic,PaintChannel::Roughness,PaintChannel::Emission}){
  p->add_layer("Independent field");PaintSettings ink;ink.front_only=false;ink.channel=channel;ink.color={.1f,.7f,.3f,.7f};ink.metallic=.8f;ink.roughness=.3f;ink.emission={.03f,.05f,.08f};
  Brush b;b.from=b.to={.02f,.01f,0};b.normal={0,0,1};b.radius=.13f;b.strength=.45f;p->begin_stroke();p->stamp(b,ink);check(p->end_stroke(),"Real editable source paint contributes its independently covered channel");p->layer_properties("Independent field",true,.65f);
 }
 p->add_layer("Hidden field");p->layer_properties("Hidden field",false,.3f);o.painted=std::move(p);
}
}
int main(int argc,char **argv)try{
 if(argc<2||argc>3)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<'\n';check(argc<3||std::string_view(argv[2])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Requested source paint bake proof requires actual A770");
 Fbo fbo;fbo.bind();SurfaceRenderer native;native.set_frame_time(0);color_consumer::Renderer independent;Scene initial;initial.active().visible=false;ImportAuthoringOptions options;options.prepare_texture_atlas=true;options.paint_resolution=64;
 for(int kind=0;kind<6;++kind){auto fixture=source_paint_fixture::input(kind);auto path=out/("case"+std::to_string(kind)+".glb");fixture.save(path);auto imported=prepare_authoring_import(initial,path,options);ink(imported.scene);const auto &object=imported.scene.active();const auto before=imported.scene.encode();
  SourceUvBakeInput input;input.paint=object.painted;input.appearance=object.appearance;input.current_material=object.material_bindings;input.current_deformation=object.deformation;input.target=object.painted->mesh();input.source_vertices.resize(input.target.positions.size());std::iota(input.source_vertices.begin(),input.source_vertices.end(),0);
  SourceUvBakeOptions bake_options;bake_options.resolution=512;auto portable=bake_source_paint(input,object.material,bake_options);const auto &result=portable.sampling;auto adopted=adopt_source_uv_bake(result);check(imported.scene.encode()==before,"Pure original-coordinate baking preserves the complete native project");
  check(result.layers.size()==object.painted->layers().size()&&result.active_layer==object.painted->active_layer(),"Portable atlas preparation retains every visible/hidden editable layer and active identity");
  for(size_t n=0;n<result.layers.size();++n){const auto &a=result.layers[n],&b=object.painted->layers()[n];check(a.id==b.id&&a.name==b.name&&a.opacity==b.opacity&&a.visible==b.visible&&a.tiles==b.tiles&&a.pbr_tiles==b.pbr_tiles&&a.emission_tiles==b.emission_tiles,"Same-atlas original-image conversion preserves exact sparse paint owners rather than resampling them");}
  auto converted=imported.scene.snapshot();auto &current=converted.active();current.painted=adopted.paint;current.material_bindings=adopted.material_bindings;current.deformation=adopted.deformation;current.appearance=adopted.appearance;converted.validate();auto encoded=converted.encode();check(Scene::decode(encoded).encode()==encoded,"Actual converted material/paint/frame owners survive native restart");
  Document a,b;a.replace(imported.scene);b.replace(converted);settle(a);settle(b);
  for(bool pbr:{false,true}){if(pbr){a.shading(false);b.shading(false);}else{a.authored_shading();b.authored_shading();}settle(a);settle(b);auto compiled=std::make_shared<MeshSnapshot>(*b.frame().surface);for(auto &range:compiled->ranges)if(range.object==object.id){range.textures=portable.textures;range.material_bindings=portable.material_bindings;}
   auto reference=draw(native,a.frame().surface),actual=draw(native,compiled);auto quality=fidelity(reference,actual);std::cout<<"SOURCE_PAINT_BAKE kind="<<kind<<" pbr="<<pbr<<" mean="<<quality.mean<<" max="<<quality.maximum<<" interior="<<quality.interior<<" interior_max="<<quality.interior_maximum<<'\n';check(quality.visible>20&&quality.mean<=2&&quality.interior_maximum<=2,"Explicit original-image conversion retains the existing visible mean/interior quality bounds");
   if(pbr){auto file=out/("baked"+std::to_string(kind)+".glb");auto surfaces=converted.export_surfaces();for(auto &surface:surfaces)if(surface.id==object.id){surface.textures=portable.textures;surface.material_bindings=portable.material_bindings;}export_glb(surfaces,file);color_consumer::Model model(file);fbo.bind();independent.render(model.data,nullptr,{},true);check(delta(actual,read())<=2,"Explicitly compiled painted surface export matches an independently parsed standard PBR consumer");}
  }
 }
 std::cout<<"PASS "<<checks<<" source-aware material bake checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
