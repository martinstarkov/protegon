#pragma once

#include <span>

#include "core/math/transform.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/resources/id.h"

namespace ptgn::impl {

template <RenderPrimitive T>
struct DrawRequest {
	/// @brief Optional texture to apply to the primitive. If the pipeline does not support
	/// texturing, this field will be ignored.
	TextureId texture;
	/// @brief Center of the primitive in world space. Origin should be accounted for in this
	/// transform.
	Transform transform;

	std::span<T> primitives;

	EffectParams effect_params;
};

using DrawTextureRequest = DrawRequest<TextureQuad>;

template <VertexType TVertex>
using DrawQuadsRequest = DrawRequest<RenderQuad<TVertex>>;

template <VertexType TVertex>
using DrawTrianglesRequest = DrawRequest<RenderTriangle<TVertex>>;

} // namespace ptgn::impl