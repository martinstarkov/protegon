#pragma once

#include "renderer/resources/id.h"

namespace ptgn::impl {

struct RenderbufferTag {};

using Renderbuffer = Id<RenderbufferTag>;

} // namespace ptgn::impl