#pragma once

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
#include "editor/renamable_item.h"
#include "panels/entity_filter_editor.h"
#include "panels/inspector_feature_helpers.h"
#include "panels/inspector_archetypes.h"
#include "panels/inspector_layout.h"
#include "panels/inspector_tabs.h"
#include "panels/inspector_fields.h"
#include "panels/inspector_targets.h"
#include "panels/inspector_component_drawers.h"
#include "panels/inspector_scripts.h"
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

inline void DrawDisabledWrappedText(std::string_view text) {
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextUnformatted(text.data(), text.data() + text.size());
	ImGui::PopTextWrapPos();
	ImGui::PopStyleColor();
}
enum class InspectorFeature : std::uint8_t {
	Transform,
	Visual,
	Interaction,
	Physics,
	UI,
	Camera,
	Scripts,
	Utilities,
	Count,
};

template <typename... T>
struct FeatureComponents {};

using TransformFeatureComponents = FeatureComponents<
	Transform, Depth, ::ptgn::impl::IgnoreParentTransform, ::ptgn::impl::IgnoreParentPosition,
	::ptgn::impl::IgnoreParentRotation, ::ptgn::impl::IgnoreParentScale,
	::ptgn::impl::IgnoreParentDepth>;

using VisualFeatureComponents = FeatureComponents<
	::ptgn::impl::IDrawable, Visible, ::ptgn::impl::IgnoreParentVisibility, Origin, BlendMode, Rect,
	Circle, RoundedRect, Polygon, Ellipse, Triangle, Line, Capsule, Arc, Color, FillStyle,
	TextureKey, ::ptgn::impl::TextureSize, ::ptgn::impl::TextureCrop, ::ptgn::impl::AnimationData,
	::ptgn::SpriteStackData, ::ptgn::impl::Offsets, Tint, ::ptgn::impl::IgnoreParentTint,
	::ptgn::impl::TextData, ::ptgn::impl::ParticleEmitterData, LightData,
	::ptgn::impl::ShadowCaster, ::ptgn::impl::GraphicsData, ::ptgn::Material, ShaderKey,
	::ptgn::impl::RenderTargetDesc, ::ptgn::impl::RenderMask, ::ptgn::impl::UILayer,
	::ptgn::impl::EffectTag, ::ptgn::impl::HDREffectTag, EffectMargin, Bloom, Blur, GaussianBlur,
	::ptgn::impl::ClearColor, ::ptgn::impl::ClearDepth, ::ptgn::impl::ClearStencil>;

using InteractionFeatureComponents = FeatureComponents<
	::ptgn::impl::Interactive, ::ptgn::impl::Draggable, ::ptgn::impl::Dropzone, InteractionLock,
	::ptgn::impl::InteractiveTag>;

using PhysicsFeatureComponents = FeatureComponents<
	Collider, RigidBody, ::ptgn::impl::IgnoreParentImmovable, BoundaryBehavior, TopDownMovement,
	PlatformerMovement, PlatformerJump>;

using UIFeatureComponents = FeatureComponents<
	::ptgn::impl::ButtonData, ::ptgn::impl::ButtonAnimationPart, ButtonBackgroundVisuals,
	ButtonBorderVisuals, ButtonSpriteVisuals, ButtonTextVisuals, ButtonSounds,
	::ptgn::impl::SliderData, ::ptgn::impl::ToggleButtonData, ::ptgn::impl::ToggleButtonGroupData,
	::ptgn::impl::ToggleButtonGroupItem, ::ptgn::impl::DropdownData, ::ptgn::impl::DropdownItem,
	::ptgn::impl::DialogueData, ::ptgn::impl::DialoguePart,
	::ptgn::impl::TooltipData, ::ptgn::impl::TooltipHoverData, ::ptgn::impl::TooltipBackgroundPart,
	::ptgn::impl::TooltipTextPart>;

using CameraFeatureComponents = FeatureComponents<
	::ptgn::impl::CameraData, ::ptgn::impl::CameraMask, ::ptgn::impl::ParentRenderTarget>;

using ScriptsFeatureComponents = FeatureComponents<::ptgn::impl::Scripts>;

using UtilitiesFeatureComponents = FeatureComponents<::ptgn::impl::Timers, Lifetime, Group>;

struct ManualFeatureState {
	FeatureTargetKey target{};
	std::array<bool, static_cast<std::size_t>(InspectorFeature::Count)> features{};
	bool text_box_state_initialized{ false };
	bool text_box_enabled{ false };
	bool scale_ratio_locked{ true };
	std::optional<ButtonVisualState> button_visual_state{};
	std::size_t dropdown_item_index{ 0 };
	std::size_t toggle_group_item_index{ 0 };

	std::string dialogue_key{};
	std::size_t dialogue_variant_index{ 0 };
	std::size_t dialogue_preview_page{ 0 };
	std::optional<std::string> dialogue_rename_key{};
	RenameModalState dialogue_rename{};
	std::optional<std::size_t> dialogue_variant_rename_index{};
	RenameModalState dialogue_variant_rename{};
	std::string dialogue_portrait_actor{};
	std::string dialogue_portrait_expression{};

	EntityFilterEditorState grounding_filter_state{};
};

inline std::vector<ManualFeatureState>& ManualFeatureStates() {
	static std::vector<ManualFeatureState> states;
	return states;
}

[[nodiscard]] inline bool SameManualFeatureTarget(
	const FeatureTargetKey& lhs, const FeatureTargetKey& rhs
) {
	if (lhs.entity && rhs.entity) {
		return lhs.entity == rhs.entity;
	}

	if (lhs.prefab && rhs.prefab) {
		return lhs.prefab == rhs.prefab && lhs.prefab_entity_path == rhs.prefab_entity_path;
	}

	return lhs == rhs;
}

inline ManualFeatureState& GetManualFeatureState(const FeatureTargetKey& target) {
	auto& states{ ManualFeatureStates() };
	const auto it{ std::ranges::find_if(states, [&target](const ManualFeatureState& state) {
		return SameManualFeatureTarget(state.target, target);
	}) };

	if (it != states.end()) {
		return *it;
	}

	states.push_back(ManualFeatureState{ .target = target });
	return states.back();
}

enum class ButtonChildPart : std::uint8_t {
	Background,
	Border,
	Text,
	Sprite,
};

struct ButtonChildInfo {
	Entity child{};
	Entity button{};
	ButtonChildPart part{ ButtonChildPart::Background };
};

