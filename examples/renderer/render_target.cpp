
#include "app/application.h"
#include "core/math/vector2.h"
#include "platform/window/window.h"
#include "renderer/renderer.h"
#include "renderer/primitives/shader.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int game_size{ 800, 800 };

float rect_thickness{ -1.0f };
float circle_thickness{ -1.0f };

class PostProcessingEffect {
public:
	PostProcessingEffect() {}

	static void Draw(const Entity& entity) {
		impl::DrawShader(entity);
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

	void OnEnter() override {
		SetBackgroundColor(color::LightGray);

		app().renderer.SetGameSize(game_size);

		CreateRect(*this, V2_float{ 200, -200 }, { 200, 200 }, color::Gray, -1.0f, Origin::Center);

		rt1 = CreateRenderTarget(*this, { 400, 400 }, color::Red);
		SetDrawOrigin(rt1, Origin::TopLeft);
		SetPosition(rt1, -game_size * 0.5f);

		auto rect1 =
			CreateRect(*this, V2_float{ 0, 0 }, { 100, 100 }, color::Orange, -1.0f, Origin::Center);

		rt1.AddToDisplayList(rect1);

		rt2 = CreateRenderTarget(*this, { 400, 400 }, color::Cyan);
		SetDrawOrigin(rt2, Origin::TopLeft);
		SetPosition(rt2, -game_size * 0.5f + V2_float{ 400, 400 });

		// Rect2 position is relative to rt position (0, 0 is center of rt).
		auto rect2 =
			CreateRect(*this, V2_float{ 0, 0 }, { 100, 100 }, color::White, -1.0f, Origin::Center);

		rt2.AddToDisplayList(rect2);
	}

	void OnUpdate() override {
		MoveArrowKeys(rt1.GetCamera(), V2_float{ 3.0f });
		MoveWASD(rt2.GetCamera(), V2_float{ 3.0f });
	}
};

int main(int, char**) {
	Application app{ "RenderTargetScene", game_size };
	app.StartWith<RenderTargetScene>();
}