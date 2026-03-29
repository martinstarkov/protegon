#pragma once

#include "renderer/primitives/id.h"
#include "renderer/primitives/resource.h"

namespace ptgn::impl {

class VertexArrayObject : public Resource<VertexArrayId> {
public:
	using Base = Resource<VertexArrayId>;
	using Base::Base;
};

} // namespace ptgn::impl