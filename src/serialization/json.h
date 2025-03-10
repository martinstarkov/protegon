#pragma once

#include <fstream>
#include <nlohmann/json.hpp>

#include "serialization/fwd.h"
#include "utility/file.h"

namespace ptgn {

[[nodiscard]] json LoadJson(const path& filepath);

} // namespace ptgn
