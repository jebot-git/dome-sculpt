// SPDX-License-Identifier: GPL-3.0-or-later
#define main visibility_base_fixture
#include "automatic-visibility-audit.cpp"
#undef main
#include "closed-mesh-source-domain.hpp"
using namespace dome::detail;using namespace dome::detail::visibility_geodesic;
namespace{size_t checks=0;void check(bool yes,const char*why){if(!yes)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}Mesh cube(float upper=1){Mesh m;box(m,0);for(auto&p:m.positions){p.x=p.x<0?-1:upper;p.y=p.y*10-1;p.z*=1000;}return m;}
std::shared_ptr<const ClosestRayVisibility> query(const Mesh&m,weight_surface::Budget&w){std::vector<bone_shadow::Point>p;for(auto v:m.positions)p.push_back({v.x,v.y,v.z});return std::make_shared<const ClosestRayVisibility>(p,m.indices,128u*1024u*1024u,w,true);}}
int main()try{
 std::function<bool()>stop;weight_surface::Budget work{0,500'000'000,stop};using P=bone_shadow::Point;using L=ClosedMeshSourceDomain::Location;auto m=cube();auto q=query(m,work);ClosedMeshSourceDomain domain(q,128u*1024u*1024u,work);ClosestRayVisibility::Scratch scratch(*q);
 check(domain.closed_components==1&&domain.closed(0),"Actual indexed closed manifold is certified without coordinate welding");
 check(domain.closest(0,P{0,0,0},P{0,0,0},scratch,work)==L::Inside,"Three exact half-open axis parities agree on an interior point");
 check(domain.closest(0,P{2,0,0},P{2,0,0},scratch,work)==L::Outside,"Independent exterior point remains outside material");
 check(domain.closest(0,P{1,0,0},P{1,0,0},scratch,work)==L::Boundary,"Actual source-on-surface is a distinct closed boundary result");
 check(domain.closest(0,P{std::nextafter(1.f,0.f),0,0},P{std::nextafter(1.f,0.f),0,0},scratch,work)==L::Inside&&domain.closest(0,P{std::nextafter(1.f,INFINITY),0,0},P{std::nextafter(1.f,INFINITY),0,0},scratch,work)==L::Outside,"One-ULP material gaps are not hidden by a ray-origin epsilon");
 check(domain.closest(0,P{-.5f,0,0},P{0,-1,0},scratch,work)==L::Inside,"True three-fifths closest point is classified as a rational, not rounded coordinates");
 for(bool above:{false,true}){float limit=above?std::nextafter(-.2f,INFINITY):-.2f;auto shape=cube(limit);auto exactq=query(shape,work);ClosedMeshSourceDomain exact(exactq,128u*1024u*1024u,work);ClosestRayVisibility::Scratch s(*exactq);check(exact.closest(0,P{-.5f,0,0},P{0,-1,0},s,work)==(above?L::Inside:L::Outside),"Nonbinary rational closest point distinguishes actual adjacent float boundary planes");}
 check(domain.exterior(0,P{2,0,0},P{3,0,0},scratch,work),"Whole finite source is certified exterior before attachment policy selection");
 check(!domain.exterior(0,P{-2,0,0},P{2,0,0},scratch,work),"Exterior endpoints cannot certify a segment that crosses material");
 check(!domain.exterior(0,P{-2,1,0},P{2,1,0},scratch,work),"Genuine coplanar/grazing source segment remains potentially interior");
 const auto gap=std::nextafter(1.f,INFINITY);check(domain.exterior(0,P{-2,gap,0},P{2,gap,0},scratch,work),"A real one-ULP parallel segment gap is certified exterior");
 auto reversed=m;for(size_t c=0;c<reversed.indices.size();c+=6)std::swap(reversed.indices[c+1],reversed.indices[c+2]);auto rq=query(reversed,work);ClosedMeshSourceDomain rd(rq,128u*1024u*1024u,work);ClosestRayVisibility::Scratch rs(*rq);check(rd.closest(0,P{0,0,0},P{0,0,0},rs,work)==L::Inside,"Material parity is invariant under arbitrary triangle winding reversal");
 auto nested=m;auto small=m;for(auto&p:small.positions)p=p*.5f;auto offset=uint32_t(nested.positions.size());nested.positions.insert(nested.positions.end(),small.positions.begin(),small.positions.end());nested.normals.resize(nested.positions.size(),{0,0,1});for(auto i:small.indices)nested.indices.push_back(i+offset);auto nq=query(nested,work);ClosedMeshSourceDomain nd(nq,128u*1024u*1024u,work);ClosestRayVisibility::Scratch ns(*nq);check(nd.closed_components==2&&nd.closest(0,P{.75f,0,0},P{.75f,0,0},ns,work)==L::Inside&&nd.closest(offset,P{.75f,0,0},P{.75f,0,0},ns,work)==L::Outside,"Nested disconnected target components retain their own solid membership, without foreign joins");
 auto wrong=query(m,work);ClosestRayVisibility::Scratch ws(*wrong);bool refused=false;try{domain.closest(0,P{0,0,0},P{0,0,0},ws,work);}catch(const std::invalid_argument&){refused=true;}check(refused,"Equal-byte geometry from a foreign owning BVH cannot substitute scratch authority");
 Mesh open;open.positions={{0,0,0},{1,0,0},{0,1,0}};open.normals.assign(3,{0,0,1});open.indices={0,1,2};auto oq=query(open,work);ClosedMeshSourceDomain od(oq,128u*1024u*1024u,work);check(!od.closed(0)&&od.closed_components==0,"Open target remains explicitly intrinsic, never fabricated as a solid");
 check(domain.nearest_material_closure(0,P{0,-2,0},P{0,2,0},scratch,work)&&domain.nearest_material_closure(0,P{0,2,0},P{0,-2,0},scratch,work),"Finite source boundary nearest point owns positive interior continuation in either endpoint order");
 check(!domain.nearest_material_closure(0,P{-2,-1,0},P{2,-1,0},scratch,work),"Coplanar boundary source has no positive-measure solid-interior continuation");
 check(!domain.nearest_material_closure(0,P{-1,-1,-1},P{-1,-1,-1},scratch,work),"A boundary point control cannot borrow finite-line interior continuation");
 check(domain.nearest_material_closure(0,P{-2,-2,-2},P{0,0,0},scratch,work),"Exact transverse vertex entry has certified one-sided material continuation");
 check(!domain.nearest_material_closure(0,P{-2,-2,-2},P{-1,-1,-1},scratch,work),"An isolated endpoint touching a vertex cannot invent an unavailable interior parameter side");
 auto invalid=m;invalid.positions[1]=invalid.positions[3];auto iq=query(invalid,work);refused=false;try{ClosedMeshSourceDomain overlap(iq,128u*1024u*1024u,work);}catch(const std::invalid_argument&){refused=true;}check(refused,"Closed embedding refuses unowned coincident vertex contacts");
 refused=false;try{ClosedMeshSourceDomain limited(q,65536,work);}catch(const std::length_error&){refused=true;}check(refused,"Closed topology/embedding workspace is refused before staging");
 std::cout<<"PASS closed_source_domain_checks="<<checks<<" work="<<work.used<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
