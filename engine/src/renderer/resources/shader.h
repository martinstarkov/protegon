#pragma once

#include <string>

#include "renderer/backend/gl/gl_handle.h"

namespace ptgn {

// Wrapper for distinguishing between Shader from path construction and Shader
// from source construction.
struct ShaderCode {
	std::string source;
};

class Renderer;

struct Shader {
	Shader() = default;

	Shader(impl::gl::Shader shader) : shader{ shader } {}

	~Shader() noexcept					 = default;
	Shader(const Shader&)				 = default;
	Shader& operator=(const Shader&)	 = default;
	Shader(Shader&&) noexcept			 = default;
	Shader& operator=(Shader&&) noexcept = default;

private:
	friend class Renderer;

	operator impl::gl::ShaderId() const {
		return shader;
	}

	impl::gl::Shader shader;
};

namespace impl {

// TODO: Move shader parsing code here.

} // namespace impl

} // namespace ptgn