#include "dome/document.hpp"
#include "dome/material-visibility.hpp"
#include "skin-affine.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using namespace dome;
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("Choose authentic unskinned fitted input and ray audit");auto scene=Scene::load(argv[1]);uint64_t ref=0;for(const auto&o:scene.objects())if(o.deformation&&o.deformation->skeleton)ref=o.id;
 const auto&r=scene.object(ref);if(!r.deformation->landmarks||!r.deformation->humanoid)throw std::runtime_error("Require actual current body frame and roles");
 const auto world=scene.world_matrix(ref);auto f=r.deformation->landmarks->front;auto dir=world.direction({f.x,f.y,f.z});Vec3 front=normalized(Vec3{float(dir[0]),float(dir[1]),float(dir[2])});
 struct View{uint64_t id;Mesh mesh;std::unique_ptr<SurfaceQuery>geometry;std::unique_ptr<MaterialSurfaceQuery>visible;};std::vector<View>views;double low=INFINITY,high=-INFINITY;
 for(const auto&o:scene.objects())if(o.mesh_active&&o.painted&&o.visible){const std::function<bool()>never=[] {return false;};detail::SkinGeometryWork work(500000000,never);auto m=detail::skin_world_mesh(o.painted->mesh(),scene.world_matrix(o.id),work);for(auto p:m.positions){low=std::min(low,double(p.y));high=std::max(high,double(p.y));}SurfaceRange range{o.id,0,m.indices.size(),o.material,o.painted->preview(o.material),o.appearance,0,o.material_bindings};views.push_back({o.id,std::move(m),{}, {}});auto&v=views.back();v.geometry=std::make_unique<SurfaceQuery>(v.mesh);v.visible=std::make_unique<MaterialSurfaceQuery>(v.mesh,std::span(&range,1));}
 const auto segs=r.deformation->skeleton->segments();nlohmann::json result;result["source_geometry_only_no_artist_weights"]=true;result["eyes"]=nlohmann::json::array();
 for(const auto&role:{"leftEye","rightEye"}){auto j=r.deformation->humanoid->joints.at(role);const auto p=segs.at(j)[0];auto q=world.point({p.x,p.y,p.z});Vec3 pivot{float(q[0]),float(q[1]),float(q[2])};const float distance=float(2*(high-low));const Vec3 from=pivot+front*distance;auto geometric=nlohmann::json::array(),visible=nlohmann::json::array();
  for(const auto&v:views){for(int kind=0;kind<2;++kind){auto h=kind?v.visible->raycast(from,front*-1,distance):v.geometry->raycast(from,front*-1,distance);if(h)(kind?visible:geometric).push_back({{"object",v.id},{"name",scene.object(v.id).name},{"triangle",h->triangle},{"bary",h->barycentric},{"distance",h->distance},{"pivot_depth",distance-h->distance}});}}
  auto sort=[](auto&a){std::sort(a.begin(),a.end(),[](const auto&x,const auto&y){return x.at("distance").template get<float>()<y.at("distance").template get<float>();});};sort(geometric);sort(visible);
  result["eyes"].push_back({{"role",role},{"pivot",std::array{pivot.x,pivot.y,pivot.z}},{"front",std::array{front.x,front.y,front.z}},{"geometry_hits",geometric},{"actual_material_visible_hits",visible}});
 }
 std::ofstream(argv[2])<<result.dump(2);return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}
