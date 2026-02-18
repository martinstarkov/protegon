#pragma once

#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"

namespace ptgn::impl {

struct RenderbufferTag {};

using Renderbuffer = Id<RenderbufferTag>;

class RenderbufferObject : public Resource<Renderbuffer> {
public:
	using Base = Resource<Renderbuffer>;
	using Base::Base;
};

} // namespace ptgn::impl