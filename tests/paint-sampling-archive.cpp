// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_PAINT_GRID_FIXTURE_ONLY
#include "paint-sampling-grid.cpp"
#include <fstream>
#include <nlohmann/json.hpp>
namespace {
bool persisted_equal(const PaintedMesh &a,const PaintedMesh &b){
 PaintImageArchive ai,bi;PaintSamplingArchive ad,bd;
 return a.encode(&ai,nullptr,&ad)==b.encode(&bi,nullptr,&bd)&&ai.encode()==bi.encode()&&ad.encode()==bd.encode();
}
}
int main(int argc,char **argv)try{
 auto source=grid_source();auto authored=welded();const std::array<uint32_t,6> corners{0,1,2,3,4,5};
 auto first=retain_paint_sampling(source->mesh(),{},authored,corners);auto paint=retain_paint_grid(*source,authored,first);
 PaintImageArchive images;PaintSamplingArchive domains;
 const auto old_source=source->encode(&images);check(source->encode(&images,nullptr,&domains)==old_source&&domains.size()==0,"Unused explicit sampling context leaves legacy implicit Paint1 bytes unchanged");
 auto envelope=paint->encode(&images,nullptr,&domains);auto pool=domains.encode(),image_data=images.encode();
 check(envelope.size()==3&&envelope.at("version")==2&&!envelope.contains("mesh")&&envelope.at("payload").contains("mesh"),"Genuine Paint2 envelope omits legacy root mesh and requires its distinct bounded pool");
 check(refuses([&]{PaintedMesh::decode(envelope);})&&refuses([&]{PaintedMesh::decode(envelope,&images);})&&refuses([&]{PaintedMesh::decode(envelope,&images,false,nullptr);}),"Every legacy and context-free new overload refuses domain-bearing Paint before bulk decode");
 auto decoded_images=PaintImageArchive::decode(image_data);auto decoded_domains=PaintSamplingArchive::decode(pool);
 check(refuses([&]{decoded_domains.require_reachable();}),"Unreferenced decoded domain cannot be accepted as an orphan");
 auto restored=PaintedMesh::decode(envelope,&decoded_images,false,nullptr,&decoded_domains);decoded_domains.require_reachable();
 check(restored->valid_paint_sampling_attachment()&&same(restored->mesh(),paint->mesh())&&restored->paint_sampling_domain()->corner_coordinates(restored->mesh())==first->corner_coordinates(authored),"Paint2 decoder reconstructs exact current authored REST and independently sealed corner basis");
 check(persisted_equal(*paint,*restored)&&images_equal(*paint->composite(Material{}),*restored->composite(Material{})),"Every editable channel, hidden layer, sharp coverage, metadata and composite image survives envelope restart exactly");
 auto second=PaintedMesh::decode(envelope,&decoded_images,false,nullptr,&decoded_domains);check(second->paint_sampling_domain()==restored->paint_sampling_domain(),"Separate paints resolve the exact shared immutable domain owner rather than equal copies");
 ink(*second);auto live=paint->snapshot();ink(*live);check(persisted_equal(*live,*second),"Actual all-channel painting after decoded-domain restart retains the same writable texels");
 auto moved=authored;for(auto &uv:moved.uv){float x=uv[0]-.5f,y=uv[1]-.5f;uv={.5f-.7f*y,.5f+.7f*x};}
 auto repeated=retain_paint_sampling(authored,first,moved,corners);domains.add(repeated);auto shared=domains.encode();check(shared.at("bases").size()==1&&shared.at("domains").size()==2,"Repeated UV records deduplicate original basis storage while retaining independent current corner maps");
 check(refuses([&]{PaintSamplingArchiveLimits limits;limits.max_domains=1;auto limited=PaintSamplingArchive::decode(shared,limits);}),"Domain count quota is checked before decoding geometry");
 for(unsigned mode=0;mode<10;++mode){auto bad=pool;
  if(mode==0)bad["version"]=2;
  if(mode==1)bad["extra"]=1;
  if(mode==2)bad["bases"][0]["extra"]=1;
  if(mode==3)bad["domains"][0]["basis"]=99;
  if(mode==4)bad["domains"][0]["corners"].get_binary()[0]=255;
  if(mode==5){auto &b=bad["domains"][0]["corners"].get_binary();std::copy_n(b.begin(),12,b.begin()+12);}
  if(mode==6)bad["bases"].push_back(bad["bases"][0]);
  if(mode==7)bad["bases"][0]["uv"].get_binary()[3]=255;
  if(mode==8)bad["bases"][0]["pins"].get_binary()[0]=2;
  if(mode==9)bad["domains"][0]["corners"].get_binary().pop_back();
  check(refuses([&]{PaintSamplingArchive::decode(bad);}),"Malformed version/shape/reference/corner/seam/basis pool refuses without borrowing authority");
 }
 for(unsigned mode=0;mode<5;++mode){auto bad=envelope;if(mode==0)bad["version"]=1;if(mode==1)bad["mesh"]=bad["payload"]["mesh"];if(mode==2)bad["domain"]=-1;if(mode==3)bad["domain"]=UINT64_MAX;if(mode==4)bad["payload"]["version"]=2;
  check(refuses([&]{PaintedMesh::decode(bad,&decoded_images,false,nullptr,&decoded_domains);}),"Paint envelope cannot downgrade, mix legacy shape, use invalid pool reference or nest another envelope");}
 auto wrong=envelope;auto &positions=wrong["payload"]["mesh"]["positions"].get_binary();positions[2]^=1;
 check(refuses([&]{PaintedMesh::decode(wrong,&decoded_images,false,nullptr,&decoded_domains);}),"Equal-count but changed REST cannot attach a borrowed sampling pool");
 auto legacy=old_source;legacy["domain"]=0;check(refuses([&]{PaintedMesh::decode(legacy,&decoded_images,false,nullptr,&decoded_domains);}),"Bare domain keys cannot hide in implicit Paint1 records");
 for(unsigned mode=0;mode<2;++mode){PaintSamplingArchiveLimits limits;if(mode==0)limits.max_bytes=first->storage_bytes()-1;else limits.geometry.max_work=1;check(refuses([&]{PaintSamplingArchive::decode(pool,limits);}),"Real retained byte and geometry-work quotas apply before accepting a domain");}
 size_t polls=0;PaintSamplingArchiveLimits measured;measured.geometry.cancelled=[&]{++polls;return false;};PaintSamplingArchive::decode(pool,measured);check(polls>5,"Real pool preflight/capture/validation cancellation stages are measured");
 for(auto end:{size_t(1),polls/2,polls}){size_t p=0;auto limits=measured;limits.geometry.cancelled=[&]{return ++p>=end;};check(refuses([&]{PaintSamplingArchive::decode(pool,limits);})&&p==end,"Early/middle/final archive cancellation never exposes a partial basis");}
 allocation::calls=0;allocation::measure=true;auto count_pool=PaintSamplingArchive::decode(pool);auto counted=PaintedMesh::decode(envelope,&decoded_images,false,nullptr,&count_pool);allocation::measure=false;auto count=allocation::calls;check(count>30,"All actual pool and Paint2 reconstruction allocations are measured");
 for(size_t prefix=0;prefix<count;++prefix){allocation::remaining=long(prefix);allocation::hit=false;bool failed=false;try{auto p=PaintSamplingArchive::decode(pool);PaintedMesh::decode(envelope,&decoded_images,false,nullptr,&p);}catch(const std::bad_alloc&){failed=true;}catch(...){allocation::remaining=-1;throw;}allocation::remaining=-1;
  check(failed&&allocation::hit&&persisted_equal(*restored,*paint),"Every persistent allocation prefix preserves complete source paint/grid/domain and permits retry");
  auto p=PaintSamplingArchive::decode(pool);auto retry=PaintedMesh::decode(envelope,&decoded_images,false,nullptr,&p);check(persisted_equal(*paint,*retry),"Every failed archive prefix can retry exact complete reconstruction");
 }
 if(argc==2){std::ofstream out(argv[1],std::ios::binary);auto bytes=nlohmann::json::to_cbor({{"paint",envelope},{"images",image_data},{"pool",pool}});out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));}
 std::cout<<"PASS "<<checks<<" Paint2 structural archive checks, allocations="<<count<<std::endl;return 0;
}catch(const std::exception &e){allocation::remaining=-1;allocation::measure=false;std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
