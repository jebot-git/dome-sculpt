// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene-file.hpp"
#include "native-read-scope.hpp"
#include "native-scene-geometry-intern.hpp"
#include "native-scene-payload.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <zlib.h>
using namespace dome;
namespace {
size_t checks=0;
void check(bool v,const char*m){if(!v)throw std::runtime_error(m);++checks;}
template<class F>bool cancelled(F&&f,bool payload=false){try{f();}catch(const std::runtime_error&e){const std::string message=e.what();return message=="Project loading cancelled"||(payload&&message=="Native HDR export/extraction cancelled");}return false;}
Scene fixture(){
 Volume::Voxels cells;for(int x=0;x<32;++x)for(int y=0;y<32;++y)cells.emplace(Int3{x,y,0},float((x+y)%7-3)*.01f);
 const auto v=Volume::from_samples(.1f,std::move(cells));Scene s;s.active().volume=std::make_shared<Volume>(v);s.add("Independent duplicate",v);s.select(1);return s;
}
void scopes(const nlohmann::json&volume){
 std::weak_ptr<int> weak;
 {auto token=std::make_shared<int>(7);weak=token;detail::NativeReadScope scope([keep=std::move(token)]{return *keep!=7;});check(!weak.expired(),"Temporary callback is owned by its read scope");detail::NativeReadScope::check();}
 check(weak.expired(),"Read scope releases its owned callback");
 size_t parent_calls=0;
 {detail::NativeReadScope parent([&]{++parent_calls;return true;});
  {detail::NativeReadScope inner([]{return false;});check(cancelled([]{detail::NativeReadScope::check();}),"False inner reader cannot hide parent cancellation");}
  {detail::NativeReadScope inner({});check(cancelled([]{detail::NativeReadScope::check();}),"Empty inner reader cannot hide parent cancellation");}
 }
 check(parent_calls==2,"Each nested check retains the actual parent authority");
 detail::NativeReadScope::check();
 struct ThrowCopy {bool*fail;ThrowCopy(bool*f):fail(f){}ThrowCopy(const ThrowCopy&o):fail(o.fail){if(*fail)throw std::bad_alloc();}bool operator()()const{return false;}};
 bool fail=false;std::function<bool()> callback=ThrowCopy(&fail);size_t outer=0;
 {detail::NativeReadScope parent([&]{++outer;return false;});fail=true;bool hit=false;
  try{detail::NativeReadScope rejected(callback);}catch(const std::bad_alloc&){hit=true;}
  check(hit,"Callback-copy bad_alloc propagates before publishing a new scope");detail::NativeReadScope::check();
 }
 check(outer==1,"Failed callback construction leaves outer scope installed");
 size_t calls=0;
 {detail::NativeReadScope scope([&]{if(++calls==1)throw std::runtime_error("callback exception");return false;});bool hit=false;
  try{detail::NativeReadScope::check();}catch(const std::runtime_error&e){hit=std::string(e.what())=="callback exception";}
  check(hit,"Actual callback exception propagates");detail::NativeReadScope::check();check(calls==2,"Callback exception clears reentry state for retry");}
 size_t nested_calls=0;bool inside=false;
 {detail::NativeReadScope scope([&]{check(!inside,"Ambient callback does not recursively invoke itself");inside=true;++nested_calls;const auto nested=Volume::decode(volume);check(nested.encode()==volume,"Callback can perform a genuine nested Volume read");inside=false;return false;});detail::NativeReadScope::check();}
 check(nested_calls==1,"Nested native decoding skips only the already executing callback");
}
template<class Decode>void decoding(const char*label,Decode decode,const nlohmann::json&expected){
 size_t count=0;{detail::NativeReadScope scope([&]{++count;return false;});check(decode()==expected,"Complete native baseline retains every encoded field");}
 check(count>4,"Actual decoder reaches bounded internal cancellation checkpoints");
 std::set<size_t> thresholds{1,count/2,count};
 for(auto stop:thresholds){size_t calls=0;std::optional<nlohmann::json> result;
  {detail::NativeReadScope scope([&]{return ++calls==stop;});check(cancelled([&]{result=decode();}),"Actual internal read cancellation propagates");}
  check(!result&&calls==stop,"Cancelled decoder never publishes a partial result");check(decode()==expected,"Cancelled native decoding retries exactly after scope teardown");
 }
 std::cout<<label<<" checkpoints="<<count<<" selected="<<thresholds.size()<<'\n';
}
void pool(){
 using Cells=CowMap<Int3,float,IntHash>;Cells a,b;for(int i=0;i<1024;++i){a[Int3{i,0,0}]=float(i)*.001f;b[Int3{i,0,0}]=float(i)*.001f;}
 detail::NativeSceneGeometryIntern intern;size_t hash_calls=0;
 {detail::NativeReadScope read([&]{++hash_calls;return false;});detail::NativeSceneGeometryIntern::intern_decoded(a,.1f,.4f);}
 const auto own=b.owner();check(own!=a.owner(),"Pool control starts with independent genuine storage");
 for(size_t stop:{size_t(2),hash_calls+1}){size_t calls=0;
  {detail::NativeReadScope read([&]{check(b.owner()==own,"Pool never publishes an alias before all comparisons complete");return ++calls==stop;});check(cancelled([&]{detail::NativeSceneGeometryIntern::intern_decoded(b,.1f,.4f);}),"Hash or equality cancellation propagates before publication");}
  check(b.owner()==own,"Cancelled pool comparison leaves the caller owner exact");
 }
 detail::NativeSceneGeometryIntern::intern_decoded(b,.1f,.4f);check(b.owner()==a.owner(),"Full retry publishes the independently verified equal storage");
 std::cout<<"pool_hash_checkpoints="<<hash_calls<<" equality_cancel="<<hash_calls+1<<'\n';
}
std::vector<uint8_t> bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void wrappers(const Scene&s,const std::filesystem::path&dir){
 const auto wire=s.encode();std::filesystem::create_directories(dir);const auto path=dir/"read-cancel.dsculpt";s.save(path);const auto before=bytes(path);
 size_t count=0;check(load_scene_file(path,[&]{++count;return false;}).scene.encode()==wire,"Actual file reader returns the complete native Scene");check(count>8,"File reader propagates callbacks into large native decoding");
 for(size_t stop:{size_t(1),count/2,count}){size_t calls=0;std::optional<LoadedSceneFile> result;check(cancelled([&]{result=load_scene_file(path,[&]{return ++calls==stop;});}),"Actual file reader cancellation rejects publication");check(!result&&calls==stop,"File cancellation reaches its selected boundary without partial publication");check(bytes(path)==before,"File reader cancellation preserves every source byte");check(load_scene_file(path).scene.encode()==wire,"Actual file cancellation permits an exact retry");}
 const auto captured=detail::capture_native_scene_payload(s,{});
 for(bool compressed:{false,true}){
 auto payload=captured;if(compressed){uLongf size=compressBound(payload.stored.size());std::vector<uint8_t> packed(size);check(compress2(packed.data(),&size,payload.stored.data(),payload.stored.size(),Z_DEFAULT_COMPRESSION)==Z_OK,"Independent zlib fixture forms a genuine compressed native payload");packed.resize(size);payload.stored=std::move(packed);payload.compressed=true;}
 const auto saved=payload.stored;size_t restore_calls=0;
 check(detail::restore_native_scene_payload(payload.stored,payload.compressed,payload.decoded_bytes,256ull*1024*1024,[&]{++restore_calls;return false;}).encode()==wire,"Direct authenticated native payload restores exactly");
 check(restore_calls>8,"Native payload restoration propagates its callback into decoding");
 for(size_t stop:{size_t(1),restore_calls/2,restore_calls}){size_t calls=0;std::optional<Scene> result;check(cancelled([&]{result=detail::restore_native_scene_payload(payload.stored,payload.compressed,payload.decoded_bytes,256ull*1024*1024,[&]{return ++calls==stop;});},true),"Direct native payload cancellation propagates");check(!result&&calls==stop&&payload.stored==saved,"Cancelled payload restoration preserves encoded input and publishes nothing");check(detail::restore_native_scene_payload(payload.stored,payload.compressed,payload.decoded_bytes,256ull*1024*1024,{}).encode()==wire,"Payload cancellation permits an exact retry");}
 std::cout<<"file_checkpoints="<<count<<" payload_compressed="<<compressed<<" payload_checkpoints="<<restore_calls<<'\n';
 }
}
}
int main(int argc,char**argv)try{
 const auto s=fixture();const auto wire=s.encode(),v=s.active().volume->encode();const auto original=nlohmann::json::to_cbor(wire);
 scopes(v);decoding("Volume",[&]{return Volume::decode(v).encode();},v);decoding("Scene",[&]{return Scene::decode(wire).encode();},wire);pool();wrappers(s,argc>1?argv[1]:"native-read-cancel-output");
 check(nlohmann::json::to_cbor(wire)==original&&s.encode()==wire,"All decoder/exception controls preserve original Scene and complete wire");
 std::cout<<"PASS "<<checks<<" native read cancellation controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
