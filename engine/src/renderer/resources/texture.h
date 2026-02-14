#pragma once

#include "renderer/backend/gl/gl_handle.h"

namespace ptgn {

class Renderer;

struct Texture {
	Texture() = default;

	Texture(impl::gl::Texture texture) : texture{ texture } {}

	~Texture() noexcept					   = default;
	Texture(const Texture&)				   = default;
	Texture& operator=(const Texture&)	   = default;
	Texture(Texture&&) noexcept			   = default;
	Texture& operator=(Texture&&) noexcept = default;

private:
	friend class Renderer;

	operator impl::gl::TextureId() const {
		return texture;
	}

	impl::gl::Texture texture;
};

} // namespace ptgn