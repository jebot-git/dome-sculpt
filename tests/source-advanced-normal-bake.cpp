// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-prepared-normal-oracle.hpp"
#include "affine-surface.hpp"
#include "dome/source-uv-bake.hpp"
#include "emission-bake-source.hpp"
#include "paint-overlay-packet.hpp"

using namespace source_artist_fixture;
using namespace source_prepared_oracle;
namespace advanced_source {
unsigned checks=0;
void require(bool yes,const char *why){if(!yes)throw std::runtime_error(why);++checks;}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception &e){std::cout<<"EXPECTED_REFUSAL "<<e.what()<<'\n';return true;}}
Scene prepared(bool legacy){
 Scene s;auto &low=s.active();Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};
 low.mesh_active=true;
 // Begin with an ordinary genuine first-Bake target. Independent current
 // material maps will be produced by real SourceUV adoption after authoring.
 auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=std::make_shared<const Image>(Image{2,2,{255,60,90,255,50,220,80,255,30,70,240,255,210,180,30,255}});maps->color.texcoord=0;low.painted=capture_source_sampling(m,maps,64);low.material_bindings=blank_typed(legacy);
 DeformationState deformation(m);deformation.morphs=MorphSet(m);auto key=m.positions;key[0].z=.045f;key[2].z=-.025f;deformation.morphs->set_target(m,"Retained native morph",key);deformation.skeleton=Skeleton{{{"root",-1,{},1},{"child",0,{},1}}};deformation.skin=SkinWeights{};deformation.skin->vertices={{{0,.7f},{1,.3f}},{{0,.2f},{1,.8f}},{{0,.4f},{1,.6f}},{{0,.9f},{1,.1f}}};s.set_deformation(1,deformation);
 auto artist=fixture(legacy,false);const auto id=s.add("Immutable artist high",Volume{});auto high=artist.scene.object(artist.source);high.id=id;high.transform.position.z=.15f;high.visible=false;high.locked=true;s.object(id)=std::move(high);s.select(1);s.validate();
 ObjectBakeSettings settings;settings.prepare_normals=true;settings.front_mm=settings.back_mm=500;settings.ao_samples=0;settings.padding=0;settings.normal_error=.03f;const auto original_high=s.object(id).painted;
 require(bake_object(s,id,settings).normal_prepared&&s.object(id).painted==original_high,"Actual artist first Bake creates source normals while retaining original high source");
 auto p=s.active().painted->snapshot();auto baked=*p->baked();p->clear_bake();for(size_t at=0;at<baked.coverage.size();++at)baked.coverage[at]=std::array<uint8_t,4>{0,64,128,255}[at%4];p->install_bake(std::move(baked));
 p->add_layer("Authored source color");PaintSettings color;color.channel=PaintChannel::Color;color.color={.85f,.12f,.45f,1};color.front_only=false;Brush brush;brush.from=brush.to={-.22f,.17f,0};brush.normal={0,0,1};brush.radius=.18f;p->begin_stroke();p->stamp(brush,color);p->end_stroke();s.active().painted=p;s.validate();return s;
}
Scene uv_source(const Scene &input,SourceNormalUvMethod method,bool seam,size_t replay_cap=256*1024*1024){
 auto out=input.snapshot();const auto &o=out.active();SourceUvBakeInput in;in.paint=o.painted;in.appearance=o.appearance;in.current_material=o.material_bindings;in.current_deformation=o.deformation;in.target=o.painted->mesh();in.source_vertices.resize(in.target.positions.size());std::iota(in.source_vertices.begin(),in.source_vertices.end(),0);
 if(seam){Mesh split;for(size_t c=0;c<in.target.indices.size();++c){const auto v=in.target.indices[c];split.positions.push_back(in.target.positions[v]);split.normals.push_back(in.target.normals[v]);split.uv.push_back(in.target.uv[v]);split.indices.push_back(uint32_t(c));}in.source_vertices=in.target.indices;in.target=std::move(split);}
 for(auto&q:in.target.uv){const auto x=q[0];q={.2f+.6f*q[1],.8f-.6f*x};}
 SourceUvBakeOptions options;options.normal_sampling=method;options.max_working_bytes=replay_cap;options.max_normal_sample_error=.03;auto adoption=adopt_source_uv_bake(bake_source_uv(in,options));auto &a=out.active();a.painted=adoption.paint;a.appearance=adoption.appearance;a.deformation=adoption.deformation;a.material_bindings=adoption.material_bindings;out.validate();return out;
}
void pose(Scene &s,bool live){if(!live)return;auto d=*s.active().deformation;d.morph_weights={.35f};d.pose.resize(2);d.pose[0].scale={1.2f,.8f,.7f};d.pose[1].scale={.8f,1.1f,1.3f};s.set_deformation(s.active().id,std::move(d));s.validate();}
// Independent declared-frame decoder. No product frame/basis/resolver/sampler.
D decode(const SculptObject&o,size_t tri,std::array<float,3>bary,size_t role,
         bool wrong_domain=false,bool omit=false,const AffineTransform &world={}){
 const auto &b=*o.material_bindings;const auto &set=*b.normal_frames;const auto &f=role==0?*set.pbr:*set.mtoon;const auto &mesh=o.painted->mesh();D N{},T{},B{};std::array<double,2>uv{};
 const auto projection=o.painted->normal_projection();const auto domain=projection->sampling_domain;const auto raster=b.normal_preparation->uv_reprojection;
 const auto &m=world.matrix();
 const double determinant=m[0]*(m[5]*m[10]-m[9]*m[6])-m[4]*(m[1]*m[10]-m[9]*m[2])+m[8]*(m[1]*m[6]-m[5]*m[2]);
 const double orientation=determinant<0?-1:1;
 for(size_t k=0;k<3;++k){const size_t corner=tri*3+k;const auto v=mesh.indices[corner];D n=vc(f.normals[corner]),t{f.tangents[corner][0],f.tangents[corner][1],f.tangents[corner][2]};
  if(o.deformation){for(size_t slot=0;slot<set.current_targets.size();++slot){const double w=o.deformation->morph_weights.empty()?0:o.deformation->morph_weights[slot];for(const auto&target:f.targets)if(target.id==set.current_targets[slot]){if(!target.normals.empty())n=add(n,mul(vc(target.normals[corner]),w));if(!target.tangents.empty())t=add(t,mul(vc(target.tangents[corner]),w));}}
   if(!o.deformation->pose.empty()){D nn{},tt{};for(auto influence:o.deformation->skin->vertices[v]){const auto a=o.deformation->pose[0].scale,c=o.deformation->pose[1].scale;D scale{a.x,a.y,a.z};if(influence.joint==1)scale={a.x*c.x,a.y*c.y,a.z*c.z};for(size_t axis=0;axis<3;++axis){nn[axis]+=n[axis]/scale[axis]*influence.weight;tt[axis]+=t[axis]*scale[axis]*influence.weight;}}n=nn;t=tt;}}
  n=unit(vc(source_artist_oracle::normal(m,{float(n[0]),float(n[1]),float(n[2])})));
  t=unit(vc(source_artist_oracle::direction(m,{float(t[0]),float(t[1]),float(t[2])})));
  N=add(N,mul(n,bary[k]));T=add(T,mul(t,bary[k]));
  B=add(B,mul(unit(crossd(n,t)),bary[k]*f.tangents[corner][3]*orientation));
  auto q=mesh.uv[v];if(domain&&!wrong_domain){const auto &coords=role&&!domain->mtoon_coordinates.empty()?domain->mtoon_coordinates:domain->coordinates;q=coords[domain->indices[corner]];}else if(!domain&&wrong_domain&&raster){const auto &before=raster->before_paint->mesh();q=before.uv[before.indices[corner]];}for(size_t axis=0;axis<2;++axis)uv[axis]+=q[axis]*bary[k];
 }
 if(role==0){N=unit(N);T=unit(T);B=unit(B);}else if(b.material.version()==MToonVersion::V1){N=unit(N);T=unit(T);B=mul(unit(crossd(N,T)),f.tangents[tri*3][3]*orientation);}
 if(omit)return unit(N);
 // These are the declared live renderer planes, distinct from Bake's normal
 // capture. v2's image remains in its original domain; no atlas rebake here.
 const auto shown=o.painted->preview(o.material);const Image *image=role==0?&shown->normal:&shown->mtoon_normal;
 if(domain){const bool detail=o.painted->baked()&&!o.painted->baked()->maps.normal.rgba.empty();image=(role==0?(detail?domain->resolved_pbr:projection->pbr):(detail?domain->resolved_mtoon:projection->mtoon)).get();}
 auto encoded=sample(*image,uv);const double scale=role==0?1:b.material.parameters.normal_scale;D ts=unit(D{(encoded[0]*2-1)*scale,(encoded[1]*2-1)*scale,encoded[2]*2-1});return unit(add(add(mul(T,ts[0]),mul(B,ts[1])),mul(N,ts[2])));
}

