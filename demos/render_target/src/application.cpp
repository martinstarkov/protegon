
#include "components/draw.h"
#include "components/sprite.h"
#include "core/game.h"
#include "core/window.h"
#include "input/input_handler.h"
#include "math/geometry/circle.h"
#include "math/geometry/rect.h"
#include "math/vector2.h"
#include "renderer/render_data.h"
#include "renderer/render_target.h"
#include "renderer/shader.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };
constexpr V2_int resolution{ window_size };

float rect_thickness{ -1.0f };
float circle_thickness{ -1.0f };

class PostProcessingEffect : public Drawable<PostProcessingEffect> {
public:
	PostProcessingEffect() {}

	static void Draw(impl::RenderData& ctx, const Entity& entity) {
		impl::RenderState state;
		state.blend_mode  = entity.GetBlendMode();
		state.shader_pass = entity.Get<impl::ShaderPass>();
		state.post_fx	  = entity.GetOrDefault<impl::PostFX>();
		state.camera	  = entity.GetOrDefault<Camera>();
		ctx.AddShader(entity, state, BlendMode::None, color::Transparent, true);
	}
};

Entity CreatePostFX(Scene& scene) {
	auto effect{ scene.CreateEntity() };

	effect.SetDraw<PostProcessingEffect>();
	effect.Show();
	effect.SetBlendMode(BlendMode::None);

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
	auto e = CreateSprite(s, "test");
	e.SetPosition(pos);
	return e;
}

struct RenderTargetScene : public Scene {
	void Enter() override {
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
			AddRect(*this, { resolution.x / 2.0f, resolution.y / 2.0f }, { 200, 200 }, color::Red)
				.SetOrigin(Origin::Center)
				.AddPostFX(grayscale);

		AddCircle(*this, { resolution.x / 2.0f, resolution.y / 2.0f }, 50.0f, color::Gold)
			.AddPostFX(blur);

		// Top left corner
		AddSprite(*this, sprite_offset).SetOrigin(Origin::TopLeft).AddPreFX(grayscale);

		AddRect(*this, { 0, 0 }, rect_size, color::Green)
			.SetOrigin(Origin::TopLeft)
			.AddPostFX(blur);

		// Top right corner
		AddCircle(
			*this, { resolution.x - circle_radius, circle_radius }, circle_radius, color::Blue
		)
			.AddPostFX(grayscale);

		AddRect(*this, { resolution.x - rect_size.x, 0 }, rect_size, color::Cyan)
			.SetOrigin(Origin::TopLeft)
			.AddPreFX(blur);

		// Bottom left
		AddSprite(*this, { sprite_offset.x, resolution.y - sprite_offset.y })
			.SetOrigin(Origin::BottomLeft)
			.AddPreFX(blur);

		AddCircle(
			*this, { circle_radius, resolution.y - circle_radius }, circle_radius, color::Purple
		)
			.AddPostFX(grayscale);

		AddSprite(*this, { resolution.x, resolution.y })
			.SetOrigin(Origin::BottomRight)
			.AddPreFX(grayscale)
			.AddPreFX(blur);

		// Bottom right
		AddRect(
			*this, { resolution.x - rect_size.x, resolution.y - rect_size.y }, rect_size,
			color::Orange
		)
			.SetOrigin(Origin::TopLeft)
			.AddPostFX(blur);

		AddCircle(
			*this, { resolution.x - circle_radius, resolution.y - circle_radius }, circle_radius,
			color::Magenta
		)
			.AddPreFX(grayscale);

		// game.window.SetSetting(WindowSetting::Resizable);
		// auto rect1 = CreateRect(*this, { 0, 0 }, { 400, 400 }, color::Red, -1.0f,
		// Origin::TopLeft);
		//// For some reason the origin of the render target is the bottom left corner of the square
		//// (i.e. 400, 800 on the screen).
		//// So { 0, 400 }, { 400, 400 } will cover the screen coordinates with a whiterect from
		///{400, / 400} to {800,800}.
		// auto rt = CreateRenderTarget(*this, { 400, 400 }, color::Cyan);
		// rt.SetOrigin(Origin::TopLeft);
		// rt.SetPosition({ 400, 400 });
		// auto rect2 =
		//	CreateRect(*this, { 0, 400 }, { 200, 200 }, color::White, -1.0f, Origin::TopLeft);
		// rt.AddToDisplayList(rect2);
	}

	void Update() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("RenderTargetScene", window_size);
	game.scene.Enter<RenderTargetScene>("");
	return 0;
}