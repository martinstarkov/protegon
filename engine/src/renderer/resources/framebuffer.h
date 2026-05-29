#pragma once

#include "core/math/vector2.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"

namespace ptgn::impl {

class FramebufferObject : public Resource<FramebufferId> {
public:
	using Base = Resource<FramebufferId>;
	using Base::Base;
};

} // namespace ptgn::impl