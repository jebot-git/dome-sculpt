// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/rig-rest.hpp"
#include "dome/source-placement.hpp"
#include <map>
namespace dome::detail {
// Worker-only normalized authority. No raw JSON/files, name matching or live
// source-object dependency is introduced into render/pick/per-vertex paths.
struct SourceRestPart {
 uint64_t object=0;
 uint32_t mesh_node=0;
 int original_skin=-1;
 std::shared_ptr<const CreatorAppearance> appearance;
 std::shared_ptr<const DeformationState> deformation;
 std::shared_ptr<const Mesh> mesh;
 AffineTransform world;
 std::vector<uint32_t> joint_nodes;
 // Local tables are original MODEL-node local; world tables are the native
 // Skeleton mesh/bind space (include Skeleton.space_matrix).
 std::vector<RigMatrix> rest_local,pose_local,rest_world,pose_world;
};
struct SourcePoseWitness {uint64_t object=0;RigMatrix local;};
enum class SourcePoseAgreement {Unrepresented,Shared,OneEditedView,ConflictingViews};
struct SourcePoseResolution {
 SourcePoseAgreement agreement=SourcePoseAgreement::Unrepresented;
 // Engaged only for genuinely shared authority. OneEditedView is identified,
 // never silently selected as a replacement for another skin's current pose.
 std::optional<RigMatrix> shared;
 std::vector<uint64_t> edited_objects;
};
struct SourceRestGraph {
 std::shared_ptr<const CreatorSourceModel> model;
 uint64_t instance=0;
 std::vector<SourceRestPart> parts;
 std::vector<int> parents;
 std::vector<RigMatrix> rest_local,rest_world;
 std::map<uint32_t,std::vector<SourcePoseWitness>> pose_witnesses;
 SourcePoseResolution pose_agreement(uint32_t) const;
};
// Cheap UI admission captures actual model+instance membership only. Full
// hierarchy/bind/pose authentication belongs to the detached worker below.
std::shared_ptr<const SourceNodePlacement> source_rest_identity(const Scene &,const SculptObject &);
std::vector<uint64_t> source_rest_targets(const Scene &,uint64_t);
struct SourceRestPose {
 std::vector<RigMatrix> local,world;
};
// One MODEL-node pose authority, resolved only from exact shared witnesses.
// Call once per detached operation, not per vertex or per rigid descendant.
SourceRestPose source_rest_pose(const SourceRestGraph &,
 const std::function<bool()> &cancelled={});
std::vector<SceneGroup> source_rest_groups(const SourceRestGraph &,
 const SourceRestPose &,std::vector<SceneGroup>,
 const std::function<bool()> &cancelled={});
// Export worker adapter: normalizes the same authority from immutable REST
// ObjectSurfaces, retaining owned mesh snapshots for detached metadata work.
SourceRestGraph capture_source_rest_graph(std::span<const ObjectSurface>,uint64_t,
 size_t max_snapshot_bytes,
 const std::function<bool()> &cancelled={});
SourceRestGraph capture_source_rest_graph(const Scene &,uint64_t,
 const std::function<bool()> &cancelled={});
} // namespace dome::detail
