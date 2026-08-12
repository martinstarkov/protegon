#include "panels/settings.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "commands/undo_stack.h"
#include "editor/editor.h"
#include "editor/editor_context.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/vector2.h"
#include "core/util/span.h"
#include "panels/inspector_fields.h"
#include "platform/window.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "renderer/renderer_settings.h"
#include "tools/debug/debug_system.h"

namespace ptgn::editor::inspector {

namespace {

bool DrawLineDebugSettings(
	EditorContext& ctx,
	bool& draw_enabled,
	Color& draw_color,
	float& draw_line_width
) {
	bool changed{ false };

	changed |= DrawValue(ctx, "Draw Enabled", draw_enabled);
	changed |= DrawValue(ctx, "Draw Color", draw_color);
	changed |= DrawValue(
		ctx, "Draw Line Width", draw_line_width,
		FieldOptions{
			.speed = 0.1f,
			.min = kMinLineWidth,
			.max = 100.0,
			.format = "%.2f",
			.flags = ImGuiSliderFlags_AlwaysClamp,
		}
	);

	return changed;
}

bool DrawFillDebugSettings(
	EditorContext& ctx,
	bool& draw_enabled,
	Color& draw_color,
	FillStyle& draw_fill_style
) {
	bool changed{ false };

	changed |= DrawValue(ctx, "Draw Enabled", draw_enabled);
	changed |= DrawValue(ctx, "Draw Color", draw_color);
	changed |= DrawValue(ctx, "Draw Fill Style", draw_fill_style);

	return changed;
}

} // namespace

template <>
struct Contents<InteractiveDebugSettings> {
	static bool Draw(EditorContext& ctx, InteractiveDebugSettings& settings) {
		return DrawLineDebugSettings(
			ctx,
			settings.draw_enabled,
			settings.draw_color,
			settings.draw_line_width
		);
	}
};

template <>
struct Contents<CollisionDebugSettings> {
	static bool Draw(EditorContext& ctx, CollisionDebugSettings& settings) {
		bool changed{ false };

		changed |= DrawFillDebugSettings(
			ctx,
			settings.draw_enabled,
			settings.draw_color,
			settings.draw_fill_style
		);

		changed |= DrawValue(ctx, "Draw CCD", settings.draw_ccd);

		return changed;
	}
};

template <>
struct Contents<LightVisibilityDebugSettings> {
	static bool Draw(EditorContext& ctx, LightVisibilityDebugSettings& settings) {
		bool changed{ false };

		changed |= DrawValue(ctx, "Draw Enabled", settings.draw_enabled);
		changed |= DrawValue(ctx, "Draw Interiors", settings.draw_interiors);

		changed |= DrawValue(ctx, "Polygon Color", settings.polygon_color);
		changed |= DrawValue(ctx, "Masks Inside Color", settings.masks_inside_color);
		changed |= DrawValue(
			ctx,
			"Does Not Mask Inside Color",
			settings.does_not_mask_inside_color
		);

		changed |= DrawValue(ctx, "Draw Fill Style", settings.draw_fill_style);

		return changed;
	}
};

template <>
struct Contents<TextDebugSettings> {
	static bool Draw(EditorContext& ctx, TextDebugSettings& settings) {
		bool changed{ DrawLineDebugSettings(
			ctx,
			settings.draw_enabled,
			settings.draw_color,
			settings.draw_line_width
		) };

		changed |= DrawValue(ctx, "Clip Draw Color", settings.clip_draw_color);

		return changed;
	}
};

template <>
struct Contents<RendererSettings> {
	static bool Draw(EditorContext& ctx, RendererSettings& settings) {
		bool changed{ false };

		changed |= DrawValue(ctx, "Tone Mapping", settings.tone_mapping.op);

		if (settings.tone_mapping.op == ToneMappingOperator::Exposure ||
			settings.tone_mapping.op == ToneMappingOperator::ACES) {
			changed |= DrawValue(
				ctx,
				"Exposure",
				settings.tone_mapping.exposure,
				FieldOptions{
					.speed = 0.05f,
					.min = 0.0,
					.max = 20.0,
					.format = "%.2f",
					.flags = ImGuiSliderFlags_AlwaysClamp,
				}
			);
		}

		changed |= DrawValue(
			ctx,
			"Gamma",
			settings.gamma,
			FieldOptions{
				.speed = 0.05f,
				.min = 0.01f,
				.max = 5.0,
				.format = "%.2f",
				.flags = ImGuiSliderFlags_AlwaysClamp,
			}
		);

		return changed;
	}
};

} // namespace ptgn::editor::inspector

