#include "core/app/application.h"

#include "core/app/window.h"
#include "ecs/components/draw.h"
#include "ecs/components/drawable.h"
#include "ecs/components/effects.h"
#include "ecs/components/sprite.h"
#include "ecs/components/transform.h"
#include "ecs/entity.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "core/log.h"
#include "debug/debug_system.h"
#include "math/vector2.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/material/shader.h"
#include "renderer/render_data.h"
#include "renderer/renderer.h"
#include "tween/follow_config.h"
#include "tween/tween_effect.h"
#include "world/scene/camera.h"
#include "world/scene/scene.h"
#include "world/scene/scene_input.h"
#include "world/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int resolution{ 800, 800 };

/*
class CameraUIScene : public Scene {
constexpr V2_int deadzone_size{ 150, 150 };
public:
	void Enter() override {
		Application::Get().texture.Load("ui_texture2", "resources/ui2.jpg");

		auto ui = CreateSprite(*this, "ui_texture2");
		ui.SetPosition({});
		ui.SetOrigin(Origin::TopLeft);

		auto camera_center = manager.CreateEntity();
		camera_center.Add<Circle>(3.0f);
		camera_center.SetPosition(Application::Get().window_.GetCenter());
		camera_center.SetTint(color::Black);
		camera_center.Show();

		auto deadzone = manager.CreateEntity();
		deadzone.Add<Rect>(deadzone_size, Origin::Center);
		deadzone.SetPosition(Application::Get().window_.GetCenter());
		deadzone.Add<LineWidth>(2.0f);
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
		Application::Get().scene_.Load<CameraUIScene>("ui_scene");
	}

	void Enter() override {
		Application::Get().texture.Load("texture", "resources/test1.jpg");

		camera.SetPosition(Application::Get().window_.GetCenter());
		// camera.SetBounds({}, window_size);

		auto texture = CreateSprite(*this, "texture");
		texture.SetPosition(Application::Get().window_.GetCenter());
		texture.Add<Interactive>();
		texture.Add<callback::KeyDown>([](auto key) {
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
		b.Add<LineWidth>(3.0f);
		b.SetTint(color::Red);
		b.Show();

		Application::Get().scene_.Enter("ui_scene");

		Application::Get().texture.Load("ui_texture", "resources/ui.jpg");

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

	void Update() override {
		V2_float center{ Application::Get().window_.GetCenter() };
		float dt{ Application::Get().dt() };

		if (input.KeyPressed(Key::W)) {
			Translate(camera,{ 0, -pan_speed * dt });
		}
		if (input.KeyPressed(Key::S)) {
			Translate(camera,{ 0, pan_speed * dt });
		}
		if (input.KeyPressed(Key::A)) {
			Translate(camera,{ -pan_speed * dt, 0 });
		}
		if (input.KeyPressed(Key::D)) {
			Translate(camera,{ pan_speed * dt, 0 });
		}

		if (input.KeyPressed(Key::Z)) {
			camera.Yaw(rotation_speed * dt);
		}

		if (input.KeyPressed(Key::X)) {
			camera.Yaw(-rotation_speed * dt);
		}

		if (input.KeyPressed(Key::C)) {
			camera.Pitch(rotation_speed * dt);
		}

		if (input.KeyPressed(Key::V)) {
			camera.Pitch(-rotation_speed * dt);
		}

		if (input.KeyPressed(Key::B)) {
			camera.Roll(rotation_speed * dt);
		}

		if (input.KeyPressed(Key::N)) {
			camera.Roll(-rotation_speed * dt);
		}

		if (input.KeyPressed(Key::E)) {
			camera.Zoom(zoom_speed * dt);
		}
		if (input.KeyPressed(Key::Q)) {
			camera.Zoom(-zoom_speed * dt);
		}

		if (input.KeyDown(Key::R)) {
			camera.SetPosition(center);
			camera.SetZoom(1.0f);
		}

		if (input.MouseDown(Mouse::Left)) {
			mouse.SetPosition( =
				camera.TransformToCamera(input.GetMousePosition());
			//camera.PanTo(camera.TransformToCamera(input.GetMousePosition()),
seconds{ 4 },SymmetricalEase::InOutSine, false); } else if (input.MouseDown(Mouse::Right)) {
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
	blur.Add<impl::ShaderPass>(Application::Get().shader.Get("blur"), nullptr);
	return blur;
}

Entity CreateGrayscale(Scene& scene) {
	auto grayscale{ CreatePostFX(scene) };
	grayscale.Add<impl::ShaderPass>(Application::Get().shader.Get("grayscale"), nullptr);
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

	void Enter() override {
		Application::Get().window_.SetResizable();
		//	camera.SetPixelRounding(true);
		LoadResource("tree", "resources/test1.jpg");

		mouse = CreateEntity();
		SetPosition(mouse, {});

		auto blur{ CreateBlur(*this) };
		auto grayscale{ CreateGrayscale(*this) };
		auto s1{ CreateSprite(*this, "tree", -resolution * 0.5f + V2_float{ 100, 400 }) };
		AddPreFX(s1, blur);
		auto s2{ CreateSprite(*this, "tree", -resolution * 0.5f + V2_float{ 700, 400 }) };
		AddPostFX(s2, grayscale);

		follow_config.move_mode = MoveMode::Lerp;
		follow_config.lerp		= { 0.5f, 0.5f };
		follow_config.deadzone	= { 300, 300 };

		// Shake(camera, 0.5f, seconds{ 5 });
		// RotateTo(camera, DegToRad(360.0f), seconds{ 5 });
		// Shake(camera, 1, seconds{ 5 }, {}, SymmetricalEase::Linear, false);
		// Shake(camera, 0, seconds{ 5 }, {}, SymmetricalEase::Linear, false);
		// FadeTo(camera, color::Red, seconds{ 5 });
		// FadeFrom(camera, color::Red, seconds{ 3 }, SymmetricalEase::InOutBack, false);
		// StartFollow(camera, mouse, follow_config);
	}

	void Update() override {
		float dt{ Application::Get().dt() };

		/*	PTGN_LOG(
				"Mouse screen pos: ", input.GetMouseWindowPosition(),
				", Mouse world pos: ", input.GetMousePosition()
			);*/

		SetPosition(mouse, input.GetMousePosition());

		if (input.KeyPressed(Key::W)) {
			Translate(camera, { 0, -pan_speed * dt });
		}
		if (input.KeyPressed(Key::S)) {
			Translate(camera, { 0, pan_speed * dt });
		}
		if (input.KeyPressed(Key::A)) {
			Translate(camera, { -pan_speed * dt, 0 });
		}
		if (input.KeyPressed(Key::D)) {
			Translate(camera, { pan_speed * dt, 0 });
		}

		if (input.KeyPressed(Key::Z)) {
			Rotate(camera, rotation_speed * dt);
		}

		if (input.KeyPressed(Key::X)) {
			Rotate(camera, -rotation_speed * dt);
		}

		if (input.KeyPressed(Key::E)) {
			camera.Zoom(zoom_speed * dt);
		}
		if (input.KeyPressed(Key::Q)) {
			camera.Zoom(-zoom_speed * dt);
		}

		if (input.MouseDown(Mouse::Left)) {
			StopFollow(camera);
		} else if (input.MouseDown(Mouse::Right)) {
			StartFollow(camera, mouse, follow_config);
		}

		Application::Get().render_.DrawText(
			content, center - 0 * V2_float{ 0.0f, font_size }, color, Origin::Center, font_size, {},
			{}, {}, {}, false
		);
		Application::Get().render_.DrawText(
			content, center + 1 * V2_float{ 0.0f, font_size }, color, Origin::Center, font_size, {},
			{}, {}, {}, true
		);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("Camera: WASD move, Q/E zoom", resolution);
	Application::Get().scene_.Enter<CameraScene>("");
	return 0;
}