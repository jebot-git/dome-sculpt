// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-prepared-normal-oracle.hpp"
#include "bake-paint-coordinates.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/native-authoring.hpp"
#include "dome/source-uv-bake.hpp"
#include "emission-bake-source.hpp"
#include "canonical-paint-sampling.hpp"
#include "affine-surface.hpp"
#include "paint-overlay-packet.hpp"
#include "native-paint-model.hpp"
#include "native-paint-frame-source.hpp"
#include "baked-normal-field.hpp"
#include "emission-paint-access.hpp"
#include "dome/shapekey-authoring.hpp"
#include "dome/tangents.hpp"
#include <fstream>
using namespace source_artist_fixture;
using namespace source_prepared_oracle;
namespace writable_source {
size_t checks=0;
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);++checks;}
ObjectBakeSettings settings(){ObjectBakeSettings s;s.prepare_normals=true;s.normal_error=.03f;s.front_mm=s.back_mm=500;s.ao_samples=0;s.padding=0;return s;}
Scene source(bool canonical){
 Scene s;auto &o=s.active();Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};o.mesh_active=true;o.painted=std::make_shared<PaintedMesh>(m,64);
 DeformationState d(m);d.skeleton=Skeleton{{{"root",-1,{},1},{"child",0,{},1}}};d.skin=SkinWeights{};d.skin->vertices={{{0,.7f},{1,.3f}},{{1,1}},{{0,.4f},{1,.6f}},{{0,1}}};s.set_deformation(o.id,d);
 if(canonical){
  auto high=m;for(auto &p:high.positions)p.z=.01f;high.normals.assign(4,normalized(Vec3{.3f,-.2f,1}));const auto original=s.add("Actual historical high",Volume{});auto &h=s.object(original);h.mesh_active=true;h.visible=false;h.locked=true;h.painted=std::make_shared<PaintedMesh>(high,64);s.select(1);auto first=settings();first.prepare_normals=false;require(bake_object(s,original,first).projected>500,"Actual legacy first Bake");
  const auto &current=s.active();auto converted=convert_canonical_normals({current.painted,{},current.material_bindings,current.deformation,current.material,{}});s.active().painted=converted.paint;s.active().material_bindings=converted.material_bindings;
  auto artist=fixture(false,false);auto a=artist.scene.object(artist.source);const auto id=s.add("Actual dual-role source",Volume{});a.id=id;a.visible=false;a.locked=true;s.object(id)=a;s.select(1);require(bake_object(s,id,settings()).projected>500,"Real dual-role canonical replacement");
 }
 s.validate();return s;
}
void paint(Scene &s,bool physical){auto p=s.active().painted->snapshot();p->add_layer("Actual writable colour");PaintSettings ink;ink.front_only=false;ink.color={.12f,.74f,.25f,.65f};Brush brush;brush.from=brush.to={-.17f,.12f,0};brush.normal={0,0,1};brush.radius=.35f;brush.strength=.8f;p->begin_stroke();p->stamp(brush,ink);require(p->end_stroke(),"Actual varying sparse color stroke");if(physical){p->add_layer("Actual writable FLOAT");ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={8,.03f,2.5f};brush.from=brush.to={.12f,-.13f,0};brush.radius=.37f;brush.strength=1;p->begin_stroke();p->stamp(brush,ink);require(p->end_stroke()&&p->has_physical_emission(),"Actual independent sparse FLOAT stroke");}p->add_layer("Hidden retained detail");p->layer_properties("Hidden retained detail",false,.37f);s.active().painted=p;s.validate();}
void uv(Scene &s,bool canonical,bool seam){
 const auto &old=s.active().painted->mesh();Mesh target;std::vector<uint32_t> vertices;
 if(seam){for(auto v:old.indices){vertices.push_back(v);target.positions.push_back(old.positions[v]);target.normals.push_back(old.normals[v]);target.uv.push_back(old.uv[v]);target.indices.push_back(uint32_t(target.indices.size()));}}
 else{target=old;vertices.resize(old.positions.size());std::iota(vertices.begin(),vertices.end(),0);}
 for(auto &q:target.uv){const auto x=q[0];q={.2f+.6f*q[1],.8f-.6f*x};}
 if(canonical){auto next=detail::CanonicalPaintSampling::create(s,s.active().id,target,vertices);s.active().painted=next.paint;s.active().deformation=next.deformation;s.active().material_bindings=next.binding;require(next.owner->matches_current(*s.active().painted),"Actual independent canonical writable owner");}
 else{auto next=detail::retain_native_paint_model(s.active(),target,vertices);auto binding=detail::bind_native_paint(next.sampling.owner,s.active().material_bindings);s.active().painted=next.sampling.paint;s.active().deformation=next.deformation;s.active().material_bindings=binding;require(next.sampling.owner->matches_current(*s.active().painted),"Actual native32 current owner");}
 s.validate();
}
void key_pose(Scene&s,bool keyed,bool live,const char *name="Actual writable current key"){if(keyed){ShapeKeyEdit key;key.action=ShapeKeyAction::Create;key.name=name;author_shapekey(s,s.active().id,key);key.action=ShapeKeyAction::Replace;key.target=s.active().deformation->morphs->targets.size()-1;key.positions=s.active().painted->mesh().positions;key.positions[0].z+=.012f;key.positions[2].z-=.009f;author_shapekey(s,s.active().id,key);}if(live){auto d=*s.active().deformation;if(d.morphs)d.morph_weights.assign(d.morphs->targets.size(),.35f);d.pose.resize(2);d.pose[0].scale={1.2f,.8f,.7f};d.pose[1].scale={.8f,1.1f,1.3f};s.set_deformation(s.active().id,d);}s.validate();}
D decode_canonical(const SculptObject &o,size_t tri,std::array<float,3> bary,size_t role,bool wrong_domain=false,bool omit=false){
 const auto &b=*o.material_bindings;const auto &set=*b.normal_frames;const auto &f=*set.pbr;const auto &m=o.painted->mesh();const auto field=o.painted->baked()?o.painted->baked()->normal_field:nullptr;D N{},T{},B{};std::array<double,2> uv{};
 for(size_t k=0;k<3;++k){const size_t c=tri*3+k,v=m.indices[c];D n=vc(f.normals[c]),t{f.tangents[c][0],f.tangents[c][1],f.tangents[c][2]};if(o.deformation){for(size_t slot=0;slot<set.current_targets.size();++slot){const double w=o.deformation->morph_weights.empty()?0:o.deformation->morph_weights[slot];for(const auto &target:f.targets)if(target.id==set.current_targets[slot]){if(!target.normals.empty())n=add(n,mul(vc(target.normals[c]),w));if(!target.tangents.empty())t=add(t,mul(vc(target.tangents[c]),w));}}if(!o.deformation->pose.empty()){D nn{},tt{};const auto a=o.deformation->pose[0].scale,h=o.deformation->pose[1].scale;for(auto weight:o.deformation->skin->vertices[v]){D scale{a.x,a.y,a.z};if(weight.joint==1)scale={a.x*h.x,a.y*h.y,a.z*h.z};for(size_t axis=0;axis<3;++axis){nn[axis]+=n[axis]/scale[axis]*weight.weight;tt[axis]+=t[axis]*scale[axis]*weight.weight;}}n=nn;t=tt;}}
  n=unit(n);t=unit(t);N=add(N,mul(n,bary[k]));T=add(T,mul(t,bary[k]));auto bitangent=crossd(n,t);if(field)bitangent=unit(bitangent);B=add(B,mul(bitangent,bary[k]*f.tangents[c][3]));auto q=m.uv[v];if(field&&b.canonical_uv&&!wrong_domain)q=b.canonical_uv->coordinates()[b.canonical_uv->mesh()->indices[c]];for(size_t axis=0;axis<2;++axis)uv[axis]+=q[axis]*bary[k];
 }
 N=unit(N);T=unit(T);B=unit(B);if(omit)return N;const Image *image=nullptr;double scale=1;if(field)image=(role?field->mtoon():field->pbr()).get();else if(o.painted->source_maps()){const auto &view=o.painted->source_maps()->normal;image=view.image.get();scale=o.painted->source_maps()->normal_scale;const auto q=uv;uv={view.uv_transform[0]*q[0]+view.uv_transform[1]*q[1]+view.uv_transform[4],view.uv_transform[2]*q[0]+view.uv_transform[3]*q[1]+view.uv_transform[5]};}if(!image)return N;auto value=sample(*image,uv);auto ts=unit(D{(value[0]*2-1)*scale,(value[1]*2-1)*scale,value[2]*2-1});return unit(add(add(mul(T,ts[0]),mul(B,ts[1])),mul(N,ts[2])));
}
D physical(const PaintedMesh &paint,std::array<double,2> uv){const int r=int(paint.resolution());const double x=uv[0]*r-.5,y=uv[1]*r-.5;require(std::isfinite(x)&&std::isfinite(y),"Finite independent physical coordinates");const int ix=int(std::floor(x)),iy=int(std::floor(y));D result{};for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const auto px=std::clamp(ix+dx,0,r-1),py=std::clamp(iy+dy,0,r-1);D texel{};for(const auto &layer:paint.layers())if(layer.visible&&layer.opacity){const auto emission=paint.emission_pixel(layer.id,px,py);const double coverage=double(emission.coverage)*paint.pixel(layer.id,px,py).data[13]/255.*layer.opacity;for(size_t axis=0;axis<3;++axis)texel[axis]=texel[axis]*(1-coverage)+emission.physical[axis]*coverage;}const double weight=(dx?x-ix:1-(x-ix))*(dy?y-iy:1-(y-iy));result=add(result,mul(texel,weight));}finite_sample(result);return result;}