[[nodiscard]] inline Entity FindButtonPart(Entity button, ButtonChildPart part) {
	if (!button || !HasChildren(button)) {
		return {};
	}

	for (Entity child : GetChildren(button)) {
		switch (part) {
			case ButtonChildPart::Background:
				if (child.Has<ButtonBackgroundVisuals>()) {
					return child;
				}
				break;
			case ButtonChildPart::Border:
				if (child.Has<ButtonBorderVisuals>()) {
					return child;
				}
				break;
			case ButtonChildPart::Text:
				if (child.Has<ButtonTextVisuals>()) {
					return child;
				}
				break;
			case ButtonChildPart::Sprite:
				if (child.Has<ButtonSpriteVisuals>()) {
					return child;
				}
				break;
		}
	}

	return {};
}

inline std::optional<EntityReference>& PreviewedButtonReference() {
	static std::optional<EntityReference> reference;
	return reference;
}

inline void ClearButtonPreviewIfDifferent(EditorContext& ctx, Entity keep = {}) {
	auto& previewed{ PreviewedButtonReference() };
	if (!previewed) {
		return;
	}

	Entity previous{ previewed->Resolve(ctx.editor) };
	if (previous && keep && previous == keep) {
		return;
	}

	if (previous && previous.Has<::ptgn::impl::ButtonData>()) {
		Button{ previous }.PreviewVisualState(std::nullopt);
	}
	previewed.reset();
}

[[nodiscard]] inline bool IsButtonRoot(Entity entity) {
	return entity &&
		   (entity.Has<::ptgn::impl::ButtonData>() || entity.Has<::ptgn::impl::SliderData>() ||
			entity.Has<::ptgn::impl::ToggleButtonData>() ||
			entity.Has<::ptgn::impl::DropdownData>());
}

template <typename Target>
[[nodiscard]] std::optional<ButtonChildInfo> GetButtonChildInfo(const Target& target) {
	if constexpr (!requires { target.entity; }) {
		return std::nullopt;
	} else {
		Entity child{ target.entity };

		if (!child) {
			return std::nullopt;
		}

		Entity button{ GetParent(child) };

		if (!IsButtonRoot(button)) {
			return std::nullopt;
		}

		if (child.Has<ButtonBackgroundVisuals>()) {
			return ButtonChildInfo{
				.child	= child,
				.button = button,
				.part	= ButtonChildPart::Background,
			};
		}

		if (child.Has<ButtonBorderVisuals>()) {
			return ButtonChildInfo{
				.child	= child,
				.button = button,
				.part	= ButtonChildPart::Border,
			};
		}

		if (child.Has<ButtonTextVisuals>()) {
			return ButtonChildInfo{
				.child	= child,
				.button = button,
				.part	= ButtonChildPart::Text,
			};
		}

		if (child.Has<ButtonSpriteVisuals>()) {
			return ButtonChildInfo{
				.child	= child,
				.button = button,
				.part	= ButtonChildPart::Sprite,
			};
		}

		return std::nullopt;
	}
}

template <typename Target>
[[nodiscard]] std::optional<ButtonVisualState> GetButtonVisualEditState(const Target& target) {
	if (!GetButtonChildInfo(target)) {
		return std::nullopt;
	}

	return GetManualFeatureState(target.GetFeatureTargetKey()).button_visual_state;
}

[[nodiscard]] inline std::span<const ButtonVisualState> GetButtonVisualStateFallbacks(
	ButtonVisualState state
) {
	using enum ButtonVisualState;

	static constexpr std::array idle{ Idle };
	static constexpr std::array hover{ Hover, Idle };
	static constexpr std::array press{ Press, Hover, Idle };
	static constexpr std::array disabled{ Disabled, Idle };
	static constexpr std::array disabled_hover{ DisabledHover, Disabled, Hover, Idle };
	static constexpr std::array disabled_press{ DisabledPress, DisabledHover, Disabled,
												Press,		   Hover,		  Idle };
	static constexpr std::array toggled{ Toggled, Idle };
	static constexpr std::array toggled_hover{ ToggledHover, Toggled, Hover, Idle };
	static constexpr std::array toggled_press{ ToggledPress, ToggledHover, Toggled,
											   Press,		 Hover,		   Idle };

	switch (state) {
		case Idle:			return idle;
		case Hover:			return hover;
		case Press:			return press;
		case Disabled:		return disabled;
		case DisabledHover: return disabled_hover;
		case DisabledPress: return disabled_press;
		case Toggled:		return toggled;
		case ToggledHover:	return toggled_hover;
		case ToggledPress:	return toggled_press;
	}

	return idle;
}

template <typename Visual, typename T, std::size_t N>
[[nodiscard]] std::optional<T> ResolveButtonVisualProperty(
	const std::array<Visual, N>& states, ButtonVisualState state,
	const std::optional<T> Visual::* member
) {
	for (const ButtonVisualState fallback : GetButtonVisualStateFallbacks(state)) {
		const auto& visual{ states[static_cast<std::size_t>(std::to_underlying(fallback))] };

		if (!visual.defined) {
			continue;
		}

		const auto& value{ visual.*member };

		if (value) {
			return value;
		}
	}

	return std::nullopt;
}

inline std::string NormalizeFeatureName(std::string_view input) {
	std::string result;
	result.reserve(input.size());

	for (const char c : input) {
		if (std::isalnum(static_cast<unsigned char>(c))) {
			result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
		}
	}

	return result;
}

template <typename Info>
[[nodiscard]] std::string GetDrawableInspectorLabel(const Info& info) {
	std::string name{ info.GetDisplayName() };

	if (const auto separator{ name.rfind("::") }; separator != std::string::npos) {
		name.erase(0, separator + 2);
	}

	return PrettyName(name);
}

