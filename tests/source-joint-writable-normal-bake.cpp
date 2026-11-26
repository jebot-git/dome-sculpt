// SPDX-License-Identifier: GPL-3.0-or-later
#define main joint_prepared_baseline_main
#include "source-prepared-normal-bake.cpp"
#undef main
#include "current-color-target-fixture.hpp"
#include "polygon-color-target.hpp"
#include "source-writable-paint.hpp"
#include "emission-paint-access.hpp"
#include "source-owning-filtered-workflow.hpp"
#include "baked-emission-domain.hpp"
#include <optional>
namespace joint_source {
size_t checks=0;
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);++checks;}
// Exact historical source construction and legacy corpus use recorded policy0.
ObjectBakeSettings settings(){ObjectBakeSettings s;s.normal_filter_quality=0;s.emission_transport=0;s.prepare_colors=s.prepare_normals=true;s.normal_error=.03f;s.front_mm=s.back_mm=500;s.ao_samples=0;s.padding=0;return s;}
Scene prepared(bool seam){auto s=current_color_target_fixture::fixture(seam,64);
 // Author colour before explicit preparation. The genuine preparation then
 // creates distinct legacy and PBR stroke values from the actual corner field.
 // A full first Bake replaces the base; this visible retained layer proves
 // the independent colour field remains present above that replacement.
 auto paint=s.active().painted->snapshot();PaintSettings color;color.front_only=false;color.channel=PaintChannel::Color;color.color={.8f,.2f,.55f,.75f};Brush brush;brush.from=brush.to={0,0,.2f};brush.normal={0,0,1};brush.radius=.18f;brush.strength=.9f;paint->begin_stroke();paint->stamp(brush,color);require(paint->end_stroke(),"Actual raw current-corner colour stroke precedes preparation and first Bake");s.active().painted=paint;s.validate();
 const auto source=s.objects().back().id;cut_color_fixture::pose(s);auto d=*s.active().deformation;d.pose.clear();d.morph_weights.assign(d.morphs->targets.size(),0);s.set_deformation(s.active().id,d);auto m=s.object(source).painted->mesh();for(size_t v=0;v<m.positions.size();++v){const auto p=m.positions[v];m.normals[v]=normalized(m.normals[v]+Vec3{.45f*p.y,.6f*p.x,.33f*p.z});}s.object(source).painted=std::make_shared<PaintedMesh>(m,64);require(bake_object(s,source,settings()).projected>500,"Genuine first native Bake prepares independent actual corner colour and normal planes");require(s.active().painted->normal_projection()&&s.active().painted->color_projection()&&s.active().polygon&&s.active().polygon->colors,"Actual joint native owning fields");return s;}
