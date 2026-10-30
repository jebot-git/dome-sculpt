// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "import-authoring-fixtures.hpp"
namespace import_fixture {
inline int joint_data(Fixture &f) {
  while (f.bin.size() % 4)
    f.bin.push_back(0);
  auto start = f.bin.size();
  for (int v = 0; v < 4; ++v)
    for (auto slot : {0u, 1u, 0u, 0u}) {
      f.bin.push_back(uint8_t(slot));
      f.bin.push_back(uint8_t(slot >> 8));
    }
  int view = int(f.j["bufferViews"].size());
  f.j["bufferViews"].push_back({{"buffer", 0},
                                {"byteOffset", start},
                                {"byteLength", f.bin.size() - start}});
  int a = int(f.j["accessors"].size());
  f.j["accessors"].push_back({{"bufferView", view},
                              {"componentType", 5123},
                              {"count", 4},
                              {"type", "VEC4"}});
  return a;
}
inline Fixture skinned(bool authored) {
  Fixture f;
  int joints = joint_data(f);
  int weights =
      f.values({.8f, .2f, 0, 0, .8f, .2f, 0, 0, .8f, .2f, 0, 0, .8f, .2f, 0, 0},
               4, "VEC4");
  auto &attributes = f.j["meshes"][0]["primitives"][0]["attributes"];
  attributes["JOINTS_0"] = joints;
  attributes["WEIGHTS_0"] = weights;
  f.j["nodes"][0]["children"].push_back(3);
  f.j["nodes"][1]["skin"] = 0;
  f.j["nodes"][2]["skin"] = 0;
  f.j["nodes"].push_back({{"name", "joint root"},
                          {"translation", {.1f, .1f, 0}},
                          {"children", {4}}});
  f.j["nodes"].push_back({{"name", "joint child"},
                          {"translation", {.1f, 0, 0}},
                          {"scale", {1.1f, 1.1f, 1.1f}}});
  f.j["skins"] = {{{"joints", {3, 4}}, {"skeleton", 3}}};
  if (authored) {
    int ibm = f.values(
        {1.1f, 0, 0, 0, 0, .9f,  0, 0, 0, 0, .8f, 0, .12f, .05f, 0,   1,
         -.6f, 0, 0, 0, 0, 1.2f, 0, 0, 0, 0, .9f, 0, -.1f, 0,    .1f, 1,
         1,    0, 0, 0, 0, 1,    0, 0, 0, 0, 1,   0, 99,   12,   3,   1},
        16, "MAT4");
    f.j["skins"][0]["inverseBindMatrices"] = ibm;
  }
  return f;
}
inline Vec3 transformed(const ImportedMatrix &m, Vec3 v) {
  return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12],
          m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
          m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]};
}
} // namespace import_fixture
