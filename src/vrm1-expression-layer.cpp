// SPDX-License-Identifier: GPL-3.0-or-later
#include "vrm1-expression-layer.hpp"
#include "paint-overlay-sample.hpp"
#include "source-paint-pairing.hpp"
#include "source-extra-uv-authority.hpp"
#include "vrm-painted.hpp"
#include <cmath>
#include <limits>
namespace dome::detail {
namespace {
using J=nlohmann::json;
using Pixel=std::array<double,4>;
double linear(double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);}
double srgb(double v){return v<=.0031308?12.92*v:1.055*std::pow(v,1/2.4)-.055;}
Pixel bilinear(const std::vector<Pixel> &values,uint32_t n,double u,double v){
 const double x=std::clamp(u,0.,1.)*n-.5,y=std::clamp(v,0.,1.)*n-.5;
 const int ix=int(std::floor(x)),iy=int(std::floor(y));const double a=x-ix,b=y-iy;
 auto at=[&](int x,int y)->const Pixel&{return values[size_t(std::clamp(y,0,int(n)-1))*n+size_t(std::clamp(x,0,int(n)-1))];};
 Pixel out{};for(size_t k=0;k<4;++k)out[k]=std::lerp(std::lerp(at(ix,iy)[k],at(ix+1,iy)[k],a),std::lerp(at(ix,iy+1)[k],at(ix+1,iy+1)[k],a),b);return out;
}
}
Vrm1ExpressionLayer compile_vrm1_expression_layer(const J &original,const SculptObject &object,
 const PaintOverlayPacket &packet,uint32_t size,const SourceUvBakeOptions &limits,
 const std::function<bool()> &cancelled){
 auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("VRM1 expression-layer compilation cancelled");};stop();
 if(!object.painted||!object.appearance||!object.material_bindings||!object.material_bindings->has_mtoon)
  throw std::invalid_argument("VRM1 expression layer requires its actual paint and typed source owners");
 const auto &binding=*object.material_bindings;binding.validate();const auto &material=binding.material;
 if(material.version()!=MToonVersion::V1||material.parameters.alpha!=MToonAlpha::Opaque||
    (object.material.alpha!=Material::AlphaMode::Automatic&&object.material.alpha!=Material::AlphaMode::Opaque)||
    material.parameters.outline!=MToonOutline::None||material.parameters.scroll!=std::array<float,2>{}||material.parameters.rotation_speed!=0)
  throw std::invalid_argument("VRM1 expression layer currently requires opaque outline-free nonanimated MToon1 source");
 if(material.parameters.rim_color.x!=0||material.parameters.rim_color.y!=0||material.parameters.rim_color.z!=0)
  throw std::invalid_argument("VRM1 rim expression layers need their independent lighting adapter");
 if(binding.normal_preparation||binding.color_preparation||binding.key_derivation||binding.physical_emission)
  throw std::invalid_argument("VRM1 prepared/absolute expression layers need their owning adapter");
 for(auto role:{MToonSlot::Normal,MToonSlot::Matcap,MToonSlot::Rim,MToonSlot::OutlineWidth,MToonSlot::Shift,MToonSlot::UvMask})
  if(binding.roles[size_t(role)].image)throw std::invalid_argument("VRM1 expression layer role needs its independent consumer adapter");
 const auto &base=binding.roles[0],&shade=binding.roles[1];const auto &bt=material.textures[0],&st=material.textures[1];
 if(base.image&&base.image==shade.image&&base.wrap_s==shade.wrap_s&&base.wrap_t==shade.wrap_t&&base.min_filter==shade.min_filter&&base.mag_filter==shade.mag_filter&&mtoon_coordinate(bt)==mtoon_coordinate(st)&&bt.offset==st.offset&&bt.scale==st.scale&&bt.rotation==st.rotation)
  throw std::invalid_argument("VRM1 expression layer shared base/shade needs its distinct painted shade adapter");
 Vrm1ExpressionLayer result;bool found=false,colour=false;
 const auto expressions=original.at("extensions").at("VRMC_vrm").value("expressions",J::object());
 for(const auto *table:{"preset","custom"})if(expressions.contains(table))for(const auto &expression:expressions.at(table)){
  for(const auto &bind:expression.value("materialColorBinds",J::array()))if(bind.at("material").get<int>()==object.appearance->source_material){
   if(bind.at("type")=="rimColor")throw std::invalid_argument("VRM1 rim colour expression needs its layered lighting adapter");
   colour|=bind.at("type")=="color";
  }
  for(const auto &bind:expression.value("textureTransformBinds",J::array()))if(bind.at("material").get<int>()==object.appearance->source_material){
  const auto scale=bind.value("scale",std::array<float,2>{1,1}),offset=bind.value("offset",std::array<float,2>{});
  if(found&&(result.scale!=scale||result.offset!=offset))throw std::invalid_argument("VRM1 painted UV expressions have incompatible texture targets");
  found=true;result.scale=scale;result.offset=offset;
 }}
 // With genuine colour-only bindings, the paint carrier stays identity and
 // no UV expression is synthesized. The original lower base factor changes;
 // the independently owned upper colour contribution remains untinted.
 if((!found&&!colour)||!std::isfinite(result.scale[0])||!std::isfinite(result.scale[1])||result.scale[0]==0||result.scale[1]==0)
  throw std::invalid_argument("VRM1 expression-layer texture target must have two finite nonzero scales");
 const size_t n=packet.resolution(),pixels=size_t(size)*size,native_pixels=n*n;
 if(size<64||size>4096||pixels>limits.max_image_pixels||native_pixels>limits.max_image_pixels)
  throw std::length_error("VRM1 expression-layer pixel limit exceeded");
 // Charge native premultiplied doubles, decoded output doubles, PNG source,
 // bounded encoder scratch, coordinate result and both merged knot vectors
 // before allocating any of them. Encoded output is bounded by writer too.
 auto charge=[&](size_t count,size_t width){if(result.working_bytes>limits.max_working_bytes||count>(limits.max_working_bytes-result.working_bytes)/width)throw std::length_error("VRM1 expression-layer prospective working limit exceeded");result.working_bytes+=count*width;};
 charge(native_pixels,sizeof(Pixel));charge(pixels,sizeof(Pixel)+10);charge(object.painted->mesh().positions.size(),sizeof(std::array<float,2>));charge(2*(n+size+2),sizeof(double));
 if(pixels>limits.max_output_bytes/6)throw std::length_error("VRM1 expression-layer prospective PNG limit exceeded");
 auto work=[&](size_t count){if(result.sample_operations>limits.max_sample_operations||count>limits.max_sample_operations-result.sample_operations)throw std::length_error("VRM1 expression-layer sampling work limit exceeded");result.sample_operations+=count;};
 const size_t maximum_knots=n+size+2;
 if(maximum_knots>std::numeric_limits<size_t>::max()/maximum_knots||maximum_knots*maximum_knots>limits.max_sample_operations/80)
  throw std::length_error("VRM1 expression-layer prospective continuous proof work limit exceeded");
 std::vector<Pixel> native(native_pixels),decoded(pixels);auto image=std::make_shared<Image>(Image{size,size,std::vector<uint8_t>(pixels*4)});
 for(size_t y=0;y<n;++y){stop();for(size_t x=0;x<n;++x){work(1+packet.layers().size());const auto v=sample_overlay_texel_values(packet,int(x),int(y),false);
  if(v[3][1]!=0||v[3][2]!=0||v[3][3]!=0||v[4]!=std::array<float,4>{})throw std::invalid_argument("VRM1 expression layers currently transport colour paint only");
  auto &p=native[y*n+x];for(size_t k=0;k<3;++k)p[k]=v[0][k];p[3]=v[3][0];
 }}
 for(uint32_t y=0;y<size;++y){stop();for(uint32_t x=0;x<size;++x){work(1);const auto p=bilinear(native,uint32_t(n),(x+.5)/size,(y+.5)/size);auto &q=decoded[size_t(y)*size+x];const size_t at=(size_t(y)*size+x)*4;
  for(size_t k=0;k<4;++k){const double value=k==3?p[3]:p[3]>0?p[k]/p[3]:0;
   if(!std::isfinite(value)||value<0||value>1+1e-6)throw std::invalid_argument("VRM1 expression layer exceeds standard unit texture representation");
   const auto byte=uint8_t(std::clamp(std::lround((k==3?std::clamp(value,0.,1.):srgb(std::clamp(value,0.,1.)))*255),0l,255l));image->rgba[at+k]=byte;q[k]=k==3?byte/255.:linear(byte/255.);
  }
 }}
 std::vector<double> knots{0,1};knots.reserve(n+size+2);for(auto count:{n,size_t(size)})for(size_t i=0;i<count;++i)knots.push_back((i+.5)/count);std::sort(knots.begin(),knots.end());knots.erase(std::unique(knots.begin(),knots.end()),knots.end());
 // On every merged texel-centre rectangle both reconstructed fields are
 // bilinear. Product(straight RGB,coverage) is biquadratic: its Bernstein
 // control hull bounds error against the degree-elevated native premult field
 // throughout the full footprint, not just at selected sample points.
 for(size_t y=1;y<knots.size();++y){stop();for(size_t x=1;x<knots.size();++x){work(80);Pixel a[2][2],b[2][2];for(int j=0;j<2;++j)for(int i=0;i<2;++i){a[j][i]=bilinear(native,uint32_t(n),knots[x-1+i],knots[y-1+j]);b[j][i]=bilinear(decoded,size,knots[x-1+i],knots[y-1+j]);}
  for(int j=0;j<3;++j)for(int i=0;i<3;++i)for(size_t k=0;k<4;++k){double product=0;
   if(k==3)product=std::lerp(std::lerp(b[0][0][3],b[0][1][3],i*.5),std::lerp(b[1][0][3],b[1][1][3],i*.5),j*.5);
   else for(int m=0;m<2;++m)for(int l=0;l<2;++l)if(i-l>=0&&i-l<2&&j-m>=0&&j-m<2)product+=b[m][l][k]*b[j-m][i-l][3]*(i==1?.5:1)*(j==1?.5:1);
   const double reference=std::lerp(std::lerp(a[0][0][k],a[0][1][k],i*.5),std::lerp(a[1][0][k],a[1][1][k],i*.5),j*.5),error=std::abs(product-reference)+1e-12;
   auto &maximum=k==3?result.maximum_coverage_error:result.maximum_color_error;maximum=std::max(maximum,error);
  }
 }}
 if(result.maximum_color_error+result.maximum_coverage_error>3./(255*1.25))throw std::invalid_argument("VRM1 straight-alpha PNG reconstruction exceeds fixed portable expression-layer error budget");
 result.paint_coordinates.reserve(object.painted->mesh().uv.size());for(auto uv:object.painted->mesh().uv){stop();for(size_t k=0;k<2;++k){uv[k]=(uv[k]-result.offset[k])/result.scale[k];if(!std::isfinite(uv[k]))throw std::invalid_argument("VRM1 expression-layer carrier coordinate is nonfinite");}result.paint_coordinates.push_back(uv);}
 if(result.paint_coordinates.size()!=object.painted->mesh().positions.size())throw std::invalid_argument("VRM1 expression layer has no complete writable coordinate owner");
 // Bound ordinary F32 interpolation/scale/add cancellation relative to the
 // native field's one-texel Lipschitz scale. This rejects huge inverse carriers
 // even when double cancellation at the vertices would appear harmless.
 for(size_t k=0;k<2;++k){double maximum=0;for(auto uv:result.paint_coordinates)maximum=std::max(maximum,std::abs(double(uv[k])));
  const double forward_error=16*std::numeric_limits<float>::epsilon()*(std::abs(double(result.scale[k]))*maximum+std::abs(double(result.offset[k]))+1);
  if(forward_error>1./(n*1024))throw std::invalid_argument("VRM1 expression carrier exceeds bounded native-field float precision");
 }
 result.image=std::move(image);stop();return result;
}
void vrm1_expression_background(const PaintedMesh &paint,ObjectSurface &out,const std::function<bool()> &cancelled){
 auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("VRM1 expression background cancelled");};stop();
 if(!paint.source_sampling_prepared()||!paint.valid_source_sampling_attachment())throw std::invalid_argument("VRM1 expression background lost source sampling authority");
 source_paint_pairing(&paint,out.material_bindings.get(),out.appearance.get(),false);if(out.appearance)extra_source_uv_pair(paint,*out.appearance,cancelled);
 const auto marker=out.textures;if(!marker||!marker->source_only||!marker->overlay||marker->overlay->source_maps()!=paint.source_maps())throw std::invalid_argument("VRM1 expression background lost its actual paint packet");
 const auto before=paint.source_sampling_original();const auto vertices=paint.source_sampling_vertices();if(!before||vertices.size()!=out.mesh.positions.size()||out.mesh.indices!=paint.mesh().indices)throw std::invalid_argument("VRM1 expression background differs from its ordered paint domain");
 const PaintSourceMaps empty;const auto &maps=before->source_maps()?*before->source_maps():empty;const size_t arrays=1+bool(maps.uv1)+maps.extra_uv_sets.size();
 if(vertices.size()>2'000'000||arrays>8||vertices.size()>128*1024*1024/(arrays*sizeof(std::array<float,2>)))throw std::length_error("VRM1 expression background coordinates exceed budget");
 auto remap=[&](const auto &v){if(v.size()!=before->mesh().positions.size())throw std::invalid_argument("VRM1 expression background original coordinates are incomplete");std::vector<std::array<float,2>> result;result.reserve(vertices.size());for(size_t i=0;i<vertices.size();++i){if((i&1023)==0)stop();result.push_back(v.at(vertices[i]));}return result;};
 auto source=std::make_shared<PaintSourceMaps>(maps);if(maps.uv1)source->uv1=std::make_shared<const std::vector<std::array<float,2>>>(remap(*maps.uv1));for(auto &[set,v]:source->extra_uv_sets)v=std::make_shared<const std::vector<std::array<float,2>>>(remap(*v));
 out.mesh.uv=remap(before->mesh().uv);out.mesh.uv_pins.clear();out.mesh.uv_seams.clear();source->validate_surface(out.mesh);
 auto textures=std::make_shared<TextureMaps>();textures->source_only=true;textures->source_maps=std::move(source);textures->transparent=marker->transparent;out.textures=std::move(textures);stop();
}
}
