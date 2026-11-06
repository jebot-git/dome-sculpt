// SPDX-License-Identifier: GPL-3.0-or-later
// Actual SOURCEFUL same-object39+40 fields. Oracle inputs come only from the
// authored fixture and independent mathematics, never a product evaluator.
#include "dome/document.hpp"
#include "dome/material-visibility.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/polygon-authoring.hpp"
#include "dome/render.hpp"
#include "dome/source-placement.hpp"
#include "dome/texture-sets.hpp"
#include "graphics.h"
#include "texture-set-field-source.hpp"
#include "texture-set-range.hpp"
#include "polygon-surface-owner.hpp"
#include "polygon-surface-preview.hpp"
#include "texture-set-source-oracle.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <map>
using namespace dome;
using namespace source_oracle;
using nlohmann::json;
namespace {
size_t checks=0;
void check(bool yes,const char*why){if(!yes)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<'\n';}
template<class F>void refuses(F fn,const char*why){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}check(failed,why);}
Vec3 vec(V v){return{float(v.x),float(v.y),float(v.z)};}
V value(Vec3 v){return{v.x,v.y,v.z};}
void ready(Document&d){if(!d.wait_asset(std::chrono::seconds(120))||!d.wait_mesh(std::chrono::seconds(120))||!d.frame().state->error.empty())throw std::runtime_error("Actual sourceful worker: "+d.frame().state->error);}
Image solid(std::array<uint8_t,4>color){Image image{64,64,std::vector<uint8_t>(64*64*4)};for(size_t i=0;i<64*64;++i)std::copy(color.begin(),color.end(),image.rgba.begin()+4*i);return image;}
Mesh mesh(bool flip){
  Mesh m;for(size_t i=0;i<authored.size();++i){m.positions.push_back(vec(authored[i]));m.normals.push_back(vec(artist(i/3,flip)));m.uv.push_back({float(atlas[i][0]),float(atlas[i][1])});}m.indices={0,1,2,3,4,5};return m;
}
struct Fixture{Scene scene;uint64_t object=1;};
Fixture fixture(bool prepared,bool reflected,bool flip){
  Scene initial;auto&o=initial.active();o.mesh_active=true;o.material.color={0,0,0,1};o.material.emissive={0,0,0};o.material.metallic=0;o.material.roughness=.5f;o.material.alpha=Material::AlphaMode::Opaque;o.material.double_sided=true;o.painted=std::make_shared<PaintedMesh>(mesh(flip),64);
  if(prepared){
    BakedTextures bake;bake.maps.color=solid({0,0,0,255});bake.maps.metal_rough=solid({255,128,0,255});bake.maps.emission=solid({0,0,0,255});bake.maps.normal=solid({128,128,255,255});bake.coverage.assign(64*64,255);
    for(int y=0;y<64;++y)for(int x=0;x<64;++x){const auto k=size_t(y*64+x)*4;bake.maps.normal.rgba[k]=uint8_t(95+(x/16)*18);bake.maps.normal.rgba[k+1]=uint8_t(110+(y/32)*30);bake.maps.normal.rgba[k+2]=240;}
    o.painted->install_bake(std::move(bake));auto converted=convert_canonical_normals({o.painted,{},{},{},o.material,{}});o.painted=std::move(converted.paint);o.material_bindings=std::move(converted.material_bindings);
  }
  // Same genuine FLOAT/original39 boundary as texture-set-field-source.cpp;
  // black radiance leaves the independent normal/specular oracle simple.
  PaintSettings ink;ink.front_only=false;ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={0,0,0};Brush brush;brush.from=brush.to={};brush.normal={0,0,1};brush.radius=2;brush.strength=1;o.painted->begin_stroke();o.painted->stamp(brush,ink);check(o.painted->end_stroke()&&o.painted->has_physical_emission(),"Actual source owns real FLOAT emission and coverage");
  if(prepared){
    Document uv;uv.replace(initial);ready(uv);uv.begin_uv();check(uv.wait_uv(),"Real before-role UV workspace opens");
    uv.edit_uv(UvAction::Scale,.8f);check(uv.wait_uv(),"Primary source island gets a genuine different current atlas");uv.edit_uv(UvAction::Next);check(uv.wait_uv(),"Actual UV selection moves to the second disconnected island");uv.edit_uv(UvAction::Scale,.8f);check(uv.wait_uv(),"Additional source island gets a genuine different current atlas");uv.end_uv(true);ready(uv);initial=uv.copy_scene();
  }
  initial.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(World{reflected}.matrix()));
  const auto before=initial.snapshot();const auto owner=detail::PolygonSurfaceOwner::capture(before,initial.active().id);PolygonWorkspace work(*owner->trace().original());work.mode(PolygonSelectionMode::Vertex);
  for(size_t t=0;t<2;++t){const auto p=vec(authored[3*t+2]);auto at=std::find_if(work.view().topology->vertices().begin(),work.view().topology->vertices().end(),[&](const auto&v){return length(v.position-p)<1e-6f;});check(at!=work.view().topology->vertices().end(),"Actual topology identifies the independently authored moved vertex");work.select(at->id);work.begin_move();work.move_to({0,0,float(height[t])});work.end_move(true);}
  if(flip){work.mode(PolygonSelectionMode::Face);work.select_all();work.apply(PolygonOperation::FlipFaces);}
  check(detail::adopt_polygon_surface_edit(initial,initial.active().id,work.view(),before),"Actual Move/Flip replay publishes its genuine39 source owner");
  // Reuse the real detached partition + actual create_texture_set route from
  // texture-set-field-source.cpp. No test constructs a domain or authority.
  const auto fields=detail::prepare_texture_set_field_sources(initial.active(),std::array<TextureSetId,2>{0,1});check(fields.size()==2,"One actual39 actor partitions into two real sourceful fields");
  for(size_t t=0;t<2;++t){const auto&f=fields[t];f->authenticate_owner(f);check(f->field().domain->triangles().size()==1&&f->field().domain->contains(uint32_t(t)),"Each detached source field owns only its genuine logical triangle");refuses([&]{f->sample(uint32_t(1-t),{.2,.3,.5});},"Source field refuses a sibling's logical triangle");}
  const auto face=fields[1]->field().domain->topology()->triangle_faces()[1];check(create_texture_set(initial,initial.active().id,std::array<uint64_t,1>{face},"Additional retained source",64)==1,"Public collection creation retains the actual second source face");select_texture_set(initial,initial.active().id,1);initial.validate();
  const auto wire=initial.encode();check(wire.at("version")==40&&wire.at("texture_set_geometry").at("version")==2,"One Scene40 actor owns genuine sourceful geometry2 fields");
  auto scene=Scene::decode(wire);check(scene.encode()==wire,"Actual sourceful39/40 grids, fields and before roots survive codec replay");
  const auto&a=scene.active();check(scene.objects().size()==1&&a.texture_sets->primary_source()&&a.texture_sets->at(1).source&&a.painted->mesh_owner()==a.texture_sets->at(1).painted->mesh_owner(),"Both sources belong to one actual object and one shared geometry control block");const auto object=a.id;return{std::move(scene),object};
}
struct Fbo{
  GLuint f=0,c=0,d=0;
  Fbo(){glGenFramebuffers(1,&f);glBindFramebuffer(GL_FRAMEBUFFER,f);glGenTextures(1,&c);glBindTexture(GL_TEXTURE_2D,c);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,side,side,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,c,0);glGenRenderbuffers(1,&d);glBindRenderbuffer(GL_RENDERBUFFER,d);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,side,side);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,d);check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Independent sourceful target is complete");}
  ~Fbo(){glDeleteRenderbuffers(1,&d);glDeleteTextures(1,&c);glDeleteFramebuffers(1,&f);}
};
using Pixels=std::vector<uint8_t>;
Pixels render(SurfaceRenderer&r,std::shared_ptr<const MeshSnapshot>s,Fbo&f,std::span<const ObjectInfo>display={},CanvasTransform canvas={}){
  glBindFramebuffer(GL_FRAMEBUFFER,f.f);const Mat4 vp{float(projection_scale),0,0,0,0,float(projection_scale),0,0,0,0,-.2f,0,0,0,0,1};r.set_frame_time(0);r.draw(std::move(s),canvas,vp,{0,0,4},side,side,true,false,false,display);Pixels p(side*side*4);glReadPixels(0,0,side,side,GL_RGBA,GL_UNSIGNED_BYTE,p.data());if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Sourceful field GL draw/read failed");return p;
}
void png(const std::filesystem::path&p,const Pixels&pixels){const auto bytes=Image{side,side,pixels}.png();std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));check(bool(f),"Actual sourceful frame saved");}
void state_contract(){GLint front=0,depth=0,program=0,vao=0,active=0;GLboolean write=false;glGetIntegerv(GL_FRONT_FACE,&front);glGetIntegerv(GL_DEPTH_FUNC,&depth);glGetBooleanv(GL_DEPTH_WRITEMASK,&write);glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);check(front==GL_CCW&&depth==GL_LESS&&write&&!program&&!vao&&active==GL_TEXTURE0&&!glIsEnabled(GL_BLEND)&&!glIsEnabled(GL_CULL_FACE)&&!glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE),"Field/ghost/cull programs leave the ordinary renderer state contract");}
void verify_streams(const Fixture&f,const MeshSnapshot&s,bool prepared,bool reflected,bool flip){
  check(s.ranges.size()==2&&s.picking&&s.mesh.indices.size()==6,"Actual first sourceful object publishes exactly two complete field ranges");std::array<bool,2> seen{};const World world{reflected};const auto wanted=triangles(flip,world);
  for(const auto&r:s.ranges){check(r.object==f.object&&r.texture_set<2&&!seen[r.texture_set]&&r.texture_set_proof&&r.count==3,"Ranges distinguish primary/additional material on the same actual object");seen[r.texture_set]=true;r.texture_set_proof->authenticate_stream(s.mesh,r);const auto&field=r.texture_set_proof->field();
    check(field.source&&field.domain->contains(r.texture_set)&&field.domain->triangles().size()==1,"Each draw retains actual sourceful member ownership");field.source->authenticate_owner(field.source);
    check(r.textures&&r.textures->polygon_surface==field.source->source_preview()&&r.source_frame&&r.source_normal_only==!prepared&&r.paint_winding_reversed==reflected,"Both material fields retain their genuine raw-input packet and winding");
    check(r.textures->polygon_surface->model()==(prepared?detail::PolygonSurfaceNormalModel::PreparedCorePbr:detail::PolygonSurfaceNormalModel::NativeNOnly),"Sourceful normal model comes from its real before N/T owner");
    if(!prepared)check(r.textures->polygon_surface->tangents().empty()&&!field.source->sample(r.texture_set,{.2,.3,.5}).normal_uv,"N-only source does not invent artist T/W or a normal role UV");
    const auto&t=wanted[r.texture_set];V center{};
    for(size_t c=0;c<3;++c){const auto v=s.mesh.indices.at(r.first+c);size_t k=0;while(k<3&&length(s.mesh.positions.at(v)-vec(t.p[k]))>2e-5f)++k;check(k<3,"Packed corner matches independent edited/reflected source geometry");const auto id=t.ids[k];center=center+t.p[k]*(1./3);
      check(length(s.source_normals.at(v)-vec(world.normal(artist(r.texture_set,flip))))<2e-5f,"Packed before N belongs to the correct source triangle");
      const auto old=atlas[id];check(s.paint_coordinates.at(v)==std::array<float,2>{float(old[0]),float(old[1])},"Sourceful paint uses original logical corners rather than current atlas");
      const auto logical=r.texture_set_proof->logical_corner(c,r.paint_winding_reversed);check(logical/3==r.texture_set,"Actual field proof keeps its shared logical triangle identity");
      if(prepared){const auto N=artist(r.texture_set,flip),T=unit(world.direction(unit(V{1,0,0}-N*N.x)));const auto raw=s.source_tangents.at(v);check(length(Vec3{raw[0],raw[1],raw[2]}-vec(T))<2e-5f&&raw[3]==(reflected?-1:1),"Packed before T/W keeps actual original/reflected role inputs");check(s.normal_coordinates.at(v)==std::array<float,4>{float(old[0]),float(old[1]),float(old[0]),float(old[1])},"Normal image uses independent original role UVs");const auto now=current_uv(id);check(std::abs(s.mesh.uv.at(v)[0]-now[0])<2e-5&&std::abs(s.mesh.uv.at(v)[1]-now[1])<2e-5&&s.mesh.uv.at(v)!=s.paint_coordinates.at(v),"Genuinely edited per-island UV0 remains separate from paint/normal roles");}
    }
    const auto hit=s.picking->raycast_target(vec(center+V{0,0,2}),{0,0,-1},3,0,f.object);check(hit&&hit->packed()&&hit->object==f.object&&s.picking->material_identity(hit->surface.triangle)==SurfaceMaterialIdentity{f.object,r.texture_set}&&s.picking->logical_triangle(hit->surface.triangle)==r.texture_set,"Actual query keeps same-object material and shared logical triangle identity");
  }
  check(seen[0]&&seen[1],"Selecting the additional source never removes the primary field");
}
enum class Appearance{Normal,RedGreen,RedBlue};
json verify_pixels(const Pixels&p,bool prepared,bool flip,const World&w,Appearance mode,bool ghost=false,bool cull_additional=false){
  const auto expected=triangles(flip,w);std::array<size_t,2> count{};size_t outside=0,culled=0;int maximum=0,normal_counter=0,uv_counter=0,frame_counter=0,member_counter=0;
  for(int y=0;y<side;++y)for(int x=0;x<side;++x){const double wx=((x+.5)*2/side-1)/projection_scale,wy=((y+.5)*2/side-1)/projection_scale;const auto off=size_t(y*side+x)*4;bool near=false,interior=false;
    for(const auto&t:expected){const auto b=barycentric(t,wx,wy);const auto minimum=*std::min_element(b.begin(),b.end());if(minimum>=-.04)near=true;if(minimum<.12)continue;if(interior)throw std::runtime_error("Independent source triangle interiors overlap");interior=true;++count[t.set];
      std::array<int,4> wanted{};
      if(cull_additional&&t.set==1&&flip){++culled;}
      else if(mode==Appearance::Normal){V point{};for(size_t k=0;k<3;++k)point=point+t.p[k]*b[k];wanted=finish(specular(source_oracle::normal(t.set,b,prepared,flip,w),point),ghost);
        const auto wrong_n=finish(specular(source_oracle::normal(t.set,b,prepared,flip,w,false,true),point),ghost),wrong_frame=finish(specular(source_oracle::normal(t.set,b,prepared,flip,w,false,false,true),point),ghost);
        for(size_t k=0;k<3;++k){normal_counter=std::max(normal_counter,std::abs(wanted[k]-wrong_n[k]));frame_counter=std::max(frame_counter,std::abs(wanted[k]-wrong_frame[k]));}
        if(prepared){const auto wrong=finish(specular(source_oracle::normal(t.set,b,true,flip,w,true),point),ghost);for(size_t k=0;k<3;++k)uv_counter=std::max(uv_counter,std::abs(wanted[k]-wrong[k]));}
      }else{
        const V color=t.set==0?V{1,0,0}:mode==Appearance::RedGreen?V{0,1,0}:V{0,0,1};wanted=finish(color,ghost);const auto wrong=finish(t.set==0?(mode==Appearance::RedGreen?V{0,1,0}:V{0,0,1}):V{1,0,0},ghost);for(size_t k=0;k<3;++k)member_counter=std::max(member_counter,std::abs(wanted[k]-wrong[k]));
      }
      for(size_t k=0;k<4;++k){const int error=std::abs(int(p[off+k])-wanted[k]);maximum=std::max(maximum,error);if(error>(mode==Appearance::Normal?4:2))throw std::runtime_error("Independent sourceful pixel mismatch x="+std::to_string(x)+" y="+std::to_string(y)+" set="+std::to_string(t.set)+" channel="+std::to_string(k)+" actual="+std::to_string(p[off+k])+" expected="+std::to_string(wanted[k]));}
    }
    if(!near){++outside;if(p[off+3])throw std::runtime_error("Sourceful silhouette outside authored geometry");}
  }
  check(count[0]>400&&count[1]>400&&outside>side*side/2,"Both genuine source fields have substantial independent pixel interiors and background");
  if(mode==Appearance::Normal){check(normal_counter>=6,"Independent normal oracle rejects edited-geometric-only shading");check(frame_counter>=6,"Independent field oracle rejects borrowing sibling geometric transport rows");if(prepared)check(uv_counter>=6,"Asymmetric normal oracle rejects current UV0 in place of original role coordinates");}
  else check(member_counter>=40,"Independent paint oracle rejects swapped same-object material bindings");
  if(cull_additional&&flip)check(culled>400,"Additional-only backface cull retains a real independent blank mask");
  return{{"primary_pixels",count[0]},{"additional_pixels",count[1]},{"background_pixels",outside},{"culled_pixels",culled},{"maximum_byte_error",maximum},{"wrong_normal_counter",normal_counter},{"wrong_role_uv_counter",uv_counter},{"wrong_sibling_frame_counter",frame_counter},{"wrong_member_paint_counter",member_counter}};
}
void paint_member(Scene&scene,TextureSetId set,std::array<float,4>color){
  check(edit_texture_set(scene,scene.active().id,set,[&](PaintedMesh&paint,Material&material){material.unlit=true;material.alpha=Material::AlphaMode::Opaque;material.double_sided=true;paint.add_layer("Independent member color");PaintSettings ink;ink.front_only=false;ink.channel=PaintChannel::Color;ink.color=color;Brush brush;brush.from=brush.to={};brush.normal={0,0,1};brush.radius=5;brush.strength=1;paint.begin_stroke();for(size_t i=0;i<32;++i)paint.stamp(brush,ink);return paint.end_stroke();}),"Actual sourceful field receives a finished saturated native color layer");
}
std::vector<ObjectInfo> policy(Document&doc,bool ghost){auto display=doc.frame().state->objects;for(auto&v:display){v.selected=v.active=v.hovered=false;v.ghost=ghost;}return display;}
}
int main(int argc,char**argv)try{
  if(argc!=2)return 2;
  const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);NativeGraphics graphics;std::cout<<"GL_RENDERER "<<glGetString(GL_RENDERER)<<'\n';Fbo target;SurfaceRenderer renderer;json evidence=json::array();
  for(bool prepared:{false,true})for(bool reflected:{false,true})for(bool flip:{false,true}){
    const auto name=std::to_string(prepared)+std::to_string(reflected)+std::to_string(flip);auto f=fixture(prepared,reflected,flip);const auto exact=f.scene.encode();Document doc;doc.replace(f.scene);ready(doc);auto base=doc.frame().surface;verify_streams(f,*base,prepared,reflected,flip);const World world{reflected};
    auto normal_pixels=render(renderer,base,target);png(out/(name+"-normal.png"),normal_pixels);auto normal_metric=verify_pixels(normal_pixels,prepared,flip,world,Appearance::Normal);const auto uploads=renderer.buffer_stats();const auto textures=renderer.texture_stats();
    check(render(renderer,base,target)==normal_pixels&&renderer.buffer_stats().geometry_uploads==uploads.geometry_uploads&&renderer.texture_stats().uploads==textures.uploads&&renderer.texture_stats().pack_builds==textures.pack_builds,"Second eye reuses exact sourceful fields/geometry/images");state_contract();
    auto corrupt=std::make_shared<MeshSnapshot>(*base);corrupt->ranges[0].texture_set_proof=base->ranges[1].texture_set_proof;refuses([&]{render(renderer,corrupt,target);},"Renderer refuses borrowing a sibling field proof on the same object");check(render(renderer,base,target)==normal_pixels,"Accepted sourceful frame recovers exactly after a foreign-proof refusal");
    CanvasTransform canvas;canvas.position={-.03f,.02f,0};canvas.scale=.9f;const auto canvas_stats=renderer.buffer_stats();auto canvas_pixels=render(renderer,base,target,{},canvas);auto canvas_metric=verify_pixels(canvas_pixels,prepared,flip,World{reflected,0,.9,{-.03,.02,0}},Appearance::Normal);check(renderer.buffer_stats().geometry_uploads==canvas_stats.geometry_uploads,"Canvas field transport remains uniforms without geometry upload");
    doc.configure_object_grab(true,false);ready(doc);check(doc.begin_object_grab(f.object),"Actual sourceful object enters the supported held route");const auto held=doc.frame().surface;check(render(renderer,held,target,policy(doc,false))==normal_pixels,"Actual held entry retains the original field appearance");const auto held_stats=renderer.buffer_stats();CanvasTransform delta;delta.position={.08f,.04f,.05f};delta.scale=.9f;delta.orientation={0,float(std::sin(.09)),0,float(std::cos(.09))};doc.move_held_object_world(delta);check(doc.frame().surface==held,"Held sourceful motion retains its actual immutable geometry snapshot");
    auto display=policy(doc,false);const World moved{reflected,.18,.9,{.08,.04,.05}};check(display.size()==1&&display[0].affine_world.has_value(),"Actual held state retains its complete affine source placement");const auto actual_matrix=display[0].affine_world->matrix(),wanted_matrix=moved.matrix();for(size_t i=0;i<16;++i)check(std::abs(actual_matrix[i]-wanted_matrix[i])<2e-5,"Live held placement matches independent affine composition");
    auto held_pixels=render(renderer,held,target,display);auto held_metric=verify_pixels(held_pixels,prepared,flip,moved,Appearance::Normal);png(out/(name+"-held.png"),held_pixels);check(renderer.buffer_stats().geometry_uploads==held_stats.geometry_uploads,"Held field transport remains uniforms without geometry upload");doc.end_object_grab(false);ready(doc);check(doc.copy_scene().encode()==exact,"Held cancellation preserves complete sourceful native state");
    auto painted=f.scene.snapshot();paint_member(painted,0,{1,0,0,1});paint_member(painted,1,{0,1,0,1});Document colors;colors.replace(painted);ready(colors);auto green=colors.frame().surface;auto green_pixels=render(renderer,green,target);auto green_metric=verify_pixels(green_pixels,prepared,flip,world,Appearance::RedGreen);png(out/(name+"-red-green.png"),green_pixels);
    const auto primary_before=texture_set_view(painted.active(),0);paint_member(painted,1,{0,0,1,1});const auto primary_after=texture_set_view(painted.active(),0);check(primary_before.painted==primary_after.painted&&primary_before.source==primary_after.source,"Additional source paint preserves actual primary COW/source ownership");Document changed;changed.replace(painted);ready(changed);auto blue=changed.frame().surface;auto blue_pixels=render(renderer,blue,target);auto blue_metric=verify_pixels(blue_pixels,prepared,flip,world,Appearance::RedBlue);png(out/(name+"-red-blue.png"),blue_pixels);check(render(renderer,green,target)==green_pixels,"Retained previous same-object fields remain exactly drawable after sibling edit");
    auto ghost_display=policy(changed,true);auto ghost_pixels=render(renderer,blue,target,ghost_display);auto ghost_metric=verify_pixels(ghost_pixels,prepared,flip,world,Appearance::RedBlue,true);png(out/(name+"-ghost.png"),ghost_pixels);state_contract();
    auto culling=painted.snapshot();check(edit_texture_set(culling,f.object,1,[](PaintedMesh&,Material&m){m.double_sided=false;return true;}),"Actual additional material alone changes cull policy");Document cull;cull.replace(culling);ready(cull);auto culled_pixels=render(renderer,cull.frame().surface,target);auto cull_metric=verify_pixels(culled_pixels,prepared,flip,world,Appearance::RedBlue,false,true);png(out/(name+"-additional-cull.png"),culled_pixels);state_contract();check(render(renderer,blue,target)==blue_pixels&&render(renderer,base,target)==normal_pixels,"Opaque and normal fields recover exact state after ghost/additional-only cull");
    const auto wire=painted.encode();Document reopened;reopened.replace(Scene::decode(wire));ready(reopened);check(render(renderer,reopened.frame().surface,target)==blue_pixels&&reopened.copy_scene().encode()==wire,"Edited sourceful FLOAT/paint/normal fields roundtrip with exact pixels and native state");check(doc.copy_scene().encode()==exact&&f.scene.encode()==exact,"All GPU/read/negative controls preserve the complete original sourceful Scene");
    evidence.push_back({{"case",name},{"normal",normal_metric},{"canvas",canvas_metric},{"held",held_metric},{"red_green",green_metric},{"red_blue",blue_metric},{"ghost",ghost_metric},{"additional_cull",cull_metric}});
  }
  std::ofstream report(out/"oracle.json");report<<json{{"scope","independent actual sourceful same-object39+40 appearance"},{"checks",checks},{"cases",evidence}}.dump(2)<<'\n';check(bool(report),"Independent sourceful per-pixel metrics saved");std::cout<<"PASS "<<checks<<" actual sourceful same-object field appearance controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
