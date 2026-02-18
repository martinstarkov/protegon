#pragma once

#include "renderer/resources/id.h"

namespace ptgn::impl {

struct VertexBufferTag {};

struct ElementBufferTag {};

struct UniformBufferTag {};

using VertexBuffer	= Id<VertexBufferTag>;
using ElementBuffer = Id<ElementBufferTag>;
using UniformBuffer = Id<UniformBufferTag>;

} // namespace ptgn::impl