D decode_native(const SculptObject&o,size_t triangle,std::array<float,3>bary){
 const auto &mesh=o.painted->mesh();const auto frames=o.material_bindings->normal_frames;D sum{};
 for(size_t k=0;k<3;++k){const size_t c=triangle*3+k,v=mesh.indices[c];D n=vc(frames?frames->pbr->normals[c]:mesh.normals[v]);
  if(frames&&o.deformation&&o.deformation->morphs)for(size_t slot=0;slot<frames->current_targets.size();++slot)for(const auto &target:frames->pbr->targets)if(target.id==frames->current_targets[slot]&&!target.normals.empty())n=add(n,mul(vc(target.normals[c]),o.deformation->morph_weights.empty()?0:o.deformation->morph_weights[slot]));
  if(o.deformation&&!o.deformation->pose.empty()){D nn{};const auto a=o.deformation->pose[0].scale,h=o.deformation->pose[1].scale;for(auto weight:o.deformation->skin->vertices[v]){D scale{a.x,a.y,a.z};if(weight.joint==1)scale={a.x*h.x,a.y*h.y,a.z*h.z};for(size_t axis=0;axis<3;++axis)nn[axis]+=n[axis]/scale[axis]*weight.weight;}n=nn;}
  sum=add(sum,mul(unit(n),bary[k]));
 }return unit(sum);
}
std::array<double,2> root_uv(const SculptObject&o,size_t triangle,std::array<float,3>bary){const auto domain=o.painted->paint_sampling_domain();const auto &basis=*domain->basis();std::array<double,2> uv{};for(size_t k=0;k<3;++k){const auto q=basis.uv[basis.indices[domain->source_corners()[triangle*3+k]]];for(size_t axis=0;axis<2;++axis)uv[axis]+=double(q[axis])*bary[k];}return uv;}
void exercise(bool canonical,bool seam,bool keyed,bool live,bool repeat,bool special,const std::filesystem::path&out){
 auto s=source(canonical);paint(s,canonical||special);uv(s,canonical,seam);key_pose(s,keyed,live);if(repeat)uv(s,canonical,!seam);if(canonical&&special){auto p=s.active().painted->snapshot();p->clear_bake();s.active().painted=p;s.validate();}
 const auto source=s.active().id;const auto source_context=s.encode();const auto resolver=detail::source_bake_normals(s.active(),{});const auto raw=detail::raw_bake_source(s.active(),{},&resolver);require(raw&&raw->source_only&&resolver.matches_prepared_source(*raw),"Factory packet authenticates current writable material/frame/paint owners");
 double direct=0,wrong_paint=0;for(size_t tri=0;tri<2;++tri)for(auto q:std::array<std::array<float,3>,3>{{{.2f,.3f,.5f},{.6f,.25f,.15f},{.15f,.7f,.15f}}}){const auto actual=resolver(tri,q);for(size_t role=0;role<2;++role)direct=std::max(direct,error(canonical?decode_canonical(s.active(),tri,q,role):decode_native(s.active(),tri,q),actual[role]));const auto root=root_uv(s.active(),tri,q);std::array<float,2> current{};for(size_t k=0;k<3;++k)for(size_t axis=0;axis<2;++axis)current[axis]+=s.active().painted->mesh().uv[s.active().painted->mesh().indices[tri*3+k]][axis]*q[k];for(size_t axis=0;axis<2;++axis)wrong_paint=std::max(wrong_paint,std::abs(root[axis]-current[axis]));}
 require(direct<3e-6&&wrong_paint>.05,"Independent normal decoder and actual different paint/current UV domains");
 const auto evaluated=s.active().deformation?s.active().deformation->preview(s.active().painted->mesh()):s.active().painted->mesh();
 // A current key may separate equal original seam vertices in evaluated space.
 // The destination therefore owns two genuinely independent strict charts;
 // sharing their old diagonal texels would be an invalid new paint target.
 Mesh mesh;const std::array<std::array<std::array<float,2>,3>,2> charts{{{{{.03f,.03f},{.47f,.03f},{.47f,.97f}}},{{{.53f,.03f},{.97f,.97f},{.53f,.97f}}}}};
 for(size_t c=0;c<evaluated.indices.size();++c){const auto v=evaluated.indices[c];mesh.positions.push_back(evaluated.positions[v]);mesh.normals.push_back(evaluated.normals[v]);mesh.uv.push_back(charts.at(c/3).at(c%3));mesh.indices.push_back(uint32_t(c));}
 const auto destination=target(s,mesh,false);s.select(destination);const auto before=s.encode();const auto native=s.snapshot();require(bake_object(s,source,settings()).projected>100,"Actual writable SOURCE Bake projects without mutating its owner");
 double maximum=0,color_error=0,emission_error=0,peak=0,wrong_material=0;size_t interior=0;const auto material=detail::EmissionExportAccess::other_channels(*s.object(source).painted,s.object(source).material,{});const auto &low=s.active().painted->mesh();
 for(size_t y=0;y<64;++y)for(size_t x=0;x<64;++x){const std::array<double,2> q{(x+.5)/64.,(y+.5)/64.};for(size_t tri=0;tri<2;++tri){auto a=low.uv[low.indices[tri*3]],b=low.uv[low.indices[tri*3+1]],c=low.uv[low.indices[tri*3+2]];const double den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1]);require(std::isfinite(den)&&std::abs(den)>1e-20,"Finite target UV determinant");const double u=((b[1]-c[1])*(q[0]-c[0])+(c[0]-b[0])*(q[1]-c[1]))/den,v=((c[1]-a[1])*(q[0]-c[0])+(a[0]-c[0])*(q[1]-c[1]))/den;if(std::min({u,v,1-u-v})<.08)continue;const std::array<float,3>bary{float(u),float(v),float(1-u-v)};for(size_t role=0;role<2;++role){const auto wanted=canonical?decode_canonical(s.object(source),tri,bary,role):decode_native(s.object(source),tri,bary);const auto actual=direction(s.active(),tri,bary,role);finite_sample(actual);for(size_t axis=0;axis<3;++axis)maximum=std::max(maximum,std::abs(wanted[axis]-actual[axis]));}const auto paint_uv=root_uv(s.object(source),tri,bary);const auto wanted=sample(material->color,paint_uv,true),actual=sample(s.active().painted->baked()->maps.color,q,true);std::array<double,2> wrong_uv{};for(size_t k=0;k<3;++k)for(size_t axis=0;axis<2;++axis)wrong_uv[axis]+=double(s.object(source).painted->mesh().uv[s.object(source).painted->mesh().indices[tri*3+k]][axis])*bary[k];const auto wrong=sample(material->color,wrong_uv,true);for(size_t axis=0;axis<3;++axis){color_error=std::max(color_error,std::abs(wanted[axis]-actual[axis]));wrong_material=std::max(wrong_material,std::abs(wanted[axis]-wrong[axis]));}const auto emission=physical(*s.object(source).painted,paint_uv);const auto &baked=*s.active().painted->baked();require(bool(baked.emission_field)==(canonical||special),"Actual writable SOURCE Bake retains genuine FLOAT presence/absence");const auto value=baked.emission_field?baked.emission_field->sample(float(q[0]),float(q[1])).resolve({},1):std::array<float,3>{};for(size_t axis=0;axis<3;++axis){peak=std::max(peak,emission[axis]);emission_error=std::max(emission_error,std::abs(emission[axis]-value[axis]));}++interior;break;}}
 require(interior>300&&maximum<.035&&color_error<.025&&wrong_material>.05&&emission_error<2e-4&&(!(canonical||special)||peak>1),"Native second Bake independently preserves normal/material/FLOAT sampling under unchanged numerical bounds");
 const auto name=std::to_string(canonical)+std::to_string(seam)+std::to_string(keyed)+std::to_string(live)+std::to_string(repeat)+std::to_string(special);const auto accepted=s.encode();s.save(out/(name+".dsculpt"));require(Scene::load(out/(name+".dsculpt")).encode()==accepted,"Writable source Save/reopen exact");Document doc;doc.replace(native);require(doc.wait_mesh(),"Actual writable worker baseline");doc.bake_from_object(source,settings());doc.save_async(out/(name+"-ordered.dsculpt")).get();require(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty()&&doc.copy_scene().encode()==accepted,"Actual writable SOURCE worker and immediately ordered Save exact");require(doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==before&&doc.redo()&&doc.wait_mesh()&&doc.copy_scene().encode()==accepted,"Writable SOURCE Bake owns one exact UndoRedo transaction");
 auto restored=s;restored.erase(destination);restored.select(source);auto original=restored.encode();original["next_id"]=source_context.at("next_id");require(original==source_context,"All source material/paint/normal/deformation/history owners remain exact");
 const auto current=detail::raw_bake_source(s.object(source));auto foreign=s.object(source);foreign.material_bindings=std::make_shared<SourceMaterialBindings>(*foreign.material_bindings);require(!resolver.matches_prepared_source(*detail::raw_bake_source(foreign)),"Equal copied writable binding cannot borrow packet");auto stripped=std::make_shared<TextureMaps>(*current);stripped->overlay=detail::PaintOverlayFactory::capture(*s.object(source).painted);require(!resolver.matches_prepared_source(*stripped),"Stripped writable frame/material marker refuses pairing");BakeSource bare;bare.mesh=s.object(source).painted->mesh();bare.materials[0].maps=current;BakeOptions limits;limits.resolution=64;limits.ao_samples=0;limits.padding=0;require(refuses([&]{bake_mesh(bare.mesh,bare,limits);}),"Naked public Bake cannot borrow genuine writable material packet");
 std::cout<<"WRITABLE_SOURCE "<<name<<" direct="<<direct<<" normal="<<maximum<<" material="<<color_error<<" wrongMaterial="<<wrong_material<<" emission="<<emission_error<<" peak="<<peak<<" interior="<<interior<<'\n';
}
void negatives(){
 for(bool canonical:{false,true}){
  auto s=source(canonical);paint(s,true);uv(s,canonical,false);
  auto alias=std::make_shared<SourceMaterialBindings>(*s.active().material_bindings);s.active().material_bindings=alias;s.validate();
  const auto baseline=s.encode();const auto resolver=detail::source_bake_normals(s.active(),{});const auto raw=detail::raw_bake_source(s.active(),{},&resolver);
  const std::array<float,3>bary{.2f,.3f,.5f};const auto expected=root_uv(s.active(),0,bary);
  auto foreign=source(canonical);paint(foreign,true);uv(foreign,canonical,false);uv(foreign,canonical,false);
  const auto native=alias->native_paint_sampling;const auto converted=alias->canonical_paint_sampling;
  if(canonical)alias->canonical_paint_sampling=foreign.active().material_bindings->canonical_paint_sampling;
  else alias->native_paint_sampling=foreign.active().material_bindings->native_paint_sampling;
  const auto captured=detail::bake_paint_coordinates(resolver.paint_domain().get(),0,bary,{.7f,.8f});
  require(std::abs(captured[0]-expected[0])<1e-7&&std::abs(captured[1]-expected[1])<1e-7&&resolver.matches_prepared_source(*raw),"Captured normal/material/paint tuple remains immutable after legally mutable binding alias edits");
  require(refuses([&]{detail::raw_bake_source(s.active());}),"A later mismatched mutable binding cannot mint a new factory packet");
  alias->native_paint_sampling=native;alias->canonical_paint_sampling=converted;require(s.encode()==baseline,"Actual mutable binding probe restores exact complete source graph");
  detail::SourceBakeNormalOptions policy;policy.max_working_bytes=1;
  require(refuses([&]{detail::source_bake_normals(s.active(),{},policy);})&&s.encode()==baseline,"Prospective writable source quota refuses atomically");
  size_t count=0;policy={};policy.cancelled=[&]{++count;return false;};detail::source_bake_normals(s.active(),{},policy);require(count>2,"Actual writable capture exposes genuine traversal polls");
  for(size_t boundary:{size_t(1),count/2,count}){size_t at=0;policy.cancelled=[&]{return ++at>=boundary;};require(refuses([&]{detail::source_bake_normals(s.active(),{},policy);})&&s.encode()==baseline,"Early/middle/final writable source capture cancellation preserves every owning source byte");}
 }
}

