// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dome::detail {
// One renderer/context owns this ledger across accepted and Pending frames.
// All methods and Store copies/drops belong to its current GL context thread.
// Requested stores are distinct from driver-private heap overhead.
class ContextStoreLedger : public std::enable_shared_from_this<ContextStoreLedger> {
 struct Node;
public:
 enum class Kind {Buffer,Texture,Sampler,VertexArray};
 enum class Domain {Material,Geometry};
 using Group=uint32_t;
 struct Limits {
  size_t material_bytes=1024ull*1024*1024,material_images=1024,material_samplers=128;
  size_t geometry_bytes=1024ull*1024*1024,max_stores=65536,metadata_bytes=128ull*1024*1024;
 };
 struct Usage {
  size_t bytes=0,images=0,samplers=0,stores=0;
  size_t retired_bytes=0,retired_images=0,retired_samplers=0,retired_stores=0;
  size_t peak_bytes=0;
 };
 class Store {
  friend class ContextStoreLedger;
  std::shared_ptr<ContextStoreLedger> ledger_;Node*node_=nullptr;
  Store(std::shared_ptr<ContextStoreLedger> l,Node*n)noexcept:ledger_(std::move(l)),node_(n){}
 public:
  Store()=default;
  Store(const Store&);
  Store(Store&&o)noexcept:ledger_(std::move(o.ledger_)),node_(std::exchange(o.node_,nullptr)){}
  Store&operator=(const Store&o){Store next(o);swap(next);return *this;}
  Store&operator=(Store&&o)noexcept{Store next(std::move(o));swap(next);return *this;}
  ~Store(){reset();}
  void reset()noexcept;
  void swap(Store&o)noexcept{ledger_.swap(o.ledger_);std::swap(node_,o.node_);}
  GLuint name()const noexcept;
  size_t bytes()const noexcept;
  Kind kind()const noexcept;
  Domain domain()const noexcept;
  Group group()const noexcept;
  explicit operator bool()const noexcept{return name()!=0;}
 };
private:
 struct Node {
  Kind kind;Domain domain;Group group;size_t bytes,refs=1,metadata;
  GLuint name=0;bool retired=false;uint64_t epoch=0;
  // Raw internal refs avoid Ledger->dependency Store->Ledger cycles.
  std::vector<Node*> dependencies;
  Node(Kind k,Domain d,Group g,size_t b,size_t m,std::vector<Node*>p):kind(k),domain(d),group(g),bytes(b),metadata(m),dependencies(std::move(p)){}
 };
 Limits limits_;std::array<Usage,2> usage_{};std::vector<Usage> groups_{1};std::list<Node> nodes_;
 size_t metadata_=4096+512;bool terminal_=false,dirty_=false,epoch_exhausted_=false;
 uint64_t used_=0,sealed_=0,completed_=0;GLsync fence_=nullptr;
 enum class Error {None,CreateFence,QueryFence,DeleteStore};Error error_=Error::None;GLenum gl_error_=GL_NO_ERROR;
 static size_t index(Domain d)noexcept{return d==Domain::Material?0:1;}
 static size_t images(Kind k)noexcept{return k==Kind::Texture;}
 static size_t samplers(Kind k)noexcept{return k==Kind::Sampler;}
 void active()const{if(terminal_)throw std::logic_error("GPU store ledger is terminal");if(epoch_exhausted_)throw std::overflow_error("GPU store use epoch exhausted");}
 void record(Error e)noexcept{if(error_==Error::None){error_=e;gl_error_=glGetError();}}
 void report(){if(error_==Error::None)return;const auto e=error_;const auto g=gl_error_;error_=Error::None;gl_error_=GL_NO_ERROR;throw std::runtime_error(std::string(e==Error::CreateFence?"GPU store retirement fence creation failed":e==Error::QueryFence?"GPU store retirement fence query failed":"GPU store deletion failed")+" (GL error "+std::to_string(g)+"); stores and debt retained");}
 static void add(Usage&u,const Node&n){u.bytes+=n.bytes;u.images+=images(n.kind);u.samplers+=samplers(n.kind);++u.stores;u.peak_bytes=std::max(u.peak_bytes,u.bytes);}
 static void retire(Usage&u,const Node&n)noexcept{u.retired_bytes+=n.bytes;u.retired_images+=images(n.kind);u.retired_samplers+=samplers(n.kind);++u.retired_stores;}
 static void remove(Usage&u,const Node&n)noexcept{u.bytes-=n.bytes;u.images-=images(n.kind);u.samplers-=samplers(n.kind);--u.stores;u.retired_bytes-=n.bytes;u.retired_images-=images(n.kind);u.retired_samplers-=samplers(n.kind);--u.retired_stores;}
 static void delete_name(Node&n)noexcept{
  if(!n.name)return;
  switch(n.kind){case Kind::Buffer:glDeleteBuffers(1,&n.name);break;case Kind::Texture:glDeleteTextures(1,&n.name);break;case Kind::Sampler:glDeleteSamplers(1,&n.name);break;case Kind::VertexArray:glDeleteVertexArrays(1,&n.name);break;}
 }
 void drop(Node*n)noexcept{
  if(--n->refs||terminal_)return;
  n->retired=true;n->epoch=used_;retire(usage_[index(n->domain)],*n);retire(groups_[n->group],*n);
 }
 bool retired()const noexcept{return usage_[0].retired_stores||usage_[1].retired_stores;}
 void try_seal(bool force)noexcept{
  if(terminal_||epoch_exhausted_||(!force&&!dirty_))return;
  GLsync next=nullptr;try{next=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);}catch(...){record(Error::CreateFence);return;}
  if(!next){record(Error::CreateFence);return;}
  try{glFlush();}catch(...){glDeleteSync(next);record(Error::CreateFence);return;}
  if(fence_)glDeleteSync(fence_);
  fence_=next;sealed_=used_;dirty_=false;
 }
