#pragma once

#include <optional>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/renderbuffer.h"
#include "renderer/resources/texture.h"

namespace ptgn {

namespace impl {

namespace gl {

class Renderer;
class GLContext;

} // namespace gl

struct RenderTarget {
	Framebuffer framebuffer_;
	std::optional<Texture> color_;
	std::optional<Renderbuffer> depth_;
	// TODO: Consider using the cache values instead to prevent synchronization issues.
	V2_int size_;
	TextureFormat format_{ TextureFormat::RGBA8 };

	RenderTarget(
		const Framebuffer& framebuffer, const std::optional<Texture>& color,
		const std::optional<Renderbuffer>& depth, V2_int size, TextureFormat format
	);

	bool operator==(const RenderTarget&) const = default;
};

class RenderTargetObject {
public:
	V2_int GetSize() const;
	TextureFormat GetFormat() const;
	void Resize(V2_int new_size);

	void Bind();

	void Clear(Color color = color::Transparent);

private:
	friend class gl::Renderer;

	RenderTargetObject(V2_int size, TextureFormat format);

	void Destroy();
};

class RenderPass {
private:
	friend class impl::gl::Renderer;

	// TODO: Use ptr.
	RenderTarget source_;

	RenderTarget ping_;
	RenderTarget pong_;

	bool has_ping_{ false };
	bool has_pong_{ false };

	// "latest output" tracking
	bool has_written_once_{ false }; // false -> latest is source
	bool latest_is_ping_{ true };	 // valid only if has_written_once == true
};

} // namespace impl

} // namespace ptgn