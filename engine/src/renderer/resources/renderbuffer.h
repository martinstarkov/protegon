#pragma once

#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"

namespace ptgn::impl {

class RenderbufferObject : public Resource<RenderbufferId> {
public:
	using Base = Resource<RenderbufferId>;
	using Base::Base;
};

} // namespace ptgn::impl