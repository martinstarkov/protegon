#pragma once

#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"

namespace ptgn::impl {

class FramebufferObject : public Resource<FramebufferId> {
public:
	using Base = Resource<FramebufferId>;
	using Base::Base;
};

} // namespace ptgn::impl