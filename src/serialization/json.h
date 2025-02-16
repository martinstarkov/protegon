#pragma once

#include <serialization/fwd.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include "utility/file.h"

namespace ptgn {

[[nodiscard]] json LoadJson(const path& filepath);

} // namespace ptgn