template <typename Visual, typename T, std::size_t N>
bool DrawButtonVisualOverrideValue(
	EditorContext& ctx, std::string_view label, std::array<Visual, N>& states,
	ButtonVisualState state, std::optional<T> Visual::* member, Entity relative_to = {}
) {
	auto& visual{ states[static_cast<std::size_t>(std::to_underlying(state))] };
	auto& value{ visual.*member };
	const bool had_override{ value.has_value() };
	const std::optional<T> inherited{ ResolveButtonVisualProperty(states, state, member) };

	// Size overrides use a label-side checkbox. Keeping the checkbox in the label
	// column prevents the controls from jumping when the override is enabled.
	const std::string normalized_label{ NormalizeFeatureName(label) };

	if constexpr (std::same_as<T, Transform>) {
		ScopedID value_scope{ std::addressof(value) };
		const bool was_enabled{ value.has_value() };
		bool enabled{ was_enabled };
		Transform displayed{ value.value_or(inherited.value_or(Transform{})) };
		float depth{ visual.depth.value_or(
			ResolveButtonVisualProperty(states, state, &Visual::depth).value_or(0.0f)
		) };
		bool inherit_position{ visual.inherit_position.value_or(
			ResolveButtonVisualProperty(states, state, &Visual::inherit_position).value_or(true)
		) };
		bool inherit_rotation{ visual.inherit_rotation.value_or(
			ResolveButtonVisualProperty(states, state, &Visual::inherit_rotation).value_or(true)
		) };
		bool inherit_scale{ visual.inherit_scale.value_or(
			ResolveButtonVisualProperty(states, state, &Visual::inherit_scale).value_or(true)
		) };
		bool inherit_depth{ visual.inherit_depth.value_or(
			ResolveButtonVisualProperty(states, state, &Visual::inherit_depth).value_or(true)
		) };
		bool changed{ false };
		bool open{ false };
		const bool toggle_changed{ DrawInspectorCustomPropertyRow(
			label,
			[&]() {
				open = ImGui::TreeNodeEx(
					"Transform##ButtonVisualRelativeTransform",
					ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
						ImGuiTreeNodeFlags_NoTreePushOnOpen
				);
			},
			[&]() { return ImGui::Checkbox("##Enabled", &enabled); }
		) };
		if (toggle_changed) {
			if (enabled) {
				value = displayed;
				visual.depth = depth;
				visual.inherit_position = inherit_position;
				visual.inherit_rotation = inherit_rotation;
				visual.inherit_scale = inherit_scale;
				visual.inherit_depth = inherit_depth;
			} else {
				value.reset();
				visual.depth.reset();
				visual.inherit_position.reset();
				visual.inherit_rotation.reset();
				visual.inherit_scale.reset();
				visual.inherit_depth.reset();
			}
			changed = true;
		}

		if (open) {
			ScopedIndent indent;
			ScopedDisabled disabled{ !enabled };
			bool fields_changed{ false };

			auto draw_ignore = [&](bool& inherit, const char* tooltip) {
				bool ignore{ !inherit };
				const bool local_changed{ ImGui::Checkbox("##IgnoreParent", &ignore) };
				if (local_changed) {
					inherit = !ignore;
				}
				DrawTooltip(tooltip);
				return local_changed;
			};

			fields_changed |= DrawPropertyRow("Position", [&]() {
				const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
				const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
				const float pick_width{ ImGui::CalcTextSize("Pick").x +
					ImGui::GetStyle().FramePadding.x * 2.0f };
				const float inherit_width{ ImGui::GetFrameHeight() };
				const float actions_width{ pick_width + inherit_width + spacing };
				const bool actions_inline{ available >= actions_width + spacing + 104.0f };
				const float fields_width{
					actions_inline ? std::max(1.0f, available - actions_width - spacing) : available
				};
				const float field_width{ InspectorSplitWidth(2, fields_width, spacing) };
				bool local_changed{ false };

				ImGui::SetNextItemWidth(field_width);
				local_changed |= ImGui::DragFloat(
					"##X", &displayed.position.x, 1.0f, 0.0f, 0.0f, "X: %.0f"
				);
				ImGui::SameLine(0.0f, spacing);
				ImGui::SetNextItemWidth(field_width);
				local_changed |= ImGui::DragFloat(
					"##Y", &displayed.position.y, 1.0f, 0.0f, 0.0f, "Y: %.0f"
				);

				if (actions_inline) {
					ImGui::SameLine(0.0f, spacing);
				}
				const ImGuiID pick_id{ ImGui::GetID("##RelativeVisualPositionPick") };
				static std::unordered_map<ImGuiID, V2_float> picked_positions;
				if (const auto it{ picked_positions.find(pick_id) }; it != picked_positions.end()) {
					if (displayed.position != it->second) {
						displayed.position = it->second;
						local_changed = true;
					} else if (!IsPositionPickingActive(ctx)) {
						picked_positions.erase(it);
					}
				}
				if (relative_to) {
					const Transform basis{ GetDrawTransform(relative_to) };
					DrawPositionPickButton(
						ctx, "RelativeVisualPosition", displayed.position,
						[relative_to](V2_float world) -> std::optional<V2_float> {
							if (!relative_to) return std::nullopt;
							return GetDrawTransform(relative_to).ApplyInverse(world);
						},
						[pick_id](V2_float picked) { picked_positions[pick_id] = picked; },
						basis.Apply(displayed.position), true
					);
				} else {
					ScopedDisabled no_basis{ true };
					ImGui::Button("Pick");
				}
				ImGui::SameLine(0.0f, spacing);
				local_changed |= draw_ignore(inherit_position, "Ignore parent position.");
				return local_changed;
			});

			fields_changed |= DrawPropertyRow("Depth", [&]() {
				const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
				const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
				const float checkbox_width{ ImGui::GetFrameHeight() };
				const bool inline_ignore{ available >= checkbox_width + spacing + 72.0f };
				ImGui::SetNextItemWidth(
					inline_ignore ? std::max(1.0f, available - checkbox_width - spacing) : -FLT_MIN
				);
				bool local_changed{ ImGui::DragFloat("##Value", &depth, 1.0f, 0.0f, 0.0f, "%.0f") };
				if (inline_ignore) ImGui::SameLine(0.0f, spacing);
				local_changed |= draw_ignore(inherit_depth, "Ignore parent depth.");
				return local_changed;
			});

			fields_changed |= DrawPropertyRow("Rotation", [&]() {
				const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
				const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
				const float checkbox_width{ ImGui::GetFrameHeight() };
				const bool inline_ignore{ available >= checkbox_width + spacing + 72.0f };
				ImGui::SetNextItemWidth(
					inline_ignore ? std::max(1.0f, available - checkbox_width - spacing) : -FLT_MIN
				);
				Degrees rotation{ displayed.rotation };
				float degrees{ rotation.value };
				bool local_changed{ false };
				if (ImGui::DragFloat(
						"##Value", &degrees, 1.0f, 0.0f, 360.0f, "%.1f deg",
						ImGuiSliderFlags_AlwaysClamp
					)) {
					displayed.rotation = Radians{ Degrees{ degrees } };
					local_changed = true;
				}
				if (inline_ignore) ImGui::SameLine(0.0f, spacing);
				local_changed |= draw_ignore(inherit_rotation, "Ignore parent rotation.");
				return local_changed;
			});

			fields_changed |= DrawPropertyRow("Scale", [&]() {
				constexpr float minimum{ 0.001f };
				constexpr float epsilon{ 0.000001f };
				const V2_float before_scale{ displayed.scale };
				const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
				const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
				const float action_width{ ImGui::GetFrameHeight() * 2.0f + spacing };
				const bool actions_inline{ available >= action_width + spacing + 104.0f };
				const float fields_width{
					actions_inline ? std::max(1.0f, available - action_width - spacing) : available
				};
				const float field_width{ InspectorSplitWidth(2, fields_width, spacing) };
				ImGui::SetNextItemWidth(field_width);
				const bool x_changed{ ImGui::DragFloat(
					"##X", &displayed.scale.x, 0.01f, -1000.0f, 1000.0f, "X: %.2f"
				) };
				ImGui::SameLine(0.0f, spacing);
				ImGui::SetNextItemWidth(field_width);
				const bool y_changed{ ImGui::DragFloat(
					"##Y", &displayed.scale.y, 0.01f, -1000.0f, 1000.0f, "Y: %.2f"
				) };
				auto clamp_axis = [](float current, float previous) {
					if (std::abs(current) >= minimum) return current;
					return (current < 0.0f || (current == 0.0f && previous < 0.0f))
						? -minimum : minimum;
				};
				displayed.scale.x = clamp_axis(displayed.scale.x, before_scale.x);
				displayed.scale.y = clamp_axis(displayed.scale.y, before_scale.y);

				const ImGuiID lock_id{ ImGui::GetID("##RelativeVisualScaleLock") };
				static std::unordered_map<ImGuiID, bool> scale_locks;
				bool& lock_ratio{ scale_locks.try_emplace(lock_id, true).first->second };
				if (lock_ratio) {
					if (x_changed && !y_changed && std::abs(before_scale.x) > epsilon) {
						displayed.scale.y = before_scale.y * (displayed.scale.x / before_scale.x);
					} else if (y_changed && !x_changed && std::abs(before_scale.y) > epsilon) {
						displayed.scale.x = before_scale.x * (displayed.scale.y / before_scale.y);
					}
				}

				if (actions_inline) ImGui::SameLine(0.0f, spacing);
				ImGui::Checkbox("##LockRatio", &lock_ratio);
				DrawTooltip("Lock the scale ratio.");
				ImGui::SameLine(0.0f, spacing);
				bool local_changed{ x_changed || y_changed };
				local_changed |= draw_ignore(inherit_scale, "Ignore parent scale.");
				return local_changed;
			});

			if (enabled && fields_changed) {
				value = displayed;
				visual.depth = depth;
				visual.inherit_position = inherit_position;
				visual.inherit_rotation = inherit_rotation;
				visual.inherit_scale = inherit_scale;
				visual.inherit_depth = inherit_depth;
				changed = true;
			}

		}
		return changed;
	}
	if (normalized_label == "size" || normalized_label == "texturesize") {
		if constexpr (std::same_as<T, V2_float>) {
			ScopedID value_scope{ std::addressof(value) };
			const bool was_enabled{ value.has_value() };
			bool enabled{ was_enabled };
			V2_float displayed{ value.value_or(inherited.value_or(V2_float{})) };
			bool field_changed{ false };

			bool changed{ DrawOptionalPropertyRow(label, enabled, false, [&]() {
				ScopedDisabled disabled{ !enabled };

				const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
				const float width{
					std::max(1.0f, (ImGui::GetContentRegionAvail().x - spacing) * 0.5f)
				};
				ImGui::SetNextItemWidth(width);
				field_changed |= ImGui::DragFloat(
					"##W", &displayed.x, kInspectorSizeDragSpeed, 0.0f, 0.0f, "W: %.3f"
				);
				ImGui::SameLine(0.0f, spacing);
				ImGui::SetNextItemWidth(width);
				field_changed |= ImGui::DragFloat(
					"##H", &displayed.y, kInspectorSizeDragSpeed, 0.0f, 0.0f, "H: %.3f"
				);
				displayed.x = std::max(0.0f, displayed.x);
				displayed.y = std::max(0.0f, displayed.y);
				return field_changed;
			}) };

			if (enabled != was_enabled) {
				if (enabled) {
					value = displayed;
				} else {
					value.reset();
				}
				changed = true;
			} else if (enabled && field_changed) {
				value = displayed;
			}

			return changed;
		} else if constexpr (std::same_as<T, std::variant<V2_float, float>>) {
			ScopedID value_scope{ std::addressof(value) };
			const bool was_enabled{ value.has_value() };
			bool enabled{ was_enabled };
			T displayed{ value.value_or(inherited.value_or(T{ V2_float{} })) };
			bool field_changed{ false };

			bool changed{ DrawOptionalPropertyRow(label, enabled, false, [&]() {
				ScopedDisabled disabled{ !enabled };

				if (std::holds_alternative<V2_float>(displayed)) {
					auto& size{ std::get<V2_float>(displayed) };
					const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
					const float width{
						std::max(1.0f, (ImGui::GetContentRegionAvail().x - spacing) * 0.5f)
					};
					ImGui::SetNextItemWidth(width);
					field_changed |= ImGui::DragFloat(
						"##W", &size.x, kInspectorSizeDragSpeed, 0.0f, 0.0f, "W: %.3f"
					);
					ImGui::SameLine(0.0f, spacing);
					ImGui::SetNextItemWidth(width);
					field_changed |= ImGui::DragFloat(
						"##H", &size.y, kInspectorSizeDragSpeed, 0.0f, 0.0f, "H: %.3f"
					);
					size.x = std::max(0.0f, size.x);
					size.y = std::max(0.0f, size.y);
				} else {
					auto& radius{ std::get<float>(displayed) };
					ImGui::SetNextItemWidth(-FLT_MIN);
					field_changed |= ImGui::DragFloat(
						"##R", &radius, kInspectorSizeDragSpeed, 0.0f, 0.0f, "R: %.3f"
					);
					radius = std::max(0.0f, radius);
				}
				return field_changed;
			}) };

			if (enabled != was_enabled) {
				if (enabled) {
					value = displayed;
				} else {
					value.reset();
				}
				changed = true;
			} else if (enabled && field_changed) {
				value = displayed;
			}

			return changed;
		}
	}

	if (normalized_label == "tint") {
		if constexpr (std::same_as<T, Color>) {
			ScopedID value_scope{ std::addressof(value) };
			const bool was_enabled{ value.has_value() };
			bool enabled{ was_enabled };
			Color displayed{ value.value_or(inherited.value_or(color::White)) };
			bool field_changed{ false };

			bool changed{ DrawOptionalPropertyRow(label, enabled, false, [&]() {
				ScopedDisabled disabled{ !enabled };
				const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
				const float swatch_width{ ImGui::GetFrameHeight() };
				const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
				const bool show_channels{
					available >= swatch_width + spacing * 4.0f + 4.0f * 44.0f
				};

				if (show_channels) {
					std::array<int, 4> channels{
						static_cast<int>(displayed.r), static_cast<int>(displayed.g),
						static_cast<int>(displayed.b), static_cast<int>(displayed.a)
					};
					const float channel_area{
						std::max(1.0f, available - swatch_width - spacing)
					};
					const float channel_width{ InspectorSplitWidth(4, channel_area, spacing) };
					static constexpr std::array<const char*, 4> ids{ "##R", "##G", "##B", "##A" };
					static constexpr std::array<const char*, 4> formats{
						"R: %d", "G: %d", "B: %d", "A: %d"
					};
					for (std::size_t i{ 0 }; i < channels.size(); ++i) {
						if (i != 0) ImGui::SameLine(0.0f, spacing);
						ImGui::SetNextItemWidth(channel_width);
						field_changed |= ImGui::DragInt(
							ids[i], &channels[i], 1.0f, 0, 255, formats[i],
							ImGuiSliderFlags_AlwaysClamp
						);
					}
					displayed = Color{
						static_cast<std::uint8_t>(std::clamp(channels[0], 0, 255)),
						static_cast<std::uint8_t>(std::clamp(channels[1], 0, 255)),
						static_cast<std::uint8_t>(std::clamp(channels[2], 0, 255)),
						static_cast<std::uint8_t>(std::clamp(channels[3], 0, 255))
					};
					ImGui::SameLine(0.0f, spacing);
				}

				ImGui::SetNextItemWidth(show_channels ? swatch_width : -FLT_MIN);
				field_changed |= DrawColorEdit(
					ctx, "##TintPicker", displayed,
					ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Uint8 |
						ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf
				);
				return field_changed;
			}) };

			if (enabled != was_enabled) {
				if (enabled) value = displayed;
				else value.reset();
				changed = true;
			} else if (enabled && field_changed) {
				value = displayed;
			}
			return changed;
		}
	}

	const bool changed{ DrawValue(ctx, label, value) };

	if (changed && !had_override && value && inherited) {
		value = *inherited;
	}

	return changed;
}

