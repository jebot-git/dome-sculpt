// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/bake-authoring.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/native-scene-append.hpp"
#include "dome/native-authoring.hpp"
#include "dome/emission-native.hpp"
#include "humanoid-sha256.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/gltf-export.hpp"
#include "dome/projection.hpp"
#include "dome/source-maps.hpp"
#include "dome/source-placement.hpp"
#include "dome/source-uv-bake.hpp"
#include "dome/tangents.hpp"
#include "texture-set-export.hpp"
#include "texture-set-bake.hpp"
#include "texture-set-field-source.hpp"
#include "polygon-surface-preview.hpp"
#include "baked-normal-field.hpp"
#include "gltf-document.hpp"
#include "cgltf.h"
#include "native-paint-sampling.hpp"
#include "polygon-surface-owner.hpp"
#include "polygon-surface-paint.hpp"
#include "paint-overlay-sample.hpp"
#include "paint-emission-sample.hpp"
#include "emission-exr-owned.hpp"
#include <bit>
#include <fstream>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool v,const char*s){if(!v)throw std::runtime_error(s);++checks;}
template<class F>void refuses(F&&f,const char*s){bool rejected=false;try{f();}catch(const std::exception&e){rejected=true;std::cout<<"EXPECTED "<<e.what()<<'\n';}check(rejected,s);}
void ready(Document &document){
 const auto timeout=std::chrono::seconds(120);
 const bool asset_ready=document.wait_asset(timeout);
 const auto deadline=std::chrono::steady_clock::now()+timeout;
 bool mesh_ready=false;
 do{
  const auto state=document.frame().state;
  if(!state->error.empty())throw std::runtime_error(state->error);
  mesh_ready=asset_ready&&document.wait_mesh(std::chrono::milliseconds(100));
 }while(asset_ready&&!mesh_ready&&std::chrono::steady_clock::now()<deadline);
 const auto state=document.frame().state;
 if(!state->error.empty())throw std::runtime_error(state->error);
 check(asset_ready&&mesh_ready,"Actual native workers settle");
}
void polygon(Document&d){check(d.wait_polygon(std::chrono::seconds(120)),"Actual native polygon queue settles");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
Image solid(std::array<uint8_t,4>rgba,uint32_t resolution=64){Image out{resolution,resolution,{}};out.rgba.resize(size_t(resolution)*resolution*4);for(size_t i=0;i<out.rgba.size();i+=4)std::copy(rgba.begin(),rgba.end(),out.rgba.begin()+i);return out;}
void native_owner_controls(const Scene&scene){
 const auto exact=scene.encode();const auto &actor=scene.active();const auto binding=actor.material_bindings;const auto native=binding->native_paint_sampling;
 const auto reopened=Scene::decode(exact);check(reopened.encode()==exact,"Actual native UV owner has an exact owning codec cycle before polygon capture");check(binding->sampling_reparameterized,"Genuine native KeepPaint before-binding declares its reparameterized sampling interpretation");
 auto cleared=scene.snapshot();auto cleared_binding=std::make_shared<SourceMaterialBindings>(*binding);cleared.active().material_bindings=cleared_binding;cleared_binding->sampling_reparameterized=false;
 refuses([&]{detail::PolygonSurfaceOwner::capture(cleared,cleared.active().id);},"An otherwise genuine native owner cannot clear its required sampling interpretation flag");
 auto candidate=scene.snapshot();auto changed=std::make_shared<SourceMaterialBindings>(*binding);candidate.active().material_bindings=changed;
 changed->native_paint_sampling=std::shared_ptr<const detail::NativePaintSampling>(native.get(),[](auto*){});
 refuses([&]{detail::PolygonSurfaceOwner::capture(candidate,candidate.active().id);},"A borrowed address/control block cannot authorize native polygon capture");
 candidate=scene.snapshot();changed=std::make_shared<SourceMaterialBindings>(*binding);candidate.active().material_bindings=changed;changed->native_paint_sampling=reopened.active().material_bindings->native_paint_sampling;
 refuses([&]{detail::PolygonSurfaceOwner::capture(candidate,candidate.active().id);},"A genuine foreign decoded native owner cannot borrow the current paint attachment");
 candidate=scene.snapshot();candidate.active().painted=native->before_paint()->snapshot();
 refuses([&]{detail::PolygonSurfaceOwner::capture(candidate,candidate.active().id);},"Original before paint cannot impersonate the current native UV attachment");
 auto future=exact;future.at("paint_sampling").at("version")=3;
 refuses([&]{Scene::decode(future);},"A changed sampling archive version cannot grant current native or polygon authority");
 auto donor=std::make_shared<PaintedMesh>(actor.painted->mesh(),64);BakedTextures bake;bake.maps.color=solid({90,100,110,255});bake.maps.metal_rough=solid({255,160,0,255});bake.maps.emission=solid({0,0,0,255});bake.maps.normal=solid({128,128,255,255});bake.coverage.assign(64*64,255);donor->install_bake(std::move(bake));const auto converted=convert_canonical_normals({donor,{},{},{},actor.material,{}});
 candidate=scene.snapshot();changed=std::make_shared<SourceMaterialBindings>(*binding);candidate.active().material_bindings=changed;changed->normal_frames=converted.material_bindings->normal_frames;changed->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;
 refuses([&]{detail::PolygonSurfaceOwner::capture(candidate,candidate.active().id);},"A real foreign canonical artist frame cannot enter the paint-only native adapter");
 detail::PolygonSurfaceTransferLimits limited;limited.maximum_working_bytes=1024;
 refuses([&]{detail::PolygonSurfaceOwner::capture(scene,actor.id,limited);},"Native field capture preserves prospective owner/snapshot quota refusal");limited={};limited.geometry.cancelled=[]{return true;};
 refuses([&]{detail::PolygonSurfaceOwner::capture(scene,actor.id,limited);},"Native field capture keeps actual cancellation before publication");
 check(scene.encode()==exact,"Every native ownership/version/frame/quota control preserves the complete original Scene");
}
void move(Document&d){
 const auto topology=d.frame().state->polygon->topology;const auto mesh=topology->triangulate().mesh;const auto a=mesh.positions.at(mesh.indices.at(0)),b=mesh.positions.at(mesh.indices.at(1)),c=mesh.positions.at(mesh.indices.at(2));const auto p=a*.2f+b*.3f+c*.5f,n=normalized(cross(b-a,c-a));
 d.polygon_pointer(p+n,n*-1,PolygonAction::BeginDrag);polygon(d);check(d.frame().state->polygon->moving,"Actual selected face ray begins a native Move gesture");const Vec3 delta{.006f,.003f,0};d.polygon_pointer(p+n+delta,n*-1,PolygonAction::Drag);polygon(d);d.polygon_pointer(p+n+delta,n*-1,PolygonAction::EndDrag);polygon(d);
}
Scene source(bool prepared,bool reflected,bool floating=false,bool varying=false,bool recipes=false,bool split_grids=false,uint32_t resolution=64){
 Mesh m;m.positions={{-.8f,-.6f,0},{.8f,-.6f,0},{.8f,.6f,0},{-.8f,.6f,0}};m.normals.assign(4,{0,0,1});if(varying)m.normals[1]=normalized(Vec3{.2f,.1f,1});m.uv={{{.1f,.1f}},{{.9f,.1f}},{{.9f,.9f}},{{.1f,.9f}}};m.indices={0,1,2,0,2,3};
 Scene s;auto&o=s.active();o.mesh_active=true;o.material.double_sided=true;o.material.color={.4f,.3f,.2f,1};o.painted=std::make_shared<PaintedMesh>(m,resolution);
 if(prepared){BakedTextures bake;bake.maps.color=solid({90,100,110,255},resolution);bake.maps.metal_rough=solid({255,160,0,255},resolution);bake.maps.emission=solid({0,0,0,255},resolution);bake.maps.normal=solid({128,128,255,255},resolution);bake.coverage.assign(size_t(resolution)*resolution,255);for(size_t y=0;y<resolution;++y)for(size_t x=0;x<resolution;++x){const auto at=(y*resolution+x)*4;bake.maps.normal.rgba[at]=uint8_t(96+16*x/resolution);bake.maps.normal.rgba[at+1]=uint8_t(112+y/3);bake.maps.normal.rgba[at+2]=240;}o.painted->install_bake(std::move(bake));auto converted=convert_canonical_normals({o.painted,{},{},{},o.material,{}});o.painted=std::move(converted.paint);o.material_bindings=std::move(converted.material_bindings);}
 if(floating){PaintSettings ink;ink.front_only=false;ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={3,.01f,1};Brush b;b.from=b.to={0,0,0};b.normal={0,0,1};b.radius=2;b.strength=1;o.painted->begin_stroke();o.painted->stamp(b,ink);check(o.painted->end_stroke(),"Negative fixture owns a genuine FLOAT stroke");}
 // The N-only positive must contain real byte paint and request the actual
 // KeepPaint UV transition. Empty ordinary Paint correctly uses the legacy
 // polygon route and is not an original-field ownership receipt.
 if(!prepared&&!floating){PaintSettings ink;ink.front_only=false;ink.channel=PaintChannel::Color;ink.color={.2f,.7f,.4f,1};Brush b;b.from=b.to={0,0,0};b.normal={0,0,1};b.radius=.7f;b.strength=.8f;b.blend=.2f;o.painted->begin_stroke();o.painted->stamp(b,ink);check(o.painted->end_stroke()&&o.painted->has_paint_data()&&!o.painted->has_physical_emission(),"Actual N-only source owns nonempty byte paint without manufactured FLOAT history");}
 if(split_grids){
  SourceUvBakeInput input;
  input.paint=o.painted;input.current_material=o.material_bindings;input.target=m;
  input.source_vertices={0,1,2,3};
  for(auto &uv:input.target.uv){uv[0]=.1f+.66f*uv[0];uv[1]=.15f+.72f*uv[1];}
  auto result=adopt_source_uv_bake(bake_source_uv(input));
  o.painted=std::move(result.paint);o.material_bindings=std::move(result.material_bindings);
 }
 if (recipes) {
   const auto target = s.active().id;
   const auto recipe_source = s.add("Texture Bake source", Volume{});
   auto &bake_source = s.object(recipe_source);
   bake_source.mesh_active = true;
   bake_source.painted = std::make_shared<PaintedMesh>(m,64);
   s.select(target);
   s.object(target).bake_recipe = capture_bake_recipe(s, recipe_source, {});
 }
 Document d;d.replace(s);ready(d);
 if(!floating){d.begin_uv();check(d.wait_uv(),"Actual original normal/paint UV workspace opens");if(!prepared){check(d.frame().state->uv->paint_sampling_available,"Actual native KeepPaint action is available");if(!d.frame().state->uv->retain_paint_sampling)d.edit_uv(UvAction::RetainPaint);}d.edit_uv(UvAction::Pick,.5f,.5f);d.edit_uv(UvAction::Scale,.8f);check(d.wait_uv(),"Actual writable UV changes independently of retained original coordinates");d.end_uv(true);ready(d);}
 if(!prepared&&!floating){const auto retained=d.copy_scene();const auto &native=retained.active();check(native.material_bindings&&native.material_bindings->native_paint_sampling&&native.material_bindings->native_paint_sampling->valid_owner(native.material_bindings->native_paint_sampling)&&native.material_bindings->native_paint_sampling->matches_current(*native.painted),"Actual native UV worker publishes its complete paint source/attachment owner");check(native.painted->mesh().uv!=m.uv&&!native.painted->has_physical_emission(),"N-only original paint UV differs from current geometry without FLOAT");native_owner_controls(retained);}

 const auto before_polygon=d.copy_scene().encode();d.begin_polygon(true);polygon(d);d.edit_polygon(PolygonAction::Faces);polygon(d);d.edit_polygon(PolygonAction::SelectAll);polygon(d);move(d);d.edit_polygon(PolygonAction::FlipFaces);polygon(d);d.end_polygon(true);ready(d);
 if(!prepared&&!floating){const auto accepted=d.copy_scene().encode();check(accepted.at("version")==39&&Scene::decode(accepted).encode()==accepted,"Actual native UV to Move/Flip field has an exact owning Scene39 codec cycle");auto cyclic=accepted;const auto &bindings=cyclic.at("source_materials").at("bindings");size_t field_binding=0;while(field_binding<bindings.size()&&bindings.at(field_binding).at("polygon_surface_transfer").is_null())++field_binding;check(field_binding<bindings.size(),"Owning native history has its actual current polygon material node");cyclic.at("source_materials").at("native_paints").at(0).at("before_binding")=field_binding;refuses([&]{Scene::decode(cyclic);},"Nested native history cannot borrow a future current-field material or create a source cycle");check(d.undo()&&d.copy_scene().encode()==before_polygon&&d.redo()&&d.copy_scene().encode()==accepted,"Actual native Move/Flip has exact global Undo/Redo and original UV owner history");Document restart;restart.replace(Scene::decode(accepted));ready(restart);restart.restore_polygon_source();ready(restart);check(restart.copy_scene().encode()==before_polygon,"Restarted polygon source Restore recovers the actual original native UV paint owner");}
 auto result=d.copy_scene();auto&actor=result.active();auto placement=AffineTransform{}.matrix();placement[0]=reflected?-1.2:1.2;placement[1]=.1;placement[4]=.3;placement[5]=.8;placement[9]=.15;placement[10]=1.1;actor.affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(placement));
 check(actor.material_bindings&&actor.material_bindings->polygon_surface_transfer,"Actual accepted native edit owns its original field");if(!prepared&&!floating){const auto &before=actor.material_bindings->polygon_surface_transfer->before();check(before->material_bindings&&before->material_bindings->native_paint_sampling&&before->paint->paint_sampling_domain()&&before->paint->paint_sampling_domain()->version()==1&&!before->paint->has_physical_emission(),"Genuine polygon before-root retains the complete native UV paint owner and original non-FLOAT basis");check(actor.material_bindings->polygon_surface_transfer->matches_current(*actor.painted),"Accepted N-only owner authenticates the actual new current Paint attachment");check(before->material_bindings->sampling_reparameterized&&!actor.material_bindings->sampling_reparameterized,"Actual native before=true and polygon current=false remain separate owning sampling interpretations");}const auto object=actor.id;const auto face=actor.polygon->topology.faces().front().id;create_texture_set(result,object,std::array<uint64_t,1>{face},"Exported retained slot",resolution);
 // Collection publication atomically replaces Scene; retain the stable ID,
 // never a reference into its previous object vector, across public edits.
 edit_texture_set(result,object,1,[](PaintedMesh&,Material&m){m.name="Actual current second material";m.roughness=.3f;return true;});result.validate();const auto wire=result.encode();check(Scene::decode(wire).encode()==wire,"Actual current per-field native source collection survives owning Scene40 and retained history transport");return result;
}
uint32_t word(const std::vector<uint8_t>&b,size_t n){return uint32_t(b.at(n))|uint32_t(b.at(n+1))<<8|uint32_t(b.at(n+2))<<16|uint32_t(b.at(n+3))<<24;}
std::vector<float>attribute(const detail::GltfDocument&d,int index,size_t n,size_t stride){const auto&a=d.json.at("accessors").at(index);check(a.at("componentType")==5126&&n<a.at("count").get<size_t>(),"Actual emitted float accessor has its full domain");const auto&v=d.json.at("bufferViews").at(a.at("bufferView").get<size_t>());const size_t offset=v.value("byteOffset",size_t(0))+a.value("byteOffset",size_t(0))+n*v.value("byteStride",stride*4);std::vector<float>r;for(size_t k=0;k<stride;++k)r.push_back(std::bit_cast<float>(word(d.binary,offset+k*4)));return r;}
uint32_t index_at(const detail::GltfDocument&d,int index,size_t n){const auto&a=d.json.at("accessors").at(index);const auto&v=d.json.at("bufferViews").at(a.at("bufferView").get<size_t>());check(a.at("componentType")==5125&&n<a.at("count").get<size_t>(),"Actual material index accessor has its own range");return word(d.binary,v.value("byteOffset",size_t(0))+a.value("byteOffset",size_t(0))+n*4);}
Vec3 vector(const std::vector<float>&v){return {v.at(0),v.at(1),v.at(2)};}
Vec3 unit(AffineVector v){return normalized(Vec3{float(v[0]),float(v[1]),float(v[2])});}
Image texture_image(const detail::GltfDocument&d,const nlohmann::json&info){const auto&texture=d.json.at("textures").at(info.at("index").get<size_t>());const auto&image=d.json.at("images").at(texture.at("source").get<size_t>());const auto&view=d.json.at("bufferViews").at(image.at("bufferView").get<size_t>());const auto offset=view.value("byteOffset",size_t(0)),count=view.at("byteLength").get<size_t>();check(offset<=d.binary.size()&&count<=d.binary.size()-offset,"Actual material PNG lies within the emitted buffer");return decode_projection_png(std::span(d.binary).subspan(offset,count));}
void audit_normals(const detail::GltfDocument &,const ObjectSurface &,float);
void retained_hdr_export(bool standard,uint32_t resolution=64){
 auto scene=source(true,false,false,false,false,false,resolution);
 const auto object=scene.active().id;
 const bool production=resolution>64;
 for(TextureSetId set:{0u,1u}){
  Vec3 center{};
  if(production){
   const auto member=texture_set_view(scene.active(),set);
   const auto &mesh=member.painted->mesh();
   const auto triangle=member.domain->triangles().front();
   for(size_t k=0;k<3;++k)center+=mesh.positions.at(mesh.indices.at(size_t(triangle)*3+k))*(1.f/3);
  }
  check(edit_texture_set(scene,object,set,[&](PaintedMesh &paint,Material &material){
   material.emissive=set?std::array<float,3>{.15f,.6f,.1f}:std::array<float,3>{.6f,.1f,.2f};
   paint.add_layer("Distinct editable HDR field");
   Brush brush;brush.from=brush.to=center;brush.normal={0,0,1};brush.radius=production?.046f:2.f;brush.strength=.8f;
   PaintSettings settings;settings.front_only=false;settings.channel=PaintChannel::Emission;
   settings.emission_units=PaintEmissionUnits::Physical;
   settings.emission=set?std::array<float,3>{.05f,11.f,1.f}:std::array<float,3>{6.f,.03f,.5f};
   paint.begin_stroke();paint.stamp(brush,settings);const bool emission=paint.end_stroke();
   if(!production)return emission;
   settings.channel=PaintChannel::Color;
   settings.color={.7f,.13f,.4f,.72f};
   brush.from.x+=.023f;brush.to=brush.from;
   paint.begin_stroke();paint.stamp(brush,settings);
   const bool color=paint.end_stroke();
   return emission&&color;
  }),"The retained texture set has independently authored FLOAT radiance");
 }
 const auto exact=scene.encode();
 Document doc;
 if(production){
  scene.save("source.dsculpt");doc.open_async("source.dsculpt").get();ready(doc);
  const auto frame=doc.frame().surface;
  check(frame&&frame->ranges.size()==2&&frame->picking,
        "Cold native Open publishes both production retained sets and authenticated picking");
  for(const auto &range:frame->ranges){
   const auto member=texture_set_view(scene.active(),range.texture_set);
   const auto paint=member.source->read_view(member);
   const auto reference=detail::PolygonSurfacePaintCodec::preview(*paint,member.material_bindings,
       member.material,{},member.source->source_preview());
   const auto &maps=*range.textures;
   check(maps.color.rgba==reference->color.rgba&&maps.metal_rough.rgba==reference->metal_rough.rgba&&
       maps.normal.rgba==reference->normal.rgba&&maps.mtoon_normal.rgba==reference->mtoon_normal.rgba&&
       maps.transparent==reference->transparent&&maps.polygon_surface,
       "Production preview matches every original material and retained normal byte");
  }
  if(standard){
   const auto active=active_texture_set(scene.active());
   const auto triangle=active.domain->triangles().front();
   const auto &mesh=active.painted->mesh();
   const auto world=scene.world_matrix(object);
   std::array<Vec3,3> points;
   for(size_t k=0;k<3;++k){
    const auto p=mesh.positions.at(mesh.indices.at(size_t(triangle)*3+k));
    const auto q=world.point({p.x,p.y,p.z});
    points[k]={float(q[0]),float(q[1]),float(q[2])};
   }
   const auto center=(points[0]+points[1]+points[2])*(1.f/3);
   const auto n=normalized(cross(points[1]-points[0],points[2]-points[0]));
   PaintSettings ink;
   ink.front_only=false;
   ink.color={.2f,.8f,.45f,.9f};
   doc.paint_settings(ink);
   doc.configure(Tool::Clay,.035f);
   NativeStrokeSample sample;
   sample.origin=center+n*.14f;
   sample.direction=n*-1;
   sample.brush.radius=.035f;
   sample.brush.pressure=1;
   sample.brush.strength=1;
   check(doc.begin_native_stroke()&&doc.sample_native_stroke(sample),
         "Production retained member admits a native desktop paint ray");
   doc.end_native_stroke(true);
   check(doc.wait_native_stroke(),"Prepared retained desktop stroke drains");
   ready(doc);
   const auto accepted=doc.copy_scene().encode();
   check(accepted!=exact,"Desktop stroke changes the selected retained member");
   check(doc.undo()&&doc.copy_scene().encode()==exact&&
         doc.redo()&&doc.copy_scene().encode()==accepted&&doc.undo()&&doc.copy_scene().encode()==exact,
         "Prepared retained desktop paint has exact Undo and Redo");
   ready(doc);
  }
 }else{doc.replace(scene);ready(doc);}
 const std::filesystem::path file=standard?"standard-retained-hdr.glb":"editable-retained-hdr.glb";
 doc.export_asset(file,standard?AssetExportFormat::Glb:AssetExportFormat::EditableGlb512).get();
 std::ifstream stream(file,std::ios::binary);
 const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(stream),{}};
 const auto restored=standard?extract_native_emission(bytes):extract_native_authoring(bytes);
 check(restored&&restored->encode()==exact&&doc.copy_scene().encode()==exact,
       "The actual export preserves every native retained HDR field and its membership");
 const auto json_length=word(bytes,12);
 detail::GltfDocument parsed;
 parsed.json=nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+json_length);
 parsed.binary.assign(bytes.begin()+28+json_length,bytes.end());
 const auto &primitives=parsed.json.at("meshes").at(0).at("primitives");
 check(primitives.size()==2,"Each retained field has its own emitted primitive");
 for(TextureSetId set:{0u,1u}){
   const auto member=texture_set_view(scene.active(),set);
   const auto &expected=member.source->source_preview()->paint_coordinates();
   const auto &p=primitives.at(set);
   const auto &m=parsed.json.at("materials").at(p.at("material").get<size_t>());
   const auto &info=m.at("emissiveTexture");
   const auto coordinate=info.at("texCoord").get<int>();
   const int uv=p.at("attributes").at("TEXCOORD_"+std::to_string(coordinate)).get<int>();
   check(coordinate>0&&p.at("extras").at("dome_sculpt_texture_set").at("id")==set,
       "The portable HDR image has the actual retained material and paint coordinates");
   size_t emitted=0;
   for(auto triangle:member.domain->triangles())for(size_t corner=0;corner<3;++corner){
     const auto vertex=index_at(parsed,p.at("indices").get<int>(),emitted++);
     const auto actual=attribute(parsed,uv,vertex,2);
     const auto &uv_expected=expected.at(size_t(triangle)*3+corner);
     check(actual[0]==uv_expected[0]&&actual[1]==uv_expected[1],
         "Each emitted HDR corner retains the original material sampling coordinate");
   }
   const auto image=texture_image(parsed,info);
   size_t peak=0;
   for(size_t i=0;i<image.rgba.size();i+=4)peak=std::max(peak,size_t(image.rgba[i+set]));
   const auto strength=m.at("extensions").at("KHR_materials_emissive_strength").at("emissiveStrength").get<float>();
   check(image.width==resolution&&image.height==resolution&&peak==255&&m.at("emissiveFactor").at(set)==1&&
         strength>(set?5.f:2.f),"Independent decoded PNG and strength keep the field's above-one dominant radiance");
 }
 if(production){
   const auto surfaces=scene.export_surfaces();
   audit_normals(parsed,surfaces.at(0),.02f);
   TextureSetLimits tight;tight.maximum_work=2'000'000;
   refuses([&]{detail::compile_texture_set_export(surfaces.at(0),tight,true);},
       "A tighter shared budget still refuses the production material grids");
 }
 if(standard){
   const auto &package=parsed.json.at("extras").at("domeSculptEmission");
   check(package.at("version")==6&&package.at("images").size()==2,
       "Standard HDR has a separate exact FLOAT resource per visible texture set");
   for(TextureSetId set:{0u,1u}){
     const auto &record=package.at("images").at(set);
     const auto &view=parsed.json.at("bufferViews").at(record.at("bufferView").get<size_t>());
     const auto raw=std::span(parsed.binary).subspan(view.at("byteOffset").get<size_t>(),
         view.at("byteLength").get<size_t>());
     const auto member=texture_set_view(scene.active(),set);
     const auto paint=member.source->read_view(member);
     const detail::PaintEmissionSampler reference(*paint);
     const auto background=member.material_bindings->physical_emission.value_or(member.material.emissive);
     size_t visited=0;
     const auto field=detail::read_emission_exr_rows(raw,[&](uint32_t y,std::span<const detail::EmissionExrPixel> pixels){
       for(size_t i=0;i<pixels.size();++i){
         const auto q=reference.sample_texel(uint32_t(i%resolution),y+uint32_t(i/resolution));
         const auto rgb=q.resolve(background,1);
         const detail::EmissionExrPixel want{rgb[0],rgb[1],rgb[2],q.legacy[0],q.legacy[1],q.legacy[2],
             q.absolute[0],q.absolute[1],q.absolute[2],q.coverage};
         if(std::bit_cast<std::array<uint32_t,10>>(want)!=std::bit_cast<std::array<uint32_t,10>>(pixels[i]))
           throw std::runtime_error("Retained FLOAT texel differs from the dense reference");
         ++visited;
       }
     },{});
     check(record.at("textureSet").at("id")==set&&field.width==resolution&&field.version==2&&
         visited==size_t(resolution)*resolution,
         "The independent OpenEXR resource carries every exact FLOAT coefficient of its member");
   }
   if(!production){
   auto bad_package=[&](const char *name,const char *expected,const auto &alter){
     auto damaged=parsed;
     auto &marker=damaged.json["extras"]["domeSculptEmission"];
     alter(marker["images"]);
     marker.erase("documentSha256");
     marker["documentSha256"]=creator_detail::sha256(nlohmann::json::to_cbor(damaged.json));
     detail::write_glb(std::move(damaged),name);
     std::ifstream file(name,std::ios::binary);
     const std::vector<uint8_t> damaged_bytes{std::istreambuf_iterator<char>(file),{}};
     std::string refusal;
     try{(void)extract_native_emission(damaged_bytes);}
     catch(const std::invalid_argument &e){refusal=e.what();}
     check(refusal.find(expected)!=std::string::npos,"Edited package cannot borrow per-field native authority");
     std::cout<<"EXPECTED "<<refusal<<'\n';
   };
   bad_package("wrong-paint-coordinate.glb","differs from its emitted primitive or coordinate",[](auto &images){images[0]["textureSet"]["texCoord"]=0;});
   bad_package("swapped-emission-fields.glb","FLOAT coefficients differ",[](auto &images){
     std::swap(images[0]["bufferView"],images[1]["bufferView"]);
     std::swap(images[0]["sha256"],images[1]["sha256"]);
   });
   bad_package("omitted-hdr-member.glb","omitted a visible physical material",[](auto &images){images.erase(1);});
   }
 }
 std::cout<<"RETAINED_HDR "<<(standard?"Standard":"Editable")<<" "<<bytes.size()
          <<" container bytes; native source exact\n";
}
void audit_normals(const detail::GltfDocument &output,const ObjectSurface &surface,float tolerance){
 float maximum=0;
 const auto &plan=*surface.texture_set_surface;
 const auto world=surface.world_matrix();
 const float reflection=world.orientation()==AffineOrientation::Reversing?-1.f:1.f;
 const auto &primitives=output.json.at("meshes").at(0).at("primitives");
 for(size_t f=0;f<primitives.size();++f){
  const auto &primitive=primitives.at(f),&attrs=primitive.at("attributes");
  const auto &material=output.json.at("materials").at(primitive.at("material").get<size_t>());
  const auto &field=plan.fields()[f];
  const auto &draw=plan.draws()[f];
  const auto &preview=field.source->source_preview();
  const bool mapped=preview->native_bake()||preview->model()==detail::PolygonSurfaceNormalModel::PreparedCorePbr;
  check(material.contains("normalTexture")==mapped,"Portable material carries exactly its authored normal role");
  const auto normal_attr=attrs.at("NORMAL").get<int>();
  int tangent_attr=0,uv_attr=0;
  std::optional<PaintSourceTexture> texture;
  if(mapped){
   check(attrs.contains("TANGENT"),"Mapped retained normals have an explicit glTF tangent");
   tangent_attr=attrs.at("TANGENT").get<int>();
   const auto &info=material.at("normalTexture");
   const auto uv_set=info.value("texCoord",0u);
   check(uv_set!=0,"The normal texture is routed outside the independent editable UV0");
   uv_attr=attrs.at("TEXCOORD_"+std::to_string(uv_set)).get<int>();
   texture.emplace();
   texture->image=std::make_shared<const Image>(texture_image(output,info));
   texture->wrap_s=texture->wrap_t=33071;
   texture->min_filter=texture->mag_filter=9729;
  }
  const auto indices=primitive.at("indices").get<int>();
  for(size_t c=0;c<draw.count;c+=3){
   const auto triangle=plan.logical_corners()[draw.first+c]/3;
   std::array<Vec3,3> normals{},tangents{},bitangents{};
   std::array<std::vector<float>,3> uv;
   for(size_t k=0;k<3;++k){
    const auto id=index_at(output,indices,c+k);
    const auto n=vector(attribute(output,normal_attr,id,3));
    normals[k]=unit(world.normal({n.x,n.y,n.z}));
    if(!mapped)continue;
    const auto t=attribute(output,tangent_attr,id,4);
    check(std::abs(dot(n,vector(t)))<2e-6f,"Exported vertex preserves an orthonormal local N/T frame");
    tangents[k]=unit(world.direction({t[0],t[1],t[2]}));
    bitangents[k]=normalized(cross(normals[k],tangents[k]))*(t[3]*reflection);
    uv[k]=attribute(output,uv_attr,id,2);
   }
   constexpr int steps=24;
   for(int i=0;i<=steps;++i)for(int j=0;j<=steps-i;++j){
    const std::array<double,3> bary{double(i)/steps,double(j)/steps,double(steps-i-j)/steps};
    Vec3 n{},t{},b{};
    std::array<double,2> lookup{};
    for(size_t k=0;k<3;++k){
     n+=normals[k]*float(bary[k]);
     if(!mapped)continue;
     t+=tangents[k]*float(bary[k]);
     b+=bitangents[k]*float(bary[k]);
     for(size_t axis=0;axis<2;++axis)lookup[axis]+=uv[k][axis]*bary[k];
    }
    auto emitted=normalized(n);
    if(texture){
     const auto cell=sample_source_texture(*texture,float(lookup[0]),float(lookup[1]),false);
     emitted=normalized(normalized(t)*(2*cell[0]-1)+normalized(b)*(2*cell[1]-1)+emitted*(2*cell[2]-1));
    }
    const auto expected=field.source->normal(triangle,bary,world,true);
    const auto error=std::atan2(length(cross(expected,emitted)),dot(expected,emitted));
    maximum=std::max(maximum,error);
    if(error>tolerance)
     throw std::runtime_error("GLB normal direction differs from the retained field by "+std::to_string(error)+" radians at "+std::to_string(i)+","+std::to_string(j));
    ++checks;
   }
  }
 }
 std::cout<<"PORTABLE normal peak radians "<<maximum<<'\n';
}
void audit(bool prepared,bool reflected,bool varying=false){
 auto scene=source(prepared,reflected,false,varying);const auto exact=scene.encode();const auto surfaces=scene.export_surfaces();check(surfaces.size()==1&&surfaces[0].texture_set_surface,"Real visible source actor yields one shared multi-material export bundle");const auto compiled=detail::compile_texture_set_export(surfaces[0]);const auto output=detail::make_gltf(surfaces);const auto&primitives=output.json.at("meshes").at(0).at("primitives");check(primitives.size()==2&&compiled.fields.size()==2,"Actual source bundle emits both nonempty material ranges");const auto&plan=*surfaces[0].texture_set_surface;const auto world=surfaces[0].world_matrix();
 for(size_t f=0;f<2;++f){const auto&primitive=primitives.at(f);const auto&attrs=primitive.at("attributes");const auto&field=plan.fields()[f];const auto&draw=plan.draws()[f];const auto&material=output.json.at("materials").at(primitive.at("material").get<size_t>());check(primitive.at("extras").at("dome_sculpt_texture_set").at("id")==field.id,"Portable primitive retains its actual object-local material identity");if(f)check(material.at("name")==field.material.name,"Additional portable material retains current authored scalars/name");
  const auto paint_set=material.at("pbrMetallicRoughness").at("baseColorTexture").value("texCoord",0u);const auto paint_attr=attrs.at("TEXCOORD_"+std::to_string(paint_set)).get<int>();const auto native=field.source->source_preview();const auto color=texture_image(output,material.at("pbrMetallicRoughness").at("baseColorTexture"));check(color.rgba==compiled.fields[f].textures->color.rgba,"Actual emitted per-material PNG retains current composite bytes");
  int normal_attr=0;
  if(prepared){check(material.contains("normalTexture")&&attrs.contains("TANGENT"),"Prepared retained role exports its actual normal image and tangent attributes");const auto image=texture_image(output,material.at("normalTexture"));check(image.rgba==native->field_owner()->before()->paint->baked()->normal_field->pbr()->rgba,"Actual emitted normal image preserves complete original role bytes");normal_attr=attrs.at("TEXCOORD_"+std::to_string(material.at("normalTexture").value("texCoord",0u)));}
  bool independent_uv=false;for(size_t c=0;c<draw.count;++c){const auto logical=plan.logical_corners()[draw.first+c],vertex=index_at(output,primitive.at("indices"),c);const auto raw_vertex=plan.geometry()->indices.at(logical);const auto uv=attribute(output,attrs.at("TEXCOORD_0"),vertex,2),paint=attribute(output,paint_attr,vertex,2);check(uv==std::vector<float>(plan.geometry()->uv.at(raw_vertex).begin(),plan.geometry()->uv.at(raw_vertex).end()),"Standard export retains current UV0 separately from original paint grid");check(paint==std::vector<float>(native->paint_coordinates().at(logical).begin(),native->paint_coordinates().at(logical).end()),"Material paint texture uses the actual retained logical corner coordinates");independent_uv|=uv!=paint;if(prepared){const auto role=attribute(output,normal_attr,vertex,2);check(role==std::vector<float>(native->normal_coordinates().at(logical).begin(),native->normal_coordinates().at(logical).end()),"Standard normal texture uses its actual independent role coordinates");}}
  check(independent_uv,"Each exported material keeps genuinely distinct current UV0 and original writable paint coordinates");
 }
 audit_normals(output,surfaces.front(),2e-5f);
 const std::string file=std::string(prepared?"prepared":"native")+
     (varying?"-smooth":"")+(reflected?"-reflected.glb":"-sheared.glb");export_glb(surfaces,file);cgltf_options options{};cgltf_data*data=nullptr;check(cgltf_parse_file(&options,file.c_str(),&data)==cgltf_result_success,"Independent cgltf parses the actual durable GLB");struct Free{cgltf_data*p;~Free(){cgltf_free(p);}}free{data};check(cgltf_load_buffers(&options,data,file.c_str())==cgltf_result_success&&cgltf_validate(data)==cgltf_result_success,"Independent standard glTF consumer validates actual buffers, materials and attributes");check(data->meshes_count==1&&data->meshes[0].primitives_count==2,"Independent consumer sees one geometry with both material primitives");
 check(scene.encode()==exact,"Portable source compilation leaves all original normal/paint/recipe/native history exact");
}
void retained_bake_source(bool prepared,bool physical,bool affine=false,bool reflected=false) {
 auto scene=source(prepared,reflected,physical);
 const auto source_id=scene.active().id;
 if(!affine)scene.active().affine_edit.reset();
 else {
  auto matrix=scene.active().affine_edit->matrix().matrix();
  matrix[2]=.07;matrix[6]=-.04;matrix[12]=.04;
  scene.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(matrix));
 }
 const auto source_world=scene.world_matrix(source_id);
 for(TextureSetId field:{0u,1u}) {
  check(edit_texture_set(scene,source_id,field,[field](PaintedMesh &paint,Material &){
   paint.add_layer("Current field paint");
   PaintSettings ink;ink.front_only=false;ink.color=field?std::array<float,4>{.8f,.05f,.05f,1}:std::array<float,4>{.01f,.1f,.85f,1};
   Brush brush;brush.from=brush.to={0,0,0};brush.normal={0,0,1};brush.radius=2;brush.strength=1;
   paint.begin_stroke();paint.stamp(brush,ink);return paint.end_stroke();
  }),"Current retained fields each contain real distinct editable paint");
 }
 auto low=scene.active().painted->mesh();
 low.normals.assign(low.positions.size(),{0,0,1});
 CanvasTransform recipient_world;
 if(affine)recipient_world={{-.12f,.06f,.02f},{0,0,.2588190451f,.9659258263f},.75f};
 const auto recipient_inverse=ob::inverse(recipient_world);
 const auto target_from_source=affine_similarity(recipient_inverse)*source_world;
 const auto target_id=scene.add("Plain Bake recipient",Volume{},recipient_world);
 auto &target=scene.active();
 target.mesh_active=true;
 for(auto &p:low.positions) {
  const auto q=target_from_source.point({p.x,p.y,p.z});
  p={float(q[0]),float(q[1]),float(q[2]+.015/recipient_world.scale)};
 }
 target.painted=std::make_shared<PaintedMesh>(low,64);
 scene.object(source_id).visible=false;
 scene.object(source_id).locked=true;
 const auto original=scene.encode();
 ObjectBakeSettings settings;
 settings.allow_backfaces=true;settings.padding=0;settings.ao_samples=0;
 Document doc;doc.replace(scene);ready(doc);
 check(doc.frame().state->can_bake,"Document lists the authenticated retained multi-material source");
 doc.bake_from_object(source_id,settings);
 const auto out=std::filesystem::path("retained-bake-source.dsculpt");
 doc.save_async(out).get();ready(doc);
 const auto result=doc.copy_scene();
 const auto baked=result.object(target_id).painted->baked();
 check(baked&&result.active().bake_recipe->source.object==source_id&&
       Scene::load(out).encode()==result.encode(),"Actual Bake and ordered Save reopen exactly");
 check(bool(baked->emission_field)==physical,"Only actual physical source uses the independent FLOAT output");
 auto before_source=scene.snapshot(),after_source=result.snapshot();
 before_source.erase(target_id);after_source.erase(target_id);
 check(before_source.encode()==after_source.encode(),"Baking preserves the complete retained source archive");
 const auto tangents=tangent_corners(low);
 const auto &source_object=result.object(source_id);
 const auto sample_at=[](const Image&image,std::array<float,2> uv,bool srgb){
  PaintSourceTexture texture;texture.image=std::shared_ptr<const Image>(&image,[](const Image *){});
  texture.wrap_s=texture.wrap_t=33071;texture.min_filter=texture.mag_filter=9729;
  return sample_source_texture(texture,uv[0],uv[1],srgb);
 };
 size_t authorized_pixels=0,distinct_coordinates=0,physical_pixels=0;
 std::array<size_t,2> field_pixels{};
 for(TextureSetId set:{0u,1u}) {
  const auto member=texture_set_view(source_object,set);
  const auto read=member.source->read_view(member);
  const auto maps=detail::PolygonSurfacePaintCodec::preview(*read,member.material_bindings,member.material);
  for(const auto triangle:member.domain->triangles()){
   const auto start=size_t(triangle)*3;
   const auto a=low.uv.at(low.indices[start]),b=low.uv.at(low.indices[start+1]),c=low.uv.at(low.indices[start+2]);
   const double determinant=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
   for(unsigned y=6;y<64;y+=7)for(unsigned x=6;x<64;x+=7){
    const float u=(x+.5f)/64,v=(y+.5f)/64;
    const double s=((u-a[0])*(c[1]-a[1])-(v-a[1])*(c[0]-a[0]))/determinant;
    const double t=((b[0]-a[0])*(v-a[1])-(b[1]-a[1])*(u-a[0]))/determinant;
    const std::array<double,3> weights{1-s-t,s,t};
    if(*std::min_element(weights.begin(),weights.end())<.08)continue;
    const size_t pixel=y*64+x;
    check(baked->coverage[pixel]==255,"Each independent interior raster point is projected");
    const auto source_sample=member.source->sample(triangle,weights);
    const auto expected_color=sample_at(maps->color,source_sample.paint_uv,true);
    const auto wrong_color=sample_at(maps->color,{u,v},true);
    for(size_t k=0;k<3;++k)
     check(std::abs(int(baked->maps.color.rgba[pixel*4+k])-int(std::lround(linear_to_srgb(expected_color[k])*255)))<=1,
           "Baked material color follows the independent original paint coordinates");
    if(std::abs(expected_color[0]-wrong_color[0])>.02f||std::abs(expected_color[2]-wrong_color[2])>.02f)
     ++distinct_coordinates;
    const auto expected_world=member.source->normal(triangle,weights,source_world,true);
    const auto expected=ob::rotate(recipient_inverse.orientation,{expected_world.x,expected_world.y,expected_world.z});
    const Vec3 expected_normal{expected.x,expected.y,expected.z};
    const auto got=tangent_frame(low,tangents,triangle,{float(weights[0]),float(weights[1]),float(weights[2])}).to_world(
        {baked->maps.normal.rgba[pixel*4]/255.f*2-1,baked->maps.normal.rgba[pixel*4+1]/255.f*2-1,baked->maps.normal.rgba[pixel*4+2]/255.f*2-1});
    check(length(normalized(got)-expected_normal)<.015f,"Projected normal retains the original role in the current physical corner frame");
    if(physical) {
     const auto value=detail::sample_overlay_values(*maps->overlay,source_sample.paint_uv[0],source_sample.paint_uv[1],false);
     const auto expected=BakedEmissionSample{{value[2][0],value[2][1],value[2][2]},
         {value[4][0],value[4][1],value[4][2]},value[3][3]}.resolve(member.material.emissive,1);
     const auto got=baked->emission_field->values()[pixel].resolve({},1);
     for(size_t k=0;k<3;++k)check(std::abs(got[k]-expected[k])<2e-5f,"Baked FLOAT emission matches continuous original-grid sampling");
     physical_pixels+=got[0]>1;
    }
    ++authorized_pixels;
    ++field_pixels[set];
   }
  }
 }
 check(authorized_pixels>=8&&field_pixels[0]&&field_pixels[1],"Oracle covers both source material ranges");
 if(!physical)check(distinct_coordinates>0,"Original paint and edited UV0 produce demonstrably different colors");
 if(physical)check(physical_pixels>0,"Fixture verifies genuine above-one radiance");
 check(doc.undo()&&doc.copy_scene().encode()==original&&doc.redo()&&doc.copy_scene().encode()==result.encode(),
       "Retained donor Bake is exactly one Undo and Redo");
 auto cancelled=scene.snapshot();bool stop=false;
 refuses([&]{bake_object(cancelled,source_id,settings,[&]{return stop;},[&](float f){stop|=f>.12f;});},
       "Detached retained-field projection can be cancelled");
 check(stop&&cancelled.encode()==original,"Retained donor cancellation leaves the complete Scene unchanged");
}

