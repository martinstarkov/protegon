#include "runtime/graphics/fx/glow.h"

#include "renderer/pipeline/draw_context.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

/*
void GlowEffect::Draw(DrawContext& ctx, Entity effect) {
	// TODO: Get properties from effect entity.

	MaterialState threshold{
		.shader = GetShaderSomehow("threshold"),
		.uniforms = {
			UniformWrite{ .name = "u_Threshold", .value = 0.75f },
		},
	};

	MaterialState blur{
		.shader = GetShaderSomehow("blur"),
		.uniforms = {},
	};

	MaterialState composite{
		.shader = GetShaderSomehow("glow_composite"),
		.uniforms = {
			UniformWrite{ .name = "u_Intensity", .value = 1.0f },
		},
	};

	RenderTargetDesc desc{
		.size = GetCurrentBoundTargetSizeSomehow(),
		.format = TextureFormat::RGBA8,
	};

	TextureRef bright = ctx.Pass()
		.Read(ctx.BoundTarget())
		.Output(desc)
		.Draw(threshold);

	TextureRef blurred = bright;

	for (int i = 0; i < 4; ++i) {
		blur.uniforms = {
			UniformWrite{ .name = "u_Direction", .value = V2_float{ 1.0f, 0.0f } },
		};

		TextureRef h = ctx.Pass()
			.Read(blurred)
			.Output(desc)
			.Draw(blur);

		ctx.Release(blurred);

		blur.uniforms = {
			UniformWrite{ .name = "u_Direction", .value = V2_float{ 0.0f, 1.0f } },
		};

		TextureRef v = ctx.Pass()
			.Read(h)
			.Output(desc)
			.Draw(blur);

		ctx.Release(h);

		blurred = v;
	}

	DrawTextureOptions composite_options;
	composite_options.extra_textures.push_back(TextureBinding{
		.name = "u_Glow",
		.texture = blurred,
	});

	ctx.DrawTexture(
		composite,
		ctx.BoundTarget(),
		FullscreenQuadSomehow(),
		0.0f,
		color::White,
		DefaultTextureCoordinatesSomehow(),
		composite_options
	);

	ctx.Release(blurred);
}
*/

} // namespace ptgn