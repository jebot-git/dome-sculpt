// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
namespace dome {
enum class EditableExportKind { Project, AvatarV1 };
std::optional<AssetExportFormat> editable_export_dialog(EditableExportKind=EditableExportKind::Project);
}
