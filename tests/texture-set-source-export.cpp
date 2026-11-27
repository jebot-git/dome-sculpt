// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/bake-authoring.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/native-scene-append.hpp"
#include "dome/native-authoring.hpp"
#include "dome/material-visibility.hpp"
#include "dome/emission-native.hpp"
#include "humanoid-sha256.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/gltf-export.hpp"
#include "dome/import-authoring.hpp"
#include "dome/projection.hpp"
#include "dome/rig-edit.hpp"
#include "dome/retopo-authoring.hpp"
#include "dome/source-maps.hpp"
#include "dome/source-placement.hpp"
#include "dome/source-uv-bake.hpp"
#include "dome/tangents.hpp"
#include "texture-set-export.hpp"
#include "texture-set-range.hpp"
#include "texture-set-bake.hpp"
#include "texture-set-field-source.hpp"
#include "polygon-surface-preview.hpp"
#include "baked-normal-field.hpp"
#include "gltf-document.hpp"
#include "cgltf.h"
#include "native-paint-sampling.hpp"
#include "native-surface-heat.hpp"
#include "polygon-surface-owner.hpp"
#include "polygon-surface-paint.hpp"
#include "paint-overlay-sample.hpp"
#include "paint-emission-sample.hpp"
#include "emission-exr-owned.hpp"
#include "emission-paint-access.hpp"
#include "source-color-fixtures.hpp"
#include "source-colors.hpp"
#include "normal-frame-runtime.hpp"
#include "source4-baked.hpp"
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
Scene source(bool prepared,bool reflected,bool floating=false,bool varying=false,bool recipes=false,bool split_grids=false,uint32_t resolution=64,uint8_t coverage=255){
 Mesh m;m.positions={{-.8f,-.6f,0},{.8f,-.6f,0},{.8f,.6f,0},{-.8f,.6f,0}};m.normals.assign(4,{0,0,1});if(varying)m.normals[1]=normalized(Vec3{.2f,.1f,1});m.uv={{{.1f,.1f}},{{.9f,.1f}},{{.9f,.9f}},{{.1f,.9f}}};m.indices={0,1,2,0,2,3};
 Scene s;auto&o=s.active();o.mesh_active=true;o.material.double_sided=true;o.material.color={.4f,.3f,.2f,1};o.painted=std::make_shared<PaintedMesh>(m,resolution);
 if(prepared){BakedTextures bake;bake.maps.color=solid({90,100,110,255},resolution);bake.maps.metal_rough=solid({255,160,0,255},resolution);bake.maps.emission=solid({0,0,0,255},resolution);bake.maps.normal=solid({128,128,255,255},resolution);bake.coverage.assign(size_t(resolution)*resolution,coverage);for(size_t y=0;y<resolution;++y)for(size_t x=0;x<resolution;++x){const auto at=(y*resolution+x)*4;bake.maps.normal.rgba[at]=uint8_t(96+16*x/resolution);bake.maps.normal.rgba[at+1]=uint8_t(112+y/3);bake.maps.normal.rgba[at+2]=240;}o.painted->install_bake(std::move(bake));auto converted=convert_canonical_normals({o.painted,{},{},{},o.material,{}});o.painted=std::move(converted.paint);o.material_bindings=std::move(converted.material_bindings);}
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
void retained_shapes(bool prepared,bool reflected){
 const auto seed=source(prepared,reflected);
 const auto object=seed.active().id;
 const std::string prefix=std::string(prepared?"prepared-":"retained-")+(reflected?"reflected-":"");
 const auto filename=[&](std::string_view suffix){return std::string(prefix)+std::string(suffix);};
 seed.save(filename("before.dsculpt"));
 if(prepared&&!reflected){
  auto neutral=seed.snapshot();
  author_shapekey(neutral,object,{});
  neutral.save(filename("neutral-default.dsculpt"));
 }
 const auto baseline=seed.encode();
 Document doc;
 doc.open_async(filename("before.dsculpt")).get();ready(doc);
 check(doc.frame().state->can_author_shapes,"Cold retained fields expose native shapekey authoring");
 const auto rest_frame=doc.frame().surface;
 const auto creation=doc.copy_scene();
 ShapeKeyEdit create;
 create.name="Retained bend";
 doc.author_shape(create);ready(doc);
 const auto neutral=doc.copy_scene().encode();
 const auto &rest=creation.active().painted->mesh();
 ShapeKeyEdit replace;
 replace.action=ShapeKeyAction::Replace;
 replace.positions=rest.positions;
 for(auto &p:replace.positions)p.z+=p.x>0?.24f:-.15f;
 doc.author_shape(replace);ready(doc);
 const auto authored=doc.copy_scene().encode();
 check(authored!=neutral&&doc.undo()&&doc.copy_scene().encode()==neutral&&
       doc.redo()&&doc.copy_scene().encode()==authored,"Retained shape authoring has exact global Undo and Redo");
 ready(doc);
 doc.edit_scene("Preview retained bend",[object](Scene &s,const auto &cancelled){
  RigEdit edit;edit.morph=MorphWeightEdit{0,1};edit_rig(s,object,edit,cancelled);
 });
 const auto project=filename("shaped.dsculpt");
 doc.save_async(project).get();ready(doc);
 const auto accepted=doc.copy_scene();
 const auto posed_frame=doc.frame().surface;
 const auto exact=accepted.encode();
 const auto world=accepted.world_matrix(object);
 const auto target=accepted.active().deformation->morphs->target_surface(rest,0);
 check(Scene::load(project).encode()==exact&&posed_frame->picking&&posed_frame->ranges.size()==2,
       "Ordered Save and cold reopen preserve authoritative retained shape data");
 for(const auto &range:posed_frame->ranges){
  const auto &field=range.texture_set_proof->field();
  const auto &before=rest_frame->ranges.at(field.id);
  const auto &old=before.textures->polygon_surface,&current=range.textures->polygon_surface;
  const auto triangle=field.domain->triangles().front();
  check(current!=old&&current->shares_streams(*old)&&current->field_owner()==old->field_owner()&&
        current->baked()==old->baked()&&current->geometric_frames()[triangle][0]==old->geometric_frames()[triangle][0]&&
        current->geometric_frames()[triangle][2]!=old->geometric_frames()[triangle][2]&&
        range.textures->color.rgba==before.textures->color.rgba&&
        range.textures->normal.rgba==before.textures->normal.rgba,
        "Both posed fields retain original immutable source streams, maps and before frames");
  range.texture_set_proof->authenticate_stream(posed_frame->mesh,range);
  Vec3 center{};
  for(size_t c=0;c<3;++c){
   const auto logical=range.texture_set_proof->logical_corner(c,range.paint_winding_reversed);
   const auto p=target.positions[rest.indices[logical]];
   const auto q=world.point({p.x,p.y,p.z});
   const Vec3 expected{float(q[0]),float(q[1]),float(q[2])};
   check(length(expected-posed_frame->mesh.positions.at(posed_frame->mesh.indices.at(range.first+c)))<1e-6f,
         "Published material corners use actual placed morph positions");
   center+=expected*(1.f/3);
  }
  const auto hit=posed_frame->picking->raycast_target(center+Vec3{0,0,2},{0,0,-1},3,0,object);
  check(hit&&posed_frame->picking->material_identity(hit->surface.triangle)==SurfaceMaterialIdentity{object,field.id},
        "Posed material picking keeps original logical material membership");
 }
 for(const auto format:{AssetExportFormat::Glb,AssetExportFormat::EditableGlb512}){
  const auto file=filename(format==AssetExportFormat::Glb?"standard.glb":"editable.glb");
  doc.export_asset(file,format).get();
  std::ifstream stream(file,std::ios::binary);
  const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(stream),{}};
  const auto json_length=word(bytes,12);
  detail::GltfDocument parsed;
  parsed.json=nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+json_length);
  parsed.binary.assign(bytes.begin()+28+json_length,bytes.end());
  const auto &gltf_mesh=parsed.json.at("meshes").at(0);
  const auto &primitives=gltf_mesh.at("primitives");
  check(primitives.size()==2&&gltf_mesh.at("weights")==nlohmann::json::array({1})&&
        gltf_mesh.at("extras").at("targetNames").at(0)=="Retained bend",
        "Both material slots export their named live morph");
  for(size_t field=0;field<primitives.size();++field){
   const auto &primitive=primitives[field],&attributes=primitive.at("attributes"),&key=primitive.at("targets").at(0);
   const auto &range=posed_frame->ranges.at(field);
   const auto triangle=range.texture_set_proof->field().domain->triangles().front();
   const auto packet=range.textures->polygon_surface;
   check(key.contains("NORMAL")&&key.contains("TANGENT")==prepared,
         "Portable morph emits direction channels required by the actual material");
   const auto &normal=parsed.json.at("materials").at(primitive.at("material").get<size_t>());
   PaintSourceTexture texture;
   if(prepared){
    texture.image=std::make_shared<const Image>(texture_image(parsed,normal.at("normalTexture")));
    texture.wrap_s=texture.wrap_t=33071;
    texture.min_filter=texture.mag_filter=9729;
   }
   bool changed_normal=false;
   for(size_t c=0;c<3;++c){
    const auto v=index_at(parsed,primitive.at("indices"),c);
    const auto base=vector(attribute(parsed,attributes.at("POSITION"),v,3));
    const auto p=base+vector(attribute(parsed,key.at("POSITION"),v,3));
    size_t logical=0;
    while(logical<3&&length(base-rest.positions[rest.indices[triangle*3+logical]])>1e-6f)++logical;
    check(logical<3&&length(target.positions[rest.indices[triangle*3+logical]]-p)<1e-6f,
          "Binary GLB accessors apply shape displacement to the actual member corner");
    const auto start=vector(attribute(parsed,attributes.at("NORMAL"),v,3));
    const auto direction=vector(attribute(parsed,key.at("NORMAL"),v,3));
    changed_normal|=length(direction)>.05f;
    const auto moved=start+direction;
    const auto n=unit(world.normal({moved.x,moved.y,moved.z}));
    auto decoded=n;
    if(prepared){
     const auto t=attribute(parsed,attributes.at("TANGENT"),v,4);
     const auto q=vector(t)+vector(attribute(parsed,key.at("TANGENT"),v,3));
     const auto tangent=unit(world.direction({q.x,q.y,q.z}));
     const auto bitangent=normalized(cross(n,tangent))*(t[3]*(reflected?-1.f:1.f));
     const auto role=normal.at("normalTexture").value("texCoord",0);
     const auto uv=attribute(parsed,attributes.at("TEXCOORD_"+std::to_string(role)),v,2);
     const auto sample=sample_source_texture(texture,uv[0],uv[1],false);
     decoded=normalized(tangent*(2*sample[0]-1)+bitangent*(2*sample[1]-1)+n*(2*sample[2]-1));
    }
    std::array<double,3> weights{};
    weights[logical]=1;
    check(dot(decoded,packet->normal_at(triangle,weights,world))>.99979f,
          "Independent GLB direction and image decoding matches the posed native material");
   }
   check(changed_normal,"Portable lighting changes on both bent material triangles");
  }
  if(format!=AssetExportFormat::Glb){
   const auto restored=extract_native_authoring(bytes);
   check(restored&&restored->encode()==exact,"Editable GLB recovers the original retained shape and all native materials");
  }
 }
 const auto &range=posed_frame->ranges.front();
 std::array<Vec3,3> p;
 for(size_t c=0;c<3;++c)p[c]=posed_frame->mesh.positions.at(posed_frame->mesh.indices.at(range.first+c));
 const auto center=p[0]*.84f+p[1]*.08f+p[2]*.08f;
 const auto direction=normalized(cross(p[1]-p[0],p[2]-p[0]));
 NativeStrokeSample stroke;
 stroke.origin=center+direction;
 stroke.direction=direction*-1;
 stroke.brush.radius=.6f;
 stroke.brush.pressure=1;
 stroke.brush.strength=1;
 stroke.brush.tool=Tool::Clay;
 doc.configure_shape_sculpt(true);ready(doc);
 check(doc.begin_native_stroke()&&doc.sample_native_stroke(stroke),"Desktop brush admits a real posed retained material hit");
 doc.end_native_stroke(false);
 check(doc.wait_native_stroke(),"Retained shape gesture cancellation drains");ready(doc);
 check(doc.copy_scene().encode()==exact,"Cancelling a retained shape stroke restores the exact scene");
 check(doc.begin_native_stroke()&&doc.sample_native_stroke(stroke),"Desktop brush accepts a second retained material hit");
 doc.end_native_stroke(true);
 doc.save_async(filename("after-stroke.dsculpt")).get();
 check(doc.wait_native_stroke(),"Retained shape gesture acceptance drains");ready(doc);
 const auto sculpted=doc.copy_scene();
 check(sculpted.encode()!=exact&&Scene::load(filename("after-stroke.dsculpt")).encode()==sculpted.encode()&&
       sculpted.active().texture_sets->primary_source()==accepted.active().texture_sets->primary_source(),
       "Accepted stroke and immediate Save carry shape displacement and the exact original material owner");
 check(doc.undo()&&doc.copy_scene().encode()==exact,"One Undo removes the accepted retained stroke");ready(doc);
 doc.configure_shape_sculpt(false);ready(doc);
 ShapeKeyEdit restore;
 restore.action=ShapeKeyAction::Restore;
 doc.author_shape(restore);ready(doc);
 check(doc.copy_scene().encode()==baseline,"Shape Restore returns the exact original retained scene");
}
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
void set_bake_pose(Scene &scene,uint64_t object,std::vector<Vec3> positions){
 ShapeKeyEdit create;create.name="Bake pose";
 author_shapekey(scene,object,create);
 ShapeKeyEdit edit;edit.action=ShapeKeyAction::Replace;edit.positions=std::move(positions);
 author_shapekey(scene,object,edit);
 RigEdit pose;pose.morph=MorphWeightEdit{0,1};
 edit_rig(scene,object,pose);
}
Vec3 posed_source_direction(const TextureSetView &field,const Mesh &posed,
    uint32_t triangle,const std::array<double,3> &bary,const AffineTransform &world){
 // Build the new axes directly from the authored endpoint and original UV
 // gradients. Only the already qualified rest material supplies the sample.
 const auto &packet=*field.source->source_preview();
 const size_t first=size_t(triangle)*3;
 const auto &uv=packet.normal_coordinates();
 const auto e1=posed.positions.at(posed.indices[first+1])-posed.positions.at(posed.indices[first]);
 const auto e2=posed.positions.at(posed.indices[first+2])-posed.positions.at(posed.indices[first]);
 const float du1=uv[first+1][0]-uv[first][0],du2=uv[first+2][0]-uv[first][0];
 const float dv1=uv[first+1][1]-uv[first][1],dv2=uv[first+2][1]-uv[first][1];
 const float inverse=1/(du1*dv2-du2*dv1);
 const auto local_n=cross(e1,e2),local_t=(e1*dv2-e2*dv1)*inverse,local_b=(e2*du1-e1*du2)*inverse;
 const auto n=unit(world.normal({local_n.x,local_n.y,local_n.z}));
 const auto raw_t=unit(world.direction({local_t.x,local_t.y,local_t.z}));
 const auto raw_b=unit(world.direction({local_b.x,local_b.y,local_b.z}));
 const auto t=normalized(raw_t-n*dot(n,raw_t));
 const auto b=cross(n,t)*(dot(cross(n,t),raw_b)<0?-1.f:1.f);
 const auto reference=packet.triangle_frames(triangle,world).second.basis;
 const auto authored=field.source->normal(triangle,bary,world,true);
 return normalized(t*dot(reference.t,authored)+b*dot(reference.b,authored)+n*dot(reference.n,authored));
}
void bake_posed_retained(bool prepared,bool reflected,bool corresponding,bool cage,bool skinned=false){
 auto recipient=source(prepared,reflected,false,false,false,prepared);
 auto donor_scene=source(true,reflected,false,false,false,true);
 if(!reflected){recipient.active().affine_edit.reset();donor_scene.active().affine_edit.reset();}
 auto append=append_native_scene(recipient,donor_scene);
 auto scene=std::move(append.scene);
 const auto target=recipient.active().id,donor=append.objects.at(0);
 scene.select(target);
 select_texture_set(scene,target,1);
 const auto start=active_texture_set(scene.active());
 const auto triangle=start.domain->triangles().front();
 const auto world=scene.world_matrix(target);
 auto low=scene.active().painted->mesh();
 for(auto &v:low.positions)v.z+=.30f+.23f*v.x+.17f*v.y;
 const auto p=low.positions.at(low.indices[triangle*3]);
 const auto e1=low.positions.at(low.indices[triangle*3+1])-p;
 const auto e2=low.positions.at(low.indices[triangle*3+2])-p;
 const auto n=cross(e1,e2);
 const auto normal=unit(world.normal({n.x,n.y,n.z}));
 const auto delta=world.inverse().direction({.014*normal.x,.014*normal.y,.014*normal.z});
 const Vec3 offset{float(delta[0]),float(delta[1]),float(delta[2])};
 auto high=low;
 for(auto &v:high.positions)v+=offset;
 set_bake_pose(scene,target,low.positions);
 set_bake_pose(scene,donor,high.positions);
 if(skinned){
  for(const auto id:{target,donor}){
   auto &object=scene.object(id);
   auto deformation=std::make_shared<DeformationState>(*object.deformation);
   deformation->skeleton.emplace();
   deformation->skeleton->bones.push_back(Bone{"Bake root"});
   deformation->skin.emplace();
   deformation->skin->vertices.resize(object.painted->mesh().positions.size(),{{0,1}});
   deformation->pose={JointTransform{{0,0,.15f},{},{1,1,1}}};
   object.deformation=std::move(deformation);
  }
  for(auto &v:low.positions)v.z+=.15f;
  for(auto &v:high.positions)v.z+=.15f;
 }
 for(TextureSetId id:{0u,1u})
  check(edit_texture_set(scene,donor,id,[id](PaintedMesh &paint,Material &material){
   material.emissive={};
   paint.add_layer("Exact posed FLOAT donor");
   Brush brush;brush.from=brush.to={};brush.normal={0,0,1};brush.radius=4;brush.strength=1;
   PaintSettings ink;ink.front_only=false;ink.channel=PaintChannel::Emission;
   ink.emission_units=PaintEmissionUnits::Physical;
   ink.emission=id?std::array<float,3>{3.f,.125f,.5f}:std::array<float,3>{.5f,7.f,1.f};
   paint.begin_stroke();paint.stamp(brush,ink);return paint.end_stroke();
  }),"Both retained donor members own distinct unclamped FLOAT emission");
 scene.object(donor).visible=false;
 scene.object(donor).locked=true;
 const auto sample_field=texture_set_view(scene.object(donor),1);
 constexpr std::array<double,3> bary{.22,.33,.45};
 const auto expected=posed_source_direction(sample_field,high,triangle,bary,world);
 check(length(expected-sample_field.source->normal(triangle,bary,world,true))>.12f,
       "Independent source oracle detects a stale donor rest basis");
 ObjectBakeSettings settings;
 settings.corresponding_topology=corresponding;
 settings.allow_backfaces=true;
 settings.ao_samples=corresponding?0:2;
 settings.padding=0;
 settings.front_mm=settings.back_mm=24;
 if(cage){
  const auto placement=scene.active().affine_edit;
  auto cage_mesh=low;
  for(auto &v:cage_mesh.positions)v+=offset*1.5f;
  settings.cage_object=scene.add("Posed cage with ordered vertices",Volume{});
  auto &object=scene.active();
  object.mesh_active=true;object.visible=false;object.affine_edit=placement;
  object.painted=std::make_shared<PaintedMesh>(std::move(cage_mesh),64);
 }
 scene.select(target);
 const auto baseline=scene.encode();
 scene.save("before.dsculpt");
 const auto probe=[&](uint64_t id){
  auto stale=scene.snapshot();
  RigEdit zero;zero.morph=MorphWeightEdit{0,0};
  stale.object(id).visible=true;stale.object(id).locked=false;
  edit_rig(stale,id,zero);
  const auto original=stale.encode();
  refuses([&]{bake_object(stale,donor,settings);},"A neutral endpoint alone misses the differently placed partner");
  check(stale.encode()==original,"Failed posed projection preserves the original pose and material fields");
 };
 if(!cage){probe(donor);probe(target);}
 auto cancelled=scene.snapshot();
 bool stop=false;
 refuses([&]{bake_object(cancelled,donor,settings,[&]{return stop;},[&](float progress){stop=progress>.25f;});},
         "Active posed Bake honours cancellation inside projection");
 check(stop&&cancelled.encode()==baseline,"Cancelled posed Bake preserves both meshes, maps, and recipes");
 Document document;
 document.open_async("before.dsculpt").get();ready(document);
 check(document.frame().state->can_bake,"Cold Open exposes the active retained Bake target");
 const auto before=document.copy_scene();
 const auto cold=before.encode();
 check(cold==baseline,"Cold Open preserves the exact scene and shares retained source seeds across history");
 document.bake_from_object(donor,settings);
 document.save_async("baked.dsculpt").get();ready(document);
 const auto saved=document.copy_scene();
 const auto exact=saved.encode();
 check(exact!=baseline&&Scene::load("baked.dsculpt").encode()==exact,
       "Ordered Save cold-reopens the two morphs and their editable Bake");
 const auto selected=active_texture_set(saved.active());
 const auto original=active_texture_set(before.active());
 const auto untouched=texture_set_view(saved.active(),0);
 check(selected.source->original_paint()==original.source->original_paint()&&
       untouched.source==texture_set_view(before.active(),0).source&&
       saved.active().deformation==before.active().deformation&&
       saved.object(donor).deformation==before.object(donor).deformation,
       "Bake preserves both authored morph owners, the original surface and the sibling field");
 const auto &maps=selected.painted->baked();
 check(maps&&maps->emission_field&&selected.bake_recipe&&
       selected.bake_recipe->source.object==donor&&
       selected.bake_recipe->settings.corresponding_topology==corresponding&&
       selected.bake_recipe->settings.cage_object==settings.cage_object,
       "The selected member carries exact HDR channels and a replayable Bake recipe");
 check(prepared?maps->normal_field&&maps->normal_field->prior()==original.painted->baked()->normal_field:
                bool(selected.source->source_preview()->native_bake()),
       "Posed Bake keeps the target's actual prepared or native normal authority");
 const auto &surface=*document.frame().surface;
 const auto range=std::ranges::find_if(surface.ranges,[&](const SurfaceRange &r){
  return r.object==target&&r.texture_set==1;
 });
 check(range!=surface.ranges.end(),"Actual viewport publishes the rebaked posed range");
 const auto n_baked=range->textures->polygon_surface->normal_at(triangle,bary,world);
 check(length(n_baked-expected)<.022f,"Baked native posed lighting matches the independent transformed donor direction");
 const auto paint_uv=selected.source->sample(triangle,bary).paint_uv;
 const auto emission=maps->emission_field->sample(paint_uv[0],paint_uv[1]).resolve({},1);
 const auto donor_uv=sample_field.source->sample(triangle,bary).paint_uv;
 const auto donor_maps=detail::PolygonSurfacePaintCodec::preview(
     *sample_field.source->read_view(sample_field),sample_field.material_bindings,sample_field.material);
 const auto donor_channels=detail::sample_overlay_values(*donor_maps->overlay,donor_uv[0],donor_uv[1],false);
 const auto radiance=BakedEmissionSample{{donor_channels[2][0],donor_channels[2][1],donor_channels[2][2]},
     {donor_channels[4][0],donor_channels[4][1],donor_channels[4][2]},donor_channels[3][3]}.resolve({},1);
 check(radiance[0]>1&&std::abs(emission[0]-radiance[0])<.003f&&
       std::abs(emission[1]-radiance[1])<.003f&&std::abs(emission[2]-radiance[2])<.003f,
       "Baked FLOAT radiance comes from the genuine posed material member");
 for(const auto &[file,format]:{
     std::pair{"posed-standard.glb",AssetExportFormat::Glb},
     std::pair{"posed-editable.glb",AssetExportFormat::EditableGlb512}}){
  document.export_asset(file,format).get();
  cgltf_options options{};
  cgltf_data *data=nullptr;
  check(cgltf_parse_file(&options,file,&data)==cgltf_result_success,"Portable worker exports the posed Bake");
  const std::unique_ptr<cgltf_data,decltype(&cgltf_free)> owner(data,cgltf_free);
  check(cgltf_load_buffers(&options,data,file)==cgltf_result_success&&
        cgltf_validate(data)==cgltf_result_success&&data->meshes_count==1&&
        data->meshes[0].primitives_count==2&&data->meshes[0].weights_count==1&&
        data->meshes[0].weights[0]==1.f,"Independent glTF consumer sees the two material slots and active morph");
  const auto &mapped=data->meshes[0].primitives[1];
  check(mapped.material&&mapped.material->normal_texture.texture&&mapped.targets_count==1&&
        cgltf_find_accessor(&mapped,cgltf_attribute_type_tangent,0),
        "Portable posed Bake carries the actual normal image, tangents and morph");
  std::ifstream stream(file,std::ios::binary);
  const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(stream),{}};
  const auto restored=format==AssetExportFormat::Glb?extract_native_emission(bytes):extract_native_authoring(bytes);
  check(restored&&restored->encode()==exact,"Portable files preserve the editable posed Bake and hidden donors");
 }
 check(document.undo()&&document.copy_scene().encode()==cold,
       "Posed Bake Undo restores the entire cold-open scene");
 check(document.redo()&&document.copy_scene().encode()==exact,"Posed Bake Redo restores the exact bake");ready(document);
 document.clear_bake();ready(document);
 const auto cleared=active_texture_set(document.copy_scene().active());
 check(cleared.painted->baked()==original.painted->baked()&&cleared.bake_recipe&&
       cleared.bake_recipe->source.object==donor,"Clear restores inherited detail on the active pose and keeps Recall");
 document.copy_scene().save("recall.dsculpt");
 document.bake_from_object(donor,cleared.bake_recipe->settings);ready(document);
 check(document.copy_scene().encode()==exact,"Rebaking the recalled live pose reproduces the exact project");
}
void skin_retained_materials(){
 auto scene=source(true,false,false,false,false,true);
 const auto target=scene.active().id;
 scene.active().affine_edit.reset();
 auto shaped=scene.active().painted->mesh();
 for(auto &v:shaped.positions)v.z+=.12f*v.x+.09f*v.y;
 set_bake_pose(scene,target,shaped.positions);
 const auto rest=scene.active().painted->mesh();
 const auto rig=scene.add("Two-joint retained material rig",Volume{});
 auto &reference=scene.active();
 reference.mesh_active=true;
 reference.visible=false;
 reference.painted=std::make_shared<PaintedMesh>(rest,64);
 auto native=std::make_shared<DeformationState>(rest);
 native->skeleton.emplace();
 Bone root{"Root"};
 root.rest.translation={-.65f,-.42f,0};
 root.length=.62f;
 Bone tip{"Tip"};
 tip.parent=0;
 tip.rest.translation={1.3f,0,0};
 tip.length=.62f;
 native->skeleton->bones={root,tip};
 reference.deformation=native;
 scene.select(target);
 select_texture_set(scene,target,1);
 const auto initial=scene.encode();
 scene.save("before-bind.dsculpt");
 const auto cold=Scene::load("before-bind.dsculpt");
 const std::array targets{target};
 detail::NativeSurfaceHeatOptions bounds;
 const auto retained=detail::native_weight_retention(cold,bounds.maximum_total-bounds.maximum_workspace);
 const auto &field=texture_set_view(cold.active(),1);
 check(field.source&&field.source->original_paint()&&field.source->source_preview()&&
       retained.bytes>field.source->original_paint()->storage_bytes(),
       "Cold Surface Heat retention accounts for the actual shared field and its ancestor paint");
 bounds.maximum_total=bounds.maximum_workspace+retained.bytes;
 auto admitted=detail::capture_native_surface_heat(cold,targets,rig,bounds);
 check(admitted.matches(cold),"Exact retained byte limit admits the complete cold source graph");
 --bounds.maximum_total;
 bool refused_bound=false;
 try{(void)detail::capture_native_surface_heat(cold,targets,rig,bounds);}
 catch(const std::length_error &){refused_bound=true;}
 check(refused_bound&&cold.encode()==initial,
       "One byte below the retained limit refuses the real graph before staging");
 Document document;
 document.replace(scene);ready(document);
 SkinAuthoringOptions bind;
 bind.mode=SkinAuthoringMode::SurfaceHeat;
 bind.reference=rig;
 bind.seed_distance=1.1f;
 check(document.frame().state->can_bind_skin,"Current retained mesh exposes the ordinary Bind Skin workflow");
 document.bind_skin(bind);
 document.save_async("bound.dsculpt").get();ready(document);
 const auto bound=document.copy_scene();
 const auto &skin=*bound.active().deformation;
 const auto exact_bind=bound.encode();
 check(skin.skeleton&&skin.skin&&bound.active().texture_sets==scene.active().texture_sets&&
       skin.morphs->encode()==scene.active().deformation->morphs->encode(),
       "Surface Heat binds the actual retained mesh without replacing its source, UVs or authored shape");
 check(Scene::load("bound.dsculpt").encode()==exact_bind&&document.undo()&&
       document.copy_scene().encode()==initial&&document.redo()&&document.copy_scene().encode()==exact_bind,
       "Native retained Bind Skin has exact history and cold reopen");ready(document);
 auto modify_pose=[&](RigEdit edit){
  document.edit_scene("Pose retained material mesh",[=](Scene &candidate,const auto &cancelled){
   edit_rig(candidate,target,edit,cancelled);
  });
  ready(document);
 };
 RigEdit flex;
 flex.joint=JointAdjustment{1,{},{0,0,.26f}};
 modify_pose(flex);
 const auto flexed=document.copy_scene();
 auto independent=shaped;
 const auto &rows=flexed.active().deformation->skin->vertices;
 for(size_t v=0;v<independent.positions.size();++v)
  for(const auto influence:rows[v])if(influence.joint==1)independent.positions[v].z+=.26f*influence.weight;
 const auto surface=document.frame().surface;
 check(surface->ranges.size()==2&&surface->picking,"Skinned retained preview publishes both material fields and picking");
 for(const auto &range:surface->ranges){
  const auto &proof=*range.texture_set_proof;
  const auto packet=range.textures->polygon_surface;
  const auto &field=proof.field();
  const auto triangle=field.domain->triangles().front();
  Vec3 center{};
  for(size_t c=0;c<3;++c){
   const auto logical=proof.logical_corner(c,range.paint_winding_reversed);
   const auto p=independent.positions[rest.indices[logical]];
   check(length(p-surface->mesh.positions[surface->mesh.indices[range.first+c]])<3e-6f,
         "Posed retained corner matches the independent two-joint displacement");
   center+=p*(1.f/3);
  }
  constexpr std::array<double,3> b{1./3,1./3,1./3};
  check(length(packet->normal_at(triangle,b,AffineTransform{})-
               posed_source_direction(field,independent,triangle,b,AffineTransform{}))<3e-6f&&
        packet->shares_streams(*field.source->source_preview()),
        "Nonrigid skinning transports the original material direction over each deformed triangle");
  const auto hit=surface->picking->raycast_target(center+Vec3{0,0,1},{0,0,-1},2,0,target);
  check(hit&&surface->picking->material_identity(hit->surface.triangle)==SurfaceMaterialIdentity{target,field.id},
        "Posed retained picking preserves the exact material membership");
 }
 const auto flexed_exact=flexed.encode();
 auto expected_refusal=std::filesystem::path("uncertified.glb");
 {std::ofstream old(expected_refusal);old<<"previous";
 }
 {
  Document refusal;
  refusal.replace(flexed);ready(refusal);
  bool rejected=false;
  try{refusal.export_asset(expected_refusal,AssetExportFormat::Glb).get();}
  catch(const std::invalid_argument &error){
   rejected=std::string_view(error.what()).find("portable angular limit in the active skin pose")!=std::string_view::npos;
  }
  check(rejected,"Export refuses the actual portable normal mismatch in a nonrigid pose");
 }
 {std::ifstream previous(expected_refusal);std::string value;previous>>value;
  check(value=="previous"&&document.copy_scene().encode()==flexed_exact,
        "Refusal preserves the previous output and the complete editable pose");
 }
 auto first=rows.front();
 float joint0=0;
 for(const auto influence:first)if(influence.joint==0)joint0=influence.weight;
 WeightToolSettings weights;
 weights.enabled=true;weights.linked=false;
 document.weight_settings(weights,joint0<.5f?0:1);ready(document);
 Brush stamp;
 stamp.from=stamp.to=independent.positions.front();
 stamp.radius=.07f;
 check(document.begin_desktop(),"Weight painting acquires the skinned retained mesh");
 document.stamp_desktop(stamp);
 document.cancel_desktop();
 check(document.copy_scene().encode()==flexed_exact,"Cancelling a retained weight gesture preserves its source");
 check(document.begin_desktop(),"A second retained weight gesture can begin");
 document.stamp_desktop(stamp);document.end_desktop();ready(document);
 const auto weighted=document.copy_scene();
 check(weighted.active().deformation->skin->vertices.front()!=first&&
       weighted.active().texture_sets==bound.active().texture_sets,
       "Weight painting changes influences and preserves each retained field exactly");
 weights.enabled=false;
 document.weight_settings(weights);
 RigEdit rigid;
 rigid.reset_pose=true;
 rigid.joint=JointAdjustment{0,{0,18,0},{.05f,0,.11f}};
 modify_pose(rigid);
 const auto ready_scene=document.copy_scene();
 const auto portable_history=ready_scene.encode();
 document.save_async("rigid-pose.dsculpt").get();
 check(Scene::load("rigid-pose.dsculpt").encode()==portable_history,
       "Cold native Open keeps both skin weights and shape data with their material history");
 for(const auto &[file,format]:{
     std::pair{"skinned-standard.glb",AssetExportFormat::Glb},
     std::pair{"skinned-editable.glb",AssetExportFormat::EditableGlb512}}){
  document.export_asset(file,format).get();
  cgltf_options options{};
  cgltf_data *data=nullptr;
  check(cgltf_parse_file(&options,file,&data)==cgltf_result_success,"Portable file emits the certified skinned material mesh");
  const std::unique_ptr<cgltf_data,decltype(&cgltf_free)> owner(data,cgltf_free);
  check(cgltf_load_buffers(&options,data,file)==cgltf_result_success&&cgltf_validate(data)==cgltf_result_success&&
        data->skins_count==1&&data->skins[0].joints_count==2&&data->meshes[0].primitives_count==2,
        "Independent consumer sees the actual two-joint rig and two material primitives");
  for(size_t f=0;f<2;++f){
   const auto &primitive=data->meshes[0].primitives[f];
   check(primitive.targets_count==1&&primitive.material->normal_texture.texture&&
         cgltf_find_accessor(&primitive,cgltf_attribute_type_weights,0)&&
         cgltf_find_accessor(&primitive,cgltf_attribute_type_joints,0),
         "Both portable primitives preserve independent normal detail, skin and morph channels");
  }
  if(format==AssetExportFormat::EditableGlb512){
   std::ifstream stream(file,std::ios::binary);
   const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(stream),{}};
   const auto replay=extract_native_authoring(bytes);
   check(replay&&replay->encode()==portable_history,"Editable GLB restores the exact skinned project");
  }
 }
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
size_t check_native_mikk_glb(const char *path, const Mesh &rest, const DeformationState *deformation=nullptr) {
 cgltf_options options{};
 cgltf_data *raw=nullptr;
 check(cgltf_parse_file(&options,path,&raw)==cgltf_result_success,"An independent consumer opens the native Bake GLB");
 std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
 check(cgltf_load_buffers(&options,raw,path)==cgltf_result_success&&cgltf_validate(raw)==cgltf_result_success&&
       raw->meshes_count==1&&raw->meshes[0].primitives_count==1,"The emitted single primitive passes independent structural validation");
 const auto &primitive=raw->meshes[0].primitives[0];
 const auto position=cgltf_find_accessor(&primitive,cgltf_attribute_type_position,0);
 const auto normal=cgltf_find_accessor(&primitive,cgltf_attribute_type_normal,0);
 const auto tangent=cgltf_find_accessor(&primitive,cgltf_attribute_type_tangent,0);
 const auto uv=cgltf_find_accessor(&primitive,cgltf_attribute_type_texcoord,0);
 const auto weights=cgltf_find_accessor(&primitive,cgltf_attribute_type_weights,0);
 const auto joints=cgltf_find_accessor(&primitive,cgltf_attribute_type_joints,0);
 check(primitive.material&&primitive.material->normal_texture.texture&&position&&normal&&tangent&&uv&&
       primitive.indices->count==rest.indices.size()&&tangent->count==position->count,
       "GLB carries its Bake normal map and an explicit tangent for every emitted vertex");
 const auto tangents=tangent_corners(rest);
 const auto read=[&](const cgltf_accessor *accessor,size_t vertex,auto &values) {
  check(accessor&&cgltf_accessor_read_float(accessor,vertex,values.data(),values.size()),"A portable vertex attribute has its expected width");
 };
 const auto current=deformation&&deformation->morphs?&*deformation->morphs:nullptr;
 check(primitive.targets_count==(current?current->targets.size():0),"The portable mesh has every native morph endpoint");
 std::vector<Mesh> shaped;
 std::vector<std::vector<Tangent>> shaped_tangents;
 for(size_t key=0;key<primitive.targets_count;++key){
  shaped.push_back(current->target_surface(rest,key));
  shaped_tangents.push_back(tangent_corners(shaped.back()));
 }
 for(size_t c=0;c<rest.indices.size();++c){
  const auto source=rest.indices[c];
  const auto emitted=cgltf_accessor_read_index(primitive.indices,c);
  std::array<float,3> p,n;
  std::array<float,4> t;
  std::array<float,2> q;
  read(position,emitted,p);read(normal,emitted,n);read(tangent,emitted,t);read(uv,emitted,q);
  const auto &a=rest.positions[source], &b=rest.normals[source];
  check(p==std::array{a.x,a.y,a.z}&&n==std::array{b.x,b.y,b.z}&&t==tangents[c]&&q==rest.uv[source],
        "Every ordered GLB corner preserves the native rest POSITION, NORMAL, TANGENT and UV");
  if(deformation&&deformation->skin){
   std::array<float,4> w,j;
   read(weights,emitted,w);read(joints,emitted,j);
   const auto &influences=deformation->skin->vertices[source];
   for(size_t k=0;k<influences.size();++k)
    check(w[k]==influences[k].weight&&j[k]==influences[k].joint,"Shared corners duplicate their true skin influences");
  }
  for(size_t key=0;key<primitive.targets_count;++key){
   cgltf_primitive attribute_view{};
   attribute_view.attributes=primitive.targets[key].attributes;
   attribute_view.attributes_count=primitive.targets[key].attributes_count;
   std::array<float,3> dp,dn,dt;
   read(cgltf_find_accessor(&attribute_view,cgltf_attribute_type_position,0),emitted,dp);
   read(cgltf_find_accessor(&attribute_view,cgltf_attribute_type_normal,0),emitted,dn);
   read(cgltf_find_accessor(&attribute_view,cgltf_attribute_type_tangent,0),emitted,dt);
   const auto &mp=shaped[key].positions[source],&mn=shaped[key].normals[source];
   const auto &mt=shaped_tangents[key][c];
   check(length(Vec3{p[0]+dp[0],p[1]+dp[1],p[2]+dp[2]}-mp)<2e-7f&&
         length(Vec3{n[0]+dn[0],n[1]+dn[1],n[2]+dn[2]}-mn)<2e-7f&&
         length(Vec3{t[0]+dt[0]-mt[0],t[1]+dt[1]-mt[1],t[2]+dt[2]-mt[2]})<2e-7f&&t[3]==mt[3],
         "Portable POSITION, NORMAL and TANGENT deltas reconstruct the actual native endpoint and hand");
  }
 }
 return position->count;
}

