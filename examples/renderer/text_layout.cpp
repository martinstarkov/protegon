#include "renderer/text/text_layout.h"

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
#include "runtime/ecs/entity.h"
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

	V2_float cell_size{ 230.0f, 112.0f };
	V2_float box_size{ 210.0f, 74.0f };

	float top{ -330.0f };

	V2_float GetCellTop(int column, int row) const {
		return {
			-cell_size.x * static_cast<float>(kColumnCount - 1) * 0.5f +
				cell_size.x * static_cast<float>(column),
			top + cell_size.y * static_cast<float>(row),
		};
	}

	Text CreateTitle(V2_float position, std::string_view text) {
		auto text{ CreateText(*this, position, Origin::CenterTop) };

		text.Content(text).Font(font).Size(13.0f).Color(color::Black).Bold(true);

		return text;
	}

	Text CreateBody(
		V2_float position, V2_float size, std::string_view content,
		HorizontalAlign horizontal_align, VerticalAlign vertical_align, WrapMode wrap_mode,
		OverflowMode overflow_mode
	) {
		auto text{ CreateText(*this, position, Origin::CenterTop) };

		Rect box{
			{ -size.x * 0.5f, 0.0f },
			{ size.x * 0.5f, size.y },
		};

		text.Content(content)
			.Font(font)
			.Size(14.0f)
			.Color(color::Black)
			.Box(box)
			.Align(horizontal_align, vertical_align)
			.Wrap(wrap_mode)
			.Overflow(overflow_mode)
			.LineSpacing(0.0f);

		return text;
	}

	Text CreateCell(
		int column, int row, std::string_view text, std::string_view content,
		HorizontalAlign horizontal_align, VerticalAlign vertical_align, WrapMode wrap_mode,
		OverflowMode overflow_mode, V2_float size = {}
	) {
		V2_float cell_top{ GetCellTop(column, row) };

		CreateTitle(cell_top, text);

		V2_float used_size{ size };

		if (!used_size.IsPositive()) {
			used_size = box_size;
		}

		return CreateBody(
			cell_top + V2_float{ 0.0f, 22.0f }, used_size, content, horizontal_align,
			vertical_align, wrap_mode, overflow_mode
		);
	}

	void OnEnter() override {
		SetBackgroundColor(color::LightGray);

		ctx().debug.text.draw_enabled = true;

		ctx().asset.Load(font, "assets/Arial.ttf");

		auto title{ CreateText(*this, {}, Origin::TopLeft) };

		title.Content("Text layout demo").Font(font).Size(28.0f).Color(color::Black).Bold(true);

		SetPosition(title, { -title.GetSize().x * 0.5f, -380.0f });

		CreateCell(
			0, 0, "WrapMode::None", "This long line will not attempt to wrap.",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::None, OverflowMode::Overflow
		);

		CreateCell(
			1, 0, "WrapMode::Word",
			"The quick brown fox jumps over the lazy dog using word wrapping.",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::Word, OverflowMode::Overflow
		);

		CreateCell(
			2, 0, "WrapMode::Character",
			"Pneumonoultramicroscopicsilicovolcanoconiosis character wrapping. But not for short "
			"words like this",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::Character, OverflowMode::Overflow
		);

		V2_float overflow_box_size{ 210.0f, 38.0f };

		CreateCell(
			0, 1, "OverflowMode::Overflow",
			"This text is not clipped to the text box bounds. Outside content stays. This text is "
			"too long for the box, so it keeps drawing outside the magenta box",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::Word, OverflowMode::Overflow,
			overflow_box_size
		);

		CreateCell(
			1, 1, "OverflowMode::ClipPartial",
			"Yo! This text is clipped to the text box bounds. Outside content disappears. This "
			"text is too long for the box, so it keeps drawing outside the magenta box",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::Word, OverflowMode::ClipPartial,
			overflow_box_size
		);

		CreateCell(
			2, 1, "OverflowMode::Clip",
			"Yo! This text is clipped to the text box bounds. Outside content disappears. This "
			"text is too long for the box, so it keeps drawing outside the magenta box",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::Word, OverflowMode::Clip,
			overflow_box_size
		);

		CreateCell(
			0, 2, "OverflowMode::Ellipsis",
			"This text is clipped to the text box bounds. Outside content disappears. This text is "
			"too long for the box, so it keeps drawing outside the magenta box",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::Word, OverflowMode::Ellipsis,
			overflow_box_size
		);

		CreateCell(
			1, 2, "OverflowMode::ScaleToFit",
			"This long text should shrink until it fits inside the available text box. This long "
			"text should shrink until it fits inside the available text box.",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::Word, OverflowMode::ScaleToFit,
			overflow_box_size
		)
			.ScaleToFit(0.35f, 1.0f);

		CreateCell(
			2, 2, "OverflowMode::ScaleToFit",
			"This long text should grow until it fits inside the available text box.",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::Word, OverflowMode::ScaleToFit
		)
			.ScaleToFit(0.35f, 2.0f);

		CreateCell(
			0, 3, "CollapseSpaces", "Many     spaces     collapse     into     single spaces.",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::Word, OverflowMode::Overflow
		)
			.CollapseSpaces();

		CreateCell(
			1, 3, "HorizontalAlign::Justify",
			"Justified text spreads spaces so wrapped lines fill the full width.",
			HorizontalAlign::Justify, VerticalAlign::Top, WrapMode::Word, OverflowMode::Overflow
		);

		CreateCell(
			2, 3, "JustifyLastLine",
			"Justified text spreads spaces so wrapped lines fill the full width.",
			HorizontalAlign::Justify, VerticalAlign::Top, WrapMode::Word, OverflowMode::Overflow
		)
			.JustifyLastLine();

		CreateCell(
			0, 4, "HorizontalAlign::Left", "short\nmedium line\nthis is the longest line",
			HorizontalAlign::Left, VerticalAlign::Top, WrapMode::None, OverflowMode::Overflow
		);

		CreateCell(
			1, 4, "HorizontalAlign::Center", "short\nmedium line\nthis is the longest line",
			HorizontalAlign::Center, VerticalAlign::Top, WrapMode::None, OverflowMode::Overflow
		);

		CreateCell(
			2, 4, "HorizontalAlign::Right", "short\nmedium line\nthis is the longest line",
			HorizontalAlign::Right, VerticalAlign::Top, WrapMode::None, OverflowMode::Overflow
		);

		CreateCell(
			0, 5, "VerticalAlign::Top", "top aligned", HorizontalAlign::Center, VerticalAlign::Top,
			WrapMode::None, OverflowMode::Overflow
		);

		CreateCell(
			1, 5, "VerticalAlign::Center", "center aligned", HorizontalAlign::Center,
			VerticalAlign::Center, WrapMode::None, OverflowMode::Overflow
		);

		CreateCell(
			2, 5, "VerticalAlign::Bottom", "bottom aligned", HorizontalAlign::Center,
			VerticalAlign::Bottom, WrapMode::None, OverflowMode::Overflow
		);
	}

	void OnUpdate() override {
		MoveWASD(ctx().camera, V2_float{ 300.0f } * ctx().dt().count());

		if (ctx().input.KeyHeld(Key::Q)) {
			ctx().camera.Zoom(ctx().dt().count());
		} else if (ctx().input.KeyHeld(Key::E)) {
			ctx().camera.Zoom(-ctx().dt().count());
		}
	}
};

int main(int, char**) {
	Application app{ "TextLayoutScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<TextLayoutScene>();
}