template <typename Visual, std::size_t N>
bool DrawButtonVisualTriStateBoolOverrideValue(
	EditorContext& ctx, std::string_view label, std::array<Visual, N>& states,
	ButtonVisualState state, std::optional<bool> Visual::* member
) {
	(void)ctx;
	auto& visual{ states[static_cast<std::size_t>(std::to_underlying(state))] };
	auto& value{ visual.*member };

	int mode{ value.has_value() ? (*value ? 1 : 2) : 0 };
	const int before_mode{ mode };
	const char* preview{ mode == 0 ? "Inherit" : mode == 1 ? "Enabled" : "Disabled" };

	const bool changed{ DrawPropertyRow(label, [&]() {
		bool local_changed{ false };
		ImGui::SetNextItemWidth(-FLT_MIN);

		if (ImGui::BeginCombo("##Value", preview)) {
			if (ImGui::Selectable("Inherit", mode == 0)) {
				mode		  = 0;
				local_changed = true;
			}
			if (ImGui::Selectable("Enabled", mode == 1)) {
				mode		  = 1;
				local_changed = true;
			}
			if (ImGui::Selectable("Disabled", mode == 2)) {
				mode		  = 2;
				local_changed = true;
			}
			ImGui::EndCombo();
		}

		return local_changed;
	}) };

	if (!changed || mode == before_mode) {
		return false;
	}

	switch (mode) {
		case 0:	 value.reset(); break;
		case 1:	 value = true; break;
		case 2:	 value = false; break;
		default: break;
	}

	return true;
}

