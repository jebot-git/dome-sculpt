// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <memory>
#include <unordered_map>
#include <utility>
namespace dome {
// Snapshot readers share immutable storage; the editing owner detaches before
// mutation. A shared map is never modified in place by a later brush stroke.
template <class Key, class Value, class Hash> class CowMap {
  using Map = std::unordered_map<Key, Value, Hash>;
  std::shared_ptr<Map> data_ = std::make_shared<Map>();
  Map &write() {
    if (!data_.unique())
      data_ = std::make_shared<Map>(*data_);
    return *data_;
  }

public:
  // A held const owner authenticates immutable COW storage across snapshots.
  // Writers detach while this owner exists; weak owners can compare control
  // block identity without retaining a succession of potentially large maps.
  std::shared_ptr<const Map> owner() const { return data_; }
  const Map &read() const { return *data_; }
  auto begin() const { return data_->cbegin(); }
  auto end() const { return data_->cend(); }
  auto find(const Key &k) const { return read().find(k); }
  bool contains(const Key &k) const { return data_->contains(k); }
  size_t size() const { return data_->size(); }
  const Value &at(const Key &k) const { return data_->at(k); }
  Value &operator[](const Key &k) { return write()[k]; }
  auto erase(const Key &k) { return write().erase(k); }
  template <class... Args> auto emplace(Args &&...args) {
    return write().emplace(std::forward<Args>(args)...);
  }
  void clear() {
    if (data_->empty())
      return;
    if (data_.unique())
      data_->clear();
    else
      data_ = std::make_shared<Map>();
  }
  CowMap &operator=(Map map) {
    data_ = std::make_shared<Map>(std::move(map));
    return *this;
  }
};
} // namespace dome