void export_mirrored_mikk() {
 Mesh mesh;
 mesh.positions={{0,0,0},{1,0,0},{0,1,0},{-1,0,0},{0,-1,0}};
 mesh.normals.assign(5,{0,0,1});
 mesh.uv={{.5f,.5f},{.9f,.5f},{.5f,.9f},{.5f,.1f},{.1f,.5f}};
 mesh.indices={0,1,2,0,3,4};
 const auto mikk=tangent_corners(mesh);
 check(mikk[0][3]!=mikk[3][3],"The same rest vertex really touches two charts with opposite tangent hands");
 Scene scene;
 auto &object=scene.active();
 object.mesh_active=true;
 object.painted=std::make_shared<PaintedMesh>(mesh,64);
 BakedTextures bake;
 bake.maps.color=solid({77,124,89,255});
 bake.maps.normal=solid({167,71,218,255});
 bake.maps.metal_rough=solid({212,111,44,255});
 bake.maps.emission=solid({0,0,0,255});
 bake.coverage.assign(64*64,255);
 object.painted->install_bake(std::move(bake));
 export_glb(scene.export_surfaces(),"mirrored-static.glb");
 check(check_native_mikk_glb("mirrored-static.glb",mesh)==6,
       "Static export keeps both channel hands without changing the native mesh");
 DeformationState deformation(mesh);
 deformation.skeleton=Skeleton{{{"Root",-1,{},.4f},{"Tip",0,{},.4f}}};
 deformation.skin=SkinWeights{};
 deformation.skin->vertices={{{0,.6f},{1,.4f}},{{1,1}},{{0,1}},{{0,1}},{{1,1}}};
 deformation.morphs=MorphSet(mesh);
 auto shape=mesh.positions;shape[1].z=.13f;shape[3].z=-.07f;
 deformation.morphs->set_target(mesh,"Twist left",shape);
 shape=mesh.positions;shape[2].z=.23f;shape[4].y-=.11f;
 deformation.morphs->set_target(mesh,"Lift edge",shape);
 deformation.morph_weights={0,1};
 scene.set_deformation(object.id,deformation);
 const auto original=scene.encode();
 Document document;document.replace(scene);ready(document);
 document.export_asset("mirrored-standard.glb",AssetExportFormat::Glb).get();
 document.export_asset("mirrored-editable.glb",AssetExportFormat::EditableGlb256).get();
 for(const auto *name:{"mirrored-standard.glb","mirrored-editable.glb"})
  check(check_native_mikk_glb(name,mesh,&deformation)==6,"Only the genuinely opposite-handed shared corner splits into two vertices");
 check(document.copy_scene().encode()==original&&Scene::load("mirrored-editable.glb").encode()==original,
       "Portable splits leave the live and reloaded Editable geometry and paint untouched");
 std::cout<<"MIKK_EXPORT native_vertices=5 portable_vertices=6 endpoints=2\n";
}

