#include <string>

#include "app/application.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "platform/window.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };
constexpr V2_int game_size{ 640, 360 }; // 2, 2

class HDTextScene : public Scene {
	Text text;
	Text text_hd;

	std::string content{ "The quick brown fox jumps over the lazy dog" };
	Color color{ color::White };
	float font_size{ 20 };

	V2_float stride{ 0.0f, 60.0f };
	std::uint32_t wrap_after{ 320 };

	void OnEnter() override {
		ctx().asset.Load("background", "examples/assets/bg.png");
		ctx().renderer.SetGameSize(game_size);

		auto sprite = CreateSprite(*this, "background", {});
		SetDepth(sprite, 0.0f);

		text = CreateText(
			*this, -2 * stride, content, color, font_size, {}, Origin::Center,
			TextProperties{ .wrap_after = wrap_after }
		);
		text.SetHD(false);

		text_hd = CreateText(
			*this, 2 * stride, content, color, font_size, {}, Origin::Center,
			TextProperties{ .wrap_after = wrap_after }
		);
	}

	void OnUpdate() override {
		ctx().renderer.DrawText(
			content, -1 * stride, color, font_size, {}, TextProperties{ .wrap_after = wrap_after },
			Origin::Center, {}, false
		);
		ctx().renderer.DrawText(
			content, 1 * stride, color, font_size, {}, TextProperties{ .wrap_after = wrap_after },
			Origin::Center, {}, true
		);
	}
};

int main(int, char**) {
	Application app{ "HDTextScene", window_size };
	app.StartWith<HDTextScene>();
}