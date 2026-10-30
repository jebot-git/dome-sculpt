// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "math.hpp"
#include "sculpt-storage.hpp"
#include <bit>
#include <iterator>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>
namespace dome {
// Volume-only 16^3 adaptive pages behind 64 shared ordered directory shards.
// Views and iterators never flatten. Only materialize() explicitly exports a
// map.
template <class Key, class Value, class Hash> class CowMap {
  static_assert(std::is_same_v<Key, Int3>);
  struct Entry {
    uint16_t index;
    Value value;
  };
  template <class T>
  using Vector = std::vector<T, storage_detail::Allocator<T>>;
  struct Page {
    Vector<Entry> sparse;
    Vector<Value> dense;
    Vector<uint64_t> bits;
    size_t count = 0;
    Page() { storage_detail::page_live(1); }
    Page(const Page &p)
        : sparse(p.sparse), dense(p.dense), bits(p.bits), count(p.count) {
      storage_detail::page_live(1);
      if (!dense.empty())
        storage_detail::page_dense(1);
      storage_detail::copied(2, sparse.size() * sizeof(Entry) +
                                    dense.size() * sizeof(Value) +
                                    bits.size() * 8);
    }
    ~Page() {
      if (!dense.empty())
        storage_detail::page_dense(-1);
      storage_detail::page_live(-1);
    }
    size_t bytes() const {
      return sizeof(Page) + 128 + sparse.capacity() * sizeof(Entry) +
             dense.capacity() * sizeof(Value) + bits.capacity() * 8;
    }
    const Value *get(uint16_t i) const {
      if (!dense.empty())
        return (bits[i / 64] & (uint64_t(1) << (i % 64))) ? &dense[i] : nullptr;
      auto p = std::lower_bound(sparse.begin(), sparse.end(), i,
                                [](auto &e, auto n) { return e.index < n; });
      return p != sparse.end() && p->index == i ? &p->value : nullptr;
    }
    uint16_t next(unsigned i) const {
      if (i >= 4096)
        return 4096;
      if (dense.empty()) {
        auto p = std::lower_bound(sparse.begin(), sparse.end(), i,
                                  [](auto &e, auto n) { return e.index < n; });
        return p == sparse.end() ? 4096 : p->index;
      }
      unsigned word = i / 64;
      uint64_t remaining = bits[word] & (~uint64_t(0) << (i % 64));
      while (!remaining && ++word < 64)
        remaining = bits[word];
      return word == 64 ? 4096
                        : uint16_t(word * 64 + std::countr_zero(remaining));
    }
    Value &access(uint16_t i) {
      if (!dense.empty()) {
        auto mask = uint64_t(1) << (i % 64);
        if (!(bits[i / 64] & mask)) {
          bits[i / 64] |= mask;
          dense[i] = Value{};
          ++count;
        }
        return dense[i];
      }
      auto p = std::lower_bound(sparse.begin(), sparse.end(), i,
                                [](auto &e, auto n) { return e.index < n; });
      if (p != sparse.end() && p->index == i)
        return p->value;
      if (count < 1023) {
        p = sparse.insert(p, {i, Value{}});
        ++count;
        return p->value;
      }
      Vector<Value> values(4096);
      Vector<uint64_t> occupancy(64);
      for (auto &e : sparse) {
        values[e.index] = e.value;
        occupancy[e.index / 64] |= uint64_t(1) << (e.index % 64);
      }
      occupancy[i / 64] |= uint64_t(1) << (i % 64);
      dense = std::move(values);
      bits = std::move(occupancy);
      Vector<Entry>().swap(sparse);
      ++count;
      storage_detail::page_dense(1);
      return dense[i];
    }
    void erase(uint16_t i) {
      if (!dense.empty() && count <= 257) {
        Vector<Entry> values;
        values.reserve(count - 1);
        for (auto index = next(0); index < 4096;
             index = next(unsigned(index) + 1))
          if (index != i)
            values.push_back({index, dense[index]});
        sparse = std::move(values);
        Vector<Value>().swap(dense);
        Vector<uint64_t>().swap(bits);
        storage_detail::page_dense(-1);
      } else if (!dense.empty())
        bits[i / 64] &= ~(uint64_t(1) << (i % 64));
      else {
        auto p = std::lower_bound(sparse.begin(), sparse.end(), i,
                                  [](auto &e, auto n) { return e.index < n; });
        sparse.erase(p);
      }
      --count;
    }
  };
  using PagePtr = std::shared_ptr<Page>;
  using Directory =
      std::map<Key, PagePtr, std::less<Key>,
               storage_detail::Allocator<std::pair<const Key, PagePtr>>>;
  struct Shard {
    Directory pages;
    size_t page_bytes = 0;
    Shard() = default;
    Shard(const Shard &s) : pages(s.pages), page_bytes(s.page_bytes) {
      storage_detail::copied(
          1, pages.size() * (sizeof(typename Directory::value_type) + 32));
    }
    size_t bytes() const {
      return sizeof(Shard) + 64 +
             pages.size() * (sizeof(typename Directory::value_type) + 96) +
             page_bytes;
    }
  };
  static int divide(int n) { return n / 16 - (n % 16 < 0); }
  static Key chunk(Key p) { return {divide(p.x), divide(p.y), divide(p.z)}; }
  static uint16_t local(Key p) {
    auto c = chunk(p);
    return uint16_t((p.x - int64_t(c.x) * 16) |
                    ((p.y - int64_t(c.y) * 16) << 4) |
                    ((p.z - int64_t(c.z) * 16) << 8));
  }
  static Key global(Key c, uint16_t i) {
    return {int(int64_t(c.x) * 16 + (i & 15)),
            int(int64_t(c.y) * 16 + ((i >> 4) & 15)),
            int(int64_t(c.z) * 16 + (i >> 8))};
  }
  static size_t bucket(Key c) { return Hash{}(c) & 63; }

public:
  class Storage {
    friend class CowMap;
    std::array<std::shared_ptr<Shard>, 64> shards_{};
    size_t count_ = 0, bytes_ = 0;
    Shard &write_shard(Key c) {
      auto &s = shards_[bucket(c)];
      if (!s) {
        s = storage_detail::shared<Shard>();
        bytes_ += s->bytes();
      } else if (!s.unique())
        s = storage_detail::shared<Shard>(*s);
      return *s;
    }
    Value &access(Key k) {
      auto c = chunk(k);
      auto &s = write_shard(c);
      size_t old = s.bytes();
      auto p = s.pages.find(c);
      bool new_page = p == s.pages.end();
      if (new_page) {
        // Build the first payload before publishing a directory entry. Either
        // allocation can fail; an empty half-published page must not survive.
        auto page = storage_detail::shared<Page>();
        auto &value = page->access(local(k));
        p = s.pages.emplace(c, std::move(page)).first;
        ++count_;
        s.page_bytes += p->second->bytes();
        bytes_ = bytes_ - old + s.bytes();
        return value;
      }
      auto &page = p->second;
      const size_t before = page->bytes();
      if (!page.unique())
        page = storage_detail::shared<Page>(*page);
      bool exists = page->get(local(k));
      auto &value = page->access(local(k));
      if (!exists)
        ++count_;
      s.page_bytes = s.page_bytes - before + page->bytes();
      bytes_ = bytes_ - old + s.bytes();
      return value;
    }
    void remove(Key k) {
      auto c = chunk(k);
      auto &s = write_shard(c);
      size_t old = s.bytes();
      auto p = s.pages.find(c);
      const size_t before = p->second->bytes();
      if (!p->second.unique())
        p->second = storage_detail::shared<Page>(*p->second);
      p->second->erase(local(k));
      --count_;
      if (!p->second->count) {
        s.page_bytes -= before;
        s.pages.erase(p);
      } else
        s.page_bytes = s.page_bytes - before + p->second->bytes();
      bytes_ = bytes_ - old + s.bytes();
    }

  public:
    Storage() = default;
    Storage(const Storage &s)
        : shards_(s.shards_), count_(s.count_), bytes_(s.bytes_) {
      storage_detail::copied(0, sizeof(shards_));
    }
    const Value *get(Key k) const {
      auto c = chunk(k);
      auto &s = shards_[bucket(c)];
      if (!s)
        return nullptr;
      auto p = s->pages.find(c);
      return p == s->pages.end() ? nullptr : p->second->get(local(k));
    }
    const Value &at(Key k) const {
      auto p = get(k);
      if (!p)
        throw std::out_of_range("Missing sculpt sample");
      return *p;
    }
    bool contains(Key k) const { return get(k) != nullptr; }
    size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }
    size_t retained_bytes() const { return sizeof(Storage) + 64 + bytes_; }
    std::shared_ptr<const void> page_owner(Key k) const {
      auto c = chunk(k);
      auto &s = shards_[bucket(c)];
      if (!s)
        return {};
      auto p = s->pages.find(c);
      return p == s->pages.end() ? std::shared_ptr<const void>{} : p->second;
    }
  };
  using Owner = std::shared_ptr<const Storage>;
  class Iterator {
    Owner owner_;
    size_t shard_ = 64;
    typename Directory::const_iterator page_{};
    uint16_t slot_ = 4096;
    mutable std::optional<std::pair<Key, Value>> value_;
    void advance() {
      while (owner_ && shard_ < 64) {
        auto &s = owner_->shards_[shard_];
        if (s && page_ != s->pages.end()) {
          slot_ = page_->second->next(0);
          return;
        }
        if (++shard_ == 64)
          break;
        if (owner_->shards_[shard_])
          page_ = owner_->shards_[shard_]->pages.begin();
      }
      owner_.reset();
    }
    friend class CowMap;

  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = std::pair<Key, Value>;
    using difference_type = std::ptrdiff_t;
    using reference = const value_type &;
    using pointer = const value_type *;
    Iterator() = default;
    explicit Iterator(Owner o) : owner_(std::move(o)), shard_(0) {
      if (owner_->shards_[0])
        page_ = owner_->shards_[0]->pages.begin();
      advance();
    }
    Iterator(Owner o, Key k)
        : owner_(std::move(o)), shard_(bucket(chunk(k))), slot_(local(k)) {
      auto &s = owner_->shards_[shard_];
      if (!s) {
        owner_.reset();
        return;
      }
      page_ = s->pages.find(chunk(k));
      if (page_ == s->pages.end() || !page_->second->get(slot_))
        owner_.reset();
    }
    reference operator*() const {
      if(!value_)value_.emplace(global(page_->first, slot_), *page_->second->get(slot_));
      return *value_;
    }
    pointer operator->() const { return &**this; }
    Iterator &operator++() {
      value_.reset();
      slot_ = page_->second->next(unsigned(slot_) + 1);
      if (slot_ == 4096) {
        ++page_;
        advance();
      }
      return *this;
    }
    Iterator operator++(int) {
      auto old = *this;
      ++*this;
      return old;
    }
    bool operator==(const Iterator &i) const {
      if (!owner_ || !i.owner_)
        return !owner_ && !i.owner_;
      return owner_ == i.owner_ && shard_ == i.shard_ && page_ == i.page_ &&
             slot_ == i.slot_;
    }
  };
  class View {
    Owner owner_;

