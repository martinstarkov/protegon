#include "panels/inspector.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <magic_enum/magic_enum.hpp>
#include <string>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "panels/inspector_fields.h"
#include "panels/scene_hierarchy.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/offsets.h"
#include "runtime/animation/tween.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/bloom.h"
#include "runtime/graphics/fx/blur.h"
#include "runtime/graphics/fx/gaussian_blur.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"

namespace ptgn::editor::inspector {

// TODO: Add Material.

template <typename... T>
struct ComponentTypes {};

// Adding a type here adds both its inspector section and its Add Component menu entry.
using DefaultInspectorComponents = ComponentTypes<
	impl::Tint, Color, Visible, Origin, Rect, Circle, FillStyle, impl::Interactive, StyledText,
	TextBox, Collider, RigidBody, TopDownMovement, impl::ParticleEmitterComponent, TextureKey,
	LightConfig, impl::ShadowCaster, impl::ButtonData, impl::ButtonAnimationPart,
	impl::AnimationData, impl::Offsets, impl::IgnoreParentOffset, impl::TweenData,
	impl::IgnoreParentTransform, impl::IgnoreParentPosition, impl::IgnoreParentRotation,
	impl::IgnoreParentScale, impl::IgnoreParentDepth, impl::EffectTag, impl::HDREffectTag, Bloom,
	Blur, GaussianBlur>;

template <>
struct Contents<impl::IDrawable> {
	static bool Draw(impl::IDrawable& drawable) {
		auto* current_info{ impl::IDrawable::FindInfo(drawable.hash) };

		std::string preview{ current_info ? current_info->name : "<Missing Drawable>" };

		bool changed{ DrawPropertyRow("Drawable", [&]() {
			bool local_changed{ false };

			if (ImGui::BeginCombo("##value", preview.c_str())) {
				for (const auto& info : impl::IDrawable::data()) {
					bool selected{ drawable.hash == info.hash };
					std::string display_name{ info.name };

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
				.max	= 1000.0,
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
constexpr int type_id_value{ 0 };

struct ComponentOptions {
	std::optional<std::string> label_override;
	bool removable{ true };
	bool default_open{ false };
};

template <typename T>
struct ComponentChangeHandler {
	static void Apply(Entity) {}
};

void MarkTextLayoutDirty(Entity entity) {
	entity.TryAdd<TextLayout>().dirty = true;
}

template <>
struct ComponentChangeHandler<StyledText> {
	static void Apply(Entity entity) {
		MarkTextLayoutDirty(entity);
	}
};

template <>
struct ComponentChangeHandler<TextBox> {
	static void Apply(Entity entity) {
		MarkTextLayoutDirty(entity);
	}
};

template <>
struct ComponentChangeHandler<ButtonTextVisuals> {
	static void Apply(Entity entity) {
		MarkTextLayoutDirty(entity);

		Entity parent{ GetParent(entity) };

		if (!parent || !parent.Has<impl::ButtonData>()) {
			return;
		}

		parent.Get<impl::ButtonData>().dirty |= impl::ButtonDirty::Text;
	}
};

template <>
struct ComponentChangeHandler<ButtonBorderVisuals> {
	static void Apply(Entity entity) {
		Entity parent{ GetParent(entity) };

		if (!parent || !parent.Has<impl::ButtonData>()) {
			return;
		}

		parent.Get<impl::ButtonData>().dirty |= impl::ButtonDirty::Border;
	}
};

template <>
struct ComponentChangeHandler<ButtonBackgroundVisuals> {
	static void Apply(Entity entity) {
		Entity parent{ GetParent(entity) };

		if (!parent || !parent.Has<impl::ButtonData>()) {
			return;
		}

		parent.Get<impl::ButtonData>().dirty |= impl::ButtonDirty::Background;
	}
};

template <>
struct ComponentChangeHandler<ButtonSpriteVisuals> {
	static void Apply(Entity entity) {
		Entity parent{ GetParent(entity) };

		if (!parent || !parent.Has<impl::ButtonData>()) {
			return;
		}

		parent.Get<impl::ButtonData>().dirty |= impl::ButtonDirty::Sprite;
	}
};

template <typename T>
bool DrawComponentHeader(Entity entity, ComponentOptions options = {}) {
	ImGui::PushID(&type_id_value<T>);

	auto name{ options.label_override.has_value() ? options.label_override.value()
												  : TypeLabel<T>() };
	auto flags{ options.default_open ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None };
	bool open{ ImGui::CollapsingHeader(name.c_str(), flags) };

	if (options.removable && ImGui::BeginPopupContextItem("ComponentContextMenu")) {
		if (ImGui::MenuItem("Remove Component")) {
			entity.Remove<T>();
			open = false;
		}
		ImGui::EndPopup();
	}

	ImGui::PopID();
	return open && entity.Has<T>();
}

template <typename T>
void DrawComponent(Entity entity, ComponentOptions options = {}) {
	if (!entity.Has<T>()) {
		return;
	}

	bool open{ DrawComponentHeader<T>(entity, options) };

	if (!open) {
		ImGui::Spacing();
		return;
	}

	ImGui::Indent();

	if (DrawComponentContents(entity.Get<T>())) {
		ComponentChangeHandler<T>::Apply(entity);
	}

	ImGui::Unindent();
}

void DrawTransformComponent(Entity entity) {
	auto& transform{ entity.TryAdd<Transform>() };
	auto& depth{ entity.TryAdd<Depth>() };

	bool open{ DrawComponentHeader<Transform>(
		entity, ComponentOptions{
					.removable	  = false,
					.default_open = true,
				}
	) };

	if (!open) {
		ImGui::Spacing();
		return;
	}

	ImGui::Indent();

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

	if (DrawValue(
			"Scale", transform.scale,
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

template <typename... T>
void DrawComponents(Entity entity, ComponentTypes<T...>) {
	(DrawComponent<T>(entity), ...);
}

template <typename T>
void DrawAddComponentItem(Entity entity) {
	if (entity.Has<T>()) {
		return;
	}

	auto name{ TypeLabel<T>() };
	if (ImGui::MenuItem(name.c_str())) {
		entity.Add<T>();
	}
}

template <typename... T>
void DrawAddComponentMenu(Entity entity, ComponentTypes<T...>) {
	(DrawAddComponentItem<T>(entity), ...);
}

void DrawAddDrawableMenu(Entity entity) {
	if (entity.Has<impl::IDrawable>()) {
		return;
	}

	if (!ImGui::BeginMenu("Drawable")) {
		return;
	}

	for (const auto& info : impl::IDrawable::data()) {
		if (ImGui::MenuItem(std::string{ info.name }.c_str())) {
			entity.Add<impl::IDrawable>(info.hash);
		}
	}

	ImGui::EndMenu();
}

} // namespace

void InspectorPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Inspector");

	auto& scene_hierarchy{ ctx.editor.GetSceneHierarchyPanel() };
	auto selected_entity{ scene_hierarchy.GetSelectedEntity() };

	if (!selected_entity) {
		ImGui::End();
		return;
	}

	auto name{ std::string{ selected_entity.GetTag() } };
	if (ImGui::InputText("Name", &name)) {
		selected_entity.SetTag(name);
	}

	ImGui::Separator();

	DrawTransformComponent(selected_entity);
	DrawComponent<impl::IDrawable>(selected_entity, { .label_override = "Drawable" });
	DrawComponents(selected_entity, DefaultInspectorComponents{});

	DrawComponent<ButtonBackgroundVisuals>(
		selected_entity, ComponentOptions{
							 .removable = false,
						 }
	);

	DrawComponent<ButtonBorderVisuals>(
		selected_entity, ComponentOptions{
							 .removable = false,
						 }
	);

	DrawComponent<ButtonSpriteVisuals>(
		selected_entity, ComponentOptions{
							 .removable = false,
						 }
	);

	DrawComponent<ButtonTextVisuals>(
		selected_entity, ComponentOptions{
							 .removable = false,
						 }
	);

	DrawComponent<ButtonSounds>(
		selected_entity, ComponentOptions{
							 .removable = false,
						 }
	);

	ImGui::Separator();

	if (ImGui::Button("Add Component", ImVec2{ -1.0f, 0.0f })) {
		ImGui::OpenPopup("AddComponentPopup");
	}

	if (ImGui::BeginPopup("AddComponentPopup")) {
		DrawAddComponentMenu(selected_entity, DefaultInspectorComponents{});
		DrawAddDrawableMenu(selected_entity);
		ImGui::EndPopup();
	}

	ImGui::End();
}

} // namespace ptgn::editor
