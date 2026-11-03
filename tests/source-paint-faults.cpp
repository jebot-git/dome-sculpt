// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/source-paint.hpp"
#include "dome/source-maps.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <iostream>
#include <new>
namespace {
thread_local bool armed=false,hit=false;
thread_local size_t calls=0,cut=0;
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
}
[[gnu::noinline]] void *operator new(size_t n){if(armed&&++calls>=cut&&cut){hit=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,size_t)noexcept{std::free(p);}
int main()try{
 using namespace dome;set_paint_composite_cache_budget(0);
 Mesh raw;raw.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};raw.normals.assign(4,{0,0,1});
 raw.uv={{-1,-1},{2,-1},{2,2},{-1,2}};raw.indices={0,1,2,0,2,3};
 auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=std::make_shared<const Image>(Image{1,1,{200,100,50,255}});
 maps->normal.image=std::make_shared<const Image>(Image{1,1,{128,128,255,255}});maps->normal.texcoord=1;
 maps->uv1=std::make_shared<const std::vector<std::array<float,2>>>(raw.uv);
 PaintSurfaceOptions policy;policy.policy=PaintUvPolicy::SourceAuthored;
 auto before=PaintedMesh::from_maps(raw,maps,64,policy);const auto original=before->encode();
 const std::vector<uint32_t> vertices{0,1,2,0,2,3};Mesh target;
 for(auto v:vertices){target.positions.push_back(raw.positions[v]);target.normals.push_back(raw.normals[v]);}
 target.indices={0,1,2,3,4,5};target.uv={{.05f,.05f},{.45f,.05f},{.45f,.45f},{.55f,.55f},{.95f,.95f},{.55f,.95f}};
 auto empty=prepare_source_paint(before,target,vertices);const auto empty_bytes=empty->encode();
 Brush brush;brush.from=brush.to={.5f,.5f,0};brush.normal={0,0,1};brush.radius=.5f;
 PaintSettings ink;ink.color={.2f,.8f,.4f,.6f};ink.front_only=false;
 auto painted=empty->snapshot();painted->begin_stroke();painted->stamp(brush,ink);require(painted->end_stroke(),"Actual allocation fixture paints source atlas");
 const auto encoded=painted->encode();size_t total=0;
 for(int phase=0;phase<4;++phase){
  auto operation=[&](std::shared_ptr<PaintedMesh> &candidate){
   if(phase==0)candidate=prepare_source_paint(before,target,vertices);
   if(phase==1){auto out=painted->encode();armed=false;require(out==encoded,"Owning encode output exact");}
   if(phase==2)candidate=PaintedMesh::decode(encoded,nullptr,false,nullptr,nullptr,SourcePaintDecodeContext{true});
   if(phase==3)candidate->stamp(brush,ink);
  };
  auto candidate=empty->snapshot();if(phase==3)candidate->begin_stroke();
  calls=cut=0;hit=false;armed=true;operation(candidate);armed=false;const size_t measured=calls;
  require(measured>0&&measured<65536,"Bounded actual owned allocation count");
  if(phase==3)require(candidate->end_stroke(),"Measured real stamp commits");
  for(size_t boundary=1;boundary<=measured;++boundary){
   candidate=empty->snapshot();if(phase==3)candidate->begin_stroke();
   const auto held=candidate;calls=0;cut=boundary;hit=false;bool failed=false;armed=true;
   try{operation(candidate);}catch(const std::bad_alloc&){failed=true;}armed=false;
   require(hit&&failed,"Every actual allocation is persistently denied without partial success");
   require(candidate==held,"Failed owning factory/reader never publishes a partial object");
   if(phase==3){require(candidate->in_stroke(),"Failed stamp retains cancellable gesture");candidate->cancel_stroke();}
   require(candidate->encode()==empty_bytes&&before->encode()==original&&painted->encode()==encoded,
     "Every denial preserves current/source owners, bytes and earlier paint");
   if(phase==3)candidate->begin_stroke();operation(candidate);
   if(phase==3)require(candidate->end_stroke(),"Actual full stamp retry commits");
   if(phase==0)require(candidate->encode()==empty_bytes,"Preparation retry exact");
   if(phase==2||phase==3)require(candidate->encode()==encoded,"Read/stamp retry exact");
  }
  total+=measured;std::cout<<"PASS phase="<<phase<<" actual persistent allocation boundaries="<<measured<<std::endl;
 }
 size_t polls=0;SourcePaintOptions options;options.cancelled=[&]{++polls;return false;};
 require(prepare_source_paint(before,target,vertices,options)->encode()==empty_bytes,"Counted preparation exact");
 const auto count=polls;require(count>1&&count<65536,"Actual preparation cancellation polls measured");
 for(size_t boundary=1;boundary<=count;++boundary){polls=0;options.cancelled=[&]{return ++polls==boundary;};bool stopped=false;
  try{(void)prepare_source_paint(before,target,vertices,options);}catch(const std::runtime_error&){stopped=true;}
  require(stopped&&polls==boundary&&before->encode()==original,"Each cancellation prevents publication and preserves input");
  require(prepare_source_paint(before,target,vertices)->encode()==empty_bytes,"Complete preparation retry after cancellation exact");
 }
 require(empty->encode()==empty_bytes&&painted->encode()==encoded,"All immutable seed states remain exact");
 std::cout<<"PASS "<<total<<" persistent allocation boundaries and "<<count<<" cancellation boundaries\n";return 0;
}catch(const std::exception &e){armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
