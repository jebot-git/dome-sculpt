// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/normal-preparation.hpp"
#include "dome/humanoid-creator.hpp"
#include "paint-normal-access.hpp"
#include "normal-frame-runtime.hpp"
#include "normal-projection-identity.hpp"
#include "source-paint-pairing.hpp"
#include <mutex>
#include <deque>
#include <algorithm>
#include <bit>
namespace dome {namespace {
uint64_t bounded(const nlohmann::json &j,uint64_t cap){if((!j.is_number_unsigned()&&(!j.is_number_integer()||j.get<int64_t>()<0))||j.get<uint64_t>()>cap)throw std::invalid_argument("Normal UV integer exceeds its declared range");return j.get<uint64_t>();}
bool same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
template<class T>struct Weak {std::weak_ptr<const T> owner;bool present;explicit Weak(const std::shared_ptr<const T>&p):owner(p),present(bool(p)){}bool matches(const std::shared_ptr<const T>&p)const{return present==bool(p)&&(!present||(owner.lock()==p&&!owner.owner_before(p)&&!p.owner_before(owner)));}};
struct UvProof {Weak<PaintedMesh> paint;Weak<SourceMaterialBindings> material;Weak<PaintNormalProjection> projection;Weak<SourceNormalUvLayout> layout;Weak<CreatorAppearance> appearance;Weak<DeformationState> deformation;nlohmann::json receipt;};
std::mutex proof_mutex;std::deque<UvProof> proofs;
}
void NormalUvReprojectionReceipt::validate()const{
 if(version!=1||resolution<64||resolution>2048||(resolution&(resolution-1))||!std::isfinite(accepted_sample_error)||accepted_sample_error<0||accepted_sample_error>.25||!std::isfinite(observed_sample_error)||observed_sample_error<0||observed_sample_error>accepted_sample_error||!sampled_texels||sampled_texels>size_t(resolution)*resolution||!source_constraints||source_constraints>raster_budget||!raster_samples||raster_samples>raster_budget||!raster_budget||raster_budget>64'000'000||!max_working_bytes||max_working_bytes>512*1024*1024)
  throw std::invalid_argument("Invalid normal UV sampled-quality/resource receipt");
}
nlohmann::json NormalUvReprojectionReceipt::encode()const{validate();return {{"version",version},{"resolution",resolution},{"accepted_sample_error",accepted_sample_error},{"observed_sample_error",observed_sample_error},{"sampled_texels",sampled_texels},{"source_constraints",source_constraints},{"raster_samples",raster_samples},{"raster_budget",raster_budget},{"max_working_bytes",max_working_bytes}};}
NormalUvReprojectionReceipt NormalUvReprojectionReceipt::decode(const nlohmann::json &j){if(!j.is_object()||j.size()!=9||bounded(j.at("version"),1)!=1)throw std::invalid_argument("Unsupported normal UV receipt");NormalUvReprojectionReceipt out;out.resolution=uint32_t(bounded(j.at("resolution"),2048));out.accepted_sample_error=j.at("accepted_sample_error").get<double>();out.observed_sample_error=j.at("observed_sample_error").get<double>();out.sampled_texels=bounded(j.at("sampled_texels"),4'194'304);out.source_constraints=bounded(j.at("source_constraints"),64'000'000);out.raster_samples=bounded(j.at("raster_samples"),64'000'000);out.raster_budget=bounded(j.at("raster_budget"),64'000'000);out.max_working_bytes=bounded(j.at("max_working_bytes"),512*1024*1024);out.validate();return out;}
size_t SourceNormalUvLayout::storage_bytes()const{
 if(source_vertices.empty()||source_vertices.size()>2'000'000||indices.empty()||indices.size()>6'000'000||indices.size()%3||uv.size()!=source_vertices.size()||(!pins.empty()&&pins.size()!=uv.size())||(!seams.empty()&&seams.size()!=indices.size()))throw std::invalid_argument("Invalid normal UV layout dimensions");
 return source_vertices.size()*sizeof(uint32_t)+indices.size()*sizeof(uint32_t)+uv.size()*sizeof(std::array<float,2>)+pins.size()+seams.size();
}
Mesh SourceNormalUvLayout::target_mesh(const Mesh &before,const std::function<bool()> &cancelled)const{
 storage_bytes();before.validate();if(indices.size()!=before.indices.size())throw std::invalid_argument("Normal UV layout changed triangle count");Mesh out;out.uv=uv;out.indices=indices;out.uv_pins=pins;out.uv_seams=seams;out.positions.reserve(uv.size());out.normals.reserve(uv.size());
 for(size_t n=0;n<uv.size();++n){if((n&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Normal UV layout construction cancelled");auto old=source_vertices[n];if(old>=before.positions.size())throw std::invalid_argument("Normal UV layout source vertex is unavailable");for(float v:uv[n])if(!std::isfinite(v)||v<0||v>1)throw std::invalid_argument("Normal UV target lies outside its unit atlas");out.positions.push_back(before.positions[old]);out.normals.push_back(before.normals[old]);}
 out.validate();for(size_t c=0;c<indices.size();++c){if((c&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Normal UV corner validation cancelled");if(source_vertices[indices[c]]!=before.indices[c])throw std::invalid_argument("Normal UV layout does not own the exact ordered source corner");}return out;
}
nlohmann::json SourceNormalUvLayout::encode()const{
 storage_bytes();auto words=[](const auto &values){std::vector<uint8_t> bytes;bytes.reserve(values.size()*4);for(uint32_t n:values)for(unsigned k=0;k<4;++k)bytes.push_back(uint8_t(n>>(k*8)));return nlohmann::json::binary(std::move(bytes));};
 std::vector<uint32_t> coordinates;coordinates.reserve(uv.size()*2);for(auto q:uv)for(float n:q){if(!std::isfinite(n)||n<0||n>1)throw std::invalid_argument("Invalid normal UV encoded coordinate");coordinates.push_back(std::bit_cast<uint32_t>(n));}
 return {{"version",1},{"source_vertices",words(source_vertices)},{"indices",words(indices)},{"uv",words(coordinates)},{"pins",nlohmann::json::binary(pins)},{"seams",nlohmann::json::binary(seams)}};
}
size_t SourceNormalUvLayout::encoded_storage_bytes(const nlohmann::json &j){
 if(!j.is_object()||j.size()!=6||bounded(j.at("version"),1)!=1)throw std::invalid_argument("Unsupported normal UV layout");const auto &v=j.at("source_vertices"),&i=j.at("indices"),&u=j.at("uv"),&p=j.at("pins"),&s=j.at("seams");
 if(!v.is_binary()||!i.is_binary()||!u.is_binary()||!p.is_binary()||!s.is_binary())throw std::invalid_argument("Normal UV layout buffers require bounded LE binary storage");
 const size_t vertices=v.get_binary().size()/4,indices=i.get_binary().size()/4;
 if(!vertices||vertices>2'000'000||v.get_binary().size()!=vertices*4||!indices||indices>6'000'000||indices%3||i.get_binary().size()!=indices*4||u.get_binary().size()!=vertices*8||(!p.get_binary().empty()&&p.get_binary().size()!=vertices)||(!s.get_binary().empty()&&s.get_binary().size()!=indices))throw std::invalid_argument("Normal UV layout buffers exceed their dimension bounds");
 return v.get_binary().size()+i.get_binary().size()+u.get_binary().size()+p.get_binary().size()+s.get_binary().size();
}
std::shared_ptr<const SourceNormalUvLayout> SourceNormalUvLayout::decode(const nlohmann::json &j){
 encoded_storage_bytes(j);const auto &v=j.at("source_vertices").get_binary(),&i=j.at("indices").get_binary(),&u=j.at("uv").get_binary(),&p=j.at("pins").get_binary(),&s=j.at("seams").get_binary();
 auto word=[](const auto &bytes,size_t at){return uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8|uint32_t(bytes[at+2])<<16|uint32_t(bytes[at+3])<<24;};auto out=std::make_shared<SourceNormalUvLayout>();out->source_vertices.reserve(v.size()/4);out->indices.reserve(i.size()/4);out->uv.reserve(v.size()/4);
 for(size_t at=0;at<v.size();at+=4)out->source_vertices.push_back(word(v,at));for(size_t at=0;at<i.size();at+=4){auto n=word(i,at);if(n>=v.size()/4)throw std::invalid_argument("Normal UV layout packed index is out of range");out->indices.push_back(n);}for(size_t at=0;at<u.size();at+=8){std::array<float,2> q{std::bit_cast<float>(word(u,at)),std::bit_cast<float>(word(u,at+4))};for(float n:q)if(!std::isfinite(n)||n<0||n>1)throw std::invalid_argument("Invalid bounded normal UV coordinate");out->uv.push_back(q);}for(auto n:p){if(n>1)throw std::invalid_argument("Invalid normal UV pin flag");out->pins.push_back(n);}for(auto n:s){if(n>1)throw std::invalid_argument("Invalid normal UV seam flag");out->seams.push_back(n);}out->storage_bytes();return out;
}
void SourceNormalUvReprojection::validate(bool deep,const std::function<bool()> &cancelled)const{
 auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Normal UV source validation cancelled");};stop();receipt.validate();
 if(!layout||!projection||!before_paint||before_paint->in_stroke()||!before_material||!before_material->normal_preparation||!before_paint->normal_projection()||before_paint->surface_diagnostics().policy==PaintUvPolicy::PolygonAuthored)throw std::invalid_argument("Normal UV projection requires its own complete prepared before-state");
 if(before_paint->resolution()!=receipt.resolution)throw std::invalid_argument("Normal UV resolution differs from its before-state");layout->storage_bytes();projection->validate(receipt.resolution);before_material->validate(deep);detail::source_paint_pairing(before_paint.get(),before_material.get(),before_appearance.get(),deep);
 if(before_material->has_mtoon){const auto &normal=before_material->roles[size_t(MToonSlot::Normal)];if(normal.wrap_s!=33071||normal.wrap_t!=33071||normal.min_filter!=9729||normal.mag_filter!=9729||normal.image!=before_paint->normal_projection()->mtoon)throw std::invalid_argument("Normal UV reprojection requires its declared linear/clamped prepared sampling input");}
 if(!deep)return;const auto metadata=receipt.encode();{std::lock_guard lock(proof_mutex);for(const auto &p:proofs)if(p.paint.matches(before_paint)&&p.material.matches(before_material)&&p.projection.matches(projection)&&p.layout.matches(layout)&&p.appearance.matches(before_appearance)&&p.deformation.matches(before_deformation)&&p.receipt==metadata)return;}
 detail::normal_frame_current_authority(*before_material,before_paint->mesh(),before_deformation.get(),cancelled);const size_t mesh_bytes=layout->uv.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>))+layout->indices.size()*4+layout->pins.size()+layout->seams.size();if(mesh_bytes>=receipt.max_working_bytes)throw std::length_error("Normal UV replay mesh exceeds working budget");auto target=layout->target_mesh(before_paint->mesh(),cancelled);NormalPreparationOptions options;options.cancelled=cancelled;options.max_angular_error=receipt.accepted_sample_error;options.max_samples=receipt.raster_budget;options.max_working_bytes=size_t(receipt.max_working_bytes)-mesh_bytes;auto expected=SourceNormalPreparationAccess::reproject(*before_paint,target,layout->source_vertices,options);
 const auto same_pixels=[](const Image &a,const Image &b){return a.width==b.width&&a.height==b.height&&a.rgba==b.rgba;};
 if(!same_pixels(*expected.projection->pbr,*projection->pbr)||!same_pixels(*expected.projection->mtoon,*projection->mtoon))throw std::invalid_argument("Normal UV plane bytes differ from exact owning source replay");
 if(expected.observed_sample_error!=receipt.observed_sample_error||expected.sampled_texels!=receipt.sampled_texels||expected.source_constraints!=receipt.source_constraints||expected.raster_samples!=receipt.raster_samples)throw std::invalid_argument("Normal UV sampled quality differs from exact owning source replay");
 try{std::lock_guard lock(proof_mutex);if(proofs.size()>=64)proofs.pop_front();proofs.push_back({Weak(before_paint),Weak(before_material),Weak(projection),Weak(layout),Weak(before_appearance),Weak(before_deformation),metadata});}catch(const std::bad_alloc&){}stop();
}
void SourceNormalUvReprojection::validate_current(const Mesh &mesh,const CreatorAppearance *appearance,const std::function<bool()> &cancelled)const{
 if(!layout||!before_paint)throw std::invalid_argument("Normal UV current layout lost its owning before-state");layout->storage_bytes();mesh.validate();const auto &before=before_paint->mesh();
 if(mesh.positions.size()!=layout->source_vertices.size()||mesh.indices!=layout->indices||mesh.uv!=layout->uv||mesh.uv_pins!=layout->pins||mesh.uv_seams!=layout->seams||bool(appearance)!=bool(before_appearance))throw std::invalid_argument("Current normal UV topology/sampling authority differs");
 if(appearance&&(appearance->asset!=before_appearance->asset||appearance->source_mesh!=before_appearance->source_mesh||appearance->source_primitive!=before_appearance->source_primitive||appearance->source_vertices.size()!=mesh.positions.size()))throw std::invalid_argument("Current normal UV original primitive identity differs");
 for(size_t v=0;v<mesh.positions.size();++v){if((v&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Normal UV current validation cancelled");auto old=layout->source_vertices[v];if(old>=before.positions.size()||!same(mesh.positions[v],before.positions[old])||!same(mesh.normals[v],before.normals[old])||(appearance&&appearance->source_vertices[v]!=before_appearance->source_vertices.at(old)))throw std::invalid_argument("Current normal UV REST/source mapping differs");}
}
} // namespace dome