void affine_source(bool canonical,const std::filesystem::path &out){
 auto scene=source(canonical);
 paint(scene,true);
 uv(scene,canonical,true);
 auto &donor=scene.active();
 const auto id=donor.id;
 const auto paint_owner=donor.painted;
 const auto binding=donor.material_bindings;
 const std::array<double,16> matrix{-1.3,.17,0,0, .20,.9,0,0, 0,0,1.12,0, .07,-.1,.2,1};
 donor.affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(matrix));
 donor.visible=false;
 const auto frame=detail::placement_frame(scene,id);
 auto mesh=paint_owner->mesh();
 for(auto &p:mesh.positions){
   const auto q=frame.world().point({p.x,p.y,p.z});
   p={float(q[0]),float(q[1]),float(q[2])};
 }
 scene.select(target(scene,mesh,false));
 scene.save(out/(canonical?"canonical-affine-before.dsculpt":"native-affine-before.dsculpt"));
 auto options=settings();
 options.corresponding_topology=true;
 const auto report=bake_object(scene,id,options);
 const auto result=scene.active().painted->baked();
 require(report.projected>500&&result&&result->emission_field&&
         bool(result->maps.normal.rgba!=result->maps.mtoon_normal.rgba)==canonical&&
         scene.object(id).painted==paint_owner&&scene.object(id).material_bindings==binding,
         "Affine matching Bake preserves the original writable donor and its normal and FLOAT roles");
}

