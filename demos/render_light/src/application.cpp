#include "components/sprite.h"
#include "core/game.h"
#include "core/window.h"
#include "input/input_handler.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "renderer/vfx/light.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

class LightScene : public Scene {
public:
	PointLight mouse_light;

	void Enter() override {
		// game.renderer.SetBackgroundColor(color::White);
		SetBackgroundColor(color::LightBlue.WithAlpha(1));

		game.window.SetSetting(WindowSetting::Resizable);
		LoadResource("test", "resources/test1.jpg");

		auto sprite = CreateSprite(*this, "test", { 50, 50 });
		SetDrawOrigin(sprite, Origin::TopLeft);

		float intensity{ 0.5f };
		float radius{ 200.0f };
		float falloff{ 1.0f };

		float step{ 80 };

		const auto create_light = [&](const Color& color) {
			static int i = 1;
			CreatePointLight(*this, V2_float{ i * step }, radius, color, intensity, falloff);
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

		mouse_light = CreatePointLight(*this, {}, 300.0f, color::Red, 0.8f, 2.0f);
		// mouse_light.SetAmbientColor(color::Red);
		// mouse_light.SetAmbientIntensity(0.1f);
	}

	void Update() override {
		SetPosition(mouse_light, input.GetMousePosition());

		DrawDebugRect({ 300, 400 }, { 100, 100 }, color::Blue, Origin::TopLeft, -1.0f);
	}

	void Exit() override {
		json j = *this;
		// SaveJson(j, "resources/light_scene.json");
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("LightScene", { 800, 800 });
	game.scene.Enter<LightScene>("");
	return 0;
}