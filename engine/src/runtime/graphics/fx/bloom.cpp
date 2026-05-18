#include "runtime/graphics/fx/bloom.h"

#include "core/math/vector2.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

/*

void BloomEffect::Apply(RenderGraphBuilder& g, Entity effect) {
	const auto& bloom = effect.Get<BloomEffect>();

	auto scene = g.SceneTexture();

	auto bright = g.Pass("Bloom Bright Extract")
					  .Read(scene, "u_Scene")
					  .Shader("bloom_extract")
					  .Uniform("u_Threshold", bloom.threshold)
					  .BlendMode(BlendMode::ReplaceRGBA)
					  .OutputLike(scene, "Bloom Bright");

	auto blurred = bright;

	for (int i = 0; i < bloom.iterations; ++i) {
		blurred = g.Pass("Bloom Blur X")
					  .Read(blurred, "u_Texture")
					  .Shader("gaussian_blur")
					  .Uniform("u_Direction", V2_float{ 1.0f, 0.0f })
					  .Uniform("u_Radius", bloom.radius)
					  .BlendMode(BlendMode::ReplaceRGBA)
					  .OutputLike(blurred, "Bloom Blur X");

		blurred = g.Pass("Bloom Blur Y")
					  .Read(blurred, "u_Texture")
					  .Shader("gaussian_blur")
					  .Uniform("u_Direction", V2_float{ 0.0f, 1.0f })
					  .Uniform("u_Radius", bloom.radius)
					  .BlendMode(BlendMode::ReplaceRGBA)
					  .OutputLike(blurred, "Bloom Blur Y");
	}

	g.Pass("Bloom Add Back")
		.Read(blurred, "u_Bloom")
		.Shader("texture")
		.Uniform("u_Intensity", bloom.intensity)
		.BlendMode(BlendMode::AddRGB)
		.WriteScene();
}

*/

} // namespace ptgn