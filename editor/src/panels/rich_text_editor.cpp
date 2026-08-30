#include "panels/rich_text_editor.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "commands/entity/entity_reference.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/angle.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/util/hash.h"
#include "editor/editor.h"
#include "editor/editor_context.h"
#include "panels/entity_filter_editor.h"
#include "panels/inspector_feature_helpers.h"
#include "panels/inspector_fields.h"
#include "panels/scene_hierarchy.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/renderer.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/offsets.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/prefab.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_group.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/custom_shader.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/bloom.h"
#include "runtime/graphics/fx/blur.h"
#include "runtime/graphics/fx/gaussian_blur.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/graphics.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/sprite_stack.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/platformer_jump.h"
#include "runtime/physics/platformer_jump_registry.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/scripting/script.h"
#include "runtime/timer/timer.h"
#include "runtime/timer/timer_event.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/dialogue.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/slider.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"

namespace ptgn::editor::inspector {

namespace {

struct RichTextSelectionState {
	std::size_t cursor{ 0 };
	std::size_t selection_start{ 0 };
	std::size_t selection_end{ 0 };
	bool apply_selection{ false };
	bool focus_source{ false };
};

struct RichTextEditorState {
	RichTextSelectionState inline_selection{};
	RichTextSelectionState window_selection{};
	bool initialized{ false };
	bool window_open{ false };
	std::array<float, 4> color{ 1.0f, 1.0f, 1.0f, 1.0f };
	std::string font{};
	float size{ kDefaultFontSize };
};

struct RichTextColorPreset {
	const char* name{};
	Color color{};
};

inline constexpr std::array kRichTextColorPresets{
	RichTextColorPreset{ "White", color::White },
	RichTextColorPreset{ "Black", color::Black },
	RichTextColorPreset{ "Red", color::Red },
	RichTextColorPreset{ "Light Red", color::LightRed },
	RichTextColorPreset{ "Orange", color::Orange },
	RichTextColorPreset{ "Yellow", color::Yellow },
	RichTextColorPreset{ "Gold", color::Gold },
	RichTextColorPreset{ "Green", color::Green },
	RichTextColorPreset{ "Blue", color::Blue },
	RichTextColorPreset{ "Sky Blue", color::SkyBlue },
	RichTextColorPreset{ "Cyan", color::Cyan },
	RichTextColorPreset{ "Teal", color::Teal },
	RichTextColorPreset{ "Magenta", color::Magenta },
	RichTextColorPreset{ "Purple", color::Purple },
	RichTextColorPreset{ "Pink", color::Pink },
	RichTextColorPreset{ "Gray", color::Gray },
	RichTextColorPreset{ "Light Gray", color::LightGray },
	RichTextColorPreset{ "Dark Gray", color::DarkGray },
};

inline constexpr std::array<float, 17> kRichTextCommonFontSizes{
	8.0f, 9.0f, 10.0f, 10.5f, 11.0f, 12.0f, 14.0f, 16.0f, 18.0f,
	20.0f, 22.0f, 24.0f, 26.0f, 28.0f, 36.0f, 48.0f, 72.0f,
};

int CaptureRichTextEditorSelection(ImGuiInputTextCallbackData* data) {
	auto* state{ static_cast<RichTextSelectionState*>(data->UserData) };
	if (!state) {
		return 0;
	}

	if (state->apply_selection) {
		const auto clamp_position = [data](std::size_t position) {
			return static_cast<int>(std::min(position, static_cast<std::size_t>(data->BufTextLen)));
		};

		data->CursorPos = clamp_position(state->cursor);
		data->SelectionStart = clamp_position(state->selection_start);
		data->SelectionEnd = clamp_position(state->selection_end);
		state->apply_selection = false;
	}

	state->cursor = static_cast<std::size_t>(std::max(data->CursorPos, 0));
	state->selection_start = static_cast<std::size_t>(std::max(data->SelectionStart, 0));
	state->selection_end = static_cast<std::size_t>(std::max(data->SelectionEnd, 0));
	return 0;
}

void ClampRichTextSelection(RichTextSelectionState& state, std::size_t size) {
	state.cursor = std::min(state.cursor, size);
	state.selection_start = std::min(state.selection_start, size);
	state.selection_end = std::min(state.selection_end, size);
}

void RequestRichTextSelection(
	RichTextSelectionState& state, std::size_t cursor, std::size_t selection_start,
	std::size_t selection_end
) {
	state.cursor = cursor;
	state.selection_start = selection_start;
	state.selection_end = selection_end;
	state.apply_selection = true;
	state.focus_source = true;
}

bool WrapRichTextSelection(
	std::string& source, RichTextSelectionState& state, std::string_view open,
	std::string_view close
) {
	ClampRichTextSelection(state, source.size());

	std::size_t begin{ std::min(state.selection_start, state.selection_end) };
	std::size_t end{ std::max(state.selection_start, state.selection_end) };

	if (begin == end) {
		begin = state.cursor;
		end = state.cursor;
	}

	source.insert(end, close);
	source.insert(begin, open);

	if (begin == end) {
		const std::size_t cursor{ begin + open.size() };
		RequestRichTextSelection(state, cursor, cursor, cursor);
	} else {
		const std::size_t selection_start{ begin + open.size() };
		const std::size_t selection_end{ end + open.size() };
		RequestRichTextSelection(state, selection_end, selection_start, selection_end);
	}

	return true;
}

[[nodiscard]] bool RichTextOpeningTagMatches(
	std::string_view source, std::size_t selection_begin, std::string_view tag,
	std::size_t& open_begin
) {
	if (selection_begin == 0 || source[selection_begin - 1] != '>') {
		return false;
	}

	open_begin = source.rfind('<', selection_begin - 1);
	if (open_begin == std::string_view::npos || open_begin + 1 >= selection_begin - 1) {
		return false;
	}

	std::string_view token{ source.substr(open_begin + 1, selection_begin - open_begin - 2) };
	if (token.starts_with('/')) {
		return false;
	}

	const auto equals{ token.find('=') };
	const std::string_view name{ token.substr(0, equals) };
	if (name == tag) {
		return true;
	}

	return tag == "s" && name == "strike";
}

[[nodiscard]] bool RichTextClosingTagMatches(
	std::string_view source, std::size_t selection_end, std::string_view tag,
	std::size_t& close_size
) {
	const std::string close{ "</" + std::string{ tag } + ">" };
	if (source.substr(selection_end, close.size()) == close) {
		close_size = close.size();
		return true;
	}

	if (tag == "s") {
		static constexpr std::string_view strike_close{ "</strike>" };
		if (source.substr(selection_end, strike_close.size()) == strike_close) {
			close_size = strike_close.size();
			return true;
		}
	}

	return false;
}

bool ToggleRichTextSelectionTag(
	std::string& source, RichTextSelectionState& state, std::string_view tag,
	std::string_view default_open, std::string_view close
) {
	ClampRichTextSelection(state, source.size());

	const std::size_t begin{ std::min(state.selection_start, state.selection_end) };
	const std::size_t end{ std::max(state.selection_start, state.selection_end) };
	if (begin == end) {
		return WrapRichTextSelection(source, state, default_open, close);
	}

	std::size_t open_begin{};
	std::size_t close_size{};
	if (!RichTextOpeningTagMatches(source, begin, tag, open_begin) ||
		!RichTextClosingTagMatches(source, end, tag, close_size)) {
		return WrapRichTextSelection(source, state, default_open, close);
	}

	const std::size_t open_size{ begin - open_begin };
	source.erase(end, close_size);
	source.erase(open_begin, open_size);

	const std::size_t selection_start{ open_begin };
	const std::size_t selection_end{ end - open_size };
	RequestRichTextSelection(state, selection_end, selection_start, selection_end);
	return true;
}

bool InsertRichTextToken(
	std::string& source, RichTextSelectionState& state, std::string_view token
) {
	ClampRichTextSelection(state, source.size());

	std::size_t begin{ std::min(state.selection_start, state.selection_end) };
	std::size_t end{ std::max(state.selection_start, state.selection_end) };
	if (begin == end) {
		begin = state.cursor;
		end = state.cursor;
	}

	source.replace(begin, end - begin, token);
	const std::size_t cursor{ begin + token.size() };
	RequestRichTextSelection(state, cursor, cursor, cursor);
	return true;
}

std::string RichTextEditorColorTag(const std::array<float, 4>& value) {
	auto byte = [](float component) {
		return static_cast<std::uint8_t>(
			std::clamp(std::lround(component * 255.0f), 0l, 255l)
		);
	};

	constexpr char digits[]{ "0123456789ABCDEF" };
	const std::array<std::uint8_t, 4> rgba{
		byte(value[0]), byte(value[1]), byte(value[2]), byte(value[3])
	};

	std::string result{ "#00000000" };
	for (std::size_t i{ 0 }; i < rgba.size(); ++i) {
		result[1 + i * 2] = digits[(rgba[i] >> 4) & 0x0F];
		result[2 + i * 2] = digits[rgba[i] & 0x0F];
	}

	if (rgba[3] == 255) {
		result.resize(7);
	}
	return result;
}

void SetRichTextEditorColor(std::array<float, 4>& destination, Color value) {
	destination = {
		static_cast<float>(value.r) / 255.0f,
		static_cast<float>(value.g) / 255.0f,
		static_cast<float>(value.b) / 255.0f,
		static_cast<float>(value.a) / 255.0f,
	};
}

[[nodiscard]] Color GetRichTextEditorColor(const std::array<float, 4>& value) {
	auto byte = [](float component) {
		return static_cast<std::uint8_t>(
			std::clamp(std::lround(component * 255.0f), 0l, 255l)
		);
	};
	return Color{ byte(value[0]), byte(value[1]), byte(value[2]), byte(value[3]) };
}

[[nodiscard]] std::vector<std::string> GetLoadedRichTextFontKeys(EditorContext& ctx) {
	std::vector<std::string> keys;
	keys.emplace_back(kDefaultFont);

	::ptgn::impl::AssetAccessor assets{ ctx.editor.GetAssetManager() };
	const auto records{ assets.GetAssets() };
	for (const auto& record : records) {
		if (record.kind != AssetKind::Font || record.key.value.empty()) {
			continue;
		}

		const FontKey key{ record.key.value };
		if (!assets.TryGet<Font>(key).has_value() || std::ranges::contains(keys, key.value)) {
			continue;
		}

		keys.emplace_back(key.value);
	}

	if (keys.size() > 1) {
		std::ranges::sort(keys.begin() + 1, keys.end());
	}
	return keys;
}

void StepRichTextCommonFontSize(float& size, int direction) {
	if (direction > 0) {
		for (const float common_size : kRichTextCommonFontSizes) {
			if (common_size > size + 0.001f) {
				size = common_size;
				return;
			}
		}
		size = kRichTextCommonFontSizes.back();
		return;
	}

	for (auto it{ kRichTextCommonFontSizes.rbegin() }; it != kRichTextCommonFontSizes.rend(); ++it) {
		if (*it < size - 0.001f) {
			size = *it;
			return;
		}
	}
	size = kRichTextCommonFontSizes.front();
}

[[nodiscard]] float RichTextSourceWidth(std::string_view source) {
	float width{ 0.0f };
	while (true) {
		const auto newline{ source.find('\n') };
		const std::string_view line{
			newline == std::string_view::npos ? source : source.substr(0, newline)
		};
		width = std::max(
			width,
			ImGui::CalcTextSize(line.data(), line.data() + line.size(), false).x
		);

		if (newline == std::string_view::npos) {
			break;
		}
		source.remove_prefix(newline + 1);
	}

	return width + ImGui::GetStyle().FramePadding.x * 2.0f + 16.0f;
}

bool DrawRichTextSourceInput(
	std::string& source, RichTextSelectionState& selection, float editor_height
) {
	const float available_width{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
	const float source_width{ RichTextSourceWidth(source) };
	const bool needs_horizontal_scroll{ source_width > available_width + 1.0f };
	const float input_width{ std::max(available_width, source_width) };

	auto draw_input = [&](float width) {
		if (selection.focus_source) {
			ImGui::SetKeyboardFocusHere();
			selection.focus_source = false;
		}

		return ImGui::InputTextMultiline(
			"##RichTextSource", &source, ImVec2{ width, editor_height },
			ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_AllowTabInput |
				(needs_horizontal_scroll ? ImGuiInputTextFlags_NoHorizontalScroll
										 : ImGuiInputTextFlags_None),
			&CaptureRichTextEditorSelection, &selection
		);
	};

	if (!needs_horizontal_scroll) {
		return draw_input(available_width);
	}

	const float child_height{
		editor_height + ImGui::GetStyle().ScrollbarSize +
		ImGui::GetStyle().FramePadding.y * 2.0f + 2.0f
	};
	ImGui::BeginChild(
		"##RichTextSourceHorizontalScroll", ImVec2{ -FLT_MIN, child_height }, false,
		ImGuiWindowFlags_HorizontalScrollbar
	);
	const bool changed{ draw_input(input_width) };
	ImGui::EndChild();
	return changed;
}

[[nodiscard]] std::size_t RichTextUtf8CharacterLength(unsigned char first_byte) {
	if ((first_byte & 0x80u) == 0u) return 1;
	if ((first_byte & 0xE0u) == 0xC0u) return 2;
	if ((first_byte & 0xF0u) == 0xE0u) return 3;
	if ((first_byte & 0xF8u) == 0xF0u) return 4;
	return 1;
}

[[nodiscard]] std::size_t RichTextCharacterPosition(
	std::string_view source, std::size_t byte_position
) {
	byte_position = std::min(byte_position, source.size());
	std::size_t character{ 1 };
	for (std::size_t i{ 0 }; i < byte_position;) {
		const auto length{ std::min(
			RichTextUtf8CharacterLength(static_cast<unsigned char>(source[i])),
			byte_position - i
		) };
		i += std::max<std::size_t>(1, length);
		++character;
	}
	return character;
}

[[nodiscard]] ImU32 RichTextPreviewColor(Color color, float alpha_scale = 1.0f) {
	const auto alpha{ static_cast<std::uint8_t>(std::clamp(
		std::lround(static_cast<float>(color.a) * alpha_scale), 0l, 255l
	)) };
	return IM_COL32(color.r, color.g, color.b, alpha);
}

void DrawRichTextPreviewGlyphText(
	ImDrawList* draw_list, ImFont* font, float font_size, ImVec2 position, ImU32 color,
	std::string_view glyph, bool italic
) {
	const int vertex_begin{ draw_list->VtxBuffer.Size };
	draw_list->AddText(
		font, font_size, position, color, glyph.data(), glyph.data() + glyph.size()
	);

	if (!italic) {
		return;
	}

	const float bottom{ position.y + font_size };
	for (int i{ vertex_begin }; i < draw_list->VtxBuffer.Size; ++i) {
		auto& vertex{ draw_list->VtxBuffer[i] };
		vertex.pos.x += (bottom - vertex.pos.y) * 0.18f;
	}
}

void DrawRichTextPreviewLayer(
	ImDrawList* draw_list, ImFont* font, float font_size, ImVec2 position, ImU32 color,
	std::string_view glyph, bool italic, float radius, float softness
) {
	const float clamped_radius{ std::clamp(radius, 0.0f, 10.0f) };
	if (clamped_radius <= 0.01f) {
		DrawRichTextPreviewGlyphText(draw_list, font, font_size, position, color, glyph, italic);
		return;
	}

	const int rings{ std::clamp(static_cast<int>(std::ceil(clamped_radius + softness)), 1, 8) };
	for (int ring{ rings }; ring >= 1; --ring) {
		const float t{ static_cast<float>(ring) / static_cast<float>(rings) };
		const float distance{ clamped_radius * t };
		const float alpha{ 0.18f + 0.52f * (1.0f - t) };
		const ImU32 ring_color{
			(color & 0x00FFFFFFu) |
			(static_cast<ImU32>(std::clamp(alpha * 255.0f, 0.0f, 255.0f)) << IM_COL32_A_SHIFT)
		};

		for (int direction{ 0 }; direction < 8; ++direction) {
			const float angle{ static_cast<float>(direction) * 0.78539816339f };
			DrawRichTextPreviewGlyphText(
				draw_list, font, font_size,
				ImVec2{
					position.x + std::cos(angle) * distance,
					position.y + std::sin(angle) * distance,
				},
				ring_color, glyph, italic
			);
		}
	}
}

struct RichTextPreviewGlyphEffect {
	ImVec2 offset{};
	float scale{ 1.0f };
};

[[nodiscard]] RichTextPreviewGlyphEffect GetRichTextPreviewGlyphEffect(
	const GlyphEffectStyle& effect, std::size_t glyph_index, double time
) {
	constexpr float kGlyphEffectPhaseStep{ 0.35f };

	RichTextPreviewGlyphEffect result;
	const float order{ static_cast<float>(glyph_index) };
	const float phase{ effect.phase + order * kGlyphEffectPhaseStep };
	const float t{ static_cast<float>(time) * effect.speed + phase };

	switch (effect.type) {
		case GlyphEffectType::None:
			break;

		case GlyphEffectType::Wave:
			result.offset.y = std::sin(t * effect.frequency) * effect.amplitude;
			break;

		case GlyphEffectType::Wobble:
			result.offset = ImVec2{
				std::sin(t * effect.frequency) * effect.amplitude,
				std::cos(t * effect.frequency * 1.37f) * effect.amplitude,
			};
			break;

		case GlyphEffectType::Shake:
			result.offset = ImVec2{
				std::sin(t * effect.frequency * 17.0f + order * 12.9898f) * effect.amplitude,
				std::cos(t * effect.frequency * 23.0f + order * 78.233f) * effect.amplitude,
			};
			break;

		case GlyphEffectType::Pulse:
			result.scale = std::max(0.1f, 1.0f + std::sin(t) * effect.amplitude);
			break;
	}

	return result;
}

void DrawRichTextPreviewGlyph(
	ImDrawList* draw_list, ImFont* font, const TextRunStyle& style, std::string_view glyph,
	ImVec2 position, float font_size, std::size_t glyph_index, double time
) {
	const auto effect{ GetRichTextPreviewGlyphEffect(style.effect, glyph_index, time) };
	const float draw_size{ std::max(1.0f, font_size * effect.scale) };
	position.x += effect.offset.x;
	position.y += effect.offset.y + (font_size - draw_size) * 0.5f;

	const bool italic{ HasFontFlag(style.flags, FontStyle::Italic) };
	const bool bold{ HasFontFlag(style.flags, FontStyle::Bold) };
	const auto& sdf{ style.sdf };

	if (sdf.outer_glow.color.a > 0 && sdf.outer_glow.width > 0.0f) {
		DrawRichTextPreviewLayer(
			draw_list, font, draw_size, position,
			RichTextPreviewColor(sdf.outer_glow.color, 0.55f), glyph, italic,
			sdf.outer_glow.width, sdf.outer_glow.softness
		);
	}

	if (sdf.shadow.color.a > 0) {
		const ImVec2 shadow_position{
			position.x + sdf.shadow_offset.x,
			position.y + sdf.shadow_offset.y,
		};
		DrawRichTextPreviewLayer(
			draw_list, font, draw_size, shadow_position,
			RichTextPreviewColor(sdf.shadow.color, 0.85f), glyph, italic,
			sdf.shadow.width, sdf.shadow.softness
		);
	}

	if (sdf.outline.color.a > 0 && sdf.outline.width > 0.0f) {
		DrawRichTextPreviewLayer(
			draw_list, font, draw_size, position,
			RichTextPreviewColor(sdf.outline.color), glyph, italic,
			sdf.outline.width, sdf.outline.softness
		);
	}

	const ImU32 text_color{ RichTextPreviewColor(style.color) };
	DrawRichTextPreviewGlyphText(
		draw_list, font, draw_size, position, text_color, glyph, italic
	);

	if (bold) {
		const float weight{ std::clamp(style.bold_weight * draw_size, 0.0f, 4.0f) };
		if (weight > 0.01f) {
			DrawRichTextPreviewGlyphText(
				draw_list, font, draw_size, ImVec2{ position.x + weight, position.y },
				text_color, glyph, italic
			);
		}
	}

	if (sdf.inner_glow.color.a > 0 && sdf.inner_glow.width > 0.0f) {
		const float strength{
			std::clamp(sdf.inner_glow.width / (sdf.inner_glow.width + sdf.inner_glow.softness + 1.0f),
				0.08f, 0.45f)
		};
		DrawRichTextPreviewGlyphText(
			draw_list, font, draw_size, position,
			RichTextPreviewColor(sdf.inner_glow.color, strength), glyph, italic
		);
	}
}

void DrawRichTextPreview(const StyledText& styled_text) {
	constexpr float padding{ 8.0f };
	ImDrawList* draw_list{ ImGui::GetWindowDrawList() };
	ImFont* font{ ImGui::GetFont() };
	const ImVec2 origin{ ImGui::GetCursorScreenPos() };
	const double time{ ImGui::GetTime() };

	float x{ padding };
	float y{ padding };
	float line_height{ ImGui::GetTextLineHeight() };
	float line_spacing{ 0.0f };
	float max_width{ padding * 2.0f };
	std::size_t glyph_index{ 0 };
	bool drew_anything{ false };

	for (const auto& run : styled_text.runs) {
		const float font_size{ std::max(1.0f, run.style.size) };
		const float tracking{ run.style.tracking * font_size };
		std::string_view remaining{ run.text };

		for (std::size_t i{ 0 }; i < remaining.size();) {
			if (remaining[i] == '\n') {
				max_width = std::max(max_width, x + padding);
				x = padding;
				y += std::max(line_height, font_size) + line_spacing;
				line_height = font_size;
				line_spacing = run.style.line_spacing;
				++i;
				continue;
			}

			const std::size_t length{ std::min(
				RichTextUtf8CharacterLength(static_cast<unsigned char>(remaining[i])),
				remaining.size() - i
			) };
			const std::string_view glyph{ remaining.substr(i, std::max<std::size_t>(1, length)) };
			const float glyph_width{
				font->CalcTextSizeA(
					font_size, FLT_MAX, 0.0f, glyph.data(), glyph.data() + glyph.size()
				).x
			};

			const auto effect{ GetRichTextPreviewGlyphEffect(run.style.effect, glyph_index, time) };
			const ImVec2 glyph_position{
				origin.x + x,
				origin.y + y,
			};
			DrawRichTextPreviewGlyph(
				draw_list, font, run.style, glyph, glyph_position, font_size, glyph_index, time
			);

			const float advance{ glyph_width + tracking };
			const ImU32 decoration_color{ RichTextPreviewColor(run.style.color) };
			if (HasFontFlag(run.style.flags, FontStyle::Underline)) {
				const float underline_y{
					glyph_position.y + effect.offset.y + font_size * 0.92f
				};
				draw_list->AddLine(
					ImVec2{ glyph_position.x + effect.offset.x, underline_y },
					ImVec2{ glyph_position.x + effect.offset.x + advance, underline_y },
					decoration_color, std::max(1.0f, font_size * 0.055f)
				);
			}
			if (HasFontFlag(run.style.flags, FontStyle::Strikethrough)) {
				const float strike_y{
					glyph_position.y + effect.offset.y + font_size * 0.52f
				};
				draw_list->AddLine(
					ImVec2{ glyph_position.x + effect.offset.x, strike_y },
					ImVec2{ glyph_position.x + effect.offset.x + advance, strike_y },
					decoration_color, std::max(1.0f, font_size * 0.05f)
				);
			}

			x += advance;
			line_height = std::max(line_height, font_size * effect.scale);
			line_spacing = std::max(line_spacing, run.style.line_spacing);
			max_width = std::max(max_width, x + padding);
			i += std::max<std::size_t>(1, length);
			++glyph_index;
			drew_anything = true;
		}
	}

	if (!drew_anything) {
		ImGui::TextDisabled("(empty)");
		return;
	}

	const float content_height{ y + line_height + padding * 2.0f };
	ImGui::Dummy(ImVec2{ std::max(max_width, ImGui::GetContentRegionAvail().x), content_height });
}

void DrawRichTextToolbarTooltip(std::string_view text) {
	DrawTooltip(std::string{ text }.c_str());
}

bool DrawRichTextToolbar(
	EditorContext& ctx, std::string& source, TextRunDefaults& defaults,
	const RichTextEditorOptions& options, RichTextEditorState& state,
	RichTextSelectionState& selection, bool allow_detached_window
) {
	(void)defaults;
	bool changed{ false };
	const float button_height{ ImGui::GetFrameHeight() };

	auto tag_button = [&](const char* label, std::string_view tag, std::string_view open,
						  std::string_view close, std::string_view tooltip) {
		if (ImGui::Button(label, ImVec2{ 0.0f, button_height })) {
			changed |= ToggleRichTextSelectionTag(source, selection, tag, open, close);
		}
		DrawRichTextToolbarTooltip(tooltip);
	};

	tag_button("B", "b", "<b>", "</b>", "Bold.\n<b>...</b>\n<b=0.2>...</b>");
	ImGui::SameLine();
	tag_button("I", "i", "<i>", "</i>", "Italic.\n<i>...</i>");
	ImGui::SameLine();
	tag_button("U", "u", "<u>", "</u>", "Underline.\n<u>...</u>");
	ImGui::SameLine();
	tag_button("S", "s", "<s>", "</s>", "Strikethrough.\n<s>...</s>");

	ImGui::SameLine();
	if (ImGui::Button("Color", ImVec2{ 0.0f, button_height })) {
		ImGui::OpenPopup("RichTextColorPopup");
	}
	DrawRichTextToolbarTooltip(
		"Text color.\n<c=red>...</c>\n<c=#RRGGBB>...</c>\n<c=#RRGGBBAA>...</c>"
	);
	if (ImGui::BeginPopup("RichTextColorPopup")) {
		const Color current_color{ GetRichTextEditorColor(state.color) };
		const char* preset_label{ "Custom" };
		for (const auto& preset : kRichTextColorPresets) {
			if (preset.color == current_color) {
				preset_label = preset.name;
				break;
			}
		}

		ImGui::SetNextItemWidth(220.0f);
		if (ImGui::BeginCombo("Preset##RichTextColorPreset", preset_label)) {
			for (const auto& preset : kRichTextColorPresets) {
				const bool selected{ preset.color == current_color };
				if (ImGui::Selectable(preset.name, selected)) {
					SetRichTextEditorColor(state.color, preset.color);
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::ColorEdit4(
			"Custom##RichTextColor", state.color.data(), ImGuiColorEditFlags_AlphaBar
		);
		if (ImGui::Button("Apply Color", ImVec2{ -FLT_MIN, 0.0f })) {
			const std::string open{ "<c=" + RichTextEditorColorTag(state.color) + ">" };
			changed |= WrapRichTextSelection(source, selection, open, "</c>");
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	if (ImGui::Button("Font", ImVec2{ 0.0f, button_height })) {
		ImGui::OpenPopup("RichTextFontPopup");
	}
	DrawRichTextToolbarTooltip("Font key.\n<font=key>...</font>");
	if (ImGui::BeginPopup("RichTextFontPopup")) {
		const auto loaded_fonts{ GetLoadedRichTextFontKeys(ctx) };
		const char* preview{ state.font.empty() ? "Default" : state.font.c_str() };
		ImGui::SetNextItemWidth(260.0f);
		if (ImGui::BeginCombo("Loaded##RichTextFont", preview)) {
			for (const auto& font : loaded_fonts) {
				const bool selected{ state.font == font };
				const char* label{ font.empty() ? "Default" : font.c_str() };
				if (ImGui::Selectable(label, selected)) {
					state.font = font;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SetNextItemWidth(260.0f);
		ImGui::InputTextWithHint("Custom##RichTextFont", "Custom font key", &state.font);
		if (ImGui::Button("Apply Font", ImVec2{ -FLT_MIN, 0.0f })) {
			changed |= WrapRichTextSelection(
				source, selection, "<font=" + state.font + ">", "</font>"
			);
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	if (ImGui::Button("Size", ImVec2{ 0.0f, button_height })) {
		ImGui::OpenPopup("RichTextSizePopup");
	}
	DrawRichTextToolbarTooltip("Font size.\n<size=32>...</size>");
	if (ImGui::BeginPopup("RichTextSizePopup")) {
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		if (ImGui::Button("-##RichTextSize", ImVec2{ button_height, button_height })) {
			StepRichTextCommonFontSize(state.size, -1);
		}
		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat("##RichTextSize", &state.size, 0.5f, 1.0f, 1000.0f, "%.1f");
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::Button("+##RichTextSize", ImVec2{ button_height, button_height })) {
			StepRichTextCommonFontSize(state.size, 1);
		}

		if (ImGui::Button("Apply Size", ImVec2{ -FLT_MIN, 0.0f })) {
			char value[64]{};
			std::snprintf(value, sizeof(value), "<size=%.3g>", static_cast<double>(state.size));
			changed |= WrapRichTextSelection(source, selection, value, "</size>");
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(100.0f);
	if (ImGui::BeginCombo("##RichTextEffects", "Effects")) {
		auto effect_item = [&](const char* label, std::string_view open, std::string_view close,
						   std::string_view tooltip) {
			if (ImGui::Selectable(label)) {
				changed |= WrapRichTextSelection(source, selection, open, close);
			}
			DrawRichTextToolbarTooltip(tooltip);
		};

		effect_item(
			"Outline", "<outline=#000000,2,1>", "</outline>",
			"<outline=color,width[,softness]>"
		);
		effect_item(
			"Shadow", "<shadow=#00000080,3,3,0,1>", "</shadow>",
			"<shadow=color,x,y[,width[,softness]]>"
		);
		effect_item(
			"Outer Glow", "<outerglow=#00FFFF,4,1>", "</outerglow>",
			"<outerglow=color,width[,softness]>"
		);
		effect_item(
			"Inner Glow", "<innerglow=#FFFFFF,2,1>", "</innerglow>",
			"<innerglow=color,width[,softness]>"
		);
		ImGui::Separator();
		effect_item(
			"Wave", "<fx=Wave,8,2,1,0>", "</fx>",
			"<fx=Wave,amplitude,frequency,speed,phase>"
		);
		effect_item(
			"Wobble", "<fx=Wobble,4,2,1,0>", "</fx>",
			"<fx=Wobble,amplitude,frequency,speed,phase>"
		);
		effect_item(
			"Shake", "<fx=Shake,3,20,1,0>", "</fx>",
			"<fx=Shake,amplitude,frequency,speed,phase>"
		);
		effect_item(
			"Pulse", "<fx=Pulse,0.15,2,1,0>", "</fx>",
			"<fx=Pulse,amplitude,frequency,speed,phase>"
		);
		ImGui::EndCombo();
	}
	DrawRichTextToolbarTooltip(
		"Effect tags.\n"
		"<outline=color,width[,softness]>\n"
		"<shadow=color,x,y[,width[,softness]]>\n"
		"<outerglow=color,width[,softness]>\n"
		"<innerglow=color,width[,softness]>\n"
		"<fx=type[,amplitude[,frequency[,speed[,phase]]]]>"
	);

	if (allow_detached_window) {
		ImGui::SameLine();
		if (ImGui::Button("Open", ImVec2{ 0.0f, button_height })) {
			state.window_open = true;
			state.window_selection = selection;
			state.window_selection.apply_selection = true;
			state.window_selection.focus_source = true;
		}
		DrawRichTextToolbarTooltip("Open a larger rich-text editor.");
	}

	if (!options.variables.empty()) {
		ImGui::SameLine();
		ImGui::SetNextItemWidth(110.0f);
		if (ImGui::BeginCombo("##RichTextVariables", "Variables")) {
			for (const auto& variable : options.variables) {
				const std::string expression{ "${" + std::string{ variable.variable } + "}" };
				if (ImGui::Selectable(std::string{ variable.label }.c_str())) {
					changed |= InsertRichTextToken(source, selection, expression);
				}
				if (!variable.preview.empty()) {
					const std::string tooltip{
						expression + "\n" + std::string{ variable.preview }
					};
					DrawRichTextToolbarTooltip(tooltip);
				} else {
					DrawRichTextToolbarTooltip(expression);
				}
			}
			ImGui::EndCombo();
		}
		DrawRichTextToolbarTooltip("Insert a context variable.\n${name}");
	}

	return changed;
}

bool DrawRichTextEditorPanel(
	EditorContext& ctx, std::string& source, TextRunDefaults& defaults,
	const RichTextEditorOptions& options, RichTextEditorState& state,
	RichTextSelectionState& selection, bool detached
) {
	bool changed{ false };
	changed |= DrawRichTextToolbar(
		ctx, source, defaults, options, state, selection, !detached
	);

	const float editor_height{
		detached
			? std::max(300.0f, ImGui::GetContentRegionAvail().y * 0.52f)
			: ImGui::GetTextLineHeightWithSpacing() *
				  static_cast<float>(std::max(options.line_count, 3)) +
				  ImGui::GetStyle().FramePadding.y * 2.0f
	};
	if (DrawRichTextSourceInput(source, selection, editor_height)) {
		changed = true;
	}
	DrawRichTextToolbarTooltip(
		"Rich-text markup.\n"
		"<b>Bold</b>\n"
		"<c=red>Red</c>\n"
		"<font=key>Font</font>\n"
		"<size=32>Large</size>\n"
		"Escape: \\<  \\>  \\\\"
	);

	const bool defaults_open{ ImGui::TreeNodeEx(
		"Defaults##RichTextDefaults",
		ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
	) };
	DrawRichTextToolbarTooltip(
		"Base style used outside tags.\nTags temporarily override these values."
	);
	if (defaults_open) {
		ScopedIndent indent;
		changed |= DrawValue(ctx, "Font", defaults.font);
		changed |= DrawValue(ctx, "Color", defaults.style.color);
		changed |= DrawValue(ctx, "Size", defaults.style.size);
		changed |= DrawValue(ctx, "Bold Weight", defaults.style.bold_weight);
		changed |= DrawValue(ctx, "Kerning", defaults.style.kerning);
		changed |= DrawValue(ctx, "Tracking", defaults.style.tracking);
		changed |= DrawValue(ctx, "Line Spacing", defaults.style.line_spacing);
		changed |= DrawValue(ctx, "Flags", defaults.style.flags);
		changed |= DrawValue(ctx, "Distance Field", defaults.style.sdf);
		changed |= DrawValue(ctx, "Effect", defaults.style.effect);
		ImGui::TreePop();
	}

	if (options.show_preview) {
		ImGui::SeparatorText("Preview");

		const std::string expanded{ ExpandRichTextVariables(
			source,
			[&options](std::string_view name) -> std::optional<std::string> {
				for (const auto& variable : options.variables) {
					if (variable.variable == name && !variable.preview.empty()) {
						return std::string{ variable.preview };
					}
				}
				return std::nullopt;
			}
		) };
		const auto parsed{ ParseRichText(expanded, defaults) };
		const auto source_diagnostics{ ParseRichText(source, defaults).diagnostics };

		ImGui::BeginChild(
			"##RichTextPreview", ImVec2{ -FLT_MIN, detached ? 180.0f : 120.0f }, true,
			ImGuiWindowFlags_HorizontalScrollbar
		);
		DrawRichTextPreview(parsed.text);
		ImGui::EndChild();
		DrawRichTextToolbarTooltip(
			"Live preview of size, color, BIUS, spacing, SDF layers and glyph effects."
		);

		for (const auto& diagnostic : source_diagnostics) {
			ImGui::TextColored(
				ImVec4{ 1.0f, 0.45f, 0.2f, 1.0f }, "Character %zu: %s",
				RichTextCharacterPosition(source, diagnostic.position), diagnostic.message.c_str()
			);
		}
	}

	return changed;
}


} // namespace

bool DrawRichTextEditor(
	EditorContext& ctx, std::string& source, TextRunDefaults& defaults,
	const RichTextEditorOptions& options
) {
	ImGui::PushID("RichTextEditor");
	const ImGuiID state_id{ ImGui::GetID("##State") };
	static std::unordered_map<ImGuiID, RichTextEditorState> states;
	auto& state{ states[state_id] };

	if (!state.initialized) {
		state.initialized = true;
		state.inline_selection.cursor = source.size();
		state.inline_selection.selection_start = source.size();
		state.inline_selection.selection_end = source.size();
		state.window_selection = state.inline_selection;
		SetRichTextEditorColor(state.color, defaults.style.color);
		state.font = defaults.font.value;
		state.size = defaults.style.size;
	}

	ClampRichTextSelection(state.inline_selection, source.size());
	ClampRichTextSelection(state.window_selection, source.size());

	bool changed{ DrawRichTextEditorPanel(
		ctx, source, defaults, options, state, state.inline_selection, false
	) };

	if (state.window_open) {
		bool open{ true };
		const std::string title{
			"Rich Text Editor###RichTextEditorWindow_" + std::to_string(state_id)
		};
		ImGui::SetNextWindowSize(ImVec2{ 900.0f, 700.0f }, ImGuiCond_FirstUseEver);
		if (ImGui::Begin(title.c_str(), &open)) {
			ImGui::PushID(static_cast<int>(state_id));
			changed |= DrawRichTextEditorPanel(
				ctx, source, defaults, options, state, state.window_selection, true
			);
			ImGui::PopID();
		}
		ImGui::End();
		state.window_open = open;
	}

	ImGui::PopID();
	return changed;
}

} // namespace ptgn::editor::inspector