void retopology_imported_pbr(bool prior_bake=false) {
 const auto bytes=source_color_fixture::bytes();
 const size_t json_size=word(bytes,12);
 detail::GltfDocument input;
 input.json=nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+json_size);
 input.binary.assign(bytes.begin()+28+json_size,bytes.end());
 auto &attributes=input.json["meshes"][0]["primitives"][0]["attributes"];
 const std::array<float,8> alternate{.83f,.67f,.07f,.8f,.14f,.1f,.94f,.2f};
 const auto append_uv=[&] {
  const auto start=input.binary.size();
  for(float value:alternate){
   const auto bits=std::bit_cast<uint32_t>(value);
   for(size_t b=0;b<4;++b)input.binary.push_back(uint8_t(bits>>(8*b)));
  }
  auto &views=input.json["bufferViews"];
  const auto id=views.size();
  views.push_back({{"buffer",0},{"byteOffset",start},{"byteLength",input.binary.size()-start},{"target",34962}});
  auto &accessors=input.json["accessors"];
  const auto index=accessors.size();
  accessors.push_back({{"bufferView",id},{"componentType",5126},{"count",4},{"type","VEC2"}});
  return index;
 };
 attributes["TEXCOORD_1"]=append_uv();
 auto &material=input.json["materials"][0];
 auto &pbr=material["pbrMetallicRoughness"];
 pbr["baseColorTexture"]={{"index",0},{"texCoord",1},{"extensions",{{"KHR_texture_transform",{{"offset",{.12f,-.09f}},{"rotation",.23f},{"scale",{.8f,.65f}}}}}}};
 pbr["metallicFactor"]=.6f;
 pbr["metallicRoughnessTexture"]={{"index",0}};
 material["occlusionTexture"]={{"index",0},{"texCoord",1},{"strength",.37f}};
 if(prior_bake) {
  material["normalTexture"]["texCoord"]=1;
  material["normalTexture"]["extensions"]["KHR_texture_transform"]={{"rotation",.31f},{"scale",{.72f,1.13f}}};
 }
 material["emissiveTexture"]={{"index",0},{"texCoord",1}};
 material["emissiveFactor"]={.75f,.3f,.1f};
 material["extensions"]["KHR_materials_emissive_strength"]["emissiveStrength"]=6;
 material["doubleSided"]=true;
 input.json["extensionsUsed"]={"KHR_texture_transform","KHR_materials_emissive_strength"};
 input.json["meshes"][0]["weights"]={.8f};
 input.json["nodes"][0]["matrix"]={-1.05,0,0,0,0,.86,0,0,0,0,1.07,0,.03,.06,.09,1};
 input.json["buffers"][0]["byteLength"]=input.binary.size();
 detail::write_glb(std::move(input),"original-import.glb");
 ImportAuthoringOptions options;options.paint_resolution=64;options.prepare_texture_atlas=true;
 auto imported=prepare_authoring_import(Scene{},"original-import.glb",options);
 check(imported.objects.size()==1,"The real GLB authoring entry point imports one posed PBR object");
 auto scene=std::move(imported.scene);
 const auto id=imported.objects.front();
 scene.select(id);
 auto &guide=scene.active();
 auto shear=AffineTransform{}.matrix();
 shear[1]=.12;shear[4]=.17;shear[9]=.1;
 guide.affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(shear));
 const auto source_world=scene.world_matrix(id);
 const auto &rest=guide.painted->mesh();
 const auto &maps=*guide.painted->source_maps();
 check(guide.appearance&&guide.material_bindings->normal_frames&&guide.deformation&&
       guide.deformation->morph_weights[0]>.7f&&maps.color.texcoord!=0&&maps.color.image&&maps.normal.image&&
       maps.occlusion.image&&maps.metal_rough.image&&maps.emission.image&&guide.material.emissive[0]>1,
       "Authentic import retains independent sampling roles, authored frames, morph and physical radiance");
 guide.painted->add_layer("Imported editable pigment");
 PaintSettings ink;ink.front_only=false;ink.color={.8f,.3f,.16f,.7f};
 Brush brush;brush.from=brush.to={0,0,0};brush.radius=2;brush.normal={0,0,1};brush.strength=.71f;
 guide.painted->begin_stroke();guide.painted->stamp(brush,ink);
 check(guide.painted->end_stroke(),"The imported source has its own nonempty editable paint layer");
 guide.painted->layer_properties("Imported editable pigment",false,.37f);
 if(prior_bake) {
  BakedTextures layers;
  layers.maps.color=solid({122,55,168,201});
  layers.maps.metal_rough=solid({207,61,103,255});
  layers.maps.normal=solid({142,196,233,255});
  layers.coverage.resize(4096);
  std::vector<BakedEmissionSample> physical(4096);
  for(size_t i=0;i<physical.size();++i){
   const auto x=i%64,y=i/64;
   layers.coverage[i]=uint8_t(38+x*3);
   layers.maps.color.rgba[i*4]=uint8_t(55+x*2);
   layers.maps.normal.rgba[i*4+1]=uint8_t(136+y);
   const float a=.21f+.006f*float(y);
   physical[i]={{.07f*a,0,.03f*a},{(.2f+.048f*float(x))*a,.089f*a,.6f*a},a};
  }
  layers.emission_field=BakedEmissionField::create(64,std::move(physical));
  guide.painted=detail::install_source_bake(*guide.painted,std::move(layers));
  check(guide.painted->encode().at("version")==8&&guide.painted->valid_source_sampling_attachment(),
        "The imported guide owns its genuine source-aware Bake and independently filtered FLOAT data");
 }
 for(const auto &o:scene.objects())if(o.id!=id)scene.object(o.id).visible=false;
 scene.save("raw-guide.dsculpt");
 const auto original=scene.encode();
 const auto geometry=guide.deformation->preview(rest);
 const auto source_tangents=prior_bake?tangent_corners(geometry):std::vector<Tangent>{};
 const auto source_packet=prior_bake?detail::PaintOverlayFactory::capture(*guide.painted,{}):nullptr;

 Document document;
 document.replace(scene);ready(document);
 document.begin_retopology();
 const auto draft=[&] {
  check(document.wait_retopology(),"Retopology edit settles");
  const auto view=document.frame().state->retopology;
  if(view&&!view->error.empty())throw std::runtime_error(view->error);
 };
 draft();
 const auto to_world=[&](Vec3 p){const auto q=source_world.point({p.x,p.y,p.z});return Vec3{float(q[0]),float(q[1]),float(q[2])};};
 // Trace a quad whose interior crosses the imported triangulation diagonal.
 for(auto corner:geometry.positions){
  check(document.begin_desktop(),"Desktop gesture admits a point on the imported geometry");
  Brush pick;pick.from=pick.to=to_world(corner*.84f);pick.radius=.01f;
  document.stamp_desktop(pick);document.end_desktop();draft();
 }
 document.edit_retopology(RetopoAction::Face);draft();
 document.end_retopology(true);
 document.save_async("raw-patch.dsculpt").get();ready(document);
 const auto result=document.copy_scene();
 const auto accepted=result.encode();
 const auto &patch=result.active();
 check(patch.retopology&&patch.retopology->source==id&&patch.painted->baked()&&
       !patch.appearance&&!patch.material_bindings&&!result.object(id).visible&&
       result.object(id).appearance->original_bytes==guide.appearance->original_bytes&&
       result.object(id).painted->source_maps()==guide.painted->source_maps(),
       "Apply produces a native Bake and the hidden guide retains its authentic images and original GLB");
 if(prior_bake)
  check(result.object(id).painted->baked()==guide.painted->baked()&&
        result.object(id).painted->source_sampling_original()==guide.painted->source_sampling_original(),
       "The separate hidden guide keeps the exact prior Bake, coverage and original paint owner");
 const auto &before=guide.painted->layers().back(),&after=patch.painted->layers().back();
 check(before.id==after.id&&before.name==after.name&&before.visible==after.visible&&before.opacity==after.opacity&&
       std::ranges::any_of(after.tiles,[](const auto &tile){return tile&&std::ranges::any_of(tile->pixels,[](const auto &p){return p.data[9]>110;});}),
       "The projected editable ink remains separate and retains its active layer properties");

 const auto &target=patch.painted->mesh();
 const auto target_world=result.world_matrix(patch.id);
 const auto relative=source_world.inverse()*target_world;
 const auto target_tangents=tangent_corners(target);
 const auto colors=detail::source_vertex_colors(guide.appearance.get(),rest.positions.size());
 detail::NormalFrameLayout layout;layout.vertices=rest.indices;
 for(uint32_t c=0;c<rest.indices.size();++c)layout.corners.push_back(c);
 const auto posed=detail::normal_frame_preview(*guide.material_bindings->normal_frames,layout,guide.deformation.get(),{},1);
 const auto &baked=*patch.painted->baked();
 const auto &image=baked.maps.color;
 const SurfaceQuery query(geometry);
 size_t samples=0,partial=0;
 float peak=0,min_dot=1;
 std::array<bool,2> source_triangles{};
 for(size_t c=0;c<target.indices.size();c+=3){
  const auto vi=target.indices[c],vj=target.indices[c+1],vk=target.indices[c+2];
  const auto a=target.uv[vi],b=target.uv[vj],d=target.uv[vk];
  const std::array u{b[0]-a[0],b[1]-a[1]},v{d[0]-a[0],d[1]-a[1]};
  const float area=u[0]*v[1]-u[1]*v[0];
  for(size_t y=3;y<image.height;y+=5)for(size_t x=4;x<image.width;x+=5){
   const auto at=y*image.width+x;
   const float px=(x+.5f)/image.width-a[0],py=(y+.5f)/image.height-a[1];
   const float wb=(px*v[1]-py*v[0])/area,wc=(u[0]*py-u[1]*px)/area,wa=1-wb-wc;
   if(std::min({wa,wb,wc})<.07f)continue;
   const auto p=target.positions[vi]*wa+target.positions[vj]*wb+target.positions[vk]*wc;
   const auto q=relative.point({p.x,p.y,p.z});
   const auto hit=query.nearest(Vec3{float(q[0]),float(q[1]),float(q[2])},SurfaceQueryOptions{.closest_maximum=.01f});
   check(hit.has_value()&&baked.coverage[at]==255,"An interior patch texel covers its actual imported surface");
   source_triangles.at(hit->triangle)=true;
   const auto role=[&](const auto &texture,bool srgb){return sample_surface_texture(rest,maps,texture,hit->triangle,hit->barycentric,srgb);};
   const auto color=role(maps.color,true),orm=role(maps.metal_rough,false),light=role(maps.emission,true),ao=role(maps.occlusion,false);
   float coverage=0,emission_coverage=0;
   std::array<float,7> rendered_bake{};
   std::array<float,3> rendered_emission{};
   Vec3 normal_detail{};
   if(prior_bake){
    const auto &previous=*guide.painted->baked();
    const auto uv=source_texture_uv(rest,maps,0,hit->triangle,hit->barycentric);
    const float sx=uv[0]*64-.5f,sy=uv[1]*64-.5f;
    const int x0=int(std::floor(sx)),y0=int(std::floor(sy));
    const float fx=sx-x0,fy=sy-y0;
    for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){
     const float weight=(dx?fx:1-fx)*(dy?fy:1-fy);
     const auto owner=source_packet->padding()->at(size_t(std::clamp(y0+dy,0,63))*64+std::clamp(x0+dx,0,63));
     if(owner<0)continue;
     const auto sample=size_t(owner)*4;
     const float a=weight*(previous.coverage[owner]/255.f);
     coverage+=a;
     for(size_t k=0;k<4;++k){const float c=previous.maps.color.rgba[sample+k]/255.f;rendered_bake[k]+=a*(k<3?srgb_to_linear(c):c);}
     for(size_t k=0;k<3;++k){
      rendered_bake[k+4]+=a*(previous.maps.metal_rough.rgba[sample+k]/255.f);
      const auto emission=previous.emission_field->texel(owner);
      rendered_emission[k]+=weight*(emission.legacy[k]+emission.absolute[k]);
     }
     emission_coverage+=weight*previous.emission_field->texel(owner).coverage;
     const auto *p=previous.maps.normal.rgba.data()+sample;
     normal_detail+=Vec3{p[0]*2.f/255-1,p[1]*2.f/255-1,p[2]*2.f/255-1}*a;
    }
    partial+=coverage>.12f&&coverage<.9f&&std::abs(coverage-emission_coverage)>.01f;
   }
   for(size_t k=0;k<4;++k){
    float tint=0;
    for(size_t j=0;j<3;++j)tint+=colors[rest.indices[hit->triangle*3+j]][k]*hit->barycentric[j];
    const auto expected=(guide.material.color[k]*color[k]*(1-coverage)+rendered_bake[k])*tint;
    const auto raw=image.rgba[at*4+k]/255.f;
    check(std::abs((k<3?srgb_to_linear(raw):raw)-expected)<.006f,"Native Bake retains sampled linear colour and vertex alpha");
   }
   const auto *pixel=baked.maps.metal_rough.rgba.data()+at*4;
   check(std::abs(pixel[0]/255.f-((1-maps.occlusion_strength*(1-ao[0]))*(1-coverage)+rendered_bake[4]))<.003f&&
         std::abs(pixel[1]/255.f-(guide.material.roughness*orm[1]*(1-coverage)+rendered_bake[5]))<.003f&&
         std::abs(pixel[2]/255.f-(guide.material.metallic*orm[2]*(1-coverage)+rendered_bake[6]))<.003f,"Separate original AO, roughness and metallic roles survive Apply");
   const auto radiance=baked.emission_field->texel(at).absolute;
   for(size_t k=0;k<3;++k)
    check(std::abs(radiance[k]-(guide.material.emissive[k]*light[k]*(1-emission_coverage)+rendered_emission[k]))<2e-5f,
          "Physical emission with its independent coverage survives the transfer without RGB8 clipping");
   const auto normal=role(maps.normal,false);
   Vec3 n,t,bitangent;
   for(size_t corner=0;corner<3;++corner){
    const auto i=hit->triangle*3+corner;
    const auto nn=posed.normals[0].at(i);
    const auto tt=posed.tangents[0].at(i);
    const auto wn=unit(source_world.normal({nn.x,nn.y,nn.z}));
    const auto wt=unit(source_world.direction({tt[0],tt[1],tt[2]}));
    const float weight=hit->barycentric[corner];
    n+=wn*weight;t+=wt*weight;bitangent+=cross(wn,wt)*(-tt[3]*weight);
   }
   auto expected=normalized(normalized(n)*(normal[2]*2-1)+
       (normalized(t)*(normal[0]*2-1)+normalized(bitangent)*(normal[1]*2-1))*maps.normal_scale);
   if(prior_bake){
    const auto first=size_t(hit->triangle)*3;
    const auto a=geometry.indices[first],b=geometry.indices[first+1],c=geometry.indices[first+2];
    const auto normal_uv=source_texture_coordinates(rest,maps,maps.normal.texcoord);
    const auto transform=maps.normal.uv_transform;
    const auto role_uv=[&](uint32_t vertex){const auto uv=normal_uv[vertex];return std::array{transform[0]*uv[0]+transform[1]*uv[1]+transform[4],transform[2]*uv[0]+transform[3]*uv[1]+transform[5]};};
    const auto p=role_uv(a),q=role_uv(b),r=role_uv(c);
    const float det=(q[0]-p[0])*(r[1]-p[1])-(q[1]-p[1])*(r[0]-p[0]);
    const auto e=to_world(geometry.positions[b])-to_world(geometry.positions[a]),f=to_world(geometry.positions[c])-to_world(geometry.positions[a]);
    const auto N=normalized(n);
    auto T=(e*(r[1]-p[1])-f*(q[1]-p[1]))/det;
    T=normalized(T-N*dot(N,T));
    auto B=cross(N,T);
    if(dot(B,(f*(q[0]-p[0])-e*(r[0]-p[0]))/det)<0)B=B*-1;
    expected=normalized(N*(normal[2]*2-1)+(T*(normal[0]*2-1)+B*(normal[1]*2-1))*maps.normal_scale);
    if(coverage>0){
     Vec3 cn,ct,cb;
     for(size_t k=0;k<3;++k){
      const auto v=geometry.indices[first+k];const auto gn=geometry.normals[v];
      const auto gt=source_tangents[first+k];
      const auto nn=unit(source_world.normal({gn.x,gn.y,gn.z})),tt=unit(source_world.direction({gt[0],gt[1],gt[2]}));
      const auto weight=hit->barycentric[k];cn+=nn*weight;ct+=tt*weight;cb+=cross(nn,tt)*(-gt[3]*weight);
     }
     const auto current=normalized(ct*normal_detail.x+cb*normal_detail.y+cn*normal_detail.z);
     expected=normalized(expected*(1-coverage)+current*coverage);
    }
   }
   const auto *normal_pixel=baked.maps.normal.rgba.data()+at*4;
   const auto local=tangent_frame(target,target_tangents,c/3,{wa,wb,wc}).to_world(
       {normal_pixel[0]*2.f/255-1,normal_pixel[1]*2.f/255-1,normal_pixel[2]*2.f/255-1});
   min_dot=std::min(min_dot,dot(expected,unit(target_world.normal({local.x,local.y,local.z}))));
   peak=std::max(peak,radiance[0]);
   ++samples;
  }
 }
 check(samples>30&&peak>1&&std::ranges::all_of(source_triangles,[](bool a){return a;})&&min_dot>.9997f&&(!prior_bake||partial>30),
       "Interior controls cover both source triangles with original HDR and posed reflected shading");
 std::cout<<"IMPORTED_PBR interior="<<samples<<" peak="<<peak<<" min_normal_dot="<<min_dot<<" partial="<<partial<<'\n';
 check(Scene::load("raw-patch.dsculpt").encode()==accepted&&document.undo()&&document.copy_scene().encode()==original&&
       document.redo()&&document.copy_scene().encode()==accepted,"Native Save, cold Open, Undo and Redo preserve the source and patch exactly");
 ready(document);
 document.export_asset("raw-standard.glb",AssetExportFormat::Glb).get();
 document.export_asset("raw-editable.glb",AssetExportFormat::EditableGlb256).get();
 check_native_mikk_glb("raw-standard.glb",target);
 check_native_mikk_glb("raw-editable.glb",target);
 check(Scene::load("raw-editable.glb").encode()==accepted,"Editable GLB restores the complete imported-source project");
}

