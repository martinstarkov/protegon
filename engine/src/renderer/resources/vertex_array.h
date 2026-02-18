#pragma once

#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"

namespace ptgn::impl {

struct VertexArrayTag {};

using VertexArray = Id<VertexArrayTag>;

class VertexArrayObject : public Resource<VertexArray> {
public:
	using Base = Resource<VertexArray>;
	using Base::Base;
};

} // namespace ptgn::impl