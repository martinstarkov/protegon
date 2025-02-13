#include <string_view>
#include <type_traits>

#include "components/draw.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "math/geometry/circle.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/render_target.h"
#include "renderer/text.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "vfx/light.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

struct BasicRectangleScene : public Scene {
	ecs::Entity rt;
	ecs::Entity t1;

	void Enter() override {
		V2_float ws{ game.window.GetSize() };
		V2_float center{ game.window.GetCenter() };

		auto c0 = manager.CreateEntity();
		c0.Add<Circle>();
		c0.Add<Transform>(center + V2_float{ 200, 170 });
		c0.Add<LineWidth>(20.0f);
		c0.Add<Radius>(V2_float{ 90 });
		c0.Add<Tint>(color::BrightGreen);
		c0.Add<Visible>();
		c0.Add<Depth>(1);

		auto r = manager.CreateEntity();
		r.Add<Rect>();
		r.Add<Transform>(center + V2_float{ 200, 200 });
		r.Add<Size>(V2_float{ 200, 100 });
		r.Add<Origin>(Origin::Center);
		r.Add<Tint>(color::Red);
		r.Add<Visible>();

		t1 = manager.CreateEntity();
		t1.Add<Triangle>(V2_float{ -150, 0 }, V2_float{ 0, -180 }, V2_float{ 150, 0 });
		t1.Add<Transform>(center + V2_float{ 0, 240 });
		t1.Add<Tint>(color::Blue);
		t1.Add<Visible>(false); // Drawn to render target.

		auto c = manager.CreateEntity();
		c.Add<Circle>();
		c.Add<Transform>(center + V2_float{ 200, 200 });
		c.Add<Radius>(V2_float{ 60 });
		c.Add<Tint>(color::LightGray);
		c.Add<Visible>();

		auto t2 = manager.CreateEntity();
		t2.Add<Triangle>(V2_float{ -150, 0 }, V2_float{ 0, -180 }, V2_float{ 150, 0 });
		t2.Add<LineWidth>(10.0f);
		t2.Add<Transform>(center + V2_float{ 0, -180 });
		t2.Add<Tint>(color::Blue);
		t2.Add<Visible>();

		V2_float light0_pos{ center + V2_float{ 100, 160 } };

		auto point_light0 = manager.CreateEntity();
		point_light0.Add<PointLight>()
			.SetRadius(250.0f)
			.SetIntensity(1.0f)
			.SetFalloff(3.0f)
			.SetColor(color::Pink)
			.SetAmbientIntensity(0.2f)
			.SetAmbientColor(color::Blue);
		point_light0.Add<Transform>(light0_pos);
		point_light0.Add<Depth>(1);
		point_light0.Add<Visible>();

		game.texture.Load("test1", "resources/test1.jpg");
		game.texture.Load("test2", "resources/test2.png");
		game.texture.Load("test01", "resources/test01.png");
		game.texture.Load("test02", "resources/test02.png");
		game.texture.Load("test03", "resources/test03.png");
		game.texture.Load("test04", "resources/test04.png");
		game.texture.Load("test05", "resources/test05.png");
		game.texture.Load("test06", "resources/test06.png");

		auto s1 = CreateSprite(manager, "test1");
		s1.Add<Transform>(V2_float{ 0, 0 });
		s1.Add<Origin>(Origin::TopLeft);
		auto s2 = CreateSprite(manager, "test2");
		s2.Add<Transform>(V2_float{ ws.x, 0 });
		s2.Add<Origin>(Origin::TopRight);

		V2_float size{ 100, 100 };

		auto create_sprite_obj = [&](std::string_view texture_key, int offset) {
			auto s = CreateSprite(manager, texture_key);
			s.Add<Transform>(V2_float{ size.x * static_cast<float>(offset), center.y });
			s.Add<Size>(size);
		};

		std::invoke(create_sprite_obj, "test01", 1);
		std::invoke(create_sprite_obj, "test02", 2);
		std::invoke(create_sprite_obj, "test03", 3);
		std::invoke(create_sprite_obj, "test04", 4);
		std::invoke(create_sprite_obj, "test05", 5);
		std::invoke(create_sprite_obj, "test06", 6);

		rt = manager.CreateEntity();
		rt.Add<RenderTarget>(manager, window_size);
		rt.Add<Transform>(center);
		rt.Add<Visible>();

		auto r2 = manager.CreateEntity();
		r2.Add<Rect>();
		r2.Add<Transform>(center + V2_float{ -100, 0 });
		r2.Add<LineWidth>(10.0f);
		r2.Add<Size>(V2_float{ 200, 200 });
		r2.Add<Origin>(Origin::Center);
		r2.Add<Tint>(color::Pink);
		r2.Add<Visible>();

		auto c2 = manager.CreateEntity();
		c2.Add<Circle>();
		c2.Add<Transform>(center + V2_float{ -200, -200 });
		c2.Add<LineWidth>(1.0f);
		c2.Add<Radius>(V2_float{ 50 });
		c2.Add<Tint>(color::Purple);
		c2.Add<Visible>();

		auto c3 = manager.CreateEntity();
		c3.Add<Circle>();
		c3.Add<Transform>(center + V2_float{ -220, -120 });
		c3.Add<LineWidth>(10.0f);
		c3.Add<Radius>(V2_float{ 80 });
		c3.Add<Tint>(color::Orange);
		c3.Add<Visible>();
		// rt.Add<Origin>(Origin::Center);
		// rt.Add<Tint>(color::Red);

		game.font.Load("test_font", "resources/test_font.ttf");

		auto text1 = manager.CreateEntity();
		text1.Add<Text>(manager, "Hello world!", color::Black, "test_font");
		text1.Add<Transform>(center - V2_float{ 0, 130 });
		text1.Add<Visible>();

		V2_float light1_pos{ center + V2_float{ 0, 160 } };
		V2_float light2_pos{ center + V2_float{ 50, -160 } };

		auto point_light1 = manager.CreateEntity();
		point_light1.Add<PointLight>()
			.SetRadius(200.0f)
			.SetIntensity(1.0f)
			.SetFalloff(3.0f)
			.SetColor(color::Cyan)
			.SetAmbientIntensity(0.2f)
			.SetAmbientColor(color::Orange);
		point_light1.Add<Transform>(light1_pos);
		point_light1.Add<Visible>();

		auto point_light2 = manager.CreateEntity();
		point_light2.Add<PointLight>()
			.SetRadius(200.0f)
			.SetIntensity(1.0f)
			.SetFalloff(3.0f)
			.SetColor(color::Orange)
			.SetAmbientIntensity(0.2f)
			.SetAmbientColor(color::Red);
		point_light2.Add<Transform>(light2_pos);
		point_light2.Add<Visible>();
	}

	void Update() override {
		const auto& r{ rt.Get<RenderTarget>() };
		r.Bind();
		r.Clear();

		r.Draw(t1);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("BasicRectangleExample", window_size, color::Transparent);
	game.scene.Enter<BasicRectangleScene>("basic_rectangle_example");
	return 0;
}