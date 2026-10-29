#pragma once

#include "serialization/serialize.h"

namespace ptgn {

enum class OverlayMode {
	Disable,
	Hide
};
PTGN_REFLECT_ENUM(OverlayMode);

} // namespace ptgn
