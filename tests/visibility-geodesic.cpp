// SPDX-License-Identifier: GPL-3.0-or-later
#define main legacy_visibility_audit_entry
#include "automatic-visibility-audit.cpp"
#undef main
#include "visibility-geodesic-bind.hpp"
#include <queue>
using namespace dome::detail;
namespace {
size_t checks=0;
void check(bool value,const char*message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<std::endl;}
void heap_oracle(){
 const size_t n=63;std::vector<std::vector<std::pair<uint32_t,float>>>edges(n);uint32_t random=12345;
 for(uint32_t a=0;a<n;++a)for(uint32_t k=0;k<8;++k){random=random*1664525+1013904223;auto b=random%n;random=random*1664525+1013904223;edges[a].push_back({b,float(random%31+1)/8});}for(uint32_t i=1;i<n;++i)edges[i-1].push_back({i,.125f});
 std::vector<float>expected(n,INFINITY),actual(n,INFINITY);expected[0]=actual[0]=0;
 for(size_t pass=0;pass<n-1;++pass)for(size_t a=0;a<n;++a)for(auto[b,c]:edges[a])expected[b]=std::min(expected[b],expected[a]+c);
 std::function<bool()>cancel;weight_surface::Budget budget{0,1'000'000,cancel};visibility_geodesic::Heap heap(n,actual,budget);heap.decrease(0);std::vector<bool>done(n);
 while(!heap.empty()){auto a=heap.pop();check(!done[a],"Indexed queue finalizes each current node once");done[a]=true;for(auto[b,c]:edges[a])if(!done[b]&&actual[a]+c<actual[b]){actual[b]=actual[a]+c;heap.decrease(b);}}
 check(expected==actual,"Bounded indexed queue matches independent full edge-relaxation solution exactly");
}
void exact_occlusion(){
 using P=bone_shadow::Point;std::vector<P>p{{0,0,0},{0,-1,1},{0,1,1},{.5f,-.5f,-.5f},{.5f,.5f,-.5f},{.5f,0,.5f}};std::vector<uint32_t>tri{0,1,2,3,4,5};std::function<bool()>cancel;weight_surface::Budget work{0,50'000'000,cancel};
 BoneShadowQuery entry(p,tri,16u*1024u*1024u,work);BoneShadowQuery::Scratch entry_scratch(entry);BoneQueryReport report;check(entry.integrate(0,P{1,0,0},P{1,2,0},1.,0.,1.,entry_scratch,work,report)>0,"Existing entry-wall query retains its explicit outgoing-wall semantics");
 visibility_geodesic::ClosestRayVisibility q(p,tri,16u*1024u*1024u,work);visibility_geodesic::ClosestRayVisibility::Scratch scratch(q);
 for(auto source:{std::array<P,2>{P{1,0,0},P{1,2,0}},std::array<P,2>{P{1,-1,0},P{1,1,0}},std::array<P,2>{P{1,-2,0},P{1,0,0}}}){
  check(!q.visible(0,source[0],source[1],scratch,work),"Actual two-sided blocker rejects head/interior/tip closest finite-source ray");
  check(!q.visible(0,source[1],source[0],scratch,work),"Exact closest-ray shadow test is endpoint-order invariant");
 }
 auto overlay=tri;for(size_t t=0;t<tri.size();t+=3)overlay.insert(overlay.end(),{tri[t],tri[t+2],tri[t+1]});BoneShadowQuery family(p,overlay,16u*1024u*1024u,work);BoneShadowQuery::Scratch family_scratch(family);P a{1,-2,0},b{1,2,0};double value=family.integrate(0,a,b,1,0,1,family_scratch,work,report);
 check(value>0&&!q.visible(0,a,b,scratch,work),"Visible distant source portions never authorize an occluded closest source point");
 check(q.visible(0,P{1,2,0},P{1,3,0},scratch,work),"Independent clear closest ray survives a real nearby blocker");
 // A genuine duplicate source-origin position does not fabricate incidence:
 // only exact t=0 is excluded, and positive coplanar intersections still block.
 std::vector<P>origin{{0,0,0},{0,0,0},{-1,-1,0},{-1,1,0}};std::vector<uint32_t>origin_face{1,2,3};visibility_geodesic::ClosestRayVisibility contact(origin,origin_face,16u*1024u*1024u,work);visibility_geodesic::ClosestRayVisibility::Scratch contact_scratch(contact);
 check(contact.visible(0,P{0,0,1},P{0,0,2},contact_scratch,work),"Unowned exact origin-plane contact alone never blocks a positive noncoplanar ray");
 check(contact.visible(0,P{1,0,0},P{2,0,0},contact_scratch,work),"Coplanar outward ray excludes only its exact starting contact");
 check(!contact.visible(0,P{-.5f,0,0},P{-.75f,0,0},contact_scratch,work),"Actual positive coplanar material overlap remains blocked");
 check(!contact.visible(0,P{-1,1,0},P{-2,2,0},contact_scratch,work),"Positive edge-grazing contact is conservatively blocked");
 auto fractional=[&](float bottom){std::vector<P>v{{0,0,0},{.5f,bottom,-.5f},{.5f,bottom,.5f},{.5f,.75f,0}};std::vector<uint32_t>f{1,2,3};visibility_geodesic::ClosestRayVisibility model(v,f,16u*1024u*1024u,work);visibility_geodesic::ClosestRayVisibility::Scratch s(model);return model.visible(0,P{1,-1,0},P{2,1,0},s,work);};
 check(!fractional(-.25f),"Exact one-fifth closest parameter detects genuine rational edge contact");
 check(fractional(std::nextafter(-.25f,INFINITY)),"One-ULP real gap stays clear without rounded source-point substitution");
 check(!fractional(std::nextafter(-.25f,-INFINITY)),"One-ULP real overlap stays blocked with unchanged boundary policy");
 const float tiny=std::numeric_limits<float>::denorm_min();std::vector<P>near{{0,0,0},{tiny,-1,-1},{tiny,1,-1},{tiny,0,1}};std::vector<uint32_t>near_face{1,2,3};visibility_geodesic::ClosestRayVisibility wall(near,near_face,16u*1024u*1024u,work);visibility_geodesic::ClosestRayVisibility::Scratch wall_scratch(wall);
 check(!wall.visible(0,P{1,0,0},P{2,0,0},wall_scratch,work),"Positive subnormal-distance wall is not discarded as an origin epsilon");
}
}
int main(int argc,char**argv)try{
 if(argc!=2)return 2;std::filesystem::path out=argv[1];std::filesystem::create_directories(out);heap_oracle();exact_occlusion();Mesh mesh;box(mesh,0);box(mesh,.003f);Skeleton rig;for(float x:{0.f,.003f}){Bone b;b.name=rig.bones.empty()?"Authored left thin limb":"Authored right thin limb";b.rest.translation={x,.02f,0};b.length=.16f;rig.bones.push_back(b);}const auto before=rig.encode();const auto positions=mesh.positions;auto legacy=automatic_bind(mesh,rig);auto result=visibility_geodesic_bind(mesh,rig);std::vector<JointTransform>pose;for(auto&b:rig.bones)pose.push_back(b.rest);pose[0].translation.x+=.1f;pose[1].translation.x-=.1f;auto actual=deform_mesh(mesh,result.weights,rig,pose);double maximum=0,mean=0,wrong=0;
 for(size_t v=0;v<mesh.positions.size();++v){auto expected=mesh.positions[v];expected.x+=v<16?.1f:-.1f;double e=length(actual.positions[v]-expected);maximum=std::max(maximum,e);mean+=e;wrong=std::max(wrong,influence(result.weights.vertices[v],v<16?1:0));}mean/=mesh.positions.size();check(influence(legacy.vertices[0],1)>.4,"Original unchanged Automatic retains genuine opposite-limb counterexample");check(maximum<=.006&&mean<=.002&&wrong==0,"Visibility-seeded native thin/disconnected field meets unchanged pose/leakage bounds");check(rig.encode()==before&&std::equal(mesh.positions.begin(),mesh.positions.end(),positions.begin(),[](Vec3 a,Vec3 b){return std::bit_cast<uint32_t>(a.x)==std::bit_cast<uint32_t>(b.x)&&std::bit_cast<uint32_t>(a.y)==std::bit_cast<uint32_t>(b.y)&&std::bit_cast<uint32_t>(a.z)==std::bit_cast<uint32_t>(b.z);}),"Private candidate leaves full rig and input geometry unchanged");std::ofstream(out/"quality.json")<<nlohmann::json{{"max_pose_error_m",maximum},{"mean_pose_error_m",mean},{"maximum_foreign_weight",wrong},{"work",result.work},{"workspace",result.workspace},{"visible_seeds",result.visible_seeds},{"occluded_candidates",result.occluded_candidates},{"weights",result.weights.encode()}}.dump(2);std::cout<<"PASS visibility_geodesic_checks="<<checks<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
