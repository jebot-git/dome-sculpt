// SPDX-License-Identifier: GPL-3.0-or-later
// Real synchronous folder/settings/selection/Cancel publication under allocation
// failure. Observe the request token at every allocation: restoring it after a
// failed publish cannot undo cancellation already observed by an old scanner.
// Only the calling thread is faulted; directory workers run normally. The
// public seed workflow initializes a real model before admission is exercised.
#include "dome/document.hpp"
#include <atomic>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <tuple>
#include <unistd.h>
using namespace dome;
namespace {thread_local bool armed=false;thread_local size_t count=0,fail=0;std::atomic<uint64_t> *nonce=nullptr;uint64_t expected=0;bool premature=false;}
void *operator new(size_t n){if(armed){++count;if(nonce&&nonce->load()!=expected)premature=true;if(fail==count)throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}
namespace dome {struct DocumentTransactionTestAccess {
 static auto tokens(Document&d){std::lock_guard lock(d.model_mutex_);return std::tuple{d.import_settings_,d.import_view_,d.import_source_.generation,d.import_source_.revision,d.import_source_.object,d.import_source_.edit_authority,d.import_request_.load(),d.content_,d.next_content_,d.undo_.size(),d.redo_.size()};}
 static auto request(Document&d){return &d.import_request_;}
 static size_t queued(Document&d){std::lock_guard lock(d.file_mutex_);return d.files_.size();}
};}
void require(bool b,const char*m){if(!b)throw std::runtime_error(m);}
void ready(Document&d){auto until=std::chrono::steady_clock::now()+std::chrono::seconds(8);while(!d.frame().state->importing||d.frame().state->importing->busy){if(std::chrono::steady_clock::now()>until)throw std::runtime_error("listing timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
int main()try{
 auto root=std::filesystem::temp_directory_path()/("dome-import-admission-"+std::to_string(getpid()));std::filesystem::create_directories(root);std::ofstream(root/"asset.obj")<<"v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
 {
  Document d;d.seed();require(d.wait_mesh(),"authority model ready");
  auto source=d.begin_import();d.configure_object_grab(true,true);
  require(d.wait_mesh(),"explicit picking ready");
  auto before=DocumentTransactionTestAccess::tokens(d);auto publication=d.frame().state;
  bool refused=false;try{d.import_asset(root/"asset.obj",ImportSettings{},source);}catch(const std::logic_error&){refused=true;}
  require(refused&&DocumentTransactionTestAccess::tokens(d)==before&&d.frame().state==publication&&DocumentTransactionTestAccess::queued(d)==0,
      "native chooser source lease cannot cross an edit-authority change");
  std::cout<<"PASS native import retains the pre-chooser edit-authority lease\n";
 }
 {
  Document d;d.seed();d.prepare_mesh();require(d.wait_asset()&&d.wait_mesh(),"UV fixture prepared");
  d.begin_uv();require(d.wait_uv(),"UV editor ready");
  auto before=DocumentTransactionTestAccess::tokens(d);auto publication=d.frame().state;
  bool refused=false;try{d.open_import_browser(root);}catch(const std::logic_error&){refused=true;}
  require(refused&&DocumentTransactionTestAccess::tokens(d)==before&&d.frame().state==publication&&DocumentTransactionTestAccess::queued(d)==0,
      "folder admission cannot reenter an active editor's file scheduling");
  d.end_uv(false);require(d.wait_uv(),"UV cancellation drains");
  std::cout<<"PASS active UV editing refuses folder admission without changing its draft\n";
 }
 size_t checked=0;
 for(int mode=0;mode<4;++mode){
  auto prepare=[&]{auto d=std::make_unique<Document>();d->seed();require(d->wait_mesh(),"initial mesh");d->open_import_browser(root);ready(*d);return d;};
  auto action=[&](Document&d){if(mode==0)d.open_import_browser(root);else if(mode==1){auto s=d.import_settings();s.paint_resolution=64;d.configure_import(s);}else d.import_action(mode==2?SpatialCommand::ImportUnits:SpatialCommand::ImportCancel);};
  size_t allocations=0;
  {auto d=prepare();nonce=DocumentTransactionTestAccess::request(*d);expected=nonce->load();count=fail=0;premature=false;armed=true;action(*d);armed=false;allocations=count;require(!premature,"request visible before commit");nonce=nullptr;}
  require(allocations>0,"no allocation boundaries");
  for(size_t boundary=1;boundary<=allocations;++boundary){auto d=prepare();auto before=DocumentTransactionTestAccess::tokens(*d);auto publication=d->frame().state;auto scene=d->copy_scene().encode();nonce=DocumentTransactionTestAccess::request(*d);expected=nonce->load();count=0;fail=boundary;premature=false;bool threw=false;armed=true;try{action(*d);}catch(const std::bad_alloc&){threw=true;}armed=false;nonce=nullptr;
   require(threw,"measured boundary was not injected");require(!premature,"old scanner observed premature cancellation");require(DocumentTransactionTestAccess::tokens(*d)==before,"private import/history state rollback mismatch");require(d->frame().state==publication,"published frame changed after failed admission");require(DocumentTransactionTestAccess::queued(*d)==0,"failed admission left queued job");require(d->copy_scene().encode()==scene,"failed import UI changed Scene");++checked;
  }
  std::cout<<"mode "<<mode<<" allocations "<<allocations<<" passed\n";
 }
 std::cout<<checked<<" import admission allocation boundaries passed\n";std::filesystem::remove_all(root);return 0;
}catch(const std::exception&e){armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