void rasterize_canonical_paint(Scene &scene){
  const auto &object=scene.active();
  SourceUvBakeInput input;
  input.paint=object.painted;
  input.current_material=object.material_bindings;
  input.current_deformation=object.deformation;
  input.target=object.painted->mesh();
  input.source_vertices.resize(input.target.positions.size());
  std::iota(input.source_vertices.begin(),input.source_vertices.end(),0);
  for(auto &q:input.target.uv)
    q={.125f+.75f*q[0],.125f+.75f*q[1]};
  auto adopted=adopt_source_uv_bake(bake_source_uv(input));
  scene.active().painted=std::move(adopted.paint);
  scene.active().material_bindings=std::move(adopted.material_bindings);
  scene.active().deformation=std::move(adopted.deformation);
  scene.active().appearance=std::move(adopted.appearance);
  scene.validate();
}

bool same_layers(const PaintedMesh &a,const PaintedMesh &b){
  if(a.active_layer()!=b.active_layer()||a.layers().size()!=b.layers().size())return false;
  for(size_t i=0;i<a.layers().size();++i){
    const auto &x=a.layers()[i],&y=b.layers()[i];
    if(x.id!=y.id||x.name!=y.name||x.visible!=y.visible||x.opacity!=y.opacity||
       x.tiles!=y.tiles||x.pbr_tiles!=y.pbr_tiles||x.emission_tiles!=y.emission_tiles)return false;
  }
  return true;
}

