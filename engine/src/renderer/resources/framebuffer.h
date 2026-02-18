#pragma once

#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"

namespace ptgn::impl {

struct FramebufferTag {};

using Framebuffer = Id<FramebufferTag>;

class FramebufferObject : public Resource<Framebuffer> {
public:
	using Base = Resource<Framebuffer>;
	using Base::Base;
};

} // namespace ptgn::impl