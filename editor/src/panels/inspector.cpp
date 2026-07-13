#include "panels/inspector.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <magic_enum/magic_enum.hpp>
#include <string>
#include <string_view>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "panels/component_editor_registry.h"
#include "panels/inspector_fields.h"
#include "panels/scene_hierarchy.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/offsets.h"
#include "runtime/animation/tween.h"
#include "runtime/ecs/component_registration.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/bloom.h"
#include "runtime/graphics/fx/blur.h"
#include "runtime/graphics/fx/gaussian_blur.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/graphics.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"

namespace ptgn::editor::inspector {

namespace {

template <std::size_t N>
struct FixedString {
	char value[N];

	constexpr FixedString(const char (&text)[N]) {
		std::copy_n(text, N, value);
	}

	[[nodiscard]] constexpr std::string_view View() const {
		return { value, N - 1 };
	}
};

template <FixedString Reason, typename... T>
std::optional<std::string_view> HasAnyComponent(Entity entity) {
	if (entity.HasAny<T...>()) {
		return Reason.View();
	}

	return std::nullopt;
}

bool DrawOptionalViewport(
	std::string_view label, std::optional<Viewport>& value, ViewportSpace viewport_space
) {
	ImGui::PushID(&value);

	bool enabled{ value.has_value() };
	bool changed{ DrawPropertyRow(label, [&]() {
		return DrawDisabledIf(IsReadOnly(), [&]() {
			return ImGui::Checkbox("##enabled", &enabled);
		});
	}) };

	if (enabled != value.has_value()) {
		if (enabled) {
			auto& viewport{ value.emplace() };

			if (viewport_space == ViewportSpace::Normalized) {
				viewport.position = V2_float{ 0.0f, 0.0f };
				viewport.size	  = V2_float{ 1.0f, 1.0f };
			}
		} else {
			value.reset();
		}

		changed = true;
	}

	if (value.has_value()) {
		ImGui::Indent();

		if (viewport_space == ViewportSpace::Normalized) {
			FieldOptions options{
				.speed	= 0.01f,
				.min	= 0.0,
				.max	= 1.0,
				.format = "%.3f",
				.flags	= ImGuiSliderFlags_AlwaysClamp,
			};

			changed |= DrawValue("Position", value->position, options);
			changed |= DrawValue("Size", value->size, options);

			if (value->size.x > 1.0f || value->size.y > 1.0f) {
				value->size.x = std::min(1.0f, value->size.x);
				value->size.y = std::min(1.0f, value->size.y);
				changed		  = true;
			}
		} else {
			changed |= DrawValue(
				"Position", value->position,
				FieldOptions{
					.speed	= 1.0f,
					.format = "%.0f",
				}
			);

			changed |= DrawValue(
				"Size", value->size,
				FieldOptions{
					.speed	= 1.0f,
					.min	= 1.0f,
					.max	= 4096.0f,
					.format = "%.0f",
				}
			);

			if (value->size.x < 1.0f || value->size.y < 1.0f) {
				value->size.x = std::max(1.0f, value->size.x);
				value->size.y = std::max(1.0f, value->size.y);
				changed		  = true;
			}
		}

		ImGui::Unindent();
	}

	ImGui::PopID();

	return changed;
}

} // namespace

// TODO: Add Material.

template <>
struct Contents<::ptgn::impl::CameraData> {
	static bool Draw(::ptgn::impl::CameraData& camera) {
		bool changed{ false };

		// Draw this first because it controls the raw viewport's defaults and bounds.
		changed |= DrawValue("Viewport Space", camera.viewport_space);

		changed |= DrawOptionalViewport("Raw Viewport", camera.raw_viewport, camera.viewport_space);

		changed |= DrawValue("Pixel Rounding", camera.pixel_rounding);
		changed |= DrawValue("Bounding Box", camera.bounding_box);

		(void)DrawReadOnlyValue("View Projection", camera.view_projection);

		return changed;
	}
};

template <>
struct Contents<::ptgn::impl::IDrawable> {
	static bool Draw(::ptgn::impl::IDrawable& drawable) {
		auto* current_info{ ::ptgn::impl::IDrawable::FindInfo(drawable.hash) };

		std::string preview{ current_info ? std::string{ current_info->GetDisplayName() }
										  : "<Unregistered Drawable>" };

		bool changed{ DrawPropertyRow("Drawable", [&]() {
			bool local_changed{ false };

			if (ImGui::BeginCombo("##value", preview.c_str())) {
				for (const auto& info : ::ptgn::impl::IDrawable::data()) {
					bool selected{ drawable.hash == info.hash };
					std::string display_name{ info.GetDisplayName() };

					if (ImGui::Selectable(display_name.c_str(), selected)) {
						drawable.hash = info.hash;
						local_changed = true;
					}

					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			return local_changed;
		}) };

		if (!current_info && drawable.hash != 0) {
			ImGui::TextDisabled("Stored hash: %zu", drawable.hash);
		}

		return changed;
	}
};

// Only types whose default reflected layout is not ideal need a specialization.
template <>
struct Contents<Hollow> {
	static bool Draw(Hollow& hollow) {
		return DrawValue(
			"Line Width", hollow.line_width,
			FieldOptions{
				.speed	= 0.1f,
				.min	= kMinLineWidth,
				.max	= 1000.0f,
				.format = "%.2f",
				.flags	= ImGuiSliderFlags_AlwaysClamp,
			}
		);
	}
};

template <>
struct Contents<Rect> {
	static bool Draw(Rect& rect) {
		bool changed{ false };

		auto size{ rect.max - rect.min };

		if (DrawValue(
				"Size", size,
				FieldOptions{
					.speed	= 0.1f,
					.min	= 0.0,
					.max	= 0.0,
					.format = "%.3f",
				}
			)) {
			size.x = std::max(size.x, 0.0f);
			size.y = std::max(size.y, 0.0f);

			auto center{ rect.GetCenter() };
			auto half_size{ size * 0.5f };

			rect.min = center - half_size;
			rect.max = center + half_size;

			changed = true;
		}

		changed |= DrawValue("Min", rect.min);
		changed |= DrawValue("Max", rect.max);

		return changed;
	}
};

template <>
struct Contents<TextRun> {
	static bool Draw(TextRun& run) {
		bool changed{ false };
		changed |= DrawValue("Text", run.text, FieldOptions{ .multiline = true });
		changed |= DrawValue("Font", run.font);
		changed |= DrawValue("Style", run.style);
		return changed;
	}
};

template <>
struct Contents<StyledText> {
	static bool Draw(StyledText& text) {
		return DrawVectorEditor(
			text.runs, VectorOptions{
						   .item_name	 = "Text Run",
						   .default_open = true,
						   .reorderable	 = true,
					   }
		);
	}
};

template <>
struct Contents<Polygon> {
	static bool Draw(Polygon& polygon) {
		return DrawVectorEditor(
			polygon.vertices, VectorOptions{
								  .item_name	= "Vertex",
								  .default_open = true,
								  .reorderable	= true,
							  }
		);
	}
};

template <>
struct Contents<ButtonBorderVisuals> {
	static bool Draw(ButtonBorderVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(visuals.states);
	}
};

template <>
struct Contents<ButtonBackgroundVisuals> {
	static bool Draw(ButtonBackgroundVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(visuals.states);
	}
};

template <>
struct Contents<ButtonTextVisuals> {
	static bool Draw(ButtonTextVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(visuals.states);
	}
};

template <>
struct Contents<ButtonSpriteVisuals> {
	static bool Draw(ButtonSpriteVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(visuals.states);
	}
};

template <>
struct Contents<ButtonSounds> {
	static bool Draw(ButtonSounds& sounds) {
		bool changed{ false };
		changed |= DrawEnumArrayEditor<ButtonVisualState>("Sounds", sounds.states);
		changed |= DrawValue("Exclusive Audio", sounds.exclusive);
		return changed;
	}
};

} // namespace ptgn::editor::inspector

namespace ptgn::editor {

namespace {

using namespace inspector;

template <typename T>
bool DrawRegisteredContents(Entity entity) {
	return DrawComponentContents(entity.Get<T>());
}

void MarkTextLayoutDirty(Entity entity) {
	if (entity.Has<TextLayout>()) {
		entity.Get<TextLayout>().dirty = true;
	}
}

void MarkParentButtonDirty(Entity entity, ::ptgn::impl::ButtonDirty dirty) {
	Entity parent{ GetParent(entity) };

	if (!parent || !parent.Has<::ptgn::impl::ButtonData>()) {
		return;
	}

	parent.Get<::ptgn::impl::ButtonData>().dirty |= dirty;
}

void MarkButtonTextDirty(Entity entity) {
	MarkTextLayoutDirty(entity);
	MarkParentButtonDirty(entity, ::ptgn::impl::ButtonDirty::Text);
}

void MarkButtonBorderDirty(Entity entity) {
	MarkParentButtonDirty(entity, ::ptgn::impl::ButtonDirty::Border);
}

void MarkButtonBackgroundDirty(Entity entity) {
	MarkParentButtonDirty(entity, ::ptgn::impl::ButtonDirty::Background);
}

void MarkButtonSpriteDirty(Entity entity) {
	MarkParentButtonDirty(entity, ::ptgn::impl::ButtonDirty::Sprite);
}

void DrawAddDrawableMenuItem(Entity entity, const auto& info) {
	auto name{ std::string{ info.GetDisplayName() } };

	if (ImGui::MenuItem(name.c_str())) {
		entity.Add<::ptgn::impl::IDrawable>(info.hash);
	}
}

void DrawAddDrawableMenu(Entity entity, std::string_view label) {
	auto menu_label{ std::string{ label } };

	if (!ImGui::BeginMenu(menu_label.c_str())) {
		return;
	}

	const auto& drawables{ ::ptgn::impl::IDrawable::data() };

	auto draw_menu_item = [entity](const ::ptgn::impl::IDrawable::Info& info) mutable {
		auto display_name{ std::string{ info.GetDisplayName() } };

		if (ImGui::MenuItem(display_name.c_str())) {
			entity.Add<::ptgn::impl::IDrawable>(info.hash);
		}
	};

	std::vector<std::string_view> groups;

	for (const auto& info : drawables) {
		if (!info.group.has_value() || info.group->empty()) {
			continue;
		}

		if (std::ranges::find(groups, info.group.value()) == groups.end()) {
			groups.push_back(info.group.value());
		}
	}

	std::ranges::sort(groups);

	for (auto group : groups) {
		auto group_label{ std::string{ group } };

		if (!ImGui::BeginMenu(group_label.c_str())) {
			continue;
		}

		for (const auto& info : drawables) {
			if (info.group.has_value() && info.group.value() == group) {
				draw_menu_item(info);
			}
		}

		ImGui::EndMenu();
	}

	bool has_grouped_drawables{ !groups.empty() };
	bool has_ungrouped_drawables{ std::ranges::any_of(drawables, [](const auto& info) {
		return !info.group.has_value() || info.group->empty();
	}) };

	if (has_grouped_drawables && has_ungrouped_drawables) {
		ImGui::Separator();
	}

	for (const auto& info : drawables) {
		if (!info.group.has_value() || info.group->empty()) {
			draw_menu_item(info);
		}
	}

	ImGui::EndMenu();
}

bool DrawScaleValue(Entity entity, V2_float& scale, const FieldOptions& options) {
	ImGui::PushID(entity.Get<UUID>());
	ImGui::PushID("Scale");

	ImGuiStorage* storage{ ImGui::GetStateStorage() };
	ImGuiID scale_lock_id{ ImGui::GetID("ScaleLocked") };
	bool scale_locked{ storage->GetBool(scale_lock_id, true) };

	bool changed{ DrawPropertyRow("Scale", [&]() {
		if (ImGui::Checkbox("##ScaleLocked", &scale_locked)) {
			storage->SetBool(scale_lock_id, scale_locked);
		}

		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(scale_locked ? "Unlock ratio" : "Lock ratio");
		}

		ImGui::SameLine();

		constexpr float kFieldSpacing{ 4.0f };

		float available_width{ ImGui::GetContentRegionAvail().x };
		float field_width{ (available_width - kFieldSpacing) * 0.5f };

		V2_float previous_scale{ scale };

		ImGui::SetNextItemWidth(field_width);
		bool x_changed{ ImGui::DragFloat(
			"##X", &scale.x, options.speed, static_cast<float>(options.min),
			static_cast<float>(options.max), options.format, options.flags
		) };

		ImGui::SameLine(0.0f, kFieldSpacing);

		ImGui::SetNextItemWidth(field_width);
		bool y_changed{ ImGui::DragFloat(
			"##Y", &scale.y, options.speed, static_cast<float>(options.min),
			static_cast<float>(options.max), options.format, options.flags
		) };

		bool value_changed{ x_changed || y_changed };

		if (!value_changed || !scale_locked) {
			return value_changed;
		}

		constexpr float kScaleEpsilon{ 0.000001f };

		if (x_changed && !y_changed) {
			if (std::abs(previous_scale.x) > kScaleEpsilon) {
				float factor{ scale.x / previous_scale.x };
				scale.y = previous_scale.y * factor;
			} else if (std::abs(previous_scale.y) <= kScaleEpsilon) {
				scale.y = scale.x;
			}
		} else if (y_changed && !x_changed) {
			if (std::abs(previous_scale.y) > kScaleEpsilon) {
				float factor{ scale.y / previous_scale.y };
				scale.x = previous_scale.x * factor;
			} else if (std::abs(previous_scale.x) <= kScaleEpsilon) {
				scale.x = scale.y;
			}
		}

		return true;
	}) };

