#pragma once

#include "core/util/file.h"

namespace ptgn::impl {

/// @return Absolute directory containing the currently running executable.
[[nodiscard]] path GetExecutableDirectory();

} // namespace ptgn::impl