void canonical_recipient(bool physical,bool partial,const std::filesystem::path &out){
  auto seed=source(true);
  rasterize_canonical_paint(seed);
  auto high=seed.active().painted->mesh();
  high.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};
  for(auto &p:high.positions){
    if(partial&&p.x<0)p.x=0;
    p.z=.02f;
  }
  high.normals.assign(high.positions.size(),normalized(Vec3{-.17f,.28f,1}));
  const auto donor=seed.add("Colour and FLOAT projection donor",Volume{});
  seed.select(donor);
  seed.active().mesh_active=true;
  seed.active().painted=std::make_shared<PaintedMesh>(high,64);
  paint(seed,physical);
  seed.active().visible=false;
  seed.active().locked=true;
  seed.select(1);
  paint(seed,true);
  const auto prior=seed.active().painted->baked();
  require(bool(prior&&prior->normal_field),"Recipient starts with an independent prior normal field");

  auto edited=seed;
  uv(edited,true,partial);
  const auto &object=edited.active();
  const auto binding=object.material_bindings;
  const auto before_paint=object.painted;
  const auto &mesh=before_paint->mesh();
  const auto &domain=*before_paint->paint_sampling_domain();
  require(domain.source_corners().size()==mesh.indices.size(),"Recipient has a complete writable corner correspondence");
  for(size_t corner=0;corner<mesh.indices.size();++corner){
    const auto writable=domain.basis()->uv[domain.basis()->indices[domain.source_corners()[corner]]];
    require(writable==seed.active().painted->mesh().uv[seed.active().painted->mesh().indices[corner]]&&
            writable!=mesh.uv[mesh.indices[corner]]&&
            writable!=binding->canonical_uv->coordinates()[mesh.indices[corner]],
            "Recipient has separate display, writable paint and retained normal coordinates");
  }
  const std::string name=partial?"partial-hdr":physical?"matching-hdr":"matching";
  const auto before=edited.encode();
  edited.save(out/(name+"-before.dsculpt"));
  auto options=settings();
  options.allow_partial=partial;
  options.corresponding_topology=!partial;
  auto control=seed;
  const auto expected=bake_object(control,donor,options);
  const auto actual=bake_object(edited,donor,options);
  const auto &want=*control.active().painted->baked();
  const auto &got=*edited.active().painted->baked();
  require(actual.projected==expected.projected&&actual.projected>900&&
          actual.missed==expected.missed&&actual.preserved_misses==expected.preserved_misses&&
          (partial?actual.preserved_misses>900:actual.rays==0&&actual.missed==0),
          "Bake takes coverage from the writable atlas and preserves uncovered prior samples");
  require(got.coverage==want.coverage&&got.maps.color.rgba==want.maps.color.rgba&&
          got.maps.metal_rough.rgba==want.maps.metal_rough.rgba,
          "Retained writable material bytes match the same surface baked before the display UV edit");
  require(physical?(want.emission_field&&got.emission_field&&
                   got.emission_field->encode()==want.emission_field->encode()&&
                   std::any_of(got.emission_field->values().begin(),got.emission_field->values().end(),
                               [](const auto &e){return e.absolute[0]>2;})):
                   (!want.emission_field&&!got.emission_field&&got.maps.emission.rgba==want.maps.emission.rgba),
          "Retained writable FLOAT emission matches the independent atlas control");
  require(got.normal_field&&want.normal_field&&got.normal_field->prior()==prior->normal_field&&
          got.normal_field->pbr()->rgba==want.normal_field->pbr()->rgba&&
          got.normal_field->mtoon()->rgba==want.normal_field->mtoon()->rgba,
          "Both normal roles stay on their independent normal atlas");
  require(edited.active().material_bindings==binding&&edited.active().painted->mesh_owner()==before_paint->mesh_owner()&&
          edited.active().painted->paint_sampling_attachment()==before_paint->paint_sampling_attachment()&&
          same_layers(*before_paint,*edited.active().painted)&&
          edited.active().bake_recipe&&edited.active().bake_recipe->source.object==donor&&
          edited.active().bake_recipe->settings.corresponding_topology==!partial,
          "Bake preserves editable layers, geometry and sampling owners and records its recipe");

  const auto accepted=edited.encode();
  edited.save(out/(name+"-accepted.dsculpt"));
  require(Scene::load(out/(name+"-accepted.dsculpt")).encode()==accepted,"Writable recipient round trips exactly");
  Document doc;
  doc.replace(Scene::load(out/(name+"-before.dsculpt")));
  require(doc.wait_mesh(),"Writable recipient builds the actual worker baseline");
  doc.bake_from_object(donor,options);
  doc.save_async(out/(name+"-ordered.dsculpt")).get();
  require(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty()&&
          doc.copy_scene().encode()==accepted&&Scene::load(out/(name+"-ordered.dsculpt")).encode()==accepted,
          "Actual Bake worker and dependent Save publish the same accepted recipient");
  require(doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==before&&
          doc.redo()&&doc.wait_mesh()&&doc.copy_scene().encode()==accepted,
          "Writable recipient Bake owns one exact Undo and Redo");
  doc.clear_bake();
  require(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty(),"Clear Bake is available on the rebaked recipient");
  const auto cleared=doc.copy_scene();
  require(!cleared.active().painted->baked()&&cleared.active().painted->paint_sampling_attachment()&&
          cleared.active().bake_recipe->source.object==donor&&
          doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==accepted,
          "Clear Bake preserves the writable authority and recipe and can be undone exactly");
  std::cout<<"WRITABLE_RECIPIENT "<<name<<" projected="<<actual.projected<<" missed="<<actual.missed
           <<" preserved="<<actual.preserved_misses<<" rays="<<actual.rays<<'\n';
}

