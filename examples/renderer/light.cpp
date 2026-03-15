#include "runtime/graphics/light.h"

#include <chrono>
#include <optional>

#include "app/application.h"
#include "app/context.h"
#include "core/math/geometry/origin.h"
#include "core/math/math_utils.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

class LightScene : public Scene {
public:
	Light mouse_light;
	Light mouse_directional_light;

	void OnEnter() override {
		app().renderer.SetBackgroundColor(color::White);
		SetBackgroundColor(color::LightBlue.WithAlpha(1.0f));

		app().asset.Load("tree", "assets/jpg.jpg");

		auto sprite = CreateSprite(*this, "tree", { -200, -200 });
		SetDrawOrigin(sprite, Origin::TopLeft);

		CreateRect(*this, { 0, 0 }, { 100, 100 }, color::Blue, -1.0f, Origin::TopLeft);

		float intensity{ 0.5f };
		float radius{ 30.0f };
		float falloff{ 2.0f };

		float step{ 80 };

		const auto create_light = [&](const Color& color) {
			static float i = 1.0f;
			CreateLight(
				*this, V2_float{ -app().renderer.GetGameSize() * 0.5f } + V2_float{ i * step },
				{ .radius = radius, .color = color, .intensity = intensity, .falloff = falloff }
			);
			i++;
		};

		create_light(color::Cyan);
		create_light(color::Green);
		create_light(color::Blue);
		create_light(color::Magenta);
		create_light(color::Yellow);
		create_light(color::Cyan);
		create_light(color::White);

		mouse_light = CreateLight(
			*this, {},
			{ .radius = 50.0f, .color = color::White, .intensity = 0.1f, .falloff = 0.2f }
		);

		mouse_directional_light = CreateLight(
			*this, V2_float{ 0, -300 },
			{ .radius	  = 100.0f,
			  .color	  = color::Red,
			  .cone_angle = 10.0f,
			  .intensity  = 0.8f,
			  .falloff	  = 0.2f }
		);

		auto sprite2 = CreateSprite(*this, "tree", { -200, 150 });
		SetDrawOrigin(sprite2, Origin::TopLeft);

		CreateRect(*this, { 200, 200 }, { 100, 100 }, color::Red, -1.0f, Origin::TopLeft);
	}

	void OnUpdate() override {
		SetPosition(mouse_light, input.GetMousePosition());
		SetPosition(mouse_directional_light, input.GetMousePosition());
		float time_scale{ 0.1f };
		auto time{ static_cast<float>(app().TimeSinceStart().count()) };
		SetRotation(mouse_directional_light, DegToRad(time * time_scale));

		auto scroll{ input.GetMouseScroll() };

		if (scroll > 0.0f) {
			mouse_directional_light.SetConeAngle(*mouse_directional_light.GetConeAngle() + 5.0f);
		} else if (scroll < 0.0f) {
			mouse_directional_light.SetConeAngle(*mouse_directional_light.GetConeAngle() - 5.0f);
		}
	}
};

int main(int, char**) {
	Application app{ "LightScene: Scroll to resize cone angle" };
	app.StartWith<LightScene>();
}