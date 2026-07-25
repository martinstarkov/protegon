#include <algorithm>
#include <chrono>
#include <optional>
#include <string_view>

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/tolerance.h"
#include "core/math/vector2.h"
#include "renderer/text/text_layout.h"
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

struct TextClipScene : public Scene {
	static constexpr FontKey font{ "arial" };

	static constexpr std::string_view lorem_ipsum{
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer vitae justo sed neque "
		"consectetur feugiat. Vestibulum ante ipsum primis in faucibus orci luctus et ultrices "
		"posuere cubilia curae; Suspendisse potenti. Praesent consequat, arcu sed facilisis "
		"tincidunt, mauris neque aliquet lectus, vitae malesuada ligula sapien vel justo.\n\n"
		"Donec venenatis sem sed neque consequat, sed tincidunt erat malesuada. Curabitur tempor "
		"lectus et dolor blandit, eget facilisis lacus interdum. Sed feugiat lectus in justo "
		"porttitor, ac consequat risus volutpat. Nulla facilisi. Donec id eros vitae magna "
		"fermentum volutpat. Pellentesque habitant morbi tristique senectus et netus et malesuada "
		"fames ac turpis egestas.\n\n"
		"Aliquam erat volutpat. Nam commodo metus vel justo suscipit, vel malesuada risus "
		"condimentum. Proin vulputate ex at odio congue, sed tempor mauris posuere. Cras et "
		"ullamcorper magna. Maecenas vitae urna id justo tincidunt faucibus. Vivamus feugiat "
		"lectus sit amet nibh tincidunt, at posuere lorem pharetra.\n\n"
		"Morbi dignissim velit sed lorem malesuada, a fermentum sem posuere. Fusce nec turpis "
		"vitae nisi volutpat dictum. Aenean quis erat non lorem scelerisque facilisis. Duis "
		"pharetra, lectus non posuere consequat, magna orci pretium erat, sit amet vulputate "
		"mauris purus vel massa. Nulla at consequat arcu.\n\n"
		"Phasellus consequat, nulla sed aliquet elementum, mi libero sagittis tellus, ut "
		"vestibulum velit sapien sed lectus. Sed interdum augue vel enim vulputate, quis pretium "
		"nisl pulvinar. Integer ac lectus et massa aliquet luctus. Suspendisse tincidunt sapien "
		"nec lacus posuere, vitae convallis augue tincidunt.\n\n"
		"Nunc sit amet orci ac mauris pellentesque ullamcorper. Quisque volutpat tristique "
		"mauris, vitae pretium enim consequat sed. Etiam auctor augue a neque malesuada, vel "
		"consectetur purus luctus. In hac habitasse platea dictumst. Donec cursus tincidunt "
		"ligula, at volutpat ipsum tincidunt vel.\n\n"
		"Vivamus eu ligula nec turpis volutpat aliquet. Aenean interdum magna vitae nulla "
		"facilisis, id commodo eros pellentesque. Curabitur vitae est eget nisi consequat "
		"tempor. Sed at ipsum sed turpis volutpat rhoncus. Integer consectetur ipsum in neque "
		"interdum, in posuere lectus ullamcorper.\n\n"
		"Mauris porta sem vel erat hendrerit, ac luctus nulla ullamcorper. Praesent efficitur "
		"nibh sed massa tincidunt, et elementum ex posuere. Pellentesque tincidunt, enim eget "
		"tristique feugiat, lorem urna luctus sem, vitae feugiat velit lorem eu neque. Cras "
		"convallis justo a arcu finibus, nec malesuada lectus interdum."
	};

	V2_float viewport_size{ 700.0f, 440.0f };
	V2_float viewport_position{ 0.0f, -260.0f };

	std::optional<Text> text;
	Rect clip_rect;

	std::size_t first_visible_line{ 0 };
	std::size_t max_first_visible_line{ 0 };

	float scroll_offset{ 0.0f };
	bool clip_enabled{ true };

	bool IsMouseInsideViewport() const {
		auto mouse{ ctx().input.GetMousePosition(Frame::World) };

		Rect viewport{
			viewport_position + V2_float{ -viewport_size.x * 0.5f, 0.0f },
			viewport_position + V2_float{ viewport_size.x * 0.5f, viewport_size.y },
		};

		return mouse.x >= viewport.min.x && mouse.x <= viewport.max.x &&
			   mouse.y >= viewport.min.y && mouse.y <= viewport.max.y;
	}

