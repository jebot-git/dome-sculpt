// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_reference_main
#include "mtoon-render.cpp"
#undef main
#define main existing_native_chunk_main
#include "sculpt-chunk-pipeline.cpp"
#undef main
#include <cstring>
#include <fstream>
namespace {
bool fail_scalar_upload=false;
bool count_material_setups=false;
size_t material_setups=0;
size_t tangent_defaults=0;
size_t batch_submissions=0,batch_runs=0;
std::shared_ptr<MeshSnapshot> mixed(const Scene &scene,const SculptSceneChunks::Ptr &previous={},bool masks=false,
    const std::shared_ptr<const void> &flat_identity={}) {
  const auto full=reference(scene);auto captured=SculptSceneChunks::capture(scene,{7,1,1,masks,true},previous);
  auto result=std::make_shared<MeshSnapshot>();result->generation=7;result->sculpt_chunks=captured.chunks;
  result->geometry_identity=flat_identity?flat_identity:std::make_shared<const int>(1);
  for(size_t r=0;r<full->ranges.size();++r) {
    const auto &range=full->ranges[r];if(captured.chunks->captures(range.object))continue;
    const auto end=r+1<full->ranges.size()?full->ranges[r+1].first_vertex:full->mesh.positions.size();
    const auto offset=result->mesh.positions.size(),first=result->mesh.indices.size();
    for(size_t v=range.first_vertex;v<end;++v){result->mesh.positions.push_back(full->mesh.positions[v]);result->mesh.normals.push_back(full->mesh.normals[v]);result->mesh.uv.push_back(full->mesh.uv[v]);}
    for(size_t c=range.first;c<range.first+range.count;++c)result->mesh.indices.push_back(uint32_t(offset+full->mesh.indices[c]-range.first_vertex));
    auto current=range;current.first=first;current.first_vertex=offset;result->ranges.push_back(std::move(current));
  }
  return result;
}
void pixel_file(const std::filesystem::path &path,const Pixels &image){std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(image.data()),std::streamsize(image.size()));check(bool(file),"Actual framebuffer evidence saved");}
Pixels render(SurfaceRenderer &renderer,const std::shared_ptr<MeshSnapshot> &scene,std::span<const ObjectInfo> display={},CanvasTransform canvas={}) {
  renderer.draw(scene,canvas,identity,{0,0,2},Fbo::size,Fbo::size,true,false,false,display);return read();
}
}
extern "C" GLint __real_glGetUniformLocation(GLuint,const GLchar*);
extern "C" void __real_glVertexAttrib4f(GLuint,GLfloat,GLfloat,GLfloat,GLfloat);
extern "C" void __wrap_glVertexAttrib4f(GLuint index,GLfloat x,GLfloat y,GLfloat z,GLfloat w) {
  if(count_material_setups&&index==5)++tangent_defaults;
  __real_glVertexAttrib4f(index,x,y,z,w);
}
extern "C" GLint __wrap_glGetUniformLocation(GLuint program,const GLchar *name) {
  if(count_material_setups&&std::strcmp(name,"base_color")==0)++material_setups;
  return __real_glGetUniformLocation(program,name);
}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
extern "C" void __wrap_glBufferData(GLenum target,GLsizeiptr size,const void *data,GLenum usage) {
  if(fail_scalar_upload&&target==GL_ARRAY_BUFFER&&usage==GL_DYNAMIC_DRAW&&size>0){fail_scalar_upload=false;__real_glBufferData(target,-1,data,usage);return;}
  __real_glBufferData(target,size,data,usage);
}
extern "C" void __real_glMultiDrawElementsBaseVertex(GLenum,const GLsizei*,GLenum,const void *const*,GLsizei,const GLint*);
extern "C" void __wrap_glMultiDrawElementsBaseVertex(GLenum mode,const GLsizei *counts,GLenum type,const void *const*offsets,GLsizei count,const GLint *bases) {
  if(count_material_setups){++batch_submissions;batch_runs+=size_t(count);}
  __real_glMultiDrawElementsBaseVertex(mode,counts,type,offsets,count,bases);
}
int main(int argc,char **argv)try{
  check(argc==2,"Usage: sculpt-chunk-render output-directory");const std::filesystem::path output=argv[1];std::filesystem::create_directories(output);
  NativeGraphics graphics;std::cout<<"GPU "<<glGetString(GL_RENDERER)<<'\n';Fbo target;target.bind();SurfaceRenderer expected,actual;
  size_t cases=0;
  for(const auto mode:{Material::AlphaMode::Opaque,Material::AlphaMode::Blend,Material::AlphaMode::Mask})for(const double sign:{1.,-1.}){
    auto scene=fixture();scene.object(1).material=white();scene.object(1).material.color={.8f,.15f,.1f,mode==Material::AlphaMode::Opaque?1.f:.6f};scene.object(1).material.alpha=mode;scene.object(1).material.double_sided=true;
    scene.object(3).material=scene.object(1).material;scene.object(3).material.color={.1f,.2f,.8f,.65f};scene.object(3).transform.position={.12f,.08f,-.04f};
    scene.object(2).material=scene.object(1).material;scene.object(2).material.color={.1f,.8f,.2f,.4f};
    if(sign<0)scene.object(1).affine_edit=std::make_shared<NativeAffineEdit>(AffineTransform(std::array<double,16>{-1,0,0,0,.27,1.2,0,0,.13,.09,.8,0,.1,-.2,.15,1}));
    const auto flat=reference(scene);auto chunked=mixed(scene);auto oracle=render(expected,flat);
    material_setups=0;tangent_defaults=0;batch_submissions=0;batch_runs=0;count_material_setups=true;auto image=render(actual,chunked);count_material_setups=false;
    check(image==oracle,"Actual GL chunk draw equals independent whole-mesher reference for opaque/Blend/Mask and reflected/sheared geometry");
    check(batch_submissions==chunked->sculpt_chunks->objects().size()&&batch_runs>batch_submissions,"Actual ordered run stream is batched once per sculpt object without chunk sorting");
    std::cout<<"ORDERED_BATCHES case="<<cases<<" actual="<<batch_submissions<<" original_runs="<<batch_runs<<'\n';
    const auto setup_limit=chunked->ranges.size()+chunked->sculpt_chunks->objects().size();
    std::cout<<"MATERIAL_SETUP case="<<cases<<" actual="<<material_setups<<" object_range_limit="<<setup_limit<<'\n';
    check(material_setups>0&&material_setups<=setup_limit,"Material state submission is bounded by actual objects/ranges, not ordered sculpt cell runs");
    std::cout<<"CONSTANT_ATTRIBUTES case="<<cases<<" actual="<<tangent_defaults<<" object_range_limit="<<setup_limit+1<<'\n';
    check(tangent_defaults>0&&tangent_defaults<=setup_limit+1,"Disabled-array constants are submitted per object/range rather than every sculpt cell run");
    check(std::any_of(image.begin(),image.end(),[](uint8_t v){return v!=0;}),"Reference and chunk geometry produce a nonblank framebuffer");
    const auto prefix=output/("case-"+std::to_string(cases++));pixel_file(prefix.string()+"-reference.rgba",oracle);pixel_file(prefix.string()+"-chunk.rgba",image);
    const auto transfers=actual.buffer_stats();check(render(actual,chunked)==image&&actual.buffer_stats().geometry_uploads==transfers.geometry_uploads&&actual.buffer_stats().mask_uploads==transfers.mask_uploads,"Second eye reuses actual GPU geometry/scalar packets without a transfer");
    std::vector<ObjectInfo> display;
    for(uint64_t id:{1u,2u,3u}){ObjectInfo info{};info.id=id;info.visible=true;info.ghost=id!=2;display.push_back(info);}
    check(render(actual,chunked,display)==render(expected,flat,display),"Ghost packets retain the original Scene/material queue and triangle order across packed objects");
    check(actual.buffer_stats().geometry_uploads==transfers.geometry_uploads,"Ghost-only changes issue no geometry upload");
    CanvasTransform canvas;canvas.position={.03f,-.04f,.01f};canvas.scale=.9f;
    check(render(actual,chunked,{},canvas)==render(expected,flat,{},canvas)&&actual.buffer_stats().geometry_uploads==transfers.geometry_uploads,"Canvas motion stays in uniforms with reference-equivalent pixels and no geometry upload");
    display[0].visible=false;check(render(actual,chunked,display)==render(expected,flat,display),"Live visibility excludes the same whole object in mixed and independent reference views");
  }
  {
    // The unchanged original grouped-sphere regression: a valid local sphere
    // becomes degenerate if absolute world float positions are materialized.
    Volume sphere(.04f);stamp(sphere,{},.36f);Scene grouped;grouped.active().volume=std::make_shared<Volume>(sphere);
    CanvasTransform group_pose;group_pose.position={.12f,-.04f,.03f};group_pose.scale=1.25f;
    const auto group=grouped.add_group("Original grouped regression",group_pose);grouped.active().group=group;
    CanvasTransform other;other.position={1,0,0};const auto second=grouped.add("Original second sphere",sphere,other);grouped.object(second).group=group;
    grouped.object(1).material=white();grouped.object(second).material=white();grouped.object(second).material.color={.1f,.7f,.2f,1};grouped.select(1);
    check(rejects([&]{reference(grouped)->mesh.validate();}),"Original grouped authored placement reproduces the absolute-world-float collapse without changing its source or guard");
    // Independent whole meshes captured at the origin, then moved through the
    // existing renderer's actual live ObjectInfo placement contract. No invalid
    // world Mesh is used as an oracle for the coordinate representation fix.
    auto origin=grouped.snapshot();for(const auto &object:grouped.objects()){origin.object(object.id).group=0;origin.object(object.id).transform={};}
    auto whole=reference(origin);for(auto &range:whole->ranges){range.object_world={};range.world_frame_valid=true;}
    std::vector<ObjectInfo> display;
    for(const auto &object:grouped.objects()){ObjectInfo item{};item.id=object.id;item.visible=true;item.world_transform=grouped.world_transform(object.id);item.world_transform_valid=true;display.push_back(item);}
    auto chunked=mixed(grouped);Mat4 view=identity;view[0]=.5f;view[12]=-.3f;
    auto draw_group=[&](SurfaceRenderer &r,const std::shared_ptr<MeshSnapshot> &s){r.draw(s,{},view,{0,0,2},Fbo::size,Fbo::size,true,false,false,display);return read();};
    const auto wanted=draw_group(expected,whole),actual_pixels=draw_group(actual,chunked);
    check(actual_pixels==wanted,"Original grouped valid local meshes render exactly like independently whole-meshed objects with actual GPU placements");
    check(std::any_of(actual_pixels.begin(),actual_pixels.end(),[](uint8_t v){return v!=0;}),"Original translated second grouped object no longer prevents a real rendered frame");
    pixel_file(output/"grouped-reference.rgba",wanted);pixel_file(output/"grouped-local.rgba",actual_pixels);
    const auto uploads=actual.buffer_stats().geometry_uploads;
    grouped.object(second).transform.position.x+=.03f;display[1].world_transform=grouped.world_transform(second);
    auto moved=mixed(grouped,chunked->sculpt_chunks,false,chunked->geometry_identity);
    check(moved->sculpt_chunks->find(second)->geometry==chunked->sculpt_chunks->find(second)->geometry,
          "Authored motion retains genuine local geometry ownership");
    check(draw_group(actual,moved)==draw_group(expected,whole)&&actual.buffer_stats().geometry_uploads==uploads,"Authored grouped motion stays in GPU placement with exact whole-reference pixels and no geometry upload");
  }
  auto scene=fixture();scene.object(3).volume=std::make_shared<Volume>(scene.object(1).volume->geometry_snapshot());scene.object(3).transform=scene.object(1).transform;
  scene.object(1).material=white();scene.object(3).material=white();scene.object(3).material.color={.1f,.8f,.2f,1};scene.object(3).volume->fill_mask(.85f);
  auto before=mixed(scene,{},true);const auto baseline=render(actual,before);check(baseline==render(expected,reference(scene,true)),"Original mixed scalar picture equals independently sampled whole-scene mask reference");const auto stats=actual.buffer_stats();
  scene.object(1).volume->fill_mask(.7f);auto after=mixed(scene,before->sculpt_chunks,true,before->geometry_identity);
  check(after->sculpt_chunks->find(1)->placed==before->sculpt_chunks->find(1)->placed&&after->sculpt_chunks->find(3)->placed==before->sculpt_chunks->find(3)->placed,"Coincident objects sharing an actual Volume geometry base retain their independent placed owners through scalar edits");
  const auto changed=render(actual,after);check(changed==render(expected,reference(scene,true)),"Updated scalar picture equals independently sampled whole-scene mask reference");check(changed!=baseline&&actual.buffer_stats().geometry_uploads==stats.geometry_uploads&&actual.buffer_stats().mask_uploads>stats.mask_uploads,"Real protection edit changes pixels with scalar-only GPU uploads and no geometry upload");
  auto older=render(actual,before);check(older==baseline,"A previously retained immutable scalar snapshot remains drawable after a newer packet");
  fail_scalar_upload=true;check(rejects([&]{render(actual,after);})&&!fail_scalar_upload,"A real GL scalar-buffer upload failure is surfaced before metadata acceptance");
  check(render(actual,before)==baseline,"Previous accepted snapshot recovers exact pixels after failed staged scalar upload");
  check(render(actual,after)==changed,"Retry after failed scalar upload draws the exact current accepted pixels");
  auto hidden=scene.snapshot();hidden.object(1).visible=false;hidden.object(3).visible=false;auto reduced=mixed(hidden,after->sculpt_chunks,true);
  const auto full_bytes=actual.buffer_stats().vertex_bytes+actual.buffer_stats().index_bytes;render(actual,reduced);
  check(actual.buffer_stats().vertex_bytes+actual.buffer_stats().index_bytes<full_bytes,"GPU cache prunes actual obsolete/replaced chunk owners");
  check(glGetError()==GL_NO_ERROR,"Renderer leaves no pending GL error after ordinary, ghost, scalar-failure and pruning paths");
  std::cout<<"PASS total "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
