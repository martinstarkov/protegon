#include <array>

#include "components/common.h"
#include "components/drawable.h"
#include "core/entity.h"
#include "core/game.h"
#include "math/vector2.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/batching/render_data.h"
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

struct Shape : public Drawable<Shape> {
	Shape() {}

	static void Draw(impl::RenderData& ctx, const Entity& entity) {
		impl::RenderState render_state;
		render_state.blend_mode_ = entity.GetBlendMode();
		render_state.shader_	 = &game.shader.Get<ShapeShader::Quad>();
		ctx.AddQuad(entity.Get<QuadVertices>().vertices, render_state);
	}
};

struct Light : public Drawable<Light> {
	Light() {}

	static void Draw(impl::RenderData& ctx, const Entity& entity) {
		impl::RenderState render_state;
		render_state.render_target_ = ctx.light_target;
		render_state.blend_mode_	= BlendMode::Blend;
		render_state.shader_		= &game.shader.Get<OtherShader::Light>();
		render_state.camera_		= ctx.light_target.GetCamera();
		auto vertices{ impl::GetQuadVertices(render_state.camera_.GetVertices(), color::White, entity.GetDepth() };
		ctx.AddTexturedQuad(
			vertices, render_state, render_state.render_target_.GetTexture().GetId()
		);
	}
};

struct SandboxScene : public Scene {
	static constexpr int X = 100;									// Number of random quads

	RNG<float> pos_rngx{ 0.0f, static_cast<float>(window_size.x) }; // Position range
	RNG<float> pos_rngy{ 0.0f, static_cast<float>(window_size.y) }; // Position range
	RNG<float> size_rng{ 10.0f, 50 };								// Size range

	void Enter() override {
		for (int i = 0; i < X; ++i) {
			V2_float top_left{ pos_rngx(), pos_rngy() };

			V2_float size{ size_rng(), size_rng() };

			std::array<V2_float, 4> points{ top_left,
											{ top_left.x + size.x, top_left.y },
											{ top_left.x + size.x, top_left.y + size.y },
											{ top_left.x, top_left.y + size.y } };

			auto e{ CreateEntity() };
			e.Add<QuadVertices>(impl::GetQuadVertices(points, Color::RandomTransparent(), {}));
			e.SetDraw<Shape>();
			e.Show();
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("SandboxScene", window_size, color::Transparent);
	game.scene.Enter<SandboxScene>("");
	return 0;
}