template <typename Visual, typename T, std::size_t N, typename Draw>
bool DrawButtonVisualOverrideTree(
	EditorContext& ctx, std::string_view label, std::array<Visual, N>& states,
	ButtonVisualState state, std::optional<T> Visual::* member, Draw&& draw
) {
	(void)ctx;
	auto& visual{ states[static_cast<std::size_t>(std::to_underlying(state))] };
	auto& value{ visual.*member };
	const std::optional<T> inherited{ ResolveButtonVisualProperty(states, state, member) };
	const bool was_enabled{ value.has_value() };
	bool enabled{ was_enabled };
	T displayed{ value.value_or(inherited.value_or(T{})) };
	bool changed{ false };
	bool open{ false };

	ScopedID value_scope{ std::addressof(value) };
	// Keep optional tree toggles consistent with the Sprite archetype: the checkbox is
	// immediately to the left of the tree node and the tree node consumes the rest of the row.
	const bool toggle_changed{ ImGui::Checkbox("##Enabled", &enabled) };
	ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
	const std::string tree_label{ std::string{ label } + "##ButtonVisualOverrideTree" };
	open = ImGui::TreeNodeEx(
		tree_label.c_str(),
		ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
			ImGuiTreeNodeFlags_NoTreePushOnOpen
	);

	if (toggle_changed) {
		if (enabled) {
			value = displayed;
		} else {
			value.reset();
		}
		changed = true;
	}

	if (open) {
		ScopedIndent indent;
		ScopedPropertyLabelOffset label_offset{ ImGui::GetStyle().IndentSpacing };
		ScopedDisabled disabled{ !enabled };
		T edited{ enabled && value ? value.value() : displayed };
		const bool contents_changed{ std::invoke(std::forward<Draw>(draw), edited) };
		if (enabled && contents_changed) {
			value = std::move(edited);
			changed = true;
		}
	}

	return changed;
}

template <typename Visual, typename T, std::size_t N>
bool DrawButtonVisualOverrideTree(
	EditorContext& ctx, std::string_view label, std::array<Visual, N>& states,
	ButtonVisualState state, std::optional<T> Visual::* member
) {
	return DrawButtonVisualOverrideTree(ctx, label, states, state, member, [&ctx](T& value) {
		return DrawDefaultContents(ctx, value);
	});
}


