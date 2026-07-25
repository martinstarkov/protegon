#pragma once

#include "core/util/file.h"
#include "serialization/json/json.h"

namespace ptgn {

json LoadJson(const path& file_path);

/// @param indent Clamped to a minimum of 0.
void SaveJson(const json& j, const path& file_path, int indent = 4);

} // namespace ptgn