namespace ptgn::editor {

namespace {

using namespace inspector;

constexpr std::uint64_t kProjectDisplaySettingsUndoKey{ 0x5345540001ULL };
constexpr std::uint64_t kProjectRenderingSettingsUndoKey{ 0x5345540002ULL };
constexpr std::uint64_t kEditorGeneralSettingsUndoKey{ 0x5345540003ULL };
constexpr std::uint64_t kDebugInteractionSettingsUndoKey{ 0x5345540004ULL };
constexpr std::uint64_t kDebugCollisionSettingsUndoKey{ 0x5345540005ULL };
constexpr std::uint64_t kDebugTextSettingsUndoKey{ 0x5345540006ULL };
constexpr std::uint64_t kDebugVisibilitySettingsUndoKey{ 0x5345540007ULL };

template <typename T, typename Apply>
void TrackSettingsChange(
	EditorContext& ctx,
	std::uint64_t key,
	std::string label,
	bool changed,
	T before,
	T after,
	Apply apply
) {
	if (!changed) {
		return;
	}

	ctx.undo.TrackInteraction(
		key,
		std::move(label),
		true,
		ImGui::IsAnyItemActive(),
		[apply, before = std::move(before)]() mutable {
			apply(before);
		},
		[apply, after = std::move(after)]() mutable {
			apply(after);
		}
	);
}

struct ProjectDisplaySettingsState {
	RendererSettings renderer;
	WindowSettings window;
};

struct ResolutionPreset {
	const char* label{ "" };
	V2_int size;
};

constexpr std::array<ResolutionPreset, 12> kResolutionPresets{
	{ { "320 x 180 (16:9)", { 320, 180 } },
	  { "640 x 360 (16:9)", { 640, 360 } },
	  { "800 x 450 (16:9)", { 800, 450 } },
	  { "960 x 540 (16:9)", { 960, 540 } },
	  { "1280 x 720 (HD)", { 1280, 720 } },
	  { "1600 x 900", { 1600, 900 } },
	  { "1920 x 1080 (Full HD)", { 1920, 1080 } },
	  { "256 x 224 (SNES)", { 256, 224 } },
	  { "320 x 240 (4:3)", { 320, 240 } },
	  { "640 x 480 (VGA)", { 640, 480 } },
	  { "800 x 600 (SVGA)", { 800, 600 } },
	  { "1024 x 768 (XGA)", { 1024, 768 } } }
};

constexpr std::array kSettingsPages{
	SettingsPage::ProjectDisplay,
	SettingsPage::ProjectRendering,
	SettingsPage::EditorGeneral,
	SettingsPage::DebugInteraction,
	SettingsPage::DebugCollision,
	SettingsPage::DebugText,
	SettingsPage::DebugVisibility,
};

static_assert(!ContainsDuplicates(kResolutionPresets, &ResolutionPreset::size));
static_assert(!ContainsDuplicates(kResolutionPresets, &ResolutionPreset::label));

[[nodiscard]] bool ContainsInsensitive(
	std::string_view text,
	std::string_view query
) {
	if (query.empty()) {
		return true;
	}

	const auto result{ std::search(
		text.begin(),
		text.end(),
		query.begin(),
		query.end(),
		[](char lhs, char rhs) {
			return std::tolower(static_cast<unsigned char>(lhs)) ==
				   std::tolower(static_cast<unsigned char>(rhs));
		}
	) };

	return result != text.end();
}

[[nodiscard]] bool IsSearchSeparator(char value) {
	return !std::isalnum(static_cast<unsigned char>(value));
}

[[nodiscard]] bool IsNavigationSearchTerm(std::string_view term) {
	constexpr std::array navigation_terms{
		std::string_view{ "project" },
		std::string_view{ "editor" },
		std::string_view{ "debug" },
		std::string_view{ "setting" },
		std::string_view{ "settings" },
		std::string_view{ "display" },
		std::string_view{ "rendering" },
		std::string_view{ "general" },
		std::string_view{ "interaction" },
		std::string_view{ "interactions" },
		std::string_view{ "collision" },
		std::string_view{ "collisions" },
		std::string_view{ "text" },
		std::string_view{ "box" },
		std::string_view{ "boxes" },
		std::string_view{ "visibility" },
		std::string_view{ "polygon" },
		std::string_view{ "polygons" },
	};

	return std::ranges::any_of(
		navigation_terms,
		[term](std::string_view candidate) {
			return candidate.size() == term.size() &&
				   ContainsInsensitive(candidate, term);
		}
	);
}

[[nodiscard]] bool MatchesSearchTerms(
	std::string_view filter,
	std::initializer_list<std::string_view> values,
	bool ignore_navigation_terms
) {
	if (filter.empty()) {
		return true;
	}

	std::size_t position{ 0 };

	while (position < filter.size()) {
		while (position < filter.size() && IsSearchSeparator(filter[position])) {
			++position;
		}

		const std::size_t start{ position };

		while (position < filter.size() && !IsSearchSeparator(filter[position])) {
			++position;
		}

		if (start == position) {
			continue;
		}

		const std::string_view term{ filter.substr(start, position - start) };

		if (ignore_navigation_terms && IsNavigationSearchTerm(term)) {
			continue;
		}


		if (!std::ranges::any_of(values, [term](std::string_view value) {
				return ContainsInsensitive(value, term);
			})) {
			return false;
		}
	}

	return true;
}

[[nodiscard]] bool MatchesFilter(
	std::string_view filter,
	std::initializer_list<std::string_view> values
) {
	return MatchesSearchTerms(filter, values, true);
}

[[nodiscard]] bool MatchesPageFilter(
	std::string_view filter,
	std::initializer_list<std::string_view> values
) {
	return MatchesSearchTerms(filter, values, false);
}

[[nodiscard]] bool PageMatchesFilter(
	SettingsPage page,
	std::string_view filter
) {
	switch (page) {
		case SettingsPage::ProjectDisplay:
			return MatchesPageFilter(
				filter,
				{
					"project display resolution source preset logical size scaling mode",
					"window size default window size resizable start maximized",
					"window background renderer background",
				}
			);

		case SettingsPage::ProjectRendering:
			return MatchesPageFilter(
				filter,
				{
					"project rendering tone mapping exposure gamma",
					"color correction hdr",
				}
			);

		case SettingsPage::EditorGeneral:
			return MatchesPageFilter(
				filter,
				{
					"editor general entity picking",
					"render only selected scene",
					"local gizmo orientation transform",
					"viewport aspect ratio lock logical size",
					"show read only inspector data components members",
				}
			);

		case SettingsPage::DebugInteraction:
			return MatchesPageFilter(
				filter,
				{
					"debug interaction interactions draw enabled color line width",
				}
			);

		case SettingsPage::DebugCollision:
			return MatchesPageFilter(
				filter,
				{
					"debug collision collisions draw enabled color fill style ccd",
					"continuous collision detection",
				}
			);

		case SettingsPage::DebugText:
			return MatchesPageFilter(
				filter,
				{
					"debug text text boxes bounds draw enabled color line width clip color",
				}
			);

		case SettingsPage::DebugVisibility:
			return MatchesPageFilter(
				filter,
				{
					"debug visibility polygons lighting draw enabled interiors",
					"polygon color masks inside does not mask inside fill style",
				}
			);
	}

	return false;
}

[[nodiscard]] const char* PageTitle(SettingsPage page) {
	switch (page) {
		case SettingsPage::ProjectDisplay: return "Project / Display";
		case SettingsPage::ProjectRendering: return "Project / Rendering";
		case SettingsPage::EditorGeneral: return "Editor / General";
		case SettingsPage::DebugInteraction: return "Debug / Interactions";
		case SettingsPage::DebugCollision: return "Debug / Collisions";
		case SettingsPage::DebugText: return "Debug / Text Boxes";
		case SettingsPage::DebugVisibility: return "Debug / Visibility Polygons";
	}

	return "Settings";
}

[[nodiscard]] const char* PageDescription(SettingsPage page) {
	switch (page) {
		case SettingsPage::ProjectDisplay:
			return "Configure the project display, window, and presentation resolution.";

		case SettingsPage::ProjectRendering:
			return "Configure tone mapping and output color correction.";

		case SettingsPage::EditorGeneral:
			return "Configure editor-only selection, rendering, gizmo, and inspector behavior.";

		case SettingsPage::DebugInteraction:
			return "Configure how interactive regions are drawn for debugging.";

		case SettingsPage::DebugCollision:
			return "Configure collider and continuous collision detection visualization.";

		case SettingsPage::DebugText:
			return "Configure text bounds and clipping visualization.";

		case SettingsPage::DebugVisibility:
			return "Configure light visibility polygon visualization.";
	}

	return "";
}

void DrawSectionTitle(std::string_view title) {
	ImGui::Spacing();
	ImGui::TextUnformatted(title.data(), title.data() + title.size());
	ImGui::Separator();
	ImGui::Spacing();
}

bool DrawResolutionMode(EditorContext& ctx) {
	constexpr std::array names{
		"Use Window Size",
		"Use Logical Size",
	};

	auto& renderer{ ctx.editor.GetRenderer() };

	int mode{ renderer.GetSettings().logical_size.has_value() ? 1 : 0 };

	bool changed{ DrawPropertyRow("Resolution Source", [&]() {
		return ImGui::Combo(
			"##value",
			&mode,
			names.data(),
			static_cast<int>(names.size())
		);
	}) };

	if (!changed) {
		return false;
	}

	if (mode == 0) {
		renderer.SetLogicalSize(std::nullopt);
	} else {
		renderer.SetLogicalSize(renderer.GetDisplayViewport().size);
	}

	return true;
}

bool DrawResolutionPreset(EditorContext& ctx) {
	auto& renderer{ ctx.editor.GetRenderer() };

	auto logical_size{ renderer.GetLogicalSize() };

	auto selected{ std::ranges::find_if(
		kResolutionPresets,
		[&](const ResolutionPreset& preset) {
			return preset.size == logical_size;
		}
	) };

	auto preview{ selected != kResolutionPresets.end() ? selected->label : "Custom" };

	return DrawPropertyRow("Preset", [&]() {
		bool changed{ false };

		if (ImGui::BeginCombo("##value", preview)) {
			for (const auto& preset : kResolutionPresets) {
				bool is_selected{ preset.size == logical_size };

				if (ImGui::Selectable(preset.label, is_selected)) {
					renderer.SetLogicalSize(preset.size);
					changed = true;
				}

				if (is_selected) {
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}

		return changed;
	});
}

bool DrawProjectDisplaySettings(
	EditorContext& ctx,
	std::string_view filter
) {
	bool changed{ false };
	auto& renderer{ ctx.editor.GetRenderer() };
	auto& window{ ctx.editor.GetWindow() };
	const ProjectDisplaySettingsState before{
		.renderer = renderer.GetSettings(),
		.window = window.GetSettings(),
	};

	const bool show_resolution{
		MatchesFilter(
			filter,
			{
				"resolution source",
				"preset",
				"logical size",
				"window size",
				"scaling mode",
			}
		)
	};

	if (show_resolution) {
		DrawSectionTitle("Resolution");

		if (MatchesFilter(filter, { "resolution source", "window size", "logical size" })) {
			changed |= DrawResolutionMode(ctx);
		}

		if (renderer.GetSettings().logical_size.has_value()) {
			if (MatchesFilter(filter, { "preset", "resolution preset" })) {
				changed |= DrawResolutionPreset(ctx);
			}

			if (MatchesFilter(filter, { "logical size", "resolution size" })) {
				auto logical_size{ renderer.GetLogicalSize() };

				if (DrawValue(
						ctx,
						"Logical Size",
						logical_size,
						FieldOptions{
							.speed = 1.0f,
							.min = 1.0,
							.max = 4096.0,
							.format = "%d",
							.flags = ImGuiSliderFlags_AlwaysClamp,
						}
					)) {
					renderer.SetLogicalSize(logical_size);
					changed = true;
				}
			}

			if (MatchesFilter(filter, { "scaling mode", "scaling" })) {
				auto scaling_mode{ renderer.GetSettings().scaling_mode };

				if (DrawValue(ctx, "Scaling Mode", scaling_mode)) {
					renderer.SetScalingMode(scaling_mode);
					changed = true;
				}
			}
		} else if (MatchesFilter(filter, { "window size", "current window size" })) {
			auto window_size{ renderer.GetDisplayViewport().size };

			ImGui::BeginDisabled();
			DrawValue(ctx, "Window Size", window_size);
			ImGui::EndDisabled();
		}
	}

	const bool show_window{
		MatchesFilter(
			filter,
			{
				"default window size",
				"resizable",
				"start maximized",
				"window background",
			}
		)
	};

	if (show_window) {
		DrawSectionTitle("Window");

		auto window_settings{ window.GetSettings() };
		bool window_changed{ false };

		if (MatchesFilter(filter, { "default window size", "window size" })) {
			DrawPropertyRow("Default Window Size", [&]() {
				ImGui::Text(
					"%d x %d",
					window_settings.size.x,
					window_settings.size.y
				);
				return false;
			});
		}

		if (MatchesFilter(filter, { "resizable", "resize window" })) {
			window_changed |= DrawValue(ctx, "Resizable", window_settings.resizable);
		}

		if (MatchesFilter(filter, { "start maximized", "maximized" })) {
			window_changed |= DrawValue(ctx, "Start Maximized", window_settings.maximized);
		}

		if (MatchesFilter(filter, { "window background", "background" })) {
			window_changed |= DrawValue(
				ctx,
				"Window Background",
				window_settings.background_color
			);
		}

		if (window_changed) {
			window.SetSettings(window_settings);
			changed = true;
		}
	}

	if (MatchesFilter(filter, { "renderer background", "presentation background" })) {
		DrawSectionTitle("Presentation");

		auto background{ renderer.GetSettings().background_color };
		if (DrawValue(ctx, "Renderer Background", background)) {
			renderer.SetBackgroundColor(background);
			changed = true;
		}
	}

	if (changed) {
		ctx.editor.MarkProjectDirty();
	}

	const ProjectDisplaySettingsState after{
		.renderer = renderer.GetSettings(),
		.window = window.GetSettings(),
	};

	TrackSettingsChange(
		ctx,
		kProjectDisplaySettingsUndoKey,
		"Change Project Display Settings",
		changed,
		before,
		after,
		[editor = &ctx.editor](const ProjectDisplaySettingsState& state) {
			editor->GetRenderer().SetSettings(state.renderer);
			editor->GetWindow().SetSettings(state.window);
			editor->MarkProjectDirty();
		}
	);

	return changed;
}

bool DrawProjectRenderingSettings(
	EditorContext& ctx,
	std::string_view filter
) {
	auto& renderer{ ctx.editor.GetRenderer() };
	const RendererSettings before{ renderer.GetSettings() };
	auto settings{ before };
	bool changed{ false };

	DrawSectionTitle("Renderer Output");

	if (MatchesFilter(filter, { "tone mapping", "tone map", "hdr" })) {
		changed |= DrawValue(ctx, "Tone Mapping", settings.tone_mapping.op);
	}

	if ((settings.tone_mapping.op == ToneMappingOperator::Exposure ||
		 settings.tone_mapping.op == ToneMappingOperator::ACES) &&
		MatchesFilter(filter, { "exposure", "brightness", "tone mapping" })) {
		changed |= DrawValue(
			ctx,
			"Exposure",
			settings.tone_mapping.exposure,
			FieldOptions{
				.speed = 0.05f,
				.min = 0.0,
				.max = 20.0,
				.format = "%.2f",
				.flags = ImGuiSliderFlags_AlwaysClamp,
			}
		);
	}

	if (MatchesFilter(filter, { "gamma", "color correction", "srgb" })) {
		changed |= DrawValue(
			ctx,
			"Gamma",
			settings.gamma,
			FieldOptions{
				.speed = 0.05f,
				.min = 0.01f,
				.max = 5.0,
				.format = "%.2f",
				.flags = ImGuiSliderFlags_AlwaysClamp,
			}
		);
	}

	if (changed) {
		renderer.SetSettings(settings);
		ctx.editor.MarkProjectDirty();
	}

	TrackSettingsChange(
		ctx,
		kProjectRenderingSettingsUndoKey,
		"Change Project Rendering Settings",
		changed,
		before,
		renderer.GetSettings(),
		[editor = &ctx.editor](const RendererSettings& value) {
			editor->GetRenderer().SetSettings(value);
			editor->MarkProjectDirty();
		}
	);

	return changed;
}

bool DrawEditorGeneralSettings(
	EditorContext& ctx,
	std::string_view filter
) {
	DrawSectionTitle("General");

	const EditorSettings before{ ctx.editor.GetSettings() };
	bool changed{ false };

	if (MatchesFilter(filter, { "entity picking", "picking", "selection" })) {
		auto entity_picking{ ctx.editor.GetSettings().entity_picking };

		if (ImGui::Checkbox("Entity Picking", &entity_picking)) {
			ctx.editor.SetEntityPickingMode(entity_picking);
			changed = true;
		}
	}

	if (MatchesFilter(filter, { "render only selected scene", "selected scene" })) {
		auto render_only_selected_scene{
			ctx.editor.GetSettings().render_only_selected_scene
		};

		if (ImGui::Checkbox(
				"Render Only Selected Scene",
				&render_only_selected_scene
			)) {
			ctx.editor.SetRenderOnlySelectedScene(render_only_selected_scene);
			changed = true;
		}

		ImGui::TextDisabled(
			"Excludes unselected scenes from the editor presentation."
		);
	}

	if (MatchesFilter(filter, { "viewport aspect ratio", "aspect ratio", "logical size" })) {
		auto settings{ ctx.editor.GetSettings() };

		if (ImGui::Checkbox(
				"Lock Viewport Aspect Ratio",
				&settings.viewport_aspect_ratio_locked
			)) {
			ctx.editor.SetEditorSettings(settings);
			changed = true;
		}

		ImGui::TextDisabled(
			"Keeps the presentation area at the logical-size aspect ratio."
		);
	}

	if (MatchesFilter(filter, { "local gizmo orientation", "gizmo", "transform orientation" })) {
		auto gizmo_uses_local_orientation{
			ctx.editor.GetSettings().gizmo_uses_local_orientation
		};

		if (ImGui::Checkbox(
				"Local Gizmo Orientation",
				&gizmo_uses_local_orientation
			)) {
			ctx.editor.SetGizmoUsesLocalOrientation(gizmo_uses_local_orientation);
			changed = true;
		}

		ImGui::TextDisabled(
			"Local orientation rotates the translate and scale axes with the entity."
		);
	}

	if (MatchesFilter(filter, { "show read only data", "read only inspector", "inspector data" })) {
		auto settings{ ctx.editor.GetSettings() };
		if (ImGui::Checkbox(
				"Show Read-Only Data",
				&settings.show_read_only_inspector_data
			)) {
			ctx.editor.SetEditorSettings(settings);
			changed = true;
		}

		ImGui::TextDisabled(
			"Shows read-only components and reflected read-only component members."
		);
	}

	if (MatchesFilter(filter, { "content browser", "assets per row", "asset grid" })) {
		auto settings{ ctx.editor.GetSettings() };

		if (ImGui::DragInt(
				"Content Browser Items Per Row",
				&settings.content_browser_items_per_row,
				1.0f,
				1,
				16,
				"%d",
				ImGuiSliderFlags_AlwaysClamp
			)) {
			ctx.editor.SetEditorSettings(settings);
			changed = true;
		}

		settings = ctx.editor.GetSettings();
		if (ImGui::Checkbox(
				"Search All Asset Folders",
				&settings.content_browser_search_entire_tree
			)) {
			ctx.editor.SetEditorSettings(settings);
			changed = true;
		}

		ImGui::TextDisabled(
			"The Content Browser counter can also be changed by hovering and scrolling."
		);
	}

	TrackSettingsChange(
		ctx,
		kEditorGeneralSettingsUndoKey,
		"Change Editor Settings",
		changed,
		before,
		ctx.editor.GetSettings(),
		[editor = &ctx.editor](const EditorSettings& settings) {
			editor->SetEditorSettings(settings);
		}
	);

	return changed;
}

bool DrawDebugInteractionSettings(
	EditorContext& ctx,
	std::string_view filter
) {
	DrawSectionTitle("Interaction");

	const auto before{ ctx.editor.GetDebugSystem().settings.interaction };
	auto settings{ before };
	bool changed{ false };

	if (MatchesFilter(filter, { "draw enabled", "enabled", "interactions" })) {
		changed |= DrawValue(ctx, "Draw Enabled", settings.draw_enabled);
	}

	if (MatchesFilter(filter, { "draw color", "color", "interactions" })) {
		changed |= DrawValue(ctx, "Draw Color", settings.draw_color);
	}

	if (MatchesFilter(filter, { "draw line width", "line width", "interactions" })) {
		changed |= DrawValue(
			ctx,
			"Draw Line Width",
			settings.draw_line_width,
			FieldOptions{
				.speed = 0.1f,
				.min = kMinLineWidth,
				.max = 100.0,
				.format = "%.2f",
				.flags = ImGuiSliderFlags_AlwaysClamp,
			}
		);
	}

	if (changed) {
		ctx.editor.GetDebugSystem().settings.interaction = settings;
		ctx.editor.MarkProjectDirty();
	}

	TrackSettingsChange(
		ctx,
		kDebugInteractionSettingsUndoKey,
		"Change Debug Interaction Settings",
		changed,
		before,
		ctx.editor.GetDebugSystem().settings.interaction,
		[editor = &ctx.editor](const auto& value) {
			editor->GetDebugSystem().settings.interaction = value;
			editor->MarkProjectDirty();
		}
	);

	return changed;
}

bool DrawDebugCollisionSettings(
	EditorContext& ctx,
	std::string_view filter
) {
	DrawSectionTitle("Collision");

	const auto before{ ctx.editor.GetDebugSystem().settings.collision };
	auto settings{ before };
	bool changed{ false };

	if (MatchesFilter(filter, { "draw enabled", "enabled", "collisions" })) {
		changed |= DrawValue(ctx, "Draw Enabled", settings.draw_enabled);
	}

	if (MatchesFilter(filter, { "draw color", "color", "collisions" })) {
		changed |= DrawValue(ctx, "Draw Color", settings.draw_color);
	}

	if (MatchesFilter(filter, { "draw fill style", "fill style", "collisions" })) {
		changed |= DrawValue(ctx, "Draw Fill Style", settings.draw_fill_style);
	}

	if (MatchesFilter(filter, { "draw ccd", "ccd", "continuous collision detection" })) {
		changed |= DrawValue(ctx, "Draw CCD", settings.draw_ccd);
	}

	if (changed) {
		ctx.editor.GetDebugSystem().settings.collision = settings;
		ctx.editor.MarkProjectDirty();
	}

	TrackSettingsChange(
		ctx,
		kDebugCollisionSettingsUndoKey,
		"Change Debug Collision Settings",
		changed,
		before,
		ctx.editor.GetDebugSystem().settings.collision,
		[editor = &ctx.editor](const auto& value) {
			editor->GetDebugSystem().settings.collision = value;
			editor->MarkProjectDirty();
		}
	);

	return changed;
}

bool DrawDebugTextSettings(
	EditorContext& ctx,
	std::string_view filter
) {
	DrawSectionTitle("Text");

	const auto before{ ctx.editor.GetDebugSystem().settings.text };
	auto settings{ before };
	bool changed{ false };

	if (MatchesFilter(filter, { "draw enabled", "enabled", "text boxes" })) {
		changed |= DrawValue(ctx, "Draw Enabled", settings.draw_enabled);
	}

	if (MatchesFilter(filter, { "draw color", "color", "text boxes" })) {
		changed |= DrawValue(ctx, "Draw Color", settings.draw_color);
	}

	if (MatchesFilter(filter, { "draw line width", "line width", "text boxes" })) {
		changed |= DrawValue(
			ctx,
			"Draw Line Width",
			settings.draw_line_width,
			FieldOptions{
				.speed = 0.1f,
				.min = kMinLineWidth,
				.max = 100.0,
				.format = "%.2f",
				.flags = ImGuiSliderFlags_AlwaysClamp,
			}
		);
	}

	if (MatchesFilter(filter, { "clip draw color", "clip color", "clipping" })) {
		changed |= DrawValue(ctx, "Clip Draw Color", settings.clip_draw_color);
	}

	if (changed) {
		ctx.editor.GetDebugSystem().settings.text = settings;
		ctx.editor.MarkProjectDirty();
	}

	TrackSettingsChange(
		ctx,
		kDebugTextSettingsUndoKey,
		"Change Debug Text Settings",
		changed,
		before,
		ctx.editor.GetDebugSystem().settings.text,
		[editor = &ctx.editor](const auto& value) {
			editor->GetDebugSystem().settings.text = value;
			editor->MarkProjectDirty();
		}
	);

	return changed;
}

bool DrawDebugVisibilitySettings(
	EditorContext& ctx,
	std::string_view filter
) {
	DrawSectionTitle("Visibility");

	const auto before{ ctx.editor.GetDebugSystem().settings.light };
	auto settings{ before };
	bool changed{ false };

	if (MatchesFilter(filter, { "draw enabled", "enabled", "visibility polygons" })) {
		changed |= DrawValue(ctx, "Draw Enabled", settings.draw_enabled);
	}

	if (MatchesFilter(filter, { "draw interiors", "interiors" })) {
		changed |= DrawValue(ctx, "Draw Interiors", settings.draw_interiors);
	}

	if (MatchesFilter(filter, { "polygon color", "visibility polygon color" })) {
		changed |= DrawValue(ctx, "Polygon Color", settings.polygon_color);
	}

	if (MatchesFilter(filter, { "masks inside color", "mask color" })) {
		changed |= DrawValue(ctx, "Masks Inside Color", settings.masks_inside_color);
	}

	if (MatchesFilter(filter, { "does not mask inside color", "non mask color" })) {
		changed |= DrawValue(
			ctx,
			"Does Not Mask Inside Color",
			settings.does_not_mask_inside_color
		);
	}

	if (MatchesFilter(filter, { "draw fill style", "fill style" })) {
		changed |= DrawValue(ctx, "Draw Fill Style", settings.draw_fill_style);
	}

	if (changed) {
		ctx.editor.GetDebugSystem().settings.light = settings;
		ctx.editor.MarkProjectDirty();
	}

	TrackSettingsChange(
		ctx,
		kDebugVisibilitySettingsUndoKey,
		"Change Debug Visibility Settings",
		changed,
		before,
		ctx.editor.GetDebugSystem().settings.light,
		[editor = &ctx.editor](const auto& value) {
			editor->GetDebugSystem().settings.light = value;
			editor->MarkProjectDirty();
		}
	);

	return changed;
}

void DrawPageNavigationItem(
	const char* label,
	SettingsPage page,
	SettingsPage& selected_page,
	std::string_view filter
) {
	if (!PageMatchesFilter(page, filter)) {
		return;
	}

	ImGui::PushID(static_cast<int>(page));

	if (ImGui::Selectable(label, selected_page == page)) {
		selected_page = page;
	}

	ImGui::PopID();
}

void DrawSettingsNavigation(
	SettingsPage& selected_page,
	std::string_view filter
) {
	const bool project_visible{
		PageMatchesFilter(SettingsPage::ProjectDisplay, filter) ||
		PageMatchesFilter(SettingsPage::ProjectRendering, filter)
	};

	const bool editor_visible{
		PageMatchesFilter(SettingsPage::EditorGeneral, filter)
	};

	const bool debug_visible{
		PageMatchesFilter(SettingsPage::DebugInteraction, filter) ||
		PageMatchesFilter(SettingsPage::DebugCollision, filter) ||
		PageMatchesFilter(SettingsPage::DebugText, filter) ||
		PageMatchesFilter(SettingsPage::DebugVisibility, filter)
	};

	if (project_visible) {
		if (!filter.empty()) {
			ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		}

		if (ImGui::TreeNodeEx(
				"Project",
				ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth
			)) {
			DrawPageNavigationItem(
				"Display",
				SettingsPage::ProjectDisplay,
				selected_page,
				filter
			);
			DrawPageNavigationItem(
				"Rendering",
				SettingsPage::ProjectRendering,
				selected_page,
				filter
			);
			ImGui::TreePop();
		}
	}

	if (editor_visible) {
		if (!filter.empty()) {
			ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		}

		if (ImGui::TreeNodeEx(
				"Editor",
				ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth
			)) {
			DrawPageNavigationItem(
				"General",
				SettingsPage::EditorGeneral,
				selected_page,
				filter
			);
			ImGui::TreePop();
		}
	}

	if (debug_visible) {
		if (!filter.empty()) {
			ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		}

		if (ImGui::TreeNodeEx(
				"Debug",
				ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth
			)) {
			DrawPageNavigationItem(
				"Interactions",
				SettingsPage::DebugInteraction,
				selected_page,
				filter
			);
			DrawPageNavigationItem(
				"Collisions",
				SettingsPage::DebugCollision,
				selected_page,
				filter
			);
			DrawPageNavigationItem(
				"Text Boxes",
				SettingsPage::DebugText,
				selected_page,
				filter
			);
			DrawPageNavigationItem(
				"Visibility Polygons",
				SettingsPage::DebugVisibility,
				selected_page,
				filter
			);
			ImGui::TreePop();
		}
	}
}

[[nodiscard]] std::optional<SettingsPage> FirstMatchingPage(
	std::string_view filter
) {
	const auto it{ std::ranges::find_if(kSettingsPages, [filter](SettingsPage page) {
		return PageMatchesFilter(page, filter);
	}) };

	return it == kSettingsPages.end()
		? std::nullopt
		: std::optional<SettingsPage>{ *it };
}

bool DrawSelectedSettingsPage(
	EditorContext& ctx,
	SettingsPage page,
	std::string_view filter
) {
	// ImGui::TextUnformatted(PageTitle(page));
	// ImGui::TextDisabled("%s", PageDescription(page));
	// ImGui::Separator();

	AutoLabelWidthScope label_width{ "SettingsWindowContent" };

	switch (page) {
		case SettingsPage::ProjectDisplay:
			return DrawProjectDisplaySettings(ctx, filter);

		case SettingsPage::ProjectRendering:
			return DrawProjectRenderingSettings(ctx, filter);

		case SettingsPage::EditorGeneral:
			return DrawEditorGeneralSettings(ctx, filter);

		case SettingsPage::DebugInteraction:
			return DrawDebugInteractionSettings(ctx, filter);

		case SettingsPage::DebugCollision:
			return DrawDebugCollisionSettings(ctx, filter);

		case SettingsPage::DebugText:
			return DrawDebugTextSettings(ctx, filter);

		case SettingsPage::DebugVisibility:
			return DrawDebugVisibilitySettings(ctx, filter);
	}

	return false;
}

void DrawHistorySectionTitle(const char* title) {
	ImGui::TextDisabled("%s", title);
	ImGui::Separator();
}

} // namespace

void SettingsWindow::Open(SettingsPage page) {
	selected_page_ = page;
	search_.fill('\0');
	open_ = true;
	focus_requested_ = true;
	recenter_requested_ = true;
}

void SettingsWindow::OnRender(EditorContext& ctx) {
	if (!open_) {
		return;
	}

	if (recenter_requested_) {
		auto* viewport{ ImGui::GetMainViewport() };
		const ImVec2 size{
			viewport->WorkSize.x * 0.60f,
			viewport->WorkSize.y * 0.70f
		};
		const ImVec2 center{
			viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
			viewport->WorkPos.y + viewport->WorkSize.y * 0.5f
		};

		ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
		ImGui::SetNextWindowPos(
			center,
			ImGuiCond_Always,
			ImVec2{ 0.5f, 0.5f }
		);
		ImGui::SetNextWindowSize(
			size,
			ImGuiCond_Always
		);
		recenter_requested_ = false;
	}

	if (focus_requested_) {
		ImGui::SetNextWindowFocus();
		focus_requested_ = false;
	}

	if (!ImGui::Begin(
			"Settings",
			&open_
		)) {
		if (undo_interaction_pending_ && !ImGui::IsAnyItemActive()) {
			ctx.undo.CommitActiveEdit();
			undo_interaction_pending_ = false;
		}
		ImGui::End();
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
		ImGui::IsKeyPressed(ImGuiKey_Escape)) {
		open_ = false;
	}

	// ImGui::TextUnformatted("Settings");
	// ImGui::Separator();

	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint(
		"##SettingsSearch",
		"Search settings",
		search_.data(),
		search_.size()
	);

	ImGui::Spacing();

	const std::string_view filter{ search_.data() };
	const auto first_matching_page{ FirstMatchingPage(filter) };

	if (!PageMatchesFilter(selected_page_, filter) && first_matching_page) {
		selected_page_ = *first_matching_page;
	}

	constexpr float navigation_width{ 230.0f };

	if (ImGui::BeginChild(
			"SettingsNavigation",
			ImVec2{ navigation_width, 0.0f },
			ImGuiChildFlags_Borders
		)) {
		DrawSettingsNavigation(selected_page_, filter);
	}
	ImGui::EndChild();

	ImGui::SameLine();

	bool settings_changed{ false };

	if (ImGui::BeginChild(
			"SettingsContent",
			ImVec2{ 0.0f, 0.0f },
			ImGuiChildFlags_Borders,
			ImGuiWindowFlags_AlwaysVerticalScrollbar
		)) {
		if (!first_matching_page) {
			ImGui::TextDisabled("No settings match \"%s\".", search_.data());
		} else {
			settings_changed =
				DrawSelectedSettingsPage(ctx, selected_page_, filter);

			if (settings_changed) {
				ctx.local.state.is_dirty = true;
			}
		}
	}
	ImGui::EndChild();

	if (settings_changed && ctx.undo.HasActiveEdit()) {
		undo_interaction_pending_ = true;
	}

	if (undo_interaction_pending_ && !ImGui::IsAnyItemActive()) {
		ctx.undo.CommitActiveEdit();
		undo_interaction_pending_ = false;
	}

	ImGui::End();
}

void UndoHistoryWindow::Open() {
	open_ = true;
	focus_requested_ = true;
}

void UndoHistoryWindow::OnRender(
	EditorContext& ctx,
	UndoStack& undo_stack
) {
	if (!open_) {
		return;
	}

	ImGui::SetNextWindowSize(ImVec2{ 390.0f, 420.0f }, ImGuiCond_FirstUseEver);

	if (focus_requested_) {
		ImGui::SetNextWindowFocus();
		focus_requested_ = false;
	}

	if (!ImGui::Begin(
			"Undo History",
			&open_,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking
		)) {
		ImGui::End();
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
		ImGui::IsKeyPressed(ImGuiKey_Escape)) {
		open_ = false;
	}

	ImGui::BeginDisabled(!undo_stack.CanUndo());
	if (ImGui::Button("Undo")) {
		ctx.local.position_picker.Cancel();
		undo_stack.Undo();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();

	ImGui::BeginDisabled(!undo_stack.CanRedo());
	if (ImGui::Button("Redo")) {
		ctx.local.position_picker.Cancel();
		undo_stack.Redo();
	}
	ImGui::EndDisabled();

	const auto history{ undo_stack.History() };
	const std::size_t cursor{ undo_stack.Cursor() };

	ImGui::SameLine();
	ImGui::TextDisabled("%zu / %zu applied", cursor, history.size());
	ImGui::Separator();

	if (!undo_stack.IsUndoRedoEnabled()) {
		ImGui::TextDisabled("Undo and redo are disabled while runtime editing is active.");
		ImGui::Separator();
	}

	if (history.empty()) {
		ImGui::TextDisabled("No undo or redo actions.");
		ImGui::End();
		return;
	}

	if (ImGui::BeginChild("UndoHistoryEntries", ImVec2{ 0.0f, 0.0f })) {
		DrawHistorySectionTitle("Redo");

		if (cursor >= history.size()) {
			ImGui::TextDisabled("No actions available to redo.");
		} else {
			for (std::size_t index{ cursor }; index < history.size(); ++index) {
				ImGui::PushID(static_cast<int>(index));
				ImGui::BulletText("%s", history[index].label.c_str());
				ImGui::PopID();
			}
		}

		ImGui::Spacing();
		DrawHistorySectionTitle("Undo");

		if (cursor == 0) {
			ImGui::TextDisabled("No actions available to undo.");
		} else {
			for (std::size_t index{ cursor }; index > 0; --index) {
				const auto& entry{ history[index - 1] };
				ImGui::PushID(static_cast<int>(index - 1));
				ImGui::BulletText("%s", entry.label.c_str());
				ImGui::PopID();
			}
		}
	}
	ImGui::EndChild();

	ImGui::End();
}

} // namespace ptgn::editor
