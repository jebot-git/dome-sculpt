// SPDX-License-Identifier: GPL-3.0-or-later
#include "normal-bake-encoding.hpp"
#include <iostream>
#include <numbers>
using namespace dome;
using namespace dome::detail;
namespace {
unsigned checks=0;
void check(bool v,const char *message){if(!v)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
template<class F> bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
// Independent Gauss-Jordan oracle; never calls the production inverse/decoder.
Vec3 solve(NormalBakeBasis f,Vec3 d) {
  double a[3][4]={{f.t.x,f.b.x,f.n.x,d.x},{f.t.y,f.b.y,f.n.y,d.y},{f.t.z,f.b.z,f.n.z,d.z}};
  for(int c=0;c<3;++c){int p=c;for(int r=c+1;r<3;++r)if(std::abs(a[r][c])>std::abs(a[p][c]))p=r;for(int k=0;k<4;++k)std::swap(a[p][k],a[c][k]);auto div=a[c][c];if(std::abs(div)<1e-12)throw std::runtime_error("Oracle singular basis");for(int k=0;k<4;++k)a[c][k]/=div;for(int r=0;r<3;++r)if(r!=c){auto v=a[r][c];for(int k=0;k<4;++k)a[r][k]-=a[c][k]*v;}}
  return normalized({float(a[0][3]),float(a[1][3]),float(a[2][3])});
}
Vec3 forward(NormalBakeBasis b,Vec3 q){return normalized(b.t*q.x+b.b*q.y+b.n*q.z);}
double angle(Vec3 a,Vec3 b){a=normalized(a);b=normalized(b);return std::atan2(length(cross(a,b)),std::clamp(double(dot(a,b)),-1.,1.));}
}
int main()try {
  NormalBakeBasis identity{{1,0,0},{0,1,0},{0,0,1}};
  for(auto desired:{Vec3{0,0,1},Vec3{.3f,-.2f,1},Vec3{-.7f,.5f,-.4f}}){auto encoded=encode_normal_bake(identity,desired);check(encoded&&angle(encoded->tangent,solve(identity,desired))<1e-6&&encoded->quantized_angle_radians<.01,"Independent inverse and bounded RGB8 reconstruction agree for a full-rank frame");}
  std::array<NormalBakeVertex,3> raw,unit;
  // Vertex1 is a .45 glTF morph between two valid orthogonal endpoint
  // frames: base N=Z/T=X, target N=X/T=Z. Morph addition precedes vertex
  // normalization; the intermediate N/T angle need not stay orthogonal.
  const Vec3 normals[]={{0,0,1},{.45f,0,.55f},{0,0,1}};
  const Vec3 tangents[]={{1,0,0},{.55f,0,.45f},{0,1,0}};
  for(int i=0;i<3;++i){raw[i]=normal_bake_vertex(normals[i],tangents[i],1,NormalVertexBitangent::RawCross);unit[i]=normal_bake_vertex(normals[i],tangents[i],1,NormalVertexBitangent::UnitCross);}
  const std::array<double,3> bary{.2,.35,.45};
  auto native=normal_bake_basis(raw,bary,NormalFragmentBasis::UnitColumns);
  auto reference=normal_bake_basis(unit,bary,NormalFragmentBasis::UnitColumns);
  auto legacy=normal_bake_basis(raw,bary,NormalFragmentBasis::RawColumns);
  check(angle(native.b,reference.b)>.01,"Varying N/T angles expose vertex-bitangent normalization before versus after interpolation");
  auto desired=normalized(Vec3{.35f,.2f,1});
  auto pbr=encode_normal_bake(reference,desired),toon=encode_normal_bake(legacy,desired);
  check(pbr&&toon&&angle(pbr->tangent,solve(reference,desired))<1e-6&&angle(toon->tangent,solve(legacy,desired))<1e-6,"Declared PBR and legacy MToon basis inverses match the independent oracle");
  check(length(pbr->tangent-toon->tangent)>.02f&&pbr->rgba!=toon->rgba,"One physical baked direction can require different PBR and MToon normal planes");
  check(angle(forward(reference,pbr->tangent),desired)<1e-6&&angle(forward(legacy,toon->tangent),desired)<1e-6,"Separate planes recover the same desired normal in their declared rest evaluations");
  const auto old=normalized(Vec3{-.6f,.2f,1}),detail=normalized(Vec3{.4f,-.5f,1});const float coverage=.37f;
  auto world_mix=normalized(forward(legacy,old)*(1-coverage)+forward(legacy,detail)*coverage);
  auto tangent_mix=forward(legacy,normalized(old*(1-coverage)+detail*coverage));
  check(angle(world_mix,tangent_mix)>.001,"Normalized world-space coverage does not commute with tangent-space mixing in a non-orthonormal frame");
  auto resolved=encode_normal_bake(legacy,world_mix);
  check(resolved&&angle(forward(legacy,resolved->tangent),world_mix)<1e-6,"A resolved normal can encode the chosen current-pose coverage result without division by coverage");
  auto reflected=reference;reflected.b=reflected.b*-1;
  auto mirrored=encode_normal_bake(reflected,desired);
  check(mirrored&&angle(mirrored->tangent,solve(reflected,desired))<1e-6,"A reflected full-rank basis retains explicit handedness in its inverse");
  check(!encode_normal_bake({{1,0,0},{2,0,0},{0,0,1}},desired),"Singular frame encoding refuses instead of inventing a rest basis");
  check(!encode_normal_bake({{1,0,0},{1,1e-8f,0},{0,0,1}},desired),"Relative conditioning detects a near-singular basis independently of absolute scale");
  check(!encode_normal_bake(identity,{1,0,0},0)&&encode_normal_bake(identity,{0,0,1},0),"Zero normal strength admits only directions its declared decoder can represent");
  check(refuses([&]{normal_bake_basis(raw,{.2,.2,.2},NormalFragmentBasis::RawColumns);}),"Malformed barycentric authority is rejected");
  auto mixed=raw;mixed[1].w=-1;check(refuses([&]{normal_bake_basis(mixed,bary,NormalFragmentBasis::RawColumns);}),"A triangle cannot silently mix tangent handedness");
  check(refuses([&]{encode_normal_bake(identity,{NAN,0,1});}),"Nonfinite input does not publish encoded normals");
  std::cout<<"PASS "<<checks<<" normal-bake encoding foundation checks\n";return 0;
} catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
