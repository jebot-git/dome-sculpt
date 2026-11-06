// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_mtoon_graphical_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/scene-file.hpp"
#include "dome/polygon.hpp"
#include "polygon-surface-preview.hpp"
#include "source-color-fixtures.hpp"
#include <fstream>
using namespace dome;
namespace {size_t polygon_programs=0,maximum_samplers=0;}
extern "C" void __real_glLinkProgram(GLuint);
extern "C" void __wrap_glLinkProgram(GLuint program){
 __real_glLinkProgram(program);GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);if(!linked)return;
 GLint count=0;glGetProgramiv(program,GL_ACTIVE_UNIFORMS,&count);size_t samplers=0;
 for(GLint n=0;n<count;++n){GLchar name[256];GLint size=0;GLenum type=0;glGetActiveUniform(program,n,sizeof(name),nullptr,&size,&type,name);if(type==GL_SAMPLER_2D||type==GL_SAMPLER_BUFFER)samplers+=size;}
 maximum_samplers=std::max(maximum_samplers,samplers);
 if(glGetUniformLocation(program,"polygon_mode")>=0){++polygon_programs;if(samplers>16)throw std::runtime_error("Actual polygon program exceeds the OpenGL3.3 minimum16 fragment samplers");if(glGetUniformLocation(program,"polygon_frames")>=0)throw std::runtime_error("Polygon program retained a second active sampler-buffer alias");}
 std::cout<<"ACTIVE_PROGRAM "<<program<<" samplers="<<samplers<<" polygon="<<(glGetUniformLocation(program,"polygon_mode")>=0)<<std::endl;
}
namespace {
void settled(Document&d){if(!d.wait_asset(std::chrono::seconds(120))||!d.wait_mesh(std::chrono::seconds(120))||!d.frame().state->error.empty())throw std::runtime_error("Actual native frame failed: "+d.frame().state->error);}
void imported_mtoon(Scene&scene){
 auto bytes=source_color_fixture::bytes();uint32_t length=0;for(unsigned k=0;k<4;++k)length|=uint32_t(bytes.at(12+k))<<(8*k);auto j=nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+length);
 j["extensionsUsed"]={"VRMC_materials_mtoon"};j["materials"][0]["extensions"]["VRMC_materials_mtoon"]={{"specVersion","1.0"},{"shadeColorFactor",{1,1,1}},{"shadeMultiplyTexture",{{"index",0}}},{"shadingShiftFactor",0}};
 auto text=j.dump();while(text.size()%4)text.push_back(' ');std::vector<uint8_t>changed;auto word=[&](uint32_t v){for(unsigned k=0;k<4;++k)changed.push_back(uint8_t(v>>(8*k)));};word(0x46546c67);word(2);word(uint32_t(20+text.size()+bytes.size()-20-length));word(uint32_t(text.size()));word(0x4e4f534a);changed.insert(changed.end(),text.begin(),text.end());changed.insert(changed.end(),bytes.begin()+20+length,bytes.end());auto raw=std::make_shared<const std::vector<uint8_t>>(std::move(changed));
 ImportLimits limits;limits.mode=ImportMode::PreserveGltf;auto asset=std::make_shared<const ImportedAsset>(import_glb(*raw,limits));const auto&primitive=asset->primitives.at(0);auto appearance=std::make_shared<CreatorAppearance>();appearance->asset=asset;appearance->original_bytes=raw;appearance->source_mesh=primitive.source_mesh;appearance->source_primitive=primitive.source_primitive;appearance->material=int(primitive.material);appearance->source_material=int(primitive.material)-1;appearance->source_node=0;appearance->material_json=j.at("materials").at(appearance->source_material).dump();appearance->source_basis_quaternion=Quat{};for(uint32_t v=0;v<primitive.mesh.positions.size();++v)appearance->source_vertices.push_back(v);
 auto images=std::make_shared<std::vector<std::shared_ptr<const Image>>>();for(const auto&image:asset->images)images->push_back(std::make_shared<const Image>(decode_projection_png(image.bytes)));appearance->decoded_images=images;auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=images->at(0);maps->normal.image=images->at(1);maps->normal_scale=.8f;for(auto*view:{&maps->color,&maps->normal}){view->wrap_s=view->wrap_t=33071;view->min_filter=view->mag_filter=9729;}
 const auto id=scene.add("Actual imported mixed MToon GLB",Volume{});auto&actor=scene.object(id);actor.mesh_active=true;actor.painted=PaintedMesh::from_maps(primitive.mesh,maps,64);actor.appearance=appearance;actor.transform.position={1.8f,0,0};actor.material.color={.6f,.4f,.2f,1};actor.material.metallic=0;actor.material.roughness=.8f;actor.material.alpha=Material::AlphaMode::Opaque;scene.validate();
}
Pixels framed(SurfaceRenderer&r,std::shared_ptr<const MeshSnapshot>s,bool back=false){
 Vec3 lo=s->mesh.positions.front(),hi=lo;for(auto p:s->mesh.positions){lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);lo.z=std::min(lo.z,p.z);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);hi.z=std::max(hi.z,p.z);}
 const auto middle=(lo+hi)*.5f;const float scale=1.5f/std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z,.01f});Mat4 vp=identity;vp[0]=back?-scale:scale;vp[5]=scale;vp[10]=back?scale:-scale;vp[12]=-middle.x*vp[0];vp[13]=-middle.y*scale;vp[14]=-middle.z*vp[10];
 r.draw(std::move(s),{},vp,ob::Vec3{middle.x,middle.y,middle.z+(back?-4:4)},Fbo::size,Fbo::size,true);return read();
}
void png(const std::filesystem::path&p,const Pixels&b){const auto bytes=Image{Fbo::size,Fbo::size,b}.png();std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
}
int main(int argc,char**argv)try{
 if(argc!=3)return 2;const std::filesystem::path source=argv[1],out=argv[2];std::filesystem::create_directories(out);
 // Produce genuine prior-state files through the same ordinary native Restore.
 for(bool canonical:{false,true})for(bool reflected:{false,true}){const std::string n=std::to_string(canonical)+std::to_string(reflected)+"0";Document d;d.replace(Scene::load(source/n/"accepted.dsculpt"));settled(d);d.restore_polygon_source();settled(d);d.copy_scene().save(out/("prior-"+n+".dsculpt"));}
 NativeGraphics graphics;std::cout<<"GL_RENDERER "<<glGetString(GL_RENDERER)<<std::endl;GLint available=0;glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&available);check(available>=16,"Real GL context supports the core minimum fragment texture units");
 Fbo fbo;SurfaceRenderer renderer;
 for(bool canonical:{false,true})for(bool reflected:{false,true})for(bool flipped:{false,true}){
  const std::string n=std::to_string(canonical)+std::to_string(reflected)+std::to_string(flipped);const auto scene=Scene::load(source/n/"accepted.dsculpt");Document d;d.replace(scene);settled(d);const auto snapshot=d.frame().surface;check(snapshot&&!snapshot->ranges.empty()&&snapshot->ranges.front().textures->polygon_surface,"Loaded ordinary native frame has its genuine current field packet");
  fbo.bind();const auto a=framed(renderer,snapshot,flipped);const auto stats=renderer.texture_stats();const auto buffers=renderer.buffer_stats();size_t visible=0;for(size_t i=3;i<a.size();i+=4)visible+=a[i]>0;check(visible>100,"Actual accepted affine/reflected Move/Flip field draws visible pixels");png(out/(n+".png"),a);
  fbo.bind();check(framed(renderer,snapshot,flipped)==a,"Actual second eye reuses exact current field pixels");check(renderer.texture_stats().uploads==stats.uploads&&renderer.texture_stats().pack_builds==stats.pack_builds&&renderer.buffer_stats().geometry_uploads==buffers.geometry_uploads,"Second eye reuses immutable field/image and geometry uploads");
  if(!flipped){auto mixed=scene.snapshot();imported_mtoon(mixed);Document m;m.replace(mixed);settled(m);fbo.bind();const auto p=framed(renderer,m.frame().surface);png(out/(n+"-mixed.png"),p);const auto st=renderer.texture_stats();fbo.bind();check(framed(renderer,m.frame().surface)==p&&renderer.texture_stats().pack_builds==st.pack_builds,"Mixed polygon/PBR and real MToon/image ranges retain scoped sampler-buffer bindings and stereo reuse");check(m.copy_scene().encode()==mixed.encode(),"Mixed material rendering leaves actual Scene and field ownership unchanged");}
  check(d.copy_scene().encode()==scene.encode(),"Renderer leaves actual accepted source and current paint graph exact");
 }
 check(polygon_programs>=2&&maximum_samplers<=16,"Actual PBR-only and mixed typed polygon programs stay within16 active fragment samplers");check(glGetError()==GL_NO_ERROR,"Actual field and mixed renderer leave no GL error");std::cout<<"PASS "<<checks<<" current polygon field graphical controls; active polygon programs="<<polygon_programs<<" max samplers="<<maximum_samplers<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
