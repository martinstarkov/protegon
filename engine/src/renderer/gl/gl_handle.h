#pragma once

#include <memory>
#include <utility>

#include "core/assert.h"
#include "renderer/gl/gl.h"

namespace ptgn::impl::gl {

enum GLResource {
	Shader,
	VertexBuffer,
	ElementBuffer,
	UniformBuffer,
	RenderBuffer,
	Texture,
	FrameBuffer,
	VertexArray
};

template <GLResource T>
class StrongGLHandle {
public:
	StrongGLHandle() = default;

	bool operator==(const StrongGLHandle&) const = default;

	explicit operator bool() const {
		return static_cast<bool>(id_);
	}

	operator GLuint() const {
		PTGN_ASSERT(id_);
		return *id_;
	}

private:
	friend class GLContext;

	StrongGLHandle(std::shared_ptr<GLuint> id) : id_{ std::move(id) } {}

	std::shared_ptr<GLuint> id_;
};

} // namespace ptgn::impl::gl