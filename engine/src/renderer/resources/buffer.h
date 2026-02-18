#pragma once

#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"

namespace ptgn::impl {

struct VertexBufferTag {};

struct ElementBufferTag {};

struct UniformBufferTag {};

using VertexBuffer	= Id<VertexBufferTag>;
using ElementBuffer = Id<ElementBufferTag>;
using UniformBuffer = Id<UniformBufferTag>;

class VertexBufferObject : public Resource<VertexBuffer> {
public:
	using Base = Resource<VertexBuffer>;
	using Base::Base;
};

class ElementBufferObject : public Resource<ElementBuffer> {
public:
	using Base = Resource<ElementBuffer>;
	using Base::Base;
};

class UniformBufferObject : public Resource<UniformBuffer> {
public:
	using Base = Resource<UniformBuffer>;
	using Base::Base;
};

} // namespace ptgn::impl