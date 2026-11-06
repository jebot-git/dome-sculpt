// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/paint-sampling.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace dome {namespace {
struct Budget {
 const PaintSamplingLimits &limits;uint64_t used=0;
 explicit Budget(const PaintSamplingLimits &l):limits(l){
  if(!l.max_vertices||l.max_vertices>2'000'000||!l.max_triangles||l.max_triangles>2'000'000||!l.max_basis_bytes||l.max_basis_bytes>128*1024*1024||!l.max_working_bytes||l.max_working_bytes>512*1024*1024||!l.max_work||l.max_work>256'000'000)throw std::invalid_argument("Invalid writable paint-domain resource limits");stop();
 }
 void stop(){if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Writable paint-domain operation cancelled");}
 void work(uint64_t n=1){if(n>limits.max_work-used)throw std::length_error("Writable paint-domain work budget exceeded");used+=n;if((used&1023)==0)stop();}
 void bytes(size_t total)const{if(total>limits.max_working_bytes)throw std::length_error("Writable paint-domain working bytes exceeded");}
};
bool same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
size_t mesh_bytes(const Mesh &m,Budget &budget,bool paint){
 if(m.positions.empty()||m.positions.size()>budget.limits.max_vertices||m.indices.empty()||m.indices.size()%3||m.indices.size()/3>budget.limits.max_triangles||m.normals.size()!=m.positions.size()||m.uv.size()!=m.positions.size()||(!m.uv_pins.empty()&&m.uv_pins.size()!=m.positions.size())||(!m.uv_seams.empty()&&m.uv_seams.size()!=m.indices.size()))throw std::invalid_argument("Writable paint-domain mesh dimensions exceed their bounds");
 // Every term is bounded above before multiplication; no unchecked sum can wrap.
 const size_t bytes=m.positions.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>))+m.indices.size()*sizeof(uint32_t)+m.uv_pins.size()+m.uv_seams.size();budget.bytes(bytes);
 budget.work(m.positions.size()+m.indices.size());budget.stop();m.validate();budget.stop();
 if(paint)for(const auto &uv:m.uv){budget.work();if(uv[0]<0||uv[0]>1||uv[1]<0||uv[1]>1)throw std::invalid_argument("Writable paint-domain basis UVs must fit its existing unit grid");}
 return bytes;
}
size_t own_bytes(const PaintSamplingDomain &domain,Budget &budget){
 if((domain.version()!=1&&domain.version()!=3)||!domain.basis()||bool(domain.derived_geometry())!=(domain.version()==3))throw std::invalid_argument("Writable paint-domain requires its immutable geometry basis");
 const auto bytes=mesh_bytes(*domain.basis(),budget,true);
 if(domain.source_corners().size()!=domain.basis()->indices.size())throw std::invalid_argument("Writable paint-domain lost complete source-corner correspondence");
 const auto all=bytes+domain.source_corners().size()*sizeof(uint32_t)+(domain.derived_geometry()?mesh_bytes(*domain.derived_geometry(),budget,false):0);
 if(all>budget.limits.max_basis_bytes)throw std::length_error("Writable paint-domain retained basis exceeds its byte limit");budget.bytes(all);return all;
}
void correspondence(const Mesh &source,const Mesh &current,std::span<const uint32_t> corners,Budget &budget,size_t owned){
 if(corners.size()!=current.indices.size()||corners.size()!=source.indices.size())throw std::invalid_argument("Writable paint-domain changed its complete triangle count");
 const size_t faces=source.indices.size()/3;budget.bytes(owned+faces);std::vector<uint8_t> seen(faces);
 for(size_t c=0;c<corners.size();c+=3){
  budget.work(3);const auto first=corners[c];
  if(first>=source.indices.size()||first%3||corners[c+1]!=first+1||corners[c+2]!=first+2||seen[first/3]++)throw std::invalid_argument("Writable paint-domain source triangles were reversed, duplicated or lost");
  for(size_t k=0;k<3;++k){const auto a=current.indices[c+k],b=source.indices[corners[c+k]];
   if(!same(current.positions[a],source.positions[b])||!same(current.normals[a],source.normals[b]))throw std::invalid_argument("Writable paint-domain requires exact ordered REST corners before pose/world evaluation");
  }
 }
 budget.stop();
}
size_t validate(const PaintSamplingDomain &domain,const Mesh &current,Budget &budget){
 const auto owned=own_bytes(domain,budget);const auto bytes=mesh_bytes(current,budget,false);budget.bytes(owned+bytes+domain.basis()->indices.size()/3);
 if(domain.version()==1)correspondence(*domain.basis(),current,domain.source_corners(),budget,owned+bytes);
 else {
  const auto &derived=*domain.derived_geometry();
  const auto same_vectors=[](const std::vector<Vec3>&a,const std::vector<Vec3>&b){return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),[](Vec3 x,Vec3 y){return same(x,y);});};
  if(!same_vectors(current.positions,derived.positions)||!same_vectors(current.normals,derived.normals)||current.indices!=derived.indices||current.uv!=derived.uv||current.uv_pins!=derived.uv_pins||current.uv_seams!=derived.uv_seams)throw std::invalid_argument("Polygon paint domain differs from its exact current derivative snapshot");
  const auto corners=domain.source_corners();if(corners.size()!=current.indices.size())throw std::invalid_argument("Polygon paint domain lost its complete current corner stream");
  // Kind3 first boundary is a genuine triangle/corner bijection with changed
  // positions. A future refinement record must carry actual barycentrics; it
  // cannot turn duplicate or missing source triangles into this kind.
  const auto faces=domain.basis()->indices.size()/3;budget.bytes(owned+bytes+faces);std::vector<uint8_t>seen(faces);
  for(size_t c=0;c<corners.size();c+=3){budget.work(3);const auto source=corners[c];if(source>=domain.basis()->indices.size()||seen[source/3]++)throw std::invalid_argument("Polygon paint domain duplicated or lost an actual source triangle");std::array<uint32_t,3>ids{corners[c],corners[c+1],corners[c+2]};std::sort(ids.begin(),ids.end());if(ids!=std::array<uint32_t,3>{(source/3)*3,(source/3)*3+1,(source/3)*3+2})throw std::invalid_argument("Polygon paint domain changed its actual source corner sides");}
  if(corners.size()!=domain.basis()->indices.size())throw std::invalid_argument("Polygon paint domain requires its separate refinement/coverage adapter");
 }
 return owned;
}
}
size_t PaintSamplingDomain::storage_bytes(const PaintSamplingLimits &limits)const{Budget b(limits);return own_bytes(*this,b);}
void PaintSamplingDomain::validate_current(const Mesh &current,const PaintSamplingLimits &limits)const{Budget b(limits);validate(*this,current,b);}
std::vector<std::array<float,2>> PaintSamplingDomain::corner_coordinates(const Mesh &current,const PaintSamplingLimits &limits)const{
 Budget b(limits);const auto owned=validate(*this,current,b);b.bytes(owned+source_corners().size()*sizeof(std::array<float,2>));std::vector<std::array<float,2>> result;result.reserve(source_corners().size());
 for(auto corner:source_corners()){b.work();result.push_back(basis()->uv[basis()->indices[corner]]);}b.stop();return result;
}
Mesh PaintSamplingDomain::raster_mesh(const Mesh &current,const PaintSamplingLimits &limits)const{
 Budget b(limits);const auto owned=validate(*this,current,b);const size_t count=source_corners().size();b.bytes(owned+count*(2*sizeof(Vec3)+sizeof(std::array<float,2>)+sizeof(uint32_t)));
 Mesh raster;raster.positions.reserve(count);raster.normals.reserve(count);raster.uv.reserve(count);raster.indices.reserve(count);
 for(size_t c=0;c<count;++c){b.work();const auto vertex=current.indices[c];raster.positions.push_back(current.positions[vertex]);raster.normals.push_back(current.normals[vertex]);raster.uv.push_back(basis()->uv[basis()->indices[source_corners()[c]]]);raster.indices.push_back(uint32_t(c));}b.stop();return raster;
}
std::shared_ptr<const PaintSamplingDomain> retain_paint_sampling(const Mesh &before,
 std::shared_ptr<const PaintSamplingDomain> previous,const Mesh &current,
 std::span<const uint32_t> corners,const PaintSamplingLimits &limits){
 Budget b(limits);if(previous&&previous->version()!=1)throw std::invalid_argument("Polygon paint UV changes require their actual owning transition adapter");const auto before_bytes=mesh_bytes(before,b,false),current_bytes=mesh_bytes(current,b,false);
 if(corners.size()!=current.indices.size()||corners.size()!=before.indices.size())throw std::invalid_argument("Writable paint-domain source-corner map dimensions differ");
 b.bytes(before_bytes+current_bytes+corners.size()*sizeof(uint32_t));
 correspondence(before,current,corners,b,before_bytes+current_bytes);
 size_t previous_bytes=0;if(previous)previous_bytes=validate(*previous,before,b);
 const auto owned=previous?previous_bytes:before_bytes+corners.size()*sizeof(uint32_t);b.bytes(before_bytes+current_bytes+owned+corners.size()*sizeof(uint32_t));
 auto basis=previous?previous->basis():std::make_shared<const Mesh>(before);std::vector<uint32_t> mapping;mapping.reserve(corners.size());
 for(auto corner:corners){b.work();mapping.push_back(previous?previous->source_corners()[corner]:corner);}
 auto result=std::shared_ptr<const PaintSamplingDomain>(new const PaintSamplingDomain(std::move(basis),std::move(mapping)));
 validate(*result,current,b);b.stop();return result;
}
} // namespace dome
