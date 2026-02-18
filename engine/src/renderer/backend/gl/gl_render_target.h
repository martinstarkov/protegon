#pragma once

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/resources/render_target.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl::gl {

class GLContext;

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