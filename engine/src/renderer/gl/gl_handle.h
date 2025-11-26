#pragma once

#include <memory>
#include <utility>

#include "core/assert.h"

namespace ptgn::impl::gl {

using Id = unsigned int;

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

	operator Id() const {
		PTGN_ASSERT(id_);
		return *id_;
	}

private:
	friend class GLContext;

	StrongGLHandle(std::shared_ptr<Id> id) : id_{ std::move(id) } {}

	std::shared_ptr<Id> id_;
};

} // namespace ptgn::impl::gl