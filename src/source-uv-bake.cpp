// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/source-uv-bake.hpp"
#include "dome/tangents.hpp"
#include "normal-frame-runtime.hpp"
#include "dome/color-preparation.hpp"
#include "paint-color-access.hpp"
#include "paint-normal-access.hpp"
#include "normal-projection-identity.hpp"
#include "source-colors.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/polygon-color.hpp"
#include <algorithm>
#include <deque>
#include <limits>
#include <map>
#include <set>
namespace dome {
namespace {
struct Hit {uint32_t face=UINT32_MAX;std::array<float,3> bary{};};
struct Raster {uint32_t size=0;std::vector<Hit> hits;std::vector<int32_t> padding;};
struct Budget {
 const SourceUvBakeOptions &o;SourceUvBakeResult &r;
 void cancel()const{if(o.cancelled&&o.cancelled())throw std::runtime_error("Source UV bake cancelled");}
 void work(){if(++r.sample_operations>o.max_sample_operations)throw std::length_error("Source UV sampling budget exceeded");if((r.sample_operations&4095)==0)cancel();}
};
uint8_t byte(float x){return uint8_t(std::lround(std::clamp(x,0.f,1.f)*255));}
std::array<float,2> uv(const Mesh&m,const std::vector<std::array<float,2>> *values,const Hit&h){std::array<float,2> q{};auto &v=values?*values:m.uv;for(int k=0;k<3;++k){auto p=v.at(m.indices.at(size_t(h.face)*3+k));q[0]+=p[0]*h.bary[k];q[1]+=p[1]*h.bary[k];}return q;}
Vec3 normal(const Mesh&m,const Hit&h){Vec3 n;for(int k=0;k<3;++k)n+=m.normals[m.indices[size_t(h.face)*3+k]]*h.bary[k];if(length(n)<1e-10f)throw std::runtime_error("Source UV bake interpolates a zero normal");return normalized(n);}
Raster raster(const Mesh&m,uint32_t size,bool source,bool pad,Budget&b){
 if(size_t(size)*size>(b.o.max_working_bytes/(sizeof(Hit)+sizeof(int32_t)*2)))throw std::length_error("Source UV raster working memory budget exceeded");
 Raster r;r.size=size;r.hits.resize(size_t(size)*size);
 for(size_t f=0;f<m.indices.size()/3;++f){b.cancel();auto a=m.uv[m.indices[f*3]],c=m.uv[m.indices[f*3+1]],d=m.uv[m.indices[f*3+2]];double den=(double(c[1])-d[1])*(a[0]-d[0])+(double(d[0])-c[0])*(a[1]-d[1]);
  if(den==0){if(source)continue;throw std::invalid_argument("Destination UV bake requires nondegenerate UV faces");}
  int x0=std::max(0,int(std::floor(std::min({a[0],c[0],d[0]})*size-.5))),x1=std::min(int(size)-1,int(std::ceil(std::max({a[0],c[0],d[0]})*size-.5)));
  int y0=std::max(0,int(std::floor(std::min({a[1],c[1],d[1]})*size-.5))),y1=std::min(int(size)-1,int(std::ceil(std::max({a[1],c[1],d[1]})*size-.5)));
  for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){if(++b.r.raster_samples>b.o.max_raster_samples)throw std::length_error("Source UV raster work budget exceeded");if((b.r.raster_samples&4095)==0)b.cancel();float u=(x+.5f)/size,v=(y+.5f)/size;float w0=float(((c[1]-d[1])*(u-d[0])+(d[0]-c[0])*(v-d[1]))/den),w1=float(((d[1]-a[1])*(u-d[0])+(a[0]-d[0])*(v-d[1]))/den),w2=1-w0-w1;if(std::min({w0,w1,w2})<-.000001f)continue;Hit next{uint32_t(f),{w0,w1,w2}};auto &old=r.hits[size_t(y)*size+x];
   if(!source&&old.face!=UINT32_MAX){auto p=[&](const Hit&h){Vec3 v;for(int k=0;k<3;++k)v+=m.positions[m.indices[size_t(h.face)*3+k]]*h.bary[k];return v;};if(length(p(old)-p(next))>1e-4f)throw std::invalid_argument("Destination UV bake overlaps independent surfaces");}old=next;
  }
 }
 if(!source&&std::none_of(r.hits.begin(),r.hits.end(),[](auto &h){return h.face!=UINT32_MAX;}))throw std::invalid_argument("Destination UV bake has no covered texels");
 if(pad){r.padding.assign(r.hits.size(),-1);for(size_t i=0;i<r.hits.size();++i)if(r.hits[i].face!=UINT32_MAX)r.padding[i]=int32_t(i);
  uint32_t passes=source?4:b.o.padding;for(uint32_t pass=0;pass<passes;++pass){b.cancel();auto next=r.padding;for(uint32_t y=0;y<size;++y)for(uint32_t x=0;x<size;++x){size_t at=size_t(y)*size+x;b.work();if(r.padding[at]>=0)continue;for(auto [dx,dy]:{std::pair{-1,0},{1,0},{0,-1},{0,1}}){int xx=int(x)+dx,yy=int(y)+dy;if(xx>=0&&yy>=0&&xx<int(size)&&yy<int(size)&&r.padding[size_t(yy)*size+xx]>=0){next[at]=r.padding[size_t(yy)*size+xx];break;}}}r.padding=std::move(next);}
 }
 return r;
}
struct Frame {Vec3 t,b,n;};
std::optional<Frame> frame(const Mesh&m,const Hit&h,const std::array<std::array<float,2>,3>&q){
 auto a=m.indices[size_t(h.face)*3],b=m.indices[size_t(h.face)*3+1],c=m.indices[size_t(h.face)*3+2];auto e1=m.positions[b]-m.positions[a],e2=m.positions[c]-m.positions[a];double u1=double(q[1][0])-q[0][0],v1=double(q[1][1])-q[0][1],u2=double(q[2][0])-q[0][0],v2=double(q[2][1])-q[0][1],det=u1*v2-u2*v1,scale=std::hypot(u1,v1)*std::hypot(u2,v2);if(!scale||std::abs(det)<=scale*1e-12)return {};auto N=normal(m,h);Vec3 T=e1*float(v2/det)-e2*float(v1/det),B=e2*float(u1/det)-e1*float(u2/det);T=T-N*dot(N,T);if(length(T)<1e-10f)return {};T=normalized(T);return Frame{T,cross(N,T)*(dot(cross(N,T),B)<0?-1.f:1.f),N};
}
std::array<float,2> affine(std::array<float,2>q,const std::array<float,6>&a){return {a[0]*q[0]+a[1]*q[1]+a[4],a[2]*q[0]+a[3]*q[1]+a[5]};}
int coordinate(const MToonTexture&t){int n=t.transform_texcoord>=0?t.transform_texcoord:int(t.texcoord);if(n<0||n>1)throw std::invalid_argument("Source UV bake requires explicit UV0 or UV1");return n;}
std::optional<MToonMaterial> typed(const CreatorAppearance*a){
 if(!a)return {};
 auto parse=[](const std::string&s){if(s.size()>2*1024*1024)throw std::length_error("Source UV material metadata exceeds budget");size_t count=0;return nlohmann::json::parse(s,[&](int depth,nlohmann::json::parse_event_t,const nlohmann::json&){if(depth>32||++count>100000)throw std::length_error("Source UV material metadata exceeds budget");return true;});};
 if(!a->asset||!a->decoded_images||a->decoded_images->size()>256||a->decoded_images->size()!=a->asset->images.size()||a->material<0||size_t(a->material)>=a->asset->materials.size())throw std::invalid_argument("Source UV appearance payload is incomplete");
 if(!a->mtoon_json.empty()){auto j=parse(a->mtoon_json);if(j.value("shader",std::string{})=="VRM/MToon")return MToonMaterial::from_vrm0(j,a->asset->textures.size());}
 if(!a->material_json.empty()){auto j=parse(a->material_json);if(j.contains("extensions")&&j["extensions"].contains("VRMC_materials_mtoon"))return MToonMaterial::from_vrm1(j,a->asset->textures.size());}return {};
}
struct Plan {
 PaintSourceTexture view;bool srgb=false,normal=false,check_backface=false;int coord=0,role=-1;
 std::array<float,6> transform{1,0,0,1,0,0};bool animated=false;
};
bool same(const Plan&a,const Plan&b){return a.view.image==b.view.image&&a.srgb==b.srgb&&a.normal==b.normal&&a.check_backface==b.check_backface&&a.coord==b.coord&&a.transform==b.transform&&!a.animated&&!b.animated&&a.view.wrap_s==b.view.wrap_s&&a.view.wrap_t==b.view.wrap_t&&a.view.min_filter==b.view.min_filter&&a.view.mag_filter==b.view.mag_filter;}
void validate(const SourceUvBakeInput&i,const SourceUvBakeOptions&o){
 if(o.cancelled&&o.cancelled())throw std::runtime_error("Source UV bake cancelled");
 if(i.paint&&i.paint->normal_projection()){
  if(!i.current_material||!i.current_material->normal_preparation)throw std::invalid_argument("Prepared normal UV source requires its own material interpretation");
  i.current_material->validate();const auto &p=*i.current_material->normal_preparation;p.validate_current(i.paint->mesh(),i.appearance.get(),o.cancelled);
  if(!detail::normal_projection_identity(*i.paint->normal_projection(),*p.projection))throw std::invalid_argument("Prepared normal UV source differs from its owning intrinsic field");
 }
 if(!std::isfinite(o.max_normal_sample_error)||o.max_normal_sample_error<0||o.max_normal_sample_error>.25)throw std::invalid_argument("Invalid normal UV sample quality bound");
 if(o.cancelled&&o.cancelled())throw std::runtime_error("Source UV bake cancelled");
 if(!i.paint||i.paint->in_stroke())throw std::invalid_argument("Finish source paint stroke before UV baking");
 if(bool(i.paint->normal_projection())!=bool(i.current_material&&i.current_material->normal_preparation))throw std::invalid_argument("Source UV normal paint and its own material interpretation differ");
 if(i.polygon_authority){
  if(!i.polygon_authority->active||!i.polygon_authority->colors||i.appearance||i.current_material||i.paint->color_projection()||i.paint->normal_projection()||!i.native_material)
   throw std::invalid_argument("Raw corner UV transfer requires its own active field and native material; prepared interpretations need their explicit adapter");
  PolygonLimits limits;limits.cancelled=o.cancelled;
  validate_polygon_derivative(*i.polygon_authority,i.paint.get(),true,limits);
  i.polygon_authority->colors->validate(i.polygon_authority->topology,limits);
 }
 if(i.paint->surface_diagnostics().policy==PaintUvPolicy::PolygonAuthored&&!i.polygon_authority)throw std::invalid_argument("Source UV baking requires explicit polygon source conversion with its owning authority; original paint was retained");
 const auto preparation=i.current_material?i.current_material->color_preparation:nullptr;
 if(preparation&&preparation->polygon_colors)throw std::invalid_argument("Current corner color UV changes require their actual field remap; original geometry, UVs and paint were retained");
 if(bool(i.paint->color_projection())!=bool(preparation))throw std::invalid_argument("Source UV color preparation authority differs from current paint");
 if(preparation){preparation->validate();preparation->validate_current(i.paint->mesh(),i.appearance.get(),o.cancelled);const auto &a=*i.paint->color_projection(),&b=*preparation->projection;if(a.vertex_rgba!=b.vertex_rgba||a.maximum_error!=b.maximum_error||a.quality_cells!=b.quality_cells)throw std::invalid_argument("Source UV projected field differs from retained preparation");}
 if(!o.max_vertices||o.max_vertices>2'000'000||!o.max_triangles||o.max_triangles>2'000'000||i.target.positions.size()>o.max_vertices||i.target.indices.size()/3>o.max_triangles||i.paint->mesh().positions.size()>o.max_vertices||i.paint->mesh().indices.size()/3>o.max_triangles)throw std::length_error("Source UV geometry budget exceeded");
 i.target.validate();auto &old=i.paint->mesh();old.validate();
 if(i.target.positions.size()>2'000'000||i.target.indices.size()/3>2'000'000||i.source_vertices.size()!=i.target.positions.size()||i.target.indices.size()!=old.indices.size()||i.target.uv.size()!=i.target.positions.size()||old.uv.size()!=old.positions.size())throw std::invalid_argument("Source UV bake requires exact ordered triangle identity");
 for(auto q:i.target.uv)if(q[0]<0||q[0]>1||q[1]<0||q[1]>1)throw std::invalid_argument("Destination UVs must fit the unit atlas");
 for(size_t n=0;n<i.source_vertices.size();++n){if((n&4095)==0&&o.cancelled&&o.cancelled())throw std::runtime_error("Source UV bake cancelled");auto v=i.source_vertices[n];if(v>=old.positions.size())throw std::invalid_argument("Source UV vertex remap is out of bounds");auto p=i.target.positions[n]-old.positions[v],q=i.target.normals[n]-old.normals[v];if(dot(p,p)!=0||dot(q,q)!=0)throw std::invalid_argument("Source UV bake cannot change geometry or normals");}
 for(size_t n=0;n<i.target.indices.size();++n){if((n&4095)==0&&o.cancelled&&o.cancelled())throw std::runtime_error("Source UV bake cancelled");if(i.source_vertices[i.target.indices[n]]!=old.indices[n])throw std::invalid_argument("Source UV corner order/remap differs");}
 if((o.resolution&&(o.resolution<64||o.resolution>4096))||o.padding>64||size_t(o.animation)>1||!std::isfinite(o.freeze_seconds)||o.freeze_seconds<0||o.freeze_seconds>1e9||!o.max_image_pixels||o.max_image_pixels>16*1024*1024||!o.max_output_bytes||o.max_output_bytes>1024ull*1024*1024||!o.max_layer_bytes||o.max_layer_bytes>128*1024*1024||!o.max_working_bytes||o.max_working_bytes>512*1024*1024||!o.max_raster_samples||o.max_raster_samples>1024ull*1024*1024||!o.max_sample_operations||o.max_sample_operations>1024ull*1024*1024)throw std::invalid_argument("Invalid source UV bake bounds");
 if(i.paint->source_maps())i.paint->source_maps()->validate_surface(old);
}
} // namespace
SourceUvBakeResult bake_source_uv(const SourceUvBakeInput&i,const SourceUvBakeOptions&o){
 validate(i,o);SourceUvBakeResult result;Budget budget{o,result};budget.cancel();
 result.target=i.target;result.source_vertices=i.source_vertices;
 const bool prepared=bool(i.paint->color_projection()),normal_prepared=bool(i.paint->normal_projection());
 result.animation_policy=o.animation;result.freeze_seconds=o.freeze_seconds;
 result.original_paint=i.paint;result.original_appearance=i.appearance;result.original_deformation=i.current_deformation;result.original_material=i.current_material;result.active_layer=i.paint->active_layer();result.original_polygon=i.polygon_authority;
 if(i.current_material) {
  i.current_material->validate();
  if(i.current_material->normal_frames) {
   detail::normal_frame_current_authority(*i.current_material,i.paint->mesh(),i.current_deformation.get(),o.cancelled);
   result.normal_frames=i.current_material->normal_frames;
  }
  if(i.current_material->has_mtoon)result.mtoon=i.current_material->material;
  result.native_reference=i.current_material->native_reference;
 } else {
  result.mtoon=typed(i.appearance.get());
  if(i.native_material) {
   i.native_material->validate();result.native_reference=*i.native_material;
  } else if(i.appearance) {
   const auto &m=i.appearance->asset->materials.at(size_t(i.appearance->material));
   result.native_reference={m.color,m.emissive,m.alpha==ImportedMaterial::AlphaMode::Opaque?1:m.alpha==ImportedMaterial::AlphaMode::Mask?2:3,m.alpha_cutoff,m.double_sided};
  }
 }
 const auto &old=i.paint->mesh();std::vector<std::array<float,2>> authored_uv1;
 const std::vector<std::array<float,2>> *uv1=i.paint->source_maps()?i.paint->source_maps()->uv1.get():nullptr;
 if(i.current_deformation)result.deformation=detail::normal_frame_remap_deformation(i.current_deformation,old,i.target,i.source_vertices,o.cancelled);
 if(i.appearance){auto appearance=std::make_shared<CreatorAppearance>(*i.appearance);appearance->source_vertices.clear();appearance->source_vertices.reserve(i.source_vertices.size());for(auto v:i.source_vertices)appearance->source_vertices.push_back(i.appearance->source_vertices.at(v));result.appearance=std::move(appearance);}
 auto old_bake=i.paint->baked();if(old_bake && old_bake->normal_interpretation!=BakedNormalInterpretation::PreparedAttributes && !old_bake->maps.normal.rgba.empty() && result.normal_frames && (!result.normal_frames->canonical || result.normal_frames->canonical->tangents.empty()))throw std::invalid_argument("Raw baked normal transfer requires retained canonical tangent authority");if(old_bake){old_bake->validate(i.paint->resolution());result.baked_bytes=old_bake->storage_bytes();if(result.baked_bytes>o.max_layer_bytes)throw std::length_error("Source UV baked/editable storage budget exceeded");result.image_bytes=result.baked_bytes-old_bake->coverage.size();if(result.image_bytes>o.max_output_bytes)throw std::length_error("Source UV baked image budget exceeded");}
 if(prepared){const size_t pixels=size_t(i.paint->resolution())*i.paint->resolution();if(pixels>o.max_image_pixels)throw std::length_error("Source UV color projection pixel budget exceeded");const size_t bytes=pixels*4;if(bytes>o.max_layer_bytes-result.baked_bytes||bytes>o.max_output_bytes-result.image_bytes)throw std::length_error("Source UV color projection storage budget exceeded");result.layer_bytes=bytes;result.image_bytes+=bytes;}
 if(i.current_deformation && (i.current_deformation->skin || i.current_deformation->morphs) && !result.normal_frames &&
    ((i.paint->source_maps() && i.paint->source_maps()->normal.image) || (old_bake && !old_bake->maps.normal.rgba.empty()) ||
     (result.mtoon && result.mtoon->parameters.normal_enabled && result.mtoon->textures[size_t(MToonSlot::Normal)].index>=0)))
  throw std::invalid_argument("Deformed normal transfer requires retained original attribute frames; derivative conversion is rest-only");
 bool animated=result.mtoon&&(result.mtoon->parameters.scroll!=std::array<float,2>{}||result.mtoon->parameters.rotation_speed!=0);
 if(animated&&o.animation==SourceUvAnimationPolicy::RejectAnimated)throw std::invalid_argument("Animated source UVs require explicit FreezeAtTime bake policy");
 bool sampling_animation=animated&&o.animation==SourceUvAnimationPolicy::FreezeAtTime;
 result.animation_frozen=sampling_animation;
 if(!animated&&i.current_material&&i.current_material->animation_frozen){if(o.animation==SourceUvAnimationPolicy::FreezeAtTime&&o.freeze_seconds!=i.current_material->freeze_seconds)throw std::invalid_argument("Restore the original animated material before choosing a different freeze time");result.animation_frozen=true;result.animation_policy=i.current_material->animation_policy;result.freeze_seconds=i.current_material->freeze_seconds;}
 if(result.animation_frozen)result.diagnostics.push_back("UV animation explicitly frozen at "+std::to_string(result.freeze_seconds)+" seconds; original animated source retained");
 auto original_role=[&](int role){PaintSourceTexture view;auto &t=result.mtoon->textures[size_t(role)];if(t.index<0)return view;if(role!=int(MToonSlot::Matcap))coordinate(t);if(i.current_material)return i.current_material->roles[size_t(role)];auto &a=*i.appearance;auto &texture=a.asset->textures.at(size_t(t.index));if(texture.image<0||size_t(texture.image)>=a.decoded_images->size())throw std::invalid_argument("Source UV MToon image reference is invalid");view.image=a.decoded_images->at(texture.image);view.wrap_s=texture.wrap_s;view.wrap_t=texture.wrap_t;view.min_filter=texture.min_filter;view.mag_filter=texture.mag_filter;view.validate();return view;};
 if(i.current_material&&i.current_material->uv_origin)result.uv_origin=i.current_material->uv_origin;
 else if(!i.polygon_authority) {
  auto origin=std::make_shared<SourceUvOrigin>();origin->paint=i.paint->snapshot();
  if(i.appearance)origin->appearance=std::make_shared<const CreatorAppearance>(*i.appearance);
  origin->material=result.mtoon;
  if(result.mtoon)for(size_t role=0;role<size_t(MToonSlot::Count);++role)origin->roles[role]=original_role(int(role));
  result.uv_origin=std::move(origin);
 }
 if(result.mtoon){bool needs=false;for(size_t role=0;role<size_t(MToonSlot::Count);++role){auto &t=result.mtoon->textures[role];if(t.index>=0&&role!=size_t(MToonSlot::Matcap))needs|=coordinate(t)==1;}if(needs){if(!i.appearance||!i.appearance->asset)throw std::invalid_argument("Resolved UV1 requires retained original primitive provenance");const ImportedPrimitive*p=nullptr;for(auto &v:i.appearance->asset->primitives)if(v.source_mesh==i.appearance->source_mesh&&v.source_primitive==i.appearance->source_primitive){if(p)throw std::invalid_argument("Source UV primitive identity is ambiguous");p=&v;}if(!p||p->uv1.size()!=p->mesh.positions.size()||i.appearance->source_vertices.size()!=old.positions.size())throw std::invalid_argument("Source UV1 requires exact retained primitive/accessor remap");if(!uv1)authored_uv1.reserve(old.positions.size());size_t vertex=0;for(auto v:i.appearance->source_vertices){if(v>=p->uv1.size())throw std::invalid_argument("Source UV1 remap is invalid");auto q=p->uv1[v];if(!std::isfinite(q[0])||!std::isfinite(q[1]))throw std::invalid_argument("Source UV1 is not finite");if(uv1){if(uv1->at(vertex)!=q)throw std::invalid_argument("PBR and MToon UV1 source domains differ");}else authored_uv1.push_back(q);++vertex;}if(!uv1)uv1=&authored_uv1;}}
 auto mapped=[&](const Plan&p,const Hit&h){auto q=uv(old,p.coord?uv1:nullptr,h);if(!p.animated)return affine(q,p.transform);float mask=1;auto maskview=original_role(8);if(maskview.image){auto &mt=result.mtoon->textures[8];auto mu=uv(old,coordinate(mt)?uv1:nullptr,h);mu=mtoon_uv(*result.mtoon,MToonSlot::UvMask,mu,0,1);mask=sample_source_texture(maskview,mu[0],mu[1],mt.encoding==MToonEncoding::Srgb)[mt.channel];budget.work();}return mtoon_uv(*result.mtoon,MToonSlot(p.role),q,o.freeze_seconds,mask);};
 std::vector<std::pair<Plan,std::shared_ptr<const Image>>> completed;std::optional<Raster> current;
 auto bake=[&](Plan p){
  if(!p.view.image)return p.view;
  if(p.coord==1&&!uv1)throw std::invalid_argument("Source UV1 bake requires its exact coordinate domain");
  p.view.validate(p.coord?2:1);p.view.image->validate();for(auto &[other,image]:completed)if(same(p,other)){auto out=p.view;out.image=image;out.texcoord=0;out.uv_transform={1,0,0,1,0,0};out.wrap_s=out.wrap_t=33071;return out;}
  uint32_t size=o.resolution?o.resolution:std::max({p.view.image->width,p.view.image->height,i.paint->resolution()});if(size_t(size)*size>o.max_image_pixels)throw std::length_error("Source UV output image pixel budget exceeded");size_t bytes=size_t(size)*size*4;if(bytes>o.max_output_bytes-result.image_bytes)throw std::length_error("Source UV output image budget exceeded");
  if(!current||current->size!=size){current.reset();current=raster(i.target,size,false,true,budget);}
  auto image=std::make_shared<Image>();image->width=image->height=size;image->rgba.assign(bytes,0);
  for(uint32_t y=0;y<size;++y){budget.cancel();for(uint32_t x=0;x<size;++x){size_t at=size_t(y)*size+x;auto &h=current->hits[at];if(h.face==UINT32_MAX)continue;auto q=mapped(p,h);auto sample_view=p.view;sample_view.uv_transform={1,0,0,1,0,0};sample_view.texcoord=0;auto value=sample_source_texture(sample_view,q[0],q[1],p.srgb);budget.work();
   if(p.normal){std::array<std::array<float,2>,3> oq,nq;for(int k=0;k<3;++k){Hit corner{h.face,{}};corner.bary[k]=1;oq[k]=mapped(p,corner);nq[k]=i.target.uv[i.target.indices[size_t(h.face)*3+k]];}
    if(p.animated){constexpr float step=.001f;auto q0=mapped(p,h);Hit bx=h,by=h;bx.bary[0]-=step;bx.bary[1]+=step;by.bary[0]-=step;by.bary[2]+=step;auto qx=mapped(p,bx),qy=mapped(p,by);oq[0]=q0;oq[1]={q0[0]+(qx[0]-q0[0])/step,q0[1]+(qx[1]-q0[1])/step};oq[2]={q0[0]+(qy[0]-q0[0])/step,q0[1]+(qy[1]-q0[1])/step};}
    auto nf=frame(i.target,h,nq);if(!nf)throw std::runtime_error("Destination UV normal frame is degenerate");Vec3 world=nf->n;auto of=frame(old,h,oq);if(of){if(p.check_backface&&(std::abs(dot(of->t,nf->b))>1e-5f||std::abs(dot(of->b,nf->t))>1e-5f))throw std::invalid_argument("MToon1 backface normal frame rotates across UVs; preserve authored tangent transport before baking this material");Vec3 v{value[0]*2-1,value[1]*2-1,value[2]*2-1};if(length(v)>1e-8f)world=normalized(of->t*v.x+of->b*v.y+of->n*v.z);}Vec3 v{dot(world,nf->t),dot(world,nf->b),dot(world,nf->n)};value[0]=v.x*.5f+.5f;value[1]=v.y*.5f+.5f;value[2]=v.z*.5f+.5f;
   }
   for(int k=0;k<4;++k)image->rgba[at*4+k]=byte(p.srgb&&k<3?linear_to_srgb(value[k]):value[k]);
  }}
  for(size_t at=0;at<current->hits.size();++at)if(current->hits[at].face==UINT32_MAX&&current->padding[at]>=0){auto from=size_t(current->padding[at]);std::copy_n(image->rgba.begin()+from*4,4,image->rgba.begin()+at*4);++result.padded_texels;}
  result.image_bytes+=bytes;completed.push_back({p,image});auto out=p.view;out.image=std::move(image);out.texcoord=0;out.uv_transform={1,0,0,1,0,0};out.wrap_s=out.wrap_t=33071;return out;
 };
 auto maps=std::make_shared<PaintSourceMaps>();if(auto source=i.paint->source_maps()){
  *maps=*source;PaintSourceTexture *dest[]={&maps->color,&maps->metal_rough,&maps->emission,&maps->normal,&maps->occlusion};int role=0;for(auto slot:dest){Plan p;p.view=*slot;p.coord=slot->texcoord;p.srgb=role==0||role==2;p.normal=role==3 && !result.normal_frames;p.transform=slot->uv_transform;*slot=bake(p);++role;}
  maps->uv1.reset();
 }
 if(result.mtoon){auto original=*result.mtoon;for(size_t role=0;role<size_t(MToonSlot::Count);++role){Plan p;p.view=original_role(int(role));if(!p.view.image)continue;p.role=int(role);auto &t=original.textures[role];p.coord=role==size_t(MToonSlot::Matcap)?0:coordinate(t);p.srgb=t.encoding==MToonEncoding::Srgb;p.normal=role==size_t(MToonSlot::Normal) && !result.normal_frames;p.check_backface=p.normal&&original.version()==MToonVersion::V1&&original.parameters.normal_enabled&&original.parameters.normal_scale!=0&&(original.parameters.cull!=MToonCull::Back||(original.parameters.outline!=MToonOutline::None&&original.parameters.outline_width>0));
   if(role==size_t(MToonSlot::Matcap)){result.mtoon_roles[role]=p.view;result.view_dependent[role]=true;continue;}
   auto q0=mtoon_uv(original,MToonSlot(role),{0,0},0,1),qx=mtoon_uv(original,MToonSlot(role),{1,0},0,1),qy=mtoon_uv(original,MToonSlot(role),{0,1},0,1);p.transform={qx[0]-q0[0],qy[0]-q0[0],qx[1]-q0[1],qy[1]-q0[1],q0[0],q0[1]};p.animated=sampling_animation&&role!=size_t(MToonSlot::UvMask)&&!(original.version()==MToonVersion::V0&&role==size_t(MToonSlot::OutlineWidth));result.mtoon_roles[role]=bake(p);
  }
  for(size_t role=0;role<size_t(MToonSlot::Count);++role)if(!result.view_dependent[role]){auto &t=result.mtoon->textures[role];t.texcoord=0;t.transform_texcoord=-1;t.offset={};t.scale={1,1};t.rotation=0;}
  if(result.animation_frozen){result.mtoon->parameters.scroll={};result.mtoon->parameters.rotation_speed=0;}
 }
 maps->validate_surface(i.target);result.maps=maps;current.reset();size_t tangent_bytes=old_bake&&!old_bake->maps.normal.rgba.empty()&&!result.normal_frames?old.indices.size()*sizeof(Tangent)*2:0;if(tangent_bytes>o.max_working_bytes||size_t(i.paint->resolution())*i.paint->resolution()*(sizeof(Hit)*2+sizeof(int32_t)*2)>o.max_working_bytes-tangent_bytes)throw std::length_error("Source UV layer transfer working memory budget exceeded");auto dest=raster(i.target,i.paint->resolution(),false,false,budget);auto source=raster(old,i.paint->resolution(),true,true,budget);uint32_t r=i.paint->resolution(),cols=r/32;result.layers=i.paint->layers();for(auto &layer:result.layers){layer.tiles.assign(size_t(cols)*cols,{});layer.pbr_tiles.assign(prepared?size_t(cols)*cols:0,{});}
 std::shared_ptr<BakedTextures> transferred;std::vector<Tangent> old_frames,new_frames;
 if(old_bake){transferred=std::make_shared<BakedTextures>();auto blank=[&](bool normal=false){Image m{r,r,std::vector<uint8_t>(size_t(r)*r*4)};if(normal)for(size_t n=0;n<m.rgba.size();n+=4){m.rgba[n]=m.rgba[n+1]=128;m.rgba[n+2]=m.rgba[n+3]=255;}return m;};transferred->maps.color=blank();transferred->maps.metal_rough=blank();transferred->maps.emission=blank();transferred->color_interpretation=old_bake->color_interpretation;transferred->normal_interpretation=old_bake->normal_interpretation;if(prepared)transferred->maps.pbr_color=blank();transferred->coverage.assign(size_t(r)*r,0);if(!old_bake->maps.normal.rgba.empty()){transferred->maps.normal=blank(true);if(normal_prepared)transferred->maps.mtoon_normal=blank(true);if(!result.normal_frames){old_frames=tangent_corners(old,o.cancelled);new_frames=tangent_corners(i.target,o.cancelled);}}}
 std::vector<std::vector<std::shared_ptr<PaintTile>>> tiles(result.layers.size(),std::vector<std::shared_ptr<PaintTile>>(size_t(cols)*cols));
 std::vector<std::vector<std::shared_ptr<PaintColorTile>>> color_tiles(prepared?result.layers.size():0,std::vector<std::shared_ptr<PaintColorTile>>(prepared?size_t(cols)*cols:0));
 for(uint32_t y=0;y<r;++y){budget.cancel();for(uint32_t x=0;x<r;++x){auto &h=dest.hits[size_t(y)*r+x];if(h.face==UINT32_MAX)continue;auto q=uv(old,nullptr,h);float px=q[0]*r-.5f,py=q[1]*r-.5f;int bx=int(std::floor(px)),by=int(std::floor(py));float fx=px-bx,fy=py-by;std::array<size_t,4> owners;std::array<float,4>w;for(int k=0;k<4;++k){int sx=std::clamp(bx+(k&1),0,int(r)-1),sy=std::clamp(by+(k>>1),0,int(r)-1);size_t at=size_t(sy)*r+sx;owners[k]=source.padding[at]>=0?size_t(source.padding[at]):at;w[k]=((k&1)?fx:1-fx)*((k>>1)?fy:1-fy);}
   if(transferred){budget.work();std::array<float,20> values{};float coverage=0;for(int k=0;k<4;++k){float a=old_bake->coverage[owners[k]]/255.f*w[k];if(a==0)continue;coverage+=a;size_t pos=owners[k]*4;for(int c=0;c<4;++c){float v=old_bake->maps.color.rgba[pos+c]/255.f;values[c]+=(c<3?srgb_to_linear(v):v)*a;if(prepared){float p=old_bake->maps.pbr_color.rgba[pos+c]/255.f;values[13+c]+=(c<3?srgb_to_linear(p):p)*a;}}for(int c=0;c<3;++c){values[4+c]+=old_bake->maps.metal_rough.rgba[pos+c]/255.f*a;values[7+c]+=srgb_to_linear(old_bake->maps.emission.rgba[pos+c]/255.f)*a;if(!old_bake->maps.normal.rgba.empty()){values[10+c]+=(old_bake->maps.normal.rgba[pos+c]/255.f*2-1)*a;if(normal_prepared)values[17+c]+=(old_bake->maps.mtoon_normal.rgba[pos+c]/255.f*2-1)*a;}}}
    if(coverage>1e-8f){for(auto &v:values)v/=coverage;if(!old_frames.empty()){auto of=tangent_frame(old,old_frames,h.face,h.bary),nf=tangent_frame(i.target,new_frames,h.face,h.bary);auto n=nf.to_tangent(of.to_world({values[10],values[11],values[12]}));values[10]=n.x;values[11]=n.y;values[12]=n.z;}size_t at=size_t(y)*r+x;transferred->coverage[at]=byte(coverage);for(int c=0;c<4;++c){transferred->maps.color.rgba[at*4+c]=byte(c<3?linear_to_srgb(values[c]):values[c]);if(prepared)transferred->maps.pbr_color.rgba[at*4+c]=byte(c<3?linear_to_srgb(values[13+c]):values[13+c]);}for(int c=0;c<3;++c){transferred->maps.metal_rough.rgba[at*4+c]=byte(values[4+c]);transferred->maps.emission.rgba[at*4+c]=byte(linear_to_srgb(values[7+c]));if(!old_bake->maps.normal.rgba.empty()){transferred->maps.normal.rgba[at*4+c]=byte(values[10+c]*.5f+.5f);if(normal_prepared)transferred->maps.mtoon_normal.rgba[at*4+c]=byte(values[17+c]*.5f+.5f);}}transferred->maps.metal_rough.rgba[at*4+3]=transferred->maps.emission.rgba[at*4+3]=255;}}
   for(size_t l=0;l<result.layers.size();++l){budget.work();std::array<PaintPixel,4> samples;for(int k=0;k<4;++k)samples[k]=i.paint->pixel(result.layers[l].id,uint32_t(owners[k]%r),uint32_t(owners[k]/r));PaintPixel value;for(int group=0;group<4;++group){int first=group==0?0:group==1?4:group==2?5:6,count=group==0?4:group==3?3:1;float coverage=0;for(int k=0;k<4;++k)coverage+=w[k]*samples[k].data[9+group]/255.f;value.data[9+group]=byte(coverage);if(coverage>1e-8f)for(int c=0;c<count;++c){bool srgb=(group==0&&c<3)||group==3;float sum=0;for(int k=0;k<4;++k){float v=samples[k].data[first+c]/255.f;if(srgb)v=srgb_to_linear(v);sum+=w[k]*samples[k].data[9+group]/255.f*v;}sum/=coverage;value.data[first+c]=byte(srgb?linear_to_srgb(sum):sum);}}
    float mask=0;for(int k=0;k<4;++k)mask+=w[k]*samples[k].data[13]/255.f;value.data[13]=byte(mask);if(value==PaintPixel{})continue;size_t at=size_t(y/32)*cols+x/32;if(!tiles[l][at]){if(sizeof(PaintTile)>o.max_layer_bytes-result.baked_bytes-result.layer_bytes)throw std::length_error("Source UV paint tile budget exceeded");result.layer_bytes+=sizeof(PaintTile);tiles[l][at]=std::make_shared<PaintTile>();result.layers[l].tiles[at]=tiles[l][at];}tiles[l][at]->pixels[size_t(y%32)*32+x%32]=value;
    if(prepared&&value.data[9]){budget.work();std::array<uint8_t,4> output{};float coverage=0;for(int k=0;k<4;++k)coverage+=w[k]*samples[k].data[9]/255.f;
     for(int c=0;c<4;++c){float sum=0;for(int k=0;k<4;++k){const auto p=i.paint->pbr_pixel(result.layers[l].id,uint32_t(owners[k]%r),uint32_t(owners[k]/r));float v=p[c]/255.f;if(c<3)v=srgb_to_linear(v);sum+=w[k]*samples[k].data[9]/255.f*v;}output[c]=byte(c<3?linear_to_srgb(sum/coverage):sum/coverage);}
     if(!color_tiles[l][at]){if(sizeof(PaintColorTile)>o.max_layer_bytes-result.baked_bytes-result.layer_bytes)throw std::length_error("Source UV PBR tile budget exceeded");result.layer_bytes+=sizeof(PaintColorTile);color_tiles[l][at]=std::make_shared<PaintColorTile>();result.layers[l].pbr_tiles[at]=color_tiles[l][at];}color_tiles[l][at]->pixels[size_t(y%32)*32+x%32]=output;
    }
   }
  }}
 if(transferred){transferred->validate(r);result.baked=std::move(transferred);}
 if(normal_prepared){
  // Layer transfer is complete. Do not keep its two scratch rasters alive
  // alongside the separately budgeted normal reprojection/replay workspace.
  dest=Raster{};source=Raster{};
  const size_t pixels=size_t(r)*r,bytes=pixels*8;if(pixels>o.max_image_pixels||bytes>o.max_output_bytes-result.image_bytes||bytes>o.max_layer_bytes-result.baked_bytes-result.layer_bytes)throw std::length_error("Intrinsic normal UV planes exceed output/storage budgets");
  auto layout=std::make_shared<SourceNormalUvLayout>();layout->source_vertices=i.source_vertices;layout->indices=i.target.indices;layout->uv=i.target.uv;layout->pins=i.target.uv_pins;layout->seams=i.target.uv_seams;const size_t mesh_bytes=layout->uv.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>))+layout->indices.size()*4+layout->pins.size()+layout->seams.size();if(mesh_bytes>=o.max_working_bytes)throw std::length_error("Normal UV replay geometry exceeds working budget");
  NormalPreparationOptions options;options.max_angular_error=o.max_normal_sample_error;options.max_working_bytes=o.max_working_bytes-mesh_bytes;options.max_samples=std::min<uint64_t>({64'000'000,o.max_raster_samples-result.raster_samples,(o.max_sample_operations-result.sample_operations)/12});options.cancelled=o.cancelled;if(!options.max_samples)throw std::length_error("Normal UV reprojection lacks raster/sampling budget");
  auto projected=SourceNormalPreparationAccess::reproject(*i.paint,i.target,i.source_vertices,options);result.raster_samples+=size_t(projected.raster_samples);result.sample_operations+=8*pixels+2*size_t(projected.sampled_texels)+2*size_t(projected.source_constraints);if(result.raster_samples>o.max_raster_samples||result.sample_operations>o.max_sample_operations)throw std::length_error("Normal UV reprojection exceeds cumulative work budget");
  auto uv=std::make_shared<SourceNormalUvReprojection>();uv->layout=std::move(layout);uv->projection=projected.projection;uv->before_paint=i.paint;uv->before_material=i.current_material;uv->before_appearance=i.appearance;uv->before_deformation=i.current_deformation;uv->receipt.resolution=r;uv->receipt.accepted_sample_error=options.max_angular_error;uv->receipt.observed_sample_error=projected.observed_sample_error;uv->receipt.sampled_texels=projected.sampled_texels;uv->receipt.source_constraints=projected.source_constraints;uv->receipt.raster_samples=projected.raster_samples;uv->receipt.raster_budget=options.max_samples;uv->receipt.max_working_bytes=o.max_working_bytes;uv->validate(true,o.cancelled);result.normal_reprojection=std::move(uv);result.image_bytes+=bytes;result.layer_bytes+=bytes;
  if(result.mtoon){auto &normal=result.mtoon_roles[size_t(MToonSlot::Normal)];normal={};normal.image=result.normal_reprojection->projection->mtoon;normal.wrap_s=normal.wrap_t=33071;normal.min_filter=normal.mag_filter=9729;}
  result.diagnostics.push_back("Intrinsic PBR/MToon and prepared detail normals retain fixed original attribute axes; source-aware sampled-quality UV receipt retained");
 }
 budget.cancel();result.diagnostics.push_back("Original images/material IDs/raw avatar retained; derived RGBA8 textures resampled without material-factor flattening");result.diagnostics.push_back(result.normal_frames?"Raw source/canonical normal vectors retain original attribute frames through the sampling-atlas change":"Normal conversion matches current UV-derivative preview frames; original authored tangent4 remains retained");return result;
}
struct SourceUvBakeAccess {
 static std::shared_ptr<PaintedMesh> adopt(const SourceUvBakeResult &result,
                                         const std::function<bool()> &cancelled) {
  const auto &old=*result.original_paint;
  if(result.layers.size()!=old.layers_.size() || result.layers.empty() || result.layers.size()>16 ||
     result.active_layer!=old.active_ || old.revision_==UINT64_MAX)
    throw std::invalid_argument("Source UV adoption cannot replace layer identity or active selection");
  const size_t columns=old.resolution()/PaintTile::size, count=columns*columns;
  size_t tiles=0,color_tiles=0;const bool prepared=bool(old.color_projection_),normal_prepared=bool(old.normal_projection_);
  for(size_t layer=0;layer<result.layers.size();++layer) {
   const auto &a=result.layers[layer],&b=old.layers_[layer];
   if(a.id!=b.id || a.name!=b.name || a.visible!=b.visible || a.opacity!=b.opacity || a.tiles.size()!=count)
    throw std::invalid_argument("Source UV adoption differs from original editable layer metadata");
   tiles+=std::count_if(a.tiles.begin(),a.tiles.end(),[](const auto &tile){return bool(tile);});
   if(a.pbr_tiles.size()!=(prepared?count:0))throw std::invalid_argument("Source UV PBR layer grid differs from color preparation");
   color_tiles+=std::count_if(a.pbr_tiles.begin(),a.pbr_tiles.end(),[](const auto &tile){return bool(tile);});
  }
  if(bool(result.normal_reprojection)!=normal_prepared)throw std::invalid_argument("Source UV adoption cannot drop or invent intrinsic normal authority");
  if(normal_prepared){const auto &u=*result.normal_reprojection;if(u.before_paint!=result.original_paint||u.before_material!=result.original_material||u.before_deformation!=result.original_deformation||u.before_appearance!=result.original_appearance||!u.layout||u.layout->source_vertices!=result.source_vertices)throw std::invalid_argument("Normal UV adoption before-state differs from its detached source lease");u.validate(true,cancelled);u.validate_current(result.target,result.appearance.get(),cancelled);}
  if(bool(result.baked)!=bool(old.baked_) || (result.baked&&(result.baked->color_interpretation!=old.baked_->color_interpretation||result.baked->normal_interpretation!=old.baked_->normal_interpretation)))
   throw std::invalid_argument("Source UV adoption cannot drop or invent a baked base");
  size_t bytes=tiles*sizeof(PaintTile)+color_tiles*sizeof(PaintColorTile)+(prepared?size_t(old.resolution())*old.resolution()*4:0)+(normal_prepared?size_t(old.resolution())*old.resolution()*8:0);
  if(result.baked){result.baked->validate(old.resolution());bytes+=result.baked->storage_bytes();}
  if(bytes>128*1024*1024)throw std::length_error("Source UV adoption exceeds combined paint storage budget");
  if(!result.maps)throw std::invalid_argument("Source UV adoption lacks its generic source table");
  result.maps->validate();
  PaintSourceMaps empty;const auto &before=old.source_maps_?*old.source_maps_:empty;
  if(before.normal_scale!=result.maps->normal_scale || before.occlusion_strength!=result.maps->occlusion_strength)
   throw std::invalid_argument("Source UV adoption changed authored normal or AO strength");
  std::array<const PaintSourceTexture*,5> source{&before.color,&before.metal_rough,&before.emission,&before.normal,&before.occlusion};
  std::array<const PaintSourceTexture*,5> target{&result.maps->color,&result.maps->metal_rough,&result.maps->emission,&result.maps->normal,&result.maps->occlusion};
  for(size_t n=0;n<source.size();++n) {
   const auto &a=*source[n],&b=*target[n];
   if(bool(a.image)!=bool(b.image) || (b.image &&
      (b.texcoord!=0 || b.uv_transform!=std::array<float,6>{1,0,0,1,0,0} ||
       b.wrap_s!=33071 || b.wrap_t!=33071 || b.min_filter!=a.min_filter || b.mag_filter!=a.mag_filter)))
    throw std::invalid_argument("Source UV adoption changed slot presence or derived sampling contract");
  }
  PaintSurfaceOptions surface;surface.cancelled=cancelled;
  auto paint=std::shared_ptr<PaintedMesh>(new PaintedMesh(result.target,old.resolution(),surface));
  if(cancelled&&cancelled())throw std::runtime_error("Source UV adoption cancelled");
  paint->source_maps_=result.maps->empty()?nullptr:std::make_shared<const PaintSourceMaps>(*result.maps);
  paint->layers_=result.layers;paint->baked_=result.baked;if(normal_prepared)paint->normal_projection_=result.normal_reprojection->projection;
  paint->active_=old.active_;paint->next_=old.next_;paint->revision_=old.revision_+1;
  paint->allocated_tiles_=tiles;paint->allocated_color_tiles_=color_tiles;
  if(prepared){if(!result.original_material||!result.original_material->color_preparation)throw std::invalid_argument("Source UV lost color preparation authority");
   ColorPreparationOptions options;options.cancelled=cancelled;options.max_projection_error=result.original_material->color_preparation->receipt.accepted_projection_error;
   auto colors=result.original_material->color_preparation->vertex_field(paint->mesh().positions.size(),result.appearance.get(),cancelled);
   paint->color_projection_=SourceColorPreparationAccess::project(*paint,colors,options).projection;
  }
  return paint;
 }
};
SourceUvAdoption adopt_source_uv_bake(const SourceUvBakeResult &result,
                                     const std::function<bool()> &cancelled) {
 SourceUvBakeInput input;input.paint=result.original_paint;input.appearance=result.original_appearance;
 input.target=result.target;input.source_vertices=result.source_vertices;input.current_material=result.original_material;input.current_deformation=result.original_deformation;input.polygon_authority=result.original_polygon;if(input.polygon_authority)input.native_material=result.native_reference;
 SourceUvBakeOptions options;options.cancelled=cancelled;validate(input,options);
 if(result.original_polygon){
  if(result.uv_origin||result.mtoon||result.normal_frames||result.normal_reprojection||result.animation_frozen||result.original_material||result.original_appearance)
   throw std::invalid_argument("Raw corner UV result cannot invent a borrowed material/source interpretation");
  auto paint=SourceUvBakeAccess::adopt(result,cancelled);
  if(result.deformation)result.deformation->validate(paint->mesh());
  return {std::move(paint),{},result.deformation,{}};
 }
 if(!result.uv_origin)throw std::invalid_argument("Source UV adoption lacks first-source provenance");
 if(result.normal_reprojection&&(!result.original_material||!result.original_material->normal_preparation||!result.original_paint->normal_projection()))throw std::invalid_argument("Source UV result cannot invent an unbound normal interpretation");
 // Populate the immutable proof through the cancellation-aware path before
 // generic binding validation can replay it without a worker callback.
 if(result.normal_reprojection)result.normal_reprojection->validate(true,cancelled);
 if(result.normal_frames!=(result.original_material?result.original_material->normal_frames:nullptr))throw std::invalid_argument("Source UV adoption cannot discard or replace original frame authority");
 auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=result.mtoon.has_value();
 if(result.mtoon)binding->material=*result.mtoon;
 binding->roles=result.mtoon_roles;binding->uv_origin=result.uv_origin;binding->sampling_reparameterized=true;
 binding->native_reference=result.native_reference;
 if(result.normal_frames){binding->normal_frames=result.normal_frames;binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;}
 binding->animation_policy=result.animation_policy;binding->freeze_seconds=result.freeze_seconds;
 binding->animation_frozen=result.animation_frozen;
 if(result.normal_reprojection){auto normal=std::make_shared<SourceNormalPreparation>(*result.original_material->normal_preparation);normal->projection=result.normal_reprojection->projection;normal->uv_reprojection=result.normal_reprojection;binding->normal_preparation=std::move(normal);}
 binding->validate();
 auto paint=SourceUvBakeAccess::adopt(result,cancelled);
 if(paint->color_projection()){auto preparation=std::make_shared<SourceColorPreparation>(*result.original_material->color_preparation);preparation->projection=paint->color_projection();binding->color_preparation=std::move(preparation);binding->validate();}
 if(result.normal_frames)detail::normal_frame_current_authority(*binding,paint->mesh(),result.deformation.get(),cancelled);
 if(result.deformation)result.deformation->validate(paint->mesh());
 return {std::move(paint),std::move(binding),result.deformation,result.appearance};
}
} // namespace dome
