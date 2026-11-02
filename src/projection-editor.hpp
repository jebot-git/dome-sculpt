// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
namespace dome {
std::optional<std::filesystem::path> projection_image_dialog();
std::optional<ProjectionToolSettings> projection_dialog(ProjectionToolSettings,ProjectionCamera,const std::string &image);
}
