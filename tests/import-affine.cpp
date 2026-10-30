// SPDX-License-Identifier: GPL-3.0-or-later
#include "import-affine.hpp"
#include "affine-matrix.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
using namespace dome;
using namespace dome::detail;
namespace {
thread_local bool armed=false,hit=false;
thread_local size_t calls=0,fail_at=SIZE_MAX;
size_t checks=0;
void check(bool ok,const char*m){if(!ok)throw std::runtime_error(m);++checks;}
template<class F>void refuses(F f,const char*m){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,m);}
void near(AffineVector a,AffineVector b,const char*m){for(size_t i=0;i<3;++i)if(std::abs(a[i]-b[i])>1e-10*std::max({1.,std::abs(a[i]),std::abs(b[i])}))throw std::runtime_error(m);++checks;}
std::shared_ptr<const ImportedAsset> fixture() {
  ImportedAsset asset;asset.mode=ImportMode::PreserveGltf;asset.nodes.resize(3);
  const float c=std::sqrt(.5f);
  asset.nodes[0].local={2,0,0,0, 0,1,0,0, 0,0,-3,0, 4,-2,7,1};
  asset.nodes[1].local={c,c,0,0, -c,c,0,0, 0,0,1,0, .2f,.3f,-.1f,1};
  asset.nodes[2].local={1,0,0,0, 0,1,0,0, 0,0,1,0, -.4f,.2f,.6f,1};
  asset.nodes[0].children={1};asset.nodes[1].parent=0;
  asset.nodes[1].children={2};asset.nodes[2].parent=1;
  return std::make_shared<const ImportedAsset>(std::move(asset));
}
}
void *operator new(size_t n){if(armed && ++calls>=fail_at){hit=true;throw std::bad_alloc();}if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}
void operator delete[](void*p,size_t)noexcept{std::free(p);}
int main()try {
  auto source=fixture();const std::array<uint32_t,1> roots{0};
  ImportedAffineHierarchy h(source,roots);
  check(h.source()==source,"immutable source owner retained strongly");
  check(h.nodes().size()==3 && h.node(2).parent==1,"selected hierarchy/source identities retained");
  const auto chain=h.local_chain(2);
  check(chain.size()==3,"original chain preserved rather than flattened");
  for(size_t n=0;n<chain.size();++n) {
    check(chain[n].first==n,"chain source identity");
    check(std::bit_cast<std::array<uint32_t,16>>(chain[n].second)==std::bit_cast<std::array<uint32_t,16>>(source->nodes[n].local),"original local matrix bits exact");
    check(gltf_trs_matrix(chain[n].second),"every emitted original local remains valid glTF TRS");
  }
  check(!gltf_trs_matrix(h.node(2).world.float_matrix()),"valid local chain has genuine world shear");
  check(h.node(2).world.orientation()==AffineOrientation::Reversing,"world reflection preserved");
  const AffineVector p{.15,-.23,.7};
  auto expected=AffineTransform(source->nodes[0].local).point(AffineTransform(source->nodes[1].local).point(AffineTransform(source->nodes[2].local).point(p)));
  near(h.node(2).world.point(p),expected,"world equals independent ordered local evaluation");
  near(h.inverse_world(2).point(expected),p,"reverse local inverses recover source point");
  near(h.node(2).world.inverse().point(expected),p,"full affine inverse agrees with local chain");
  near(h.normal_world(2,{.3,.7,-.2}),h.node(2).world.normal({.3,.7,-.2}),"factorized and world normals agree");
  check(h.orientation_world(2)==AffineOrientation::Reversing,"factorized determinant orientation");
  auto old_source=source;source.reset();
  check(h.source()==old_source,"source remains live independently of caller handle");
  refuses([&]{(void)h.node(4);},"unknown source identity refuses");
  refuses([&]{ImportedAffineHierarchy bad(old_source,roots,2);},"selected node quota refuses");
  refuses([&]{ImportedAffineHierarchy bad(old_source,roots,129);},"unbounded caller quota refuses");
  auto corrupt=[&](auto fn){auto copy=std::make_shared<ImportedAsset>(*old_source);fn(*copy);return std::shared_ptr<const ImportedAsset>(copy);};
  refuses([&]{ImportedAffineHierarchy bad(corrupt([](auto&a){a.nodes[0].children.push_back(1);}),roots);},"duplicate children refuse");
  refuses([&]{ImportedAffineHierarchy bad(corrupt([](auto&a){a.nodes[2].children.push_back(0);}),roots);},"cycle refuses");
  refuses([&]{ImportedAffineHierarchy bad(corrupt([](auto&a){a.nodes[1].parent=2;}),roots);},"parent authority mismatch refuses");
  refuses([&]{ImportedAffineHierarchy bad(corrupt([](auto&a){a.nodes[0].children={999};}),roots);},"out-of-range child refuses");
  refuses([&]{ImportedAffineHierarchy bad(corrupt([](auto&a){a.nodes[0].local[4]=.25f;}),roots);},"invalid sheared original local refuses");
  auto singular=corrupt([](auto&a){a.nodes[0].local[0]=0;});
  ImportedAffineHierarchy flat(singular,roots);
  check(flat.node(2).world.orientation()==AffineOrientation::Singular,"singular forward graph retained explicitly");
  refuses([&]{(void)flat.inverse_world(2);},"singular source inverse has actionable refusal");
  refuses([&]{(void)flat.normal_world(2,{1,0,0});},"singular factor normal refuses");
  auto extreme=corrupt([](auto&a){a.nodes[0].local[0]=1e-20f;a.nodes[0].local[5]=1;});
  ImportedAffineHierarchy angled(extreme,roots);
  refuses([&]{(void)angled.node(2).world.normal({1,0,0});},"flattened nearly parallel world has explicit conditioning refusal");
  auto stable_normal=angled.normal_world(2,{1,0,0});
  check(std::isfinite(stable_normal[0]) && std::abs(stable_normal[0])>1e19 && std::abs(stable_normal[1])>.1,
        "original local normal chain retains eligible extreme anisotropy");
  {
    auto reordered=std::make_shared<ImportedAsset>();reordered->nodes.resize(8);
    reordered->nodes[4]=old_source->nodes[0];reordered->nodes[4].children={1};
    reordered->nodes[1]=old_source->nodes[1];reordered->nodes[1].parent=4;reordered->nodes[1].children={7};
    reordered->nodes[7]=old_source->nodes[2];reordered->nodes[7].parent=1;
    reordered->nodes[7].local[3]=-0.f;
    const std::array<uint32_t,1> selected{4};
    ImportedAffineHierarchy different_ids(reordered,selected);
    const auto local=different_ids.local_chain(7);
    check(local[0].first==4 && local[1].first==1 && local[2].first==7 && different_ids.node(7).parent==1,
          "source identities never alias selected array slots");
    check(std::signbit(local[2].second[3]),"raw local signed zero survives exact chain");
    near(different_ids.node(7).world.point(p),expected,"unselected source nodes do not change selected graph");
  }
  {
    auto deep=std::make_shared<ImportedAsset>();deep->nodes.resize(128);
    for(int n=0;n<128;++n) {
      auto &node=deep->nodes[n];node.local={1,0,0,0, 0,1,0,0, 0,0,1,0, .001f,-.002f,.003f,1};
      node.parent=n-1;if(n<127)node.children={uint32_t(n+1)};
    }
    deep->nodes[0].local[0]=-2;
    ImportedAffineHierarchy bounded(deep,roots);
    check(bounded.local_chain(127).size()==128,"full bounded depth remains iterative-exportable");
    near(bounded.inverse_world(127).point(bounded.node(127).world.point(p)),p,"128-node inverse chain");
    check(bounded.orientation_world(127)==AffineOrientation::Reversing,"128-node reflected orientation");
  }
  size_t observations=0;
  {ImportedAffineHierarchy measured(old_source,roots,128,[&]{++observations;return false;});}
  const size_t cancellations=observations;
  for(size_t n=1;n<=cancellations;++n){observations=0;refuses([&]{ImportedAffineHierarchy cancelled(old_source,roots,128,[&]{return ++observations>=n;});},"each hierarchy cancellation refuses atomically");}
  fail_at=SIZE_MAX;calls=0;armed=true;
  {ImportedAffineHierarchy measured(old_source,roots);auto c=measured.local_chain(2);}
  armed=false;const size_t allocations=calls;
  check(allocations>0,"hierarchy fixture measures real allocation boundaries");
  for(size_t n=1;n<=allocations;++n) {
    fail_at=n;calls=0;hit=false;armed=true;bool caught=false;
    try{ImportedAffineHierarchy denied(old_source,roots);auto c=denied.local_chain(2);}catch(const std::bad_alloc&){caught=true;}
    armed=false;check(hit&&caught,"persistent allocation failure propagates safely");
    check(ImportedAffineHierarchy(old_source,roots).local_chain(2)==chain,"retry preserves exact source chain");
  }
  std::cout<<checks<<" source affine hierarchy checks +"<<allocations<<" persistent allocation +"<<cancellations<<" cancellation boundaries passed\n";return 0;
}catch(const std::exception&e){armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
