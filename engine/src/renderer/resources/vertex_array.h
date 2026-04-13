#pragma once

#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"

namespace ptgn::impl {

class VertexArrayObject : public Resource<VertexArrayId> {
public:
	using Base = Resource<VertexArrayId>;
	using Base::Base;
};

} // namespace ptgn::impl