void native_recipient(bool physical,const std::filesystem::path &out){
  auto seed=source(false);
  const auto rest=seed.active().painted->mesh();
  auto high=rest;
  for(auto &p:high.positions)p.z=.02f;
  high.normals.assign(high.positions.size(),normalized(Vec3{-.37f,.28f,1}));
  const auto donor=seed.add("Native writable atlas projection donor",Volume{});
  seed.select(donor);
  seed.active().mesh_active=true;
  seed.active().painted=std::make_shared<PaintedMesh>(high,64);
  paint(seed,physical);
  seed.active().visible=false;
  seed.active().locked=true;
  seed.select(1);
  paint(seed,physical);
  auto scene=seed;
  uv(scene,false,false);
  const auto material=scene.active().material_bindings;
  const auto before_paint=scene.active().painted;
  const std::string name=physical?"hdr":"ldr";
  scene.save(out/(name+"-before.dsculpt"));
  const auto before=scene.encode();
  auto options=settings();
  options.prepare_normals=false;
  options.corresponding_topology=true;
  const auto reference=bake_object(seed,donor,options);
  const auto result=bake_object(scene,donor,options);
  const auto &expected=*seed.active().painted->baked();
  const auto baked=scene.active().painted->baked();
  require(result.projected==reference.projected&&result.projected==4096&&result.rays==0&&
          baked->coverage==expected.coverage&&baked->maps.normal.rgba==expected.maps.normal.rgba&&
          baked->maps.color.rgba==expected.maps.color.rgba&&baked->maps.metal_rough.rgba==expected.maps.metal_rough.rgba,
          "Native writable Bake matches the original atlas material, normal and coverage");
  require(physical?(baked->emission_field&&expected.emission_field&&
                    baked->emission_field->encode()==expected.emission_field->encode()):
                   (!baked->emission_field&&baked->maps.emission.rgba==expected.maps.emission.rgba),
          "Native writable Bake preserves the emitted radiance representation");
  require(scene.active().material_bindings==material&&scene.active().painted->mesh_owner()==before_paint->mesh_owner()&&
          scene.active().painted->paint_sampling_attachment()==before_paint->paint_sampling_attachment()&&
          same_layers(*scene.active().painted,*before_paint),"Native writable recipient preserves every live owner and layer");

  const auto accepted=scene.encode();
  scene.save(out/(name+"-accepted.dsculpt"));
  require(Scene::load(out/(name+"-accepted.dsculpt")).encode()==accepted,"Native writable baked project reopens precisely");
  Document doc;
  doc.replace(Scene::load(out/(name+"-before.dsculpt")));
  require(doc.wait_mesh()&&doc.frame().state->can_bake,"Real worker opens native writable Bake on the retained target");
  doc.bake_from_object(donor,options);
  doc.save_async(out/(name+"-ordered.dsculpt")).get();
  require(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty()&&
          doc.copy_scene().encode()==accepted&&Scene::load(out/(name+"-ordered.dsculpt")).encode()==accepted,
          "Native Bake and immediately ordered Save publish exact retained state");
  require(doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==before&&
          doc.redo()&&doc.wait_mesh()&&doc.copy_scene().encode()==accepted,
          "Native writable Bake has exact Undo and Redo");

  const auto original_tangent=tangent_corners(rest);
  const auto wrong_tangent=tangent_corners(scene.active().painted->mesh());
  require(original_tangent!=wrong_tangent,"The edited display atlas is a sensitive wrong-frame control");
  const auto snapshot=doc.frame().surface;
  require(snapshot&&snapshot->mesh.indices.size()==rest.indices.size()&&
          snapshot->paint_coordinates.size()==snapshot->mesh.positions.size(),
          "The live worker publishes its complete writable Bake corner layout");
  for(size_t c=0;c<rest.indices.size();++c){
    const auto v=snapshot->mesh.indices[c];
    require(snapshot->tangents.at(v)==original_tangent[c]&&
            snapshot->paint_coordinates.at(v)==rest.uv[rest.indices[c]],
            "Live worker uses the original tangent and paint UV for each corner after Bake");
  }

  auto rebake=scene;
  uv(rebake,false,true);
  require(rebake.active().painted->baked()==baked,"Further UV changes keep the exact baked field");
  rebake.save(out/(name+"-edited.dsculpt"));
  auto reopened=Scene::load(out/(name+"-edited.dsculpt"));
  require(reopened.encode()==rebake.encode(),"Native Bake survives a further UV edit through encode and replay");
  const auto resolve=detail::source_bake_normals(reopened.active(),{});
  const auto maps=seed.active().painted->preview(seed.active().material);
  constexpr std::array<float,3> bary{.28f,.33f,.39f};
  for(uint32_t tri=0;tri<rest.indices.size()/3;++tri){
    std::array<double,2> coordinate{};
    for(size_t k=0;k<3;++k)for(size_t axis=0;axis<2;++axis)
      coordinate[axis]+=double(rest.uv[rest.indices[size_t(tri)*3+k]][axis])*bary[k];
    const auto rgb=sample(maps->normal,coordinate);
    const auto ideal=tangent_frame(rest,original_tangent,tri,bary).to_world(
        {float(rgb[0]*2-1),float(rgb[1]*2-1),float(rgb[2]*2-1)});
    require(length(resolve(tri,bary)[0]-ideal)<1e-5f,"Reopened writable Bake donates its retained normal in the correct domain");
  }
  doc.clear_bake();
  require(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty()&&
          !doc.copy_scene().active().painted->baked()&&doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==accepted,
          "Native writable Clear Bake and Undo refresh the same corner layout");

  for(bool editable:{false,true}){
    const auto glb=out/(name+(editable?"-editable.glb":".glb"));
    doc.export_asset(glb,editable?AssetExportFormat::EditableGlb256:AssetExportFormat::Glb).get();
    std::ifstream stream(glb,std::ios::binary);
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(stream),{}};
    uint32_t count=0;
    for(unsigned k=0;k<4;++k)count|=uint32_t(bytes.at(12+k))<<(8*k);
    const auto gltf=nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+count);
    const auto &primitive=gltf.at("meshes").at(0).at("primitives").at(0);
    const auto &output_material=gltf.at("materials").at(primitive.at("material").get<size_t>());
    const auto uv_set=output_material.at("normalTexture").at("texCoord").get<unsigned>();
    require(uv_set>0&&primitive.at("attributes").contains("TANGENT")&&
            primitive.at("attributes").contains("TEXCOORD_"+std::to_string(uv_set))&&
            output_material.at("pbrMetallicRoughness").at("baseColorTexture").at("texCoord")==uv_set,
            "GLB shares the actual paint basis between baked normals and colour and retains UV0");
    if(editable){
      const auto extracted=extract_native_authoring(bytes);
      require(extracted&&extracted->encode()==accepted,"Editable GLB restores the entire baked paint and owner history");
    }
    require(doc.copy_scene().encode()==accepted,"Portable export leaves the working scene exact");
  }
  std::cout<<"NATIVE_WRITABLE_RECIPIENT "<<name<<" projected="<<result.projected<<" rays="<<result.rays<<'\n';
}

