// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "sculpt-surface-chunks.hpp"
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>
namespace dome::detail {
inline size_t sculpt_index_sum(size_t a,size_t b) {
  if(b>SIZE_MAX-a)throw std::length_error("Sculpt index accounting overflow");
  return a+b;
}
// Persistent AVL directory. Updates clone only the search/rotation path; a
// complete next manifest does not copy all previous tile/edge metadata. There
// is no parent pointer or global cache retaining obsolete manifests.
template<class K,class V,class Traits> struct PersistentSculptIndex {
  struct Node;
  using Ptr = std::shared_ptr<const Node>;
  struct Node {
    K key;
    V value;
    Ptr left,right;
    unsigned height;
    size_t count,payload_bytes;
    SculptChunkBounds bounds;
    Node(K k,V v,Ptr a,Ptr b) : key(std::move(k)),value(std::move(v)),
        left(std::move(a)),right(std::move(b)),
        height(1+std::max(h(left),h(right))),count(sculpt_index_sum(1,sculpt_index_sum(n(left),n(right)))),
        payload_bytes(sculpt_index_sum(Traits::bytes(value),sculpt_index_sum(left?left->payload_bytes:0,right?right->payload_bytes:0))),
        bounds(Traits::bounds(value)) {
      if(left)bounds.include(left->bounds);
      if(right)bounds.include(right->bounds);
    }
  };
  Ptr root;
  static unsigned h(const Ptr &p) { return p?p->height:0; }
  static size_t n(const Ptr &p) { return p?p->count:0; }
  static Ptr node(const K &k,const V &v,Ptr a,Ptr b,auto &w) {
    w.tick();
    // Requested node/control-block storage plus a conservative allocation
    // allowance. This is an owning working budget, not an RSS measurement.
    w.charge(sizeof(Node)+128);
    return std::make_shared<const Node>(k,v,std::move(a),std::move(b));
  }
  static Ptr balanced(const K &k,const V &v,Ptr a,Ptr b,auto &w) {
    if(h(a)>h(b)+1) {
      if(h(a->left)>=h(a->right))
        return node(a->key,a->value,a->left,node(k,v,a->right,b,w),w);
      auto c=a->right;
      return node(c->key,c->value,node(a->key,a->value,a->left,c->left,w),
                  node(k,v,c->right,b,w),w);
    }
    if(h(b)>h(a)+1) {
      if(h(b->right)>=h(b->left))
        return node(b->key,b->value,node(k,v,a,b->left,w),b->right,w);
      auto c=b->left;
      return node(c->key,c->value,node(k,v,a,c->left,w),
                  node(b->key,b->value,c->right,b->right,w),w);
    }
    return node(k,v,std::move(a),std::move(b),w);
  }
  static Ptr put(Ptr p,const K &k,const V &v,auto &w) {
    w.tick();
    if(!p)return node(k,v,{},{},w);
    if(k<p->key)return balanced(p->key,p->value,put(p->left,k,v,w),p->right,w);
    if(p->key<k)return balanced(p->key,p->value,p->left,put(p->right,k,v,w),w);
    return node(k,v,p->left,p->right,w);
  }
  static Ptr remove(Ptr p,const K &k,auto &w) {
    w.tick();
    if(!p)return {};
    if(k<p->key)return balanced(p->key,p->value,remove(p->left,k,w),p->right,w);
    if(p->key<k)return balanced(p->key,p->value,p->left,remove(p->right,k,w),w);
    if(!p->left)return p->right;
    if(!p->right)return p->left;
    auto next=p->right.get();while(next->left) { w.tick(); next=next->left.get(); }
    return balanced(next->key,next->value,p->left,remove(p->right,next->key,w),w);
  }
  const V *find(const K &k) const {
    auto p=root.get();
    while(p) { if(k<p->key)p=p->left.get();else if(p->key<k)p=p->right.get();else return &p->value; }
    return nullptr;
  }
  void put(const K &k,const V &v,auto &w) { root=put(root,k,v,w); }
  void erase(const K &k,auto &w) { root=remove(root,k,w); }
  template<class F> static void visit(const Ptr &p,F &fn,auto &w) {
    if(!p)return;
    visit(p->left,fn,w);w.tick();fn(p->key,p->value);visit(p->right,fn,w);
  }
  template<class F> void visit(F fn,auto &w) const { visit(root,fn,w); }
};
} // namespace dome::detail
