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
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/text/text_style.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"
#include "tools/debug/debug_system.h"

using namespace ptgn;

struct TextLayoutScene : public Scene {
	static constexpr std::string_view font{ "arial" };

	static constexpr int kColumnCount{ 3 };

	float scale{ 40.0f };

	V2_float cell_size{ 230.0f, 112.0f };
	V2_float box_size{ 210.0f, 74.0f };

	float top{ -330.0f };
	float left{ -265.0f };

	Text CreateTitle(V2_float position, std::string_view label) {
		auto text{ CreateText(*this, position, Origin::CenterTop) };

		text.Content(label).Font(font).Size(13.0f).Color(color::Black).Bold(true, 0.12f);

		return text;
	}

	Text CreateBody(
		V2_float position, std::string_view content, HorizontalAlign horizontal_align,
		VerticalAlign vertical_align, WrapMode wrap_mode, OverflowMode overflow_mode
	) {
		auto text{ CreateText(*this, position, Origin::CenterTop) };

		Rect box{
			{ -box_size.x * 0.5f, 0.0f },
			{ box_size.x * 0.5f, box_size.y },
		};

		text.Content(content)
			.Font(font)
			.Size(14.0f)
			.Color(color::Black)
			.Box(box)
			.Align(horizontal_align, vertical_align)
			.Wrap(wrap_mode)
			.Overflow(overflow_mode)
			.LineSpacing(2.0f);

		return text;
	}

	Text CreateCell(
		int column, int row, std::string_view label, std::string_view content,
		HorizontalAlign horizontal_align, VerticalAlign vertical_align, WrapMode wrap_mode,
		OverflowMode overflow_mode
	) {
		V2_float cell_top{
			left + cell_size.x * static_cast<float>(column),
			top + cell_size.y * static_cast<float>(row),
		};

		CreateTitle(cell_top, label);

		return CreateBody(
			cell_top + V2_float{ 0.0f, 22.0f }, content, horizontal_align, vertical_align,
			wrap_mode, overflow_mode
		);
	}

	void OnEnter() override {
		SetBackgroundColor(color::LightGray);

		ctx().debug.text.draw_enabled = true;

		ctx().asset.Load(font, "assets/Arial.ttf");

		CreateText(*this, { 0.0f, -390.0f }, Origin::CenterTop)
			.Content("Text layout demo")
			.Font(font)
			.Size(28.0f)
			.Color(color::Black)
			.Bold(true, 0.18f);

		CreateCell(
			0, 0, "WrapMode::None", "This is one very long line that overflows horizontally.",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::None, OverflowMode::Overflow
		);

		CreateCell(
			1, 0, "WrapMode::Word",
			"The quick brown fox jumps over the lazy dog using word wrapping.",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::Word, OverflowMode::Overflow
		);

		CreateCell(
			2, 0, "WrapMode::Character", "Supercalifragilisticexpialidocious character wrapping.",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::Character, OverflowMode::Overflow
		);

		CreateCell(
			0, 1, "OverflowMode::Overflow",
			"This text is too long for the box, so it keeps drawing outside the box.",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::Word, OverflowMode::Overflow
		);

		CreateCell(
			1, 1, "OverflowMode::Clip",
			"This text is clipped to the text box bounds. Outside content disappears.",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::Word, OverflowMode::Clip
		);

		CreateCell(
			2, 1, "OverflowMode::Ellipsis",
			"This text demonstrates ellipsis by limiting the box to two visible lines.",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::Word, OverflowMode::Ellipsis
		)
			.MaxLines(2, true);

		CreateCell(
			0, 2, "OverflowMode::ShrinkToFit",
			"This long text should shrink until it fits inside the available text box.",
			HorizontalAlign::Center, VerticalAlign::Center, WrapMode::Word,
			OverflowMode::ShrinkToFit
		)
			.Shrink(0.35f, 1.0f);

		CreateCell(
			1, 2, "HorizontalAlign::Left", "short\nmedium line\nthis is the longest line",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::None, OverflowMode::Overflow
		);

		CreateCell(
			2, 2, "HorizontalAlign::Center", "short\nmedium line\nthis is the longest line",
			HorizontalAlign::Center, VerticalAlign::Top, WrapMode::None, OverflowMode::Overflow
		);

		CreateCell(
			0, 3, "HorizontalAlign::Right", "short\nmedium line\nthis is the longest line",
			HorizontalAlign::Right, VerticalAlign::Top, WrapMode::None, OverflowMode::Overflow
		);

		CreateCell(
			1, 3, "HorizontalAlign::Justify",
			"Justified text spreads spaces so wrapped lines fill the full width.",
			HorizontalAlign::Justify, VerticalAlign::Top, WrapMode::Word, OverflowMode::Overflow
		);

		CreateCell(
			2, 3, "CollapseSpaces", "Many     spaces     collapse     into     single spaces.",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::Word, OverflowMode::Overflow
		)
			.CollapseSpaces();

		CreateCell(
			0, 4, "VerticalAlign::Top", "top aligned", HorizontalAlign::Center, VerticalAlign::Top,
			WrapMode::None, OverflowMode::Overflow
		);

		CreateCell(
			1, 4, "VerticalAlign::Center", "center aligned", HorizontalAlign::Center,
			VerticalAlign::Center, WrapMode::None, OverflowMode::Overflow
		);

		CreateCell(
			2, 4, "VerticalAlign::Bottom", "bottom aligned", HorizontalAlign::Center,
			VerticalAlign::Bottom, WrapMode::None, OverflowMode::Overflow
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
	PTGN_WITH_EDITOR(app);
	app.StartWith<TextLayoutScene>();
}