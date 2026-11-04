// SPDX-License-Identifier: GPL-3.0-or-later
#define main zero_morph_native_fixture
#include "zero-morph-rest.cpp"
#undef main
namespace {
using D3=std::array<double,3>;
D3 independent_normal(const RigMatrix &m,Vec3 n){double a[3][6]{};for(size_t r=0;r<3;++r){for(size_t c=0;c<3;++c)a[r][c]=m[c*4+r];a[r][r+3]=1;}for(size_t c=0;c<3;++c){size_t pivot=c;for(size_t r=c+1;r<3;++r)if(std::abs(a[r][c])>std::abs(a[pivot][c]))pivot=r;if(std::abs(a[pivot][c])<1e-12)throw std::runtime_error("Independent singular joint");for(size_t k=0;k<6;++k)std::swap(a[c][k],a[pivot][k]);const auto divisor=a[c][c];for(size_t k=0;k<6;++k)a[c][k]/=divisor;for(size_t r=0;r<3;++r)if(r!=c){const auto factor=a[r][c];for(size_t k=0;k<6;++k)a[r][k]-=factor*a[c][k];}}D3 result{};const D3 value{n.x,n.y,n.z};for(size_t r=0;r<3;++r)for(size_t c=0;c<3;++c)result[r]+=a[c][r+3]*value[c];return result;}
Vec3 independent_vertex(Vec3 n,const VertexWeights &weights,const std::vector<RigMatrix> &matrices){D3 sum{};for(auto w:weights){auto v=independent_normal(matrices.at(w.joint),n);for(size_t k=0;k<3;++k)sum[k]+=v[k]*w.weight;}const auto magnitude=std::sqrt(sum[0]*sum[0]+sum[1]*sum[1]+sum[2]*sum[2]);return {float(sum[0]/magnitude),float(sum[1]/magnitude),float(sum[2]/magnitude)};}
Skeleton skeleton(){Skeleton s{{{"root",-1,{},.2f},{"tip",0,{},.1f}}};s.bones[1].rest.translation={0,.04f,0};return s;}
SkinWeights weights(size_t count){SkinWeights w;for(size_t v=0;v<count;++v)w.vertices.push_back(v%3==0?VertexWeights{{0,.7f},{1,.3f}}:v%3==1?VertexWeights{{0,1}}:VertexWeights{{1,1}});return w;}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);auto native=fixture();auto base=native.active().painted->mesh();auto rig=skeleton();auto skin=weights(base.positions.size());
 Mesh split;for(size_t c=0;c<base.indices.size();++c){auto v=base.indices[c];split.positions.push_back(base.positions[v]);split.normals.push_back(c==3?normalized(Vec3{-.2f,.6f,1}):base.normals[v]);split.uv.push_back(base.uv[v]);split.indices.push_back(uint32_t(c));split.uv_pins.push_back(base.uv_pins[v]);}split.uv_seams=base.uv_seams;
 for(const auto &rest:{base,split})for(int variant=0;variant<6;++variant){auto s=rig;auto w=weights(rest.positions.size());std::vector<JointTransform> pose;for(auto b:s.bones)pose.push_back(b.rest);
  if(variant==1)pose[0].translation={.04f,-.02f,.01f};
  if(variant==2){pose[0].rotation={std::cos(.35f),0,0,std::sin(.35f)};pose[1].rotation={std::cos(-.2f),std::sin(-.2f),0,0};}
  if(variant==3){pose[0].scale={2,.5f,1.7f};pose[1].scale={.8f,1.3f,.7f};}
  if(variant==4){pose[0].scale={-1.2f,.7f,1.8f};pose[1].rotation={std::cos(.2f),0,std::sin(.2f),0};}
  if(variant==5){RigMatrix affine{1,0,0,0,.35f,1,0,0,.1f,.2f,1,0,0,0,0,1};pose[1].full_local_matrix=affine;s.inverse_bind_override=s.inverse_bind();}
  auto result=deform_mesh(rest,w,s,pose);const auto matrices=s.skin_matrices(pose);for(size_t v=0;v<rest.positions.size();++v)check(length(result.normals[v]-independent_vertex(rest.normals[v],w.vertices[v],matrices))<2e-6f,"Independent double Gauss-Jordan oracle matches each authored weighted joint normal");
  check(result.indices==rest.indices&&result.uv==rest.uv&&result.uv_pins==rest.uv_pins&&result.uv_seams==rest.uv_seams,"Skinning preserves split topology and complete UV authoring data");
 }
 auto analytic=base;analytic.normals.assign(4,normalized(Vec3{.6f,.8f,.3f}));auto one=Skeleton{{{"only",-1,{},.2f}}};SkinWeights one_weights;one_weights.vertices.assign(4,{{0,1}});JointTransform scale;scale.scale={2,.5f,1.5f};auto result=deform_mesh(analytic,one_weights,one,std::array{scale});check(length(result.normals[0]-normalized(Vec3{.3f,1.6f,.2f}))<2e-6f,"Analytic nonuniform scale uses inverse-transpose rather than geometric or direct-vector normals");
 auto d=*native.active().deformation;d.skeleton=rig;d.skin=skin;for(auto b:rig.bones)d.pose.push_back(b.rest);native.set_deformation(1,d);auto before=native.encode();Document doc;doc.replace(native.snapshot());ready(doc);ObjectBakeSettings options;options.ao_samples=0;options.padding=0;doc.bake_from_object(2,options);ready(doc);auto accepted=doc.copy_scene().encode();check(bool(doc.copy_scene().active().painted->baked()),"Real native Bake accepts an explicit identity rig pose with authored normals and zero morphs");check(doc.undo()&&doc.copy_scene().encode()==before&&doc.redo()&&doc.copy_scene().encode()==accepted,"Native skinned rest Bake preserves exact Undo and Redo history");ready(doc);doc.save(out/"skin-normals.dsculpt");auto loaded=Scene::load(out/"skin-normals.dsculpt");check(loaded.encode()==accepted,"Native save/restart retains exact authored normals, skin, rig and bake");
 auto bad=base;bad.normals.assign(4,{1,0,0});Skeleton opposing{{{"a",-1,{},.1f},{"b",-1,{},.1f}}};SkinWeights mixed;mixed.vertices.assign(4,{{0,.5f},{1,.5f}});std::array<JointTransform,2> opposite{};opposite[1].full_local_matrix=RigMatrix{-1,0,0,0,0,1,0,0,0,0,-1,0,0,0,0,1};bool refused=false;try{deform_mesh(bad,mixed,opposing,opposite);}catch(const std::invalid_argument &){refused=true;}check(refused,"A genuinely cancelling weighted normal is refused rather than replaced by geometric shading");
 std::cout<<"PASS "<<checks<<" authored skin-normal transport checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
