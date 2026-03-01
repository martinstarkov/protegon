#include "renderer/vfx/light.h"

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "platform/input/input_handler.h"
#include "platform/window/window.h"
#include "renderer/renderer.h"
#include "runtime/ecs/components/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

class LightScene : public Scene {
public:
	PointLight mouse_light;

	void OnEnter() override {
		// app().renderer.SetBackgroundColor(color::White);
		SetBackgroundColor(color::LightBlue.WithAlpha(1.0f));

		app().asset.Load("test", "assets/test1.jpg");

		auto sprite = CreateSprite(*this, "test", { -200, -200 });
		SetDrawOrigin(sprite, Origin::TopLeft);

		CreateRect(*this, { 0, 0 }, { 100, 100 }, color::Blue, -1.0f, Origin::TopLeft);

		float intensity{ 0.5f };
		float radius{ 30.0f };
		float falloff{ 2.0f };

		float step{ 80 };

		const auto create_light = [&](const Color& color) {
			static int i = 1;
			CreatePointLight(
				*this, V2_float{ -camera.GetViewportSize() * 0.5f } + V2_float{ i * step }, radius,
				color, intensity, falloff
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

		// auto ambient = CreatePointLight(*this, { 400, 400 }, 400.0f, color::White, 0.0f,
		// falloff); ambient.SetAmbientColor(color::White); ambient.SetAmbientIntensity(0.1f);

		mouse_light = CreatePointLight(*this, {}, 50.0f, color::White, 0.8f, 1.0f);

		auto sprite2 = CreateSprite(*this, "test", { -200, 150 });
		SetDrawOrigin(sprite2, Origin::TopLeft);

		CreateRect(*this, { 200, 200 }, { 100, 100 }, color::Red, -1.0f, Origin::TopLeft);
		// mouse_light.SetAmbientColor(color::Red);
		// mouse_light.SetAmbientIntensity(0.1f);
	}

	void OnUpdate() override {
		// PTGN_LOG(input.GetMousePosition());
		SetPosition(mouse_light, input.GetMousePosition());

		// DrawDebugRect({ 300, 400 }, { 100, 100 }, color::Blue, Origin::TopLeft, -1.0f);
	}

	void OnExit() override {
		// TODO: Fix.
		// json j = *this;
		// SaveJson(j, "assets/light_scene.json");
	}
};

int main(int, char**) {
	Application app{ "LightScene" };
	app.StartWith<LightScene>();
}