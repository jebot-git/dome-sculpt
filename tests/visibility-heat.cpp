// SPDX-License-Identifier: GPL-3.0-or-later
#define main legacy_visibility_fixture_entry
#include "automatic-visibility-audit.cpp"
#undef main
#include "visibility-heat-bind.hpp"
using namespace dome::detail;
namespace {size_t checks=0,failures=0;void check(bool v,const char*m){++checks;if(!v)++failures;std::cout<<(v?"PASS ":"FAIL ")<<m<<std::endl;}}
int main(int argc,char**argv)try{
 if(argc!=2)return 2;std::filesystem::create_directories(argv[1]);BindOptions options;options.maximum_work=500'000'000;
 Mesh rectangle;rectangle.positions={{-1,0,0},{1,0,0},{-1,1,0},{1,1,0}};rectangle.normals.assign(4,{0,0,1});rectangle.indices={0,1,2,1,3,2};Skeleton rig;for(float x:{-1.f,1.f}){Bone b;b.name=rig.bones.empty()?"Left source":"Right source";b.rest.translation={x,0,.1f};b.length=1;rig.bones.push_back(b);}options.maximum_seed_distance=.3f;
 auto result=visibility_heat_bind(rectangle,rig,options);
 // Independent dense4x4 system: rectangle half-cotangent/incident counts yield
 // horizontal0.5, vertical2 and diagonal0; each mixed mass is0.5. Multiply
 // reference row by4M: screen=2/d², not a fitted strength parameter.
 const double screen=2/(double(.1f)*double(.1f));std::array<std::array<double,5>,4>a{};
 for(size_t i=0;i<4;++i){a[i][i]=screen+2.5;a[i][i^1]=-.5;a[i][i^2]=-2;a[i][4]=(i%2==0)?screen:0;}
 for(size_t k=0;k<4;++k){double pivot=a[k][k];for(size_t j=k;j<5;++j)a[k][j]/=pivot;for(size_t i=0;i<4;++i)if(i!=k){double v=a[i][k];for(size_t j=k;j<5;++j)a[i][j]-=v*a[k][j];}}
 double error=0;for(size_t v=0;v<4;++v)error=std::max(error,std::abs(influence(result.weights.vertices[v],0)-a[v][4]));
 check(error<1e-7,"Actual positive surface heat matches independently assembled dense mixed-mass energy");
 check(result.heat_residual<=1e-9&&result.partition_error<=1e-7,"Actual PCG replay certifies residual and pre-prune partition independently");
 check(std::abs(a[0][4]-(screen+.5)/(screen+1))<1e-12,"Dense solve agrees with separate analytic symmetric rectangle solution");
 auto extended=rig;Bone unused;unused.name="Far unused deform source";unused.rest.translation={100,0,0};unused.length=100;extended.bones.push_back(unused);auto extra=visibility_heat_bind(rectangle,extended,options);
 check(extra.weights.encode()==result.weights.encode(),"Unseeded far deform source cannot alter the actual heat energy/output");
 extended.bones.back().deform=false;auto nondeform=visibility_heat_bind(rectangle,extended,options);check(nondeform.weights.encode()==result.weights.encode(),"Nondeform source never supplies a heat anchor");
 Mesh thin;box(thin,0);box(thin,.003f);Skeleton limbs;for(float x:{0.f,.003f}){Bone b;b.name=limbs.bones.empty()?"First thin limb":"Second thin limb";b.rest.translation={x,.02f,0};b.length=.16f;limbs.bones.push_back(b);}auto field=visibility_heat_bind(thin,limbs,options);double wrong=0;for(size_t v=0;v<thin.positions.size();++v)wrong=std::max(wrong,influence(field.weights.vertices[v],v<16?1:0));check(wrong==0,"Independent close disconnected closed limbs retain zero foreign heat weight");
 const AffineTransform affine(std::array<double,16>{-2,0,0,0,.5,1.5,0,0,0,.25,.75,0,16,-8,4,1});auto placed_mesh=thin;for(auto&p:placed_mesh.positions){auto q=affine.point({p.x,p.y,p.z});p={float(q[0]),float(q[1]),float(q[2])};}auto placed=limbs;placed.space_matrix=affine.float_matrix();auto transformed=visibility_heat_bind(placed_mesh,placed,options);
 check(transformed.weights.encode()==field.weights.encode(),"Exact full affine reflection/shear metric preserves independently controlled thin-limb heat field");
 size_t calls=0;bool cancel=false;try{visibility_heat_bind(thin,limbs,options,[&]{return ++calls==32;});}catch(const std::runtime_error&e){cancel=std::string(e.what())=="Bone heat cancelled";}check(cancel&&calls==32,"Actual heat construction cancels at deterministic mid-helper boundary");
 auto low=options;low.maximum_work=10;bool bounded=false;try{visibility_heat_bind(thin,limbs,low);}catch(const std::length_error&){bounded=true;}check(bounded,"Heat construction refuses actual insufficient work before emission");
 bool bytes=false;try{visibility_heat_bind(thin,limbs,options,{},65536);}catch(const std::length_error&){bytes=true;}check(bytes,"Heat construction refuses actual insufficient workspace before copies");
 std::ofstream(std::filesystem::path(argv[1])/"quality.json")<<nlohmann::json{{"checks",checks},{"failures",failures},{"affine_field_exact",transformed.weights.encode()==field.weights.encode()},{"affine_base",field.weights.encode()},{"affine_current",transformed.weights.encode()},{"dense_error",error},{"residual",result.heat_residual},{"partition",result.partition_error},{"thin_foreign_weight",wrong},{"work",field.work},{"workspace",field.workspace}}.dump(2);std::cout<<"RESULT heat_checks="<<checks<<" failures="<<failures<<std::endl;return failures?1:0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
