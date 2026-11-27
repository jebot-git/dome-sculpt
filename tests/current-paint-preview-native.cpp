// SPDX-License-Identifier: GPL-3.0-or-later
#include "current-paint-preview-fixture.hpp"
#include "dome/material-visibility.hpp"
#include "dome/source-maps.hpp"
#include "dome/texture-sets.hpp"
#include "texture-set-worker.hpp"
#include <atomic>
#include <unistd.h>
using namespace current_fixture;
namespace {std::atomic_bool auditing=false;std::atomic_size_t materializations=0;}
std::shared_ptr<const TextureMaps> real_preview(const PaintedMesh*,const Material&,const std::function<bool()>&) asm("__real__ZNK4dome11PaintedMesh7previewERKNS_8MaterialERKSt8functionIFbvEE");
std::shared_ptr<const TextureMaps> wrapped_preview(const PaintedMesh*,const Material&,const std::function<bool()>&) asm("__wrap__ZNK4dome11PaintedMesh7previewERKNS_8MaterialERKSt8functionIFbvEE");
std::shared_ptr<const TextureMaps> wrapped_preview(const PaintedMesh*p,const Material&m,const std::function<bool()>&c){if(auditing)++materializations;return real_preview(p,m,c);}
std::shared_ptr<const TextureMaps> real_emission(const PaintedMesh&,const Material&,const std::function<bool()>&) asm("__real__ZN4dome6detail20EmissionExportAccess7previewERKNS_11PaintedMeshERKNS_8MaterialERKSt8functionIFbvEE");
std::shared_ptr<const TextureMaps> wrapped_emission(const PaintedMesh&,const Material&,const std::function<bool()>&) asm("__wrap__ZN4dome6detail20EmissionExportAccess7previewERKNS_11PaintedMeshERKNS_8MaterialERKSt8functionIFbvEE");
std::shared_ptr<const TextureMaps> wrapped_emission(const PaintedMesh&p,const Material&m,const std::function<bool()>&c){if(auditing)++materializations;return real_emission(p,m,c);}
namespace {
void production_texture_sets(){
 constexpr uint32_t resolution=1024;
 Mesh mesh;
 mesh.positions={{-.8f,-.8f,0},{.8f,-.8f,0},{0,.8f,0},
                 {-.8f,-.8f,.5f},{.8f,-.8f,.5f},{0,.8f,.5f}};
 mesh.normals.assign(6,{0,0,1});
 mesh.uv={{{.1f,.1f}},{{.9f,.1f}},{{.5f,.9f}},{{.1f,.1f}},{{.9f,.1f}},{{.5f,.9f}}};
 mesh.indices={0,1,2,3,4,5};
 Scene scene;
 const auto id=scene.active().id;
 std::array<TextureSetInitialField,2> fields;
 for(auto&field:fields)field.resolution=resolution;
 fields[1].material.color[3]=.75f;
 fields[1].material.alpha=Material::AlphaMode::Mask;
 initialize_texture_sets(scene,id,mesh,fields,std::array<TextureSetId,2>{0,1});
 for(TextureSetId set:{0u,1u}){
  check(edit_texture_set(scene,id,set,[set](PaintedMesh&paint,Material&){
   PaintSettings ink;
   ink.channel=PaintChannel::Emission;
   ink.emission_units=PaintEmissionUnits::Physical;
   ink.emission=set?std::array<float,3>{.2f,8.f,.4f}:std::array<float,3>{5.f,.2f,.1f};
   const float z=set?.5f:0.f;
   stroke(paint,ink,{.12f,.09f,z},.8f,.035f);
   ink.channel=PaintChannel::Color;
   ink.color={.2f,.5f,.1f,.7f};
   stroke(paint,ink,{-.15f,-.07f,z},.8f,.045f);
   return true;
  }),"Production fixture authors actual color and FLOAT emission in each member");
 }
 select_texture_set(scene,id,1);
 const auto exact=scene.encode();
 auto path=std::filesystem::temp_directory_path()/("current-grid-texture-sets-"+std::to_string(getpid())+".dsculpt");
 scene.save(path);
 Document doc;
 const auto before_materializations=materializations.load();
 auditing=true;
 doc.open_async(path).get();
 std::filesystem::remove(path);
 check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Cold native Open publishes both 1024-square paint fields within default limits");
 const auto frame=doc.frame().surface;
 check(frame&&frame->ranges.size()==2&&frame->picking,"Native frame publishes both material ranges and authenticated picking");
 for(const auto&range:frame->ranges){
  const auto&packet=range.textures->current_grid_preview;
  check(packet&&packet->physical_emission()&&packet->layers()->resolution()==resolution,
        "Each actual member has an independent sparse full-resolution FLOAT preview");
  packet->authenticate_range(frame->mesh,range,true);
  check(packet->alpha_texel(0,0)==fields[range.texture_set].material.color[3],
        "Unpainted padding retains the exact base alpha of its own material");
  check(packet->transparent(),"Both sparse painted alpha and translucent base are classified for blending");
 }
 check(doc.copy_scene().encode()==exact,"Cold Open keeps the exact native data and active texture set");
 PaintSettings ink;
 ink.color={.9f,.2f,.6f,1};
 ink.front_only=false;
 doc.paint_settings(ink);
 doc.configure(Tool::Clay,.06f);
 NativeStrokeSample sample;
 sample.origin={0,0,2};
 sample.direction={0,0,-1};
 sample.brush.radius=.06f;
 sample.brush.pressure=1;
 sample.brush.strength=1;
 check(doc.begin_native_stroke()&&doc.sample_native_stroke(sample),"Desktop paint ray begins on the selected full-resolution member");
 doc.end_native_stroke(true);
 auto save=doc.save_async(path);
 check(doc.wait_native_stroke()&&doc.wait_mesh()&&doc.frame().state->error.empty(),
       "Selected-set stroke finishes without dense field materialization");
 save.get();
 auditing=false;
 check(materializations==before_materializations,"Cold preview and desktop stroke both avoid dense CPU map generation");
 const auto accepted=doc.copy_scene().encode();
 check(accepted!=exact&&Scene::load(path).encode()==accepted,"Ordered Save preserves exact accepted texture-set stroke");
 std::filesystem::remove(path);
 check(doc.undo()&&doc.copy_scene().encode()==exact,"One Undo restores both original members");
 check(doc.redo()&&doc.copy_scene().encode()==accepted,"One Redo restores the exact painted result");
 check(doc.wait_mesh(),"Production texture-set history worker drains");
}
}
int main()try{
 set_paint_composite_cache_budget(0);
 for(bool physical:{false,true}){
  auto scene=fixture(64,physical);auto original=scene.encode();Document d;auditing=true;auto frame=published(d,scene);auditing=false;
  check(materializations==0,"Actual render producer selects sparse packet before any legacy preview/HDR materialization");auto p=owner(*frame);const auto&maps=*frame->ranges.front().textures;
  check(maps.color.rgba.empty()&&maps.metal_rough.rgba.empty()&&maps.emission.rgba.empty()&&!maps.overlay&&!maps.source_maps,"Render-only packet neither synthesizes CPU maps nor borrows source/emission roles");
  check(p->physical_emission()==physical,"Genuine FLOAT presence selects the independent physical emission interpretation");p->authenticate_range(frame->mesh,frame->ranges.front(),true);
  auto refs=physical?detail::EmissionExportAccess::other_channels(*scene.active().painted,scene.active().material,{}):reference(scene);
  check(p->transparent()==refs->transparent,"Current alpha classification preserves exact legacy material blend ordering");const auto r=scene.active().painted->resolution();for(uint32_t y=0;y<r;++y)for(uint32_t x=0;x<r;++x)check(std::lround(std::clamp(p->alpha_texel(x,y),0.f,1.f)*255)==refs->color.rgba[(size_t(y)*r+x)*4+3],"Sparse native alpha matches actual old byte-image alpha for every texel");
  auto before=frame->ranges.front();auto stale=before;auto flag_copy=std::make_shared<TextureMaps>(*before.textures);flag_copy->transparent=!p->transparent();stale.textures=flag_copy;check(refuses([&]{p->authenticate_range(frame->mesh,stale,true);}),"Legal mutable map copy cannot change sealed current-grid transparency or blend ordering");stale=before;stale.object++;check(refuses([&]{p->authenticate_range(frame->mesh,stale,true);}),"Foreign actor identity refuses sparse packet");stale=before;stale.material.color[0]+=.01f;check(refuses([&]{p->authenticate_range(frame->mesh,stale,true);}),"Changed material factors refuse stale packet");auto geometry=frame->mesh;geometry.uv.front()[0]+=.001f;check(refuses([&]{p->authenticate_range(geometry,before,true);}),"Changed ordered native UVs refuse sparse packet");
  auto borrowed=std::shared_ptr<const detail::CurrentGridPaintPreview>(p.get(),[](auto*){});check(refuses([&]{p->authenticate_owner(borrowed);}),"Forged packet control block refuses authentication");
  for(float u:{-.1f,.08f,.2345f,.5f,.78123f,1.1f})for(float v:{.01f,.125f,.5123f,.9f,1.1f}){float x=std::clamp(u,0.f,1.f)*r-.5f,y=std::clamp(v,0.f,1.f)*r-.5f,fx=x-std::floor(x),fy=y-std::floor(y),expected=0;for(int k=0;k<4;++k){int px=std::clamp(int(std::floor(x))+(k&1),0,int(r)-1),py=std::clamp(int(std::floor(y))+(k>>1),0,int(r)-1);expected+=((k&1)?fx:1-fx)*((k>>1)?fy:1-fy)*(refs->color.rgba[(size_t(py)*r+px)*4+3]/255.f);}check(std::abs(p->sample_alpha(u,v)-expected)<1e-7f,"Independent legacy-image bilinear alpha agrees with picking sampler");}
  auto current_range=before,old_range=before;current_range.material.alpha=old_range.material.alpha=Material::AlphaMode::Mask;current_range.material.alpha_cutoff=old_range.material.alpha_cutoff=.6f;old_range.textures=refs;const std::array<SurfaceRange,1> current_ranges{current_range},old_ranges{old_range};MaterialSurfaceQuery current(frame->mesh,current_ranges),old(frame->mesh,old_ranges);for(float x:{-.7f,-.3f,.0f,.3f,.7f})for(float y:{-.7f,-.2f,.2f,.7f})check(bool(current.raycast({x,y,2},{0,0,-1},4))==bool(old.raycast({x,y,2},{0,0,-1},4)),"Actual material query has identical native mask coverage");
  auto again=detail::CurrentGridPaintPreview::for_render(scene.active());check(again->current_grid_preview->contribution_owner()==p->contribution_owner(),"Unchanged actor reuses exact immutable contribution key");
  auto alternate=scene.active();alternate.id++;alternate.material.color[1]=.13f;auto other=detail::CurrentGridPaintPreview::for_render(alternate);check(other->current_grid_preview->contribution_owner()!=p->contribution_owner(),"Different actual material retains a distinct contribution");check(detail::CurrentGridPaintPreview::for_render(scene.active())->current_grid_preview->contribution_owner()==p->contribution_owner(),"Alternate actor/material does not invalidate unchanged cache key");
  if(!physical){auto public_maps=scene.active().painted->preview(scene.active().material);check(!public_maps->color.rgba.empty()&&!public_maps->current_grid_preview,"Existing public PaintedMesh preview retains real CPU images");auto surfaces=scene.preview_surfaces();check(!surfaces.front().textures->color.rgba.empty()&&!surfaces.front().textures->current_grid_preview,"Existing public Scene preview/export surfaces retain real CPU images");}
  check(scene.encode()==original&&d.copy_scene().encode()==original,"Capturing current render packets leaves every native field and metadata unchanged");
  PaintSettings ink;ink.color={.9f,.1f,.3f,.41f};d.paint_settings(ink);check(d.begin_native_stroke(),"Actual native paint gesture starts");NativeStrokeSample sample;sample.origin={0,0,2};sample.direction={0,0,-1};sample.brush.radius=.19f;sample.brush.strength=.7f;sample.brush.pressure=1;d.sample_native_stroke(sample);d.end_native_stroke();
  auto path=std::filesystem::temp_directory_path()/("current-grid-preview-"+std::to_string(getpid())+".dsculpt");d.save_async(path).get();check(d.wait_native_stroke()&&d.wait_mesh(),"Native gesture and ordered Save drain before current preview publication");check(d.frame().state->error.empty(),"Current-grid native update reports no error");auto endpoint=d.copy_scene().encode();check(endpoint!=original&&Scene::load(path).encode()==endpoint,"Ordered Save/reopen persists actual native layer result");check(frame->ranges.front().textures->current_grid_preview==p&&scene.encode()==original,"Held prior frame/native owner remains immutable after edit");
  check(d.undo()&&d.copy_scene().encode()==original&&d.wait_mesh(),"One Undo restores complete native source");check(d.redo()&&d.copy_scene().encode()==endpoint&&d.wait_mesh(),"One Redo restores exact native endpoint");check(d.begin_native_stroke(),"Native cancel gesture starts");sample.origin.x=.15f;d.sample_native_stroke(sample);d.end_native_stroke(false);check(d.wait_native_stroke()&&d.copy_scene().encode()==endpoint,"Cancel preserves source and native history");std::filesystem::remove(path);
 }
 {
  Scene sets;std::array<TextureSetInitialField,2> fields;fields[0].resolution=64;fields[1].resolution=128;fields[0].material.color={.7f,.1f,.2f,.23f};fields[1].material.color={.1f,.3f,.8f,.81f};initialize_texture_sets(sets,sets.active().id,quad(),fields,std::array<TextureSetId,2>{0,1});const auto exact=sets.encode();Document d;auditing=true;auto frame=published(d,sets);auditing=false;check(materializations==0&&frame->ranges.size()==2,"Actual multi-resolution texture-set render publisher avoids eager dense images");
  for(const auto&r:frame->ranges){check(r.textures->current_grid_preview&&r.texture_set_proof,"Actual plain set owns both independent sparse packet and genuine field/range proof");r.textures->current_grid_preview->authenticate_range(frame->mesh,r,true);r.texture_set_proof->authenticate_stream(frame->mesh,r);auto old=detail::prepare_texture_set_range(r.texture_set_proof->plan(),r.texture_set_proof->geometry_layout(),r.texture_set,0,0);check(!old->textures()->current_grid_preview&&!old->textures()->color.rgba.empty(),"Default reference/export texture-set preparation still materializes real images");}
  check(sets.encode()==exact&&d.copy_scene().encode()==exact,"Multi-set render packets preserve all distinct resolutions/material/native owners");
 }
 auto sourced=fixture();auto source=std::make_shared<PaintSourceMaps>();source->color.image=std::make_shared<const Image>(Image{1,1,{1,2,3,4}});sourced.active().painted=PaintedMesh::from_maps(quad(),source,64);check(!detail::CurrentGridPaintPreview::eligible(sourced.active())&&!detail::CurrentGridPaintPreview::for_render(sourced.active()),"Original source-backed material remains outside plain current-grid admission");
 production_texture_sets();
 std::cout<<"PASS "<<checks<<" current-grid native authority/materialization/history checks\n";
}catch(const std::exception&e){auditing=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