void affine_plain_bake_source(bool texture_sets){
 Mesh mesh;
 mesh.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};
 mesh.normals.assign(4,{0,0,1});
 mesh.uv={{0,0},{1,0},{1,1},{0,1}};
 mesh.indices={0,1,2,0,2,3};
 Scene scene;
 const auto donor=scene.active().id;
 std::array<TextureSetInitialField,2> fields;
 fields[0].resolution=fields[1].resolution=64;
 fields[0].material.name="First plain material";fields[1].material.name="Second plain material";
 const std::array<TextureSetId,2> membership{0,1};
 if(texture_sets)initialize_texture_sets(scene,donor,mesh,fields,membership);
 else{
  scene.active().mesh_active=true;
  scene.active().painted=std::make_shared<PaintedMesh>(mesh,64);
 }
 const std::array<uint8_t,4> encoded{176,89,236,255};
 for(TextureSetId id:{0u,1u}) {
  const auto install=[id,encoded](PaintedMesh &paint,Material &){
   BakedTextures detail;
   detail.maps.color=solid(id?std::array<uint8_t,4>{10,180,20,255}:std::array<uint8_t,4>{180,10,20,255});
   detail.maps.metal_rough=solid({255,128,0,255});
   detail.maps.emission=solid({0,0,0,255});
   detail.maps.normal=solid(encoded);
   detail.coverage.assign(64*64,255);
   paint.install_bake(std::move(detail));
   return true;
  };
  if(texture_sets)edit_texture_set(scene,donor,id,install);
  else{
   install(*scene.active().painted,scene.active().material);
   break;
  }
 }
 auto transform=AffineTransform{}.matrix();
 transform[0]=-1.2;transform[1]=.3;transform[2]=.07;
 transform[4]=.35;transform[5]=.8;transform[6]=-.04;transform[9]=.2;transform[10]=1.1;
 const AffineTransform world(transform);
 scene.active().affine_edit=std::make_shared<const NativeAffineEdit>(world);
 scene.active().visible=false;
 const auto recipient=scene.add("Front-only recipient",Volume{});
 for(auto &p:mesh.positions) {
  const auto q=world.point({p.x,p.y,p.z});
  p={float(q[0]),float(q[1]),float(q[2]+.01)};
 }
 scene.active().mesh_active=true;
 scene.active().painted=std::make_shared<PaintedMesh>(mesh,64);
 const auto before=scene.encode();
 Document doc;doc.replace(scene);ready(doc);
 check(doc.frame().state->can_bake,"The reflected plain donor is available");
 ObjectBakeSettings settings;settings.padding=0;settings.ao_samples=0;
 doc.bake_from_object(donor,settings);ready(doc);
 const auto result=doc.copy_scene();
 const auto bake=result.object(recipient).painted->baked();
 check(bake&&std::count(bake->coverage.begin(),bake->coverage.end(),255)==64*64,
       "Front-only Bake accounts for reflected winding without reordering source material corners");
 const auto t=unit(world.direction({1,0,0})),n=unit(world.normal({0,0,1}));
 const auto b=cross(n,t)*-1.f;
 const auto expected=normalized(t*(encoded[0]/255.f*2-1)+b*(encoded[1]/255.f*2-1)+n*(encoded[2]/255.f*2-1));
 const auto target_corners=tangent_corners(mesh);
 for(size_t triangle=0;triangle<2;++triangle) {
  const size_t x=triangle?16:48,y=triangle?48:16,at=(y*64+x)*4;
  const auto channel=texture_sets?triangle:0;
  check(bake->maps.color.rgba[at+channel]>bake->maps.color.rgba[at+1-channel],
        "Reflected placement keeps the actual per-triangle material");
  const auto mapped=tangent_frame(mesh,target_corners,triangle,{.25f,.5f,.25f}).to_world({
      bake->maps.normal.rgba[at]/255.f*2-1,bake->maps.normal.rgba[at+1]/255.f*2-1,
      bake->maps.normal.rgba[at+2]/255.f*2-1});
  check(length(mapped-expected)<.015f,"Plain baked detail uses transformed original Mikk tangent and handedness");
 }
 check(doc.undo()&&doc.copy_scene().encode()==before,"The reflected donor and recipient Undo exactly");
}