	void ApplyScroll() {
		if (!text.has_value()) {
			return;
		}

		SetPosition(text.value(), viewport_position - V2_float{ 0.0f, scroll_offset });

		clip_rect = Rect{
			{ -viewport_size.x * 0.5f, scroll_offset },
			{ viewport_size.x * 0.5f, scroll_offset + viewport_size.y },
		};

		if (clip_enabled) {
			text->Clip(clip_rect, TextClipMode::Clip);
		}
	}

	void SetFirstVisibleLine(std::size_t line_index) {
		if (!text.has_value()) {
			return;
		}

		const auto& layout{ text->GetLayout() };

		if (layout.lines.empty()) {
			first_visible_line = 0;
			scroll_offset	   = 0.0f;
			ApplyScroll();
			return;
		}

		first_visible_line = std::min(line_index, max_first_visible_line);

		scroll_offset =
			layout.lines[first_visible_line].bounds.min.y - layout.lines.front().bounds.min.y;

		ApplyScroll();
	}

	void CalculateScrollRange() {
		if (!text.has_value()) {
			return;
		}

		const auto& layout{ text->GetLayout() };

		if (layout.lines.empty()) {
			max_first_visible_line = 0;
			return;
		}

		float visible_height{ 0.0f };
		auto first_line{ layout.lines.size() };

		while (first_line > 0) {
			auto candidate{ first_line - 1 };
			float next_height{ visible_height + layout.lines[candidate].size.y };

			if (visible_height > 0.0f && next_height > viewport_size.y &&
				!NearlyEqual(next_height, viewport_size.y)) {
				break;
			}

			visible_height = next_height;
			first_line	   = candidate;
		}

		max_first_visible_line = first_line;
	}

	void ScrollText() {
		if (!text.has_value() || !clip_enabled || !IsMouseInsideViewport()) {
			return;
		}

		float scroll_y{ ctx().input.GetMouseScroll().y };

		if (NearlyEqual(scroll_y, 0.0f)) {
			return;
		}

		if (scroll_y < 0.0f) {
			SetFirstVisibleLine(first_visible_line + 1);
		} else if (first_visible_line > 0) {
			SetFirstVisibleLine(first_visible_line - 1);
		}
	}

	void ToggleClip() {
		if (!text.has_value()) {
			return;
		}

		clip_enabled = !clip_enabled;

		if (clip_enabled) {
			text->Clip({ .rect = clip_rect, .mode = TextClipMode::Clip });
		} else {
			text->Clip(std::nullopt);
		}
	}

	void OnEnter() override {
		SetBackgroundColor(color::LightGray);

		ctx().debug.settings.text.draw_enabled = true;

		ctx().asset.Load(font, "assets/Arial.ttf");

		auto title{ CreateText(*this, { 0.0f, -360.0f }, Origin::CenterTop) };

		title.Content("Text clipping and scrolling")
			.Font(font)
			.Size(28.0f)
			.Color(color::Black)
			.Bold(true);

		auto instructions{ CreateText(*this, { 0.0f, -320.0f }, Origin::CenterTop) };

		instructions
			.Content("Hover the viewport and use the mouse wheel. Q toggles clipping. E/R zoom.")
			.Font(font)
			.Size(15.0f)
			.Color(color::Black);

		auto clipped_text{ CreateText(*this, viewport_position, Origin::CenterTop) };

		clipped_text.Content(lorem_ipsum)
			.Font(font)
			.Size(18.0f)
			.Color(color::Black)
			.Box(
				{
					{ -viewport_size.x * 0.5f, 0.0f },
					{ viewport_size.x * 0.5f, 0.0f },
				}
			)
			.Align(HorizontalAlign::Justify, VerticalAlign::Top)
			.Wrap(WrapMode::Word)
			.Overflow(OverflowMode::Overflow)
			.LineSpacing(0.15f);

		text		 = clipped_text;
		clip_enabled = true;

		CalculateScrollRange();
		SetFirstVisibleLine(0);
	}

	void OnUpdate() override {
		MoveWASD(ctx().camera, V2_float{ 300.0f } * ctx().dt().count());

		if (ctx().input.KeyPressed(Key::Q)) {
			ToggleClip();
		}

		ScrollText();

		if (ctx().input.KeyHeld(Key::E)) {
			ctx().camera.Zoom(ctx().dt().count());
		} else if (ctx().input.KeyHeld(Key::R)) {
			ctx().camera.Zoom(-ctx().dt().count());
		}
	}
};

int main(int, char**) {
	Application app{ "TextClipScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<TextClipScene>();
}