  public:
    explicit View(Owner o) : owner_(std::move(o)) {}
    size_t size() const { return owner_->size(); }
    bool empty() const { return owner_->empty(); }
    Iterator begin() const { return Iterator(owner_); }
    Iterator end() const { return {}; }
    Iterator find(Key k) const { return Iterator(owner_, k); }
    bool contains(Key k) const { return owner_->contains(k); }
    const Value &at(Key k) const { return owner_->at(k); }
    const Owner &owner() const { return owner_; }
    std::unordered_map<Key, Value, Hash> materialize() const {
      std::unordered_map<Key, Value, Hash> result;
      result.reserve(size());
      for (auto [k, v] : *this)
        result.emplace(k, v);
      return result;
    }
    template <class Range> bool operator==(const Range &r) const {
      if (size() != r.size())
        return false;
      for (auto [k, v] : *this) {
        auto i = r.find(k);
        if (i == r.end() || i->second != v)
          return false;
      }
      return true;
    }
    bool operator==(const View &r) const {
      if (owner_ == r.owner_)
        return true;
      return this->template operator== <View>(r);
    }
    template <class Range>
    friend bool operator==(const Range &l, const View &r) {
      return r == l;
    }
  };

private:
  std::shared_ptr<Storage> empty_, data_;
  Storage &write() {
    if (!data_.unique())
      data_ = storage_detail::shared<Storage>(*data_);
    return *data_;
  }

public:
  CowMap() : empty_(storage_detail::shared<Storage>()), data_(empty_) {}
  Owner owner() const { return data_; }
  View read() const { return View(data_); }
  Iterator begin() const { return read().begin(); }
  Iterator end() const { return {}; }
  Iterator find(Key k) const { return read().find(k); }
  bool contains(Key k) const { return data_->contains(k); }
  const Value *get(Key k) const { return data_->get(k); }
  size_t size() const { return data_->size(); }
  bool empty() const { return data_->empty(); }
  const Value &at(Key k) const { return data_->at(k); }
  Value &operator[](Key k) { return write().access(k); }
  size_t erase(Key k) {
    if (!contains(k))
      return 0;
    write().remove(k);
    return 1;
  }
  std::pair<Iterator, bool> emplace(Key k, Value v) {
    if (contains(k))
      return {find(k), false};
    (*this)[k] = v;
    return {find(k), true};
  }
  std::pair<Iterator, bool> try_emplace(Key k, Value v) {
    return emplace(k, v);
  }
  void clear() noexcept { data_ = empty_; }
  CowMap &operator=(std::unordered_map<Key, Value, Hash> map) {
    CowMap next;
    for (auto [k, v] : map)
      next.emplace(k, v);
    data_ = std::move(next.data_);
    return *this;
  }
};
} // namespace dome