template <std::size_t N>
bool DrawButtonBorderLineWidth(
	Entity button_entity, std::array<ButtonShapeVisual, N>& states, ButtonVisualState state,
	std::optional<std::variant<V2_float, float>> fallback_size = std::nullopt
) {
	auto& visual{ states[static_cast<std::size_t>(std::to_underlying(state))] };
	auto& value{ visual.fill_style };
	const std::optional<FillStyle> inherited{
		ResolveButtonVisualProperty(states, state, &ButtonShapeVisual::fill_style)
	};

	auto effective_size = ResolveButtonVisualProperty(states, state, &ButtonShapeVisual::size);
	if (!effective_size.has_value()) {
		if (Entity background{ FindButtonPart(button_entity, ButtonChildPart::Background) };
			background && background.Has<ButtonBackgroundVisuals>()) {
			const auto& backgrounds{ background.Get<ButtonBackgroundVisuals>().states };
			effective_size =
				ResolveButtonVisualProperty(backgrounds, state, &ButtonShapeVisual::size);
		}
	}
	if (!effective_size.has_value() && button_entity && button_entity.HasAny<Rect, Circle>()) {
		effective_size = Button{ button_entity }.GetSize();
	}
	if (!effective_size.has_value() && fallback_size.has_value()) {
		effective_size = fallback_size;
	}

	float maximum_width{ kInspectorMinLineWidth };
	if (effective_size.has_value()) {
		maximum_width = std::visit(
			[](const auto& size) -> float {
				using T = std::remove_cvref_t<decltype(size)>;
				if constexpr (std::same_as<T, V2_float>) {
					return std::max(
						kInspectorMinLineWidth, std::min(std::abs(size.x), std::abs(size.y)) * 0.5f
					);
				} else {
					return std::max(kInspectorMinLineWidth, std::abs(size));
				}
			},
			*effective_size
		);
	}

	const bool was_enabled{ value.has_value() };
	bool enabled{ was_enabled };
	const FillStyle resolved{
		value.value_or(inherited.value_or(FillStyle{ kInspectorMinLineWidth }))
	};
	float line_width{ resolved.GetLineWidth().value_or(kInspectorMinLineWidth) };
	line_width = std::clamp(line_width, kInspectorMinLineWidth, maximum_width);

	bool field_changed{ false };
	const bool row_changed{ DrawOptionalPropertyRow("Line Width", enabled, false, [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		field_changed = ImGui::DragFloat(
			"##Value", &line_width, kInspectorScalarDragSpeed, kInspectorMinLineWidth,
			maximum_width, "%.2f", ImGuiSliderFlags_AlwaysClamp
		);
		return field_changed;
	}) };

	bool changed{ row_changed };
	if (enabled != was_enabled) {
		if (enabled) {
			value = FillStyle{ std::clamp(line_width, kInspectorMinLineWidth, maximum_width) };
		} else {
			value.reset();
		}
		changed = true;
	} else if (enabled && field_changed) {
		value = FillStyle{ std::clamp(line_width, kInspectorMinLineWidth, maximum_width) };
	}
	return changed;
}

/// @brief Shared shape-visual fields used by button-managed shape parts and UI controls
/// that intentionally expose the same appearance model (for example dialogue overrides).
/// Backgrounds are always solid; borders expose a line-width override.
template <std::size_t N>
bool DrawButtonShapeVisualFields(
	EditorContext& ctx, Entity button_entity, std::array<ButtonShapeVisual, N>& states,
	ButtonVisualState state, bool border,
	std::optional<std::variant<V2_float, float>> fallback_size = std::nullopt,
	bool draw_transform = true
) {
	bool changed{ false };

	if (draw_transform) {
		changed |= DrawButtonVisualOverrideValue(
			ctx, "Transform", states, state, &ButtonShapeVisual::transform, button_entity
		);
	}

	ImGui::SeparatorText("Visual");
	changed |= DrawButtonVisualOverrideValue(
		ctx, "Size", states, state, &ButtonShapeVisual::size
	);
	changed |= DrawButtonVisualOverrideValue(
		ctx, "Color", states, state, &ButtonShapeVisual::color
	);

	if (border) {
		changed |= DrawButtonBorderLineWidth(
			button_entity, states, state, std::move(fallback_size)
		);
	}

	changed |= DrawButtonVisualOverrideValue(
		ctx, "Origin", states, state, &ButtonShapeVisual::origin
	);
	changed |= DrawButtonVisualOverrideValue(
		ctx, "Anchor", states, state, &ButtonShapeVisual::anchor
	);

	return changed;
}

/// @brief Shared sprite-visual fields used by button-managed sprites and dialogue sprites.
template <std::size_t N>
bool DrawButtonSpriteVisualFields(
	EditorContext& ctx, std::array<ButtonSpriteVisual, N>& states, ButtonVisualState state,
	Entity relative_to = {}, bool draw_transform = true
) {
	bool changed{ false };

	if (draw_transform) {
		changed |= DrawButtonVisualOverrideValue(
			ctx, "Transform", states, state, &ButtonSpriteVisual::transform, relative_to
		);
	}

	ImGui::SeparatorText("Visual");
	changed |= DrawButtonVisualOverrideValue(
		ctx, "Texture Key", states, state, &ButtonSpriteVisual::texture
	);
	changed |= DrawButtonVisualOverrideValue(
		ctx, "Texture Size", states, state, &ButtonSpriteVisual::size
	);
	changed |= DrawButtonVisualOverrideValue(
		ctx, "Origin", states, state, &ButtonSpriteVisual::origin
	);
	changed |= DrawButtonVisualOverrideValue(
		ctx, "Anchor", states, state, &ButtonSpriteVisual::anchor
	);
	changed |= DrawButtonVisualOverrideValue(
		ctx, "Tint", states, state, &ButtonSpriteVisual::tint
	);
	changed |= DrawButtonVisualOverrideTree(
		ctx, "Animation", states, state, &ButtonSpriteVisual::animation
	);
	changed |= DrawButtonVisualOverrideTree(
		ctx, "Animation Options", states, state, &ButtonSpriteVisual::animation_options
	);

	return changed;
}


[[nodiscard]] inline bool IsFeatureManuallyAdded(
	const FeatureTargetKey& target, InspectorFeature feature
) {
	const auto& states{ ManualFeatureStates() };
	const auto it{ std::ranges::find_if(states, [&target](const ManualFeatureState& state) {
		return SameManualFeatureTarget(state.target, target);
	}) };

	if (it == states.end()) {
		return false;
	}

	return it->features[static_cast<std::size_t>(feature)];
}

inline void SetFeatureManuallyAdded(const FeatureTargetKey& target, InspectorFeature feature, bool added) {
	auto& state{ GetManualFeatureState(target) };
	state.features[static_cast<std::size_t>(feature)] = added;

	if (!std::ranges::any_of(state.features, std::identity{})) {
		std::erase_if(ManualFeatureStates(), [&target](const ManualFeatureState& candidate) {
			return SameManualFeatureTarget(candidate.target, target);
		});
	}
}

template <typename Target, typename T>
[[nodiscard]] ComponentState<T> CaptureSupportedFeatureComponent(const Target& target) {
	if constexpr (Target::template Supports<T>()) {
		return target.template Capture<T>();
	} else {
		return std::nullopt;
	}
}