void affine_bake_target(bool texture_set,bool prepared,bool reflected){
 Mesh mesh;
 mesh.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};
 mesh.normals.assign(4,{0,0,1});
 mesh.uv={{.10f,.10f},{.90f,.10f},{.90f,.90f},{.10f,.90f}};
 mesh.indices={0,1,2,0,2,3};
 auto matrix=AffineTransform{}.matrix();
 matrix[0]=reflected?-1.6:1.6;matrix[1]=.25;matrix[2]=.1;
 matrix[4]=.4;matrix[5]=.72;matrix[6]=-.2;
 matrix[8]=.3;matrix[9]=.24;matrix[10]=2.4;
 matrix[12]=.2;matrix[13]=-.3;matrix[14]=.12;
 const AffineTransform world(matrix);
 const auto tangent=unit(world.direction({1,0,0})),normal=unit(world.normal({0,0,1}));
 const auto bitangent=cross(normal,tangent)*(reflected?-1.f:1.f);
 const auto expected=normalized(tangent*.28f+bitangent*.18f+normal*.94f);

 Scene scene;
 const auto target=scene.active().id;
 if(texture_set){
  std::array<TextureSetInitialField,2> fields;
  fields[0].resolution=fields[1].resolution=64;
  fields[0].material.name="Other target layer";fields[1].material.name="Affine recipient";
  initialize_texture_sets(scene,target,mesh,fields,std::array<TextureSetId,2>{0,1});
  select_texture_set(scene,target,1);
 }else{
  scene.active().mesh_active=true;scene.active().painted=std::make_shared<PaintedMesh>(mesh,64);
 }
 scene.active().affine_edit=std::make_shared<const NativeAffineEdit>(world);

 const auto donor=scene.add("Physical distance donor",Volume{});
 auto high=mesh;
 for(auto &p:high.positions){
  const auto q=world.point({p.x,p.y,p.z});
  p=Vec3{float(q[0]),float(q[1]),float(q[2])}+normal*.027f;
 }
 high.normals.assign(4,expected);
 if(reflected)for(size_t c=0;c<high.indices.size();c+=3)
  std::swap(high.indices[c+1],high.indices[c+2]);
 scene.active().mesh_active=true;scene.active().visible=false;
 scene.active().material.color={.02f,.65f,.08f,1};
 scene.active().painted=std::make_shared<PaintedMesh>(high,64);

 const auto cage=scene.add("Independent matching cage",Volume{});
 auto cage_mesh=mesh;
 const auto offset=world.inverse().direction({normal.x*.031,normal.y*.031,normal.z*.031});
 for(auto &p:cage_mesh.positions)p+=Vec3{float(offset[0]),float(offset[1]),float(offset[2])};
 auto cage_matrix=matrix;
 cage_matrix[12]+=.02;
 const auto correction=world.inverse().direction({-.02,0,0});
 for(auto &p:cage_mesh.positions)p+=Vec3{float(correction[0]),float(correction[1]),float(correction[2])};
 scene.active().mesh_active=true;scene.active().visible=false;
 scene.active().painted=std::make_shared<PaintedMesh>(std::move(cage_mesh),64);
 scene.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(cage_matrix));
 scene.select(target);
 const auto original=scene.encode();

 ObjectBakeSettings settings;
 settings.front_mm=34;settings.back_mm=0;settings.padding=0;settings.ao_samples=4;
 settings.cage_object=cage;settings.prepare_normals=prepared;
 auto short_ray=settings;short_ray.cage_object=0;short_ray.front_mm=25;
 auto detached=scene.snapshot();
 refuses([&]{bake_object(detached,donor,short_ray);},"A 25 mm world-bounded ray cannot reach the 27 mm donor");
 auto short_cage=settings;short_cage.front_mm=29;
 refuses([&]{bake_object(detached,donor,short_cage);},"The 31 mm world cage cannot fit inside a 29 mm bound");
 check(detached.encode()==original,"Out-of-bounds rays and cage preserve the complete affine project");
 Document doc;doc.replace(scene);ready(doc);
 check(doc.frame().state->can_bake,"The affine recipient is available through Document Bake");
 doc.bake_from_object(donor,settings);
 const auto saved=std::filesystem::path("affine-target.dsculpt");
 doc.save_async(saved).get();ready(doc);
 const auto result=doc.copy_scene();
 const auto field=active_texture_set(result.active());
 const auto baked=field.painted->baked();
 check(baked&&field.bake_recipe&&field.bake_recipe->settings.cage_object==cage&&
       Scene::load(saved).encode()==result.encode(),"Affine Bake, exact cage recipe and ordered Save reopen");
 const auto same_position=[](Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;};
 check(result.world_matrix(target).matrix()==world.matrix()&&
       std::ranges::equal(result.active().painted->mesh().positions,mesh.positions,same_position),
       "Bake preserves the target's local vertices and affine placement");
 if(texture_set)check(!result.active().painted->baked(),"Other target field remains untouched");
 const Image &normal_image=baked->normal_field?*baked->normal_field->pbr():baked->maps.normal;
 for(const auto pixel:{size_t(37*64+19),size_t(43*64+22),size_t(34*64+26)}){
  const auto at=pixel*4;
  check(baked->coverage[pixel]==255&&baked->maps.color.rgba[at+1]>160,
        "The independently placed donor projects onto the affine target");
  const auto direction=normalized(tangent*(normal_image.rgba[at]/255.f*2-1)+
      bitangent*(normal_image.rgba[at+1]/255.f*2-1)+normal*(normal_image.rgba[at+2]/255.f*2-1));
  check(length(direction-expected)<.015f,"Baked texture decodes to the world normal in the rendered target frame");
 }
 check(doc.undo()&&doc.copy_scene().encode()==original&&doc.redo()&&doc.copy_scene().encode()==result.encode(),
       "Affine target Bake Undo and Redo preserve the exact project");
}

