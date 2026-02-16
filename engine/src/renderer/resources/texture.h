#pragma once

#include "core/math/vector2.h"
#include "renderer/backend/gl/gl_handle.h"
#include "runtime/ecs/components/generic.h"

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

namespace impl {

/// Component for a custom texture size to be used instead of the actual texture size. This can be
/// used for example to render a texture at a larger size.
struct TextureSize : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct TextureCrop {
	// Position and size are V2_float instead of V2_int to allow for smooth increase in display size
	// (for example).

	// Top left position (in pixels) within the texture from which the crop starts.
	V2_float position;

	// Size of the crop in pixels. Zero size will use full size of texture.
	V2_float size;

	bool operator==(const TextureCrop&) const = default;
};

} // namespace impl

} // namespace ptgn