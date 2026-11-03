// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only same-TU access to the private segment certificate; no production hook.
#include "../src/volume-binding.cpp"
#include <iostream>
using namespace dome;
using namespace dome::detail;
namespace {
size_t checks=0;
void check(bool value,const char *text){if(!value)throw std::runtime_error(text);++checks;std::cout<<"PASS "<<text<<'\n';}
void box(Mesh&m,Vec3 lo,Vec3 hi){uint32_t base=m.positions.size();for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x){m.positions.push_back({x?hi.x:lo.x,y?hi.y:lo.y,z?hi.z:lo.z});m.normals.push_back({0,0,1});m.uv.push_back({0,0});}constexpr uint32_t tris[]={0,2,3,0,3,1,4,5,7,4,7,6,0,1,5,0,5,4,2,6,7,2,7,3,0,4,6,0,6,2,1,3,7,1,7,5};for(auto i:tris)m.indices.push_back(base+i);}
Endpoint free_point(double x,double y,double z){Endpoint e;e.free={x,y,z};return e;}
bool certificate(Mesh m,Endpoint a,Endpoint b){std::function<bool()> cancel;Work w{0,10'000'000,cancel};auto p=components(m,w);std::vector<long double> events;events.reserve(m.indices.size()/3+2);std::vector<volume_exact::Interval> coverage;coverage.reserve(m.indices.size()/3);return segment_material(m,p,a,b,events,coverage,w);}
}
int main()try{
Mesh solid;box(solid,{0,0,0},{1,1,1});check(certificate(solid,free_point(.1,.1,.5),free_point(.9,.9,.5)),"Complete interior chord is material");check(certificate(solid,free_point(.1,.1,1),free_point(.9,.9,1)),"Coplanar face path is covered across actual triangulation boundary");check(!certificate(solid,free_point(-.1,.5,.5),free_point(1.1,.5,.5)),"Outside end intervals are not authorized by interior midpoint");
Mesh gap;box(gap,{0,0,0},{1,1,1});box(gap,{2,0,0},{3,1,1});check(!certificate(gap,free_point(.5,.5,1),free_point(2.5,.5,1)),"Coplanar separated face patches cannot bridge an air gap");check(!certificate(gap,free_point(.5,.5,.5),free_point(2.5,.5,.5)),"Interior endpoints in separate solids cannot bridge air");
Mesh tiny;float begin=std::nextafter(1.f,2.f);box(tiny,{0,0,0},{1,1,1});box(tiny,{begin,0,0},{2,1,1});check(!certificate(tiny,free_point(.5,.5,1),free_point(1.5,.5,1)),"One-binary32-ULP coplanar air gap remains an actual gap");
auto shear=[](Point p){return Point{p[0]+p[1]*.5,p[1]+p[2]*.5,p[2]+p[0]*.5};};auto shear_mesh=[&](Mesh m){for(auto&p:m.positions)p=as_vec(shear(as_point(p)));return m;};auto shear_endpoint=[&](Point p){Endpoint e;e.free=shear(p);return e;};
check(certificate(shear_mesh(solid),shear_endpoint({.125,.25,1}),shear_endpoint({.875,.75,1})),"Non-axis coplanar patch uses the independent exact determinant path");
check(!certificate(shear_mesh(gap),shear_endpoint({.5,.5,1}),shear_endpoint({2.5,.5,1})),"Non-axis coplanar air gap cannot borrow the constant-plane shortcut");
Mesh cavities;box(cavities,{0,0,0},{10,1,1});box(cavities,{2,.25,.25},{3,.75,.75});box(cavities,{6,.25,.25},{7,.75,.75});check(!certificate(cavities,free_point(.5,.5,.5),free_point(9.5,.5,.5)),"Multiple cavity intersections reject chord despite material endpoints and midpoint");check(certificate(cavities,free_point(.5,.125,.5),free_point(9.5,.125,.5)),"Full chord beside cavities remains material");check(!certificate(cavities,free_point(.5,.25,.5),free_point(9.5,.25,.5)),"Ambiguous grazing cavity boundary is conservatively refused");
Mesh torus;constexpr int n=12;for(int u=0;u<n;++u)for(int v=0;v<n;++v){double a=2*std::numbers::pi*u/n,b=2*std::numbers::pi*v/n;torus.positions.push_back({float((3+std::cos(b))*std::cos(a)),float((3+std::cos(b))*std::sin(a)),float(std::sin(b))});torus.normals.push_back({0,0,1});torus.uv.push_back({0,0});}for(int u=0;u<n;++u)for(int v=0;v<n;++v){auto at=[](int u,int v){return uint32_t((u%n)*n+v%n);};auto a=at(u,v),b=at(u+1,v),c=at(u+1,v+1),d=at(u,v+1);torus.indices.insert(torus.indices.end(),{a,b,c,a,c,d});}
std::function<bool()> cancel;Work work{0,1'000'000,cancel};check(components(torus,work).count==1,"Closed torus has one genuine triangle fan at every vertex");torus.positions[(n/2)*n]=torus.positions[0];bool refused=false;try{components(torus,work);}catch(const std::invalid_argument&e){refused=std::string(e.what()).find("triangle fans")!=std::string::npos;}check(refused,"Two welded fans in the same edge-connected component cannot become a zero-thickness portal");
std::cout<<"PASS "<<checks<<" private whole-segment cases\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