	ImGui::PopID();
	ImGui::PopID();

	return changed;
}

void DrawTransformComponent(Entity entity) {
	auto& transform{ entity.TryAdd<Transform>() };
	auto& depth{ entity.TryAdd<Depth>() };

	ImGui::PushID(ComponentTypeId<Transform>());
	bool open{ ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen) };
	ImGui::PopID();

	if (!open) {
		ImGui::Spacing();
		return;
	}

	auto read_only_reason{ HasAnyComponent<
		"Controlled by Button Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals,
		ButtonSpriteVisuals, ButtonTextVisuals>(entity) };

	if (read_only_reason.has_value() && !read_only_reason->empty()) {
		ImGui::TextDisabled(
			"%.*s", static_cast<int>(read_only_reason->size()), read_only_reason->data()
		);
	}

	ImGui::Indent();

	ReadOnlyScope read_only_scope{ read_only_reason.has_value() };

	DrawValue(
		"Position", transform.position,
		FieldOptions{
			.speed	= 1.0f,
			.format = "%.0f",
		}
	);

	DrawValue(
		"Depth", depth.value,
		FieldOptions{
			.speed	= 0.05f,
			.min	= -1000.0,
			.max	= 1000.0,
			.format = "%.2f",
			.flags	= ImGuiSliderFlags_AlwaysClamp,
		}
	);