void retained_bake_xr_fixture(const std::filesystem::path &input,const std::filesystem::path &reference) {
 auto scene=source(true,true);
 const auto donor=scene.active().id;
 auto &source_object=scene.active();
 source_object.visible=false;source_object.locked=true;
 const auto source_world=scene.world_matrix(donor);
 const auto placement=source_object.affine_edit;
 auto mesh=source_object.painted->mesh();
 mesh.normals.assign(mesh.positions.size(),{0,0,1});
 auto cage_mesh=mesh;
 const auto down=source_world.inverse().direction({0,0,-.06});
 const auto up=source_world.inverse().direction({0,0,.01});
 for(auto &p:mesh.positions)p+=Vec3{float(down[0]),float(down[1]),float(down[2])};
 for(auto &p:cage_mesh.positions)p+=Vec3{float(up[0]),float(up[1]),float(up[2])};
 const auto target=scene.add("Texture-set Bake recipient",Volume{});
 const auto cage=scene.add("Full matched cage",Volume{});
 scene.active().mesh_active=true;scene.active().visible=false;
 scene.active().painted=std::make_shared<PaintedMesh>(std::move(cage_mesh),64);
 scene.active().affine_edit=placement;
 std::array<TextureSetInitialField,2> recipients;
 recipients[0].resolution=recipients[1].resolution=64;
 recipients[0].material.name="Left set";recipients[1].material.name="Right set";
 std::vector<TextureSetId> membership(mesh.indices.size()/3,0);
 for(auto triangle:scene.object(donor).texture_sets->at(1).domain->triangles())
  membership[triangle]=1;
 initialize_texture_sets(scene,target,mesh,recipients,membership);
 scene.object(target).affine_edit=placement;
 scene.select(target);select_texture_set(scene,target,1);
 ObjectBakeSettings settings;
 settings.front_mm=80;settings.padding=0;settings.ao_samples=4;settings.cage_object=cage;
 settings.allow_backfaces=true;
 bake_object(scene,donor,settings);
 check(edit_texture_set(scene,target,1,[](PaintedMesh &paint,Material &){
  paint.clear_bake();return true;
 }),"The selected fixture set retains its successful recipe");
 scene.save(input);
 const auto encoded=scene.encode();
 auto plain=Scene::load(input);
 check(plain.encode()==encoded,"Prepared donor and plain retained recipe reopen in the same Scene40 archive");
 auto foreign=encoded;
 auto &records=foreign.at("texture_set_geometry").at("paints");
 const auto recipe=std::find_if(records.begin(),records.end(),[](auto &record){
  return record.at("painted").contains("field_recipe");
 });
 check(recipe!=records.end(),"Fixture encodes the actual selected plain recipe member");
 recipe->at("painted").at("payload").at("set")=0;
 refuses([&]{Scene::decode(foreign);},"Mixed retained graph rejects a plain recipe borrowing another set");
 const auto original=plain.encode();
 auto shortened=settings;shortened.front_mm=40;
 refuses([&]{bake_object(plain,donor,shortened);},"The shortened approach cannot reach the original prepared field");
 check(plain.encode()==original,"Failed probe leaves both fields unchanged");
 bake_object(plain,donor,settings);
 check(active_texture_set(plain.active()).painted->baked()&&!plain.active().painted->baked(),
       "The donor's original prepared role enters only the selected target field");
 plain.save(reference);
}
}
void bake_retained_recipient(TextureSetId id,bool physical,uint32_t resolution=64){
 auto scene=source(true,physical,false,false,false,physical,resolution);
 const auto target=scene.active().id;
 if(!physical)scene.active().affine_edit.reset();
 if(resolution>64)for(TextureSetId set:{0u,1u}){
  const auto member=texture_set_view(scene.active(),set);
  const auto triangle=member.domain->triangles().front();
  const auto &mesh=member.painted->mesh();
  Vec3 center{};
  for(size_t k=0;k<3;++k)center+=mesh.positions.at(mesh.indices.at(size_t(triangle)*3+k))*(1.f/3);
  check(edit_texture_set(scene,target,set,[&](PaintedMesh &paint,Material &material){
   material.emissive=set?std::array<float,3>{.2f,.4f,.6f}:std::array<float,3>{.7f,.1f,.3f};
   paint.add_layer("Distinct LDR layers");
   Brush brush;brush.from=brush.to=center;brush.normal={0,0,1};brush.radius=.046f;brush.strength=.8f;
   PaintSettings emissive;emissive.front_only=false;emissive.channel=PaintChannel::Emission;
   emissive.emission_units=PaintEmissionUnits::MaterialRelative;emissive.emission={.4f,.15f,.8f};
   paint.begin_stroke();paint.stamp(brush,emissive);
   const bool emission=paint.end_stroke();
   emissive.channel=PaintChannel::Color;emissive.color={.55f,.13f,.6f,.72f};
   brush.from.x+=.021f;brush.to=brush.from;
   paint.begin_stroke();paint.stamp(brush,emissive);
   return paint.end_stroke()&&emission&&!paint.has_physical_emission();
  }),"Both large retained fields contain genuine LDR color and emission paint");
 }
 select_texture_set(scene,target,id);
 const auto old=texture_set_view(scene.active(),id);
 const auto neighbor=texture_set_view(scene.active(),id==0?1:0);
 const auto world=scene.world_matrix(target);
 const auto prior=old.painted->baked()->normal_field;
 const auto &preview=*old.source->source_preview();
 check((preview.paint_coordinates()!=preview.normal_coordinates())==physical,
       "Fixture distinguishes matching and independently resampled paint and normal coordinates");
 const auto covered=old.domain->triangles().front();
 const auto old_normal=old.source->normal(covered,{.2,.3,.5},world,true);
 Mesh high=scene.active().painted->mesh();
 const auto local=normalized(Vec3{.18f,-.24f,-1.f});
 Vec3 displacement{0,0,.012f};
 if(physical&&resolution>64){
  const auto outward=unit(world.normal({0,0,1}));
  const auto local_offset=world.inverse().direction({.012*outward.x,.012*outward.y,.012*outward.z});
  displacement={float(local_offset[0]),float(local_offset[1]),float(local_offset[2])};
 }
 for(auto &p:high.positions)p+=displacement;
 high.normals.assign(high.positions.size(),local);
 const auto placement=scene.active().affine_edit;
 const auto donor=scene.add("Independent production Bake donor",Volume{});
 auto &object=scene.active();object.mesh_active=true;
 object.affine_edit=placement;
 object.painted=std::make_shared<PaintedMesh>(high,64);
 object.material.color={.82f,.11f,.27f,1};
 if(physical)object.material.emissive={3.f,.1f,.2f};
 // Affine donors use the ordinary texture-set placement path. Give both
 // donor fields the same material so the target's role sampling is isolated.
 if(placement){
  const uint64_t face=1;
  create_texture_set(scene,donor,std::array<uint64_t,1>{face},"Donor face",64);
  edit_texture_set(scene,donor,1,[](PaintedMesh &,Material &m){
   m.color={.82f,.11f,.27f,1};m.emissive={3.f,.1f,.2f};return true;
  });
 }
 scene.select(target);
 const auto baseline=scene.encode();
 ObjectBakeSettings settings;settings.allow_backfaces=true;settings.padding=0;settings.ao_samples=2;
 Document document;document.replace(scene);ready(document);
 if(resolution>64){
  const auto rendered=document.frame().surface;
  size_t fields=0;
  for(const auto &range:rendered->ranges){
   if(range.object!=target)continue;
   const auto member=texture_set_view(scene.active(),range.texture_set);
   const auto read=member.source->read_view(member);
   const auto expected=detail::PolygonSurfacePaintCodec::preview(
       *read,member.material_bindings,member.material,{},member.source->source_preview());
   const auto equal=[](const Image &a,const Image &b){
    return a.width==b.width&&a.height==b.height&&a.rgba==b.rgba;
   };
   const auto &actual=*range.textures;
   check(equal(actual.color,expected->color)&&equal(actual.metal_rough,expected->metal_rough)&&
         equal(actual.emission,expected->emission)&&equal(actual.color_coverage,expected->color_coverage)&&
         equal(actual.emission_coverage,expected->emission_coverage)&&equal(actual.normal,expected->normal)&&
         equal(actual.mtoon_normal,expected->mtoon_normal)&&actual.transparent==expected->transparent,
         "Large retained preview matches all legacy material, coverage and normal bytes");
   ++fields;
  }
  check(fields==2,"Large native preview publishes both complete retained ranges");
 }
 check(document.can_select_texture_sets(),"Actual retained set selection is available");
 document.texture_set_action(SpatialCommand::TextureNext);ready(document);
 check(document.frame().state->active_texture_set==(id==0?1:0),"The actual next-set command selects the other retained field");
 document.texture_set_action(SpatialCommand::TexturePrevious);ready(document);
 check(document.copy_scene().encode()==baseline,"The previous-set command restores the active field");
 check(document.frame().state->can_bake&&!document.frame().state->can_clear_bake,
       "Actual prepared retained target admits Bake and protects its inherited detail");
 auto stopped=scene.snapshot();
 bool cancelled=false;
 bool interrupted=false;
 try{
  bake_object(stopped,donor,settings,[&]{return cancelled;},[&](float x){cancelled=x>.6f;});
 }catch(const std::runtime_error &error){
  interrupted=std::string_view(error.what()).find("cancelled")!=std::string_view::npos;
 }
 check(cancelled&&interrupted&&stopped.encode()==baseline,
       "Actual cancellation interrupts projection and preserves both role grids");
 if(physical&&resolution>64){
  auto too_short=settings;too_short.front_mm=too_short.back_mm=1;
  bool missed=false;
  try{bake_object(stopped,donor,too_short);}
  catch(const std::runtime_error &error){missed=std::string_view(error.what()).starts_with("Projection missed");}
  check(missed&&stopped.encode()==baseline,
        "Strict affine projection with a short range keeps the original split grids exactly");
 }
 document.bake_from_object(donor,settings);
 const auto file=std::filesystem::path("retained-target.dsculpt");
 document.save_async(file).get();ready(document);
 auto accepted=document.copy_scene();
 const auto saved=accepted.encode();
 check(saved!=baseline&&Scene::load(file).encode()==saved,"Retained Bake saves and reopens exactly");
 const auto current=texture_set_view(accepted.active(),id);
 const auto other=texture_set_view(accepted.active(),id==0?1:0);
 check(current.bake_recipe&&current.bake_recipe->source.object==donor&&
       current.source->original_paint()->baked()==old.source->original_paint()->baked(),
       "New recipe retains the original field owner");
 check(other.source==neighbor.source&&other.painted->baked()==neighbor.painted->baked(),
       "Bake changes only the selected material slot");
 const auto baked=current.painted->baked();
 check(baked->normal_field->prior()==prior&&baked->normal_field->pbr()->rgba!=prior->pbr()->rgba,
       "New normal field descends from the exact original authority");
 check(bool(baked->emission_field)==physical,"Projected FLOAT radiance is retained when present");
 const auto N=current.source->normal(covered,{.2,.3,.5},world,true);
 const auto expected=unit(world.normal({local.x,local.y,local.z}));
 check(length(N-expected)<.018f&&length(N-old_normal)>.08f,
       "Published normal resolves the projected direction under the actual placed basis");
 if(physical&&resolution>64){
  const auto coordinates=current.source->sample(covered,{.2,.3,.5});
  check(coordinates.normal_uv&&coordinates.paint_uv!=*coordinates.normal_uv,
        "Both sampled split-grid coordinates remain independently authored after strict projection");
  const auto emission=baked->emission_field->sample(coordinates.paint_uv[0],coordinates.paint_uv[1]).resolve({},1);
  check(std::abs(emission[0]-3.f)<1e-5f&&std::abs(emission[1]-.1f)<1e-5f&&std::abs(emission[2]-.2f)<1e-5f,
        "Strict reflected projection retains the donor's unclamped physical RGB in the paint grid");
 }
 if(!physical){
  const auto path=std::filesystem::path("retained-target.glb");
  document.export_asset(path,AssetExportFormat::Glb).get();
  cgltf_options options{};
  cgltf_data *data=nullptr;
  check(cgltf_parse_file(&options,path.c_str(),&data)==cgltf_result_success,
        "The normal export worker writes an actual GLB for the baked retained field");
  const std::unique_ptr<cgltf_data,decltype(&cgltf_free)> owner(data,cgltf_free);
  check(cgltf_load_buffers(&options,data,path.c_str())==cgltf_result_success&&
        cgltf_validate(data)==cgltf_result_success&&data->meshes_count==2&&
        data->meshes[0].primitives_count==2&&data->meshes[1].primitives_count==1,
        "Independent cgltf validates the worker's two material primitives");
  check(document.copy_scene().encode()==saved,"Portable export preserves the native Bake state");
 }
 check(document.undo()&&document.copy_scene().encode()==baseline&&
       document.redo()&&document.copy_scene().encode()==saved,
       "Retained Bake has exact Undo and Redo");
 auto forged=saved;
 auto &scene_wire=physical?forged.at("payload"):forged;
 auto &paints=scene_wire.at("texture_set_geometry").at("paints");
 auto member=std::find_if(paints.begin(),paints.end(),[](const auto &p){return p.at("painted").contains("field_bake");});
 check(member!=paints.end(),"The saved current field has an explicit rebake envelope");
 auto &paint=member->at("painted");
 paint.erase("field_bake");
 refuses([&]{Scene::decode(forged);},"An ordinary retained member cannot smuggle in an alternative bake");
 document.clear_bake();ready(document);
 const auto cleared=document.copy_scene();
 const auto field=texture_set_view(cleared.active(),id);
 check(field.painted->baked()==old.painted->baked()&&
       field.bake_recipe->source.object==donor&&!document.frame().state->can_clear_bake,
       "Clear restores inherited detail and keeps the last recipe available");
 check(Scene::decode(cleared.encode()).encode()==cleared.encode()&&
       document.undo()&&document.copy_scene().encode()==saved,"Clear survives cold replay and exact Undo");
}
void bake_multiple_retained_fields(){
 bake_retained_recipient(1,false,1024);
 auto seed=Scene::load("retained-target.dsculpt");
 const auto target=seed.active().id;
 const auto recipe=*active_texture_set(seed.active()).bake_recipe;
 const auto original_donor=seed.object(recipe.source.object);
 auto different=original_donor.painted->mesh();
 const auto direction=normalized(Vec3{-.24f,.11f,-1.f});
 different.normals.assign(different.positions.size(),direction);
 const auto alternate=seed.add("Independent second normal donor",Volume{});
 auto &donor=seed.active();
 donor.mesh_active=true;
 donor.painted=std::make_shared<PaintedMesh>(different,64);
 donor.transform=original_donor.transform;
 donor.material.color={.27f,.62f,.09f,1};
 donor.visible=false;
 donor.locked=true;
 seed.select(target);
 Document document;document.replace(seed);ready(document);
 for(unsigned index=0;index<4;++index){
  const TextureSetId selected=index%2;
  if(document.frame().state->active_texture_set!=selected){
   document.texture_set_action(SpatialCommand::TextureNext);ready(document);
  }
  const auto before=document.copy_scene();
  const auto previous=active_texture_set(before.active());
  const auto neighbor=texture_set_view(before.active(),1-selected);
  const auto source=index<2?original_donor.id:alternate;
  const auto file=std::filesystem::path("multi-"+std::to_string(index)+".dsculpt");
  document.bake_from_object(source,recipe.settings);
  document.save_async(file).get();ready(document);
  const auto accepted=document.copy_scene();
  const auto current=active_texture_set(accepted.active());
  const auto other=texture_set_view(accepted.active(),1-selected);
  const auto field=current.painted->baked()->normal_field;
  const auto prior=previous.painted->baked()->normal_field;
  check(current.id==selected&&field->prior()==prior&&current.bake_recipe->source.object==source&&
        current.source->original_paint()==previous.source->original_paint(),
        "Each Bake records its own recipe and exact normal event on the selected field");
  check(other.source==neighbor.source&&other.painted->baked()==neighbor.painted->baked(),
        "Alternating the active field preserves the independent member");
  check(field->pbr()==field->mtoon()&& &field->normal_coverage()==&prior->normal_coverage(),
        "Equal normal planes and immutable full coverage retain shared storage");
  if(index==1)check(field->pbr()==prior->pbr()&&field->retained_bytes()-prior->retained_bytes()<4096,
        "Repeating the same projection records an event while sharing the byte-identical normal payload");
  if(index>=2){
   const auto triangle=current.domain->triangles().front();
   const auto world=accepted.world_matrix(target);
   const auto normal=current.source->normal(triangle,{.2,.3,.5},world,true);
   check(field->pbr()!=prior->pbr()&&length(normal-direction)<.018f,
         "A different donor changes the selected resolved field and preserves its earlier plane");
  }
  if(index==3){
   const auto exact=accepted.encode();
   check(Scene::load(file).encode()==exact,"The multi-field project cold-reopens exactly");
   check(document.undo()&&document.copy_scene().encode()==before.encode()&&
         document.redo()&&document.copy_scene().encode()==exact,
         "The multi-field Bake has exact Undo and Redo");
   ready(document);
   document.export_asset("multi.glb",AssetExportFormat::Glb).get();
   check(document.copy_scene().encode()==exact,"Standard GLB preserves both editable Bake histories");
  }
 }
}
void bake_retained_native(bool reflected,bool smooth,bool outward=false){
 auto scene=source(false,reflected,false,smooth);
 const auto target=scene.active().id;
 if(!reflected)scene.active().affine_edit.reset();
 select_texture_set(scene,target,1);
 const auto old=active_texture_set(scene.active());
 const auto neighbor=texture_set_view(scene.active(),0);
 const auto world=scene.world_matrix(target);
 const auto &preview=*old.source->source_preview();
 const auto triangle=old.domain->triangles().front();
 const std::array<double,3> covered{.68,.18,.14};
 const std::array<double,3> uncovered{.18,.53,.29};
 const auto before=old.source->normal(triangle,uncovered,world,true);
 check(preview.model()==detail::PolygonSurfaceNormalModel::NativeNOnly&&
       preview.tangents().empty()&&!preview.native_bake(),
       "Native target starts with actual authored normals and no tangent field");
 const auto &mesh=old.painted->mesh();
 Mesh donor_mesh;
 const auto role=preview.normals().at(size_t(triangle)*3);
 auto tilted=normalized(role+Vec3{.19f,-.28f,0});
 if(outward&&dot(unit(world.normal({tilted.x,tilted.y,tilted.z})),before)<0)tilted=tilted*-1;
 std::array<Vec3,3> positions;
 for(size_t k=0;k<3;++k)positions[k]=mesh.positions.at(mesh.indices.at(size_t(triangle)*3+k));
 donor_mesh.positions={positions[0],positions[0]*.44f+positions[1]*.56f,
                       positions[0]*.44f+positions[2]*.56f};
 for(auto &point:donor_mesh.positions)point.z+=.014f;
 donor_mesh.normals.assign(3,tilted);
 donor_mesh.indices={0,1,2};
 donor_mesh.uv={{.1f,.1f},{.9f,.1f},{.1f,.9f}};
 const auto placement=scene.active().affine_edit;
 const auto donor=scene.add("Partly covering native Bake donor",Volume{});
 auto &high=scene.active();
 high.mesh_active=true;
 high.painted=std::make_shared<PaintedMesh>(donor_mesh,64);
 high.affine_edit=placement;
 high.material.color={.21f,.73f,.46f,1};
 if(reflected)create_texture_set(scene,donor,std::array<uint64_t,1>{1},"Donor",64);
 scene.active().visible=false;
 scene.active().locked=true;
 scene.select(target);
 const auto baseline=scene.encode();
 ObjectBakeSettings settings;settings.allow_backfaces=true;settings.allow_partial=true;settings.padding=0;
 settings.ao_samples=2;
 Document document;document.replace(scene);ready(document);
 check(document.frame().state->can_bake&&!document.frame().state->can_clear_bake,
       "Native retained target admits the actual Bake command");
 document.bake_from_object(donor,settings);
 const auto path=std::filesystem::path(outward?"native-target-smooth-portable.dsculpt":"native-target.dsculpt");
 document.save_async(path).get();ready(document);
 const auto accepted=document.copy_scene();
 const auto saved=accepted.encode();
 const auto current=active_texture_set(accepted.active());
 const auto next=current.source->source_preview()->native_bake();
 check(next&&next->width==64&&current.painted->baked()&&current.painted->baked()->maps.normal.rgba.empty(),
       "The bake stores detail separately from the original normal and material owners");
 const auto expected=unit(world.normal({tilted.x,tilted.y,tilted.z}));
 const auto at_hit=current.source->normal(triangle,covered,world,true);
 const auto at_miss=current.source->normal(triangle,uncovered,world,true);
 check(length(at_hit-expected)<.02f,"Retained baked direction matches the authored donor under the placed frame");
 check(length(at_miss-before)<3e-6f,"No projected texel restores the exact smooth inherited normal");
 check(Scene::load(path).encode()==saved&&saved!=baseline,"Native normal Bake reopens through its real Paint7 envelope");
 check(texture_set_view(accepted.active(),0).source==neighbor.source,
       "An unrelated retained member preserves its exact original owner");
 auto malformed=saved;
 auto &paints=malformed.at("texture_set_geometry").at("paints");
 auto slot=std::find_if(paints.begin(),paints.end(),[](const auto &p){
   return p.at("painted").contains("normal_detail");
 });
 check(slot!=paints.end()&&slot->at("painted").at("field_bake")==2,"The native envelope names its actual normal image");
 slot->at("painted")["normal_detail"]=UINT32_MAX;
 refuses([&]{Scene::decode(malformed);},"An absent normal image cannot be borrowed by a native bake");
 if(!smooth||outward){
  const auto surfaces=accepted.export_surfaces();
  const auto portable=detail::make_gltf(surfaces);
  audit_normals(portable,surfaces.front(),.02f);
  if(outward){
   const auto &normal_preview=*current.source->source_preview();
   const auto normal_start=size_t(triangle)*3;
   const auto &normal=normal_preview.normals();
   check(length(normal.at(normal_start)-normal.at(normal_start+1))>1e-3f||
         length(normal.at(normal_start)-normal.at(normal_start+2))>1e-3f,
         "Smooth portable Bake fixture owns genuinely varying recorded vertex normals");
   const auto compiled=detail::compile_texture_set_export(surfaces.front());
   check(compiled.fields.at(1).textures->normal.rgba!=normal_preview.native_bake()->rgba,
         "Varying Bake exports a reprojected image while retaining editable native pixels");
   TextureSetLimits limited;
   limited.maximum_bytes=64*1024;
   refuses([&]{detail::compile_texture_set_export(surfaces.front(),limited);},
           "Retained normal export reserves working storage before compiling");
  }
  const auto verify_file=[&](const std::filesystem::path &glb, AssetExportFormat format){
   document.export_asset(glb,format).get();
   cgltf_options options{};
   cgltf_data *data=nullptr;
   check(cgltf_parse_file(&options,glb.c_str(),&data)==cgltf_result_success,
         "The portable GLB worker exports the native normal bake");
   const std::unique_ptr<cgltf_data,decltype(&cgltf_free)> guard(data,cgltf_free);
   check(cgltf_load_buffers(&options,data,glb.c_str())==cgltf_result_success&&
         cgltf_validate(data)==cgltf_result_success&&data->meshes_count==1&&
         data->meshes[0].primitives_count==2,"Independent glTF validates both material primitives");
   const auto &mapped=data->meshes[0].primitives[1];
   check(mapped.material&&mapped.material->normal_texture.texture&&
         mapped.material->normal_texture.texcoord>0&&
         cgltf_find_accessor(&mapped,cgltf_attribute_type_tangent,0),
         "The actual GLB worker carries mapped tangents and its independent normal coordinates");
  };
  verify_file(smooth?"native-normal-smooth-bake.glb":"native-normal-bake.glb",AssetExportFormat::Glb);
  if(outward)verify_file("native-normal-smooth-editable.glb",AssetExportFormat::EditableGlb256);
 }else{
  const std::filesystem::path target_path="opposed-normal-bake.glb";
  std::filesystem::remove(target_path);
  refuses([&]{document.export_asset(target_path,AssetExportFormat::Glb).get();},
          "A varying normal field outside a portable positive-Z frame fails before writing");
  check(!std::filesystem::exists(target_path),"Failed normal export leaves no partial file");
 }
 check(document.copy_scene().encode()==saved,"Normal export preserves exact native Bake and source data");
 check(document.undo()&&document.copy_scene().encode()==baseline&&
       document.redo()&&document.copy_scene().encode()==saved,"Native retained texture Bake has exact history");
 document.clear_bake();ready(document);
 const auto cleared=document.copy_scene();
 const auto field=active_texture_set(cleared.active());
 check(!field.source->source_preview()->native_bake()&&field.painted->baked()==old.painted->baked()&&
       field.bake_recipe->source.object==donor,"Clear restores original Paint and normal state while keeping Recall");
 check(Scene::decode(cleared.encode()).encode()==cleared.encode()&&
       document.undo()&&document.copy_scene().encode()==saved,"Clear survives cold replay and exact Undo");
}
void recipe_reference_remap(bool prepared) {
 auto scene = source(prepared,false,false,false,true);
 const auto before = scene.encode();
 const auto target = scene.active().id;
 const auto source_id = scene.active().bake_recipe->source.object;
 const auto original = active_texture_set(scene.active());
 check(original.source && original.bake_recipe && original.bake_recipe->source.object == source_id,
       "The real prepared texture field retains its original recipe");
 auto erased = scene.snapshot();
 erased.erase(source_id);
 const auto &erased_object = erased.object(target);
 const auto erased_field = active_texture_set(erased_object);
 check(erased_field.bake_recipe->source.object == unavailable_bake_object &&
       erased_field.bake_recipe->source.original_object == source_id &&
       erased_field.source->read_view(erased_field) == original.source->read_view(original),
       "Prepared-field deletion preserves the exact read view and original source identity");
 check(erased_object.polygon->origin == erased_field.source->polygon_owner()->before() &&
       erased_field.source->source_preview()->matches(erased_field.material_bindings,
           original.source->read_view(original)->paint_sampling_attachment()) &&
       erased_field.source->source_preview()->shares_streams(*original.source->source_preview()),
       "Prepared-field deletion preserves one authenticated origin, attachment and preview streams");
 const auto erased_wire = erased.encode();
 check(Scene::decode(erased_wire).encode() == erased_wire,
       "The prepared native project reopens with its deleted source marker");

 auto appended = append_native_scene(scene,scene);
 const auto &appended_object = appended.scene.active();
 const auto appended_field = active_texture_set(appended_object);
 check(appended_field.bake_recipe->source.object != source_id &&
       appended_field.bake_recipe->source.original_object == source_id &&
       appended.scene.object(appended_field.bake_recipe->source.object).name == scene.object(source_id).name,
       "Same-scene append namespaces the prepared field recipe");
 check(appended_field.source->read_view(appended_field) == original.source->read_view(original) &&
       appended_object.polygon->origin == appended_field.source->polygon_owner()->before() &&
       appended_field.source->source_preview()->shares_streams(*original.source->source_preview()),
       "Same-scene append preserves shared raster and authenticated preview data");
 const auto nested = appended.scene.encode();
 check(Scene::decode(nested).encode() == nested && scene.encode() == before,
       "Both prepared source namespaces and the original native archive reopen exactly");
 const auto &nodes = nested.at("polygon_surfaces").at("nodes");
 size_t variant = 0;
 while (variant < nodes.size() && nodes[variant].at("version") == 1)
   ++variant;
 check(variant < nodes.size() && nodes[variant].at("sampling_node").get<size_t>() < variant,
       "Prepared source namespaces explicitly share an authenticated earlier sampling node");
 auto forged = nested;
 forged["polygon_surfaces"]["nodes"][variant]["sampling_node"] = variant;
 refuses([&]{Scene::decode(forged);},"Shared polygon sampling rejects a self-reference");
 forged = nested;
 auto &foreign = forged["polygon_surfaces"]["nodes"][variant]["recipe"]["operations"][0];
 foreign["delta"][0] = foreign["delta"][0].get<float>() + 0.01f;
 refuses([&]{Scene::decode(forged);},"Shared polygon sampling rejects a different geometric recipe");

}
int main(int argc,char **argv)try {
 const std::string_view mode=argc>1?argv[1]:"";
 if(argc==2&&mode=="--recipe-reference-remap") {
  for(bool prepared:{false,true})recipe_reference_remap(prepared);
  std::cout<<"PASS "<<checks<<" prepared texture recipe controls\n";
  return 0;
 }
 if(argc==3&&mode=="--inspect"){
  std::cout<<nlohmann::json{{"scene",Scene::load(argv[2]).encode()}}.dump()<<'\n';
  return 0;
 }
 if(argc==4&&mode=="--texture-set-xr-fixture"){
  retained_bake_xr_fixture(argv[2],argv[3]);
  return 0;
 }
 if(argc==3&&mode=="--retained-native-target"){
  std::filesystem::create_directories(argv[2]);std::filesystem::current_path(argv[2]);
  bake_retained_native(false,false);
  bake_retained_native(true,true);
  bake_retained_native(true,true,true);
  std::cout<<"PASS "<<checks<<" native-normal retained target checks\n";
  return 0;
 }
 if((argc==3||argc==4)&&(mode=="--retained-hdr-editable"||mode=="--retained-hdr-standard")){
  std::filesystem::create_directories(argv[2]);std::filesystem::current_path(argv[2]);
  const bool standard=mode=="--retained-hdr-standard";
  retained_hdr_export(standard,argc==4?uint32_t(std::stoul(argv[3])):64);
  std::cout<<"PASS "<<checks<<" retained "<<(standard?"Standard":"Editable")<<" HDR controls\n";
  return 0;
 }
 if((argc==3||argc==4||argc==5)&&mode=="--retained-bake-target"){
  std::filesystem::create_directories(argv[2]);std::filesystem::current_path(argv[2]);
  if(argc==5&&std::string_view(argv[4])=="reflected")bake_retained_recipient(0,true,uint32_t(std::stoul(argv[3])));
  else if(argc==4)bake_retained_recipient(1,false,uint32_t(std::stoul(argv[3])));
  else{
   bake_retained_recipient(1,false);
   bake_retained_recipient(0,true);
  }
  std::cout<<"PASS "<<checks<<" retained target Bake controls\n";
  return 0;
 }
 if(argc==3&&mode=="--retained-multi-bake-target"){
  std::filesystem::create_directories(argv[2]);std::filesystem::current_path(argv[2]);
  bake_multiple_retained_fields();
  std::cout<<"PASS "<<checks<<" repeated production retained Bake controls\n";
  return 0;
 }
 if(argc==3&&mode=="--retained-bake-source"){
  std::filesystem::create_directories(argv[2]);std::filesystem::current_path(argv[2]);
  retained_bake_source(false,false);retained_bake_source(true,false);retained_bake_source(false,true);
  retained_bake_source(true,false,true);retained_bake_source(true,false,true,true);
  retained_bake_source(false,true,true,true);
  affine_plain_bake_source(false);affine_plain_bake_source(true);
  affine_bake_target(false,false,false);affine_bake_target(true,false,true);
  affine_bake_target(false,true,true);
  retained_bake_xr_fixture("mixed-before.dsculpt","mixed-baked.dsculpt");
  std::cout<<"PASS "<<checks<<" retained source Bake controls\n";
  return 0;
 }
 if(argc>1){std::filesystem::create_directories(argv[1]);std::filesystem::current_path(argv[1]);}
 for(bool prepared:{false,true})for(bool reflected:{false,true})audit(prepared,reflected);
 for(bool prepared:{false,true})for(bool reflected:{false,true})audit(prepared,reflected,true);
 auto hdr=source(false,false,true);
 refuses([&]{detail::make_gltf(hdr.export_surfaces());},"Actual FLOAT source export retains its precision-qualified per-material guard");
 std::cout<<"PASS "<<checks<<" actual retained standard source export controls\n";
 return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
