// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
#include <map>
namespace dome {
// One static sample. Unspecified weights are zero; finite inputs clamp to[0,1].
// Preset/custom namespaces remain distinct. Custom expressions may suppress
// preset procedural groups; no custom expression is implicitly procedural.
struct Vrm1ExpressionWeights {
  std::map<std::string,float> preset,custom;
};
struct Vrm1ExpressionColor {
  uint32_t material=0;
  std::string type;
  std::array<float,4> target{};
  float weight=0;
};
struct Vrm1ExpressionUv {
  uint32_t material=0;
  std::array<float,2> scale{1,1},offset{};
  float weight=0;
};
struct Vrm1ExpressionEvaluation {
  Vrm1ExpressionWeights effective;
  // Original node/target identities. Zero entries deliberately reset driven
  // source targets; native-only authored keys are outside this program.
  std::map<std::pair<uint32_t,uint32_t>,float> morphs;
  std::vector<Vrm1ExpressionColor> colors;
  std::vector<Vrm1ExpressionUv> textures;
};
// Pure evaluation of validated actual VRMC_vrm1.0 metadata, not animation or
// serialized ownership authority. Results retain original material/node IDs.
Vrm1ExpressionEvaluation evaluate_vrm1_expressions(const nlohmann::json &gltf,
    const Vrm1ExpressionWeights &,const std::function<bool()> &cancelled={});
// Detached atomic static authoring operation on the authenticated source
// instance containing object. Each invocation uses the supplied Scene as its
// initial material state. For interactive preview, repeatedly evaluate the
// same immutable baseline; feeding its previous result back would accumulate.
// The original expression program/source bytes and all paint remain retained.
// Use Document::edit_scene for native worker, oneUndo and orderedSave semantics.
Scene bake_vrm1_expression_pose(const Scene &,uint64_t object,
    const Vrm1ExpressionWeights &,const std::function<bool()> &cancelled={});
}
