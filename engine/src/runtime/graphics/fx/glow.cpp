#include "runtime/graphics/fx/glow.h"

#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

void Glow::Draw(DrawContext& ctx, Entity entity) {
	const auto& glow{ entity.Get<Glow>() };

	ctx.Pass([&](auto& pass) -> RenderPassHandle {
		auto scene{ pass.BoundTarget() };

		auto bright{ pass.CreateLike(scene, "isolate_bright")
						 .Read(scene)
						 .Uniform("u_Threshold", glow.threshold)
						 .Uniform("u_SoftKnee", glow.soft_knee) };

		RenderPassHandle blurred{ bright };

		for (auto i{ 0uz }; i < glow.blur_iterations; ++i) {
			auto blur_x{ pass.CreateLike(blurred, "gaussian_blur")
							 .Read(blurred)
							 .Uniform("u_Direction", V2_float{ 1.0f, 0.0f })
							 .Uniform("u_Radius", glow.radius) };

			auto blur_y{ pass.CreateLike(blur_x, "gaussian_blur")
							 .Read(blur_x)
							 .Uniform("u_Direction", V2_float{ 0.0f, 1.0f })
							 .Uniform("u_Radius", glow.radius) };

			blurred = blur_y;
		}

		return pass.CreateLike(scene, "composite_additive")
			.Read(scene)
			.Read(blurred, 1, "u_Additive")
			.Uniform("u_Intensity", glow.intensity)
			.Uniform("u_Tint", glow.tint);
	});
}

} // namespace ptgn