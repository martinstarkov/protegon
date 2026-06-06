#include "runtime/graphics/fx/blur.h"

#include "renderer/draw_context.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

void Blur::Draw(DrawContext& ctx, Entity entity) {
	const auto& blur{ entity.Get<Blur>() };

	if (!blur.iterations) {
		return;
	}

	ctx.Pass([&](auto& pass) {
		RenderPassHandle blurred{ pass.BoundTarget() };

		for (auto i{ 0uz }; i < blur.iterations; ++i) {
			RenderPassHandle blur_output{ pass.CreateLike(blurred, "blur").Read(blurred) };

			blurred = blur_output;
		}

		return blurred;
	});
}

} // namespace ptgn