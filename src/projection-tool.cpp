// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/projection-tool.hpp"
#include "text-utf8.hpp"
#include <algorithm>
#include <cctype>
namespace dome {
void ProjectionToolSettings::validate() const {
  if(image.cancelled)throw std::invalid_argument("Projection tool settings cannot retain a worker callback");
  validate_projection_parameters(camera,image);
  if(int(occlusion)<0||int(occlusion)>1)throw std::invalid_argument("Invalid projection occlusion choice");
  if(!std::isfinite(preview_depth)||preview_depth<=0||preview_depth>100)
    throw std::invalid_argument("Projection preview distance must be positive and at most 100 metres");
}
std::vector<ProjectionEntry> scan_projection_directory(const std::filesystem::path &path,
    const std::function<bool()> &cancelled) {
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Projection image browsing cancelled");};stop();
  std::vector<ProjectionEntry> result;size_t seen=0;
  std::error_code error;
  std::filesystem::directory_iterator entries(path,error);
  if(error)throw std::runtime_error("Cannot open image directory: "+error.message());
  for(const auto &entry:entries) {
    stop();if(++seen>8192)throw std::length_error("Image directory exceeds 8192 entries; choose a smaller directory");
    auto name=entry.path().filename().string();
    if(name.size()>1024||!text::valid_utf8(name))continue;
    auto directory=entry.is_directory(error);if(error){error.clear();continue;}
    if(!directory) {
      if(!entry.is_regular_file(error)||error){error.clear();continue;}
      auto extension=entry.path().extension().string();
      for(char &c:extension)c=char(std::tolower(static_cast<unsigned char>(c)));
      if(extension!=".png")continue;
    }
    if(result.size()>=512)throw std::length_error("More than 512 image/directory entries; choose a smaller directory");
    result.push_back({std::move(name),directory});
  }
  stop();std::sort(result.begin(),result.end(),[](const auto &a,const auto &b){
    return a.directory!=b.directory?a.directory>b.directory:a.name<b.name;
  });return result;
}

std::string projection_parameter_text(const ProjectionToolSettings &s,size_t i) {
  const auto &c=s.camera;const auto &o=s.image;
  auto number=[](const char *label,float value){return std::string(label)+std::to_string(value);};
  switch(i){
  case 0:return number("Opacity ",o.opacity);case 1:return c.mode==ProjectionMode::Orthographic?"Orthographic":"Perspective";
  case 2:return number("Width ",c.width);case 3:return number("Height ",c.height);
  case 4:return number("Field of view ",c.vertical_fov*57.29578f);case 5:return number("Aspect ",c.aspect);
  case 6:return number("Near ",c.near_depth);case 7:return number("Far ",c.far_depth);case 8:return number("Preview distance ",s.preview_depth);
  case 9:return o.front_only?"Front faces only":"Both face directions";case 10:return o.visibility?"Check visibility":"Through surfaces";
  case 11:return s.occlusion==ProjectionOcclusion::VisibleScene?"Visible scene occludes":"Target only occludes";
  case 12:return std::string("Color space: ")+std::array{"Automatic","sRGB","Linear"}[int(o.color_space)];
  case 13:return std::string("Scalar: ")+std::array{"Luminance","Red","Green","Blue","Alpha"}[int(o.scalar)];
  case 14:return o.alpha==ProjectionAlpha::Coverage?"Alpha as coverage":"Alpha as channel value";
  case 15:return o.tint?"Tint with brush":"Use image color";
  case 16:return number("U scale ",std::hypot(o.uv_transform[0],o.uv_transform[1]));
  case 17:return number("V scale ",std::hypot(o.uv_transform[2],o.uv_transform[3]));
  case 18:return number("Image rotation ",std::atan2(o.uv_transform[2],o.uv_transform[0])*57.29578f);
  case 19:return number("U offset ",o.uv_transform[4]);case 20:return number("V offset ",o.uv_transform[5]);
  case 21:return s.preview?"Projector preview on":"Projector preview off";
  case 22:return number("Facing threshold ",o.minimum_facing);case 23:return number("Visibility tolerance ",o.visibility_epsilon);
  case 24:return "Flip image horizontally";case 25:return "Flip image vertically";
  default:throw std::invalid_argument("Unknown projection parameter");
  }
}
void adjust_projection_parameter(ProjectionToolSettings &s,size_t i,int direction){
  if(direction!=1&&direction!=-1)throw std::invalid_argument("Projection adjustment requires a direction");
  auto &c=s.camera;auto &o=s.image;const float factor=direction>0?1.1f:1/1.1f;
  auto scaled=[&](float &v,float lo,float hi){v=std::clamp(v*factor,lo,hi);};
  switch(i){
  case 0:o.opacity=std::clamp(o.opacity+direction*.05f,0.f,1.f);break;
  case 1:c.mode=c.mode==ProjectionMode::Orthographic?ProjectionMode::Perspective:ProjectionMode::Orthographic;break;
  case 2:scaled(c.width,.001f,100);break;case 3:scaled(c.height,.001f,100);break;
  case 4:scaled(c.vertical_fov,.01f,3.0543262f);break;case 5:scaled(c.aspect,.01f,100);break;
  case 6:c.near_depth=std::clamp(c.near_depth+direction*.001f,0.f,c.far_depth*.99f);break;
  case 7:scaled(c.far_depth,std::max(.001f,c.near_depth*1.01f),100);break;
  case 8:scaled(s.preview_depth,.001f,100);break;case 9:o.front_only=!o.front_only;break;case 10:o.visibility=!o.visibility;break;
  case 11:s.occlusion=s.occlusion==ProjectionOcclusion::VisibleScene?ProjectionOcclusion::TargetOnly:ProjectionOcclusion::VisibleScene;break;
  case 12:o.color_space=ProjectionColorSpace((int(o.color_space)+3+direction)%3);break;
  case 13:o.scalar=ProjectionScalar((int(o.scalar)+5+direction)%5);break;
  case 14:o.alpha=o.alpha==ProjectionAlpha::Coverage?ProjectionAlpha::ChannelValue:ProjectionAlpha::Coverage;break;
  case 15:o.tint=!o.tint;break;
  case 16:case 17:{const size_t base=(i-16)*2;const float n=std::hypot(o.uv_transform[base],o.uv_transform[base+1]);const float f=std::clamp(n*factor,.01f,20.f)/n;for(size_t k=base;k<base+2;++k)o.uv_transform[k]*=f;break;}
  case 18:{auto m=o.uv_transform;const float a=direction*.08726646f,cs=std::cos(a),sn=std::sin(a);o.uv_transform[0]=cs*m[0]-sn*m[2];o.uv_transform[1]=cs*m[1]-sn*m[3];o.uv_transform[2]=sn*m[0]+cs*m[2];o.uv_transform[3]=sn*m[1]+cs*m[3];break;}
  case 19:case 20:o.uv_transform[i-15]=std::clamp(o.uv_transform[i-15]+direction*.05f,-20.f,20.f);break;
  case 21:s.preview=!s.preview;break;case 22:o.minimum_facing=std::clamp(o.minimum_facing+direction*.05f,0.f,1.f);break;
  case 23:scaled(o.visibility_epsilon,.000001f,.1f);break;
  case 24:case 25:{const size_t row=(i-24)*2;for(size_t k=row;k<row+2;++k)o.uv_transform[k]*=-1;o.uv_transform[4+i-24]=1-o.uv_transform[4+i-24];break;}
  default:throw std::invalid_argument("Unknown projection parameter");
  }
  s.validate();
}
std::string projection_tool_label(const ProjectionView *v,int sector){
  static const std::array<const char*,12> labels={"Browse images","Previous","Next","Options","Preview","Use image","Color / stencil","Place projector","Back","Color presets","Decrease","Increase"};
  if(sector<0||sector>=12)return {};
  if(!v)return labels[sector];
  if(sector==7&&(v->loading||v->browsing))return v->loading?"Cancel image load":"Cancel browsing";
  if(v->panel==ProjectionPanel::Browser){if(sector==3)return "Open / load";if(sector==4)return "Parent folder";}
  if(sector==3&&v->panel==ProjectionPanel::Parameters)return "Image tools";
  if(sector==5)return v->settings.enabled?"Image enabled":"Image disabled";
  if(sector==6)return v->settings.image.usage==ProjectionUsage::Color?"Color image":"Stencil image";
  if(sector==7&&v->placing)return "Finish placement";
  return labels[sector];
}
} // namespace dome
