// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/projection.hpp"
#include "dome/mesh-processing.hpp"
#include "dome/scene.hpp"
#include <cerrno>
#include <fcntl.h>
#include <png.h>
#include <sys/stat.h>
#include <unistd.h>
namespace dome {
namespace {
void limits_valid(const ProjectionImageLimits &l) {
  if (!l.max_encoded_bytes || l.max_encoded_bytes>256ull*1024*1024 || !l.max_pixels ||
      l.max_pixels>67'108'864 || !l.max_dimension || l.max_dimension>8192)
    throw std::invalid_argument("Invalid projection image limits");
}
void cancel(const ProjectionOptions &o) {
  if (o.cancelled&&o.cancelled()) throw std::runtime_error("Image projection cancelled");
}
bool unit(float v) { return std::isfinite(v)&&v>=0&&v<=1; }
Vec3 convert(ob::Vec3 value) { return {value.x,value.y,value.z}; }
std::array<float,4> image_sample(const Image &image,float u,float v,bool srgb,bool premultiply) {
  float x=u*image.width-.5f,y=v*image.height-.5f;
  int left=int(std::floor(x)),top=int(std::floor(y));float fx=x-left,fy=y-top;
  std::array<float,4> result{};
  for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){
    int px=std::clamp(left+dx,0,int(image.width)-1),py=std::clamp(top+dy,0,int(image.height)-1);
    auto at=(size_t(py)*image.width+px)*4;float weight=(dx?fx:1-fx)*(dy?fy:1-fy),alpha=image.rgba[at+3]/255.f;
    for(int c=0;c<3;++c){float value=image.rgba[at+c]/255.f;if(srgb)value=srgb_to_linear(value);result[c]+=value*weight*(premultiply?alpha:1);}
    result[3]+=alpha*weight;
  }
  if(premultiply)for(int c=0;c<3;++c)result[c]=result[3]>1e-8f?result[c]/result[3]:0;
  for(auto &value:result)value=std::clamp(value,0.f,1.f);
  return result;
}
float scalar(const std::array<float,4> &value,ProjectionScalar choice) {
  switch(choice){
  case ProjectionScalar::Red:return value[0];
  case ProjectionScalar::Green:return value[1];
  case ProjectionScalar::Blue:return value[2];
  case ProjectionScalar::Alpha:return value[3];
  case ProjectionScalar::Luminance:return value[0]*.2126f+value[1]*.7152f+value[2]*.0722f;
  }
  throw std::invalid_argument("Invalid projection scalar channel");
}
} // namespace
Image decode_projection_png(std::span<const uint8_t> bytes,const ProjectionImageLimits &limits) {
  limits_valid(limits);
  if(bytes.empty()||bytes.size()>limits.max_encoded_bytes)throw std::length_error("Projection PNG exceeds encoded file bounds");
  png_image file{};file.version=PNG_IMAGE_VERSION;
  struct Cleanup{png_image*file;~Cleanup(){png_image_free(file);}}cleanup{&file};
  if(!png_image_begin_read_from_memory(&file,bytes.data(),bytes.size()))throw std::runtime_error("Cannot decode projection PNG: "+std::string(file.message));
  if(!file.width||!file.height||file.width>limits.max_dimension||file.height>limits.max_dimension||uint64_t(file.width)*file.height>limits.max_pixels)
    throw std::length_error("Projection PNG exceeds decoded image bounds");
  file.format=PNG_FORMAT_RGBA;
  Image image{file.width,file.height,std::vector<uint8_t>(size_t(file.width)*file.height*4)};
  if(!png_image_finish_read(&file,nullptr,image.rgba.data(),0,nullptr))throw std::runtime_error("Cannot read projection PNG pixels: "+std::string(file.message));
  image.validate();return image;
}
Image load_projection_png(const std::filesystem::path &path,const ProjectionImageLimits &limits) {
  limits_valid(limits);
  int fd=open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NONBLOCK);
  if(fd<0)throw std::runtime_error("Cannot open projection image");
  struct Close{int fd;~Close(){close(fd);}}closer{fd};
  struct stat st{};if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<=0||uint64_t(st.st_size)>limits.max_encoded_bytes)
    throw std::runtime_error("Projection image must be a bounded regular PNG file");
  std::vector<uint8_t> bytes(size_t(st.st_size));size_t done=0;
  while(done<bytes.size()){
    auto n=read(fd,bytes.data()+done,bytes.size()-done);
    if(n<0&&errno==EINTR)continue;
    if(n<=0)throw std::runtime_error("Projection image changed or could not be read");
    done+=size_t(n);
  }
  uint8_t extra;ssize_t count;
  do{count=read(fd,&extra,1);}while(count<0&&errno==EINTR);
  if(count!=0)throw std::runtime_error("Projection image changed while loading");
  return decode_projection_png(bytes,limits);
}
struct PaintProjection::Impl {
  std::shared_ptr<const Image> image;
  ProjectionCamera camera;
  ProjectionOptions options;
  Vec3 origin,right,up,back;
  float half_height=1;
  std::shared_ptr<const SurfaceQuery> query;
  Impl(Image input,ProjectionCamera view,const Mesh&surface,ProjectionOptions opts)
      :image(std::make_shared<const Image>(std::move(input))),camera(view),options(std::move(opts)){
    configure();
    if(options.visibility)query=std::make_shared<SurfaceQuery>(surface,options.cancelled);
    cancel(options);
  }
  void configure(){
    cancel(options);image->validate();validate_transform(camera.pose);
    if(int(camera.mode)<0||int(camera.mode)>1||int(options.usage)<0||int(options.usage)>1||
       int(options.color_space)<0||int(options.color_space)>2||int(options.scalar)<0||int(options.scalar)>4||
       int(options.alpha)<0||int(options.alpha)>1||
       !std::isfinite(camera.width)||camera.width<=0||camera.width>100||
       !std::isfinite(camera.height)||camera.height<=0||camera.height>100||
       !std::isfinite(camera.aspect)||camera.aspect<.01f||camera.aspect>100||
       !std::isfinite(camera.vertical_fov)||camera.vertical_fov<.01f||camera.vertical_fov>3.0543262f||
       !std::isfinite(camera.near_depth)||camera.near_depth<0||!std::isfinite(camera.far_depth)||camera.far_depth<=camera.near_depth||camera.far_depth>100||
       !unit(options.opacity)||!unit(options.minimum_facing)||!std::isfinite(options.visibility_epsilon)||options.visibility_epsilon<=0||options.visibility_epsilon>1||
       !options.max_samples_per_stamp||options.max_samples_per_stamp>4'194'304)
      throw std::invalid_argument("Invalid image projection camera or bounds");
    for(float v:options.uv_transform)if(!std::isfinite(v)||std::abs(v)>100)throw std::invalid_argument("Invalid stencil UV transform");
    auto&m=options.uv_transform;double scale=std::hypot(double(m[0]),double(m[1]))*std::hypot(double(m[2]),double(m[3]));if(!scale||std::abs(double(m[0])*m[3]-double(m[1])*m[2])<=scale*1e-12)throw std::invalid_argument("Singular stencil UV transform");
    origin=convert(camera.pose.position);right=convert(ob::rotate(camera.pose.orientation,{1,0,0}));
    up=convert(ob::rotate(camera.pose.orientation,{0,1,0}));back=convert(ob::rotate(camera.pose.orientation,{0,0,1}));
    half_height=std::tan(camera.vertical_fov*.5f);
    cancel(options);
  }
};
PaintProjection::PaintProjection(Image image,ProjectionCamera camera,const Mesh &surface,ProjectionOptions options)
    :impl_(std::make_shared<Impl>(std::move(image),camera,surface,std::move(options))){}
