#include "runtime/graphics/camera.h"

#include "app/application.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "platform/input/input_handler.h"
#include "platform/input/key.h"
#include "platform/input/mouse.h"
#include "platform/window/window.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/shader.h"
#include "renderer/renderer.h"
#include "runtime/animation/effects.h"
#include "runtime/animation/follow_config.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_manager.h"
#include "tools/debug/debug_system.h"

using namespace ptgn;

/*
class CameraUIScene : public Scene {
constexpr V2_int deadzone_size{ 150, 150 };
public:
	void OnEnter() override {
		game.texture.Load("ui_texture2", "assets/ui2.jpg");

		auto ui = CreateSprite(*this, "ui_texture2");
		ui.SetPosition({});
		ui.SetOrigin(Origin::TopLeft);

		auto camera_center = manager.CreateEntity();
		camera_center.Add<Circle>(3.0f);
		camera_center.SetPosition(app().window.GetCenter());
		camera_center.SetTint(color::Black);
		camera_center.Show();

		auto deadzone = manager.CreateEntity();
		deadzone.Add<Rect>(deadzone_size, Origin::Center);
		deadzone.SetPosition(app().window.GetCenter());
		deadzone.Add<FillStyle>(2.0f);
		deadzone.SetOrigin;
		deadzone.SetTint(color::DarkGreen);
		deadzone.Show();

		camera.FadeFrom(color::Black, seconds{ 3 });
		camera.FadeTo(color::Red, seconds{ 3 });
		camera.FadeFrom(color::Red, seconds{ 3 });
	}
};
*/

/*
class CameraExampleScene : public Scene {
public:
	const float pan_speed	   = 200.0f;
	const float rotation_speed = 1.0f;
	const float zoom_speed{ 0.4f };

	Entity rt;
	Entity ui;
	Entity mouse;

	CameraExampleScene() {
		app().scene.Load<CameraUIScene>("ui_scene");
	}

	void OnEnter() override {
		game.texture.Load("texture", "assets/test1.jpg");

		camera.SetPosition(app().window.GetCenter());
		// camera.SetBounds({}, window_size);

		auto texture = CreateSprite(*this, "texture");
		texture.SetPosition(app().window.GetCenter());
		texture.Add<Interactive>();
		texture.Add<callback::KeyPressed>([](auto key) {
			if (key == Key::W) {
				PTGN_LOG("Key down W");
			}
		});
		texture.Add<callback::KeyPressed>([](auto key) {
			if (key == Key::W) {
				PTGN_LOG("Key pressed W");
			}
		});
		texture.Add<callback::KeyUp>([](auto key) {
			if (key == Key::W) {
				PTGN_LOG("Key up W");
			}
		});
		texture.Add<callback::MouseMove>([](auto mouse) { PTGN_LOG("Mouse move: ", mouse); });
		texture.Add<callback::MouseDown>([](auto mouse) { PTGN_LOG("Mouse down: ", mouse); });
		texture.Add<callback::MouseUp>([](auto mouse) { PTGN_LOG("Mouse up: ", mouse); });
		texture.Add<callback::MousePressed>([](auto mouse) { PTGN_LOG("Mouse pressed: ", mouse); });
		texture.Add<callback::MouseScroll>([](auto scroll) { PTGN_LOG("Mouse scroll: ", scroll); });

		auto b = manager.CreateEntity();
		b.Add<Rect>(window_size, Origin::TopLeft);
		b.SetPosition({});
		b.Add<FillStyle>(3.0f);
		b.SetTint(color::Red);
		b.Show();

		app().scene.Enter("ui_scene");

		game.texture.Load("ui_texture", "assets/ui.jpg");

		ui = CreateSprite(*this, "ui_texture");
		ui.SetPosition(V2_float{ window_size.x, 0 });
		ui.SetOrigin(Origin::TopRight);
		ui.Hide();

		rt = manager.CreateEntity();
		rt.Add<RenderTarget>(manager, window_size);
		rt.SetPosition({});
		rt.Show();

		mouse = manager.CreateEntity();
		mouse.SetPosition({});
		mouse.Add<Circle>(20.0f);
		mouse.SetTint(color::Red);
		mouse.Show();

		camera.PanTo({ 0, 0 }, seconds{ 3 });
		camera.PanTo({ 800, 0 }, seconds{ 3 });
		camera.PanTo({ 800, 800 }, seconds{ 3 });
		camera.PanTo({ 0, 800 }, seconds{ 3 });
		StartFollow(camera,mouse);
		camera.SetLerp(V2_float{ 0.9f });
		// camera.SetOffset(V2_float{ -75, -75 });
		camera.SetDeadzone(deadzone_size);

		camera.ZoomTo(0.5f, seconds{ 3 });
		camera.ZoomTo(2.0f, seconds{ 3 });
		camera.ZoomTo(0.25f, seconds{ 3 });
		camera.ZoomTo(1.0f, seconds{ 3 });

		camera.RotateTo(DegToRad(90.0f), seconds{ 3 });
		camera.RotateTo(DegToRad(0.0f), seconds{ 3 });
		camera.RotateTo(DegToRad(-90.0f), seconds{ 3 });
		camera.RotateTo(DegToRad(0.0f), seconds{ 3 });
	}

	void OnUpdate() override {
		V2_float center{ app().window.GetCenter() };
		float dt{ app().DeltaTime().count() };

		if (input.KeyHeld(Key::W)) {
			Translate(camera,{ 0, -pan_speed * dt });
		}
		if (input.KeyHeld(Key::S)) {
			Translate(camera,{ 0, pan_speed * dt });
		}
		if (input.KeyHeld(Key::A)) {
			Translate(camera,{ -pan_speed * dt, 0 });
		}
		if (input.KeyHeld(Key::D)) {
			Translate(camera,{ pan_speed * dt, 0 });
		}

		if (input.KeyHeld(Key::Z)) {
			camera.Yaw(rotation_speed * dt);
		}

		if (input.KeyHeld(Key::X)) {
			camera.Yaw(-rotation_speed * dt);
		}

		if (input.KeyHeld(Key::C)) {
			camera.Pitch(rotation_speed * dt);
		}

		if (input.KeyHeld(Key::V)) {
			camera.Pitch(-rotation_speed * dt);
		}

		if (input.KeyHeld(Key::B)) {
			camera.Roll(rotation_speed * dt);
		}

		if (input.KeyHeld(Key::N)) {
			camera.Roll(-rotation_speed * dt);
		}

		if (input.KeyHeld(Key::E)) {
			camera.Zoom(zoom_speed * dt);
		}
		if (input.KeyHeld(Key::Q)) {
			camera.Zoom(-zoom_speed * dt);
		}

		if (input.KeyHeld(Key::R)) {
			camera.SetPosition(center);
			camera.SetZoom(1.0f);
		}

		if (input.MousePressed(Mouse::Left)) {
			mouse.SetPosition( =
				camera.TransformToCamera(input.GetMousePosition());
			//camera.PanTo(camera.TransformToCamera(input.GetMousePosition()),
seconds{ 4 },Ease::InOutSine, false); } else if (input.MousePressed(Mouse::Right)) {
			StopFollow(camera);
		}

		const auto& r{ rt.Get<RenderTarget>() };
		r.Bind();
		r.Clear();

		r.Draw(ui);
	}
};
*/

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

