// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_CANONICAL_CONVERSION_FAULT_HELPERS_ONLY
#include "canonical-normal-conversion-faults.cpp"
#include "canonical-normal-uv.hpp"
#include "baked-normal-field.hpp"
#include "dome/source-maps.hpp"
#include <numeric>
#include <type_traits>
using detail::CanonicalNormalUv;
using detail::CanonicalNormalUvOptions;
static_assert(!std::is_default_constructible_v<CanonicalNormalUv>);
static_assert(!std::is_copy_constructible_v<CanonicalNormalUv>);
namespace {
unsigned checks=0;
void check(bool yes,const char *label){if(!yes)throw std::runtime_error(label);++checks;std::cout<<"PASS "<<label<<'\n';}
bool refuses(auto call){try{call();return false;}catch(const std::exception&){return true;}}
auto signature(const std::shared_ptr<const CanonicalNormalUv> &uv){return uv->layout()->encode();}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;auto scene=Scene::load(argv[1]);const auto exact=scene.encode();const auto &object=scene.active();
 const auto origin=object.material_bindings->canonical_attributes;const auto field=object.painted->baked()->normal_field;
 const auto &root=origin->before_paint()->mesh();auto target=root;
 std::vector<uint32_t> map(root.positions.size());std::iota(map.begin(),map.end(),0);
 for(auto &q:target.uv){q[0]=q[0]*.8f+.1f;q[1]=q[1]*.8f+.1f;}
 auto first=CanonicalNormalUv::create(origin,target,map);first->validate_current(target);
 check(first->valid_owner(first)&&first->origin()==origin&&first->layout()->source_vertices==map,"Real conversion creates authentic exact UV correspondence without intrinsic paint planes");
 auto domain=field->sampling_domain(first);
 check(domain->resolved_pbr==field->pbr()&&domain->resolved_mtoon==field->mtoon()&&!domain->detail&&domain->coordinates==root.uv,
   "Retained domain shares original quantized resolved planes and exact root coordinates");
 Mesh split;std::vector<uint32_t> split_map;split.indices.resize(root.indices.size());std::iota(split.indices.begin(),split.indices.end(),0);
 for(auto v:target.indices){split.positions.push_back(target.positions[v]);split.normals.push_back(target.normals[v]);auto q=target.uv[v];split.uv.push_back({1-q[1],q[0]});split_map.push_back(v);}
 split.uv_pins.assign(split.positions.size(),0);split.uv_pins[0]=1;split.uv_seams.assign(split.indices.size(),1);
 auto second=first->reparameterize(split,split_map);second->validate_current(split);auto split_domain=field->sampling_domain(second);
 bool correspondence=true;for(size_t c=0;c<split.indices.size();++c)correspondence&=split_domain->coordinates[split.indices[c]]==root.uv[root.indices[c]];
 check(correspondence&&second->layout()->source_vertices==split_map&&second->layout()->pins==split.uv_pins&&second->layout()->seams==split.uv_seams,
   "Seam-expanded vertices retain every ordered source corner, editable pin and seam");
 // Compare the actual existing bilinear sampler at interior points and edges.
 // There is one resolved quantized image, never separate intrinsic/detail filtering.
 PaintSourceTexture sampler;sampler.image=field->pbr();sampler.min_filter=sampler.mag_filter=9729;sampler.wrap_s=sampler.wrap_t=33071;
 size_t samples=0;double error=0;
 for(size_t f=0;f<root.indices.size()/3;++f)for(int a=0;a<=8;++a)for(int b=0;b<=8-a;++b){
  const std::array<float,3> w{a/8.f,b/8.f,1-(a+b)/8.f};std::array<float,2> expected{},actual{};
  for(size_t c=0;c<3;++c)for(size_t k=0;k<2;++k){expected[k]+=root.uv[root.indices[f*3+c]][k]*w[c];actual[k]+=split_domain->coordinates[split.indices[f*3+c]][k]*w[c];}
  auto x=sample_source_texture(sampler,expected[0],expected[1],false),y=sample_source_texture(sampler,actual[0],actual[1],false);
  for(size_t k=0;k<4;++k)error=std::max(error,double(std::abs(x[k]-y[k])));++samples;
 }
 check(samples>0&&error==0,"Independent barycentric bilinear samples remain exact after atlas rotation and vertex splitting");
 auto current=second;auto current_mesh=split;std::vector<uint32_t> identity(split.positions.size());std::iota(identity.begin(),identity.end(),0);
 std::weak_ptr<const CanonicalNormalUv> old=second;first.reset();second.reset();split_domain.reset();domain.reset();
 auto third=current->reparameterize(current_mesh,identity);current.reset();
 check(old.expired()&&third->origin()==origin&&third->storage_bytes()==CanonicalNormalUv::create(origin,current_mesh,split_map)->storage_bytes(),
   "Repeated atlas edits flatten directly to root without retaining previous UV meshes or raster history");
 auto bad=split;bad.positions[0].x+=.01f;check(refuses([&]{third->validate_current(bad);})&&refuses([&]{third->reparameterize(bad,identity);}),"POSITION edits cannot impersonate UV-only correspondence");
 bad=split;bad.normals[0].x+=.02f;check(refuses([&]{third->validate_current(bad);})&&refuses([&]{third->reparameterize(bad,identity);}),"NORMAL edits cannot borrow original frame authority");
 bad=split;std::swap(bad.indices[0],bad.indices[1]);check(refuses([&]{third->reparameterize(bad,identity);}),"Changed ordered source corners are refused");
 bad=split;bad.uv[0][0]=1.5f;check(refuses([&]{third->reparameterize(bad,identity);}),"Out-of-atlas target coordinates are refused");
 bad=split;bad.uv_pins[0]=0;check(refuses([&]{third->validate_current(bad);}),"Current editable pin metadata remains part of the immutable layout");
 auto foreign=Scene::decode(exact).active().material_bindings->canonical_attributes;
 check(refuses([&]{field->sampling_domain(CanonicalNormalUv::create(foreign,target,map));}),"Same-value foreign conversion cannot authorize the active field");
 auto alias=std::shared_ptr<const CanonicalNormalUv>(third.get(),[](auto*){});
 check(refuses([&]{field->sampling_domain(alias);}),"Borrowed raw-pointer control blocks cannot fabricate a UV owner");
 CanonicalNormalUvOptions low;low.max_working_bytes=third->storage_bytes()-1;
 check(refuses([&]{third->reparameterize(split,identity,low);})&&scene.encode()==exact,"Prospective complete correspondence quota refuses before publishing new state");
 for(unsigned mode=0;mode<3;++mode){
  auto produce=[&]{return mode==0?CanonicalNormalUv::create(origin,target,map):mode==1?third->reparameterize(split,identity):CanonicalNormalUv::create(origin,split,split_map);};
  const auto expected=signature(produce());conversion_fault::calls=0;conversion_fault::at=SIZE_MAX;conversion_fault::enabled=true;auto measured=produce();conversion_fault::enabled=false;const auto count=conversion_fault::calls;
  check(count>0&&count<4096,"Measure actual finite UV factory persistent C++ allocation prefixes");
  for(size_t cut=1;cut<=count;++cut){conversion_fault::calls=0;conversion_fault::at=cut;conversion_fault::hit=false;bool caught=false;conversion_fault::enabled=true;
   try{produce();}catch(const std::bad_alloc&){caught=true;}conversion_fault::enabled=false;
   if(!caught||!conversion_fault::hit||scene.encode()!=exact||signature(produce())!=expected)throw std::runtime_error("UV allocation rollback or retry differs");
  }
  size_t polls=0;CanonicalNormalUvOptions o;o.cancelled=[&]{++polls;return false;};
  auto cancellable=[&]{return mode==0?CanonicalNormalUv::create(origin,target,map,o):mode==1?third->reparameterize(split,identity,o):CanonicalNormalUv::create(origin,split,split_map,o);};cancellable();
  for(size_t cut=1;cut<=polls;++cut){size_t at=0;o.cancelled=[&]{return ++at>=cut;};if(!refuses(cancellable)||scene.encode()!=exact)throw std::runtime_error("UV cancellation changed original source state");}
  check(signature(produce())==expected,"Every measured allocation failure and cancellation retries to exact complete UV correspondence");
  std::cout<<"PASS mode="<<mode<<" persistent_cpp_prefixes="<<count<<" all_cancel_polls="<<polls<<'\n';
 }
 check(scene.encode()==exact&&object.material_bindings->canonical_attributes==origin&&object.painted->baked()->normal_field==field,
   "All transitions preserve original whole scene and exact source/image/field owners");
 std::cout<<"PASS "<<checks<<" authentic converted normal UV checks samples="<<samples<<" maximum_error="<<error<<'\n';return 0;
}catch(const std::exception &e){conversion_fault::enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
