#pragma once

#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"

namespace ptgn::impl {

class VertexBufferObject : public Resource<VertexBufferId> {
public:
	using Base = Resource<VertexBufferId>;
	using Base::Base;
};

class ElementBufferObject : public Resource<ElementBufferId> {
public:
	using Base = Resource<ElementBufferId>;
	using Base::Base;
};

class UniformBufferObject : public Resource<UniformBufferId> {
public:
	using Base = Resource<UniformBufferId>;
	using Base::Base;
};

} // namespace ptgn::impl