	DrawValue(
		"Rotation", transform.rotation,
		FieldOptions{
			.speed	= 1.0f,
			.min	= 0.0,
			.max	= 360.0,
			.format = "%.1f deg",
			.flags	= ImGuiSliderFlags_AlwaysClamp,
		}
	);

	if (DrawScaleValue(
			entity, transform.scale,
			FieldOptions{
				.speed	= 0.01f,
				.min	= -1000.0,
				.max	= 1000.0,
				.format = "%.2f",
				.flags	= ImGuiSliderFlags_AlwaysClamp,
			}
		)) {
		transform.ClampScale();
	}

	ImGui::Unindent();
}

} // namespace

PTGN_REGISTER_COMPONENT(
	::ptgn::impl::CameraData,
	{
		.draw_contents = &DrawRegisteredContents<::ptgn::impl::CameraData>,
	}
);

PTGN_REGISTER_COMPONENT(
	Rect,
	{
		.get_read_only_reason = &HasAnyComponent<
			"Controlled by Button Shape Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals>,
		.draw_contents = &DrawRegisteredContents<Rect>,
	}
);

PTGN_REGISTER_COMPONENT(
	Polygon, {
				 .draw_contents = &DrawRegisteredContents<Polygon>,
			 }
);

PTGN_REGISTER_COMPONENT(
	Circle,
	{ .get_read_only_reason = &HasAnyComponent<
		  "Controlled by Button Shape Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals> }
);

PTGN_REGISTER_COMPONENT(
	::ptgn::impl::IDrawable, { .label		  = "Drawable",
							   .draw_contents = &DrawRegisteredContents<::ptgn::impl::IDrawable>,
							   .draw_add_menu = &DrawAddDrawableMenu }
);

PTGN_REGISTER_COMPONENT(
	StyledText, {
					.on_changed = &MarkTextLayoutDirty,
					.get_read_only_reason =
						&HasAnyComponent<"Controlled by Button Text Visuals", ButtonTextVisuals>,
					.draw_contents = &DrawRegisteredContents<StyledText>,
				}
);

PTGN_REGISTER_COMPONENT(
	TextBox, {
				 .on_changed = &MarkTextLayoutDirty,
				 .get_read_only_reason =
					 &HasAnyComponent<"Controlled by Button Text Visuals", ButtonTextVisuals>,
				 .draw_contents = &DrawRegisteredContents<TextBox>,
			 }
);

PTGN_REGISTER_COMPONENT(
	Color,
	{ .get_read_only_reason = &HasAnyComponent<
		  "Controlled by Button Shape Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals> }
);

PTGN_REGISTER_COMPONENT(
	FillStyle,
	{ .get_read_only_reason = &HasAnyComponent<
		  "Controlled by Button Shape Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals>,
	  .draw_contents = &DrawRegisteredContents<FillStyle> }
);

PTGN_REGISTER_COMPONENT(
	TextureKey, { .get_read_only_reason =
					  &HasAnyComponent<"Controlled by Button Sprite Visuals", ButtonSpriteVisuals> }
);

PTGN_REGISTER_COMPONENT(
	Tint, { .get_read_only_reason =
				&HasAnyComponent<"Controlled by Button Sprite Visuals", ButtonSpriteVisuals> }
);

PTGN_REGISTER_COMPONENT(
	::ptgn::impl::AnimationData,
	{ .get_read_only_reason =
		  &HasAnyComponent<"Controlled by Button Sprite Visuals", ButtonSpriteVisuals> }
);

PTGN_REGISTER_COMPONENT(
	::ptgn::impl::TextureSize,
	{ .get_read_only_reason =
		  &HasAnyComponent<"Controlled by Button Sprite Visuals", ButtonSpriteVisuals> }
);

PTGN_REGISTER_COMPONENT(
	::ptgn::impl::ButtonAnimationPart,
	{ .get_read_only_reason =
		  &HasAnyComponent<"Controlled by Button Sprite Visuals", ButtonSpriteVisuals> }
);

PTGN_REGISTER_COMPONENT(
	Visible, { .get_read_only_reason = &HasAnyComponent<
				   "Controlled by Button Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals,
				   ButtonSpriteVisuals, ButtonTextVisuals> }
);

PTGN_REGISTER_COMPONENT(
	Origin, { .get_read_only_reason = &HasAnyComponent<
				  "Controlled by Button Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals,
				  ButtonSpriteVisuals, ButtonTextVisuals> }
);

PTGN_REGISTER_COMPONENT(
	ButtonBackgroundVisuals, {
								 .removable		  = false,
								 .addable		  = false,
								 .draw_after_tags = true,
								 .on_changed	  = &MarkButtonBackgroundDirty,
								 .draw_contents = &DrawRegisteredContents<ButtonBackgroundVisuals>,
							 }
);

PTGN_REGISTER_COMPONENT(
	ButtonBorderVisuals, {
							 .removable		  = false,
							 .addable		  = false,
							 .draw_after_tags = true,
							 .on_changed	  = &MarkButtonBorderDirty,
							 .draw_contents	  = &DrawRegisteredContents<ButtonBorderVisuals>,
						 }
);

PTGN_REGISTER_COMPONENT(
	ButtonSpriteVisuals, {
							 .removable		  = false,
							 .addable		  = false,
							 .draw_after_tags = true,
							 .on_changed	  = &MarkButtonSpriteDirty,
							 .draw_contents	  = &DrawRegisteredContents<ButtonSpriteVisuals>,
						 }
);

PTGN_REGISTER_COMPONENT(
	ButtonTextVisuals, {
						   .removable		= false,
						   .addable			= false,
						   .draw_after_tags = true,
						   .on_changed		= &MarkButtonTextDirty,
						   .draw_contents	= &DrawRegisteredContents<ButtonTextVisuals>,
					   }
);

PTGN_REGISTER_COMPONENT(
	ButtonSounds, {
					  .removable	   = false,
					  .addable		   = false,
					  .draw_after_tags = true,
					  .draw_contents   = &DrawRegisteredContents<ButtonSounds>,
				  }
);

void InspectorPanel::OnRender(EditorContext& ctx) {
	inspector::InspectorAssetManagerScope asset_manager_scope{ ctx.editor.GetAssetManager() };

	ImGui::Begin("Inspector");

	auto& scene_hierarchy{ ctx.editor.GetSceneHierarchyPanel() };
	auto selected_entity{ scene_hierarchy.GetSelectedEntity() };

	if (!selected_entity) {
		ImGui::End();
		return;
	}

	auto name{ std::string{ selected_entity.Get<Tag>() } };
	if (ImGui::InputText("Name", &name)) {
		selected_entity.Add<Tag>(name);
	}

	ImGui::Separator();

	DrawTransformComponent(selected_entity);
	ComponentEditorRegistry::DrawComponents(selected_entity);
	ComponentEditorRegistry::DrawTagComponents(selected_entity);
	ComponentEditorRegistry::DrawComponents(selected_entity, true);

	ImGui::Separator();

	if (ImGui::Button("Add Component", ImVec2{ -1.0f, 0.0f })) {
		ImGui::OpenPopup("AddComponentPopup");
	}

	if (ImGui::BeginPopup("AddComponentPopup")) {
		ComponentEditorRegistry::DrawAddComponentMenu(selected_entity);
		ImGui::EndPopup();
	}

	ImGui::End();
}

} // namespace ptgn::editor
