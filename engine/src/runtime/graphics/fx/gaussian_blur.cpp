#include "runtime/graphics/fx/gaussian_blur.h"

#include "core/math/vector2.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

void GaussianBlur::Draw(DrawContext& ctx, Entity entity) {
	const auto& gaussian_blur{ entity.Get<GaussianBlur>() };

	if (!gaussian_blur.blur_iterations) {
		return;
	}

	ctx.Pass([&](auto& pass) {
		auto scene{ pass.BoundTarget() };

		RenderPassHandle blurred{ scene };

		for (auto i{ 0uz }; i < gaussian_blur.blur_iterations; ++i) {
			RenderPassHandle blur_x{ pass.CreateLike(blurred, "gaussian_blur")
										 .Read(blurred)
										 .Uniform("u_Direction", V2_float{ 1.0f, 0.0f })
										 .Uniform("u_Radius", gaussian_blur.radius) };

			RenderPassHandle blur_y{ pass.CreateLike(blur_x, "gaussian_blur")
										 .Read(blur_x)
										 .Uniform("u_Direction", V2_float{ 0.0f, 1.0f })
										 .Uniform("u_Radius", gaussian_blur.radius) };

			blurred = blur_y;
		}

		return blurred;
	});
}

} // namespace ptgn