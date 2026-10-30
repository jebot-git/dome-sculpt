// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "csg.hpp"
#include "scene.hpp"
namespace dome {
struct ProceduralSculptOptions {
  enum class Pattern : unsigned { Panels, Recesses, Mixed };
  Pattern pattern = Pattern::Panels;
  unsigned axis = 1; // Object-local X/Y/Z; reverse chooses the negative face.
  bool reverse = false;
  unsigned rows = 4, columns = 4;
  uint32_t seed = 1;
  float density = .75f, gap = .2f, depth = .06f, rounding = .15f;
  size_t maximum_samples = 4'000'000, maximum_work = 128'000'000;
  size_t maximum_workspace_bytes = 256 * 1024 * 1024;
  std::function<bool()> cancelled;
  std::function<void(float)> progress;
  void validate() const;
};
struct ProceduralSculptReport {
  size_t projected = 0, primitives = 0, samples = 0, changed_samples = 0, work = 0;
  bool changed = false;
};
struct ProceduralSculptResult {
  std::optional<Volume> replacement;
  ProceduralSculptReport report;
};
// Detached ordered CSG, followed by one protection blend. Publishes a NEW
// reversible sculpt layer; all existing layers, masks, settings and source
// archives remain intact. No-op/cancellation/failure leave the source exact.
ProceduralSculptResult compose_sculpt_boxes(const Volume &, std::span<const CsgBox>,
  const ProceduralSculptOptions &, std::string layer_name = "Procedural detail");
ProceduralSculptResult procedural_sculpt(const Volume &, const ProceduralSculptOptions & = {});
bool procedural_sculpt_object(Scene &, uint64_t object, const ProceduralSculptOptions &,
                             ProceduralSculptReport * = nullptr);
} // namespace dome
