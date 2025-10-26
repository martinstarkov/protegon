#include <string>

#include "core/ecs/components/sprite.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/entity.h"
#include "core/app/application.h"
#include "core/app/window.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/text/font.h"
#include "renderer/renderer.h"
#include "renderer/text/text.h"
#include "renderer/material/texture.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };
constexpr V2_int resolution{ 640, 360 }; // 2, 2

class ResolutionTextScene : public Scene {
	Text text;
	Text text_hd;

	std::string content{ "The quick brown fox jumps over the lazy dog" };
	Color color{ color::White };
	FontSize font_size{ 20 };

	void Enter() override {
		Application::Get().window_.SetResizable();
		LoadResource("background", "resources/bg.png");
		Application::Get().render_.SetGameSize(resolution);

		CreateSprite(*this, "background", {});

		text = CreateText(*this, content, color, font_size);
		SetPosition(text, -2 * V2_float{ 0.0f, text.GetFontSize() });
		text.SetHD(false);

		text_hd = CreateText(*this, content, color, font_size);
		SetPosition(text_hd, 2 * V2_float{ 0.0f, text.GetFontSize() });
	}

	void Update() override {
		Application::Get().render_.DrawText(
			content, -1 * V2_float{ 0.0f, text.GetFontSize() }, color, Origin::Center, font_size,
			{}, {}, {}, {}, false
		);
		Application::Get().render_.DrawText(
			content, 1 * V2_float{ 0.0f, text.GetFontSize() }, color, Origin::Center, font_size, {},
			{}, {}, {}, true
		);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("ResolutionTextScene", window_size);
	Application::Get().scene_.Enter<ResolutionTextScene>("");
	return 0;
}