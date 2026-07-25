#pragma once

#include "core/graphics/color.h"
#include "serialization/serialize.h"

namespace ptgn {

struct WindowSettings {
    Color background_color{ color::Transparent };

    PTGN_REFLECT(WindowSettings, background_color)
};

} // namespace ptgn