public:
 ContextStoreLedger():ContextStoreLedger(Limits{}){}
 explicit ContextStoreLedger(Limits l):limits_(l){if(l.metadata_bytes<metadata_)throw std::length_error("GPU store fixed CPU metadata budget exceeded");}
 ContextStoreLedger(const ContextStoreLedger&)=delete;ContextStoreLedger&operator=(const ContextStoreLedger&)=delete;
 ~ContextStoreLedger(){clear(true);}
 // 512B/group includes old-plus-new Usage-vector capacity during growth.
 Group create_group(){active();if(groups_.size()>=1024||metadata_>limits_.metadata_bytes||512>limits_.metadata_bytes-metadata_)throw std::length_error("GPU store group CPU metadata budget exceeded");groups_.emplace_back();metadata_+=512;return Group(groups_.size()-1);}
 Usage usage(Domain d)const noexcept{return usage_[index(d)];}
 Usage usage(Group g)const{if(g>=groups_.size())throw std::out_of_range("Unknown GPU store group");return groups_[g];}
 size_t metadata_bytes()const noexcept{return metadata_;}
 bool terminal()const noexcept{return terminal_;}
 void preflight(Domain d,size_t bytes,size_t names=0,size_t samples=0)const{
  active();const auto&u=usage_[index(d)];const auto limit=d==Domain::Material?limits_.material_bytes:limits_.geometry_bytes;
  if(u.bytes>limit||bytes>limit-u.bytes)throw std::length_error("GPU store old/new/retired requested byte budget exceeded");
  if(d==Domain::Geometry&&(names||samples))throw std::invalid_argument("Geometry ledger owns only buffer/VAO stores");
  if(d==Domain::Material&&(u.images>limits_.material_images||names>limits_.material_images-u.images||u.samplers>limits_.material_samplers||samples>limits_.material_samplers-u.samplers))throw std::length_error("GPU store old/new/retired image/sampler budget exceeded");
 }
 Store create(Kind kind,size_t bytes,Domain domain,Group group,std::span<const Store>dependencies={}){
  active();report();if(group>=groups_.size())throw std::out_of_range("Unknown GPU store group");
  if(domain==Domain::Geometry&&kind!=Kind::Buffer&&kind!=Kind::VertexArray)throw std::invalid_argument("Geometry ledger owns only buffer/VAO stores");
  if((kind==Kind::Sampler||kind==Kind::VertexArray)&&bytes)throw std::invalid_argument("Nonstorage GL name cannot claim requested bytes");
  preflight(domain,bytes,images(kind),samplers(kind));
  if(dependencies.size()>64||nodes_.size()>=limits_.max_stores)throw std::length_error("GPU store/dependency count budget exceeded");
  const size_t metadata=1024+dependencies.size()*sizeof(Node*);
  if(metadata_>limits_.metadata_bytes||metadata>limits_.metadata_bytes-metadata_)throw std::length_error("GPU store CPU ownership metadata budget exceeded");
  auto self=shared_from_this();std::vector<Node*>parents;parents.reserve(dependencies.size());
  for(const auto&p:dependencies){if(p.ledger_.get()!=this||!p.node_||!p.name()||p.node_->retired)throw std::invalid_argument("GPU alias borrowed a foreign or retired store");parents.push_back(p.node_);}
  nodes_.emplace_back(kind,domain,group,bytes,metadata,std::move(parents));auto&n=nodes_.back();for(auto*p:n.dependencies)++p->refs;
  metadata_+=metadata;add(usage_[index(domain)],n);add(groups_[group],n);Store result(std::move(self),&n);
  mark_used();
  switch(kind){case Kind::Buffer:glGenBuffers(1,&n.name);break;case Kind::Texture:glGenTextures(1,&n.name);break;case Kind::Sampler:glGenSamplers(1,&n.name);break;case Kind::VertexArray:glGenVertexArrays(1,&n.name);break;}
  const auto error=glGetError();if(!n.name||error!=GL_NO_ERROR)throw std::runtime_error("GPU store GL name creation failed ("+std::to_string(error)+"); lease retained for retirement");
  return result;
 }
 // Call before actual uploads/copies/draws using any accepted or Pending store.
 // It does not allocate, fence, poll or wait, including unchanged/held views.
 void mark_used()noexcept{if(terminal_)return;if(used_==UINT64_MAX){epoch_exhausted_=true;return;}++used_;dirty_=true;}
 // Force before no-throw frame publication. CPU-only releases afterward are
 // covered by this fence until another mark_used; no late fallible seal needed.
 void seal(bool force=false){active();report();try_seal(force);report();}
 void seal_noexcept(bool force=false)noexcept{try_seal(force);}
 bool collect(){
  active();report();if(!retired())return true;
  // Only debt with no completed/presealed coverage needs a newer fence.
  bool uncovered=false;for(const auto&n:nodes_)if(n.retired&&n.epoch>completed_&&(!fence_||n.epoch>sealed_)){uncovered=true;break;}
  if(uncovered){try_seal(false);report();}
  if(fence_){const auto status=glClientWaitSync(fence_,0,0);if(status==GL_WAIT_FAILED){record(Error::QueryFence);report();}if(status==GL_ALREADY_SIGNALED||status==GL_CONDITION_SATISFIED){completed_=sealed_;glDeleteSync(fence_);fence_=nullptr;}}
  for(auto it=nodes_.begin();it!=nodes_.end();){
   auto&n=*it;if(!n.retired||n.epoch>completed_){++it;continue;}
   delete_name(n);const auto error=glGetError();if(error!=GL_NO_ERROR){if(error_==Error::None){error_=Error::DeleteStore;gl_error_=error;}report();}
   n.name=0;for(auto*p:n.dependencies)drop(p);remove(usage_[index(n.domain)],n);remove(groups_[n.group],n);metadata_-=n.metadata;it=nodes_.erase(it);
  }
  return !retired();
 }
 // Terminal only: no later allocation. Outstanding Store handles remain valid
 // CPU objects with name0; they never become dangling aliases or call GL later.
 void clear(bool releaseGL)noexcept{
  if(terminal_)return;
  terminal_=true;
  if(releaseGL){for(auto kind:{Kind::VertexArray,Kind::Texture,Kind::Sampler,Kind::Buffer})for(auto&n:nodes_)if(n.kind==kind)delete_name(n);if(fence_)glDeleteSync(fence_);}
  for(auto&n:nodes_)n.name=0;
  fence_=nullptr;usage_={};for(auto&u:groups_)u={};dirty_=false;
 }
 void abandon()noexcept{clear(false);}
};
inline ContextStoreLedger::Store::Store(const Store&o):ledger_(o.ledger_),node_(o.node_){if(node_)++node_->refs;}
inline void ContextStoreLedger::Store::reset()noexcept{if(node_){ledger_->drop(node_);node_=nullptr;}ledger_.reset();}
inline GLuint ContextStoreLedger::Store::name()const noexcept{return node_?node_->name:0;}
inline size_t ContextStoreLedger::Store::bytes()const noexcept{return node_?node_->bytes:0;}
inline ContextStoreLedger::Kind ContextStoreLedger::Store::kind()const noexcept{return node_?node_->kind:Kind::Buffer;}
inline ContextStoreLedger::Domain ContextStoreLedger::Store::domain()const noexcept{return node_?node_->domain:Domain::Material;}
inline ContextStoreLedger::Group ContextStoreLedger::Store::group()const noexcept{return node_?node_->group:0;}
} // namespace dome::detail
