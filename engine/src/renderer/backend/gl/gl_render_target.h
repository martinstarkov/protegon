#pragma once

#include <optional>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/render_target.h"
#include "renderer/resources/renderbuffer.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl::gl {

class GLContext;

// TODO: Figure out how the render target should look.
// Most likely get rid of gl_render_target.

class RenderTarget {
public:
	RenderTarget() = default;
	~RenderTarget() noexcept;
	RenderTarget(const RenderTarget&) = delete;
	RenderTarget(RenderTarget&&) noexcept;
	RenderTarget& operator=(const RenderTarget&) = delete;
	RenderTarget& operator=(RenderTarget&&) noexcept;

	void Resize(V2_int new_size);

	void Bind();

	void Clear(Color color = color::Transparent);

private:
	RenderTarget(
		GLContext& gl, const Framebuffer& framebuffer, const std::optional<Texture>& color,
		const std::optional<Renderbuffer>& depth, V2_int size, TextureFormat format
	);

	void Destroy();

	impl::RenderTarget render_target_;

	GLContext* gl_{ nullptr };
};

} // namespace ptgn::impl::gl