void uv(Scene&s,size_t replay_cap=256*1024*1024){const auto&o=s.active();auto mesh=o.painted->mesh();std::vector<uint32_t> vertices(mesh.positions.size());std::iota(vertices.begin(),vertices.end(),0);for(auto&q:mesh.uv){const auto x=q[0];q={.2f+.6f*q[1],.8f-.6f*x};}auto derivative=prepare_polygon_uv_derivative(o,mesh,vertices,{});SourceUvBakeInput input;input.paint=o.painted;input.current_material=o.material_bindings;input.current_deformation=o.deformation;input.polygon_authority=o.polygon;input.target_polygon=derivative.authority;input.target=derivative.mesh;input.source_vertices=derivative.source_vertices;SourceUvBakeOptions options;options.max_working_bytes=replay_cap;options.normal_sampling=SourceNormalUvMethod::RetainSource;options.max_normal_sample_error=.03;auto adopted=adopt_source_uv_bake(bake_source_uv(input,options));auto&a=s.active();a.painted=adopted.paint;a.material_bindings=adopted.material_bindings;a.deformation=adopted.deformation;a.appearance=adopted.appearance;a.polygon=derivative.authority;s.validate();require(a.painted->paint_sampling_domain()&&a.material_bindings->color_preparation->paint_sampling&&a.material_bindings->normal_preparation->sampling_domain->metadata_sampling_domain==a.painted->paint_sampling_domain(),"Actual combined Source8 normal metadata and independent current corner paint share their genuine geometry attachment");}
void pose(Scene&s,bool live){if(!live)return;auto d=*s.active().deformation;d.morph_weights={.35f};d.pose.resize(2);d.pose[0].scale={1.2f,.8f,.7f};d.pose[1].scale={.8f,1.1f,1.3f};s.set_deformation(s.active().id,d);s.validate();}
void stroke(Scene&s){auto p=s.active().painted->snapshot();p->add_layer("Actual joint physical paint");PaintSettings ink;ink.front_only=false;ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={8,.03f,2.5f};Brush b;b.from=b.to={0,0,.2f};b.normal={0,0,1};b.radius=.18f;b.strength=1;p->begin_stroke();p->stamp(b,ink);require(p->end_stroke()&&p->has_physical_emission(),"Actual post-UV joint sparse FLOAT stroke");s.active().painted=p;s.validate();}
D decode(const SculptObject&o,size_t tri,std::array<float,3>bary,size_t role,bool wrong_domain=false,bool omit=false){
 const auto &b=*o.material_bindings;const auto &set=*b.normal_frames;const auto &f=role==0?*set.pbr:*set.mtoon;const auto &mesh=o.painted->mesh();D N{},T{},B{};std::array<double,2>uv{};
 const auto projection=o.painted->normal_projection();const auto domain=projection->sampling_domain;const auto raster=b.normal_preparation->uv_reprojection;
 for(size_t k=0;k<3;++k){const size_t corner=tri*3+k;const auto v=mesh.indices[corner];D n=vc(f.normals[corner]),t{f.tangents[corner][0],f.tangents[corner][1],f.tangents[corner][2]};
  if(o.deformation){for(size_t slot=0;slot<set.current_targets.size();++slot){const double w=o.deformation->morph_weights.empty()?0:o.deformation->morph_weights[slot];for(const auto&target:f.targets)if(target.id==set.current_targets[slot]){if(!target.normals.empty())n=add(n,mul(vc(target.normals[corner]),w));if(!target.tangents.empty())t=add(t,mul(vc(target.tangents[corner]),w));}}
   if(!o.deformation->pose.empty()){D nn{},tt{};for(auto influence:o.deformation->skin->vertices[v]){const auto a=o.deformation->pose[0].scale,c=o.deformation->pose[1].scale;D scale{a.x,a.y,a.z};if(influence.joint==1)scale={a.x*c.x,a.y*c.y,a.z*c.z};for(size_t axis=0;axis<3;++axis){nn[axis]+=n[axis]/scale[axis]*influence.weight;tt[axis]+=t[axis]*scale[axis]*influence.weight;}}n=nn;t=tt;}}
  n=unit(n);t=unit(t);N=add(N,mul(n,bary[k]));T=add(T,mul(t,bary[k]));B=add(B,mul(unit(crossd(n,t)),bary[k]*f.tangents[corner][3]));
  auto q=mesh.uv[v];if(domain&&!wrong_domain){const auto &coords=role&&!domain->mtoon_coordinates.empty()?domain->mtoon_coordinates:domain->coordinates;q=coords[domain->indices[corner]];}else if(!domain&&wrong_domain&&raster){const auto &before=raster->before_paint->mesh();q=before.uv[before.indices[corner]];}for(size_t axis=0;axis<2;++axis)uv[axis]+=q[axis]*bary[k];
 }
 if(role==0){N=unit(N);T=unit(T);B=unit(B);}else if(b.material.version()==MToonVersion::V1){N=unit(N);T=unit(T);B=mul(unit(crossd(N,T)),f.tangents[tri*3][3]);}
 if(omit)return unit(N);
 // These are the declared live renderer planes, distinct from Bake's normal
 // capture. v2's image remains in its original domain; no atlas rebake here.
 std::shared_ptr<const TextureMaps> shown;const Image *image=nullptr;
 if(!domain){shown=detail::EmissionExportAccess::other_channels(*o.painted,o.material,{});image=role==0?&shown->normal:&shown->mtoon_normal;}
 if(domain){const bool detail=o.painted->baked()&&!o.painted->baked()->maps.normal.rgba.empty();image=(role==0?(detail?domain->resolved_pbr:projection->pbr):(detail?domain->resolved_mtoon:projection->mtoon)).get();}
 auto encoded=sample(*image,uv);const double scale=role==0?1:b.material.parameters.normal_scale;D ts=unit(D{(encoded[0]*2-1)*scale,(encoded[1]*2-1)*scale,encoded[2]*2-1});return unit(add(add(mul(T,ts[0]),mul(B,ts[1])),mul(N,ts[2])));
}
D physical(const PaintedMesh &paint,std::array<double,2> uv){const int r=int(paint.resolution());const double x=uv[0]*r-.5,y=uv[1]*r-.5;require(std::isfinite(x)&&std::isfinite(y),"Finite independent physical coordinates");const int ix=int(std::floor(x)),iy=int(std::floor(y));D result{};for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const auto px=std::clamp(ix+dx,0,r-1),py=std::clamp(iy+dy,0,r-1);D texel{};for(const auto &layer:paint.layers())if(layer.visible&&layer.opacity){const auto emission=paint.emission_pixel(layer.id,px,py);const double coverage=double(emission.coverage)*paint.pixel(layer.id,px,py).data[13]/255.*layer.opacity;for(size_t axis=0;axis<3;++axis)texel[axis]=texel[axis]*(1-coverage)+emission.physical[axis]*coverage;}const double weight=(dx?x-ix:1-(x-ix))*(dy?y-iy:1-(y-iy));result=add(result,mul(texel,weight));}finite_sample(result);return result;}

