#pragma once

#include <optional>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/backend/gl/gl_framebuffer.h"
#include "renderer/backend/gl/gl_renderbuffer.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl::gl {

class GLContext;
class RenderTargets;
class Renderer;

// TODO: Decide whether to use ids for render targets and cache or give user ownership.

class RenderTarget {
public:
	RenderTarget() = default;

	V2_int GetSize() const;
	TextureFormat GetFormat() const;

private:
	friend class RenderTargets;
	friend class Renderer;

	RenderTarget(
		const Framebuffer& framebuffer, const std::optional<Texture>& color,
		const std::optional<Renderbuffer>& depth, V2_int size, TextureFormat format
	);

	Framebuffer framebuffer_;
	std::optional<Texture> color_;
	std::optional<Renderbuffer> depth_;
	// TODO: Consider using the cache values instead to prevent synchronization issues.
	V2_int size_;
	TextureFormat format_{ TextureFormat::RGBA8 };

	bool operator==(const RenderTarget&) const = default;
};

class RenderTargets {
public:
	RenderTarget CreateRenderTarget(V2_int size, TextureFormat format) const;

	void ResizeRenderTarget(RenderTarget& render_target, V2_int new_size) const;

	void DestroyRenderTarget(RenderTarget& render_target);

	void BindRenderTarget(const RenderTarget& render_target);

	void ClearRenderTarget(const RenderTarget& render_target, Color color);

private:
	friend class GLContext;

	explicit RenderTargets(GLContext& gl);
	~RenderTargets() noexcept						   = default;
	RenderTargets(const RenderTargets&)				   = delete;
	RenderTargets(RenderTargets&&) noexcept			   = delete;
	RenderTargets& operator=(const RenderTargets&)	   = delete;
	RenderTargets& operator=(RenderTargets&&) noexcept = delete;

	GLContext& gl_;
};

} // namespace ptgn::impl::gl