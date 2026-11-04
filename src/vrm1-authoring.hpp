// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-authoring-capture.hpp"
#include "vrm1-painted-material.hpp"
#include <map>
#include <set>
#include <algorithm>
namespace dome::detail {
class Vrm1AuthoringCompilation;
std::shared_ptr<const Vrm1AuthoringCompilation> compile_vrm1_authoring(
    std::shared_ptr<const NativeAuthoringCapture>,const SourceUvBakeOptions &,
    size_t,const std::function<bool()> &);
// Only the actual Scene-owning compiler can create this transport authority.
// Neither arbitrary surfaces nor a boolean claiming equivalence can mint it.
class Vrm1AuthoringCompilation final {
 friend std::shared_ptr<const Vrm1AuthoringCompilation> compile_vrm1_authoring(
    std::shared_ptr<const NativeAuthoringCapture>,const SourceUvBakeOptions &,
    size_t,const std::function<bool()> &);
 std::shared_ptr<const NativeAuthoringCapture> capture_;
 nlohmann::json original_;
 std::vector<ObjectSurface> surfaces_;
 std::map<uint64_t,Vrm1PaintedMaterial> materials_;
 std::set<uint64_t> compiled_;
 std::vector<std::string> diagnostics_;
 Vrm1AuthoringCompilation()=default;
public:
 const NativeAuthoringCapture &capture()const{return *capture_;}
 const auto &original()const{return original_;}
 const auto &surfaces()const{return surfaces_;}
 const auto &diagnostics()const{return diagnostics_;}
 bool owns(const ObjectSurface &surface)const {
  return std::any_of(surfaces_.begin(),surfaces_.end(),[&](const auto &s){return &s==&surface;});
 }
 bool compiled(const ObjectSurface &surface)const {
  if(!owns(surface))throw std::invalid_argument("VRM1 compiled surface is not its sealed owner");
  return compiled_.contains(surface.id);
 }
 const Vrm1PaintedMaterial *material(const ObjectSurface &surface)const {
  if(!owns(surface))throw std::invalid_argument("VRM1 material override is not its sealed surface");
  auto at=materials_.find(surface.id);return at==materials_.end()?nullptr:&at->second;
 }
};
}
