#include <array>

#include "components/common.h"
#include "components/drawable.h"
#include "components/offsets.h"
#include "core/entity.h"
#include "core/game.h"
#include "math/math.h"
#include "math/vector2.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/batching/render_data.h"
#include "rendering/graphics/vfx/light.h"
#include "rendering/resources/shader.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 1000, 1000 }; //{ 1280, 720 };

/*

ctx.DrawQuad();
ctx.DrawQuad();
ctx.DrawQuad();
ctx.DrawQuad();
ctx.DrawQuad();
ctx.DrawCircle();
ctx.DrawCircle();
ctx.DrawCircle();
ctx.DrawLight();
ctx.DrawLight();
ctx.DrawLight();

*/

struct Quad : public Drawable<Quad> {
	Quad() {}

	static void Draw(impl::RenderData& ctx, const Entity& entity) {
		impl::RenderState render_state;
		render_state.blend_mode_ = entity.GetBlendMode();
		render_state.shader_	 = { &game.shader.Get<ShapeShader::Quad>() };
		ctx.AddQuad(entity.Get<QuadVertices>().vertices, render_state);
	}
};

struct Circle : public Drawable<Circle> {
	Circle() {}

	static void Draw(impl::RenderData& ctx, const Entity& entity) {
		impl::RenderState render_state;
		render_state.blend_mode_ = entity.GetBlendMode();
		render_state.shader_	 = { &game.shader.Get<ShapeShader::Circle>() };
		ctx.AddQuad(entity.Get<QuadVertices>().vertices, render_state);
	}
};

struct Light : public Drawable<Light> {
	Light() {}

	static void Draw(impl::RenderData& ctx, const Entity& entity) {
		impl::RenderState render_state;
		render_state.blend_mode_ = BlendMode::Add;
		render_state.shader_	 = { &game.shader.Get<OtherShader::Light>() };
		ctx.AddShader(
			render_state, { std::function([entity](const Shader& shader) {
				PointLight light{ entity };

				auto offset_transform{ GetOffset(entity) };
				auto transform{ entity.GetAbsoluteTransform() };
				transform = transform.RelativeTo(offset_transform);
				float radius{ light.GetRadius() * Abs(transform.scale.x) };

				shader.SetUniform("u_LightPosition", transform.position);
				shader.SetUniform("u_LightIntensity", light.GetIntensity());
				shader.SetUniform("u_LightRadius", radius);
				shader.SetUniform("u_Falloff", light.GetFalloff());
				shader.SetUniform("u_Color", light.GetColor().Normalized());
				auto ambient_color{ PointLight::GetShaderColor(light.GetAmbientColor()) };
				shader.SetUniform("u_AmbientColor", ambient_color);
				shader.SetUniform("u_AmbientIntensity", light.GetAmbientIntensity());
			}) },
			BlendMode::Add, false
		);
	}
};

struct SandboxScene : public Scene {
	static constexpr int X = 100;									// Number of random quads
	static constexpr int Y = 10;									// Number of random lights

	RNG<float> pos_rngx{ 0.0f, static_cast<float>(window_size.x) }; // Position range
	RNG<float> pos_rngy{ 0.0f, static_cast<float>(window_size.y) }; // Position
	RNG<float> size_rng{ 10.0f, 70.0f };							// Size range
	RNG<float> light_radius_rng{ 10.0f, 200.0f };					// Light radius range
	RNG<float> intensity_rng{ 0.0f, 10.0f };						// Intensity range

	void Enter() override {
		for (int i = 0; i < X; ++i) {
			V2_float top_left{ pos_rngx(), pos_rngy() };

			V2_float size{ size_rng(), size_rng() };

			std::array<V2_float, 4> points{ top_left,
											{ top_left.x + size.x, top_left.y },
											{ top_left.x + size.x, top_left.y + size.y },
											{ top_left.x, top_left.y + size.y } };

			/*auto e{ CreatePointLight(*this, top_left, 50.0f, color::Blue, 1.0f, 2.0f) };
			e.SetDraw<Light>();
			e.Show();*/
			auto e{ CreateEntity() };
			float texture_index{ 0.0f };
			float line_width{ 10.0f };
			V2_float radius{ size };
			texture_index = 0.0f; // 1.0f; // 0.005f + line_width / std::min(radius.x, radius.y);
								  // e.SetDraw<Circle>();
			e.SetDraw<Quad>();
			e.Add<QuadVertices>(
				impl::GetQuadVertices(points, Color::RandomTransparent(), {}, texture_index)
			);
			e.Show();
		}

		for (int i = 0; i < Y; ++i) {
			V2_float top_left{ pos_rngx(), pos_rngy() };

			V2_float size{ size_rng(), size_rng() };

			std::array<V2_float, 4> points{ top_left,
											{ top_left.x + size.x, top_left.y },
											{ top_left.x + size.x, top_left.y + size.y },
											{ top_left.x, top_left.y + size.y } };

			auto e{ CreatePointLight(
				*this, top_left, light_radius_rng(), color::Blue, intensity_rng(), 2.0f
			) };
			e.SetDraw<Light>();
			e.Show();
			//   auto e{ CreateEntity() };
			//   float texture_index{ 0.0f };
			//   float line_width{ 10.0f };
			//   V2_float radius{ size };
			//   texture_index = 0.0f; // 1.0f; // 0.005f + line_width / std::min(radius.x,
			//   radius.y);
			//					  // e.SetDraw<Circle>();
			//   e.SetDraw<Quad>();
			//   e.Add<QuadVertices>(
			//	impl::GetQuadVertices(points, Color::RandomTransparent(), {}, texture_index)
			//);
			//   e.Show();
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("SandboxScene", window_size, color::Transparent);
	game.scene.Enter<SandboxScene>("");
	return 0;
}