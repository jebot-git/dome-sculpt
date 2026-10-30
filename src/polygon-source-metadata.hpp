// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/polygon.hpp"
#include <nlohmann/json.hpp>
namespace dome::polygon_source_detail {
struct Face {
  uint64_t source_face = 0, first_triangle = 0, triangle_count = 0;
  std::vector<uint64_t> vertices;
};
struct Metadata {
  bool version_present = false, vertices_present = false,
       corners_present = false, polygons_present = false;
  uint64_t version = 0;
  std::vector<uint64_t> vertices, corners;
  std::vector<Face> polygons;
};
// DOM JSON destruction itself may allocate. Keep this temporary worker parse
// entirely SAX/native so allocation failure cannot escape a noexcept DOM
// destructor. Unknown primitive extras remain ignored, never reserialized.
template <class Visit>
class Reader final : public nlohmann::json_sax<nlohmann::json> {
  using Json = nlohmann::json;
  enum class Kind {
    Root,
    Face,
    Vertices,
    Corners,
    Polygons,
    Loop,
    IgnoredObject,
    IgnoredArray
  };
  enum class Key {
    None,
    Unknown,
    Version,
    Vertices,
    Corners,
    Polygons,
    SourceFace,
    First,
    Count
  };
  struct Frame {
    Kind kind;
    Key key = Key::None;
    uint8_t seen = 0;
    Face face;
  };
  Metadata out_;
  std::vector<Frame> stack_;
  Visit &visit_;
  size_t source_limit_, corner_count_ = 0;
  const PolygonLimits &limits_;
  bool started_ = false, complete_ = false;
  bool ignored() const {
    return !stack_.empty() && (stack_.back().kind == Kind::IgnoredObject ||
                               stack_.back().kind == Kind::IgnoredArray ||
                               stack_.back().key == Key::Unknown);
  }
  void push(Kind kind) {
    if (stack_.size() >= 64)
      throw std::length_error(
          "Original polygon metadata nesting exceeds its budget");
    stack_.push_back({kind});
  }
  bool other() {
    visit_();
    if (!ignored())
      return false;
    if (stack_.back().key == Key::Unknown)
      stack_.back().key = Key::None;
    return true;
  }
  bool integer(uint64_t value) {
    visit_();
    if (stack_.empty())
      return false;
    auto &f = stack_.back();
    if (ignored()) {
      if (f.key == Key::Unknown)
        f.key = Key::None;
      return true;
    }
    if (f.kind == Kind::Vertices) {
      if (out_.vertices.size() >= source_limit_)
        throw std::length_error("Original vertex witness exceeds its domain");
      out_.vertices.push_back(value);
    } else if (f.kind == Kind::Corners) {
      if (out_.corners.size() >= source_limit_)
        throw std::length_error("Original corner witness exceeds its domain");
      out_.corners.push_back(value);
    } else if (f.kind == Kind::Loop) {
      if (stack_.size() < 2)
        return false;
      auto &loop = stack_[stack_.size() - 2].face.vertices;
      if (loop.size() >= limits_.maximum_face_corners ||
          corner_count_ >= limits_.maximum_corners)
        throw std::length_error("Original N-gon corner budget exceeded");
      ++corner_count_;
      loop.push_back(value);
    } else if (f.kind == Kind::Root && f.key == Key::Version) {
      out_.version = value;
      f.key = Key::None;
    } else if (f.kind == Kind::Face) {
      if (f.key == Key::SourceFace)
        f.face.source_face = value;
      else if (f.key == Key::First)
        f.face.first_triangle = value;
      else if (f.key == Key::Count)
        f.face.triangle_count = value;
      else
        return false;
      f.key = Key::None;
    } else
      return false;
    return true;
  }

public:
  Reader(Visit &visit, size_t source_limit, const PolygonLimits &limits)
      : visit_(visit), source_limit_(source_limit), limits_(limits) {
    if (source_limit > limits.maximum_corners)
      throw std::length_error(
          "Original vertex witness exceeds its resource budget");
  }
  bool null() override { return other(); }
  bool boolean(bool) override { return other(); }
  bool number_integer(typename Json::number_integer_t n) override {
    return n >= 0 ? integer(uint64_t(n)) : other();
  }
  bool number_unsigned(typename Json::number_unsigned_t n) override {
    return integer(n);
  }
  bool number_float(typename Json::number_float_t,
                    const typename Json::string_t &) override {
    return other();
  }
  bool string(typename Json::string_t &) override { return other(); }
  bool binary(typename Json::binary_t &) override { return other(); }
  bool key(typename Json::string_t &name) override {
    visit_();
    if (stack_.empty())
      return false;
    auto &f = stack_.back();
    if (f.kind == Kind::IgnoredObject)
      return true;
    if (f.kind == Kind::Root) {
      if (name == "dome_source_polygons_version") {
        if (out_.version_present)
          return false;
        out_.version_present = true;
        f.key = Key::Version;
      } else if (name == "dome_source_vertices") {
        if (out_.vertices_present)
          return false;
        out_.vertices_present = true;
        f.key = Key::Vertices;
      } else if (name == "dome_source_corners") {
        if (out_.corners_present)
          return false;
        out_.corners_present = true;
        f.key = Key::Corners;
      } else if (name == "dome_source_polygons") {
        if (out_.polygons_present)
          return false;
        out_.polygons_present = true;
        f.key = Key::Polygons;
      } else
        f.key = Key::Unknown;
    } else if (f.kind == Kind::Face) {
      uint8_t bit = 0;
      if (name == "source_face") {
        f.key = Key::SourceFace;
        bit = 1;
      } else if (name == "vertices") {
        f.key = Key::Vertices;
        bit = 2;
      } else if (name == "first_triangle") {
        f.key = Key::First;
        bit = 4;
      } else if (name == "triangle_count") {
        f.key = Key::Count;
        bit = 8;
      } else
        return false;
      if (f.seen & bit)
        return false;
      f.seen |= bit;
    } else
      return false;
    return true;
  }
  bool start_object(size_t) override {
    visit_();
    if (stack_.empty()) {
      if (started_)
        return false;
      started_ = true;
      push(Kind::Root);
      return true;
    }
    if (ignored()) {
      stack_.back().key = Key::None;
      push(Kind::IgnoredObject);
      return true;
    }
    if (stack_.back().kind == Kind::Polygons) {
      if (out_.polygons.size() >= limits_.maximum_faces)
        throw std::length_error("Original face budget exceeded");
      push(Kind::Face);
      return true;
    }
    return false;
  }
  bool end_object() override {
    visit_();
    if (stack_.empty())
      return false;
    auto kind = stack_.back().kind;
    if (kind == Kind::Face) {
      if (stack_.back().seen != 15)
        return false;
      out_.polygons.push_back(std::move(stack_.back().face));
    } else if (kind == Kind::Root)
      complete_ = true;
    else if (kind != Kind::IgnoredObject)
      return false;
    stack_.pop_back();
    return true;
  }
  bool start_array(size_t) override {
    visit_();
    if (stack_.empty())
      return false;
    if (ignored()) {
      stack_.back().key = Key::None;
      push(Kind::IgnoredArray);
      return true;
    }
    auto &f = stack_.back();
    Kind kind;
    if (f.kind == Kind::Root && f.key == Key::Vertices)
      kind = Kind::Vertices;
    else if (f.kind == Kind::Root && f.key == Key::Corners)
      kind = Kind::Corners;
    else if (f.kind == Kind::Root && f.key == Key::Polygons)
      kind = Kind::Polygons;
    else if (f.kind == Kind::Face && f.key == Key::Vertices)
      kind = Kind::Loop;
    else
      return false;
    f.key = Key::None;
    push(kind);
    return true;
  }
  bool end_array() override {
    visit_();
    if (stack_.empty())
      return false;
    auto k = stack_.back().kind;
    if (k != Kind::Vertices && k != Kind::Corners && k != Kind::Polygons &&
        k != Kind::Loop && k != Kind::IgnoredArray)
      return false;
    stack_.pop_back();
    return true;
  }
  bool parse_error(size_t, const std::string &,
                   const nlohmann::detail::exception &) override {
    return false;
  }
  Metadata finish() {
    if (!complete_ || !stack_.empty())
      throw std::invalid_argument("Invalid original polygon metadata");
    return std::move(out_);
  }
};
template <class Visit>
Metadata parse(const std::string &text, size_t source_limit,
               const PolygonLimits &limits, Visit &&visit) {
  Reader<std::remove_reference_t<Visit>> reader(visit, source_limit, limits);
  if (!nlohmann::json::sax_parse(text, &reader))
    throw std::invalid_argument("Invalid original polygon witness metadata");
  return reader.finish();
}
} // namespace dome::polygon_source_detail
