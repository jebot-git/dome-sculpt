// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/color-preparation.hpp"
#include "dome/polygon-color.hpp"
#include "paint-color-access.hpp"
#include "polygon-color-target.hpp"
#include "source-paint-pairing.hpp"
#include <bit>
#include <algorithm>
namespace dome {namespace {
uint64_t bounded(const nlohmann::json &j,uint64_t cap){if((!j.is_number_unsigned()&&(!j.is_number_integer()||j.get<int64_t>()<0))||j.get<uint64_t>()>cap)throw std::invalid_argument("Color UV integer exceeds its declared range");return j.get<uint64_t>();}
bool same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
}
size_t SourceColorUvLayout::storage_bytes()const{
 if(source_vertices.empty()||source_vertices.size()>2'000'000||indices.empty()||indices.size()>6'000'000||indices.size()%3||uv.size()!=source_vertices.size()||(!pins.empty()&&pins.size()!=uv.size())||(!seams.empty()&&seams.size()!=indices.size()))throw std::invalid_argument("Invalid color UV layout dimensions");
 return source_vertices.size()*sizeof(uint32_t)+indices.size()*sizeof(uint32_t)+uv.size()*sizeof(std::array<float,2>)+pins.size()+seams.size();
}
Mesh SourceColorUvLayout::target_mesh(const Mesh &before,const std::function<bool()> &cancelled)const{
 storage_bytes();before.validate();if(indices.size()!=before.indices.size())throw std::invalid_argument("Color UV layout changed triangle count");Mesh out;out.uv=uv;out.indices=indices;out.uv_pins=pins;out.uv_seams=seams;out.positions.reserve(uv.size());out.normals.reserve(uv.size());
 for(size_t n=0;n<uv.size();++n){if((n&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Color UV layout construction cancelled");auto old=source_vertices[n];if(old>=before.positions.size())throw std::invalid_argument("Color UV layout source vertex is unavailable");for(float v:uv[n])if(!std::isfinite(v)||v<0||v>1)throw std::invalid_argument("Color UV target lies outside its unit atlas");out.positions.push_back(before.positions[old]);out.normals.push_back(before.normals[old]);}
 out.validate();for(size_t c=0;c<indices.size();++c){if((c&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Color UV corner validation cancelled");if(source_vertices[indices[c]]!=before.indices[c])throw std::invalid_argument("Color UV layout does not own the exact ordered source corner");}return out;
}
nlohmann::json SourceColorUvLayout::encode()const{
 storage_bytes();auto words=[](const auto &values){std::vector<uint8_t> bytes;bytes.reserve(values.size()*4);for(uint32_t n:values)for(unsigned k=0;k<4;++k)bytes.push_back(uint8_t(n>>(k*8)));return nlohmann::json::binary(std::move(bytes));};
 std::vector<uint32_t> coordinates;coordinates.reserve(uv.size()*2);for(auto q:uv)for(float n:q){if(!std::isfinite(n)||n<0||n>1)throw std::invalid_argument("Invalid color UV encoded coordinate");coordinates.push_back(std::bit_cast<uint32_t>(n));}
 return {{"version",1},{"source_vertices",words(source_vertices)},{"indices",words(indices)},{"uv",words(coordinates)},{"pins",nlohmann::json::binary(pins)},{"seams",nlohmann::json::binary(seams)}};
}
size_t SourceColorUvLayout::encoded_storage_bytes(const nlohmann::json &j){
 if(!j.is_object()||j.size()!=6||bounded(j.at("version"),1)!=1)throw std::invalid_argument("Unsupported color UV layout");const auto &v=j.at("source_vertices"),&i=j.at("indices"),&u=j.at("uv"),&p=j.at("pins"),&s=j.at("seams");
 if(!v.is_binary()||!i.is_binary()||!u.is_binary()||!p.is_binary()||!s.is_binary())throw std::invalid_argument("Color UV layout buffers require bounded LE binary storage");
 const size_t vertices=v.get_binary().size()/4,indices=i.get_binary().size()/4;
 if(!vertices||vertices>2'000'000||v.get_binary().size()!=vertices*4||!indices||indices>6'000'000||indices%3||i.get_binary().size()!=indices*4||u.get_binary().size()!=vertices*8||(!p.get_binary().empty()&&p.get_binary().size()!=vertices)||(!s.get_binary().empty()&&s.get_binary().size()!=indices))throw std::invalid_argument("Color UV layout buffers exceed their dimension bounds");
 return v.get_binary().size()+i.get_binary().size()+u.get_binary().size()+p.get_binary().size()+s.get_binary().size();
}
std::shared_ptr<const SourceColorUvLayout> SourceColorUvLayout::decode(const nlohmann::json &j){
 encoded_storage_bytes(j);const auto &v=j.at("source_vertices").get_binary(),&i=j.at("indices").get_binary(),&u=j.at("uv").get_binary(),&p=j.at("pins").get_binary(),&s=j.at("seams").get_binary();
 auto word=[](const auto &bytes,size_t at){return uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8|uint32_t(bytes[at+2])<<16|uint32_t(bytes[at+3])<<24;};auto out=std::make_shared<SourceColorUvLayout>();out->source_vertices.reserve(v.size()/4);out->indices.reserve(i.size()/4);out->uv.reserve(v.size()/4);
 for(size_t at=0;at<v.size();at+=4)out->source_vertices.push_back(word(v,at));for(size_t at=0;at<i.size();at+=4){auto n=word(i,at);if(n>=v.size()/4)throw std::invalid_argument("Color UV layout packed index is out of range");out->indices.push_back(n);}for(size_t at=0;at<u.size();at+=8){std::array<float,2> q{std::bit_cast<float>(word(u,at)),std::bit_cast<float>(word(u,at+4))};for(float n:q)if(!std::isfinite(n)||n<0||n>1)throw std::invalid_argument("Invalid bounded color UV coordinate");out->uv.push_back(q);}for(auto n:p){if(n>1)throw std::invalid_argument("Invalid color UV pin flag");out->pins.push_back(n);}for(auto n:s){if(n>1)throw std::invalid_argument("Invalid color UV seam flag");out->seams.push_back(n);}out->storage_bytes();return out;
}

void ColorUvReprojectionReceipt::validate()const{
 if(version!=1||resolution<64||resolution>2048||(resolution&(resolution-1))||!std::isfinite(accepted_projection_error)||accepted_projection_error<0||accepted_projection_error>1||!projected_texels||projected_texels>size_t(resolution)*resolution||!tested_quality_cells||tested_quality_cells>max_quality_cells||!max_quality_cells||max_quality_cells>1'000'000'000||!raster_samples||raster_samples>raster_budget||!raster_budget||raster_budget>512'000'000||!max_working_bytes||max_working_bytes>512*1024*1024)
  throw std::invalid_argument("Invalid current-colour UV quality/resource receipt");
 for(float error:projection_error_bound)if(!std::isfinite(error)||error<0||error>accepted_projection_error)throw std::invalid_argument("Current-colour UV projection exceeds its own accepted quality bound");
}
nlohmann::json ColorUvReprojectionReceipt::encode()const{validate();return {{"version",version},{"resolution",resolution},{"accepted_projection_error",accepted_projection_error},{"projection_error_bound",projection_error_bound},{"projected_texels",projected_texels},{"tested_quality_cells",tested_quality_cells},{"raster_samples",raster_samples},{"raster_budget",raster_budget},{"max_quality_cells",max_quality_cells},{"max_working_bytes",max_working_bytes}};}
ColorUvReprojectionReceipt ColorUvReprojectionReceipt::decode(const nlohmann::json &j){if(!j.is_object()||j.size()!=10||bounded(j.at("version"),1)!=1)throw std::invalid_argument("Unsupported current-colour UV receipt");ColorUvReprojectionReceipt out;out.resolution=uint32_t(bounded(j.at("resolution"),2048));out.accepted_projection_error=j.at("accepted_projection_error").get<float>();out.projection_error_bound=j.at("projection_error_bound").get<std::array<float,4>>();out.projected_texels=bounded(j.at("projected_texels"),4'194'304);out.tested_quality_cells=bounded(j.at("tested_quality_cells"),1'000'000'000);out.raster_samples=bounded(j.at("raster_samples"),512'000'000);out.raster_budget=bounded(j.at("raster_budget"),512'000'000);out.max_quality_cells=bounded(j.at("max_quality_cells"),1'000'000'000);out.max_working_bytes=bounded(j.at("max_working_bytes"),512*1024*1024);out.validate();return out;}
void SourceColorUvReprojection::validate(bool deep,const std::function<bool()> &cancelled)const{
 auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Current-colour UV source validation cancelled");};stop();receipt.validate();
 if(!layout||!projection||!polygon_colors||!polygon_colors->topology||!polygon_colors->field||!before_paint||before_paint->in_stroke()||before_paint->surface_diagnostics().policy==PaintUvPolicy::PolygonAuthored||!before_paint->color_projection()||!before_material||!before_material->color_preparation||before_appearance||before_paint->normal_projection()||before_material->normal_preparation)
  throw std::invalid_argument("Current-colour UV requires its own complete strictly prepared before-state; unrelated PA/creator/normal authority cannot substitute");
 const auto &prior=*before_material->color_preparation;const auto &old_descriptor=prior.current_descriptor();
 if(prior.receipt.source_field!=SourceColorField::PolygonCorners||!old_descriptor||!old_descriptor->topology||!old_descriptor->field||old_descriptor->field!=polygon_colors->field||old_descriptor->field.owner_before(polygon_colors->field)||polygon_colors->field.owner_before(old_descriptor->field))
  throw std::invalid_argument("UV-only editing must retain the actual independently owned current corner field");
 if(before_paint->resolution()!=receipt.resolution||receipt.accepted_projection_error>prior.receipt.accepted_projection_error)
  throw std::invalid_argument("Current-colour UV resolution or accepted quality changed without its own policy");
 layout->storage_bytes();projection->validate(receipt.resolution);before_material->validate(deep);detail::source_paint_pairing(before_paint.get(),before_material.get(),before_appearance.get(),deep);
 if(projection->maximum_error!=receipt.projection_error_bound||projection->quality_cells!=receipt.tested_quality_cells)
  throw std::invalid_argument("Current-colour UV pixels lack their own measured quality receipt");
 if(!deep)return;
 if(before_deformation)before_deformation->validate(before_paint->mesh());
 auto old_shape=old_descriptor->topology->encode(),new_shape=polygon_colors->topology->encode();auto remove_uv=[](auto &j){for(auto &face:j.at("faces"))for(auto &corner:face.at("corners"))corner[2]=std::array<float,2>{0,0};};remove_uv(old_shape);remove_uv(new_shape);
 if(old_shape!=new_shape)throw std::invalid_argument("Current-colour UV changed stable geometric/corner/edge identities");
 auto target=layout->target_mesh(before_paint->mesh(),cancelled);auto colors=detail::polygon_target_colors(target,*polygon_colors,cancelled);
 ColorPreparationOptions options;options.cancelled=cancelled;options.max_projection_error=receipt.accepted_projection_error;options.max_quality_cells=receipt.max_quality_cells;options.max_working_bytes=size_t(receipt.max_working_bytes);
 auto expected=SourceColorPreparationAccess::project_uv(*before_paint,target,colors,options,receipt.raster_budget);
 if(expected.projected_texels!=receipt.projected_texels||expected.raster_samples!=receipt.raster_samples||expected.projection->maximum_error!=projection->maximum_error||expected.projection->quality_cells!=projection->quality_cells||expected.projection->vertex_rgba->width!=projection->vertex_rgba->width||expected.projection->vertex_rgba->height!=projection->vertex_rgba->height||expected.projection->vertex_rgba->rgba!=projection->vertex_rgba->rgba)
  throw std::invalid_argument("Current-colour UV projection/quality differs from exact owning source replay");stop();
}
void SourceColorUvReprojection::validate_current(const Mesh &mesh,const CreatorAppearance *appearance,const std::function<bool()> &cancelled)const{
 if(!layout||!before_paint||!polygon_colors||appearance)throw std::invalid_argument("Current-colour UV lost its own current geometry/source authority");
 layout->storage_bytes();mesh.validate();const auto &before=before_paint->mesh();
 if(mesh.positions.size()!=layout->source_vertices.size()||mesh.indices!=layout->indices||mesh.uv!=layout->uv||mesh.uv_pins!=layout->pins||mesh.uv_seams!=layout->seams)
  throw std::invalid_argument("Current-colour UV sampling/corner topology differs");
 for(size_t v=0;v<mesh.positions.size();++v){if((v&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Current-colour UV current validation cancelled");auto old=layout->source_vertices[v];if(old>=before.positions.size()||!same(mesh.positions[v],before.positions[old])||!same(mesh.normals[v],before.normals[old]))throw std::invalid_argument("Current-colour UV REST/source corner mapping differs");}
 detail::polygon_target_colors(mesh,*polygon_colors,cancelled);
}

} // namespace dome
