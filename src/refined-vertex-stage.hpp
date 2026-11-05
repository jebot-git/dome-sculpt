// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "key-mirror-refinement.hpp"
#include "dome/affine.hpp"
namespace dome::detail {
enum class VertexBitangentPolicy { RawCross, UnitCross };
enum class VertexColumnPath { Source, Canonical };
struct VertexStageMatrices {
 std::array<float,16> model{};
 std::array<float,9> normal{};
 float hand=1;
};
VertexStageMatrices vertex_stage_affine(const AffineTransform &);
// Explicit LOGICAL before-face corners after existing worker pose/placement.
// Spans carry data only; they do not certify any mesh/material/paint ownership.
struct BeforeVertexColumns {
 std::span<const Vec3> positions,normals;
 std::span<const std::array<float,4>> tangents;
};
struct PulledBackVertexStage {
 std::vector<Vec3> positions,normals,tangents,bitangents;
 size_t working_bytes=0,source_vertex_evaluations=0;
};
PulledBackVertexStage pullback_vertex_stage(const KeyMirrorRefinement &,
 BeforeVertexColumns,const VertexStageMatrices &,VertexBitangentPolicy,
 const KeyMirrorLimits & = {},VertexColumnPath = VertexColumnPath::Source);
}