std::array<double,2> paint_uv(const SculptObject&o,size_t triangle,std::array<float,3>bary){const auto&d=*o.painted->paint_sampling_domain();const auto&m=*d.basis();std::array<double,2>q{};for(size_t k=0;k<3;++k){const auto uv=m.uv.at(m.indices.at(d.source_corners()[triangle*3+k]));for(size_t axis=0;axis<2;++axis)q[axis]+=double(uv[axis])*bary[k];}return q;}
// Shared by the monolithic regression and the instrumented consumers of its
// captured inputs. The arithmetic and sample grid live in one place.
void check_dense(const Scene &s,uint64_t source,uint32_t checked_resolution){
 const auto&baked=*s.active().painted->baked();require(baked.emission_domain&&!baked.emission_field&&baked.emission_domain->field()->resolution()==64&&baked.emission_domain->target_resolution()==checked_resolution,"Checked HDR owns the genuine unchanged source64 field independently from selected target grid");
  // Independent immutable double texels, read once for THIS accepted phase.
  // They come from original layer/pixel data, never the production field,
  // projection resolver or transfer result. No cache survives a mutation.
  const auto&source_paint=*s.object(source).painted;const int source_r=source_paint.resolution();std::vector<D>radiance(size_t(source_r)*source_r);
  for(int y=0;y<source_r;++y)for(int x=0;x<source_r;++x)radiance[size_t(y)*source_r+x]=physical(source_paint,{(x+.5)/source_r,(y+.5)/source_r});
  auto expected_hdr=[&](std::array<double,2>q){const double x=q[0]*source_r-.5,y=q[1]*source_r-.5;const int ix=int(std::floor(x)),iy=int(std::floor(y));D value{};for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const auto&pixel=radiance[size_t(std::clamp(iy+dy,0,source_r-1))*source_r+std::clamp(ix+dx,0,source_r-1)];value=add(value,mul(pixel,(dx?x-ix:1-(x-ix))*(dy?y-iy:1-(y-iy))));}return value;};
  double hdr_maximum=0;uint64_t hdr_samples=0;std::array<double,2>hdr_worst{};
  owning_filtered::offcentre(s,source,[&](const SculptObject &object,size_t tri,std::array<float,3>bary,size_t role){
   if(role==0){const auto root=paint_uv(object,tri,bary);std::array<double,2>q{};const auto&mesh=s.active().painted->mesh();for(size_t k=0;k<3;++k)for(size_t axis=0;axis<2;++axis)q[axis]+=double(mesh.uv[mesh.indices[tri*3+k]][axis])*bary[k];const auto wanted=expected_hdr(root);const auto actual=baked.emission_domain->field()->sample(float(q[0]),float(q[1])).resolve({},1);for(size_t axis=0;axis<3;++axis){if(!std::isfinite(actual[axis]))throw std::runtime_error("Independent dense HDR consumer encountered nonfinite output");const double error=std::abs(wanted[axis]-actual[axis]);if(error>hdr_maximum){hdr_maximum=error;hdr_worst=q;}}++hdr_samples;}
   return decode(object,tri,bary,role);},[](const SculptObject &object,size_t tri,std::array<float,3>bary,size_t role,const TextureMaps *maps){return direction(object,tri,bary,role,maps);},require);
  std::cout<<std::setprecision(12)<<"CHECKED_HDR_EXTREMUM samples="<<hdr_samples<<" absolute="<<hdr_maximum<<" uv="<<hdr_worst[0]<<','<<hdr_worst[1]<<std::endl;
  require(hdr_samples>300&&hdr_maximum<2e-4,"Every independent current-grid offcentre HDR sample retains the unchanged2e-4 bound");
}

