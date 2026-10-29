#pragma once

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl {

class FramebufferObject : public Resource<FramebufferId> {
public:
	using Base = Resource<FramebufferId>;
	using Base::Base;

	void Resize(V2_int new_size);

	void Bind();

	void Clear(Color clear_color);
	void Clear(Depth clear_depth);
	void Clear(Stencil clear_stencil);
	void Clear(DepthStencil clear_depth_stencil);

	std::optional<std::int32_t> ReadEntityId(V2_int pixel) const;

	TextureId GetTexture() const;
	TextureDesc GetDesc() const;
};

} // namespace ptgn::impl