void retopology_retained_materials(bool reflected) {
 auto scene=source(true,reflected,false,false,true,true,64,reflected?200:255);
 const auto guide=scene.active().id;
 if(!reflected)scene.active().affine_edit.reset();
 const auto placement=scene.world_matrix(guide);
 uint64_t imported_donor=0;
 if(!reflected) {
  Scene donor;
  auto mesh=scene.active().painted->mesh();
  for(auto &p:mesh.positions)p.z+=.012f;
  auto &o=donor.active();o.mesh_active=true;o.painted=std::make_shared<PaintedMesh>(mesh,64);
  BakedTextures maps;
  maps.maps.color=solid({55,157,98,255});
  maps.maps.normal=solid({161,112,241,255});
  maps.maps.metal_rough=solid({233,110,28,255});
  maps.maps.emission=solid({0,0,0,255});
  maps.coverage.assign(64*64,255);
  for(size_t i=0;i<64*64;++i)maps.maps.color.rgba[i*4]=uint8_t(52+i%64);
  o.painted->install_bake(std::move(maps));
  export_glb(donor.export_surfaces(),"imported-pbr-donor.glb");
  ImportAuthoringOptions options;options.paint_resolution=64;
  auto imported=prepare_authoring_import(scene,"imported-pbr-donor.glb",options);
  check(imported.objects.size()==1,"Actual GLB importer publishes one mapped PBR donor");
  imported_donor=imported.objects.front();
  scene=std::move(imported.scene);
  check(scene.object(imported_donor).appearance && scene.object(imported_donor).appearance->original_bytes &&
        scene.object(imported_donor).painted->source_maps()->color.image &&
        scene.object(imported_donor).painted->source_maps()->normal.image,
        "Imported source owns its original scene bytes, diffuse image and tangent-space normal image");
  scene.object(imported_donor).visible=false;
  scene.select(guide);
  select_texture_set(scene,guide,1);
  ObjectBakeSettings bake;bake.ao_samples=0;bake.allow_backfaces=true;
  bake_object(scene,imported_donor,bake);
  check(texture_set_view(scene.active(),1).bake_recipe->source.object==imported_donor,
        "The prepared secondary slot receives an actual material Bake from the imported GLB");
 }
 for(TextureSetId id : {0u,1u}) {
  check(edit_texture_set(scene,guide,id,[id,reflected](PaintedMesh &paint,Material &) {
   Brush brush;
   brush.from=brush.to={0,0,0};brush.normal={0,0,1};brush.radius=2;brush.strength=.85f;
   if(reflected) {
    paint.add_layer("Retained floating light");
    PaintSettings glow;glow.channel=PaintChannel::Emission;glow.emission_units=PaintEmissionUnits::Physical;
    glow.front_only=false;glow.emission={3,.01f,1};
    paint.begin_stroke();paint.stamp(brush,glow);
    if(!paint.end_stroke())return false;
   }
   paint.add_layer(id?"Second retained pigment":"Hidden retained pigment");
   PaintSettings settings;
   settings.front_only=false;
   settings.color=id?std::array<float,4>{.18f,.35f,.97f,1}:std::array<float,4>{.86f,.29f,.12f,1};
   paint.begin_stroke();paint.stamp(brush,settings);
   if(!paint.end_stroke())return false;
   if(!id)paint.layer_properties("Hidden retained pigment",false,.55f);
   return true;
  }),"Each original field owns an independent visible or hidden authored layer");
 }
 for(const auto &o:scene.objects())if(o.id!=guide)scene.object(o.id).visible=false;
 const auto exact=scene.encode();
 scene.save("prepared-guide.dsculpt");
 const auto &source_mesh=scene.active().painted->mesh();
 const auto to_world=[&](Vec3 p){const auto q=placement.point({p.x,p.y,p.z});return Vec3{float(q[0]),float(q[1]),float(q[2])};};

 Document document;
 document.replace(scene);ready(document);
 document.begin_retopology();
 check(document.wait_retopology() && document.frame().state->retopology->source,"Native retopology opens the actual prepared guide");
 const auto wait_draft=[&] {
  check(document.wait_retopology() && document.frame().state->retopology->error.empty(),"Retopology edit completed");
 };
 for(TextureSetId slot : {0u,1u}) {
  const auto owner=texture_set_view(scene.active(),slot);
  check(owner.source && owner.source->source_preview()->normal_coordinates()!=owner.source->source_preview()->paint_coordinates(),
        "Prepared slot has genuinely separate normal and paint coordinates");
  for(const auto face:owner.domain->triangles()) {
   std::array<Vec3,3> points;
   for(size_t k=0;k<3;++k)points[k]=source_mesh.positions[source_mesh.indices[face*3+k]];
   const auto centre=(points[0]+points[1]+points[2])/3.f;
   for(auto corner:points) {
    check(document.begin_desktop(),"Desktop admits a real retopology point gesture");
    Brush brush;brush.from=brush.to=to_world(centre+(corner-centre)*.84f);brush.radius=.01f;
    document.stamp_desktop(brush);document.end_desktop();wait_draft();
   }
   document.edit_retopology(RetopoAction::Face);wait_draft();
  }
 }
 check(document.frame().state->retopology->topology->faces().size()==2,"Native worker constructs one new triangle on each material region");
 document.end_retopology(true);
 document.save_async("prepared-retopo.dsculpt").get();ready(document);
 const auto after=document.copy_scene();
 const auto &patch=after.active();
 check(patch.retopology && patch.retopology->source==guide && !patch.polygon && patch.texture_sets &&
       !patch.texture_sets->topology()->polygon_ids() && !after.object(guide).visible &&
       after.object(guide).texture_sets==scene.active().texture_sets,
       "Transferred patch owns a new triangle atlas and retains the exact hidden source collection");
 SurfaceQuery query(source_mesh);
 const auto target_world=after.world_matrix(patch.id);
 const auto source_from_target=placement.inverse()*target_world;
 const auto &target=patch.painted->mesh();
 const auto target_tangents=tangent_corners(target);
 size_t checked_normals=0;
 float min_dot=1;
 for(TextureSetId id : {0u,1u}) {
  const auto old=texture_set_view(scene.active(),id), field=texture_set_view(patch,id);
  const auto &old_layer=old.painted->layers().back(), &new_layer=field.painted->layers().back();
  check(!field.source && !field.material_bindings && field.domain->triangles().size()==1 &&
        field.painted->baked() && !field.painted->baked()->normal_field &&
        field.bake_recipe==old.bake_recipe && old_layer.id==new_layer.id && old_layer.name==new_layer.name &&
        old_layer.opacity==new_layer.opacity && old_layer.visible==new_layer.visible,
        "Each projected field keeps the painted layers, Bake recipe and native shading independently of the guide");
  check(std::ranges::any_of(new_layer.tiles,[id](const auto &tile) {
   return tile && std::ranges::any_of(tile->pixels,[id](const auto &pixel) {
    return pixel.data[9]>120 && pixel.data[id?2:0]>180 && pixel.data[id?0:2]<155;
   });
  }),"The destination has actual independent source layer samples");
  if(reflected){
   check(field.painted->has_physical_emission(),"The original floating light layer remains editable in each projected material");
   const auto &coverage=field.painted->baked()->coverage;
   check(std::ranges::any_of(coverage,[](auto a){return a==200;}) &&
         std::ranges::all_of(coverage,[](auto a){return !a || a==200;}),
         "Reorientation of the prepared normals preserves fractional material Bake coverage");
  }
  const auto composite=detail::EmissionExportAccess::other_channels(*field.painted,field.material,{});
  const auto triangle=field.domain->triangles().front();
  const auto a=target.uv[target.indices[3*triangle]],b=target.uv[target.indices[3*triangle+1]],c=target.uv[target.indices[3*triangle+2]];
  const std::array u{b[0]-a[0],b[1]-a[1]},v{c[0]-a[0],c[1]-a[1]};
  const float area=u[0]*v[1]-u[1]*v[0];
  const auto size=composite->normal.width;
  check(size==field.painted->resolution() && field.painted->baked()->coverage.size()==size_t(size)*size,
        "The new patch has an actual native normal map and corresponding material coverage");
  for(size_t y=0;y<size;y+=3)for(size_t x=0;x<size;x+=3) {
   const float dx=(float(x)+.5f)/size-a[0],dy=(float(y)+.5f)/size-a[1];
   const float w1=(dx*v[1]-dy*v[0])/area,w2=(u[0]*dy-u[1]*dx)/area;
   const std::array weights{1-w1-w2,w1,w2};
   if(std::ranges::min(weights)<.12f)continue;
   Vec3 p{};for(size_t i=0;i<3;++i)p+=target.positions[target.indices[triangle*3+i]]*weights[i];
   const auto q=source_from_target.point({p.x,p.y,p.z});
   const auto hit=query.closest({float(q[0]),float(q[1]),float(q[2])});
   check(old.domain->contains(hit.triangle),"Independent sampler finds the same actual source material for the output texel");
   const auto &s=hit.barycentric;
   const auto expected=old.source->normal(hit.triangle,{s[0],s[1],s[2]},placement,true);
   const auto *rgb=composite->normal.rgba.data()+4*(y*size+x);
   const auto local=tangent_frame(target,target_tangents,triangle,weights).to_world({rgb[0]/127.5f-1,rgb[1]/127.5f-1,rgb[2]/127.5f-1});
   const auto actual=unit(target_world.normal({local.x,local.y,local.z}));
   min_dot=std::min(min_dot,dot(expected,actual));
   ++checked_normals;
  }
 }
 if(imported_donor)
  check(after.object(imported_donor).appearance==scene.object(imported_donor).appearance &&
        texture_set_view(patch,1).bake_recipe->source.object==imported_donor,
        "Retopology and ordered Save retain the original imported asset and its Bake relationship");
 std::cout<<"RETOPO_NORMAL samples="<<checked_normals<<" min_dot="<<min_dot<<'\n';
 check(checked_normals>20 && min_dot>.9997f,"An independent retained PBR sampler agrees with both resampled canonical normal grids");
 const auto accepted=after.encode();
 check(Scene::load("prepared-retopo.dsculpt").encode()==accepted && document.undo()&&document.copy_scene().encode()==exact&&
       document.redo()&&document.copy_scene().encode()==accepted,"Ordered Save, cold Open and Undo/Redo preserve the prepared guide and both new slots");
 ready(document);
 document.export_asset("prepared-editable.glb",AssetExportFormat::EditableGlb256).get();
 check(Scene::load("prepared-editable.glb").encode()==accepted,"Editable GLB reopens both the original prepared fields and their projected patch");
 document.export_asset("prepared-standard.glb",AssetExportFormat::Glb).get();
 cgltf_options options{};cgltf_data *data=nullptr;
 check(cgltf_parse_file(&options,"prepared-standard.glb",&data)==cgltf_result_success,"Independent GLB reader opens the new two-slot patch");
 std::unique_ptr<cgltf_data,decltype(&cgltf_free)> ownership(data,cgltf_free);
 check(cgltf_load_buffers(&options,data,"prepared-standard.glb")==cgltf_result_success && cgltf_validate(data)==cgltf_result_success &&
       data->meshes_count==1 && data->meshes[0].primitives_count==2 &&
       data->meshes[0].primitives[0].material->normal_texture.texture && data->meshes[0].primitives[1].material->normal_texture.texture,
       "Standard GLB carries two independent native normal textures on one retopologized mesh");
}

