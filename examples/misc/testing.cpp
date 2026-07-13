#include <algorithm>
#include <chrono>
#include <string_view>

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "renderer/text/text_style.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/text/text.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"
#include "tools/debug/debug_system.h"

using namespace ptgn;

struct TextLayoutScene : public Scene {
	static constexpr FontKey font{ "arial" };

	static constexpr int kColumnCount{ 3 };

	float scale{ 40.0f };

	V2_float cell_size{ 230.0f, 112.0f };
	V2_float box_size{ 210.0f, 74.0f };

	float top{ -330.0f };
	float left{ -265.0f };

	Text CreateBody(
		V2_float position, V2_float size, std::string_view content,
		HorizontalAlign horizontal_align, VerticalAlign vertical_align, WrapMode wrap_mode,
		OverflowMode overflow_mode
	) {
		auto text{ CreateText(*this, position, Origin::Center) };

		Rect box{ size };

		text.Content(content)
			.Font(font)
			.Size(14.0f)
			.Color(color::Black)
			.Box(box)
			.Align(horizontal_align, vertical_align)
			.Wrap(wrap_mode)
			.Overflow(overflow_mode);

		return text;
	}

	Text CreateCell(
		int column, int row, std::string_view text, std::string_view content,
		HorizontalAlign horizontal_align, VerticalAlign vertical_align, WrapMode wrap_mode,
		OverflowMode overflow_mode, V2_float size = {}
	) {
		V2_float cell_top{
			left + cell_size.x * static_cast<float>(column),
			top + cell_size.y * static_cast<float>(row),
		};

		V2_float used_size{ size };
		if (!used_size.IsPositive()) {
			used_size = box_size;
		}

		return CreateBody(
			{}, used_size, content, horizontal_align, vertical_align, wrap_mode, overflow_mode
		);
	}

	void OnEnter() override {
		SetBackgroundColor(color::LightGray);

		ctx().debug.text.draw_enabled = true;

		ctx().asset.Load(font, "assets/Arial.ttf");

		V2_float overflow_box_size{ 210.0f, 38.0f };

		CreateCell(
			2, 1, "OverflowMode::Clip",
			"Yo! This text is clipped to the text box bounds. Outside content disappears. This "
			"text is "
			"too long for the box, so it keeps drawing outside the magenta box",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::Word, OverflowMode::Clip,
			overflow_box_size
		);
	}

	void OnUpdate() override {
		MoveWASD(ctx().camera, V2_float{ 300.0f } * ctx().dt().count());

		if (ctx().input.KeyHeld(Key::Q)) {
			scale += -10.0f * ctx().dt().count();
			ctx().camera.Zoom(V2_float{ 10.0f } * ctx().dt().count());
		} else if (ctx().input.KeyHeld(Key::E)) {
			scale += 10.0f * ctx().dt().count();
			ctx().camera.Zoom(-V2_float{ 10.0f } * ctx().dt().count());
		}

		scale = std::clamp(scale, 0.0001f, 10000.0f);
	}
};

int main(int, char**) {
	Application app{ "TextLayoutScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<TextLayoutScene>();
}