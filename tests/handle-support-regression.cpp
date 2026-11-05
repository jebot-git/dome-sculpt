// SPDX-License-Identifier: GPL-3.0-or-later
#define main visibility_fixture_entry
#include "automatic-visibility-audit.cpp"
#undef main
#include "visibility-heat-bind.hpp"
using namespace dome::detail;using namespace dome::detail::visibility_geodesic;
namespace {size_t checks=0;void check(bool value,const char*why){if(!value)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
Mesh unit_cube(){Mesh m;box(m,0);for(auto&p:m.positions){p.x*=1000;p.y=p.y*10-1;p.z*=1000;}return m;}
Mesh connected_u(){
 const std::array<float,4>x{-.001f,.001f,.002f,.004f};const std::array<float,3>y{0,.01f,.2f};const std::array<float,2>z{-.001f,.001f};
 Mesh m;std::map<std::array<int,3>,uint32_t>vertices;
 auto filled=[](int a,int b,int c){return a>=0&&a<3&&b>=0&&b<2&&c==0&&(b==0||a!=1);};
 auto vertex=[&](std::array<int,3>p){auto[it,created]=vertices.emplace(p,uint32_t(m.positions.size()));if(created){m.positions.push_back({x[p[0]],y[p[1]],z[p[2]]});m.normals.push_back({0,0,1});}return it->second;};
 const std::array<std::array<std::array<int,3>,4>,6>faces{{{{{0,0,0},{0,0,1},{0,1,1},{0,1,0}}},{{{1,0,0},{1,1,0},{1,1,1},{1,0,1}}},{{{0,0,0},{1,0,0},{1,0,1},{0,0,1}}},{{{0,1,0},{0,1,1},{1,1,1},{1,1,0}}},{{{0,0,0},{0,1,0},{1,1,0},{1,0,0}}},{{{0,0,1},{1,0,1},{1,1,1},{0,1,1}}}}};
 const std::array<std::array<int,3>,6>directions{{{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}}};
 for(int a=0;a<3;++a)for(int b=0;b<2;++b)if(filled(a,b,0))for(size_t f=0;f<6;++f)if(!filled(a+directions[f][0],b+directions[f][1],directions[f][2])){std::array<uint32_t,4>v;for(size_t k=0;k<4;++k)v[k]=vertex({a+faces[f][k][0],b+faces[f][k][1],faces[f][k][2]});m.indices.insert(m.indices.end(),{v[0],v[1],v[2],v[0],v[2],v[3]});}
 m.validate();return m;
}
}
int main()try{
 std::function<bool()>stop;BindOptions options;options.maximum_work=500'000'000;options.maximum_seed_distance=3;
 auto m=unit_cube();Skeleton rig;Bone b;b.name="Declared finite control";rig.bones.push_back(b);const std::array<HeatSourceSegment,1>sources{{{{0,0,0},{2,1,0},0}}};
 weight_surface::Budget a{0,options.maximum_work,stop},c{0,options.maximum_work,stop};visibility_geodesic::Bytes ab{0,256u*1024u*1024u},cb{0,256u*1024u*1024u};
 auto strict=visibility_heat::prepare(m,rig,options,a,ab,sources,HeatDomainPolicy::ClosedMaterial);auto supported=visibility_heat::prepare(m,rig,options,c,cb,sources,HeatDomainPolicy::ComponentHandleSupport);
 check((strict.system.seeds[14][0]&1)==0&&(supported.system.seeds[14][0]&1)!=0,"Whole supported finite handle retains a self-visible nearest source beyond the skin, unlike pointwise containment");
 const std::array<HeatSourceSegment,1>outside{{{{2,-.8f,0},{2,.8f,0},0}}};auto attachment=visibility_heat_bind(m,rig,options,{},256u*1024u*1024u,outside,HeatDomainPolicy::ComponentHandleSupport);
 check(attachment.exterior_components==1&&attachment.interior_components==0&&std::all_of(attachment.weights.vertices.begin(),attachment.weights.vertices.end(),[](auto&r){return r.size()==1&&r[0].joint==0&&r[0].weight==1;}),"Closed attachment with all handles genuinely exterior receives a complete owning field");
 const std::array<HeatSourceSegment,1>boundary{{{{1,0,0},{1,0,0},0}}};bool refused=false;try{visibility_heat_bind(m,rig,options,{},256u*1024u*1024u,boundary,HeatDomainPolicy::ComponentHandleSupport);}catch(const std::runtime_error&e){refused=std::string(e.what()).find("boundary-only handles")!=std::string::npos;}check(refused,"Boundary-only point cannot silently select exterior-attachment or interior support");
 auto touching=m;for(auto&p:touching.positions)p.x-=1;auto right=m;for(auto&p:right.positions)p.x+=1;const auto left_count=touching.positions.size();for(auto p:right.positions)touching.positions.push_back(p);touching.normals.resize(touching.positions.size(),{0,0,1});for(auto i:right.indices)touching.indices.push_back(uint32_t(i+left_count));Skeleton twins;
 for(float x:{-1.f,1.f}){Bone t;t.name=twins.bones.empty()?"First genuine source":"Second genuine source";t.rest.translation={x,-.8f,0};t.length=1.6f;twins.bones.push_back(t);}const auto input_positions=touching.positions;const auto input_rig=twins.encode();auto field=visibility_heat_bind(touching,twins,options,{},256u*1024u*1024u,{},HeatDomainPolicy::ComponentHandleSupport);
 double foreign=0;for(size_t v=0;v<touching.positions.size();++v)foreign=std::max(foreign,influence(field.weights.vertices[v],v<left_count?1:0));check(foreign==0&&field.interior_components==2,"Coincident touching closed components retain distinct rows and source support without coordinate welding");
 std::vector<JointTransform>pose;for(auto t:twins.bones)pose.push_back(t.rest);pose[0].translation.z+=.1f;pose[1].translation.z-=.1f;auto moved=deform_mesh(touching,field.weights,twins,pose);double error=0;for(size_t v=0;v<touching.positions.size();++v){auto expected=touching.positions[v];expected.z+=v<left_count?.1f:-.1f;error=std::max(error,double(length(moved.positions[v]-expected)));}check(error<1e-6,"Independent opposite poses preserve touching-part rigid motion and shared geometric positions separately");
 check(twins.encode()==input_rig&&std::equal(touching.positions.begin(),touching.positions.end(),input_positions.begin(),[](Vec3 x,Vec3 y){return x.x==y.x&&x.y==y.y&&x.z==y.z;}),"Control support construction preserves complete original skeleton and geometry");
 auto u=connected_u();Skeleton arms;for(float x:{0.f,.003f}){Bone t;t.name=arms.bones.empty()?"One native folded arm":"Other native folded arm";t.rest.translation={x,0,0};t.length=.2f;arms.bones.push_back(t);}BindOptions thin;thin.maximum_work=500'000'000;auto curved=visibility_heat_bind(u,arms,thin,{},256u*1024u*1024u,{},HeatDomainPolicy::ComponentHandleSupport);pose.clear();for(auto t:arms.bones)pose.push_back(t.rest);pose[0].translation.x+=.1f;pose[1].translation.x-=.1f;auto bent=deform_mesh(u,curved.weights,arms,pose);double maximum=0,mean=0;size_t count=0;
 for(size_t v=0;v<u.positions.size();++v)if(u.positions[v].y>=.18f){auto expected=u.positions[v];expected.x+=u.positions[v].x<.0015f?.1f:-.1f;const double e=length(bent.positions[v]-expected);maximum=std::max(maximum,e);mean+=e;++count;}if(!count)throw std::runtime_error("Real distal U rows required");mean/=count;
 std::cout<<"FOLDED_U max="<<maximum<<" mean="<<mean<<" work="<<curved.work<<std::endl;check(maximum<=.006&&mean<=.002,"Actual connected narrow-gap folded limbs meet unchanged3%maximum/1%mean distal pose limits");
 std::cout<<"PASS handle_support_checks="<<checks<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
