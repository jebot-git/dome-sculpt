// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/import-browser.hpp"
#include <optional>
namespace dome {
std::optional<ImportSettings> import_options_dialog(const std::filesystem::path &,ImportSettings);
}
