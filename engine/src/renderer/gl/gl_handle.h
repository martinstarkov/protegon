#pragma once

#include <memory>

#include "core/assert.h"
#include "renderer/gl/gl.h"

namespace ptgn::impl::gl {

enum Resource {
	Shader,
	VertexBuffer,
	ElementBuffer,
	UniformBuffer,
	RenderBuffer,
	Texture,
	FrameBuffer,
	VertexArray
};

template <Resource T>
class Handle {
public:
	Handle() = default;

	bool operator==(const Handle&) const = default;

	explicit operator bool() const {
		return static_cast<bool>(id_);
	}

	operator GLuint() const {
		PTGN_ASSERT(id_);
		return *id_;
	}

private:
	friend class GLContext;

	explicit Handle(std::shared_ptr<GLuint> id) : id_{ std::move(id) } {}

	std::shared_ptr<GLuint> id_{ 0 };
};

} // namespace ptgn::impl::gl