void check_history(const Scene &baked,const Scene &before,uint64_t source,
    const ObjectBakeSettings &settings,const std::filesystem::path &out,
    const std::string &name,const nlohmann::json &original){
 const auto accepted=baked.encode();
 const auto path=out/(name+".dsculpt");
 baked.save(path);
 require(Scene::load(path).encode()==accepted,"Combined SOURCE Save/reopen full owning graph exact");
 Document doc;
 doc.replace(before.snapshot());
 require(doc.wait_mesh(),"Joint SOURCE worker baseline publishes");
 doc.bake_from_object(source,settings);
 doc.save_async(out/(name+"-ordered.dsculpt")).get();
 require(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty()&&doc.copy_scene().encode()==accepted,
         "Joint actual worker and immediately ordered Save match native endpoint");
 require(doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==original&&
         doc.redo()&&doc.wait_mesh()&&doc.copy_scene().encode()==accepted,
         "Joint SOURCE Bake owns one exact UndoRedo boundary");
}

void check_source_context(const Scene &s,uint64_t source,const nlohmann::json &original){
 auto source_only=s;
 source_only.erase(s.active().id);
 source_only.select(source);
 auto context=source_only.encode();
 context["next_id"]=original.at("next_id");
 require(context==original,"All actual joint original/source/normal/paint/pose/UV history remains exact");
}

enum class CheckedTask { Full, Refusal, Seed, Capture };