template <typename... T>
struct InspectorFeatureState {
	bool manually_added{ false };
	std::tuple<ComponentState<T>...> components;
};

template <typename Target, typename... T>
[[nodiscard]] InspectorFeatureState<T...> CaptureInspectorFeatureState(
	const Target& target, InspectorFeature feature, FeatureComponents<T...>
) {
	return InspectorFeatureState<T...>{
		.manually_added = IsFeatureManuallyAdded(target.GetFeatureTargetKey(), feature),
		.components		= std::tuple{ CaptureSupportedFeatureComponent<Target, T>(target)... },
	};
}

template <typename Target, typename T>
auto MakeSupportedFeatureComponentApply(Target& target) {
	if constexpr (Target::template Supports<T>()) {
		return target.template MakeApply<T>();
	} else {
		return [](ComponentState<T>) {
		};
	}
}

template <typename ApplyTuple, typename StateTuple, std::size_t... I>
void ApplyFeatureComponentStates(ApplyTuple& apply, StateTuple&& state, std::index_sequence<I...>) {
	(std::invoke(std::get<I>(apply), std::get<I>(std::forward<StateTuple>(state))), ...);
}

template <typename Target, typename... T>
auto MakeInspectorFeatureApply(Target& target, InspectorFeature feature, FeatureComponents<T...>) {
	const FeatureTargetKey target_key{ target.GetFeatureTargetKey() };
	auto component_apply{ std::tuple{ MakeSupportedFeatureComponentApply<Target, T>(target)... } };

	return
		[target_key, feature,
		 component_apply = std::move(component_apply)](InspectorFeatureState<T...> state) mutable {
			SetFeatureManuallyAdded(target_key, feature, state.manually_added);

			ApplyFeatureComponentStates(
				component_apply, std::move(state.components), std::index_sequence_for<T...>{}
			);
		};
}

template <typename Target, typename T>
void RemoveSupportedFeatureComponent(Target& target) {
	if constexpr (Target::template Supports<T>()) {
		target.template SetLive<T>(std::nullopt);
	}
}

template <typename Target, typename... T>
void TrackInspectorFeatureState(
	Target& target, InspectorFeature feature, std::string label, InspectorFeatureState<T...> before,
	InspectorFeatureState<T...> after, FeatureComponents<T...> components
) {
	auto apply{ MakeInspectorFeatureApply(target, feature, components) };

	target.ctx.undo.PushApplied(
		std::move(label), [apply, before]() mutable { apply(before); },
		[apply, after]() mutable { apply(after); }
	);
}

template <typename Target, typename... T>
bool AddInspectorFeature(
	Target& target, InspectorFeature feature, std::string_view label,
	FeatureComponents<T...> components
) {
	auto before{ CaptureInspectorFeatureState(target, feature, components) };

	if (before.manually_added) {
		return false;
	}

	SetFeatureManuallyAdded(target.GetFeatureTargetKey(), feature, true);

	auto after{ CaptureInspectorFeatureState(target, feature, components) };

	TrackInspectorFeatureState(
		target, feature, std::string{ "Add " } + std::string{ label } + " Feature",
		std::move(before), std::move(after), components
	);

	return true;
}

template <typename Default, typename Target, typename... T>
bool AddInspectorFeatureWithDefault(
	Target& target, InspectorFeature feature, std::string_view label,
	FeatureComponents<T...> components
) {
	auto before{ CaptureInspectorFeatureState(target, feature, components) };

	SetFeatureManuallyAdded(target.GetFeatureTargetKey(), feature, true);

	if constexpr (Target::template Supports<Default>()) {
		if (!target.template Capture<Default>()) {
			target.template SetLive<Default>(Default{});
		}
	}

	auto after{ CaptureInspectorFeatureState(target, feature, components) };

	TrackInspectorFeatureState(
		target, feature, std::string{ "Add " } + std::string{ label } + " Feature",
		std::move(before), std::move(after), components
	);

	return true;
}

template <typename Target, typename... T>
bool DeleteInspectorFeature(
	Target& target, InspectorFeature feature, std::string_view label,
	FeatureComponents<T...> components
) {
	auto before{ CaptureInspectorFeatureState(target, feature, components) };

	SetFeatureManuallyAdded(target.GetFeatureTargetKey(), feature, false);
	(RemoveSupportedFeatureComponent<Target, T>(target), ...);

	auto after{ CaptureInspectorFeatureState(target, feature, components) };

	TrackInspectorFeatureState(
		target, feature, std::string{ "Delete " } + std::string{ label } + " Feature",
		std::move(before), std::move(after), components
	);

	return true;
}

struct FeatureHeaderResult {
	bool open{ false };
	bool changed{ false };
};

template <typename Target, typename... T>
FeatureHeaderResult DrawFeatureHeader(
	Target& target, InspectorFeature feature, std::string_view label, ImGuiTreeNodeFlags flags,
	FeatureComponents<T...> components, bool allow_delete = true
) {
	ScopedID feature_scope{ static_cast<int>(feature) };
	const auto section{ DrawInspectorSectionHeader(
		label,
		"##FeatureHeader",
		InspectorSectionOptions{
			.default_open = (flags & ImGuiTreeNodeFlags_DefaultOpen) != 0,
			.removable = allow_delete,
		}
	) };

	FeatureHeaderResult result{ .open = section.open };
	if (section.remove_requested) {
		result.changed = DeleteInspectorFeature(target, feature, label, components);
		result.open = false;
	}
	return result;
}

template <typename Target, typename T>
bool DrawIgnoreCheckbox(Target& target, std::string_view tooltip) {
	auto before{ target.template Capture<T>() };
	bool enabled{ before.has_value() };

	if (!ImGui::Checkbox("##IgnoreParent", &enabled)) {
		return false;
	}

	target.template SetLive<T>(enabled ? ComponentState<T>{ T{} } : std::nullopt);

	auto after{ target.template Capture<T>() };

	TrackComponentState(
		target, enabled ? "Ignore Parent" : "Inherit From Parent", std::move(before),
		std::move(after), true
	);

	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());
	}

	return true;
}


template <typename Target, typename T>
[[nodiscard]] bool HasFeatureComponent(const Target& target) {
	if constexpr (!Target::template Supports<T>()) {
		return false;
	} else {
		return target.template Capture<T>().has_value();
	}
}

template <typename Target, typename... T>
[[nodiscard]] bool HasAnyFeatureComponent(const Target& target, FeatureComponents<T...>) {
	return (HasFeatureComponent<Target, T>(target) || ...);
}

