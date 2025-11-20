#include "core/app/application.h"

#include "core/app/window.h"
#include "core/input/input_handler.h"
#include "ecs/components/origin.h"
#include "ecs/components/sprite.h"
#include "renderer/api/color.h"
#include "renderer/renderer.h"
#include "renderer/vfx/light.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

class LightScene : public Scene {
public:
	PointLight mouse_light;

	void Enter() override {
		// Application::Get().render_.SetBackgroundColor(color::White);
		SetBackgroundColor(color::LightBlue.WithAlpha(1.0f));

		Application::Get().window_.SetResizable();
		LoadResource("test", "resources/test1.jpg");

		auto sprite = CreateSprite(*this, "test", { -200, -200 });
		SetDrawOrigin(sprite, Origin::TopLeft);

		CreateRect(*this, { 0, 0 }, { 100, 100 }, color::Blue, -1.0f, Origin::TopLeft);

		float intensity{ 0.5f };
		float radius{ 30.0f };
		float falloff{ 2.0f };

		float step{ 80 };

		const auto create_light = [&](Color color) {
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

	void Update() override {
		// PTGN_LOG(input.GetMousePosition());
		SetPosition(mouse_light, input.GetMousePosition());

		// DrawDebugRect({ 300, 400 }, { 100, 100 }, color::Blue, Origin::TopLeft, -1.0f);
	}

	void Exit() override {
		json j = *this;
		// SaveJson(j, "resources/light_scene.json");
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("LightScene", { 800, 800 });
	Application::Get().scene_.Enter<LightScene>("");
	return 0;
}