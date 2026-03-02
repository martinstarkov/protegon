#pragma once

#include "renderer/primitives/id.h"
#include "renderer/primitives/resource.h"

namespace ptgn::impl {

struct FramebufferTag {};

using FramebufferId = Id<FramebufferTag>;

class FramebufferObject : public Resource<FramebufferId> {
public:
	using Base = Resource<FramebufferId>;
	using Base::Base;
};

} // namespace ptgn::impl