void exercise(bool seam,bool live,bool repeat,const std::filesystem::path&out,
              uint32_t checked_resolution=0,CheckedTask task=CheckedTask::Full){
 auto s=prepared(seam);uv(s);if(repeat)uv(s);stroke(s);pose(s,live);const auto source=s.active().id;const auto original=s.encode();const auto raw=detail::raw_bake_source(s.active());const auto resolver=detail::source_bake_normals(s.active(),{});require(raw&&!raw->source_only&&raw->overlay&&raw->normal_projection&&resolver.matches_prepared_source(*raw),"Joint source resolves colour factors separately while authenticating actual normal/material/paint owners");double direct=0,omitted_normal=0,wrong_normal=0;const auto triangles=s.active().painted->mesh().indices.size()/3;for(size_t tri=0;tri<triangles;++tri){const std::array<float,3>bary{.25f,.35f,.4f};const auto wanted=decode(s.active(),tri,bary,0);direct=std::max(direct,error(wanted,resolver(tri,bary)[0]));const auto omitted=decode(s.active(),tri,bary,0,false,true),wrong=decode(s.active(),tri,bary,0,true,false);for(size_t axis=0;axis<3;++axis){omitted_normal=std::max(omitted_normal,std::abs(wanted[axis]-omitted[axis]));wrong_normal=std::max(wrong_normal,std::abs(wanted[axis]-wrong[axis]));}}require(direct<3e-6&&omitted_normal>.05&&wrong_normal>.05,"Independent joint live source normal equation rejects omitted and wrong-normal-domain controls");
 if(task==CheckedTask::Capture)s.save(out/"original-source.dsculpt");
 const auto evaluated=s.active().deformation->preview(s.active().painted->mesh());const auto&domain=*s.active().painted->paint_sampling_domain();const auto&basis=*domain.basis();Mesh destination_mesh;for(size_t c=0;c<evaluated.indices.size();++c){const auto v=evaluated.indices[c];destination_mesh.positions.push_back(evaluated.positions[v]);destination_mesh.normals.push_back(evaluated.normals[v]);destination_mesh.uv.push_back(basis.uv[basis.indices[domain.source_corners()[c]]]);destination_mesh.indices.push_back(uint32_t(c));}const auto destination=target(s,destination_mesh,false);s.select(destination);
 auto final_settings=settings();
 if(checked_resolution){
  owning_filtered::select_resolution(s,source,checked_resolution,out,require);
  final_settings.normal_filter_quality=1;
  final_settings.emission_transport=1;
 }
 if(task==CheckedTask::Seed){
  std::cout<<"CHECKED_SOURCE_SEED resolution="<<checked_resolution<<std::endl;
  return;
 }
 if(task==CheckedTask::Refusal){
  owning_filtered::refusal(s,source,final_settings,out,require);
  std::cout<<"CHECKED_SOURCE_REFUSAL resolution="<<checked_resolution<<std::endl;
  return;
 }
 const auto before=s.encode();
 const auto native=s.snapshot();
 require(bake_object(s,source,final_settings).projected>500,"Actual combined Source8 SOURCE second Bake projects");
 const auto target_maps=s.active().painted->preview(s.active().material);const auto material=detail::EmissionExportAccess::other_channels(*s.object(source).painted,s.object(source).material,{});const auto&low=s.active().painted->mesh();double normal_error=0,material_error=0,emission_error=0,peak=0,wrong_material=0,omitted_pbr=0;size_t interior=0;
 for(size_t y=0;y<64;++y)for(size_t x=0;x<64;++x){const std::array<double,2>q{(x+.5)/64.,(y+.5)/64.};for(size_t tri=0;tri<triangles;++tri){const auto a=low.uv[low.indices[tri*3]],b=low.uv[low.indices[tri*3+1]],c=low.uv[low.indices[tri*3+2]];const double den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1]);require(std::isfinite(den)&&std::abs(den)>1e-20,"Independent finite joint target UV determinant");const double u=((b[1]-c[1])*(q[0]-c[0])+(c[0]-b[0])*(q[1]-c[1]))/den,v=((c[1]-a[1])*(q[0]-c[0])+(a[0]-c[0])*(q[1]-c[1]))/den;if(std::min({u,v,1-u-v})<.08)continue;const std::array<float,3>bary{float(u),float(v),float(1-u-v)};const auto wanted=decode(s.object(source),tri,bary,0);for(size_t role=0;role<2;++role){const auto actual=direction(s.active(),tri,bary,role,target_maps.get());finite_sample(actual);for(size_t axis=0;axis<3;++axis)normal_error=std::max(normal_error,std::abs(wanted[axis]-actual[axis]));}const auto root=paint_uv(s.object(source),tri,bary);const auto wanted_color=sample(material->pbr_color,root,true),actual_color=sample(s.active().painted->baked()->maps.pbr_color,q,true);const auto legacy=sample(material->color,root,true),actual_legacy=sample(s.active().painted->baked()->maps.color,q,true);std::array<double,2> current_uv{};for(size_t k=0;k<3;++k)for(size_t axis=0;axis<2;++axis)current_uv[axis]+=double(s.object(source).painted->mesh().uv[s.object(source).painted->mesh().indices[tri*3+k]][axis])*bary[k];const auto wrong=sample(material->pbr_color,current_uv,true);for(size_t axis=0;axis<3;++axis){material_error=std::max({material_error,std::abs(wanted_color[axis]-actual_color[axis]),std::abs(legacy[axis]-actual_legacy[axis])});wrong_material=std::max(wrong_material,std::abs(wanted_color[axis]-wrong[axis]));omitted_pbr=std::max(omitted_pbr,std::abs(wanted_color[axis]-legacy[axis]));}const auto emission=physical(*s.object(source).painted,root);const auto&baked=*s.active().painted->baked();require(bool(baked.emission_field)!=bool(baked.emission_domain),"Joint second Bake owns exactly one actual independent FLOAT output");const auto value=(baked.emission_domain?baked.emission_domain->field():baked.emission_field)->sample(float(q[0]),float(q[1])).resolve({},1);for(size_t axis=0;axis<3;++axis){emission_error=std::max(emission_error,std::abs(emission[axis]-value[axis]));peak=std::max(peak,emission[axis]);}++interior;break;}}
 std::cout<<"JOINT_OBSERVATION seam="<<seam<<" live="<<live<<" repeat="<<repeat<<" direct="<<direct<<" normal="<<normal_error<<" material="<<material_error<<" wrongMaterial="<<wrong_material<<" omittedPbr="<<omitted_pbr<<" omittedNormal="<<omitted_normal<<" wrongNormal="<<wrong_normal<<" emission="<<emission_error<<" peak="<<peak<<" interior="<<interior<<std::endl;
 const auto source_world=s.world_transform(source),target_world=s.world_transform(destination);std::cout<<"JOINT_WORLD source="<<source_world.position.x<<','<<source_world.position.y<<','<<source_world.position.z<<" rotation="<<source_world.orientation.w<<','<<source_world.orientation.x<<','<<source_world.orientation.y<<','<<source_world.orientation.z<<" scale="<<source_world.scale<<" target="<<target_world.position.x<<','<<target_world.position.y<<','<<target_world.position.z<<" rotation="<<target_world.orientation.w<<','<<target_world.orientation.x<<','<<target_world.orientation.y<<','<<target_world.orientation.z<<" scale="<<target_world.scale<<std::endl;
 std::ofstream(out/"observed-source.json")<<original.dump(2);s.save(out/"observed-second-bake.dsculpt");
 require(interior>500&&normal_error<.035&&material_error<.025&&wrong_material>.05&&omitted_pbr>.05&&emission_error<2e-4&&peak>1,"Independent joint normal, current corner colour and HDR retain unchanged exact numerical bounds");
 if(task==CheckedTask::Capture){
  require(native.encode()==before,"Detached Bake preserves the complete pre-Bake scene");
  check_source_context(s,source,original);
  std::cout<<"CHECKED_SOURCE_CAPTURE resolution="<<checked_resolution<<std::endl;
  return;
 }
 if(checked_resolution){
   check_dense(s,source,checked_resolution);
 }
 const auto name=std::to_string(seam)+std::to_string(live)+std::to_string(repeat);
 check_history(s,native,source,final_settings,out,name,before);
 check_source_context(s,source,original);
 std::cout<<(checked_resolution?"CHECKED_JOINT_SOURCE ":"JOINT_SOURCE ")<<name<<" direct="<<direct<<" normal="<<normal_error<<" material="<<material_error<<" wrongMaterial="<<wrong_material<<" omittedPbr="<<omitted_pbr<<" omittedNormal="<<omitted_normal<<" wrongNormal="<<wrong_normal<<" emission="<<emission_error<<" peak="<<peak<<" interior="<<interior<<'\n';
}
// Both consumers reopen the exact files from one real Bake. The runner pins
// their bytes and grants read-only access to the capture directory.
void check_capture(uint32_t resolution,const std::filesystem::path &capture,
                   const std::optional<std::filesystem::path> &worker_output){
 const auto before=Scene::load(capture/"selected-target-resolution.dsculpt");
 const auto baked=Scene::load(capture/"observed-second-bake.dsculpt");
 const auto original_scene=Scene::load(capture/"original-source.dsculpt");
 const auto source=original_scene.active().id;
 const auto original=original_scene.encode();
 const auto &before_target=before.active();
 const auto &baked_target=baked.active();
 require(before_target.id==baked_target.id && before_target.id!=source &&
         before_target.painted && baked_target.painted &&
         before_target.painted->resolution()==resolution &&
         baked_target.painted->resolution()==resolution &&
         !before_target.painted->baked() && baked_target.painted->baked(),
         "Capture retains the same selected unbaked and baked target");
 require(before.object(source).painted && before.object(source).painted->resolution()==64 &&
         before.object(source).painted->has_physical_emission(),
         "Capture retains the genuine independent 64-pixel physical source");
 check_source_context(before,source,original);
 check_source_context(baked,source,original);
 const auto settings=baked_target.bake_recipe;
 require(settings && settings->source.object==source && settings->settings.normal_filter_quality==1 &&
         settings->settings.emission_transport==1,
         "Capture retains the checked native bake recipe and exact source ID");
 if(worker_output){
  std::filesystem::create_directories(*worker_output);
  check_history(baked,before,source,settings->settings,*worker_output,"000",before.encode());
 }else{
  check_dense(baked,source,resolution);
 }
}

