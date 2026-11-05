// SPDX-License-Identifier: GPL-3.0-or-later
#include "vrm1-expression-layer.hpp"
#include "vrm1-mask-support.hpp"
#include "paint-overlay-sample.hpp"
#include "source-paint-pairing.hpp"
#include "source-extra-uv-authority.hpp"
#include "vrm-painted.hpp"
#include <cmath>
#include <limits>
#include <numeric>
namespace dome::detail {
namespace {
using J=nlohmann::json;
using Pixel=std::array<double,4>;
double linear(double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);}
double srgb(double v){return v<=.0031308?12.92*v:1.055*std::pow(v,1/2.4)-.055;}
// MatCap is an additive view-normal term, independent of native colour paint.
// Its same genuine field occurs in lower/upper primitives. Require a finite
// nonnegative future coefficient box and reserve a conservative F32 composition
// margin at the existing native lighting range; this is not a VRM schema limit.
double matcap_margin(const J &expressions,const SculptObject &object,
 Vrm1ExpressionLayer &stats,const SourceUvBakeOptions &limits,
 const std::function<bool()> &cancelled){
 const auto &binding=*object.material_bindings;if(!binding.roles[size_t(MToonSlot::Matcap)].image)return 0;
 if(stats.working_bytes>limits.max_working_bytes||64>limits.max_working_bytes-stats.working_bytes)throw std::length_error("VRM1 MatCap prospective working limit exceeded");
 stats.working_bytes+=64;
 const auto &factor=binding.material.parameters.matcap_color;const std::array<double,3> initial{factor.x,factor.y,factor.z};auto low=initial,high=initial;
 auto work=[&]{if(stats.sample_operations>limits.max_sample_operations||4>limits.max_sample_operations-stats.sample_operations)throw std::length_error("VRM1 MatCap prospective parameter work limit exceeded");stats.sample_operations+=4;};
 for(const char *table:{"preset","custom"})if(expressions.contains(table))for(const auto &expression:expressions.at(table)){
  if(cancelled&&cancelled())throw std::runtime_error("VRM1 MatCap parameter compilation cancelled");
  work();std::array<double,3> delta{};
  if(expression.contains("materialColorBinds"))for(const auto &bind:expression.at("materialColorBinds")){work();if(bind.at("material").get<int>()!=object.appearance->source_material||bind.at("type")!="matcapColor")continue;for(size_t k=0;k<3;++k){const double target=bind.at("targetValue").at(k).get<double>();if(!std::isfinite(target)||std::abs(target)>1e20)throw std::invalid_argument("VRM1 MatCap target exceeds finite source metadata bounds");delta[k]+=target-initial[k];}}
  for(size_t k=0;k<3;++k){low[k]+=std::min(0.,delta[k]);high[k]+=std::max(0.,delta[k]);}
 }
 double maximum=0;for(size_t k=0;k<3;++k){if(!std::isfinite(low[k])||!std::isfinite(high[k])||low[k]<0||high[k]>1e20)throw std::invalid_argument("VRM1 MatCap future coefficients need a nonnegative finite adapter");maximum=std::max(maximum,high[k]);}
 const double error=32*std::numeric_limits<float>::epsilon()*1.25*maximum;
 if(error>=3./255)throw std::invalid_argument("VRM1 MatCap float composition exceeds fixed portable budget");
 return error;
}
// Parametric Rim and MatCap share the real rim-multiply role. Their additive
// terms remain independent from paint RGB. Bound the complete expression weight
// box before allocating image scratch; the source coefficients are not clamped.
double rim_margin(const J &expressions,const SculptObject &object,
 Vrm1ExpressionLayer &stats,const SourceUvBakeOptions &limits,
 const std::function<bool()> &cancelled){
 const auto &binding=*object.material_bindings;const auto &factor=binding.material.parameters.rim_color;
 const std::array<double,3> initial{factor.x,factor.y,factor.z};
 bool relevant=initial!=std::array<double,3>{};
 for(const char *table:{"preset","custom"})if(expressions.contains(table))for(const auto &expression:expressions.at(table)){
  if(cancelled&&cancelled())throw std::runtime_error("VRM1 Rim parameter compilation cancelled");
  if(expression.contains("materialColorBinds"))for(const auto &bind:expression.at("materialColorBinds"))if(bind.at("material").get<int>()==object.appearance->source_material&&bind.at("type")=="rimColor")relevant=true;
 }
 if(!relevant)return 0;
 if(binding.material.parameters.alpha!=MToonAlpha::Opaque)throw std::invalid_argument("VRM1 masked Rim layers need their independent membership adapter");
 if(stats.working_bytes>limits.max_working_bytes||64>limits.max_working_bytes-stats.working_bytes)throw std::length_error("VRM1 Rim prospective working limit exceeded");
 stats.working_bytes+=64;auto low=initial,high=initial;
 auto work=[&]{if(stats.sample_operations>limits.max_sample_operations||4>limits.max_sample_operations-stats.sample_operations)throw std::length_error("VRM1 Rim prospective parameter work limit exceeded");stats.sample_operations+=4;};
 for(const char *table:{"preset","custom"})if(expressions.contains(table))for(const auto &expression:expressions.at(table)){
  if(cancelled&&cancelled())throw std::runtime_error("VRM1 Rim parameter compilation cancelled");
  work();std::array<double,3> delta{};
  if(expression.contains("materialColorBinds"))for(const auto &bind:expression.at("materialColorBinds")){work();if(bind.at("material").get<int>()!=object.appearance->source_material||bind.at("type")!="rimColor")continue;for(size_t k=0;k<3;++k){const double target=bind.at("targetValue").at(k).get<double>();if(!std::isfinite(target)||std::abs(target)>1e20)throw std::invalid_argument("VRM1 Rim target exceeds finite source metadata bounds");delta[k]+=target-initial[k];}}
  for(size_t k=0;k<3;++k){low[k]+=std::min(0.,delta[k]);high[k]+=std::max(0.,delta[k]);}
 }
 double maximum=0;for(size_t k=0;k<3;++k){if(!std::isfinite(low[k])||!std::isfinite(high[k])||low[k]<0||high[k]>1e20)throw std::invalid_argument("VRM1 Rim future coefficients need a nonnegative finite adapter");maximum=std::max(maximum,high[k]);}
 // Tiny exponents use implementation-specific safety epsilons. Preserve the
 // source and refuse that still unqualified model instead of changing its power.
 if(maximum&&binding.material.parameters.rim_power<.0009765625f)throw std::invalid_argument("VRM1 Rim tiny exponent needs its independent model adapter");
 const double error=32*std::numeric_limits<float>::epsilon()*1.25*maximum;
 if(error>=3./255)throw std::invalid_argument("VRM1 Rim float composition exceeds fixed portable budget");
 return error;
}
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
 const bool opaque=material.parameters.alpha==MToonAlpha::Opaque&&(object.material.alpha==Material::AlphaMode::Automatic||object.material.alpha==Material::AlphaMode::Opaque);
 const bool masked=material.parameters.alpha==MToonAlpha::Mask&&(object.material.alpha==Material::AlphaMode::Automatic||object.material.alpha==Material::AlphaMode::Mask||int(object.material.alpha)==binding.native_reference.alpha);
 if(material.version()!=MToonVersion::V1||(!opaque&&!masked)||
    material.parameters.outline!=MToonOutline::None||material.parameters.scroll!=std::array<float,2>{}||material.parameters.rotation_speed!=0)
  throw std::invalid_argument("VRM1 expression layer currently requires opaque outline-free nonanimated MToon1 source");
 if(binding.canonical_attributes||binding.canonical_uv||binding.native_paint_sampling||binding.normal_preparation||binding.color_preparation||binding.key_derivation||binding.physical_emission)
  throw std::invalid_argument("VRM1 prepared/absolute expression layers need their owning adapter");
 for(auto role:{MToonSlot::Normal,MToonSlot::OutlineWidth,MToonSlot::Shift,MToonSlot::UvMask})
  if(binding.roles[size_t(role)].image)throw std::invalid_argument("VRM1 expression layer role needs its independent consumer adapter");
 if(masked&&binding.roles[size_t(MToonSlot::Rim)].image)throw std::invalid_argument("VRM1 masked Rim texture needs its independent membership adapter");
 const auto &base=binding.roles[0],&shade=binding.roles[1];const auto &bt=material.textures[0],&st=material.textures[1];
 Vrm1ExpressionLayer result;bool found=false,colour=false,same_target=true;
 result.shade_alias=base.image&&base.image==shade.image&&base.wrap_s==shade.wrap_s&&base.wrap_t==shade.wrap_t&&base.min_filter==shade.min_filter&&base.mag_filter==shade.mag_filter&&mtoon_coordinate(bt)==mtoon_coordinate(st)&&bt.offset==st.offset&&bt.scale==st.scale&&bt.rotation==st.rotation;
 if(result.shade_alias)for(size_t k=0;k<3;++k)result.shade_divisor[k]=object.material.color[k]!=0?object.material.color[k]:1;
 const std::array<float,3> initial_shade{material.parameters.shade_color.x,material.parameters.shade_color.y,material.parameters.shade_color.z};
 auto shade_factor=[&](const std::array<float,3> &factor,bool initial){if(result.shade_alias)for(size_t k=0;k<3;++k){const double value=double(factor[k])/result.shade_divisor[k];if(!std::isfinite(value)||value<0||value>1e20)throw std::invalid_argument(initial?"VRM1 shared painted initial shade coefficient exceeds finite source bounds":"VRM1 shared painted shade target exceeds finite nonnegative source representation");if(initial){double scale=1;while(scale<value)scale*=2;result.paint_rgb_scale[k]=float(scale);}}};
 shade_factor(initial_shade,true);
 std::array<double,3> shade_low{},shade_high{};if(result.shade_alias)for(size_t k=0;k<3;++k)shade_low[k]=shade_high[k]=double(initial_shade[k])/result.shade_divisor[k];
 const double initial_alpha=masked?vrm_native_factor(material.parameters.base_color[3],binding.native_reference.color[3],object.material.color[3]):1;
 double alpha_low=initial_alpha,alpha_high=initial_alpha;
 const double cutoff=object.material.alpha_cutoff!=binding.native_reference.alpha_cutoff?object.material.alpha_cutoff:material.parameters.alpha_cutoff;
 const auto expressions=original.at("extensions").at("VRMC_vrm").value("expressions",J::object());
 const double additive_error=matcap_margin(expressions,object,result,limits,cancelled)+rim_margin(expressions,object,result,limits,cancelled);
 for(const auto *table:{"preset","custom"})if(expressions.contains(table))for(const auto &expression:expressions.at(table)){
  std::array<double,3> shade_delta{};double alpha_delta=0;
  for(const auto &bind:expression.value("materialColorBinds",J::array()))if(bind.at("material").get<int>()==object.appearance->source_material){
   colour|=bind.at("type")=="color";
   if(masked&&bind.at("type")=="color"){const double alpha=bind.at("targetValue").at(3).get<double>();if(!std::isfinite(alpha)||std::abs(alpha)>1e20)throw std::invalid_argument("VRM1 mask colour target is outside finite source metadata bounds");alpha_delta+=alpha-initial_alpha;}
   if(bind.at("type")=="shadeColor"){const auto target=bind.at("targetValue").get<std::array<float,4>>();shade_factor({target[0],target[1],target[2]},false);if(result.shade_alias)for(size_t k=0;k<3;++k)shade_delta[k]+=(double(target[k])-initial_shade[k])/result.shade_divisor[k];}
  }
  if(masked){alpha_low+=std::min(0.,alpha_delta);alpha_high+=std::max(0.,alpha_delta);}
  if(result.shade_alias)for(size_t k=0;k<3;++k){shade_low[k]+=std::min(0.,shade_delta[k]);shade_high[k]+=std::max(0.,shade_delta[k]);}
  for(const auto &bind:expression.value("textureTransformBinds",J::array()))if(bind.at("material").get<int>()==object.appearance->source_material){
  const auto raw_scale=bind.value("scale",std::array<double,2>{1,1}),raw_offset=bind.value("offset",std::array<double,2>{});
  std::array<float,2> scale{},offset{};for(size_t k=0;k<2;++k){if(!std::isfinite(raw_scale[k])||!std::isfinite(raw_offset[k])||std::abs(raw_scale[k])>1e20||std::abs(raw_offset[k])>1e20)throw std::invalid_argument("VRM1 texture target is outside finite source metadata bounds");scale[k]=float(raw_scale[k]);offset[k]=float(raw_offset[k]);}
  if(found&&(result.scale!=scale||result.offset!=offset))same_target=false;
  if(!found){result.scale=scale;result.offset=offset;}found=true;
 }}
 // VRM1 targetValue is an unrestricted float4. The serialized initial MToon
 // shade factor remains unit, while every expression may accumulate deltas.
 // Bound the entire [0,1] weight box, including simultaneous custom/presets,
 // rather than assuming one active expression or clamping their true targets.
 if(result.shade_alias)for(size_t k=0;k<3;++k){const double bound=std::max(std::abs(shade_low[k]),std::abs(shade_high[k]));if(!std::isfinite(bound)||bound>1e20)throw std::invalid_argument("VRM1 accumulated shade coefficients exceed finite source bounds");result.maximum_shade_coefficient=std::max(result.maximum_shade_coefficient,bound);}
 if(masked&&(!std::isfinite(alpha_low)||!std::isfinite(alpha_high)||alpha_low<0||alpha_high>1||!std::isfinite(cutoff)||cutoff<0||cutoff>1))throw std::invalid_argument("VRM1 masked alpha expressions need their native unit-alpha adapter");
 const bool wide_shade=result.maximum_shade_coefficient>1;
 const double lighting_bound=std::max(1.,result.maximum_shade_coefficient)+.25;
 // Source per-material bindings need not be copied onto an upper role
 // whose actual contribution is UV-invariant. Shared shade is the new paint
 // image itself; other shade/emission roles require immutable data proof.
 auto active_components=[&](std::array<float,3> initial,std::string_view type){std::array<bool,3> active{};for(size_t k=0;k<3;++k){if(!std::isfinite(initial[k]))throw std::invalid_argument("VRM1 upper role has nonfinite native coefficients");active[k]=initial[k]!=0;}
  size_t visits=0;for(const auto *table:{"preset","custom"})if(expressions.contains(table))for(const auto &expression:expressions.at(table)){stop();if(expression.contains("materialColorBinds"))for(const auto &bind:expression.at("materialColorBinds")){if((++visits&255)==0)stop();if(bind.at("material").get<int>()==object.appearance->source_material&&bind.at("type").get_ref<const std::string&>()==type)for(size_t k=0;k<3;++k){const double target=bind.at("targetValue").at(k).get<double>();if(!std::isfinite(target)||std::abs(target)>1e20)throw std::invalid_argument("VRM1 colour target is outside finite source metadata bounds");active[k]=active[k]||target!=0;}}}
  return active;
 };
 auto invariant_image=[&](const PaintSourceTexture &role,std::array<bool,3> active){
  if(!role.image||std::none_of(active.begin(),active.end(),[](bool v){return v;}))return true;
  const auto &image=*role.image;const size_t pixels=size_t(image.width)*image.height;
  if(pixels>limits.max_image_pixels||result.sample_operations>limits.max_sample_operations||pixels>(limits.max_sample_operations-result.sample_operations)/3)throw std::length_error("VRM1 invariant-role prospective pixel/work limit exceeded");
  result.sample_operations+=pixels*3;
  for(size_t y=0;y<image.height;++y){stop();for(size_t x=0;x<image.width;++x)for(size_t k=0;k<3;++k)if(active[k]&&image.rgba[(y*image.width+x)*4+k]!=image.rgba[k])return false;}
  return true;
 };
 if(!found&&!colour)throw std::invalid_argument("VRM1 expression layer has no relevant original binding");
 if(found&&(!same_target||result.scale[0]==0||result.scale[1]==0)){
  bool invariant=result.shade_alias||invariant_image(shade,active_components({material.parameters.shade_color.x,material.parameters.shade_color.y,material.parameters.shade_color.z},"shadeColor"));
  const auto &emission=material.parameters.emission;std::array<float,3> effective{vrm_native_factor(emission.x,binding.native_reference.emission[0],object.material.emissive[0]),vrm_native_factor(emission.y,binding.native_reference.emission[1],object.material.emissive[1]),vrm_native_factor(emission.z,binding.native_reference.emission[2],object.material.emissive[2])};
  invariant=invariant&&(material.parameters.emission_strength==0||invariant_image(binding.roles[size_t(MToonSlot::Emission)],active_components(effective,"emissionColor")));
  auto rim_active=active_components({material.parameters.rim_color.x,material.parameters.rim_color.y,material.parameters.rim_color.z},"rimColor");
  if(binding.roles[size_t(MToonSlot::Matcap)].image){const auto matcap_active=active_components({material.parameters.matcap_color.x,material.parameters.matcap_color.y,material.parameters.matcap_color.z},"matcapColor");for(size_t k=0;k<3;++k)rim_active[k]=rim_active[k]||matcap_active[k];}
  invariant=invariant&&invariant_image(binding.roles[size_t(MToonSlot::Rim)],rim_active);
  if(!invariant)throw std::invalid_argument(same_target?"VRM1 zero UV target couples moving upper roles to fixed paint":"VRM1 differing UV targets couple moving upper roles to fixed paint");
  result.forward_uv=false;result.scale={1,1};result.offset={};
 }
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
 const size_t divisor=std::gcd(n,size_t(size));
 const size_t shared_centres=(n/divisor)%2&&(size/divisor)%2?divisor:0;
 const size_t knot_count=n+size-shared_centres+2,cells=knot_count-1;
 bool compensate_tent=wide_shade&&size>n&&size%n==0&&(size/n)%2==0;
 size_t prefilter_work=wide_shade?(compensate_tent?13:9):0;
 // Forecast the full native sampling, optional prefilter and continuous proof
 // together, before dense allocations. The centre intersection count is exact:
 // n/g and size/g must both be odd to share the g rational centre knots.
 const auto &alpha_image=base.image;const size_t alpha_pixels=masked&&alpha_image?size_t(alpha_image->width)*alpha_image->height:0;
 if(alpha_pixels>limits.max_image_pixels)throw std::length_error("VRM1 masked source-alpha prospective pixel limit exceeded");
 size_t forecast=result.sample_operations;
 auto forecast_work=[&](size_t count,size_t width){if(forecast>limits.max_sample_operations||count>(limits.max_sample_operations-forecast)/width)throw std::length_error("VRM1 expression-layer prospective complete sampling work limit exceeded");forecast+=count*width;};
 if(masked){forecast_work(alpha_pixels,1);forecast_work(native_pixels,1+packet.layers().size());}
 forecast_work(native_pixels,1+packet.layers().size());forecast_work(pixels,1+prefilter_work);forecast_work(cells*cells,80);
 const uint32_t support_multiple=size>=n?uint32_t(size/n):0;
 const uint32_t support_logical=support_multiple?uint32_t(n)*(support_multiple%2?support_multiple:support_multiple-1):size;
 uint32_t logical_size=size,inset=0;double image_scale=1,image_offset=0;size_t support_remaining=0;
 bool support_reconstruction=false;
 if(masked){
  // All source samplers/mips use convex alpha reconstruction. Scan the REAL
  // immutable role image, then combine that bound with the native independent
  // paint's premultiplied alpha and coverage at every native knot. Their
  // bilinear difference has no lower interior extremum. No role is flattened.
  unsigned minimum=255;work(alpha_pixels);
  if(alpha_image)for(size_t y=0;y<alpha_image->height;++y){stop();for(size_t x=0;x<alpha_image->width;++x)minimum=std::min(minimum,unsigned(alpha_image->rgba[(y*alpha_image->width+x)*4+3]));}
  const double original_alpha=alpha_low*(minimum/255.);double lower=1;
  for(size_t y=0;y<n;++y){stop();for(size_t x=0;x<n;++x){work(1+packet.layers().size());const auto v=sample_overlay_texel_values(packet,int(x),int(y),false);lower=std::min(lower,original_alpha*(1-double(v[3][0]))+double(v[0][3]));}}
  // Keep a conservative F32 filtering/composition margin. Cutoff zero can
  // never discard the native clamped alpha, including genuine authored zero.
  if(cutoff>0&&lower<cutoff+1e-5){support_remaining=forecast-result.sample_operations;require_vrm1_mask_paint_support(expressions,object,packet,support_logical,alpha_low,cutoff,result,limits,support_remaining,cancelled);support_reconstruction=true;}
 }
 if(support_reconstruction){
  // An odd multiple aligns every native texel centre with an output centre.
  // Keep the requested PNG dimensions; its padded interior carries the exact
  // sampling footprint via a genuine affine paint-coordinate carrier.
  logical_size=support_logical;inset=(size-logical_size)/2;image_scale=double(logical_size)/size;image_offset=double(inset)/size;compensate_tent=false;prefilter_work=9;
  // Reforecast the actual aligned representation as a whole before allocating
  // its dense scratch. Support scans have already consumed their own budget.
  size_t remaining=result.sample_operations;auto reserve=[&](size_t count,size_t width){if(remaining>limits.max_sample_operations||count>(limits.max_sample_operations-remaining)/width)throw std::length_error("VRM1 mask support reconstruction prospective work limit exceeded");remaining+=count*width;};
  const size_t g=std::gcd(n,size_t(logical_size)),shared=(n/g)%2&&(logical_size/g)%2?g:0,cells=n+logical_size-shared+1;
  reserve(native_pixels,1+packet.layers().size());reserve(size_t(logical_size)*logical_size,1);reserve(pixels,prefilter_work+(logical_size!=size));reserve(cells*cells,80);
 }
 const size_t proof_divisor=std::gcd(n,size_t(logical_size)),proof_shared=(n/proof_divisor)%2&&(logical_size/proof_divisor)%2?proof_divisor:0,proof_knot_count=n+logical_size-proof_shared+2;
 if(prefilter_work)work(pixels*prefilter_work);
 std::vector<Pixel> native(native_pixels),decoded(size_t(logical_size)*logical_size);auto image=std::make_shared<Image>(Image{size,size,std::vector<uint8_t>(pixels*4)});
 for(size_t y=0;y<n;++y){stop();for(size_t x=0;x<n;++x){work(1+packet.layers().size());const auto v=sample_overlay_texel_values(packet,int(x),int(y),false);
  if(v[3][1]!=0||v[3][2]!=0||v[3][3]!=0||v[4]!=std::array<float,4>{})throw std::invalid_argument("VRM1 expression layers currently transport colour paint only");
  auto &p=native[y*n+x];for(size_t k=0;k<3;++k){p[k]=v[0][k];if(double(v[0][k])*result.paint_rgb_scale[k]>double(v[3][0])*(1+1e-6))throw std::invalid_argument("VRM1 initial shade factorization exceeds actual unit paint-image range");}p[3]=v[3][0];
 }}
 for(uint32_t y=0;y<logical_size;++y){stop();for(uint32_t x=0;x<logical_size;++x){work(1);auto p=bilinear(native,uint32_t(n),(x+.5)/logical_size,(y+.5)/logical_size);
  if(compensate_tent){
   // An even integer upsample misaligns native/output texel-centre knots.
   // A linear texture adds a second tent reconstruction to the native field.
   // A bounded local prefilter compensates its knot smoothing. Its values are
   // still genuine premultiplied colour/coverage, and the unchanged continuous
   // proof below measures the resulting texture against the ORIGINAL field.
   Pixel adjacent{};for(auto d:{std::array<int,2>{-1,0},{1,0},{0,-1},{0,1}}){const auto a=bilinear(native,uint32_t(n),(double(x)+d[0]+.5)/logical_size,(double(y)+d[1]+.5)/logical_size);for(size_t k=0;k<4;++k)adjacent[k]+=a[k];}
   for(size_t k=0;k<4;++k)p[k]+=.25*(4*p[k]-adjacent[k]);
   p[3]=std::clamp(p[3],0.,1.);for(size_t k=0;k<3;++k)p[k]=std::clamp(p[k],0.,p[3]);
  }
  auto &q=decoded[size_t(y)*logical_size+x];const size_t at=(size_t(y+inset)*size+x+inset)*4;
  for(size_t k=0;k<4;++k){const double value=k==3?p[3]:p[3]>0?p[k]/p[3]*result.paint_rgb_scale[k]:0;
   if(!std::isfinite(value)||value<0||value>1+1e-6)throw std::invalid_argument("VRM1 expression layer exceeds standard unit texture representation");
   const auto byte=uint8_t(std::clamp(std::lround((k==3?std::clamp(value,0.,1.):srgb(std::clamp(value,0.,1.)))*255),0l,255l));image->rgba[at+k]=byte;q[k]=k==3?byte/255.:linear(byte/255.)/result.paint_rgb_scale[k];
  }
 }}
 // RGB at genuinely zero native coverage has no pointwise contribution.
 // Its filtered value does matter at the boundary. Extend genuine neighbouring
 // straight colour there, without changing coverage or positive-coverage data.
 // All reads come from the immutable native field (never propagated padding).
 // The continuous proof below still decides admission; this is no error waiver.
 if(wide_shade||support_reconstruction)for(uint32_t y=0;y<logical_size;++y){stop();for(uint32_t x=0;x<logical_size;++x){
  const double u=(x+.5)/logical_size,v=(y+.5)/logical_size;if(bilinear(native,uint32_t(n),u,v)[3]!=0)continue;
  Pixel sum{};for(int j=-1;j<=1;++j)for(int i=-1;i<=1;++i)if(i||j){const auto p=bilinear(native,uint32_t(n),(double(x)+i+.5)/logical_size,(double(y)+j+.5)/logical_size);for(size_t k=0;k<4;++k)sum[k]+=p[k];}
  if(sum[3]>0)for(size_t k=0;k<3;++k){const double value=std::clamp(sum[k]/sum[3]*result.paint_rgb_scale[k],0.,1.);const auto byte=uint8_t(std::clamp(std::lround(srgb(value)*255),0l,255l));const size_t at=size_t(y)*logical_size+x,image_at=(size_t(y+inset)*size+x+inset)*4;image->rgba[image_at+k]=byte;decoded[at][k]=linear(byte/255.)/result.paint_rgb_scale[k];}
 }}
 if(logical_size!=size){work(pixels);for(uint32_t y=0;y<size;++y){stop();for(uint32_t x=0;x<size;++x)if(x<inset||x>=inset+logical_size||y<inset||y>=inset+logical_size){const auto sx=uint32_t(std::clamp(int(x)-int(inset),0,int(logical_size)-1))+inset,sy=uint32_t(std::clamp(int(y)-int(inset),0,int(logical_size)-1))+inset;std::copy_n(image->rgba.data()+(size_t(sy)*size+sx)*4,4,image->rgba.data()+(size_t(y)*size+x)*4);}}}
 std::vector<double> knots{0,1};knots.reserve(n+logical_size+2);for(auto count:{n,size_t(logical_size)})for(size_t i=0;i<count;++i)knots.push_back((i+.5)/count);std::sort(knots.begin(),knots.end());knots.erase(std::unique(knots.begin(),knots.end()),knots.end());if(knots.size()!=proof_knot_count)throw std::logic_error("VRM1 expression-layer exact centre-knot forecast differs");
 // On every merged texel-centre rectangle both reconstructed fields are
 // bilinear. Product(straight RGB,coverage) is biquadratic: its Bernstein
 // control hull bounds error against the degree-elevated native premult field
 // throughout the full footprint, not just at selected sample points.
 for(size_t y=1;y<knots.size();++y){stop();for(size_t x=1;x<knots.size();++x){work(80);Pixel a[2][2],b[2][2];for(int j=0;j<2;++j)for(int i=0;i<2;++i){a[j][i]=bilinear(native,uint32_t(n),knots[x-1+i],knots[y-1+j]);b[j][i]=bilinear(decoded,logical_size,knots[x-1+i],knots[y-1+j]);}
  for(int j=0;j<3;++j)for(int i=0;i<3;++i)for(size_t k=0;k<4;++k){double product=0;
   if(k==3)product=std::lerp(std::lerp(b[0][0][3],b[0][1][3],i*.5),std::lerp(b[1][0][3],b[1][1][3],i*.5),j*.5);
   else for(int m=0;m<2;++m)for(int l=0;l<2;++l)if(i-l>=0&&i-l<2&&j-m>=0&&j-m<2)product+=b[m][l][k]*b[j-m][i-l][3]*(i==1?.5:1)*(j==1?.5:1);
   const double reference=std::lerp(std::lerp(a[0][0][k],a[0][1][k],i*.5),std::lerp(a[1][0][k],a[1][1][k],i*.5),j*.5),error=std::abs(product-reference)+1e-12;
   auto &maximum=k==3?result.maximum_coverage_error:result.maximum_color_error;maximum=std::max(maximum,error);
  }
 }}
 if(result.maximum_color_error+result.maximum_coverage_error>3./(255*lighting_bound)-additive_error/lighting_bound)throw std::invalid_argument("VRM1 straight-alpha PNG reconstruction exceeds fixed portable expression-layer error budget: RGB="+std::to_string(result.maximum_color_error)+" coverage="+std::to_string(result.maximum_coverage_error)+" light="+std::to_string(lighting_bound));
 result.paint_coordinates.reserve(object.painted->mesh().uv.size());for(auto uv:object.painted->mesh().uv){stop();for(size_t k=0;k<2;++k){uv[k]=((support_reconstruction?float(uv[k]*image_scale+image_offset):uv[k])-result.offset[k])/result.scale[k];if(!std::isfinite(uv[k]))throw std::invalid_argument("VRM1 expression-layer carrier coordinate is nonfinite");}result.paint_coordinates.push_back(uv);}
 if(result.paint_coordinates.size()!=object.painted->mesh().positions.size())throw std::invalid_argument("VRM1 expression layer has no complete writable coordinate owner");
 // Bound ordinary F32 interpolation/scale/add cancellation relative to the
 // native field's one-texel Lipschitz scale. This rejects huge inverse carriers
 // even when double cancellation at the vertices would appear harmless.
 for(size_t k=0;k<2;++k){double maximum=0;for(auto uv:result.paint_coordinates)maximum=std::max(maximum,std::abs(double(uv[k])));
  const double forward_error=16*std::numeric_limits<float>::epsilon()*(std::abs(double(result.scale[k]))*maximum+std::abs(double(result.offset[k]))+1+(support_reconstruction?image_scale+image_offset:0))/image_scale;
  if(forward_error>1./(n*1024))throw std::invalid_argument("VRM1 expression carrier exceeds bounded native-field float precision");
 }
 for(size_t k=0;k<3;++k)result.shade_divisor[k]*=result.paint_rgb_scale[k];
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