PaintProjection PaintProjection::with_camera(ProjectionCamera camera)const{return with_camera(camera,impl_->options);}
PaintProjection PaintProjection::with_camera(ProjectionCamera camera,ProjectionOptions options)const{
  auto updated=std::make_shared<Impl>(*impl_);updated->camera=camera;updated->options=std::move(options);updated->configure();
  if(updated->options.visibility&&!updated->query)throw std::logic_error("Visibility requires rebuilding the projector with a surface on its asset worker");
  auto result=*this;result.impl_=std::move(updated);return result;
}
uint64_t PaintProjection::max_samples_per_stamp()const{return impl_->options.max_samples_per_stamp;}
std::optional<ProjectedPaint> PaintProjection::sample(Vec3 position,Vec3 normal,const PaintSettings &brush)const{
  const auto &p=*impl_;const auto &o=p.options;cancel(o);brush.validate();
  if(!o.opacity)return {};
  if(!finite(position)||!finite(normal)||!std::isfinite(length(normal))||length(normal)<1e-8f)throw std::invalid_argument("Invalid projected surface sample");
  auto delta=position-p.origin;Vec3 local{dot(delta,p.right)/p.camera.pose.scale,dot(delta,p.up)/p.camera.pose.scale,dot(delta,p.back)/p.camera.pose.scale};
  float depth=-local.z;if(depth<p.camera.near_depth||depth>p.camera.far_depth||(p.camera.mode==ProjectionMode::Perspective&&depth<=1e-8f))return {};
  float half_width=p.camera.width*.5f,half_height=p.camera.height*.5f;
  if(p.camera.mode==ProjectionMode::Perspective){half_height=p.half_height*depth;half_width=half_height*p.camera.aspect;}
  float u=.5f+local.x/(2*half_width),v=.5f-local.y/(2*half_height);
  auto &m=o.uv_transform;float transformed_u=m[0]*u+m[1]*v+m[4],transformed_v=m[2]*u+m[3]*v+m[5];
  if(!std::isfinite(transformed_u)||!std::isfinite(transformed_v)||transformed_u<0||transformed_u>1||transformed_v<0||transformed_v>1)return {};
  auto ray_origin=p.camera.mode==ProjectionMode::Perspective?p.origin:
      p.origin+p.right*(local.x*p.camera.pose.scale)+p.up*(local.y*p.camera.pose.scale)-p.back*(p.camera.near_depth*p.camera.pose.scale);
  auto direction=position-ray_origin;float distance=length(direction);
  auto toward_camera=p.camera.mode==ProjectionMode::Perspective?normalized(p.origin-position):p.back;
  if(o.front_only&&dot(normalized(normal),toward_camera)<=o.minimum_facing)return {};
  if(o.visibility&&p.query){
    auto hit=p.query->raycast(ray_origin,distance>1e-8f?direction:p.back*-1,distance+o.visibility_epsilon);
    if(!hit||std::abs(hit->distance-distance)>o.visibility_epsilon)return {};
  }
  bool srgb=o.color_space==ProjectionColorSpace::SRGB||
      (o.color_space==ProjectionColorSpace::Automatic&&o.usage==ProjectionUsage::Color&&(brush.channel==PaintChannel::Color||brush.channel==PaintChannel::Emission));
  auto value=image_sample(*p.image,transformed_u,transformed_v,srgb,o.alpha==ProjectionAlpha::Coverage);
  float alpha=o.alpha==ProjectionAlpha::Coverage?value[3]:1;
  ProjectedPaint result{brush,o.opacity*alpha};
  float data=scalar(value,o.scalar);
  if(o.usage==ProjectionUsage::Stencil){
    result.opacity=o.opacity*data*(o.scalar==ProjectionScalar::Alpha?1:alpha);
  }else if(brush.channel==PaintChannel::Color){
    for(int c=0;c<3;++c)result.settings.color[c]=std::clamp(value[c]*(o.tint?brush.color[c]:1),0.f,1.f);
    result.settings.color[3]=(o.alpha==ProjectionAlpha::ChannelValue?value[3]:1)*(o.tint?brush.color[3]:1);
  }else if(brush.channel==PaintChannel::Emission){
    for(int c=0;c<3;++c)result.settings.emission[c]=std::clamp(value[c]*(o.tint?brush.emission[c]:1),0.f,1.f);
  }else{
    if(brush.channel==PaintChannel::Metallic)result.settings.metallic=data*(o.tint?brush.metallic:1);
    else if(brush.channel==PaintChannel::Roughness)result.settings.roughness=data*(o.tint?brush.roughness:1);
    else result.settings.mask=data*(o.tint?brush.mask:1);
  }
  result.opacity=std::clamp(result.opacity,0.f,1.f);
  if(!result.opacity)return {};
  return result;
}
void PaintedMesh::stamp_projection(const Brush &brush,const PaintSettings &settings,const PaintProjection &projection) {
  uint64_t samples=0;
  stamp_sampled(brush,settings,[&](Vec3 position,Vec3 normal)->std::optional<PaintSample>{
    if(++samples>projection.max_samples_per_stamp())throw std::length_error("Projection stamp sample budget exceeded");
    auto sample=projection.sample(position,normal,settings);
    if(!sample)return {};
    return PaintSample{sample->settings,sample->opacity};
  });
}
} // namespace dome
