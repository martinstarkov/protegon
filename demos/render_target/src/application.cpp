
#include "components/draw.h"
#include "components/movement.h"
#include "components/sprite.h"
#include "core/game.h"
#include "core/window.h"
#include "math/vector2.h"
#include "renderer/render_data.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int resolution{ 800, 800 };

float rect_thickness{ -1.0f };
float circle_thickness{ -1.0f };

class PostProcessingEffect {
public:
	PostProcessingEffect() {}

	static void Draw(impl::RenderData& ctx, const Entity& entity) {
		impl::RenderState state;
		state.blend_mode  = GetBlendMode(entity);
		state.shader_pass = entity.Get<impl::ShaderPass>();
		state.post_fx	  = entity.GetOrDefault<PostFX>();
		state.camera	  = entity.GetOrDefault<Camera>();

		impl::DrawShaderCommand cmd;
		cmd.entity		 = entity;
		cmd.render_state = state;

		ctx.Submit(cmd);
	}
};

PTGN_DRAWABLE_REGISTER(PostProcessingEffect);

Entity CreatePostFX(Scene& scene) {
	auto effect{ scene.CreateEntity() };

	SetDraw<PostProcessingEffect>(effect);
	Show(effect);
	SetBlendMode(effect, BlendMode::ReplaceRGBA);

	return effect;
}

Entity CreateBlur(Scene& scene) {
	auto blur{ CreatePostFX(scene) };
	blur.Add<impl::ShaderPass>(game.shader.Get("blur"), nullptr);
	return blur;
}

Entity CreateGrayscale(Scene& scene) {
	auto grayscale{ CreatePostFX(scene) };
	grayscale.Add<impl::ShaderPass>(game.shader.Get("grayscale"), nullptr);
	return grayscale;
}

Entity AddRect(Scene& s, V2_float pos, V2_float size, Color color) {
	auto e = CreateRect(s, pos, size, color, rect_thickness);
	return e;
}

Entity AddCircle(Scene& s, V2_float pos, float radius, Color color) {
	auto e = CreateCircle(s, pos, radius, color, circle_thickness);
	return e;
}

Entity AddSprite(Scene& s, V2_float pos) {
	auto e = CreateSprite(s, "test", pos);
	return e;
}

struct RenderTargetScene : public Scene {
	RenderTarget rt1;
	RenderTarget rt2;

	void Enter() override {
		SetBackgroundColor(color::LightGray);
		game.window.SetResizable();
		game.renderer.SetGameSize(resolution);

		CreateRect(*this, V2_float{ 200, -200 }, { 200, 200 }, color::Gray, -1.0f, Origin::Center);

		rt1 = CreateRenderTarget(*this, { 400, 400 }, color::Red);
		SetDrawOrigin(rt1, Origin::TopLeft);
		SetPosition(rt1, -resolution * 0.5f);

		auto rect1 =
			CreateRect(*this, V2_float{ 0, 0 }, { 100, 100 }, color::Orange, -1.0f, Origin::Center);

		rt1.AddToDisplayList(rect1);

		rt2 = CreateRenderTarget(*this, { 400, 400 }, color::Cyan);
		SetDrawOrigin(rt2, Origin::TopLeft);
		SetPosition(rt2, -resolution * 0.5f + V2_float{ 400, 400 });

		// Rect2 position is relative to rt position (0, 0 is center of rt).
		auto rect2 =
			CreateRect(*this, V2_float{ 0, 0 }, { 100, 100 }, color::White, -1.0f, Origin::Center);

		rt2.AddToDisplayList(rect2);
	}

	void Update() override {
		MoveArrowKeys(rt1.GetCamera(), V2_float{ 3.0f });
		MoveWASD(rt2.GetCamera(), V2_float{ 3.0f });
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("RenderTargetScene", resolution);
	game.scene.Enter<RenderTargetScene>("");
	return 0;
}