void native_key_recipient(bool physical,const std::filesystem::path &out){
  auto scene=source(false);
  const auto rest=scene.active().painted->mesh();
  paint(scene,physical);
  uv(scene,false,false);
  key_pose(scene,true,false);
  const auto control_tangents=tangent_corners(rest);
  const auto binding=scene.active().material_bindings;
  const auto frames=binding->normal_frames->pbr;
  require(binding->native_paint_frames&&frames->tangents!=control_tangents&&
          !frames->targets.front().tangents.empty(),
          "A real authored shapekey has an independent tangent basis and nonempty direction targets");
  auto add_donor=[&](std::string label,Vec3 normal,bool partial){
    auto mesh=rest;
    for(auto &p:mesh.positions){
      if(partial&&p.x<0)p.x=0;
      p.z=.02f;
    }
    mesh.normals.assign(mesh.positions.size(),normal);
    const auto id=scene.add(std::move(label),Volume{});
    scene.select(id);
    scene.active().mesh_active=true;
    scene.active().painted=std::make_shared<PaintedMesh>(std::move(mesh),64);
    paint(scene,physical);
    scene.active().visible=false;
    scene.active().locked=true;
    scene.select(1);
    return id;
  };
  const auto direction=normalized(Vec3{-.37f,.28f,1});
  const auto donor=add_donor("Keyed mesh normal and material donor",direction,false);
  const auto second=add_donor("Partial keyed mesh donor",normalized(Vec3{.25f,-.3f,1}),true);
  const auto before_paint=scene.active().painted;
  const auto model=scene.active().deformation;
  const auto before=scene.encode();
  const std::string name=physical?"hdr":"ldr";
  scene.save(out/(name+"-before.dsculpt"));
  auto options=settings();
  options.prepare_normals=false;
  options.corresponding_topology=true;
  const auto report=bake_object(scene,donor,options);
  const auto bake=scene.active().painted->baked();
  require(report.projected==4096&&report.rays==0&&bake&&
          bake->normal_interpretation==BakedNormalInterpretation::NativeAttributes&&
          bake->maps.mtoon_normal.rgba.empty()&&bool(bake->emission_field)==physical&&
          scene.active().material_bindings==binding&&scene.active().deformation==model&&
          same_layers(*scene.active().painted,*before_paint),"Native Bake retains the actual authored frame and writable layers");
  const auto sample=bake->maps.normal.rgba.data()+4*(41*64+29);
  const auto t=frames->tangents[0];
  const Vec3 tangent{t[0],t[1],t[2]},normal=frames->normals[0],bitangent=cross(normal,tangent)*t[3];
  const auto decoded=normalized(tangent*(sample[0]/127.5f-1)+bitangent*(sample[1]/127.5f-1)+normal*(sample[2]/127.5f-1));
  const auto wrong=control_tangents.front();
  const Vec3 wrong_tangent{wrong[0],wrong[1],wrong[2]};
  const auto misread=normalized(wrong_tangent*(sample[0]/127.5f-1)+
      cross(normal,wrong_tangent)*(wrong[3]*(sample[1]/127.5f-1))+normal*(sample[2]/127.5f-1));
  require(length(decoded-direction)<.015f&&length(misread-direction)>.25f,
          "Baked texels decode the donor direction in the authored frame and reject the writable Mikk control");

  const auto accepted=scene.encode();
  scene.save(out/(name+"-accepted.dsculpt"));
  require(Scene::load(out/(name+"-accepted.dsculpt")).encode()==accepted,"Keyed writable Bake restarts exactly");
  Document doc;
  doc.replace(Scene::load(out/(name+"-before.dsculpt")));
  require(doc.wait_mesh()&&doc.frame().state->can_bake,"Keyed recipient is eligible in the actual shared Bake controls");
  doc.bake_from_object(donor,options);
  doc.save_async(out/(name+"-ordered.dsculpt")).get();
  require(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty()&&doc.copy_scene().encode()==accepted&&
          Scene::load(out/(name+"-ordered.dsculpt")).encode()==accepted,"Keyed Bake publishes exact ordered Save");
  require(doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==before&&
          doc.redo()&&doc.wait_mesh()&&doc.copy_scene().encode()==accepted,"Keyed Bake owns one Undo and Redo");
  const auto frame=doc.frame().surface;
  require(frame&&frame->ranges.size()==1&&frame->ranges[0].source_frame&&!frame->ranges[0].canonical_frame&&
          frame->source_tangents.at(frame->mesh.indices.front())==t&&
          frame->paint_coordinates.at(frame->mesh.indices.front())==rest.uv.front(),
          "Live viewport uses authored tangent columns and independent paint UVs");
  doc.clear_bake();
  require(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty()&&
          !doc.copy_scene().active().painted->baked()&&doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==accepted,
          "Clear Bake preserves keyed authority and is reversible");

  for(bool editable:{false,true}){
    const auto glb=out/(name+(editable?"-editable.glb":".glb"));
    doc.export_asset(glb,editable?AssetExportFormat::EditableGlb256:AssetExportFormat::Glb).get();
    std::ifstream stream(glb,std::ios::binary);
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(stream),{}};
    uint32_t count=0;
    for(unsigned k=0;k<4;++k)count|=uint32_t(bytes.at(12+k))<<(8*k);
    const auto gltf=nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+count);
    const auto &primitive=gltf.at("meshes").at(0).at("primitives").at(0);
    const auto &material=gltf.at("materials").at(primitive.at("material").get<size_t>());
    const auto coord=material.at("normalTexture").at("texCoord").get<unsigned>();
    const auto &key=primitive.at("targets").at(0);
    require(coord>0&&key.contains("POSITION")&&key.contains("NORMAL")&&key.contains("TANGENT")&&
            material.at("pbrMetallicRoughness").at("baseColorTexture").at("texCoord")==coord,
            "Both GLB routes bind normal and paint coordinates and retain every authored key channel");
    if(editable){
      const auto extracted=extract_native_authoring(bytes);
      require(extracted&&extracted->encode()==accepted,"Editable keyed GLB restores the authored bake exactly");
    }
  }

  options.corresponding_topology=false;
  options.allow_partial=true;
  const auto partial=bake_object(scene,second,options);
  const auto patched=scene.active().painted->baked();
  require(partial.projected==2048&&partial.preserved_misses==2048,"Partial rebake preserves all missed keyed texels");
  for(auto pixel:{size_t(32*64+12),size_t(32*64+50)}){
    const auto old=bake->maps.normal.rgba.begin()+pixel*4;
    const bool same=std::equal(old,old+4,patched->maps.normal.rgba.begin()+pixel*4);
    require(same==(pixel%64<32),"Partial keyed normal Bake only updates the intersecting region");
  }
  uv(scene,false,true);
  key_pose(scene,true,false,"New key after partial Bake and seam edit");
  require(scene.active().painted->baked()==patched,"Later UV seam split and additional key keep the authored normal image");
  scene.save(out/(name+"-edited.dsculpt"));
  auto reopened=Scene::load(out/(name+"-edited.dsculpt"));
  require(reopened.encode()==scene.encode(),"Nested Bake and edited key histories reopen exactly");
  const auto &object=reopened.active();
  const auto resolved=detail::source_bake_normals(object,{});
  const auto wanted=resolved(0,{.1f,.45f,.45f});
  require(length(wanted[0]-normalized(Vec3{.25f,-.3f,1}))<.015f&&length(wanted[0]-wanted[1])<1e-7f,
          "Reopened keyed Bake donates the correct normal from its retained paint grid");
  std::cout<<"NATIVE_KEY_RECIPIENT "<<name<<" projected="<<report.projected<<" preserved="<<partial.preserved_misses<<'\n';
}
}
int main(int argc,char **argv) try {
 if(argc==3&&std::string_view(argv[1])=="--sources"){
   auto scene=Scene::load(argv[2]);
   scene.erase(scene.active().id);
   std::cout<<scene.encode().dump()<<'\n';
   return 0;
 }
 if(argc==3&&std::string_view(argv[1])=="--recipient"){
   const std::filesystem::path out=argv[2];
   std::filesystem::create_directories(out);
   writable_source::canonical_recipient(false,false,out);
   for(bool partial:{false,true})writable_source::canonical_recipient(true,partial,out);
   std::cout<<"PASS "<<writable_source::checks<<" writable recipient checks\n";
   return 0;
 }
 if(argc==3&&std::string_view(argv[1])=="--native-recipient"){
   const std::filesystem::path out=argv[2];
   std::filesystem::create_directories(out);
   for(bool physical:{false,true})writable_source::native_recipient(physical,out);
   std::cout<<"PASS "<<writable_source::checks<<" native writable recipient checks\n";
   return 0;
 }
 if(argc==3&&std::string_view(argv[1])=="--key-recipient"){
   const std::filesystem::path out=argv[2];
   std::filesystem::create_directories(out);
   for(bool physical:{false,true})writable_source::native_key_recipient(physical,out);
   std::cout<<"PASS "<<writable_source::checks<<" keyed writable recipient checks\n";
   return 0;
 }
 if(argc!=2)return 2;
 const std::filesystem::path out=argv[1];
 std::filesystem::create_directories(out);
 for(bool canonical:{false,true}){
   writable_source::affine_source(canonical,out);
   for(bool seam:{false,true})for(bool keyed:{false,true})for(bool live:{false,true})
     for(bool repeat:{false,true})for(bool special:{false,true})
       writable_source::exercise(canonical,seam,keyed,live,repeat,special,out);
 }
 writable_source::negatives();
 std::cout<<"PASS "<<writable_source::checks<<" writable source checks\n";
 return 0;
} catch(const std::exception &error) {
 std::cerr<<"FAIL "<<error.what()<<'\n';
 return 1;
}
