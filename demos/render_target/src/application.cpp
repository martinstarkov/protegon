
#include "components/draw.h"
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
		ctx.AddShader(entity, state, color::Transparent);
	}
};

PTGN_DRAWABLE_REGISTER(PostProcessingEffect);

Entity CreatePostFX(Scene& scene) {
	auto effect{ scene.CreateEntity() };

	SetDraw<PostProcessingEffect>(effect);
	Show(effect);
	SetBlendMode(effect, BlendMode::None);

	return effect;
}

Entity CreateBlur(Scene& scene) {
	auto blur{ CreatePostFX(scene) };
	blur.Add<impl::ShaderPass>(game.shader.Get<ScreenShader::Blur>(), nullptr);
	return blur;
}

Entity CreateGrayscale(Scene& scene) {
	auto grayscale{ CreatePostFX(scene) };
	grayscale.Add<impl::ShaderPass>(game.shader.Get<ScreenShader::Grayscale>(), nullptr);
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
	void Enter() override {
		SetBackgroundColor(color::LightGray);
		game.window.SetSetting(WindowSetting::Resizable);
		game.renderer.SetLogicalResolution(resolution);

		/*
		LoadResource("test", "resources/test1.jpg");

		// Helper for reuse
		auto grayscale = CreateGrayscale(*this);
		auto blur	   = CreateBlur(*this);

		// === Dimensions for objects ===
		V2_float rect_size{ 80, 80 };
		float circle_radius{ 40.0f };
		V2_float sprite_size{ game.texture.GetSize("test"
		) };							// Approximate, since sprite has no size defined
		V2_float sprite_offset{ 0, 0 }; // used as safe offset from edge

		// Center
		auto rect1 =
			AddRect(*this, { resolution.x / 2.0f, resolution.y / 2.0f }, { 200, 200 }, color::Red);
		SetDrawOrigin(rect1, Origin::Center);
		AddPostFX(rect1, grayscale);

		auto circle1 =
			AddCircle(*this, { resolution.x / 2.0f, resolution.y / 2.0f }, 50.0f, color::Gold);
		AddPostFX(circle1, blur);

		// Top left corner
		auto sprite1 = AddSprite(*this, sprite_offset);
		SetDrawOrigin(sprite1, Origin::TopLeft);
		AddPreFX(sprite1, grayscale);

		auto rect2 = AddRect(*this, { 0, 0 }, rect_size, color::Green);
		SetDrawOrigin(rect2, Origin::TopLeft);
		AddPostFX(rect2, blur);

		// Top right corner
		auto circle2 = AddCircle(
			*this, { resolution.x - circle_radius, circle_radius }, circle_radius, color::Blue
		);
		AddPostFX(circle2, grayscale);

		auto rect3 = AddRect(*this, { resolution.x - rect_size.x, 0 }, rect_size, color::Cyan);
		SetDrawOrigin(rect3, Origin::TopLeft);
		AddPreFX(rect3, blur);

		// Bottom left
		auto sprite2 = AddSprite(*this, { sprite_offset.x, resolution.y - sprite_offset.y });
		SetDrawOrigin(sprite2, Origin::BottomLeft);
		AddPreFX(sprite2, blur);

		auto circle3 = AddCircle(
			*this, { circle_radius, resolution.y - circle_radius }, circle_radius, color::Purple
		);
		AddPostFX(circle3, grayscale);

		auto sprite3 = AddSprite(*this, { resolution.x, resolution.y });
		SetDrawOrigin(sprite3, Origin::BottomRight);
		AddPreFX(sprite3, grayscale);
		AddPreFX(sprite3, blur);

		// Bottom right
		auto rect4 = AddRect(
			*this, { resolution.x - rect_size.x, resolution.y - rect_size.y }, rect_size,
			color::Orange
		);
		SetDrawOrigin(rect4, Origin::TopLeft);
		AddPostFX(rect4, blur);

		auto circle4 = AddCircle(
			*this, { resolution.x - circle_radius, resolution.y - circle_radius }, circle_radius,
			color::Magenta
		);
		AddPreFX(circle4, grayscale);
		*/

		auto rect1 = CreateRect(*this, { 0, 0 }, { 400, 400 }, color::Red, -1.0f, Origin::TopLeft);
		auto rt	   = CreateRenderTarget(*this, { 400, 400 }, color::Cyan);
		SetDrawOrigin(rt, Origin::TopLeft);
		SetPosition(rt, { 400, 400 });
		// Rect2 position is relative to rt position.
		auto rect2 =
			CreateRect(*this, { 200, 200 }, { 100, 100 }, color::White, -1.0f, Origin::TopLeft);
		rt.AddToDisplayList(rect2);
	}

	void Update() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("RenderTargetScene", resolution);
	game.scene.Enter<RenderTargetScene>("");
	return 0;
}