// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-field-source.hpp"
#include "texture-set-paint-access.hpp"
#include "polygon-surface-owner.hpp"
#include "polygon-surface-preview.hpp"
#include "baked-normal-field.hpp"
#include "dome/material-bindings.hpp"
#include <algorithm>
namespace dome::detail {namespace {
void stop(const TextureSetLimits&l){if(l.cancelled&&l.cancelled())throw std::runtime_error("Texture-set source-field preparation cancelled");}
template<class T>bool owner(const std::shared_ptr<const T>&a,const std::shared_ptr<const T>&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
PolygonSurfaceTransferLimits polygon_limits(const TextureSetLimits&l){PolygonSurfaceTransferLimits p;p.maximum_working_bytes=l.maximum_bytes;p.maximum_entries=std::min(l.maximum_work,size_t(8'000'000));p.geometry.cancelled=l.cancelled;return p;}
void source_model(const TextureSetView&v,const TextureSetLimits&l){
 if(!v.painted||v.painted->in_stroke()||!TextureSetPaintAccess::valid_raster(*v.painted))throw std::invalid_argument("Texture-set source needs its actual finished Paint/raster owner");
 v.material.validate();
 if(v.appearance||v.painted->source_maps()||v.painted->source_sampling_prepared()||v.painted->color_projection()||v.painted->normal_projection())throw std::invalid_argument("Texture-set original image/appearance fields need their complete per-role source adapter");
 if(v.material_bindings){
  const auto&b=v.material_bindings;if(b->polygon_surface_transfer&&b->polygon_surface_transfer->before()->texture_sets)throw std::invalid_argument("A retained multi-material before model needs its actual per-field polygon8 source adapter");if(!b->polygon_surface_transfer)throw std::invalid_argument("Texture-set prepared frame requires its complete field source adapter");b->validate();b->polygon_surface_transfer->validate_binding(*b,polygon_limits(l));b->polygon_surface_transfer->validate_current(*v.painted,polygon_limits(l));
 }else if(v.painted->paint_sampling_domain()||(v.painted->baked()&&(v.painted->baked()->normal_field||v.painted->baked()->normal_authority)))throw std::invalid_argument("Texture-set retained sampling/normal needs its actual bound material field");
}
}
size_t TextureSetFieldSource::working_bytes(const TextureSetView&source,const TextureSetLimits&limits){
 limits.validate();if(!source.painted)throw std::invalid_argument("Texture-set source has no actual Paint owner");
 size_t bytes=TextureSetPaintAccess::working_bytes(*source.painted,source.painted->resolution());const auto metadata=TextureSetPaintAccess::snapshot_bytes(*source.painted);
 if(bytes>limits.maximum_bytes||metadata>(limits.maximum_bytes-bytes)/3)throw std::length_error("Texture-set source/member/read metadata exceed simultaneous byte quota");bytes+=metadata*3;
 const size_t fixed=sizeof(TextureSetFieldSource)+sizeof(TextureSetView)+source.name.size()+source.material.name.size()+256;
 if(fixed>limits.maximum_bytes-bytes)throw std::length_error("Texture-set source descriptor exceeds its simultaneous byte quota");return bytes+fixed;
}
std::shared_ptr<const TextureSetFieldSource>TextureSetFieldSource::capture(const TextureSetView&source,std::shared_ptr<const TextureSetFaceDomain>domain,const TextureSetLimits&limits){
 return capture_prepared(source,std::move(domain),{},limits);
}
std::shared_ptr<const TextureSetFieldSource>TextureSetFieldSource::capture_prepared(const TextureSetView&source,
    std::shared_ptr<const TextureSetFaceDomain>domain,std::shared_ptr<const PolygonSurfacePreview>preview,
    const TextureSetLimits&limits,const PaintedMesh*raster_template,OriginalPaint original_paint){
 limits.validate();stop(limits);source_model(source,limits);if(domain)domain->authenticate_owner(domain);
 if(!domain||!domain->topology()||!domain->matches(source.painted->mesh_owner())||!owner(domain->topology()->geometry(),source.painted->mesh_owner()))throw std::invalid_argument("Texture-set source field borrowed another geometry/membership owner");
 if(source.material_bindings&&source.material_bindings->polygon_surface_transfer){
  const auto&topology=*domain->topology();const auto actual=source.material_bindings->polygon_surface_transfer->trace().current()->triangulate(polygon_limits(limits).geometry);
  if(!topology.polygon_ids()||actual.triangle_faces.size()!=topology.triangle_faces().size()||actual.mesh.indices.size()!=topology.triangle_corners().size()||!std::equal(actual.triangle_faces.begin(),actual.triangle_faces.end(),topology.triangle_faces().begin()))throw std::invalid_argument("Source field material faces differ from the actual owning polygon trace");
  for(size_t c=0;c<actual.mesh.indices.size();++c){if((c&1023)==0)stop(limits);if(actual.corners.at(actual.mesh.indices[c])!=topology.triangle_corners()[c])throw std::invalid_argument("Source field material corners borrowed another polygon authority");}
 }
 const auto&mesh=source.painted->mesh();if(mesh.indices.size()>limits.maximum_corners||mesh.indices.size()/3>limits.maximum_triangles||domain->triangles().size()>limits.maximum_work)throw std::length_error("Texture-set source membership exceeds its geometry/work quota");
 uint32_t previous=0;bool first=true;for(auto t:domain->triangles()){stop(limits);if(t>=mesh.indices.size()/3||(!first&&t<=previous))throw std::invalid_argument("Texture-set source membership is not its exact ordered triangle domain");previous=t;first=false;}
 const auto peak=working_bytes(source,limits);
 if(source.material_bindings&&source.material_bindings->polygon_surface_transfer){const auto&before=*source.material_bindings->polygon_surface_transfer->before();const auto replay=PolygonSurfaceOwner::source_working_bytes(before,polygon_limits(limits));const size_t copies=preview?1:2;if(replay>(limits.maximum_bytes-peak)/copies)throw std::length_error("Texture-set shared before inputs/replay plus member/read raster exceed their simultaneous byte quota");}
 auto result=std::shared_ptr<TextureSetFieldSource>(new TextureSetFieldSource);
 // The codec already holds the detached seed read from one wire record.
 // Members decoded from that record must keep the same immutable original.
 result->original_=original_paint==OriginalPaint::DecodedSeed?source.painted:source.painted->snapshot();
 result->field_=source;result->field_.source.reset();result->field_.id=domain->set();result->field_.domain=std::move(domain);
 auto member=raster_template?TextureSetPaintAccess::bind_raster(*result->original_,result->field_.domain,*raster_template,limits):TextureSetPaintAccess::bind(*result->original_,result->field_.domain,result->original_->resolution(),limits);result->field_.painted=member->snapshot();
 if(source.material_bindings)result->polygon_=source.material_bindings->polygon_surface_transfer;
 // Construct and validate the actual privileged read view before sealing the
 // descriptor. This exercises both source and new membership proofs now.
 const auto readable=TextureSetPaintAccess::field_source_view(*result->field_.painted,*result->original_,result->field_.domain,limits);result->readable_=readable;
 if(result->polygon_){
  result->polygon_->validate_current(*readable,polygon_limits(limits));
  if(preview){if(!preview->matches(source.material_bindings,readable->paint_sampling_attachment())||!owner(preview->field_owner(),result->polygon_))throw std::invalid_argument("Texture-set shared before inputs belong to a foreign field/attachment");result->preview_=std::move(preview);}
  else result->preview_=PolygonSurfacePreview::create(*readable,source.material_bindings,polygon_limits(limits));
 }
 stop(limits);std::shared_ptr<const TextureSetFieldSource>sealed(new const TextureSetFieldSource(std::move(*result)));sealed->self_=sealed;sealed->validate_field(sealed->field_,limits);return sealed;
}
bool TextureSetFieldSource::owns_read_paint(const PaintedMesh&value)const noexcept{return &value==original_.get()||&value==readable_.get();}
void TextureSetFieldSource::authenticate_owner(const std::shared_ptr<const TextureSetFieldSource>&value)const{const auto expected=self_.lock();if(!expected||!owner(expected,value))throw std::invalid_argument("Texture-set field source borrowed another control block");}
void TextureSetFieldSource::validate_member(const TextureSetView&value,const TextureSetLimits&limits,bool rebake)const{
 limits.validate();stop(limits);authenticate_owner(self_.lock());
 if(!value.painted||value.id!=field_.id||!owner(value.domain,field_.domain)||!owner(value.painted->mesh_owner(),field_.domain->topology()->geometry())||!owner(TextureSetPaintAccess::domain(*value.painted),field_.domain)||!owner(value.appearance,field_.appearance)||!owner(value.material_bindings,field_.material_bindings)||(!rebake&&!owner(value.bake_recipe,field_.bake_recipe))||!TextureSetPaintAccess::valid_raster(*value.painted)||value.painted->in_stroke())throw std::invalid_argument("Texture-set field changed actual set/geometry/source/material/recipe authority");
 value.material.validate();if(value.source&&!owner(value.source,self_.lock()))throw std::invalid_argument("Texture-set record changed its actual field source descriptor");
 if(!TextureSetPaintAccess::valid_source_member(*value.painted,*original_,field_.domain,rebake?value.painted.get():field_.painted.get()))throw std::invalid_argument("Texture-set current field changed original geometry/raster/source authority");
}
void TextureSetFieldSource::validate_field(const TextureSetView&value,const TextureSetLimits&limits,bool deep)const{
 validate_member(value,limits);if(!TextureSetPaintAccess::same_current(*value.painted,*field_.painted))throw std::invalid_argument("Texture-set field source has stale current pixel/layer values");
 if(deep&&polygon_)polygon_->validate_current(*readable_,polygon_limits(limits));
}
std::shared_ptr<const PaintedMesh>TextureSetFieldSource::read_view(const TextureSetView&value,const TextureSetLimits&limits)const{validate_field(value,limits,false);return readable_;}
std::shared_ptr<const TextureSetFieldSource>TextureSetFieldSource::with_current(const TextureSetView&value,const TextureSetLimits&limits)const{
 if(value.painted&&value.painted->resolution()!=field_.painted->resolution())
  return with_material_grid(value,limits);
 return with_member(value,limits,false,preview_?preview_->native_bake():nullptr);
}
std::shared_ptr<const TextureSetFieldSource> TextureSetFieldSource::with_material_grid(
    const TextureSetView &value,const TextureSetLimits &limits)const{
 const auto before=field_.painted->baked();
 const auto after=value.painted?value.painted->baked():nullptr;
 if(!preview_||preview_->model()!=PolygonSurfaceNormalModel::PreparedCorePbr||
    !before||!before->normal_field||!after||!owner(before->normal_field,after->normal_field)||
    !owner(value.bake_recipe,field_.bake_recipe))
  throw std::invalid_argument("Resized material grid lost its independent normal or Bake recipe");
 after->validate(value.painted->resolution(),true);
 return with_member(value,limits,true,{});
}
std::shared_ptr<const TextureSetFieldSource> TextureSetFieldSource::with_cleared_bake(
    const TextureSetView &value,const TextureSetLimits &limits)const{
 const auto original=original_->baked();
 const auto restored=value.painted?value.painted->baked():nullptr;
 const bool same_grid=value.painted&&value.painted->resolution()==original_->resolution();
 if((same_grid&&!owner(original,restored))||
    (!same_grid&&(!original||!original->normal_field||!restored||
                  !owner(original->normal_field,restored->normal_field)))||
    !owner(value.bake_recipe,field_.bake_recipe))
  throw std::invalid_argument("Clear Bake lost its inherited material grid or normal field");
 return with_member(value,limits,true,{});
}
std::shared_ptr<const TextureSetFieldSource>TextureSetFieldSource::with_bake(
    const TextureSetView&value,const TextureSetLimits&limits,std::shared_ptr<const Image> native)const{
 if(!polygon_||!value.painted||!value.bake_recipe)
  throw std::invalid_argument("Retained texture-set Bake requires its actual field and recipe");
 const auto baked=value.painted->baked();
 const bool changed=!owner(baked,original_->baked());
 if(polygon_->has_normal_field()){
  if(native||!original_->baked())
   throw std::invalid_argument("Prepared normal detail keeps its independent original role");
  if(changed){
   const auto initial=original_->baked()->normal_field;
   auto ancestor=baked&&baked->normal_field?baked->normal_field->prior():nullptr;
   while(ancestor&&!owner(ancestor,initial)){stop(limits);ancestor=ancestor->prior();}
   if(!ancestor)throw std::invalid_argument("Retained texture-set Bake must extend its own original normal field");
  }
 }else if(changed!=bool(native)){
  throw std::invalid_argument("Native normal and material Bake need the same retained field");
 }
 if(native){
  if(!baked||native->width!=value.painted->resolution()||native->height!=native->width)
   throw std::invalid_argument("Native normal bake has different dimensions from its retained grid");
  if(native->rgba.size()>limits.maximum_bytes)
   throw std::length_error("Native normal bake exceeds its grid budget");
  native->validate();
  bool covered=false;
  for(size_t i=0;i<native->rgba.size();i+=4){
   if((i&4095)==0)stop(limits);
   const auto *pixel=native->rgba.data()+i;
   if(pixel[3]==255){
    if(baked->coverage.at(i/4)!=255)
     throw std::invalid_argument("Native normal bake is outside its covered material grid");
    covered=true;
   }else if(pixel[3]!=0||pixel[0]!=128||pixel[1]!=128||pixel[2]!=255)
    throw std::invalid_argument("Native normal bake has nonneutral or fractional uncovered data");
  }
  if(!covered)throw std::invalid_argument("Native normal bake has no authored coverage");
 }
 return with_member(value,limits,true,std::move(native));
}
std::shared_ptr<const TextureSetFieldSource>TextureSetFieldSource::with_member(
    const TextureSetView&value,const TextureSetLimits&limits,bool rebake,std::shared_ptr<const Image> native)const{
 validate_member(value,limits,rebake);if(working_bytes(value,limits)>limits.maximum_bytes)throw std::length_error("Updated source field exceeds metadata/raster quota");
 auto next=std::shared_ptr<TextureSetFieldSource>(new TextureSetFieldSource);next->field_=value;next->field_.source.reset();next->field_.painted=value.painted->snapshot();next->original_=original_;next->polygon_=polygon_;next->readable_=TextureSetPaintAccess::field_source_view(*next->field_.painted,*original_,field_.domain,limits,next->field_.painted.get(),readable_.get());
 next->preview_=preview_?preview_->with_bake(*next->readable_,std::move(native)):nullptr;
 std::shared_ptr<const TextureSetFieldSource>sealed(new const TextureSetFieldSource(std::move(*next)));sealed->self_=sealed;sealed->validate_field(sealed->field_,limits);return sealed;
}
std::shared_ptr<const TextureSetFieldSource>TextureSetFieldSource::repartition(const TextureSetView&value,std::shared_ptr<const TextureSetFaceDomain>domain,const TextureSetLimits&limits)const{
 auto source=value;source.painted=read_view(value,limits);source.domain.reset();source.source.reset();
 const auto current_bake=source.painted->baked();
 const auto seed_bake=original_->baked();
 const bool same_normal=current_bake&&seed_bake&&seed_bake->normal_field&&
                        owner(current_bake->normal_field,seed_bake->normal_field);
 if(source.painted->resolution()!=original_->resolution()){
  const auto current_paint=source.painted;
  source.painted=original_;
  auto next=capture_prepared(source,domain,preview_->with_bake(*original_,{}),limits);
  auto prototype=TextureSetPaintAccess::source_grid_prototype(*original_,domain,current_paint->resolution(),limits);
  auto current=next->field();
  current.painted=TextureSetPaintAccess::bind_raster(*current_paint,domain,*prototype,limits);
  return same_normal?next->with_material_grid(current,limits):next->with_bake(current,limits);
 }
 if(owner(source.painted->baked(),original_->baked()))return capture_prepared(source,std::move(domain),preview_,limits);
 const auto with_bake=source.painted;
 auto baseline=with_bake->snapshot();TextureSetPaintAccess::restore_source_bake(*baseline,*original_);
 source.painted=baseline;
 auto next=capture_prepared(source,std::move(domain),preview_->with_bake(*baseline,{}),limits);
 auto current=next->field();current.painted=TextureSetPaintAccess::bind_raster(*with_bake,current.domain,*current.painted,limits);
 return same_normal?next->with_material_grid(current,limits):next->with_bake(current,limits,preview_->native_bake());
}
TextureSetFieldSample TextureSetFieldSource::sample(uint32_t triangle,std::array<double,3>b,const TextureSetLimits&limits)const{
 limits.validate();stop(limits);authenticate_owner(self_.lock());if(!field_.domain->contains(triangle))throw std::invalid_argument("A foreign logical triangle cannot sample this material field");double sum=0;for(double x:b){if(!std::isfinite(x)||x<0||x>1)throw std::invalid_argument("Texture-set field barycentrics are outside their actual triangle");sum+=x;}if(std::abs(sum-1)>1e-6)throw std::invalid_argument("Texture-set field barycentrics do not form an actual triangle point");
 TextureSetFieldSample out;out.triangle=triangle;out.barycentric=b;const auto&t=*field_.domain->topology();out.face=t.triangle_faces()[triangle];for(size_t c=0;c<3;++c)out.corners[c]=t.triangle_corners()[size_t(triangle)*3+c];
 if(preview_){
  std::array<double,2>paint{},normal{};
  for(size_t c=0;c<3;++c){const auto uv=preview_->paint_coordinates().at(size_t(triangle)*3+c);for(size_t k=0;k<2;++k)paint[k]+=b[c]*uv[k];if(!preview_->normal_coordinates().empty()){const auto role=preview_->normal_coordinates().at(size_t(triangle)*3+c);for(size_t k=0;k<2;++k)normal[k]+=b[c]*role[k];}}
  out.paint_uv={float(paint[0]),float(paint[1])};if(!preview_->normal_coordinates().empty())out.normal_uv=std::array<float,2>{float(normal[0]),float(normal[1])};
 }
 else{const auto&m=*t.geometry();for(size_t c=0;c<3;++c){const auto uv=m.uv.at(m.indices.at(size_t(triangle)*3+c));for(size_t k=0;k<2;++k)out.paint_uv[k]+=float(b[c]*uv[k]);}}
 return out;
}
Vec3 TextureSetFieldSource::normal(uint32_t triangle,std::array<double,3>b,const AffineTransform&world,bool pbr,const TextureSetLimits&limits)const{
 (void)sample(triangle,b,limits);if(preview_&&(pbr||!polygon_->has_normal_field()))return preview_->normal_at(triangle,b,world,limits.cancelled);if(polygon_)return polygon_->native_normal(triangle,b,world,polygon_limits(limits));
 if(original_->baked()&&!original_->baked()->maps.normal.rgba.empty())throw std::invalid_argument("Canonical texture-set normal sampling needs its actual role evaluator");
 const auto&m=*field_.domain->topology()->geometry();Vec3 n{};for(size_t c=0;c<3;++c)n+=m.normals.at(m.indices.at(size_t(triangle)*3+c))*float(b[c]);const auto placed=world.normal({n.x,n.y,n.z});return normalized(Vec3{float(placed[0]),float(placed[1]),float(placed[2])});
}
std::shared_ptr<const PaintedMesh>texture_set_source_read_view(const TextureSetView&view,const TextureSetLimits&limits){
 if(!view.source)return view.painted;view.source->authenticate_owner(view.source);return view.source->read_view(view,limits);
}
} // namespace dome::detail