int main(int argc,char **argv)try {
 const std::string_view mode=argc>1?argv[1]:"";
 if(argc==3&&mode=="--native-mikk-export") {
  std::filesystem::create_directories(argv[2]);
  std::filesystem::current_path(argv[2]);
  export_mirrored_mikk();
  std::cout<<"PASS "<<checks<<" native Mikk export checks\n";
  return 0;
 }
 if(argc==3&&(mode=="--imported-retopology"||mode=="--imported-prior-bake")) {
  std::filesystem::create_directories(argv[2]);
  std::filesystem::current_path(argv[2]);
  retopology_imported_pbr(mode=="--imported-prior-bake");
  std::cout<<"PASS "<<checks<<" imported material retopology checks\n";
  return 0;
 }
 if(argc==4&&mode=="--retained-retopology"){
  std::filesystem::create_directories(argv[2]);
  std::filesystem::current_path(argv[2]);
  retopology_retained_materials(std::string_view(argv[3])=="affine");
  std::cout<<"PASS "<<checks<<" retained material retopology checks\n";
  return 0;
 }
 if(argc==3&&mode=="--retained-skin"){
  const auto out=std::filesystem::absolute(argv[2]);
  std::filesystem::create_directories(out/"skin");
  std::filesystem::current_path(out/"skin");
  skin_retained_materials();
  std::filesystem::create_directories(out/"bake");
  std::filesystem::current_path(out/"bake");
  bake_posed_retained(true,true,false,true,true);
  std::cout<<"PASS "<<checks<<" retained skin, material and Bake controls\n";
  return 0;
 }
 if(argc==3&&mode=="--posed-retained-bake"){
  const auto out=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(out);
  for(const auto &[leaf,prepared,reflected,corresponding,cage]:{
      std::tuple{"native-ray",false,false,false,false},
      std::tuple{"prepared-matching",true,true,true,false},
      std::tuple{"prepared-cage",true,true,false,true}}){
   std::filesystem::create_directories(out/leaf);std::filesystem::current_path(out/leaf);
   bake_posed_retained(prepared,reflected,corresponding,cage);
  }
  std::cout<<"PASS "<<checks<<" posed retained Bake controls\n";
  return 0;
 }
 if(argc==3&&mode=="--retained-shapes"){
  std::filesystem::create_directories(argv[2]);std::filesystem::current_path(argv[2]);
  for(bool prepared:{false,true})for(bool reflected:{false,true})retained_shapes(prepared,reflected);
  std::cout<<"PASS "<<checks<<" retained texture-set shapekey controls\n";
  return 0;
 }
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