template <typename Target, typename... T>
[[nodiscard]] bool HasInspectorFeature(
	const Target& target, InspectorFeature feature, FeatureComponents<T...> components
) {
	return IsFeatureManuallyAdded(target.GetFeatureTargetKey(), feature) ||
		   HasAnyFeatureComponent(target, components);
}

template <typename Target>
[[nodiscard]] bool HasTransformFeature(const Target& target) {
	if (ArchetypeRequiresTransform(ResolveInspectorArchetype(target))) {
		return true;
	}
	return HasInspectorFeature(target, InspectorFeature::Transform, TransformFeatureComponents{});
}

template <typename Target>
[[nodiscard]] bool IsPrimarySceneRenderTarget(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };
		return entity && entity == entity.GetScene().GetRenderTarget();
	} else {
		return false;
	}
}

template <typename Target>
[[nodiscard]] bool IsReservedFixedCamera(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };
		return entity && entity == entity.GetScene().GetFixedCamera();
	} else {
		return false;
	}
}

template <typename Target>
[[nodiscard]] bool HasVisualFeature(const Target& target) {
	if (IsReservedFixedCamera(target)) {
		return false;
	}

	if (IsPrimarySceneRenderTarget(target)) {
		return true;
	}

	if (IsFeatureManuallyAdded(target.GetFeatureTargetKey(), InspectorFeature::Visual)) {
		return true;
	}

	return HasFeatureComponent<Target, ::ptgn::impl::IDrawable>(target);
}

template <typename Target>
[[nodiscard]] bool HasInteractionFeature(const Target& target) {
	if (IsFeatureManuallyAdded(target.GetFeatureTargetKey(), InspectorFeature::Interaction)) {
		return true;
	}

	const bool button_control{ HasFeatureComponent<Target, ::ptgn::impl::ButtonData>(target) };
	const bool slider_control{ HasFeatureComponent<Target, ::ptgn::impl::SliderData>(target) };
	const bool draggable{ HasFeatureComponent<Target, ::ptgn::impl::Draggable>(target) };
	const bool dropzone{ HasFeatureComponent<Target, ::ptgn::impl::Dropzone>(target) };
	const bool interactive_tag{ HasFeatureComponent<Target, ::ptgn::impl::InteractiveTag>(target) };

	// ButtonData owns ordinary interaction, and SliderData owns its draggable behavior. Keep the
	// generic Interaction feature hidden unless the user attached an additional interaction role.
	if (button_control && !dropzone && !interactive_tag && (!draggable || slider_control)) {
		return false;
	}

	const bool has_editable_component{
		HasFeatureComponent<Target, ::ptgn::impl::Interactive>(target) || draggable || dropzone ||
		interactive_tag
	};

	const bool has_visible_read_only_component{
		target.ctx.local.settings.show_read_only_inspector_data &&
		[&]() {
			if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
				return target.entity.template Has<InteractionLock>();
			} else if constexpr (Target::template Supports<InteractionLock>()) {
				return target.template Capture<InteractionLock>().has_value();
			} else {
				return false;
			}
		}()
	};

	return has_editable_component || has_visible_read_only_component;
}

template <typename Target>
[[nodiscard]] bool HasPhysicsFeature(const Target& target) {
	return HasInspectorFeature(target, InspectorFeature::Physics, PhysicsFeatureComponents{});
}

template <typename Target>
[[nodiscard]] bool HasUIFeature(const Target& target) {
	if constexpr (requires { target.entity; }) {
		if (target.entity && target.entity.template Has<::ptgn::impl::DialogueData>()) {
			return true;
		}
	}
	return HasInspectorFeature(target, InspectorFeature::UI, UIFeatureComponents{});
}

template <typename Target>
[[nodiscard]] bool HasCameraFeature(const Target& target) {
	if (IsPrimarySceneRenderTarget(target)) {
		return false;
	}

	return HasInspectorFeature(target, InspectorFeature::Camera, CameraFeatureComponents{});
}

template <typename Target>
[[nodiscard]] bool HasScriptsFeature(const Target& target) {
	return HasInspectorFeature(target, InspectorFeature::Scripts, ScriptsFeatureComponents{});
}

template <typename Target>
[[nodiscard]] bool HasUtilitiesFeature(const Target& target) {
	return HasInspectorFeature(target, InspectorFeature::Utilities, UtilitiesFeatureComponents{});
}



// Non-template feature entry points. The implementations stay in their feature-specific .cpp files.
bool DrawTransformFeature(
	EntityInspectorTarget& target, bool draw_header = true, bool redirect_button_part = true,
	bool draw_inline_separator = true
);
bool DrawTransformFeature(
	PrefabInspectorTarget& target, bool draw_header = true, bool redirect_button_part = true,
	bool draw_inline_separator = true
);

bool AddSpriteAnimationFeature(EntityInspectorTarget& target);
bool AddSpriteAnimationFeature(PrefabInspectorTarget& target);
bool DrawSpriteAnimationFeature(EntityInspectorTarget& target);
bool DrawSpriteAnimationFeature(PrefabInspectorTarget& target);

bool DrawVisualFeature(
	EntityInspectorTarget& target, bool draw_header = true, bool allow_renderer_change = true,
	std::string_view header_label = "Visual"
);
bool DrawVisualFeature(
	PrefabInspectorTarget& target, bool draw_header = true, bool allow_renderer_change = true,
	std::string_view header_label = "Visual"
);

bool DrawButtonChildStateTransformFeature(
	EntityInspectorTarget& target, const ButtonChildInfo& child_info, ButtonVisualState state
);
bool DrawButtonChildStateVisualFeature(
	EntityInspectorTarget& target, const ButtonChildInfo& child_info, ButtonVisualState state
);
bool DrawSpritePrimary(EntityInspectorTarget& target);

bool DrawInteractionFeature(EntityInspectorTarget& target);
bool DrawInteractionFeature(PrefabInspectorTarget& target);

bool DrawPhysicsFeature(EntityInspectorTarget& target);
bool DrawPhysicsFeature(PrefabInspectorTarget& target);

bool DrawUIFeature(EntityInspectorTarget& target);
bool DrawUIFeature(PrefabInspectorTarget& target);

bool DrawCameraFeature(EntityInspectorTarget& target);
bool DrawCameraFeature(PrefabInspectorTarget& target);

bool DrawScriptsFeature(EntityInspectorTarget& target);
bool DrawScriptsFeature(PrefabInspectorTarget& target);

bool DrawUtilitiesFeature(EntityInspectorTarget& target);
bool DrawUtilitiesFeature(PrefabInspectorTarget& target);

bool DrawFeatureInspector(EntityInspectorTarget& target);
bool DrawFeatureInspector(PrefabInspectorTarget& target);

} // namespace ptgn::editor::inspector