void affine_source(bool legacy,SourceNormalUvMethod method,const std::filesystem::path &folder){
 auto scene=uv_source(prepared(legacy),method,true);
 auto &source=scene.active();
 const auto id=source.id;
 const auto paint=source.painted;
 const auto binding=source.material_bindings;
 const std::array<double,16> matrix{-1.4,.2,0,0, .22,.81,0,0, 0,0,1.3,0, .8,-.4,.2,1};
 const AffineTransform placement(matrix);
 source.affine_edit=std::make_shared<const NativeAffineEdit>(placement);
 source.visible=false;
 const auto frame=detail::placement_frame(scene,id);
 detail::SourceBakeNormalOptions options;
 options.placement=&frame;
 const auto resolver=detail::source_bake_normals(source,{},options);
 auto mesh=paint->mesh();
 double maximum=0;
 for(uint32_t tri=0;tri<mesh.indices.size()/3;++tri){
   constexpr std::array<float,3> bary{.2f,.3f,.5f};
   const auto actual=resolver(tri,bary);
   for(size_t role=0;role<2;++role)
     maximum=std::max(maximum,error(decode(source,tri,bary,role,false,false,frame.world()),actual[role]));
 }
 require(maximum<3e-6,"Both UV-edited normal roles survive reflection and shear in the independent decoder");
 for(auto &p:mesh.positions){
   const auto q=frame.world().point({p.x,p.y,p.z});
   p={float(q[0]),float(q[1]),float(q[2])};
 }
 const auto destination=target(scene,mesh,legacy);
 scene.select(destination);
 const std::string name=std::string(legacy?"mt0-":"mt1-")+
     (method==SourceNormalUvMethod::RetainSource?"retained":"raster");
 scene.save(folder/(name+"-affine-before.dsculpt"));
 ObjectBakeSettings settings;
 settings.prepare_normals=true;
 settings.corresponding_topology=true;
 settings.normal_error=.03f;
 settings.front_mm=settings.back_mm=2;
 settings.ao_samples=0;
 settings.padding=0;
 const auto report=bake_object(scene,id,settings);
 const auto result=scene.active().painted->baked();
 require(report.projected>500&&result&&result->maps.normal.rgba!=result->maps.mtoon_normal.rgba&&
         scene.object(id).painted==paint&&scene.object(id).material_bindings==binding,
         "Matching Bake transfers both placed UV-edited roles and keeps the source exact");
}
void negatives(SourceNormalUvMethod method){
 auto scene=uv_source(prepared(false),method,true);const auto before=scene.encode();const auto &source=scene.active();
 detail::SourceBakeNormalOptions limit;limit.max_working_bytes=1;
 require(refuses([&]{detail::source_bake_normals(source,{},limit);})&&scene.encode()==before,"Advanced recipe prospective memory refusal is atomic");
 size_t polls=0;limit={};limit.cancelled=[&]{++polls;return false;};auto resolver=detail::source_bake_normals(source,{},limit);const auto total=polls;
 for(const size_t boundary:{size_t(1),size_t(2),total/3,total/2,total-1,total}){size_t at=0;limit.cancelled=[&]{return ++at>=boundary;};require(refuses([&]{detail::source_bake_normals(source,{},limit);})&&scene.encode()==before,"Advanced recipe cancellation at early/replay/capture/final boundaries leaves exact source context");}
 require(total>3,"Advanced normal adapter exposes cancellation across genuine recipe replay and capture");
 auto missing=source;auto binding=std::make_shared<SourceMaterialBindings>(*missing.material_bindings);auto preparation=std::make_shared<SourceNormalPreparation>(*binding->normal_preparation);preparation->uv_reprojection.reset();preparation->sampling_domain.reset();binding->normal_preparation=preparation;missing.material_bindings=binding;
 require(refuses([&]{detail::source_bake_normals(missing,{});})&&scene.encode()==before,"Reparameterized source cannot invent missing original normal-domain authority");
 auto forged=source;binding=std::make_shared<SourceMaterialBindings>(*forged.material_bindings);preparation=std::make_shared<SourceNormalPreparation>(*binding->normal_preparation);
 if(method==SourceNormalUvMethod::Rasterize){auto recipe=std::make_shared<SourceNormalUvReprojection>(*preparation->uv_reprojection);auto layout=std::make_shared<SourceNormalUvLayout>(*recipe->layout);layout->uv[0][0]+=.125f;recipe->layout=layout;preparation->uv_reprojection=recipe;}
 else{auto recipe=std::make_shared<SourceNormalSampling>(*preparation->sampling_domain);auto projection=std::make_shared<PaintNormalProjection>(*recipe->projection);auto domain=std::make_shared<NormalSamplingDomain>(*projection->sampling_domain);domain->coordinates[0][0]+=.125f;projection->sampling_domain=domain;recipe->projection=projection;preparation->sampling_domain=recipe;}
 binding->normal_preparation=preparation;forged.material_bindings=binding;
 require(refuses([&]{detail::source_bake_normals(forged,{});})&&scene.encode()==before,"Changed ordered-corner/domain metadata is rejected by actual recipe replay");
 auto raw=detail::raw_bake_source(source);require(raw&&resolver.matches_prepared_source(*raw),"Actual independently captured material packet pairs with the factory-minted normal resolver");
 auto stale=std::make_shared<TextureMaps>(*raw);auto edited=source.painted->snapshot();auto payload=*edited->baked();edited->clear_bake();if(method==SourceNormalUvMethod::RetainSource){require(refuses([&]{edited->install_bake(payload);}),"Retained ClearBake cannot reintroduce a prior normal event without its actual field transition");}else edited->install_bake(std::move(payload));if(stale->source_only)stale->overlay=detail::PaintOverlayFactory::capture(*edited);else stale->baked_base=edited->baked();
 BakeSource high;high.mesh=source.painted->mesh();high.materials[0].maps=stale;BakeOptions options;options.resolution=64;options.ao_samples=0;options.padding=0;
 require(!resolver.matches_prepared_source(*stale)&&refuses([&]{detail::bake_mesh_source_normals(high.mesh,high,options,resolver);})&&scene.encode()==before,"A later Rasterize event or cleared retained event cannot borrow the earlier resolver material owner");
 auto other=uv_source(prepared(false),method,true);auto foreign=detail::source_bake_normals(other.active(),{});require(!foreign.matches_prepared_source(*raw)&&refuses([&]{detail::bake_mesh_source_normals(high.mesh,high,options,foreign);}),"Independent genuine source with matching bytes cannot authorize another source's normal/material owners");
}
void ancestor_guards(const std::filesystem::path &folder){
 auto chain=uv_source(prepared(false),SourceNormalUvMethod::RetainSource,true,384*1024*1024);
 chain=uv_source(chain,SourceNormalUvMethod::RetainSource,true);chain=uv_source(chain,SourceNormalUvMethod::RetainSource,true);chain.validate();
 const auto before=chain.encode();const auto &source=chain.active();detail::SourceBakeNormalOptions options;size_t polls=0;options.cancelled=[&]{++polls;return false;};bool quota=false;
 try{detail::source_bake_normals(source,{},options);}catch(const std::length_error&){quota=true;}
 require(quota&&polls<16&&chain.encode()==before,"A larger genuine historical UV receipt is refused prospectively below caller quota before deep replay, without any source mutation");
 options.max_working_bytes=512*1024*1024;options.cancelled={};const auto resolver=detail::source_bake_normals(source,{},options);double e=0;for(size_t role=0;role<2;++role)e=std::max(e,error(decode(source,0,{.2f,.3f,.5f},role),resolver(0,{.2f,.3f,.5f})[role]));require(e<3e-6&&chain.encode()==before,"Three genuine retained edits remain independently correct when the full historical replay cap fits the caller budget");
 auto cycle=source;auto binding=std::make_shared<SourceMaterialBindings>(*source.material_bindings);auto preparation=std::make_shared<SourceNormalPreparation>(*binding->normal_preparation);auto recipe=std::make_shared<SourceNormalSampling>(*preparation->sampling_domain);preparation->sampling_domain=recipe;binding->normal_preparation=preparation;recipe->before_material=binding;cycle.material_bindings=binding;bool cyclic=false;
 try{detail::source_bake_normals(cycle,{},options);}catch(const std::invalid_argument&error){cyclic=std::string(error.what())=="Source normal replay authority is cyclic";}recipe->before_material.reset();require(cyclic&&chain.encode()==before,"Actual cyclic before-material authority refuses before recursive validation and releases forged test owners");
 const auto id=source.id;auto mesh=source.deformation?source.deformation->preview(source.painted->mesh()):source.painted->mesh();chain.select(target(chain,mesh,false));const auto baseline=chain.encode();ObjectBakeSettings bake;bake.prepare_normals=true;bake.normal_error=.03f;bake.ao_samples=0;bake.padding=0;bake.front_mm=bake.back_mm=500;
 require(refuses([&]{bake_object(chain,id,bake);})&&chain.encode()==baseline,"Native Bake's declared 256MiB source cap refuses larger historical replay atomically");
 const auto path=folder/"ancestor-budget-prior.dsculpt";const std::string previous="Keep exact earlier native save bytes\n";{std::ofstream f(path,std::ios::binary);f<<previous;}
 Document document;document.replace(chain.snapshot());require(document.wait_mesh(),"Historical quota native Scene reaches initial worker state");bool rejected=false;try{document.bake_from_object(id,bake);document.save_async(path).get();}catch(const std::exception&){rejected=true;}
 std::ifstream file(path,std::ios::binary);const std::string actual{std::istreambuf_iterator<char>(file),{}};require(rejected&&document.wait_asset()&&document.wait_mesh()&&actual==previous&&document.copy_scene().encode()==baseline&&!document.frame().state->can_undo&&!document.frame().state->can_redo,"Historical replay quota failure preserves ordered Save bytes, full native state and history");
}
void run(bool legacy,bool live,bool seam,SourceNormalUvMethod method,const std::filesystem::path&folder,bool repeated=false){
 auto scene=uv_source(prepared(legacy),method,seam);if(repeated){scene=uv_source(scene,method,seam);scene=uv_source(scene,method,seam);}pose(scene,live);const auto source=scene.active().id;const auto snapshot=scene.encode();const auto &o=scene.object(source);const auto before_paint=o.painted;const auto before_binding=o.material_bindings;auto resolve=detail::source_bake_normals(o,{});require(bool(resolve),"Genuine reprojected/retained-domain source receives an owning normal adapter");const auto raw=detail::raw_bake_source(o);require(raw&&raw->source_only&&raw->overlay&&raw->source_maps&&resolve.matches_prepared_source(*raw),"Actual advanced source carries separate raw material, paint and normal authority owners");if(method==SourceNormalUvMethod::RetainSource)require(raw->overlay->baked()&&raw->overlay->baked()->normal_authority==o.painted->normal_projection()->sampling_domain->detail,"Actual raw retained source carries its genuine first-Bake normal detail event");
 double maximum=0,wrong=0,omitted=0,swapped=0;for(size_t tri=0;tri<o.painted->mesh().indices.size()/3;++tri)for(auto bary:std::array<std::array<float,3>,3>{{{.2f,.3f,.5f},{.6f,.25f,.15f},{.15f,.7f,.15f}}}){auto actual=resolve(uint32_t(tri),bary);for(size_t role=0;role<2;++role){const auto wanted=decode(o,tri,bary,role);maximum=std::max(maximum,error(wanted,actual[role]));for(size_t c=0;c<3;++c){wrong=std::max(wrong,std::abs(wanted[c]-decode(o,tri,bary,role,true)[c]));omitted=std::max(omitted,std::abs(wanted[c]-decode(o,tri,bary,role,false,true)[c]));swapped=std::max(swapped,std::abs(wanted[c]-decode(o,tri,bary,1-role)[c]));}}}
 require(maximum<3e-6&&wrong>.05&&omitted>.05&&swapped>.05,"Independent finite dual-role/weighted-skin/morph/domain decoder matches actual capture and rejects wrong-domain/omitted/swapped controls");
 auto mesh=o.deformation?o.deformation->preview(o.painted->mesh()):o.painted->mesh();if(repeated)for(size_t v=0;v<mesh.uv.size();++v)mesh.uv[v]={o.painted->mesh().positions[v].x+.5f,o.painted->mesh().positions[v].y+.5f};const auto destination=target(scene,mesh,legacy);scene.select(destination);const auto before=scene.encode();ObjectBakeSettings options;options.prepare_normals=true;options.normal_error=.03f;options.front_mm=options.back_mm=500;options.ao_samples=0;options.padding=0;const auto native_before=scene.snapshot();require(bake_object(scene,source,options).projected>0,"Actual native Bake accepts an advanced source without discarding source/normal metadata");require(scene.object(source).painted==before_paint&&scene.object(source).material_bindings==before_binding,"Actual advanced-source Bake retains exact immutable source owners");
 double baked_error=0,color_error=0,interior_omitted=0,interior_swapped=0,interior_wrong=0;size_t interior=0;const auto material=scene.object(source).painted->preview(scene.object(source).material);const auto&low=scene.object(destination).painted->mesh();for(size_t y=0;y<64;++y)for(size_t x=0;x<64;++x){const std::array<double,2>q{(x+.5)/64.,(y+.5)/64.};for(size_t tri=0;tri<low.indices.size()/3;++tri){auto a=low.uv[low.indices[tri*3]],b=low.uv[low.indices[tri*3+1]],c=low.uv[low.indices[tri*3+2]];double den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1]);require(std::isfinite(den)&&std::abs(den)>1e-20,"Independent advanced target UV determinant is finite");double u=((b[1]-c[1])*(q[0]-c[0])+(c[0]-b[0])*(q[1]-c[1]))/den,v=((c[1]-a[1])*(q[0]-c[0])+(a[0]-c[0])*(q[1]-c[1]))/den;require(std::isfinite(u)&&std::isfinite(v),"Independent advanced target barycentrics are finite");if(std::min({u,v,1-u-v})<.08)continue;std::array<float,3>bary{float(u),float(v),float(1-u-v)};for(size_t role=0;role<2;++role){auto wanted=decode(scene.object(source),tri,bary,role),actual=direction(scene.object(destination),tri,bary,role);finite_sample(actual);const auto omitted=decode(scene.object(source),tri,bary,role,false,true),swapped=decode(scene.object(source),tri,bary,1-role),wrong=decode(scene.object(source),tri,bary,role,true);for(size_t axis=0;axis<3;++axis){baked_error=std::max(baked_error,std::abs(actual[axis]-wanted[axis]));interior_omitted=std::max(interior_omitted,std::abs(omitted[axis]-wanted[axis]));interior_swapped=std::max(interior_swapped,std::abs(swapped[axis]-wanted[axis]));interior_wrong=std::max(interior_wrong,std::abs(wrong[axis]-wanted[axis]));}}std::array<double,2>source_uv{};for(size_t k=0;k<3;++k)for(size_t axis=0;axis<2;++axis)source_uv[axis]+=scene.object(source).painted->mesh().uv[low.indices[tri*3+k]][axis]*bary[k];auto color=sample(material->color,source_uv,true),actual_color=sample(scene.object(destination).painted->baked()->maps.color,q,true);for(size_t axis=0;axis<3;++axis)color_error=std::max(color_error,std::abs(color[axis]-actual_color[axis]));++interior;break;}}
 require(std::isfinite(color_error)&&color_error<.025,"Independent composed material consumer preserves genuine alternate-UV color and authored layers beside retained normal domains");
 require(interior_omitted>.05&&interior_swapped>.05&&interior_wrong>.05,"Omitted, swapped-role and wrong-domain controls fail beyond .035 on these same actual second-Bake interior texels");
 require(interior>300&&std::isfinite(baked_error)&&baked_error<.035,"Independent actual second-Bake texels preserve both advanced source roles within unchanged .035 tolerance");
 const auto encoded=scene.encode();const std::string name=std::string(legacy?"mt0":"mt1")+(live?"-posed":"-rest")+(seam?"-seam":"-shared")+(method==SourceNormalUvMethod::RetainSource?"-retained":"-raster")+(repeated?"-three-edits":"");const auto path=folder/(name+".dsculpt");scene.save(path);require(Scene::load(path).encode()==encoded,"Advanced source and second-Bake native persistence preserves complete owning recipes");Document document;document.replace(native_before);require(document.wait_mesh(),"Advanced native worker reaches initial geometry");document.bake_from_object(source,options);auto saved=document.save_async(folder/(name+"-ordered.dsculpt"));saved.get();require(document.wait_asset()&&document.wait_mesh()&&document.frame().state->error.empty()&&document.copy_scene().encode()==encoded,"Advanced worker plus immediate ordered Save exactly matches native expected result");require(document.undo()&&document.wait_mesh()&&document.copy_scene().encode()==before&&document.redo()&&document.wait_mesh()&&document.copy_scene().encode()==encoded,"Advanced source worker has one exact whole-Scene Undo/Redo boundary");
 auto retained_scene=scene;retained_scene.erase(destination);retained_scene.select(source);auto context=retained_scene.encode();require(context.at("next_id").get<uint64_t>()==snapshot.at("next_id").get<uint64_t>()+1,"Adding the one actual destination consumes exactly one Scene object identity");context["next_id"]=snapshot.at("next_id");if(context!=snapshot)std::cerr<<"SOURCE_CONTEXT_DIFF "<<nlohmann::json::diff(context,snapshot).dump()<<'\n';require(context==snapshot,"Second Bake leaves every original artist/source context field exact apart from the explicitly checked object allocator");
 std::cout<<"ADVANCED_SOURCE legacy="<<legacy<<" live="<<live<<" seam="<<seam<<" retained="<<(method==SourceNormalUvMethod::RetainSource)<<" direct_error="<<maximum<<" wrong_domain="<<wrong<<" omitted="<<omitted<<" swapped="<<swapped<<" second_bake_error="<<baked_error<<" interior_omitted="<<interior_omitted<<" interior_swapped="<<interior_swapped<<" interior_wrong="<<interior_wrong<<" color_error="<<color_error<<" interior="<<interior<<'\n';
}
}
int main(int argc,char **argv) try {
 if(argc==3&&std::string_view(argv[1])=="--sources"){
   auto scene=Scene::load(argv[2]);
   scene.erase(scene.active().id);
   std::cout<<scene.encode().dump()<<'\n';
   return 0;
 }
 if(argc!=2)return 2;
 std::filesystem::create_directories(argv[1]);
 for(auto method:{SourceNormalUvMethod::Rasterize,SourceNormalUvMethod::RetainSource}){
   advanced_source::negatives(method);
   for(bool legacy:{false,true}){
     advanced_source::affine_source(legacy,method,argv[1]);
     for(bool live:{false,true})for(bool seam:{false,true})
       advanced_source::run(legacy,live,seam,method,argv[1]);
   }
 }
 advanced_source::run(false,true,true,SourceNormalUvMethod::RetainSource,argv[1],true);
 advanced_source::ancestor_guards(argv[1]);
 std::cout<<"PASS "<<advanced_source::checks<<" advanced source checks\n";
 return 0;
} catch(const std::exception &error) {
 std::cerr<<"FAIL "<<error.what()<<'\n';
 return 1;
}