class CameraScene : public Scene {
public:
	const float pan_speed{ 200.0f };
	const float rotation_speed{ 1.0f };
	const float zoom_speed{ 0.4f };

	Entity mouse;
	TargetFollowConfig follow_config;

	std::string content{ "The quick brown fox jumps over the lazy dog" };
	Color color{ color::White };
	FontSize font_size{ 20 };
	V2_int center{ 0, 0 };

	void OnEnter() override {
		//	camera.SetPixelRounding(true);
		app().asset.Load("tree", "assets/test1.jpg");

		mouse = CreateEntity();
		SetPosition(mouse, {});

		auto blur{ CreateBlur(*this) };
		auto grayscale{ CreateGrayscale(*this) };
		auto game_size{ app().renderer.GetGameSize() };
		auto s1{ CreateSprite(*this, "tree", -game_size * 0.5f + V2_float{ 100, 400 }) };
		AddPreFX(s1, blur);
		auto s2{ CreateSprite(*this, "tree", -game_size * 0.5f + V2_float{ 700, 400 }) };
		AddPostFX(s2, grayscale);

		follow_config.move_mode = MoveMode::Lerp;
		follow_config.lerp		= { 0.5f, 0.5f };
		follow_config.deadzone	= { 300, 300 };

		// Shake(camera, 0.5f, seconds{ 5 });
		// RotateTo(camera, DegToRad(360.0f), seconds{ 5 });
		// Shake(camera, 1, seconds{ 5 }, {}, Ease::Linear, false);
		// Shake(camera, 0, seconds{ 5 }, {}, Ease::Linear, false);
		// FadeTo(camera, color::Red, seconds{ 5 });
		// FadeFrom(camera, color::Red, seconds{ 3 }, Ease::InOutBack, false);
		// StartFollow(camera, mouse, follow_config);
	}

	void OnUpdate() override {
		float dt{ app().DeltaTime().count() };

		/*	PTGN_LOG(
				"Mouse screen pos: ", input.GetMouseWindowPosition(),
				", Mouse world pos: ", input.GetMousePosition()
			);*/

		SetPosition(mouse, input.GetMousePosition());

		if (input.KeyHeld(Key::W)) {
			Translate(camera, { 0, -pan_speed * dt });
		}
		if (input.KeyHeld(Key::S)) {
			Translate(camera, { 0, pan_speed * dt });
		}
		if (input.KeyHeld(Key::A)) {
			Translate(camera, { -pan_speed * dt, 0 });
		}
		if (input.KeyHeld(Key::D)) {
			Translate(camera, { pan_speed * dt, 0 });
		}

		if (input.KeyHeld(Key::Z)) {
			Rotate(camera, rotation_speed * dt);
		}

		if (input.KeyHeld(Key::X)) {
			Rotate(camera, -rotation_speed * dt);
		}

		if (input.KeyHeld(Key::E)) {
			camera.Zoom(zoom_speed * dt);
		}
		if (input.KeyHeld(Key::Q)) {
			camera.Zoom(-zoom_speed * dt);
		}

		if (input.MousePressed(Mouse::Left)) {
			StopFollow(camera);
		} else if (input.MousePressed(Mouse::Right)) {
			StartFollow(camera, mouse, follow_config);
		}

		app().renderer.DrawText(
			content, center - 0 * V2_float{ 0.0f, font_size }, color, Origin::Center, font_size, {},
			{}, {}, {}, false
		);
		app().renderer.DrawText(
			content, center + 1 * V2_float{ 0.0f, font_size }, color, Origin::Center, font_size, {},
			{}, {}, {}, true
		);
	}
};

int main(int, char**) {
	Application game{ "Camera: WASD move, Q/E zoom" };
	game.StartWith<CameraScene>();
}