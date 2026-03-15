#pragma once

#include "renderer/primitives/id.h"
#include "renderer/primitives/resource.h"

namespace ptgn::impl {

class RenderbufferObject : public Resource<RenderbufferId> {
public:
	using Base = Resource<RenderbufferId>;
	using Base::Base;
};

} // namespace ptgn::impl