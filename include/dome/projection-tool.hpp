// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "projection.hpp"
#include <string>
#include <vector>
namespace dome {
enum class ProjectionOcclusion { TargetOnly, VisibleScene };
struct ProjectionToolSettings {
  ProjectionCamera camera=[] {ProjectionCamera value;value.pose.position={0,0,2};return value;}();
  ProjectionOptions image;
  bool enabled=false,preview=true;
  ProjectionOcclusion occlusion=ProjectionOcclusion::VisibleScene;
  float preview_depth=1;
  void validate() const;
};
struct ProjectionToolImage {
  std::shared_ptr<const Image> pixels;
  std::string name;
};
struct ProjectionEntry {std::string name;bool directory=false;};
enum class ProjectionPanel {Tools,Browser,Parameters};
// Session-only immutable native tool publication. The painted pixels belong
// to ordinary paint layers; no projector/tool data is added to Scene archives.
struct ProjectionView {
  std::shared_ptr<const ProjectionToolImage> image;
  ProjectionToolSettings settings;
  ProjectionPanel panel=ProjectionPanel::Tools;
  std::filesystem::path directory;
  std::vector<ProjectionEntry> entries;
  size_t selected=0,parameter=0;
  uint64_t request=0;
  bool loading=false,browsing=false,placing=false;
  uint32_t placement_owner=0;bool placement_armed=false,placement_pressed=false;
  std::optional<ProjectionToolSettings> placement_before;
  std::string error;
};
inline constexpr size_t projection_parameter_count=26;
std::string projection_parameter_text(const ProjectionToolSettings &,size_t);
void adjust_projection_parameter(ProjectionToolSettings &,size_t,int direction);
std::string projection_tool_label(const ProjectionView *,int sector);
// Worker-only bounded directory scan, no recursive traversal or scene access.
std::vector<ProjectionEntry> scan_projection_directory(const std::filesystem::path &,
    const std::function<bool()> &cancelled={});
} // namespace dome