void negatives(){
 auto s=prepared(false);uv(s);const auto baseline=s.encode();
 auto binding=std::make_shared<SourceMaterialBindings>(*s.active().material_bindings);
 auto color=std::make_shared<SourceColorPreparation>(*binding->color_preparation);
 binding->color_preparation=color;s.active().material_bindings=binding;
 const auto original=color->original_material;color->original_material=binding;bool cyclic=false;
 try{detail::source_bake_normals(s.active(),{});}catch(const std::invalid_argument&e){cyclic=std::string_view(e.what()).find("replay authority is cyclic")!=std::string_view::npos;}
 color->original_material=original;s.active().material_bindings=std::make_shared<const SourceMaterialBindings>(*binding);
 require(cyclic&&s.encode()==baseline,"Whole independent colour/normal replay cycle refuses before deep validation and restores exact owning graph");
 detail::SourceBakeNormalOptions limits;limits.max_working_bytes=1;
 require(refuses([&]{detail::source_bake_normals(s.active(),{},limits);})&&s.encode()==baseline,"Joint prospective capture quota refuses atomically");
 auto larger=prepared(false);uv(larger,512*1024*1024);const auto exact=larger.encode();
 // SourceUV's simultaneous colour and normal stages each own one half of
 // the genuine512MiB request. Test the ACTUAL256MiB receipt boundary, rather
 // than inventing a512MiB normal receipt from that aggregate request.
 const auto authority=larger.active().material_bindings->normal_preparation->sampling_domain->receipt.max_working_bytes;
 require(authority==256*1024*1024,"Actual joint replay receipt records its disjoint256MiB share of genuine512MiB transfer");
 limits={};limits.max_working_bytes=authority;require(bool(detail::source_bake_normals(larger.active(),{},limits))&&larger.encode()==exact,"Exact actual owning replay allowance accepts without mutation");
 size_t polls=0;limits={};limits.max_working_bytes=authority-1;limits.cancelled=[&]{++polls;return false;};bool prospective=false;
 try{detail::source_bake_normals(larger.active(),{},limits);}catch(const std::length_error&e){prospective=std::string_view(e.what()).find("prospective working budget")!=std::string_view::npos;}
 std::cout<<"JOINT_QUOTA authority="<<authority<<" caller="<<limits.max_working_bytes<<" polls="<<polls<<std::endl;require(prospective&&polls<16&&larger.encode()==exact,"One byte below the actual joint replay allowance refuses before deep replay and preserves every source byte");
}
}
#ifndef DOME_JOINT_SOURCE_EMBEDDED
int main(int argc,char**argv)try{
  if(argc>=2){
    const std::string_view command=argv[1];
    const bool worker=command=="--checked-worker";
    if(command=="--checked-dense"||worker){
      if(argc!=(worker?5:4))return 2;
      const auto resolution=owning_filtered::resolution(argv[2]);
      const auto output=worker?std::optional<std::filesystem::path>(argv[4]):std::nullopt;
      joint_source::check_capture(resolution,argv[3],output);
      std::cout<<"PASS "<<joint_source::checks<<" checked joint-writable SOURCE "
               <<(worker?"history":"dense oracle")<<" controls"<<std::endl;
      return 0;
    }
  }
  if(argc==5){
    const std::string_view command=argv[1];
    auto task=joint_source::CheckedTask::Full;
    if(command=="--checked-seed")task=joint_source::CheckedTask::Seed;
    else if(command=="--checked-capture")task=joint_source::CheckedTask::Capture;
    else if(command=="--checked-refusal")task=joint_source::CheckedTask::Refusal;
    else if(command!="--checked-case")return 2;
    const auto axes=owning_filtered::bits(argv[2],3);
    const auto selected=owning_filtered::resolution(argv[3]);
    if((task==joint_source::CheckedTask::Seed||task==joint_source::CheckedTask::Capture)&&selected==64)
      throw std::invalid_argument("Checked input requires an explicit target resize");
    const std::filesystem::path out=argv[4];
    std::filesystem::create_directories(out);
    joint_source::exercise(axes[0],axes[1],axes[2],out,selected,task);
    std::cout<<"PASS "<<joint_source::checks<<" checked joint-writable SOURCE controls"<<std::endl;
    return 0;
  }
  if(argc==3&&std::string_view(argv[1])=="--legacy-controls"){
    argv[1]=argv[2];
    argc=2;
  }
  if(argc!=2)return 2;
  const std::filesystem::path out=argv[1];
  std::filesystem::create_directories(out);
  for(bool seam:{false,true})for(bool live:{false,true})for(bool repeat:{false,true})
    joint_source::exercise(seam,live,repeat,out);
  joint_source::negatives();
  std::cout<<"PASS "<<joint_source::checks<<" joint source checks\n";
  return 0;
}catch(const std::exception&e){
  std::cerr<<"FAIL "<<e.what()<<'\n';
  return 1;
}

#endif
