#include "panels/inspector_internal.h"

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

void DrawDisabledWrappedText(std::string_view text) {
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
	EntityFilterEditorState grounding_filter_state{};
};

std::vector<ManualFeatureState>& ManualFeatureStates() {
	static std::vector<ManualFeatureState> states;
	return states;
}

[[nodiscard]] bool SameManualFeatureTarget(
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

ManualFeatureState& GetManualFeatureState(const FeatureTargetKey& target) {
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

[[nodiscard]] Entity FindButtonPart(Entity button, ButtonChildPart part);

[[nodiscard]] bool IsButtonRoot(Entity entity) {
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

[[nodiscard]] std::span<const ButtonVisualState> GetButtonVisualStateFallbacks(
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

std::string NormalizeFeatureName(std::string_view input) {
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
	ButtonVisualState state, std::optional<T> Visual::* member
) {
	auto& visual{ states[static_cast<std::size_t>(std::to_underlying(state))] };
	auto& value{ visual.*member };
	const bool had_override{ value.has_value() };
	const std::optional<T> inherited{ ResolveButtonVisualProperty(states, state, member) };

	// Size overrides use a label-side checkbox. Keeping the checkbox in the label
	// column prevents the controls from jumping when the override is enabled.
	const std::string normalized_label{ NormalizeFeatureName(label) };
	if (normalized_label == "size" || normalized_label == "texturesize") {
		if constexpr (std::same_as<T, V2_float>) {
			ScopedID value_scope{ std::addressof(value) };
			const bool was_enabled{ value.has_value() };
			bool enabled{ was_enabled };
			V2_float displayed{ value.value_or(inherited.value_or(V2_float{})) };
			bool field_changed{ false };

			bool changed{ DrawOptionalPropertyRow(label, enabled, false, [&]() {
				if (!enabled) {
					ImGui::BeginDisabled();
					DrawUnsetOptionalInlineValue();
					ImGui::EndDisabled();
					return false;
				}

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
				if (!enabled) {
					ImGui::BeginDisabled();
					DrawUnsetOptionalInlineValue();
					ImGui::EndDisabled();
					return false;
				}

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

	ScopedID value_scope{ std::addressof(value) };

	if (ImGui::Checkbox("##Enabled", &enabled)) {
		if (enabled) {
			value = displayed;
		} else {
			value.reset();
		}
		changed = true;
	}

	ImGui::SameLine();
	const std::string tree_label{ std::string{ label } + "##ButtonVisualOverrideTree" };
	const bool open{ ImGui::TreeNodeEx(
		tree_label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
	) };

	if (open) {
		ScopedIndent indent;
		ScopedPropertyLabelOffset label_offset{ ImGui::GetStyle().IndentSpacing };
		ScopedDisabled disabled{ !enabled };

		T edited{ enabled ? value.value() : displayed };
		const bool contents_changed{ std::invoke(std::forward<Draw>(draw), edited) };

		if (enabled && contents_changed) {
			value	= std::move(edited);
			changed = true;
		}

		ImGui::TreePop();
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
	Entity button_entity, std::array<ButtonShapeVisual, N>& states, ButtonVisualState state
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
		if (!enabled) {
			DrawUnsetOptionalInlineValue();
			return false;
		}
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


[[nodiscard]] bool IsFeatureManuallyAdded(
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

void SetFeatureManuallyAdded(const FeatureTargetKey& target, InspectorFeature feature, bool added) {
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

	const std::string header_label{ std::string{ label } + "##FeatureHeader" };

	FeatureHeaderResult result{
		.open = ImGui::CollapsingHeader(header_label.c_str(), flags),
	};

	if (allow_delete && ImGui::BeginPopupContextItem("##FeatureContext")) {
		if (ImGui::MenuItem("Delete Feature")) {
			result.changed = DeleteInspectorFeature(target, feature, label, components);
			result.open	   = false;
		}

		ImGui::EndPopup();
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

template <typename Target>
struct TransformFeatureState {
	Transform transform{};
	Depth depth{};
	ComponentState<ButtonBackgroundVisuals> button_backgrounds{};
	ComponentState<ButtonBorderVisuals> button_borders{};
	ComponentState<ButtonTextVisuals> button_texts{};
	ComponentState<ButtonSpriteVisuals> button_sprites{};
	bool ignore_position{ false };
	bool ignore_rotation{ false };
	bool ignore_scale{ false };
	bool ignore_depth{ false };
	bool ignore_transform{ false };
};

template <typename Target>
TransformFeatureState<Target> CaptureTransformFeature(const Target& target) {
	const bool ignore_transform{
		target.template Capture<::ptgn::impl::IgnoreParentTransform>().has_value()
	};

	return TransformFeatureState<Target>{
		.transform			= target.template Capture<Transform>().value_or(Transform{}),
		.depth				= target.template Capture<Depth>().value_or(Depth{}),
		.button_backgrounds = target.template Capture<ButtonBackgroundVisuals>(),
		.button_borders		= target.template Capture<ButtonBorderVisuals>(),
		.button_texts		= target.template Capture<ButtonTextVisuals>(),
		.button_sprites		= target.template Capture<ButtonSpriteVisuals>(),
		.ignore_position =
			ignore_transform ||
			target.template Capture<::ptgn::impl::IgnoreParentPosition>().has_value(),
		.ignore_rotation =
			ignore_transform ||
			target.template Capture<::ptgn::impl::IgnoreParentRotation>().has_value(),
		.ignore_scale	  = ignore_transform ||
							target.template Capture<::ptgn::impl::IgnoreParentScale>().has_value(),
		.ignore_depth	  = ignore_transform ||
							target.template Capture<::ptgn::impl::IgnoreParentDepth>().has_value(),
		.ignore_transform = ignore_transform,
	};
}

template <typename Target>
auto MakeTransformFeatureApply(Target& target) {
	auto apply_transform{ target.template MakeApply<Transform>() };
	auto apply_depth{ target.template MakeApply<Depth>() };
	auto apply_position{ target.template MakeApply<::ptgn::impl::IgnoreParentPosition>() };
	auto apply_rotation{ target.template MakeApply<::ptgn::impl::IgnoreParentRotation>() };
	auto apply_scale{ target.template MakeApply<::ptgn::impl::IgnoreParentScale>() };
	auto apply_depth_ignore{ target.template MakeApply<::ptgn::impl::IgnoreParentDepth>() };
	auto apply_transform_ignore{ target.template MakeApply<::ptgn::impl::IgnoreParentTransform>() };
	auto apply_backgrounds{
		target.template MakeApply<ButtonBackgroundVisuals>(&MarkButtonBackgroundDirty)
	};
	auto apply_borders{ target.template MakeApply<ButtonBorderVisuals>(&MarkButtonBorderDirty) };
	auto apply_texts{ target.template MakeApply<ButtonTextVisuals>(&MarkButtonTextDirty) };
	auto apply_sprites{ target.template MakeApply<ButtonSpriteVisuals>(&MarkButtonSpriteDirty) };

	return [apply_transform, apply_depth, apply_position, apply_rotation, apply_scale,
			apply_depth_ignore, apply_transform_ignore, apply_backgrounds, apply_borders,
			apply_texts, apply_sprites](TransformFeatureState<Target> state) mutable {
		apply_backgrounds(state.button_backgrounds);
		apply_borders(state.button_borders);
		apply_texts(state.button_texts);
		apply_sprites(state.button_sprites);
		apply_transform(state.transform);
		apply_depth(state.depth);
		apply_transform_ignore(
			state.ignore_transform
				? ComponentState<
					  ::ptgn::impl::IgnoreParentTransform>{ ::ptgn::impl::IgnoreParentTransform{} }
				: std::nullopt
		);
		apply_position(
			state.ignore_position
				? ComponentState<
					  ::ptgn::impl::IgnoreParentPosition>{ ::ptgn::impl::IgnoreParentPosition{} }
				: std::nullopt
		);
		apply_rotation(
			state.ignore_rotation
				? ComponentState<
					  ::ptgn::impl::IgnoreParentRotation>{ ::ptgn::impl::IgnoreParentRotation{} }
				: std::nullopt
		);
		apply_scale(
			state.ignore_scale
				? ComponentState<
					  ::ptgn::impl::IgnoreParentScale>{ ::ptgn::impl::IgnoreParentScale{} }
				: std::nullopt
		);
		apply_depth_ignore(
			state.ignore_depth
				? ComponentState<
					  ::ptgn::impl::IgnoreParentDepth>{ ::ptgn::impl::IgnoreParentDepth{} }
				: std::nullopt
		);
	};
}

template <typename Target>
void SetTransformFeatureLive(Target& target, const TransformFeatureState<Target>& state) {
	target.template SetLive<ButtonBackgroundVisuals>(
		state.button_backgrounds, &MarkButtonBackgroundDirty
	);
	target.template SetLive<ButtonBorderVisuals>(state.button_borders, &MarkButtonBorderDirty);
	target.template SetLive<ButtonTextVisuals>(state.button_texts, &MarkButtonTextDirty);
	target.template SetLive<ButtonSpriteVisuals>(state.button_sprites, &MarkButtonSpriteDirty);
	target.template SetLive<Transform>(state.transform);
	target.template SetLive<Depth>(state.depth);
	target.template SetLive<::ptgn::impl::IgnoreParentTransform>(
		state.ignore_transform
			? ComponentState<
				  ::ptgn::impl::IgnoreParentTransform>{ ::ptgn::impl::IgnoreParentTransform{} }
			: std::nullopt
	);
	target.template SetLive<::ptgn::impl::IgnoreParentPosition>(
		state.ignore_position
			? ComponentState<
				  ::ptgn::impl::IgnoreParentPosition>{ ::ptgn::impl::IgnoreParentPosition{} }
			: std::nullopt
	);
	target.template SetLive<::ptgn::impl::IgnoreParentRotation>(
		state.ignore_rotation
			? ComponentState<
				  ::ptgn::impl::IgnoreParentRotation>{ ::ptgn::impl::IgnoreParentRotation{} }
			: std::nullopt
	);
	target.template SetLive<::ptgn::impl::IgnoreParentScale>(
		state.ignore_scale
			? ComponentState<::ptgn::impl::IgnoreParentScale>{ ::ptgn::impl::IgnoreParentScale{} }
			: std::nullopt
	);
	target.template SetLive<::ptgn::impl::IgnoreParentDepth>(
		state.ignore_depth
			? ComponentState<::ptgn::impl::IgnoreParentDepth>{ ::ptgn::impl::IgnoreParentDepth{} }
			: std::nullopt
	);
}

template <typename Visuals>
void ApplyButtonVisualTransformDelta(
	ComponentState<Visuals>& visuals, std::optional<ButtonVisualState> selected_state,
	const Transform& before, const Transform& after
) {
	if (!visuals || !selected_state || before == after) {
		return;
	}

	const auto index{ static_cast<std::size_t>(std::to_underlying(*selected_state)) };
	auto& visual{ visuals->states[index] };

	if (!visual.transform) {
		return;
	}

	auto& transform{ *visual.transform };
	transform.position += after.position - before.position;
	transform.rotation =
		Radians{ transform.rotation.value + after.rotation.value - before.rotation.value };

	constexpr float epsilon{ 0.000001f };

	if (std::abs(before.scale.x) > epsilon) {
		transform.scale.x *= after.scale.x / before.scale.x;
	}

	if (std::abs(before.scale.y) > epsilon) {
		transform.scale.y *= after.scale.y / before.scale.y;
	}

	transform.ClampScale();
}

template <typename Target>
void ApplyButtonVisualTransformDelta(
	TransformFeatureState<Target>& state, std::optional<ButtonVisualState> selected_state,
	const Transform& before
) {
	ApplyButtonVisualTransformDelta(
		state.button_backgrounds, selected_state, before, state.transform
	);
	ApplyButtonVisualTransformDelta(state.button_borders, selected_state, before, state.transform);
	ApplyButtonVisualTransformDelta(state.button_texts, selected_state, before, state.transform);
	ApplyButtonVisualTransformDelta(state.button_sprites, selected_state, before, state.transform);
}

template <typename Target>
[[nodiscard]] std::optional<V2_float> GetTargetWorldReferencePosition(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		if (entity && entity.Has<Transform>()) {
			return GetDrawTransform(entity).position;
		}
	}

	return std::nullopt;
}

template <typename Target>
[[nodiscard]] std::optional<V2_float> GetTargetWorldReferencePosition(
	const Target& target, V2_float local_position
) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		if (entity && entity.Has<Transform>()) {
			return GetDrawTransform(entity).Apply(local_position);
		}
	}

	return std::nullopt;
}

template <typename Target>
[[nodiscard]] PositionPicker::Convert MakeTransformPositionConverter(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		return [entity](V2_float world_position) mutable -> std::optional<V2_float> {
			if (!entity || !entity.Has<Transform>()) {
				return std::nullopt;
			}

			const bool ignores_parent_position{ entity.Has<::ptgn::impl::IgnoreParentTransform>() ||
												entity.Has<::ptgn::impl::IgnoreParentPosition>() };

			Entity parent{ GetParent(entity) };

			if (!parent || ignores_parent_position) {
				return world_position;
			}

			return GetDrawTransform(parent).ApplyInverse(world_position);
		};
	} else {
		return [](V2_float) -> std::optional<V2_float> {
			return std::nullopt;
		};
	}
}

template <typename Target>
[[nodiscard]] bool ShouldShowTransformRelativePosition(const Target& target) {
	if constexpr (!requires { target.entity; }) {
		return false;
	} else {
		Entity entity{ target.entity };

		if (!entity) {
			return false;
		}

		const bool ignores_parent_position{ entity.Has<::ptgn::impl::IgnoreParentTransform>() ||
											entity.Has<::ptgn::impl::IgnoreParentPosition>() };

		return !ignores_parent_position && static_cast<bool>(GetParent(entity));
	}
}

template <typename Target, typename Apply>
bool DrawTransformFeatureFields(
	Target& target, TransformFeatureState<Target>& state, Apply apply_state
) {
	constexpr ImGuiTableFlags flags{ ImGuiTableFlags_SizingStretchProp |
									 ImGuiTableFlags_NoSavedSettings |
									 ImGuiTableFlags_NoPadOuterX };

	if (!ImGui::BeginTable("##TransformFields", 5, flags)) {
		return false;
	}

	const float compact_width{ ImGui::GetFrameHeight() };
	const float pick_width{ ImGui::CalcTextSize("Pick").x +
							ImGui::GetStyle().FramePadding.x * 2.0f };

	ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 76.0f);
	ImGui::TableSetupColumn("Lock", ImGuiTableColumnFlags_WidthFixed, compact_width);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.0f);
	ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, pick_width);
	ImGui::TableSetupColumn("Inheritance", ImGuiTableColumnFlags_WidthFixed, compact_width);

	bool changed{ false };
	auto& editor_state{ GetManualFeatureState(target.GetFeatureTargetKey()) };

	auto begin_row = [](const char* id, const char* label) {
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
		ImGui::PushID(id);
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
	};

	auto draw_ignore = [](bool& value, const char* tooltip) {
		ImGui::TableSetColumnIndex(4);
		const bool local_changed{ ImGui::Checkbox("##IgnoreParent", &value) };
		DrawTooltip(tooltip);
		return local_changed;
	};

	begin_row("Position", "Position");
	ImGui::TableSetColumnIndex(2);
	{
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float available{ ImGui::GetContentRegionAvail().x };
		const float field_width{ std::max(36.0f, (available - spacing) * 0.5f) };

		ImGui::SetNextItemWidth(field_width);
		changed |=
			ImGui::DragFloat("##X", &state.transform.position.x, 1.0f, 0.0f, 0.0f, "X: %.0f");
		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(field_width);
		changed |=
			ImGui::DragFloat("##Y", &state.transform.position.y, 1.0f, 0.0f, 0.0f, "Y: %.0f");
	}
	ImGui::TableSetColumnIndex(3);
	const auto selected_button_state{ GetButtonVisualEditState(target) };
	DrawPositionPickButton(
		target.ctx, "Position", state.transform.position, MakeTransformPositionConverter(target),
		PositionPicker::Apply{
			[apply_state, state, selected_button_state](V2_float picked) mutable {
				const Transform before_transform{ state.transform };
				state.transform.position = picked;
				ApplyButtonVisualTransformDelta(state, selected_button_state, before_transform);
				apply_state(state);
			} },
		GetTargetWorldReferencePosition(target), ShouldShowTransformRelativePosition(target)
	);
	changed |= draw_ignore(state.ignore_position, "Ignore parent position.");
	ImGui::PopID();

	begin_row("Depth", "Depth");
	ImGui::TableSetColumnIndex(2);
	ImGui::SetNextItemWidth(-FLT_MIN);
	changed |= ImGui::DragFloat(
		"##Value", &state.depth.value, 0.05f, -1000.0f, 1000.0f, "%.2f",
		ImGuiSliderFlags_AlwaysClamp
	);
	changed |= draw_ignore(state.ignore_depth, "Ignore parent depth.");
	ImGui::PopID();

	begin_row("Rotation", "Rotation");
	ImGui::TableSetColumnIndex(2);
	ImGui::SetNextItemWidth(-FLT_MIN);

	Degrees rotation{ state.transform.rotation };
	float degrees{ rotation.value };

	if (ImGui::DragFloat(
			"##Value", &degrees, 1.0f, 0.0f, 360.0f, "%.1f deg", ImGuiSliderFlags_AlwaysClamp
		)) {
		state.transform.rotation = Radians{ Degrees{ degrees } };
		changed					 = true;
	}

	changed |= draw_ignore(state.ignore_rotation, "Ignore parent rotation.");

	ImGui::PopID();

	begin_row("Scale", "Scale");
	ImGui::TableSetColumnIndex(1);
	ImGui::Checkbox("##LockRatio", &editor_state.scale_ratio_locked);
	DrawTooltip(
		"Lock the scale ratio. Editing either axis changes the other by the same proportional "
		"factor."
	);

	ImGui::TableSetColumnIndex(2);
	{
		constexpr float kMinScaleMagnitude{ 0.001f };
		constexpr float kScaleRatioEpsilon{ 0.000001f };

		const V2_float before_scale{ state.transform.scale };

		const auto clamp_scale{ [](float value, float previous) {
			if (std::abs(value) >= kMinScaleMagnitude) {
				return value;
			}

			const float sign{ value < 0.0f || (value == 0.0f && previous < 0.0f) ? -1.0f : 1.0f };
			return sign * kMinScaleMagnitude;
		} };

		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float available{ ImGui::GetContentRegionAvail().x };
		const float field_width{ std::max(36.0f, (available - spacing) * 0.5f) };

		ImGui::SetNextItemWidth(field_width);
		const bool x_changed{ ImGui::DragFloat(
			"##X", &state.transform.scale.x, 0.01f, -1000.0f, 1000.0f, "X: %.2f",
			ImGuiSliderFlags_AlwaysClamp
		) };

		ImGui::SameLine(0.0f, spacing);

		ImGui::SetNextItemWidth(field_width);
		const bool y_changed{ ImGui::DragFloat(
			"##Y", &state.transform.scale.y, 0.01f, -1000.0f, 1000.0f, "Y: %.2f",
			ImGuiSliderFlags_AlwaysClamp
		) };

		if (x_changed) {
			state.transform.scale.x = clamp_scale(state.transform.scale.x, before_scale.x);
		}
		if (y_changed) {
			state.transform.scale.y = clamp_scale(state.transform.scale.y, before_scale.y);
		}

		if (editor_state.scale_ratio_locked) {
			if (x_changed && !y_changed) {
				if (std::abs(before_scale.x) > kScaleRatioEpsilon) {
					state.transform.scale.y =
						before_scale.y * (state.transform.scale.x / before_scale.x);
				} else if (std::abs(before_scale.y) <= kScaleRatioEpsilon) {
					state.transform.scale.y = state.transform.scale.x;
				}
			} else if (y_changed && !x_changed) {
				if (std::abs(before_scale.y) > kScaleRatioEpsilon) {
					state.transform.scale.x =
						before_scale.x * (state.transform.scale.y / before_scale.y);
				} else if (std::abs(before_scale.x) <= kScaleRatioEpsilon) {
					state.transform.scale.x = state.transform.scale.y;
				}
			}
		}

		state.transform.scale.x = clamp_scale(state.transform.scale.x, before_scale.x);
		state.transform.scale.y = clamp_scale(state.transform.scale.y, before_scale.y);

		changed |= x_changed || y_changed;
	}
	changed |= draw_ignore(state.ignore_scale, "Ignore parent scale.");
	ImGui::PopID();

	ImGui::EndTable();
	return changed;
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
		if (target.entity && target.entity.template Has<DialogueData>()) {
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


template <typename Target>
bool DrawTransformFeature(
	Target& target, bool draw_header = true, bool redirect_button_part = true,
	bool draw_inline_separator = true
);

template <typename Target, typename Visuals, typename Visual, typename Callback>
bool DrawButtonChildStateTransformComponent(
	Target& target, const ButtonChildInfo& child_info, ButtonVisualState state,
	Origin fallback_anchor, std::string_view part_label, Callback callback
) {
	if constexpr (!Target::template Supports<Visuals>()) {
		return false;
	} else {
		(void)child_info;
		(void)fallback_anchor;

		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<Visuals>()) };

		auto before{ target.template Capture<Visuals>() };
		Visuals visuals{ before.value_or(Visuals{}) };
		const auto index{ static_cast<std::size_t>(std::to_underlying(state)) };
		auto& visual{ visuals.states[index] };
		const Transform resolved_transform{
			ResolveButtonVisualProperty(visuals.states, state, &Visual::transform)
				.value_or(Transform{})
		};

		const bool was_enabled{ visual.transform.has_value() };
		bool enabled{ was_enabled };
		bool changed{ false };

		if (ImGui::Checkbox("##SetTransform", &enabled)) {
			if (enabled) {
				visual.transform = resolved_transform;
			} else {
				visual.transform.reset();
			}
			// Transform is only an optional property of an enabled part. Toggling it must not
			// toggle the part itself.
			visual.defined = true;
			target.template SetLive<Visuals>(ComponentState<Visuals>{ visuals }, callback);
			auto after{ target.template Capture<Visuals>() };
			TrackComponentState(
				target, std::string{ enabled ? "Enable " : "Disable " } +
					std::string{ part_label } + " Transform",
				std::move(before), std::move(after), true, callback
			);
			changed = true;
		}

		ImGui::SameLine();
		ImGui::BeginDisabled(!enabled);
		const bool open{ ImGui::TreeNodeEx(
			"Transform##ButtonVisualStateTransform",
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
		) };
		ImGui::EndDisabled();
		DrawTooltip("Override this state's transform, or leave it unchecked to inherit.");

		if (open) {
			if (enabled) {
				// Reuse the ordinary Transform feature so managed button parts get the exact same
				// position picker, scale-ratio lock, depth, and ignore-parent controls as entities.
				auto& editor_state{ GetManualFeatureState(target.GetFeatureTargetKey()) };
				const auto previous_state{ editor_state.button_visual_state };
				editor_state.button_visual_state = state;
				changed |= DrawTransformFeature(target, false, false, false);
				editor_state.button_visual_state = previous_state;
			}
			ImGui::TreePop();
		}

		return changed;
	}
}

template <typename Target>
bool DrawButtonChildStateTransformFeature(
	Target& target, const ButtonChildInfo& child_info, ButtonVisualState state
) {
	switch (child_info.part) {
		case ButtonChildPart::Background:
			return DrawButtonChildStateTransformComponent<
				Target, ButtonBackgroundVisuals, ButtonShapeVisual>(
				target, child_info, state, child_info.button.GetOrDefault<Origin>(),
				"Button Background", &MarkButtonBackgroundDirty
			);
		case ButtonChildPart::Border:
			return DrawButtonChildStateTransformComponent<
				Target, ButtonBorderVisuals, ButtonShapeVisual>(
				target, child_info, state, child_info.button.GetOrDefault<Origin>(),
				"Button Border", &MarkButtonBorderDirty
			);
		case ButtonChildPart::Text:
			return DrawButtonChildStateTransformComponent<
				Target, ButtonTextVisuals, ButtonTextVisual>(
				target, child_info, state, Origin::Center, "Button Text", &MarkButtonTextDirty
			);
		case ButtonChildPart::Sprite:
			return DrawButtonChildStateTransformComponent<
				Target, ButtonSpriteVisuals, ButtonSpriteVisual>(
				target, child_info, state, child_info.button.GetOrDefault<Origin>(),
				"Button Sprite", &MarkButtonSpriteDirty
			);
	}

	return false;
}

template <typename Target>
bool DrawTransformFeature(
	Target& target, bool draw_header, bool redirect_button_part, bool draw_inline_separator
) {
	if (redirect_button_part) {
		if (const auto child_info{ GetButtonChildInfo(target) }) {
			if (const auto state{ GetButtonVisualEditState(target) }) {
				return DrawButtonChildStateTransformFeature(target, *child_info, *state);
			}
		}
	}

	if (!HasTransformFeature(target)) {
		return false;
	}

	FeatureHeaderResult header{
		.open	 = true,
		.changed = false,
	};

	if (draw_header) {
		header = DrawFeatureHeader(
			target, InspectorFeature::Transform, "Transform", ImGuiTreeNodeFlags_DefaultOpen,
			TransformFeatureComponents{}
		);

		if (!header.open) {
			return header.changed;
		}
	} else if (draw_inline_separator) {
		ImGui::SeparatorText("Transform");
	}

	std::optional<ScopedIndent> feature_indent;
	if (draw_header) {
		feature_indent.emplace();
	}

	const auto before{ CaptureTransformFeature(target) };
	auto state{ before };
	auto apply{ MakeTransformFeatureApply(target) };
	bool changed{ header.changed };

	changed |= DrawTransformFeatureFields(target, state, apply);

	if (changed) {
		state.ignore_transform = false;
		state.transform.ClampScale();
		ApplyButtonVisualTransformDelta(state, GetButtonVisualEditState(target), before.transform);
		SetTransformFeatureLive(target, state);
	}

	if (changed) {
		ScopedID target_scope{ target.Id() };
		const ImGuiID key{ ImGui::GetID("##TransformFeature") };

		TrackUndoableInteraction(
			target.ctx, key, "Edit Transform", true, [apply, before]() mutable { apply(before); },
			[apply, state]() mutable { apply(state); }
		);
	}

	return changed;
}

template <typename Target>
[[nodiscard]] PositionPicker::Convert MakeLocalPositionConverter(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		return [entity](V2_float world_position) mutable -> std::optional<V2_float> {
			if (!entity || !entity.Has<Transform>()) {
				return std::nullopt;
			}

			return GetDrawTransform(entity).ApplyInverse(world_position);
		};
	} else {
		return [](V2_float) -> std::optional<V2_float> {
			return std::nullopt;
		};
	}
}

template <typename Target>
[[nodiscard]] constexpr bool CanPickLocalPosition() {
	return requires(Target target) { target.entity; };
}

template <typename Target, typename Component, typename Locator, typename Callback>
bool DrawPickableLocalPosition(
	Target& target, Component& component, std::string_view label, Locator locator,
	Callback callback, bool* remove_requested = nullptr
) {
	V2_float& position{ locator(component) };

	const bool changed{ DrawPropertyRow(label, [&]() {
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float pick_width{ ImGui::CalcTextSize("Pick").x +
								ImGui::GetStyle().FramePadding.x * 2.0f };
		const float remove_width{ remove_requested ? ImGui::GetFrameHeight() : 0.0f };
		const float remove_spacing{ remove_requested ? spacing : 0.0f };
		const float available{ ImGui::GetContentRegionAvail().x };
		const float field_width{ std::max(
			36.0f, (available - pick_width - remove_width - remove_spacing - spacing * 2.0f) * 0.5f
		) };

		bool local_changed{ false };

		ImGui::SetNextItemWidth(field_width);
		local_changed |= ImGui::DragFloat(
			"##X", &position.x, kInspectorPositionDragSpeed, 0.0f, 0.0f, "X: %.2f"
		);

		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(field_width);
		local_changed |= ImGui::DragFloat(
			"##Y", &position.y, kInspectorPositionDragSpeed, 0.0f, 0.0f, "Y: %.2f"
		);

		ImGui::SameLine(0.0f, spacing);

		{
			ScopedDisabled disabled{ !CanPickLocalPosition<Target>() };

			auto apply{ target.template MakeApply<Component>(callback) };
			Component snapshot{ component };

			DrawPositionPickButton(
				target.ctx, label, position, MakeLocalPositionConverter(target),
				[apply, snapshot = std::move(snapshot), locator](V2_float picked) mutable {
					locator(snapshot) = picked;
					apply(ComponentState<Component>{ snapshot });
				},
				GetTargetWorldReferencePosition(target, position), true
			);

			if constexpr (!CanPickLocalPosition<Target>()) {
				DrawTooltip("Position picking is available for scene entities.");
			}
		}

		if (remove_requested) {
			ImGui::SameLine(0.0f, spacing);
			if (ImGui::Button("-", ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() })) {
				*remove_requested = true;
			}
			DrawTooltip("Delete this vertex.");
		}

		return local_changed;
	}) };

	return changed;
}

template <typename Target, typename Component, typename Value, typename Locator, typename Callback>
bool DrawGeometryValue(
	Target& target, Component& component, Value& value, std::string_view label, Locator locator,
	Callback callback
);

template <
	std::size_t I, typename Target, typename Component, typename Parent, typename Locator,
	typename Callback>
bool DrawReflectedGeometryMember(
	Target& target, Component& component, Parent& parent, Locator locator, Callback callback
) {
	auto members{ ReflectMembers(parent) };
	auto& member{ std::get<I>(members) };

	auto member_locator = [locator](Component& root) -> decltype(auto) {
		auto reflected{ ReflectMembers(locator(root)) };
		return std::get<I>(reflected).value;
	};

	return DrawGeometryValue(
		target, component, member.value, PrettyName(member.name), member_locator, callback
	);
}

template <
	typename Target, typename Component, typename Parent, typename Locator, typename Callback,
	std::size_t... I>
bool DrawReflectedGeometryMembers(
	Target& target, Component& component, Parent& parent, Locator locator, Callback callback,
	std::index_sequence<I...>
) {
	bool changed{ false };
	((changed |= DrawReflectedGeometryMember<I>(target, component, parent, locator, callback)),
	 ...);
	return changed;
}

template <
	typename Target, typename Component, typename Variant, typename Locator, typename Callback,
	std::size_t... I>
bool DrawGeometryVariant(
	Target& target, Component& component, Variant& value, std::string_view label, Locator locator,
	Callback callback, std::index_sequence<I...>
) {
	ScopedID variant_scope{ std::addressof(value) };
	bool changed{ false };
	std::string preview{ "None" };
	std::optional<std::size_t> requested_index;

	auto update_preview = [&]<std::size_t Index>() {
		if (value.index() != Index) {
			return;
		}

		using Alternative = std::variant_alternative_t<Index, Variant>;

		if constexpr (!std::same_as<Alternative, std::monostate>) {
			preview = VariantTypeLabel<Alternative>();
		}
	};

	(update_preview.template operator()<I>(), ...);

	changed |= DrawPropertyRow(label, [&]() {
		bool local_changed{ false };

		if (ImGui::BeginCombo("##value", preview.c_str())) {
			auto draw_option = [&]<std::size_t Index>() {
				using Alternative = std::variant_alternative_t<Index, Variant>;

				const std::string option{ std::same_as<Alternative, std::monostate>
											  ? "None"
											  : VariantTypeLabel<Alternative>() };

				if constexpr (std::default_initializable<Alternative>) {
					const bool selected{ value.index() == Index };

					if (ImGui::Selectable(option.c_str(), selected) && !selected) {
						requested_index = Index;
						local_changed	= true;
					}
				}
			};

			(draw_option.template operator()<I>(), ...);
			ImGui::EndCombo();
		}

		return local_changed;
	});

	if (requested_index) {
		auto apply_requested = [&]<std::size_t Index>() {
			if (*requested_index == Index) {
				value.template emplace<Index>();
			}
		};

		(apply_requested.template operator()<I>(), ...);
		return true;
	}

	auto draw_selected = [&]<std::size_t Index>() {
		if (value.index() != Index) {
			return;
		}

		using Alternative = std::variant_alternative_t<Index, Variant>;

		if constexpr (!std::same_as<Alternative, std::monostate>) {
			auto alternative_locator = [locator](Component& root) -> Alternative& {
				return std::get<Index>(locator(root));
			};

			changed |= DrawGeometryValue(
				target, component, std::get<Index>(value), VariantTypeLabel<Alternative>(),
				alternative_locator, callback
			);
		}
	};

	(draw_selected.template operator()<I>(), ...);
	return changed;
}

template <typename Mask>
	requires std::integral<Mask>
bool DrawColliderMaskList(std::vector<Mask>& masks) {
	bool changed{ false };
	std::optional<std::size_t> remove_index;
	std::optional<std::pair<std::size_t, std::size_t>> move;

	ImGui::SeparatorText("Collides with Masks");

	if (ImGui::Button("+ Mask", ImVec2{ -FLT_MIN, 0.0f })) {
		masks.emplace_back();
		changed = true;
	}

	for (std::size_t index{ 0 }; index < masks.size(); ++index) {
		ScopedID item_scope{ static_cast<int>(index) };

		int displayed{ 0 };
		if constexpr (std::signed_integral<Mask>) {
			displayed = static_cast<int>(
				std::clamp<long long>(static_cast<long long>(masks[index]), 0, 64)
			);
		} else {
			displayed = static_cast<int>(
				std::min<std::uint64_t>(static_cast<std::uint64_t>(masks[index]), 64)
			);
		}

		const std::string label{ "Mask " + std::to_string(index + 1) };

		changed |= DrawPropertyRow(label, [&]() {
			bool local_changed{ false };
			const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
			const float button_width{ ImGui::GetFrameHeight() };
			const float actions_width{ button_width * 3.0f + spacing * 3.0f };
			const float field_width{
				std::max(36.0f, ImGui::GetContentRegionAvail().x - actions_width)
			};

			ImGui::SetNextItemWidth(field_width);
			if (ImGui::DragInt(
					"##value", &displayed, 1.0f, 0, 64, "%d", ImGuiSliderFlags_AlwaysClamp
				)) {
				masks[index]  = static_cast<Mask>(std::clamp(displayed, 0, 64));
				local_changed = true;
			}

			ImGui::SameLine(0.0f, spacing);
			{
				ScopedDisabled disabled{ index == 0 };
				if (ImGui::ArrowButton("##up", ImGuiDir_Up)) {
					move = std::pair{ index, index - 1 };
				}
			}

			ImGui::SameLine(0.0f, spacing);
			{
				ScopedDisabled disabled{ index + 1 >= masks.size() };
				if (ImGui::ArrowButton("##down", ImGuiDir_Down)) {
					move = std::pair{ index, index + 1 };
				}
			}

			ImGui::SameLine(0.0f, spacing);
			if (ImGui::Button("X##remove", ImVec2{ button_width, button_width })) {
				remove_index = index;
			}

			return local_changed;
		});
	}

	if (move) {
		std::ranges::iter_swap(
			masks.begin() + static_cast<std::ptrdiff_t>(move->first),
			masks.begin() + static_cast<std::ptrdiff_t>(move->second)
		);
		changed = true;
	} else if (remove_index) {
		masks.erase(masks.begin() + static_cast<std::ptrdiff_t>(*remove_index));
		changed = true;
	}

	return changed;
}

template <typename Target, typename Component, typename Value, typename Locator, typename Callback>
bool DrawGeometryValue(
	Target& target, Component& component, Value& value, std::string_view label, Locator locator,
	Callback callback
) {
	using Type = std::remove_cvref_t<Value>;

	if constexpr (std::same_as<Type, V2_float>) {
		const std::string normalized{ NormalizeFeatureName(label) };
		if constexpr (std::same_as<std::remove_cvref_t<Component>, Ellipse>) {
			return DrawWHValue(label, value, kInspectorSizeDragSpeed, 0.0f, 0.0f, "%.3f");
		} else if (normalized == "size" || normalized == "dimensions") {
			return DrawWHValue(label, value, kInspectorSizeDragSpeed, 0.0f, 0.0f, "%.3f");
		} else {
			return DrawPickableLocalPosition(target, component, label, locator, callback);
		}
	} else if constexpr (std::same_as<Type, Rect>) {
		bool changed{ false };
		V2_float size{ value.GetSize() };

		if (DrawWHValue("Size", size, kInspectorSizeDragSpeed, 0.0f, 0.0f, "%.3f")) {
			size.x = std::max(size.x, 0.0f);
			size.y = std::max(size.y, 0.0f);

			const V2_float center{ value.GetCenter() };
			const V2_float half_size{ size * 0.5f };
			value.min = center - half_size;
			value.max = center + half_size;
			changed	  = true;
		}

		auto min_locator = [locator](Component& root) -> V2_float& {
			return locator(root).min;
		};
		auto max_locator = [locator](Component& root) -> V2_float& {
			return locator(root).max;
		};

		changed |= DrawPickableLocalPosition(target, component, "Min", min_locator, callback);
		changed |= DrawPickableLocalPosition(target, component, "Max", max_locator, callback);

		return changed;
	} else if constexpr (kIsVector<Type>) {
		if constexpr (
			std::same_as<std::remove_cvref_t<Component>, Collider> &&
			std::integral<typename Type::value_type>
		) {
			const std::string normalized{ NormalizeFeatureName(label) };

			if (normalized.contains("collideswith")) {
				return DrawColliderMaskList(value);
			}

			return DrawValue(target.ctx, label, value);
		} else if constexpr (std::same_as<typename Type::value_type, V2_float>) {
			bool changed{ false };
			std::optional<std::size_t> remove;

			if (ImGui::Button("+ Vertex", ImVec2{ -FLT_MIN, 0.0f })) {
				value.push_back(value.empty() ? V2_float{} : value.back());
				changed = true;
			}

			for (std::size_t index{ 0 }; index < value.size(); ++index) {
				ScopedID vertex_scope{ static_cast<int>(index) };
				auto vertex_locator = [locator, index](Component& root) -> V2_float& {
					return locator(root)[index];
				};
				bool remove_vertex{ false };

				changed |= DrawPickableLocalPosition(
					target, component, std::string{ "Vertex " } + std::to_string(index + 1),
					vertex_locator, callback, &remove_vertex
				);

				if (remove_vertex) {
					remove = index;
				}
			}

			if (remove) {
				value.erase(value.begin() + static_cast<std::ptrdiff_t>(*remove));
				changed = true;
			}

			return changed;
		} else {
			return DrawValue(target.ctx, label, value);
		}
	} else if constexpr (kIsArray<Type>) {
		if constexpr (std::same_as<typename Type::value_type, V2_float>) {
			bool changed{ false };

			for (std::size_t index{ 0 }; index < value.size(); ++index) {
				ScopedID vertex_scope{ static_cast<int>(index) };
				auto vertex_locator = [locator, index](Component& root) -> V2_float& {
					return locator(root)[index];
				};

				changed |= DrawPickableLocalPosition(
					target, component, std::string{ "Vertex " } + std::to_string(index + 1),
					vertex_locator, callback
				);
			}

			return changed;
		} else {
			return DrawValue(target.ctx, label, value);
		}
	} else if constexpr (kIsVariant<Type>) {
		return DrawGeometryVariant(
			target, component, value, label, locator, callback,
			std::make_index_sequence<std::variant_size_v<Type>>{}
		);
	} else if constexpr (std::integral<Type>) {
		const std::string normalized{ NormalizeFeatureName(label) };

		if constexpr (std::same_as<std::remove_cvref_t<Component>, Collider>) {
			if (normalized == "mask") {
				return DrawValue(
					target.ctx, label, value,
					FieldOptions{
						.speed = 1.0f,
						.min   = 0.0f,
						.max   = 64.0f,
						.flags = ImGuiSliderFlags_AlwaysClamp,
					}
				);
			}
		}

		return DrawValue(target.ctx, label, value);
	} else if constexpr (std::same_as<Type, float>) {
		const std::string normalized{ NormalizeFeatureName(label) };
		if (normalized.contains("radius") || normalized.contains("radii")) {
			return DrawRValue(label, value, 0.1f, 0.0f, 0.0f, "%.3f");
		}
		return DrawValue(target.ctx, label, value);
	} else if constexpr (ReflectedValue<Type>) {
		auto reflected{ ReflectValue(value) };
		auto value_locator = [locator](Component& root) -> decltype(auto) {
			return ReflectValue(locator(root)).value;
		};

		return DrawGeometryValue(
			target, component, reflected.value, label, value_locator, callback
		);
	} else if constexpr (ReflectedMembers<Type>) {
		auto members{ ReflectMembers(value) };

		return DrawReflectedGeometryMembers(
			target, component, value, locator, callback,
			std::make_index_sequence<std::tuple_size_v<decltype(members)>>{}
		);
	} else {
		return DrawValue(target.ctx, label, value);
	}
}

template <typename Target, typename Component, typename Callback = std::nullptr_t>
bool DrawGeometryComponent(Target& target, Component& component, Callback callback = nullptr) {
	auto root_locator = [](Component& value) -> Component& {
		return value;
	};

	return DrawGeometryValue(
		target, component, component, TypeLabel<Component>(), root_locator, callback
	);
}

template <typename Target, typename T, typename Draw, typename Callback = std::nullptr_t>
bool DrawRequiredInlineVisualComponent(
	Target& target, std::string_view label, Draw&& draw, Callback callback = nullptr
) {
	return DrawRequiredComponent<Target, T>(
		target, label, false, std::forward<Draw>(draw), callback
	);
}

template <typename Target, typename T, typename Draw, typename Callback = std::nullptr_t>
bool DrawRequiredInlineVisualComponentWithDefault(
	Target& target, std::string_view label, T default_value, Draw&& draw,
	Callback callback = nullptr
) {
	ScopedID target_scope{ target.Id() };
	ScopedID component_scope{ static_cast<int>(Hash<T>()) };

	auto before{ target.template Capture<T>() };
	bool changed{ false };

	if (!before) {
		target.template SetLive<T>(std::move(default_value), callback);
		changed = true;
	}

	T value{ target.template Capture<T>().value_or(T{}) };
	changed |= std::invoke(std::forward<Draw>(draw), value);

	if (changed) {
		target.template SetLive<T>(std::move(value), callback);
	}

	auto after{ target.template Capture<T>() };
	TrackComponentState(
		target, std::string{ "Edit " } + std::string{ label }, std::move(before), std::move(after),
		changed, callback
	);

	return changed;
}

template <typename Target, typename T>
bool DrawOptionalVisualComponent(
	Target& target, std::string_view label, bool tree = false, bool contents_read_only = false,
	bool toggle_read_only = false
) {
	if constexpr (std::is_empty_v<T>) {
		return DrawOptionalComponent<Target, T>(
			target, label, false, [](T&) { return false; }, contents_read_only, toggle_read_only
		);
	} else {
		return DrawOptionalComponent<Target, T>(
			target, label, tree,
			[&target](T& value) {
				return DrawRegisteredComponentContents(
					target.ctx, Hash<T>(), std::addressof(value)
				);
			},
			contents_read_only, toggle_read_only
		);
	}
}

[[nodiscard]] bool IsShapeRenderer(std::string_view visual) {
	return visual == "rect" || visual == "circle" || visual == "roundedrect" ||
		   visual == "polygon" || visual == "ellipse" || visual == "triangle" || visual == "line" ||
		   visual == "capsule" || visual == "arc";
}

[[nodiscard]] bool IsEffectRenderer(std::string_view visual);

inline constexpr V2_float kInspectorDefaultShapeSize{ 100.0f, 100.0f };
inline constexpr float kInspectorDefaultShapeRadius{ 50.0f };

// Keep renderer selected geometry consistent with the Scene Hierarchy create menu.
template <typename T>
[[nodiscard]] T MakeDefaultShapeGeometry() {
	if constexpr (std::same_as<T, Rect>) {
		if constexpr (std::constructible_from<T, V2_float>) {
			return T{ kInspectorDefaultShapeSize };
		}
	} else if constexpr (std::same_as<T, Circle>) {
		if constexpr (std::constructible_from<T, float>) {
			return T{ kInspectorDefaultShapeRadius };
		}
	} else if constexpr (std::same_as<T, Line>) {
		if constexpr (std::constructible_from<T, V2_float, V2_float>) {
			return T{ V2_float{ -100.0f, -100.0f }, V2_float{ 100.0f, 100.0f } };
		}
	} else if constexpr (std::same_as<T, Polygon>) {
		std::vector<V2_float> vertices{
			{ 0.0f, -50.0f },  { 47.0f, -15.0f },  { 29.0f, 40.0f },
			{ -29.0f, 40.0f }, { -47.0f, -15.0f },
		};

		if constexpr (std::constructible_from<T, std::vector<V2_float>>) {
			return T{ std::move(vertices) };
		}
	} else if constexpr (std::same_as<T, Ellipse>) {
		if constexpr (std::constructible_from<T, V2_float>) {
			return T{ V2_float{ 100.0f, 50.0f } };
		}
	} else if constexpr (std::same_as<T, Arc>) {
		if constexpr (std::constructible_from<T, float, float, float, bool>) {
			return T{ kInspectorDefaultShapeRadius, 0.0f, 90.0f, true };
		} else if constexpr (std::constructible_from<T, float, Degrees, Degrees, bool>) {
			return T{
				kInspectorDefaultShapeRadius,
				Degrees{ 0.0f },
				Degrees{ 90.0f },
				true,
			};
		}
	} else if constexpr (std::same_as<T, RoundedRect>) {
		if constexpr (std::constructible_from<T, V2_float, float>) {
			return T{ kInspectorDefaultShapeSize, 10.0f };
		} else if constexpr (std::constructible_from<T, Rect, float>) {
			return T{ MakeDefaultShapeGeometry<Rect>(), 10.0f };
		}
	} else if constexpr (std::same_as<T, Triangle>) {
		if constexpr (std::constructible_from<T, V2_float, V2_float, V2_float>) {
			return T{
				V2_float{ -100.0f, 50.0f },
				V2_float{ 0.0f, -50.0f },
				V2_float{ 100.0f, 50.0f },
			};
		}
	} else if constexpr (std::same_as<T, Capsule>) {
		if constexpr (std::constructible_from<T, V2_float, V2_float, float>) {
			return T{
				V2_float{ -100.0f, -100.0f },
				V2_float{ 100.0f, 100.0f },
				kInspectorDefaultShapeRadius,
			};
		} else if constexpr (std::constructible_from<T, Line, float>) {
			return T{
				MakeDefaultShapeGeometry<Line>(),
				kInspectorDefaultShapeRadius,
			};
		}
	}

	return T{};
}

using RendererOwnedComponents = FeatureComponents<
	Rect, Circle, RoundedRect, Polygon, Ellipse, Triangle, Line, Capsule, Arc, Color, FillStyle,
	TextureKey, ::ptgn::SpriteStackData, ::ptgn::impl::TextureSize, ::ptgn::impl::TextureCrop,
	::ptgn::impl::AnimationData, ::ptgn::impl::Offsets, ::ptgn::impl::TextData,
	::ptgn::impl::ParticleEmitterData, LightData, ::ptgn::impl::ShadowCaster,
	::ptgn::impl::GraphicsData, ::ptgn::Material, ShaderKey, ::ptgn::impl::RenderTargetDesc,
	::ptgn::impl::EffectTag, ::ptgn::impl::HDREffectTag, EffectMargin, Bloom, Blur, GaussianBlur,
	::ptgn::impl::ClearColor, ::ptgn::impl::ClearDepth, ::ptgn::impl::ClearStencil>;

template <typename Target, typename T>
void ClearRendererOwnedComponent(Target& target) {
	if constexpr (Target::template Supports<T>()) {
		target.template SetLive<T>(std::nullopt);
	}
}

template <typename Target, typename... T>
void ClearRendererOwnedComponents(Target& target, FeatureComponents<T...>) {
	(ClearRendererOwnedComponent<Target, T>(target), ...);
}

template <typename T, typename Target>
void SetRendererOwnedComponent(Target& target, T value = T{}) {
	if constexpr (Target::template Supports<T>()) {
		target.template SetLive<T>(std::move(value));
	}
}

[[nodiscard]] ::ptgn::impl::TextData MakeDefaultTextRendererData() {
	::ptgn::impl::TextData data;
	auto members{ ReflectMembers(data) };

	std::apply(
		[](auto&&... member) {
			(
				[&] {
					using Member = std::remove_cvref_t<decltype(member.value)>;

					if constexpr (std::same_as<Member, StyledText>) {
						if (member.value.runs.empty()) {
							member.value.runs.emplace_back();
						}
					}
				}(),
				...);
		},
		members
	);

	return data;
}

template <typename Target>
void InitializeRendererOwnedComponents(Target& target, std::string_view visual) {
	auto add_shape = [&]<typename T>() {
		SetRendererOwnedComponent<T>(target, MakeDefaultShapeGeometry<T>());
		SetRendererOwnedComponent<Color>(target, Color{ color::White });
		SetRendererOwnedComponent<FillStyle>(target, FillStyle{ Solid{} });
	};

	if (visual == "rect") {
		add_shape.template operator()<Rect>();
	} else if (visual == "circle") {
		add_shape.template operator()<Circle>();
	} else if (visual == "roundedrect") {
		add_shape.template operator()<RoundedRect>();
	} else if (visual == "polygon") {
		add_shape.template operator()<Polygon>();
	} else if (visual == "ellipse") {
		add_shape.template operator()<Ellipse>();
	} else if (visual == "triangle") {
		add_shape.template operator()<Triangle>();
	} else if (visual == "line") {
		SetRendererOwnedComponent<Line>(target, MakeDefaultShapeGeometry<Line>());
		SetRendererOwnedComponent<Color>(target, Color{ color::White });
		SetRendererOwnedComponent<FillStyle>(target, FillStyle{ kInspectorMinLineWidth });
	} else if (visual == "capsule") {
		add_shape.template operator()<Capsule>();
	} else if (visual == "arc") {
		add_shape.template operator()<Arc>();
	} else if (visual == "spritestack") {
		SetRendererOwnedComponent<SpriteStackData>(target);
	} else if (visual.contains("text")) {
		SetRendererOwnedComponent<::ptgn::impl::TextData>(target, MakeDefaultTextRendererData());
	} else if (visual.contains("particle")) {
		SetRendererOwnedComponent<::ptgn::impl::ParticleEmitterData>(target);
	} else if (visual.contains("light")) {
		SetRendererOwnedComponent<LightData>(target);
	} else if (visual.contains("customshader")) {
		SetRendererOwnedComponent<::ptgn::Material>(target);
	} else if (visual.contains("graphics")) {
		SetRendererOwnedComponent<::ptgn::impl::GraphicsData>(target);
	} else if (visual.contains("rendertarget")) {
		SetRendererOwnedComponent<::ptgn::impl::RenderTargetDesc>(target);
	}

	if (visual.contains("gaussianblur")) {
		SetRendererOwnedComponent<GaussianBlur>(target);
	} else if (visual.contains("blur")) {
		SetRendererOwnedComponent<Blur>(target);
	} else if (visual.contains("bloom")) {
		SetRendererOwnedComponent<Bloom>(target);
	}
}

template <typename Target>
void ApplyRendererSelection(Target& target, ComponentState<::ptgn::impl::IDrawable> drawable) {
	ClearRendererOwnedComponents(target, RendererOwnedComponents{});
	target.template SetLive<::ptgn::impl::IDrawable>(drawable);

	if (!drawable) {
		return;
	}

	const auto* info{ ::ptgn::impl::IDrawable::FindInfo(drawable->hash) };

	if (!info) {
		return;
	}

	InitializeRendererOwnedComponents(
		target, NormalizeFeatureName(GetDrawableInspectorLabel(*info))
	);
}

struct RendererRowResult {
	std::string visual{};
	bool changed{ false };
};

template <typename Target>
RendererRowResult DrawRendererRow(Target& target) {
	using Drawable = ::ptgn::impl::IDrawable;

	const bool primary_scene_target{ IsPrimarySceneRenderTarget(target) };

	auto drawable{ target.template Capture<Drawable>() };
	auto before_visible{ target.template Capture<Visible>() };

	bool visible{ before_visible ? before_visible->visible : true };

	const auto* info{ drawable ? Drawable::FindInfo(drawable->hash) : nullptr };

	const std::string preview{ primary_scene_target ? "Render Target"
							   : info				? GetDrawableInspectorLabel(*info)
													: "None" };

	const float start_x{ ImGui::GetCursorPosX() };
	const float checkbox_slot{ ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x };

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Renderer");
	MeasurePropertyLabel("Renderer", start_x + checkbox_slot);
	ImGui::SameLine();
	ImGui::SetCursorPosX(GetPropertyValueX(start_x));

	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float visible_width{ ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x +
							   ImGui::CalcTextSize("Visible").x };
	const float combo_width{
		std::max(80.0f, ImGui::GetContentRegionAvail().x - visible_width - spacing)
	};

	bool renderer_changed{ false };
	auto before_renderer{
		CaptureInspectorFeatureState(target, InspectorFeature::Visual, VisualFeatureComponents{})
	};

	auto choose_renderer = [&](ComponentState<Drawable> selected) {
		const bool same_renderer{ selected.has_value() == drawable.has_value() &&
								  (!selected || selected->hash == drawable->hash) };

		if (same_renderer) {
			return;
		}

		ApplyRendererSelection(target, selected);

		if (!selected) {
			SetFeatureManuallyAdded(target.GetFeatureTargetKey(), InspectorFeature::Visual, true);
		}

		drawable		 = target.template Capture<Drawable>();
		renderer_changed = true;
	};

	std::size_t renderer_popup_items{ 3 };
	for (const auto& candidate : Drawable::data()) {
		const std::string visual{ NormalizeFeatureName(GetDrawableInspectorLabel(candidate)) };
		if (!IsShapeRenderer(visual) && !IsEffectRenderer(visual)) {
			++renderer_popup_items;
		}
	}

	const float renderer_popup_height{ ImGui::GetStyle().WindowPadding.y * 2.0f +
									   static_cast<float>(renderer_popup_items) *
										   ImGui::GetTextLineHeightWithSpacing() -
									   ImGui::GetStyle().ItemSpacing.y };

	ImGui::SetNextItemWidth(combo_width);

	ImGui::BeginDisabled(primary_scene_target);

	ImGui::SetNextWindowSizeConstraints(
		ImVec2{ 0.0f, renderer_popup_height }, ImVec2{ FLT_MAX, renderer_popup_height }
	);

	if (ImGui::BeginCombo("##RendererSelector", preview.c_str())) {
		if (!primary_scene_target) {
			if (ImGui::Selectable("None", !drawable.has_value())) {
				choose_renderer(std::nullopt);
			}

			auto draw_candidate = [&](const auto& candidate) {
				ScopedID candidate_scope{ static_cast<const void*>(std::addressof(candidate)) };
				const std::string label{ GetDrawableInspectorLabel(candidate) };
				const bool selected{ drawable && drawable->hash == candidate.hash };

				if (ImGui::Selectable(label.c_str(), selected)) {
					choose_renderer(Drawable{ candidate.hash });
				}

				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			};

			for (const auto& candidate : Drawable::data()) {
				const std::string visual{
					NormalizeFeatureName(GetDrawableInspectorLabel(candidate))
				};

				if (!IsShapeRenderer(visual) && !IsEffectRenderer(visual)) {
					draw_candidate(candidate);
				}
			}

			if (ImGui::BeginMenu("Shapes")) {
				for (const auto& candidate : Drawable::data()) {
					const std::string visual{
						NormalizeFeatureName(GetDrawableInspectorLabel(candidate))
					};

					if (IsShapeRenderer(visual)) {
						draw_candidate(candidate);
					}
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Effects")) {
				for (const auto& candidate : Drawable::data()) {
					const std::string visual{
						NormalizeFeatureName(GetDrawableInspectorLabel(candidate))
					};

					if (IsEffectRenderer(visual)) {
						draw_candidate(candidate);
					}
				}

				ImGui::EndMenu();
			}
		}

		ImGui::EndCombo();
	}

	ImGui::EndDisabled();

	if (primary_scene_target && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip("The scene render target renderer cannot be changed.");
	}

	if (renderer_changed) {
		auto after_renderer{ CaptureInspectorFeatureState(
			target, InspectorFeature::Visual, VisualFeatureComponents{}
		) };

		TrackInspectorFeatureState(
			target, InspectorFeature::Visual, "Change Renderer", std::move(before_renderer),
			std::move(after_renderer), VisualFeatureComponents{}
		);
	}

	bool visible_changed{ false };

	if (drawable || primary_scene_target) {
		ImGui::SameLine(0.0f, spacing);
		visible_changed = ImGui::Checkbox("Visible##RendererVisible", &visible);
	}

	if (visible_changed) {
		Visible updated{ before_visible.value_or(Visible{}) };
		updated.visible = visible;
		target.template SetLive<Visible>(ComponentState<Visible>{ updated });
	}

	auto after_visible{ target.template Capture<Visible>() };

	TrackComponentState(
		target, "Toggle Visibility", std::move(before_visible), std::move(after_visible),
		visible_changed
	);

	const auto* selected_info{ drawable ? Drawable::FindInfo(drawable->hash) : nullptr };

	return RendererRowResult{
		.visual	 = primary_scene_target ? "rendertarget"
				 : selected_info ? NormalizeFeatureName(GetDrawableInspectorLabel(*selected_info))
								 : std::string{},
		.changed = renderer_changed,
	};
}

template <typename Target>
bool DrawEffectMargin(Target& target) {
	return DrawOptionalComponent<Target, EffectMargin>(
		target, "Effect Margin", false, [&target](EffectMargin& margin) {
			const FieldOptions options{
				.speed = 1.0f,
				.min   = 0.0,
				.max   = 4096.0,
				.flags = ImGuiSliderFlags_AlwaysClamp,
			};

			return [&target, &options]<typename Margin>(Margin& value) {
				if constexpr (ReflectedValue<Margin>) {
					auto member{ ReflectValue(value) };
					return DrawValue(target.ctx, "Effect Margin", member.value, options);
				} else if constexpr (ReflectedMembers<Margin>) {
					bool changed{ false };
					auto members{ ReflectMembers(value) };

					std::apply(
						[&](auto&&... member) {
							((changed |= DrawValue(
								  target.ctx, PrettyName(member.name), member.value, options
							  )),
							 ...);
						},
						members
					);

					return changed;
				} else {
					return DrawDefaultContents(target.ctx, value);
				}
			}(margin);
		}
	);
}

[[nodiscard]] bool IsEffectRenderer(std::string_view visual) {
	if (visual.empty()) {
		return false;
	}

	const bool standard_renderer{
		visual == "rect" || visual == "circle" || visual == "roundedrect" || visual == "polygon" ||
		visual == "ellipse" || visual == "triangle" || visual == "line" || visual == "capsule" ||
		visual == "arc" || visual.contains("sprite") || visual.contains("text") ||
		visual.contains("particle") || visual.contains("light") || visual.contains("graphics") ||
		visual.contains("customshader") || visual.contains("rendertarget")
	};

	return !standard_renderer;
}

template <typename T>
bool DrawFlattenedConfig(EditorContext& ctx, T& value);

template <typename Target, typename Effect>
bool DrawSelectedEffectComponent(Target& target, std::string_view label) {
	if constexpr (!Target::template Supports<Effect>()) {
		return false;
	} else {
		return DrawRequiredComponent<Target, Effect>(
			target, label, false,
			[&target](Effect& value) { return DrawFlattenedConfig(target.ctx, value); }
		);
	}
}

template <typename Target>
bool DrawVisualEffects(Target& target, std::string_view visual) {
	bool changed{ false };

	if (visual.contains("gaussianblur")) {
		changed |= DrawSelectedEffectComponent<Target, GaussianBlur>(target, "Gaussian Blur");
	} else if (visual.contains("blur")) {
		changed |= DrawSelectedEffectComponent<Target, Blur>(target, "Blur");
	} else if (visual.contains("bloom")) {
		changed |= DrawSelectedEffectComponent<Target, Bloom>(target, "Bloom");
	}

	if (IsEffectRenderer(visual)) {
		changed |= DrawEffectMargin(target);
	}

	return changed;
}

inline bool IsTextWrapSettingName(std::string_view normalized) {
	return normalized == "allowwordbreakinoverflow" || normalized == "inserthyphenonsplit" ||
		   normalized == "preventsinglelettersplit" || normalized == "requirethreeletterremainder";
}

inline std::string TextWrapSettingLabel(std::string_view normalized) {
	if (normalized == "allowwordbreakinoverflow") {
		return "Break Overflowing Words";
	}
	if (normalized == "inserthyphenonsplit") {
		return "Insert Hyphen on Split";
	}
	if (normalized == "preventsinglelettersplit") {
		return "Prevent Single-Letter Split";
	}
	if (normalized == "requirethreeletterremainder") {
		return "Require Three-Letter Remainder";
	}
	return PrettyName(normalized);
}

inline const char* TextWrapSettingTooltip(std::string_view normalized) {
	if (normalized == "allowwordbreakinoverflow") {
		return "Split words that cannot fit on an empty line.";
	}
	if (normalized == "inserthyphenonsplit") {
		return "Insert a hyphen when a word is split.";
	}
	if (normalized == "preventsinglelettersplit") {
		return "Avoid leaving one letter behind when splitting.";
	}
	if (normalized == "requirethreeletterremainder") {
		return "Keep at least three letters on the next line when possible.";
	}
	return nullptr;
}

template <typename T, typename F>
void ForEachTextWrapMode(T& value, bool plain_mode_name, F&& fn) {
	if constexpr (ReflectedMembers<T>) {
		auto members{ ReflectMembers(value) };
		auto visit = [&](auto&& member) {
			using Member = std::remove_cvref_t<decltype(member.value)>;
			const std::string normalized{ NormalizeFeatureName(member.name) };

			if constexpr (std::is_enum_v<Member>) {
				if (normalized == "wrapmode" || (plain_mode_name && normalized == "mode")) {
					fn(member.value);
				}
			} else if constexpr (ReflectedMembers<Member>) {
				if (normalized == "wrap") {
					ForEachTextWrapMode(member.value, true, fn);
				}
			}
		};
		std::apply([&](auto&&... member) { (visit(member), ...); }, members);
	}
}

template <typename Wrap>
bool DrawTextWrapMode(EditorContext& ctx, Wrap& wrap, bool plain_mode_name) {
	bool changed{ false };
	bool drawn{ false };

	if constexpr (std::is_enum_v<std::remove_cvref_t<Wrap>>) {
		return DrawValue(ctx, "Wrap Mode", wrap);
	} else {
		ForEachTextWrapMode(wrap, plain_mode_name, [&](auto& mode) {
			if (!drawn) {
				changed |= DrawValue(ctx, "Wrap Mode", mode);
				drawn	 = true;
			}
		});
	}

	return changed;
}

template <typename T, typename F>
void ForEachTextWrapSetting(T& value, bool inside_wrap, F&& fn) {
	if constexpr (ReflectedMembers<T>) {
		auto members{ ReflectMembers(value) };
		auto visit = [&](auto&& member) {
			using Member = std::remove_cvref_t<decltype(member.value)>;
			const std::string normalized{ NormalizeFeatureName(member.name) };

			if constexpr (std::same_as<Member, bool>) {
				if (IsTextWrapSettingName(normalized)) {
					fn(member.value, normalized);
				}
			} else if constexpr (ReflectedMembers<Member>) {
				if (inside_wrap || normalized == "wrap") {
					ForEachTextWrapSetting(member.value, true, fn);
				}
			}
		};
		std::apply([&](auto&&... member) { (visit(member), ...); }, members);
	}
}

template <typename Wrap>
bool DrawTextWrapSettings(EditorContext&, Wrap& wrap, bool inside_wrap) {
	if constexpr (!ReflectedMembers<Wrap>) {
		return false;
	} else {
		std::vector<std::string> selected;
		ForEachTextWrapSetting(wrap, inside_wrap, [&](bool& value, std::string_view normalized) {
			if (value) {
				selected.push_back(TextWrapSettingLabel(normalized));
			}
		});

		std::string preview;
		for (const auto& item : selected) {
			if (!preview.empty()) {
				preview += ", ";
			}
			preview += item;
		}
		if (preview.empty()) {
			preview = "None";
		}

		return DrawPropertyRow("Wrap Settings", [&]() {
			bool changed{ false };
			if (ImGui::BeginCombo("##WrapSettings", preview.c_str())) {
				ForEachTextWrapSetting(
					wrap, inside_wrap, [&](bool& value, std::string_view normalized) {
						const std::string item_label{ TextWrapSettingLabel(normalized) };
						if (ImGui::Selectable(
								item_label.c_str(), value, ImGuiSelectableFlags_DontClosePopups
							)) {
							value	= !value;
							changed = true;
						}
						DrawTooltip(TextWrapSettingTooltip(normalized));
					}
				);
				ImGui::EndCombo();
			}
			return changed;
		});
	}
}

template <typename Alignment>
bool DrawTextAlignment(EditorContext& ctx, Alignment& alignment) {
	if constexpr (!ReflectedMembers<Alignment>) {
		return DrawValue(ctx, "Alignment", alignment);
	} else {
		bool changed{ false };
		auto members{ ReflectMembers(alignment) };

		auto draw_member = [&](auto&& member) {
			const std::string normalized{ NormalizeFeatureName(member.name) };

			if (normalized.contains("horizontal")) {
				changed |= DrawValue(ctx, "Horizontal Align", member.value);
			} else if (normalized.contains("vertical")) {
				changed |= DrawValue(ctx, "Vertical Align", member.value);
			} else {
				changed |= DrawValue(ctx, PrettyName(member.name), member.value);
			}
		};

		std::apply([&](auto&&... member) { (draw_member(member), ...); }, members);

		return changed;
	}
}

template <typename Shrink>
bool DrawTextShrinkScale(EditorContext& ctx, Shrink& shrink) {
	if constexpr (!ReflectedMembers<Shrink>) {
		return DrawValue(ctx, "Shrink Scale", shrink);
	} else {
		float* minimum{ nullptr };
		float* maximum{ nullptr };
		auto members{ ReflectMembers(shrink) };

		auto find_bound = [&](auto&& member) {
			using Member = std::remove_cvref_t<decltype(member.value)>;

			if constexpr (std::same_as<Member, float>) {
				const std::string normalized{ NormalizeFeatureName(member.name) };

				if (normalized == "min") {
					minimum = std::addressof(member.value);
				} else if (normalized == "max") {
					maximum = std::addressof(member.value);
				}
			}
		};

		std::apply([&](auto&&... member) { (find_bound(member), ...); }, members);

		if (!minimum || !maximum) {
			return DrawDefaultContents(ctx, shrink);
		}

		bool changed{ false };

		const float normalized_minimum{ std::max(0.0f, *minimum) };
		const float normalized_maximum{ std::max(normalized_minimum, *maximum) };

		if (*minimum != normalized_minimum || *maximum != normalized_maximum) {
			*minimum = normalized_minimum;
			*maximum = normalized_maximum;
			changed	 = true;
		}

		if (DrawValue(
				ctx, "Min Scale", *minimum,
				FieldOptions{
					.speed	= 0.01f,
					.format = "%.3f",
				}
			)) {
			*minimum = std::clamp(*minimum, 0.0f, *maximum);
			changed	 = true;
		}

		if (DrawValue(
				ctx, "Max Scale", *maximum,
				FieldOptions{
					.speed	= 0.01f,
					.format = "%.3f",
				}
			)) {
			*maximum = std::max(std::max(0.0f, *maximum), *minimum);
			changed	 = true;
		}

		return changed;
	}
}

template <typename Style>
bool DrawTextBoxAdditionalStyle(EditorContext& ctx, Style& style) {
	if constexpr (!ReflectedMembers<Style>) {
		return DrawDefaultContents(ctx, style);
	} else {
		bool changed{ false };
		bool direct_wrap_settings_drawn{ false };
		auto members{ ReflectMembers(style) };

		auto draw_member = [&](auto&& member) {
			const std::string normalized{ NormalizeFeatureName(member.name) };

			if (normalized == "alignment") {
				changed |= DrawTextAlignment(ctx, member.value);
				return;
			}
			if (normalized == "horizontalalign") {
				changed |= DrawValue(ctx, "Horizontal Align", member.value);
				return;
			}
			if (normalized == "verticalalign") {
				changed |= DrawValue(ctx, "Vertical Align", member.value);
				return;
			}
			if (normalized == "wrap") {
				changed |= DrawTextWrapMode(ctx, member.value, true);
				changed |= DrawTextWrapSettings(ctx, member.value, true);
				return;
			}
			if (normalized == "wrapmode") {
				changed |= DrawValue(ctx, "Wrap Mode", member.value);
				return;
			}
			if (normalized == "collapsespaces") {
				changed |= DrawValue(ctx, "Collapse Spaces", member.value);
				return;
			}
			if (normalized == "justifylastline") {
				changed |= DrawValue(ctx, "Justify Last Line", member.value);
				return;
			}
			if (IsTextWrapSettingName(normalized)) {
				if (!direct_wrap_settings_drawn) {
					changed					   |= DrawTextWrapSettings(ctx, style, false);
					direct_wrap_settings_drawn	= true;
				}
				return;
			}
			if (normalized == "shrinkscale") {
				const bool open{ ImGui::TreeNodeEx(
					"Shrink to Fit##TextShrinkToFit", ImGuiTreeNodeFlags_SpanAvailWidth
				) };
				if (open) {
					ScopedIndent indent;
					ScopedPropertyLabelOffset label_offset{ ImGui::GetStyle().IndentSpacing };
					changed |= DrawTextShrinkScale(ctx, member.value);
					ImGui::TreePop();
				}
				return;
			}

			changed |= DrawValue(ctx, PrettyName(member.name), member.value);
		};

		std::apply([&](auto&&... member) { (draw_member(member), ...); }, members);
		return changed;
	}
}

template <std::size_t I, typename Target, typename TextData>
bool DrawTextBoxMember(Target& target, TextData& text_data, auto& box) {
	using Box = std::remove_cvref_t<decltype(box)>;
	bool changed{ false };
	auto& editor_state{ GetManualFeatureState(target.GetFeatureTargetKey()) };

	bool box_has_non_default_data{ false };

	if constexpr (JsonSerializable<Box> && std::default_initializable<Box>) {
		json current			 = box;
		json defaults			 = Box{};
		box_has_non_default_data = current != defaults;
	}

	if (!editor_state.text_box_state_initialized) {
		editor_state.text_box_enabled			= box_has_non_default_data;
		editor_state.text_box_state_initialized = true;
	} else if (box_has_non_default_data) {
		editor_state.text_box_enabled = true;
	}

	bool enabled{ editor_state.text_box_enabled };

	ScopedID box_scope{ "TextBox" };

	if (ImGui::Checkbox("##Enabled", &enabled)) {
		editor_state.text_box_enabled = enabled;

		if (!enabled) {
			box = Box{};
		}

		changed = true;
	}

	ImGui::SameLine();

	const bool open{ ImGui::TreeNodeEx("Text Box##Tree", ImGuiTreeNodeFlags_SpanAvailWidth) };

	if (!open) {
		return changed;
	}

	ScopedIndent indent;
	ScopedPropertyLabelOffset box_label_offset{ ImGui::GetStyle().IndentSpacing };
	ScopedDisabled disabled{ !enabled };
	auto box_members{ ReflectMembers(box) };

	auto draw_box_member = [&](auto&& box_member) {
		const std::string box_name{ NormalizeFeatureName(box_member.name) };

		if (box_name == "rect") {
			using BoxMember = std::remove_cvref_t<decltype(box_member.value)>;

			if constexpr (std::same_as<BoxMember, Rect>) {
				auto box_locator = [](TextData& value) -> Box& {
					auto reflected{ ReflectMembers(value) };
					return std::get<I>(reflected).value;
				};

				auto rect_locator = [box_locator](TextData& value) -> Rect& {
					auto reflected{ ReflectMembers(box_locator(value)) };
					constexpr std::size_t count{ std::tuple_size_v<decltype(reflected)> };
					Rect* result{ nullptr };

					[&]<std::size_t... Index>(std::index_sequence<Index...>) {
						(
							[&] {
								auto& candidate{ std::get<Index>(reflected) };

								if constexpr (
									std::same_as<
										std::remove_cvref_t<decltype(candidate.value)>, Rect>
								) {
									if (NormalizeFeatureName(candidate.name) == "rect") {
										result = std::addressof(candidate.value);
									}
								}
							}(),
							...);
					}(std::make_index_sequence<count>{});

					return *result;
				};

				changed |= DrawGeometryValue(
					target, text_data, box_member.value, "Rect", rect_locator, &MarkTextLayoutDirty
				);
			} else {
				changed |= DrawValue(target.ctx, "Rect", box_member.value);
			}

			return;
		}

		if (box_name == "style") {
			const bool style_open{ ImGui::TreeNodeEx(
				"Additional Options##TextBoxAdditionalOptions", ImGuiTreeNodeFlags_SpanAvailWidth
			) };

			if (style_open) {
				changed |= DrawTextBoxAdditionalStyle(target.ctx, box_member.value);

				ImGui::TreePop();
			}

			return;
		}

		changed |= DrawValue(target.ctx, PrettyName(box_member.name), box_member.value);
	};

	std::apply([&](auto&&... box_member) { (draw_box_member(box_member), ...); }, box_members);

	ImGui::TreePop();
	return changed;
}

template <std::size_t I, typename Target, typename TextData>
bool DrawTextPrimaryMember(Target& target, TextData& text_data) {
	auto members{ ReflectMembers(text_data) };
	auto& member{ std::get<I>(members) };
	const std::string normalized{ NormalizeFeatureName(member.name) };

	if (normalized == "text" || normalized == "content" || normalized == "defaults" ||
		normalized.contains("richtextsource")) {
		return false;
	}

	if (normalized.contains("glyph") || normalized.contains("clip") ||
		normalized.contains("currentrun")) {
		return false;
	}

	if (normalized == "box") {
		using Box = std::remove_cvref_t<decltype(member.value)>;

		if constexpr (ReflectedMembers<Box>) {
			return DrawTextBoxMember<I>(target, text_data, member.value);
		}
	}

	return DrawValue(target.ctx, PrettyName(member.name), member.value);
}

template <typename Target, typename TextData, std::size_t... I>
bool DrawTextPrimaryMembers(Target& target, TextData& text_data, std::index_sequence<I...>) {
	bool changed{ false };
	((changed |= DrawTextPrimaryMember<I>(target, text_data)), ...);
	return changed;
}

template <typename Target>
bool DrawTextPrimary(Target& target, ::ptgn::impl::TextData& text_data) {
	bool changed{ false };

	TextRunDefaults defaults{};
	if (!text_data.text.runs.empty()) {
		defaults.font = text_data.text.runs.front().font;
		defaults.style = text_data.text.runs.front().style;
	}

	std::string source{ SerializeStyledTextToRichText(text_data.text, defaults) };
	if (DrawRichTextEditor(target.ctx, source, defaults)) {
		text_data.text = ParseRichText(source, defaults).text;
		text_data.current_run_index =
			text_data.text.runs.empty() ? 0 : text_data.text.runs.size() - 1;
		changed = true;
	}

	auto members{ ReflectMembers(text_data) };
	changed |= DrawTextPrimaryMembers(
		target, text_data, std::make_index_sequence<std::tuple_size_v<decltype(members)>>{}
	);
	return changed;
}

template <std::size_t OuterIndex, std::size_t InnerIndex, typename Target, typename Clip>
bool DrawTextClipMember(Target& target, ::ptgn::impl::TextData& text_data, Clip& clip) {
	auto members{ ReflectMembers(clip) };
	auto& member{ std::get<InnerIndex>(members) };
	using Member = std::remove_cvref_t<decltype(member.value)>;
	const std::string normalized{ NormalizeFeatureName(member.name) };

	if constexpr (std::same_as<Member, Rect>) {
		if (normalized.contains("rect")) {
			auto locator = [](auto& root) -> Rect& {
				auto outer{ ReflectMembers(root) };
				auto& clip_value{ std::get<OuterIndex>(outer).value };

				if constexpr (kIsOptional<std::remove_cvref_t<decltype(clip_value)>>) {
					auto inner{ ReflectMembers(*clip_value) };
					return std::get<InnerIndex>(inner).value;
				} else {
					auto inner{ ReflectMembers(clip_value) };
					return std::get<InnerIndex>(inner).value;
				}
			};

			return DrawGeometryValue(
				target, text_data, member.value, "Clip Rect", locator, &MarkTextLayoutDirty
			);
		}
	}

	return DrawValue(target.ctx, PrettyName(member.name), member.value);
}

template <std::size_t OuterIndex, typename Target, typename Clip, std::size_t... InnerIndex>
bool DrawTextClipMembers(
	Target& target, ::ptgn::impl::TextData& text_data, Clip& clip,
	std::index_sequence<InnerIndex...>
) {
	bool changed{ false };
	((changed |= DrawTextClipMember<OuterIndex, InnerIndex>(target, text_data, clip)), ...);
	return changed;
}

template <std::size_t I, typename Target>
bool DrawTextAdditionalMember(Target& target, ::ptgn::impl::TextData& text_data) {
	auto members{ ReflectMembers(text_data) };
	auto& member{ std::get<I>(members) };
	const std::string normalized{ NormalizeFeatureName(member.name) };

	if (!normalized.contains("glyph") && !normalized.contains("clip")) {
		return false;
	}

	using Member = std::remove_cvref_t<decltype(member.value)>;

	if (normalized.contains("clip")) {
		if constexpr (std::same_as<Member, Rect>) {
			ImGui::SeparatorText("Clip Rect");

			auto locator = [](auto& root) -> Rect& {
				auto reflected{ ReflectMembers(root) };
				return std::get<I>(reflected).value;
			};

			return DrawGeometryValue(
				target, text_data, member.value, "Clip Rect", locator, &MarkTextLayoutDirty
			);
		} else if constexpr (kIsOptional<Member>) {
			using OptionalValue = typename Member::value_type;

			if constexpr (std::same_as<OptionalValue, Rect>) {
				bool changed{ false };
				bool enabled{ member.value.has_value() };

				ImGui::PushID(member.name.data(), member.name.data() + member.name.size());

				if (DrawOptionalLabelRow("Clip Rect", enabled, false)) {
					if (enabled) {
						member.value.emplace();
					} else {
						member.value.reset();
					}
					changed = true;
				}

				if (member.value) {
					ScopedIndent indent;
					ScopedPropertyLabelOffset label_offset{ ImGui::GetStyle().IndentSpacing };

					auto locator = [](auto& root) -> Rect& {
						auto reflected{ ReflectMembers(root) };
						return *std::get<I>(reflected).value;
					};

					changed |= DrawGeometryValue(
						target, text_data, *member.value, "Clip Rect", locator, &MarkTextLayoutDirty
					);
				}

				ImGui::PopID();
				return changed;
			} else if constexpr (ReflectedMembers<OptionalValue>) {
				bool enabled{ member.value.has_value() };
				bool changed{ false };

				ImGui::PushID(member.name.data(), member.name.data() + member.name.size());

				changed |= DrawOptionalLabelRow(PrettyName(member.name), enabled, false);

				if (enabled != member.value.has_value()) {
					if (enabled) {
						member.value.emplace();
					} else {
						member.value.reset();
					}
					changed = true;
				}

				if (member.value) {
					ScopedIndent indent;
					ScopedPropertyLabelOffset label_offset{ ImGui::GetStyle().IndentSpacing };
					auto clip_members{ ReflectMembers(*member.value) };

					changed |= DrawTextClipMembers<I>(
						target, text_data, *member.value,
						std::make_index_sequence<std::tuple_size_v<decltype(clip_members)>>{}
					);
				}

				ImGui::PopID();
				return changed;
			} else {
				return DrawValue(target.ctx, PrettyName(member.name), member.value);
			}
		} else if constexpr (ReflectedMembers<Member>) {
			auto clip_members{ ReflectMembers(member.value) };

			return DrawTextClipMembers<I>(
				target, text_data, member.value,
				std::make_index_sequence<std::tuple_size_v<decltype(clip_members)>>{}
			);
		}
	}

	return DrawValue(target.ctx, PrettyName(member.name), member.value);
}

template <typename Target, std::size_t... I>
bool DrawTextAdditionalMembers(
	Target& target, ::ptgn::impl::TextData& text_data, std::index_sequence<I...>
) {
	bool changed{ false };
	((changed |= DrawTextAdditionalMember<I>(target, text_data)), ...);
	return changed;
}

template <typename Target>
bool DrawTextAdditional(Target& target, ::ptgn::impl::TextData& text_data) {
	auto members{ ReflectMembers(text_data) };

	return DrawTextAdditionalMembers(
		target, text_data, std::make_index_sequence<std::tuple_size_v<decltype(members)>>{}
	);
}

template <typename T>
bool DrawFlattenedConfig(EditorContext& ctx, T& value) {
	if constexpr (!ReflectedMembers<T>) {
		return DrawDefaultContents(ctx, value);
	} else {
		bool changed{ false };
		auto members{ ReflectMembers(value) };

		auto draw_member = [&](auto&& member) {
			ScopedID member_scope{ static_cast<const void*>(std::addressof(member.value)) };
			using Member = std::remove_cvref_t<decltype(member.value)>;
			const std::string normalized{ NormalizeFeatureName(member.name) };

			if (normalized == "config" || normalized == "data") {
				if constexpr (
					ReflectedValue<Member> || ReflectedMembers<Member> ||
					ReflectedReadOnlyMembers<Member>
				) {
					changed |= DrawDefaultContents(ctx, member.value);
					return;
				}
			}

			changed |= DrawValue(ctx, PrettyName(member.name), member.value);
		};

		std::apply([&](auto&&... member) { (draw_member(member), ...); }, members);

		return changed;
	}
}

template <typename Target>
bool DrawLineWidthVisual(Target& target) {
	if constexpr (!Target::template Supports<FillStyle>()) {
		return false;
	} else {
		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<FillStyle>()) };

		auto before{ target.template Capture<FillStyle>() };
		bool enabled{ before.has_value() };
		bool changed{ false };

		if (ImGui::Checkbox("##Enabled", &enabled)) {
			target.template SetLive<FillStyle>(
				enabled ? ComponentState<FillStyle>{ FillStyle{ kInspectorMinLineWidth } }
						: std::nullopt
			);
			changed = true;
		}

		ImGui::SameLine();

		const float checkbox_offset{ ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x };
		ScopedPropertyLabelOffset label_offset{ checkbox_offset };

		float line_width{ kInspectorMinLineWidth };
		if (const auto current{ target.template Capture<FillStyle>() }) {
			line_width = current->GetLineWidth().value_or(kInspectorMinLineWidth);
		}

		{
			ScopedDisabled disabled{ !enabled };
			if (DrawValue(
					target.ctx, "Line Width", line_width,
					FieldOptions{
						.speed	= kInspectorScalarDragSpeed,
						.min	= kInspectorMinLineWidth,
						.max	= 1000.0f,
						.format = "%.2f",
						.flags	= ImGuiSliderFlags_AlwaysClamp,
					}
				)) {
				target.template SetLive<FillStyle>(FillStyle{ line_width });
				changed = true;
			}
		}

		auto after{ target.template Capture<FillStyle>() };
		TrackComponentState(
			target, "Edit Line Width", std::move(before), std::move(after), changed
		);

		return changed;
	}
}

template <typename T>
float GetShapeRadiusLimit(const T& value) {
	using Value = std::remove_cvref_t<T>;

	if constexpr (std::same_as<Value, float>) {
		return std::abs(value);
	} else if constexpr (std::same_as<Value, V2_float>) {
		return std::max(std::abs(value.x), std::abs(value.y));
	} else if constexpr (kIsArray<Value> || kIsVector<Value>) {
		float limit{ 0.0f };
		for (const auto& element : value) {
			limit = std::max(limit, GetShapeRadiusLimit(element));
		}
		return limit;
	} else if constexpr (ReflectedValue<Value>) {
		return GetShapeRadiusLimit(ReflectValue(const_cast<Value&>(value)).value);
	} else if constexpr (ReflectedMembers<Value>) {
		float limit{ 0.0f };
		auto members{ ReflectMembers(const_cast<Value&>(value)) };

		auto inspect_member = [&](auto&& member) {
			using Member = std::remove_cvref_t<decltype(member.value)>;
			const std::string normalized{ NormalizeFeatureName(member.name) };

			if (normalized.contains("radius") || normalized.contains("radii")) {
				if constexpr (
					std::same_as<Member, float> || std::same_as<Member, V2_float> ||
					kIsArray<Member> || kIsVector<Member> || ReflectedValue<Member> ||
					ReflectedMembers<Member>
				) {
					limit = std::max(limit, GetShapeRadiusLimit(member.value));
				}
			} else if constexpr (ReflectedValue<Member> || ReflectedMembers<Member>) {
				limit = std::max(limit, GetShapeRadiusLimit(member.value));
			}
		};

		std::apply([&](auto&&... member) { (inspect_member(member), ...); }, members);

		return limit;
	} else {
		return 0.0f;
	}
}

template <typename T>
float GetShapeSizeLimit(const T& value) {
	using Value = std::remove_cvref_t<T>;

	if constexpr (std::same_as<Value, Rect>) {
		const V2_float size{ value.GetSize() };
		return std::max(std::abs(size.x), std::abs(size.y));
	} else if constexpr (ReflectedValue<Value>) {
		return GetShapeSizeLimit(ReflectValue(const_cast<Value&>(value)).value);
	} else if constexpr (ReflectedMembers<Value>) {
		float limit{ 0.0f };
		auto members{ ReflectMembers(const_cast<Value&>(value)) };

		auto inspect_member = [&](auto&& member) {
			using Member = std::remove_cvref_t<decltype(member.value)>;
			const std::string normalized{ NormalizeFeatureName(member.name) };

			if constexpr (std::same_as<Member, Rect>) {
				limit = std::max(limit, GetShapeSizeLimit(member.value));
			} else if constexpr (std::same_as<Member, V2_float>) {
				if (normalized.contains("size") || normalized.contains("dimension")) {
					limit = std::max(
						limit, std::max(std::abs(member.value.x), std::abs(member.value.y))
					);
				}
			} else if constexpr (std::same_as<Member, float>) {
				if (normalized == "width" || normalized == "height") {
					limit = std::max(limit, std::abs(member.value));
				}
			} else if constexpr (ReflectedValue<Member> || ReflectedMembers<Member>) {
				limit = std::max(limit, GetShapeSizeLimit(member.value));
			}
		};

		std::apply([&](auto&&... member) { (inspect_member(member), ...); }, members);

		return limit;
	} else {
		return 0.0f;
	}
}

template <typename T>
float GetEllipseLineWidthLimit(const T& value) {
	using Value = std::remove_cvref_t<T>;

	if constexpr (std::same_as<Value, V2_float>) {
		return std::min(std::abs(value.x), std::abs(value.y));
	} else if constexpr (ReflectedValue<Value>) {
		return GetEllipseLineWidthLimit(ReflectValue(const_cast<Value&>(value)).value);
	} else if constexpr (ReflectedMembers<Value>) {
		float limit{ 0.0f };
		std::optional<float> x_radius;
		std::optional<float> y_radius;
		auto members{ ReflectMembers(const_cast<Value&>(value)) };

		auto inspect = [&](auto&& member) {
			using Member = std::remove_cvref_t<decltype(member.value)>;
			const std::string normalized{ NormalizeFeatureName(member.name) };

			if constexpr (std::same_as<Member, V2_float>) {
				if (normalized.contains("radius") || normalized.contains("radii")) {
					limit = std::max(
						limit, std::min(std::abs(member.value.x), std::abs(member.value.y))
					);
				}
			} else if constexpr (std::same_as<Member, float>) {
				if (normalized == "radiusx" || normalized == "xradius") {
					x_radius = std::abs(member.value);
				} else if (normalized == "radiusy" || normalized == "yradius") {
					y_radius = std::abs(member.value);
				}
			} else if constexpr (ReflectedValue<Member> || ReflectedMembers<Member>) {
				limit = std::max(limit, GetEllipseLineWidthLimit(member.value));
			}
		};

		std::apply([&](auto&&... member) { (inspect(member), ...); }, members);
		if (x_radius && y_radius) {
			limit = std::max(limit, std::min(*x_radius, *y_radius));
		}
		return limit;
	} else {
		return 0.0f;
	}
}

template <typename T>
float GetShapeLineWidthLimit(const T& value, const Transform& transform) {
	using Value = std::remove_cvref_t<T>;

	float limit{ 1000.0f };

	if constexpr (std::same_as<Value, Rect>) {
		const V2_float size{ value.GetSize(transform) };
		limit = std::min(size.x, size.y) * 0.5f;
	} else if constexpr (std::same_as<Value, RoundedRect>) {
		const V2_float size{ value.rect.GetSize(transform) };
		const float half_min_size{ std::min(size.x, size.y) * 0.5f };
		limit = half_min_size;
	} else if constexpr (std::same_as<Value, Ellipse>) {
		const V2_float radius{ value.GetRadius(transform) };
		limit = std::min(radius.x, radius.y);
	} else if constexpr (
		std::same_as<Value, Circle> || std::same_as<Value, Capsule> || std::same_as<Value, Arc>
	) {
		limit = value.GetRadius(transform);
	}

	return std::max(kInspectorMinLineWidth, limit);
}

template <typename Target>
Transform GetShapeLineWidthTransform(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		if (entity && entity.Has<Transform>()) {
			return GetDrawTransform(entity);
		}
	}

	if constexpr (Target::template Supports<Transform>()) {
		return target.template Capture<Transform>().value_or(Transform{});
	}

	return {};
}

template <typename Target, typename Shape>
bool DrawShapeFillStyle(Target& target) {
	if constexpr (!Target::template Supports<FillStyle>()) {
		return false;
	} else {
		const Shape shape{ target.template Capture<Shape>().value_or(Shape{}) };
		const Transform transform{ GetShapeLineWidthTransform(target) };

		const float unscaled_line_width_limit{ GetShapeLineWidthLimit(shape, Transform{}) };
		const float scaled_line_width_limit{ GetShapeLineWidthLimit(shape, transform) };
		const float line_width_scale{ scaled_line_width_limit / unscaled_line_width_limit };

		return DrawOptionalComponent<Target, FillStyle>(
			target, "Style", false,
			[&target, unscaled_line_width_limit, scaled_line_width_limit,
			 line_width_scale](FillStyle& style) {
				FillStyle displayed_style{ style };

				if (const auto stored_line_width{ style.GetLineWidth() }) {
					displayed_style = FillStyle{ std::max(
						kInspectorMinLineWidth, stored_line_width.value() / line_width_scale
					) };
				}

				if (!DrawFillStyle(
						target.ctx, "Style", displayed_style, unscaled_line_width_limit
					)) {
					return false;
				}

				if (const auto displayed_line_width{ displayed_style.GetLineWidth() }) {
					style = FillStyle{ std::clamp(
						displayed_line_width.value() * line_width_scale, kInspectorMinLineWidth,
						scaled_line_width_limit
					) };
				} else {
					style = FillStyle{ Solid{} };
				}

				return true;
			}
		);
	}
}

template <typename Target>
bool DrawShapeVisual(Target& target, std::string_view visual) {
	bool changed{ false };

	auto draw_shape = [&]<typename T>() {
		changed |= DrawRequiredInlineVisualComponentWithDefault<Target, T>(
			target, TypeLabel<T>(), MakeDefaultShapeGeometry<T>(),
			[&target](T& value) { return DrawGeometryComponent(target, value); }
		);
	};

	if (visual == "rect") {
		draw_shape.template operator()<Rect>();
	} else if (visual == "circle") {
		draw_shape.template operator()<Circle>();
	} else if (visual == "roundedrect") {
		draw_shape.template operator()<RoundedRect>();
	} else if (visual == "polygon") {
		draw_shape.template operator()<Polygon>();
	} else if (visual == "ellipse") {
		draw_shape.template operator()<Ellipse>();
	} else if (visual == "triangle") {
		draw_shape.template operator()<Triangle>();
	} else if (visual == "line") {
		draw_shape.template operator()<Line>();
	} else if (visual == "capsule") {
		draw_shape.template operator()<Capsule>();
	} else if (visual == "arc") {
		draw_shape.template operator()<Arc>();
	}

	changed |= DrawOptionalVisualComponent<Target, Color>(target, "Color");
	if (visual == "line") {
		changed |= DrawLineWidthVisual(target);
	} else if (visual == "rect") {
		changed |= DrawShapeFillStyle<Target, Rect>(target);
	} else if (visual == "circle") {
		changed |= DrawShapeFillStyle<Target, Circle>(target);
	} else if (visual == "roundedrect") {
		changed |= DrawShapeFillStyle<Target, RoundedRect>(target);
	} else if (visual == "capsule") {
		changed |= DrawShapeFillStyle<Target, Capsule>(target);
	} else if (visual == "ellipse") {
		changed |= DrawShapeFillStyle<Target, Ellipse>(target);
	} else if (visual == "arc") {
		changed |= DrawShapeFillStyle<Target, Arc>(target);
	} else {
		changed |= DrawOptionalVisualComponent<Target, FillStyle>(target, "Style");
	}

	return changed;
}

bool DrawAnimationDataFlattened(
	EditorContext& ctx, ::ptgn::impl::AnimationData& animation,
	std::optional<std::size_t> detected_frame_count, std::optional<V2_int> texture_size
) {
	const auto initial_frame_size{ animation.config.frame_size };
	const V2_int initial_start_pixel{ animation.config.start_pixel };

	bool changed{ false };
	auto members{ ReflectMembers(animation) };

	auto draw_member = [&](auto&& member) {
		const std::string normalized{ NormalizeFeatureName(member.name) };

		using Member = std::remove_cvref_t<decltype(member.value)>;

		if (normalized == "config") {
			if constexpr (ReflectedMembers<Member>) {
				auto config_members{ ReflectMembers(member.value) };

				std::apply(
					[&](auto&&... config_member) {
						auto draw_config_member = [&](auto&& mem) {
							const std::string normalized_name{ NormalizeFeatureName(mem.name) };

							using Value = std::remove_cvref_t<decltype(mem.value)>;

							if constexpr (std::same_as<Value, std::size_t>) {
								if (normalized_name == "framecount") {
									if (detected_frame_count) {
										mem.value = *detected_frame_count;
									}

									{
										ScopedDisabled disabled{ detected_frame_count.has_value() };

										changed |= DrawValue(ctx, PrettyName(mem.name), mem.value);
									}

									if (detected_frame_count) {
										DrawTooltip(
											"Frame count is automatically detected from the "
											"texture asset file name."
										);
									}

									return;
								}
							}

							changed |= DrawValue(ctx, PrettyName(mem.name), mem.value);
						};

						(draw_config_member(config_member), ...);
					},
					config_members
				);

				return;
			}
		}

		changed |= DrawValue(ctx, PrettyName(member.name), member.value);
	};

	std::apply([&](auto&&... member) { (draw_member(member), ...); }, members);

	{
		auto& start_pixel{ animation.config.start_pixel };
		const V2_int before_start_pixel{ start_pixel };
		const auto before_frame_size{ animation.config.frame_size };
		const std::size_t before_current_frame{ animation.current_frame };
		const bool frame_size_edited{ animation.config.frame_size != initial_frame_size };
		const bool start_pixel_edited{ start_pixel != initial_start_pixel };

		start_pixel.x = std::max(start_pixel.x, 0);
		start_pixel.y = std::max(start_pixel.y, 0);

		if (animation.config.frame_size.has_value()) {
			auto& frame_size{ *animation.config.frame_size };
			frame_size.x = std::max(frame_size.x, 1);
			frame_size.y = std::max(frame_size.y, 1);
		}

		std::size_t available_frame_count{ animation.config.frame_count };

		if (texture_size.has_value() && texture_size->IsPositive()) {
			const V2_int size{ *texture_size };
			start_pixel.x = std::min(start_pixel.x, size.x - 1);
			start_pixel.y = std::min(start_pixel.y, size.y - 1);

			if (animation.config.frame_size.has_value()) {
				auto& frame_size{ *animation.config.frame_size };
				frame_size.x = std::min(frame_size.x, size.x);
				frame_size.y = std::min(frame_size.y, size.y);

				if (frame_size_edited && !start_pixel_edited) {
					frame_size.x = std::min(frame_size.x, size.x - start_pixel.x);
					frame_size.y = std::min(frame_size.y, size.y - start_pixel.y);
				} else {
					start_pixel.x = std::min(start_pixel.x, size.x - frame_size.x);
					start_pixel.y = std::min(start_pixel.y, size.y - frame_size.y);
				}

				frame_size.x  = std::min(frame_size.x, size.x - start_pixel.x);
				frame_size.y  = std::min(frame_size.y, size.y - start_pixel.y);
				start_pixel.x = std::min(start_pixel.x, size.x - frame_size.x);
				start_pixel.y = std::min(start_pixel.y, size.y - frame_size.y);

				available_frame_count =
					static_cast<std::size_t>((size.x - start_pixel.x) / frame_size.x);
			} else if (
				const auto frame_size{
					::ptgn::impl::GetFrameSize(texture_size, animation.config.frame_count) };
				frame_size && frame_size->IsPositive()
			) {
				start_pixel.x = std::min(start_pixel.x, size.x - frame_size->x);
				start_pixel.y = std::min(start_pixel.y, size.y - frame_size->y);
				available_frame_count =
					static_cast<std::size_t>((size.x - start_pixel.x) / frame_size->x);
			} else {
				available_frame_count = 0;
			}
		}

		const std::size_t usable_frame_count{
			std::min(animation.config.frame_count, available_frame_count)
		};

		animation.current_frame =
			usable_frame_count == 0 ? 0 : std::min(animation.current_frame, usable_frame_count - 1);

		changed |= start_pixel != before_start_pixel;
		changed |= animation.config.frame_size != before_frame_size;
		changed |= animation.current_frame != before_current_frame;
	}

	if (ctx.local.settings.show_read_only_inspector_data) {
		[&]<typename T>(T& value) {
			if constexpr (ReflectedReadOnlyMembers<T>) {
				auto read_only_members{ ReflectReadOnlyMembers(value) };

				std::apply(
					[&](auto&&... member) {
						(DrawReadOnlyValue(ctx, PrettyName(member.name), member.value), ...);
					},
					read_only_members
				);
			}
		}(animation);
	}

	return changed;
}

template <typename Value>
[[nodiscard]] std::optional<V2_int> ExtractTexturePixelSize(const Value& value);

template <typename Value>
[[nodiscard]] std::optional<V2_int> ExtractTexturePixelSize(const Value& value) {
	using Type = std::remove_cvref_t<Value>;

	if constexpr (std::same_as<Type, V2_int>) {
		return value;
	} else if constexpr (std::same_as<Type, V2_float>) {
		return V2_int{ static_cast<int>(std::round(value.x)),
					   static_cast<int>(std::round(value.y)) };
	} else if constexpr (ReflectedValue<Type>) {
		return ExtractTexturePixelSize(ReflectValue(value).value);
	} else if constexpr (ReflectedMembers<Type>) {
		auto members{ ReflectMembers(value) };

		if constexpr (std::tuple_size_v<decltype(members)> == 1) {
			return ExtractTexturePixelSize(std::get<0>(members).value);
		}
	}

	return std::nullopt;
}

template <typename Value>
bool DrawTextureSizeAsIntegers(EditorContext& ctx, Value& value) {
	using Type = std::remove_cvref_t<Value>;

	if constexpr (std::same_as<Type, V2_int>) {
		return DrawWHValue("Texture Size", value, 1.0f, 0, 4096);
	} else if constexpr (std::same_as<Type, V2_float>) {
		V2_int displayed{ static_cast<int>(std::round(value.x)),
						  static_cast<int>(std::round(value.y)) };

		if (!DrawWHValue("Texture Size", displayed, 1.0f, 0, 4096)) {
			return false;
		}

		value = V2_float{ static_cast<float>(displayed.x), static_cast<float>(displayed.y) };
		return true;
	} else if constexpr (ReflectedValue<Type>) {
		auto member{ ReflectValue(value) };
		return DrawTextureSizeAsIntegers(ctx, member.value);
	} else if constexpr (ReflectedMembers<Type>) {
		auto members{ ReflectMembers(value) };

		if constexpr (std::tuple_size_v<decltype(members)> == 1) {
			return DrawTextureSizeAsIntegers(ctx, std::get<0>(members).value);
		} else {
			return DrawDefaultContents(ctx, value);
		}
	} else {
		return DrawDefaultContents(ctx, value);
	}
}

template <typename Value>
bool SetTextureSizeFromPixels(Value& value, V2_float pixels) {
	using Type = std::remove_cvref_t<Value>;

	if constexpr (std::same_as<Type, V2_float>) {
		value = pixels;
		return true;
	} else if constexpr (std::same_as<Type, V2_int>) {
		value = V2_int{ static_cast<int>(std::round(pixels.x)),
						static_cast<int>(std::round(pixels.y)) };
		return true;
	} else if constexpr (ReflectedValue<Type>) {
		auto member{ ReflectValue(value) };
		return SetTextureSizeFromPixels(member.value, pixels);
	} else if constexpr (ReflectedMembers<Type>) {
		auto members{ ReflectMembers(value) };

		if constexpr (std::tuple_size_v<decltype(members)> == 1) {
			return SetTextureSizeFromPixels(std::get<0>(members).value, pixels);
		}
	}

	return false;
}

template <typename Target>
[[nodiscard]] std::optional<std::size_t> ResolveDetectedAnimationFrameCount(const Target& target) {
	if constexpr (!Target::template Supports<TextureKey>()) {
		return std::nullopt;
	} else {
		const auto texture_key{ target.template Capture<TextureKey>() };

		if (!texture_key) {
			return std::nullopt;
		}

		return ::ptgn::impl::DetectAnimationFrameCount(
			target.ctx.editor.GetAssetManager(), *texture_key
		);
	}
}

template <typename Target>
[[nodiscard]] std::optional<V2_int> ResolveAnimationTextureSize(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		if (entity) {
			const auto texture_size{ GetTextureSize(entity) };
			return texture_size && texture_size->IsPositive() ? texture_size : std::nullopt;
		}
	}

	if constexpr (Target::template Supports<::ptgn::impl::TextureSize>()) {
		if (const auto texture_size{ target.template Capture<::ptgn::impl::TextureSize>() }) {
			const auto pixels{ ExtractTexturePixelSize(*texture_size) };
			return pixels && pixels->IsPositive() ? pixels : std::nullopt;
		}
	}

	return std::nullopt;
}

template <typename Target>
bool SynchronizeAnimationFrameData(Target& target, std::string_view reason) {
	using AnimationData = ::ptgn::impl::AnimationData;
	using TextureCrop	= ::ptgn::impl::TextureCrop;

	if constexpr (!Target::template Supports<AnimationData>()) {
		return false;
	} else {
		auto before_animation{ target.template Capture<AnimationData>() };

		if (!before_animation) {
			return false;
		}

		AnimationData animation{ *before_animation };

		if (const auto detected{ ResolveDetectedAnimationFrameCount(target) }) {
			animation.config.frame_count = *detected;
		}

		const auto texture_size{ ResolveAnimationTextureSize(target) };

		if (!animation.config.frame_size.has_value()) {
			animation.config.frame_size =
				::ptgn::impl::GetFrameSize(texture_size, animation.config.frame_count);
		}

		if (animation.config.frame_count == 0) {
			animation.current_frame = 0;
		} else {
			animation.current_frame %= animation.config.frame_count;
		}

		animation.frame_dirty = true;
		target.template SetLive<AnimationData>(animation);
		auto after_animation{ target.template Capture<AnimationData>() };
		TrackComponentState(target, reason, std::move(before_animation), after_animation, true);

		if constexpr (Target::template Supports<TextureCrop>()) {
			auto before_crop{ target.template Capture<TextureCrop>() };
			TextureCrop crop{ before_crop.value_or(TextureCrop{}) };
			crop.Update(animation, texture_size);
			target.template SetLive<TextureCrop>(crop);
			auto after_crop{ target.template Capture<TextureCrop>() };
			TrackComponentState(
				target, "Update Animation Texture Crop", std::move(before_crop),
				std::move(after_crop), true
			);
		}

		return true;
	}
}

template <typename Target>
bool SynchronizeAnimationTextureCrop(Target& target) {
	using AnimationData = ::ptgn::impl::AnimationData;
	using TextureCrop	= ::ptgn::impl::TextureCrop;

	if constexpr (
		!Target::template Supports<AnimationData>() || !Target::template Supports<TextureCrop>()
	) {
		return false;
	} else {
		const auto animation{ target.template Capture<AnimationData>() };

		if (!animation) {
			return false;
		}

		auto before_crop{ target.template Capture<TextureCrop>() };
		TextureCrop crop{ before_crop.value_or(TextureCrop{}) };
		crop.Update(*animation, ResolveAnimationTextureSize(target));
		target.template SetLive<TextureCrop>(crop);
		auto after_crop{ target.template Capture<TextureCrop>() };
		TrackComponentState(
			target, "Update Animation Texture Crop", std::move(before_crop), std::move(after_crop),
			true
		);
		return true;
	}
}

template <typename Target>
bool DisableAnimationTextureCrop(Target& target) {
	using TextureCrop = ::ptgn::impl::TextureCrop;

	if constexpr (!Target::template Supports<TextureCrop>()) {
		return false;
	} else {
		auto before_crop{ target.template Capture<TextureCrop>() };

		if (!before_crop) {
			return false;
		}

		target.template SetLive<TextureCrop>(std::nullopt);
		auto after_crop{ target.template Capture<TextureCrop>() };
		TrackComponentState(
			target, "Disable Texture Crop", std::move(before_crop), std::move(after_crop), true
		);
		return true;
	}
}

bool DrawSpriteStackData(
	EditorContext& ctx, SpriteStackData& data, std::optional<int> detected_slice_count
) {
	bool changed{ false };

	int displayed_slice_count{ detected_slice_count.value_or(data.slice_count) };

	{
		ScopedDisabled disabled{ detected_slice_count.has_value() };

		changed |= DrawValue(
			ctx, "Slice Count", displayed_slice_count,
			FieldOptions{
				.speed	= 1.0f,
				.min	= 1.0f,
				.max	= 10000.0f,
				.format = "%d",
				.flags	= ImGuiSliderFlags_AlwaysClamp,
			}
		);
	}

	if (detected_slice_count) {
		DrawTooltip(
			"Slice count is automatically detected from the "
			"_slicesN suffix in the texture key."
		);
	} else if (changed) {
		data.slice_count = static_cast<std::size_t>(std::max(displayed_slice_count, 1));
	}

	changed |= DrawValue(ctx, "Slice Order", data.slice_order);

	changed |= DrawValue(
		ctx, "Layer Offset", data.layer_offset,
		FieldOptions{
			.speed	= 0.1f,
			.min	= -1000.0f,
			.max	= 1000.0f,
			.format = "%.2f",
		}
	);

	changed |= DrawValue(ctx, "Pixel Snap", data.pixel_snap);

	return changed;
}

template <typename Target, AssetKeyType Key>
bool DrawOptionalAssetComponent(Target& target, std::string_view label);

template <typename Target>
bool DrawSpriteStackPrimary(Target& target) {
	bool changed{ false };

	changed |= DrawOptionalAssetComponent<Target, TextureKey>(target, "Texture Key");

	const auto texture_key{ target.template Capture<TextureKey>() };

	const std::optional<std::size_t> detected_slice_count{
		texture_key ? ::ptgn::impl::DetectSpriteStackSliceCount(
						  target.ctx.editor.GetAssetManager(), *texture_key
					  )
					: std::nullopt
	};

	changed |= DrawRequiredInlineVisualComponent<Target, SpriteStackData>(
		target, "Sprite Stack", [&target, detected_slice_count](SpriteStackData& value) {
			return DrawSpriteStackData(target.ctx, value, detected_slice_count);
		}
	);

	return changed;
}

template <typename Target>
bool DrawSpritePrimary(Target& target) {
	using AnimationData = ::ptgn::impl::AnimationData;

	bool changed{ false };
	const auto before_texture{ target.template Capture<TextureKey>() };
	const auto before_animation{ target.template Capture<AnimationData>() };
	const auto before_texture_size{ target.template Capture<::ptgn::impl::TextureSize>() };

	std::optional<V2_float> texture_size_default;

	if (!before_texture_size && before_animation && before_animation->config.frame_size) {
		V2_float scale{ 1.0f };

		if constexpr (Target::template Supports<Transform>()) {
			if (const auto transform{ target.template Capture<Transform>() }) {
				scale = V2_float{ std::abs(transform->scale.x), std::abs(transform->scale.y) };
			}
		}

		const auto frame_size{ *before_animation->config.frame_size };

		texture_size_default = V2_float{ static_cast<float>(frame_size.x) * scale.x,
										 static_cast<float>(frame_size.y) * scale.y };
	}

	changed |= DrawOptionalAssetComponent<Target, TextureKey>(target, "Texture Key");

	const auto detected_frame_count{ ResolveDetectedAnimationFrameCount(target) };

	changed |= DrawOptionalComponent<Target, ::ptgn::impl::TextureSize>(
		target, "Texture Size", false,
		[&target, before_texture_size, texture_size_default](::ptgn::impl::TextureSize& value) {
			bool initialized{ false };

			if (!before_texture_size && texture_size_default) {
				initialized = SetTextureSizeFromPixels(value, *texture_size_default);
			}

			return DrawTextureSizeAsIntegers(target.ctx, value) || initialized;
		}
	);

	const auto animation_texture_size{ ResolveAnimationTextureSize(target) };

	changed |= DrawOptionalComponent<Target, AnimationData>(
		target, "Animation", true,
		[&target, detected_frame_count, animation_texture_size](AnimationData& value) {
			const std::size_t before_frame_count{ value.config.frame_count };

			const bool local_changed{ DrawAnimationDataFlattened(
				target.ctx, value, detected_frame_count, animation_texture_size
			) };

			const bool frame_count_changed{ before_frame_count != value.config.frame_count };

			if (frame_count_changed) {
				if (!value.config.frame_size.has_value()) {
					value.config.frame_size = ::ptgn::impl::GetFrameSize(
						animation_texture_size, value.config.frame_count
					);
				}

				value.current_frame = value.config.frame_count == 0
										? 0
										: value.current_frame % value.config.frame_count;

				value.frame_dirty = true;
			}

			return local_changed || frame_count_changed;
		}
	);

	const auto after_texture{ target.template Capture<TextureKey>() };
	const auto after_animation{ target.template Capture<AnimationData>() };
	const bool texture_changed{ before_texture != after_texture };
	const bool animation_enabled{ after_animation.has_value() };
	const bool animation_was_enabled{ before_animation.has_value() };
	const bool frame_count_changed{ before_animation && after_animation &&
									before_animation->config.frame_count !=
										after_animation->config.frame_count };

	if (!animation_enabled && animation_was_enabled) {
		changed |= DisableAnimationTextureCrop(target);
	} else if (animation_enabled && texture_changed) {
		changed |=
			SynchronizeAnimationFrameData(target, "Recalculate Animation Frame Size From Texture");
	} else if (animation_enabled && (!animation_was_enabled || frame_count_changed)) {
		changed |= SynchronizeAnimationTextureCrop(target);
	}

	return changed;
}

template <typename Target>
bool DrawMaterialDetails(Target& target, ::ptgn::Material& material) {
	bool changed{ false };

	changed |= DrawVectorEditor(
		target.ctx, material.uniforms,
		VectorOptions{
			.item_name = "Uniform",
			.add_label = "+ Add Uniform",
			.add_first = true,
		}
	);

	const std::size_t max_texture_slots{
		std::max(std::size_t{ 1 }, static_cast<std::size_t>(target.ctx.editor.GetMaxTextureSlots()))
	};

	changed |= DrawValue(
		target.ctx, "Texture Slot Capacity", material.texture_slot_capacity,
		FieldOptions{
			.speed	= 1.0f,
			.min	= 1.0f,
			.max	= static_cast<float>(max_texture_slots),
			.format = "%llu",
			.flags	= ImGuiSliderFlags_AlwaysClamp,
		}
	);

	if (material.texture_slot_capacity.has_value()) {
		const std::size_t clamped{
			std::clamp(*material.texture_slot_capacity, std::size_t{ 1 }, max_texture_slots)
		};

		if (*material.texture_slot_capacity != clamped) {
			material.texture_slot_capacity = clamped;
			changed						   = true;
		}
	}

	return changed;
}

template <typename Target, AssetKeyType Key>
bool DrawOptionalAssetComponent(Target& target, std::string_view label) {
	if constexpr (!Target::template Supports<Key>()) {
		return false;
	} else {
		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<Key>()) };

		auto before{ target.template Capture<Key>() };
		std::optional<Key> value{ before };

		const bool changed{ DrawOptionalAssetKeyInline(target.ctx, label, value, FieldOptions{}) };

		if (changed) {
			target.template SetLive<Key>(value);
		}

		auto after{ target.template Capture<Key>() };

		TrackComponentState(
			target, std::string{ "Edit " } + std::string{ label }, std::move(before),
			std::move(after), changed
		);

		return changed;
	}
}

template <typename Target>
bool DrawCustomShaderMaterial(Target& target, ::ptgn::Material& material) {
	std::optional<ShaderKey> shader{ material.shader.value.empty()
										 ? std::nullopt
										 : std::optional<ShaderKey>{ material.shader } };

	bool changed{ DrawOptionalAssetKeyInline(target.ctx, "Shader Key", shader, FieldOptions{}) };

	if (changed) {
		material.shader = shader.value_or(ShaderKey{});
	}

	changed |= DrawMaterialDetails(target, material);
	return changed;
}

template <typename Target>
bool DrawCustomShaderPrimary(Target& target) {
	bool changed{ false };

	if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
		auto before{ target.template Capture<::ptgn::Material>() };
		bool material_changed{ false };

		if (!before) {
			target.template SetLive<::ptgn::Material>(::ptgn::Material{});
			material_changed = true;
		}

		{
			ScopedID target_scope{ target.Id() };
			ScopedID component_scope{ static_cast<int>(Hash<::ptgn::Material>()) };
			auto& material{ target.entity.template Get<::ptgn::Material>() };
			material_changed |= DrawCustomShaderMaterial(target, material);
		}

		auto after{ target.template Capture<::ptgn::Material>() };
		TrackComponentState(
			target, "Edit Material", std::move(before), std::move(after), material_changed
		);

		changed |= material_changed;
	} else {
		changed |= DrawRequiredComponent<Target, ::ptgn::Material>(
			target, "Material", false, [&target](::ptgn::Material& material) {
				return DrawCustomShaderMaterial(target, material);
			}
		);
	}

	changed |= DrawOptionalAssetComponent<Target, TextureKey>(target, "Texture Key");

	changed |= DrawOptionalComponent<Target, Rect>(
		target, "Size", false,
		[](Rect& value) {
			V2_float size{ value.GetSize() };

			if (!DrawWHValue("Size", size, kInspectorSizeDragSpeed, 0.0f, 0.0f, "%.3f")) {
				return false;
			}

			size.x = std::max(0.0f, size.x);
			size.y = std::max(0.0f, size.y);

			const V2_float center{ value.GetCenter() };
			const V2_float half_size{ size * 0.5f };

			value.min = center - half_size;
			value.max = center + half_size;
			return true;
		},
		false, false, nullptr, Rect{ V2_float{ 100.0f, 100.0f } }
	);

	return changed;
}

template <typename Target>
bool DrawSpriteAdditional(Target& target) {
	bool changed{ false };
	const bool animated{ target.template Capture<::ptgn::impl::AnimationData>().has_value() };

	changed |= DrawOptionalVisualComponent<Target, ::ptgn::impl::Offsets>(target, "Offsets", true);

	if (animated) {
		changed |= DrawReadOnlyExistingReflected<Target, ::ptgn::impl::TextureCrop>(
			target, "Texture Crop", true
		);
	} else {
		changed |= DrawOptionalVisualComponent<Target, ::ptgn::impl::TextureCrop>(
			target, "Texture Crop", true
		);
	}

	return changed;
}

template <typename Target, typename T>
bool DrawOptionalNamedValue(Target& target, std::string_view label) {
	return DrawOptionalComponent<Target, T>(target, label, false, [&target, label](T& value) {
		return [&target, label]<typename Value>(Value& reflected_value) {
			if constexpr (ReflectedValue<Value>) {
				auto member{ ReflectValue(reflected_value) };
				return DrawValue(target.ctx, label, member.value);
			} else if constexpr (ReflectedMembers<Value>) {
				auto members{ ReflectMembers(reflected_value) };

				if constexpr (std::tuple_size_v<decltype(members)> == 1) {
					auto& member{ std::get<0>(members) };
					return DrawValue(target.ctx, label, member.value);
				} else {
					return DrawMembers(target.ctx, reflected_value);
				}
			} else {
				return DrawDefaultContents(target.ctx, reflected_value);
			}
		}(value);
	});
}

template <typename Target>
bool DrawRenderTargetPrimary(Target& target) {
	bool changed{ false };

	const bool primary_scene_target{ IsPrimarySceneRenderTarget(target) };

	std::optional<V2_int> framebuffer_size;

	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		if (entity && entity.Has<::ptgn::impl::FramebufferObject>()) {
			const V2_int actual_size{ RenderTarget{ entity }.GetSize() };

			if (actual_size.IsPositive()) {
				framebuffer_size = actual_size;
			}
		}
	}

	if (primary_scene_target) {
		changed |= DrawRequiredInlineVisualComponent<Target, ::ptgn::impl::RenderTargetDesc>(
			target, "Render Target",
			[&target, framebuffer_size](::ptgn::impl::RenderTargetDesc& value) {
				return DrawRenderTargetDesc(target.ctx, value, framebuffer_size, true);
			}
		);
	} else {
		changed |= DrawOptionalComponent<Target, ::ptgn::impl::RenderTargetDesc>(
			target, "Render Target", false,
			[&target, framebuffer_size](::ptgn::impl::RenderTargetDesc& value) {
				return DrawRenderTargetDesc(target.ctx, value, framebuffer_size);
			}
		);
	}

	changed |= DrawOptionalNamedValue<Target, ::ptgn::impl::ClearColor>(target, "Clear Color");

	changed |= DrawOptionalNamedValue<Target, ::ptgn::impl::ClearDepth>(target, "Clear Depth");

	changed |= DrawOptionalNamedValue<Target, ::ptgn::impl::ClearStencil>(target, "Clear Stencil");

	return changed;
}

template <typename Target>
bool DrawVisualLayers(Target& target) {
	if constexpr (
		!Target::template Supports<::ptgn::impl::RenderMask>() ||
		!Target::template Supports<::ptgn::impl::UILayer>()
	) {
		return false;
	} else {
		auto before_mask{ target.template Capture<::ptgn::impl::RenderMask>() };
		auto before_ui{ target.template Capture<::ptgn::impl::UILayer>() };

		::ptgn::impl::RenderMask mask{ before_mask.value_or(::ptgn::impl::RenderMask{}) };
		bool ui_layer{ before_ui.has_value() };

		ScopedID target_scope{ target.Id() };
		ScopedID layers_scope{ "VisualLayers" };

		const bool changed{ DrawLayerMaskValue("Layers", mask.layers, ui_layer) };

		if (!changed) {
			return false;
		}

		target.template SetLive<::ptgn::impl::RenderMask>(mask);

		ComponentState<::ptgn::impl::UILayer> ui_state;
		if (ui_layer) {
			ui_state = ::ptgn::impl::UILayer{};
		}
		target.template SetLive<::ptgn::impl::UILayer>(ui_state);

		auto after_mask{ target.template Capture<::ptgn::impl::RenderMask>() };
		auto after_ui{ target.template Capture<::ptgn::impl::UILayer>() };

		auto apply_mask{ target.template MakeApply<::ptgn::impl::RenderMask>() };
		auto apply_ui{ target.template MakeApply<::ptgn::impl::UILayer>() };
		const ImGuiID key{ ImGui::GetID("##VisualLayersEdit") };

		TrackUndoableInteraction(
			target.ctx, key, "Edit Layers", true,
			[apply_mask, apply_ui, before_mask, before_ui]() mutable {
				apply_mask(before_mask);
				apply_ui(before_ui);
			},
			[apply_mask, apply_ui, after_mask, after_ui]() mutable {
				apply_mask(after_mask);
				apply_ui(after_ui);
			}
		);

		return true;
	}
}

template <typename Target>
bool DrawVisualAdditionalOptions(Target& target, std::string_view visual, bool draw_tint) {
	const bool open{
		ImGui::TreeNodeEx("Additional Options##Visual", ImGuiTreeNodeFlags_SpanAvailWidth)
	};

	if (!open) {
		return false;
	}

	bool changed{ false };

	{
		ScopedUnindent align_with_additional_options;

		if (visual.contains("sprite")) {
			changed |= DrawSpriteAdditional(target);
		}

		if (visual == "rect" || visual == "circle" || visual == "sprite") {
			changed |= DrawOptionalVisualComponent<Target, ::ptgn::impl::ShadowCaster>(
				target, "Shadow Caster", true
			);
		}

		if (visual.contains("text")) {
			ScopedID text_additional_scope{ "TextAdditional" };
			changed |= DrawRequiredComponent<Target, ::ptgn::impl::TextData>(
				target, "Text Additional Options", false,
				[&target](::ptgn::impl::TextData& value) {
					return DrawTextAdditional(target, value);
				},
				&MarkTextLayoutDirty
			);
		}

		changed |= DrawOptionalVisualComponent<Target, BlendMode>(target, "Blend Mode");

		if (draw_tint) {
			changed |= DrawOptionalVisualComponent<Target, Tint>(target, "Tint");

			changed |= DrawOptionalVisualComponent<Target, ::ptgn::impl::IgnoreParentTint>(
				target, "Ignore Parent Tint"
			);
		}

		if (!IsPrimarySceneRenderTarget(target)) {
			changed |= DrawOptionalVisualComponent<Target, ::ptgn::impl::IgnoreParentVisibility>(
				target, "Ignore Parent Visibility"
			);

			changed |= DrawVisualLayers(target);
		}
	}

	ImGui::TreePop();
	return changed;
}

template <typename Target, typename Visuals, typename Draw, typename Callback>
bool DrawButtonChildStateVisualComponent(
	Target& target, ButtonVisualState state, std::string_view part_label, Draw&& draw,
	Callback callback, bool draw_visual_separator = true
) {
	if constexpr (!Target::template Supports<Visuals>()) {
		return false;
	} else {
		if (draw_visual_separator) {
			ImGui::SeparatorText("Visual");
		}
		AutoLabelWidthScope label_width{ "ButtonVisualStateVisualFields" };
		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<Visuals>()) };

		auto before{ target.template Capture<Visuals>() };
		Visuals visuals{ before.value_or(Visuals{}) };
		const auto index{ static_cast<std::size_t>(std::to_underlying(state)) };
		auto& visual{ visuals.states[index] };

		const bool changed{ std::invoke(std::forward<Draw>(draw), visuals, visual) };

		if (changed) {
			// The part checkbox owns whether this visual state is defined. Editing or clearing an
			// individual optional property must never disable the entire part.
			visual.defined = true;
			target.template SetLive<Visuals>(ComponentState<Visuals>{ visuals }, callback);
		}

		auto after{ target.template Capture<Visuals>() };
		TrackComponentState(
			target, std::string{ "Edit " } + std::string{ part_label } + " State Visual",
			std::move(before), std::move(after), changed, callback
		);

		return changed;
	}
}


template <typename Target>
bool DrawButtonChildStateVisualFeature(
	Target& target, const ButtonChildInfo& child_info, ButtonVisualState state
) {
	switch (child_info.part) {
		case ButtonChildPart::Background:
			return DrawButtonChildStateVisualComponent<Target, ButtonBackgroundVisuals>(
				target, state, "Button Background",
				[&target, state](auto& visuals, ButtonShapeVisual&) {
					bool changed{ false };
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Size", visuals.states, state, &ButtonShapeVisual::size
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Origin", visuals.states, state, &ButtonShapeVisual::origin
					);
					DrawTooltip("Local origin used by this part.");
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Anchor", visuals.states, state, &ButtonShapeVisual::anchor
					);
					DrawTooltip("Point on the button this part is anchored to.");
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Color", visuals.states, state, &ButtonShapeVisual::color
					);
					return changed;
				},
				&MarkButtonBackgroundDirty
			);

		case ButtonChildPart::Border:
			return DrawButtonChildStateVisualComponent<Target, ButtonBorderVisuals>(
				target, state, "Button Border",
				[&target, &child_info, state](auto& visuals, ButtonShapeVisual&) {
					bool changed{ false };
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Size", visuals.states, state, &ButtonShapeVisual::size
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Origin", visuals.states, state, &ButtonShapeVisual::origin
					);
					DrawTooltip("Local origin used by this part.");
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Anchor", visuals.states, state, &ButtonShapeVisual::anchor
					);
					DrawTooltip("Point on the button this part is anchored to.");
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Color", visuals.states, state, &ButtonShapeVisual::color
					);
					changed |= DrawButtonBorderLineWidth(child_info.button, visuals.states, state);
					return changed;
				},
				&MarkButtonBorderDirty
			);

		case ButtonChildPart::Text:
			return DrawButtonChildStateVisualComponent<Target, ButtonTextVisuals>(
				target, state, "Button Text",
				[&target, state](ButtonTextVisuals& visuals, ButtonTextVisual& visual) {
					bool changed{ false };

					// Draw the managed text using the same TextData component drawer used by ordinary
					// Text entities. Changes to content/text-box data are copied back into the active
					// button visual state, while TextData-only properties stay on the managed entity.
					auto text_before{ target.template Capture<::ptgn::impl::TextData>() };
					::ptgn::impl::TextData text_data{
						text_before.value_or(::ptgn::impl::TextData{})
					};
					const ::ptgn::impl::TextData displayed_before{ text_data };
					if (DrawInspectorValueContents(
						target.ctx, Hash<::ptgn::impl::TextData>(), std::addressof(text_data)
					)) {
						if (text_data.text != displayed_before.text) {
						visual.styled_text = text_data.text;
					}
						if (text_data.box != displayed_before.box) {
							visual.box = text_data.box;
						}
						visual.defined = true;
						target.template SetLive<::ptgn::impl::TextData>(
							text_data, &MarkTextLayoutDirty
						);
						changed = true;
					}

					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Origin", visuals.states, state, &ButtonTextVisual::origin
					);
					DrawTooltip("Local origin used by this part.");

					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Anchor", visuals.states, state, &ButtonTextVisual::anchor
					);
					DrawTooltip("Point on the button this part is anchored to.");

					changed |= DrawButtonVisualTriStateBoolOverrideValue(
						target.ctx, "Auto Box", visuals.states, state, &ButtonTextVisual::auto_box
					);
					DrawTooltip("Inherit, enable, or disable automatic text-box sizing.");

					changed |= DrawButtonVisualOverrideTree(
						target.ctx, "Padding", visuals.states, state, &ButtonTextVisual::padding
					);

					return changed;
				},
				&MarkButtonTextDirty, false
			);

		case ButtonChildPart::Sprite:
			return DrawButtonChildStateVisualComponent<Target, ButtonSpriteVisuals>(
				target, state, "Button Sprite",
				[&target, state](auto& visuals, ButtonSpriteVisual&) {
					bool changed{ false };

					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Texture Key", visuals.states, state,
						&ButtonSpriteVisual::texture
					);

					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Origin", visuals.states, state, &ButtonSpriteVisual::origin
					);
					DrawTooltip("Local origin used by this part.");

					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Anchor", visuals.states, state, &ButtonSpriteVisual::anchor
					);
					DrawTooltip("Point on the button this part is anchored to.");

					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Texture Size", visuals.states, state, &ButtonSpriteVisual::size
					);

					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Tint", visuals.states, state, &ButtonSpriteVisual::tint
					);

					changed |= DrawButtonVisualOverrideTree(
						target.ctx, "Animation", visuals.states, state,
						&ButtonSpriteVisual::animation, [&target](AnimationConfig& animation) {
							return DrawInspectorValueContents(
								target.ctx, Hash<AnimationConfig>(), std::addressof(animation)
							);
						}
					);

					changed |= DrawButtonVisualOverrideTree(
						target.ctx, "Animation Options", visuals.states, state,
						&ButtonSpriteVisual::animation_options
					);

					return changed;
				},
				&MarkButtonSpriteDirty
			);
	}

	return false;
}

template <typename Target>
bool DrawVisualFeature(Target& target, bool draw_header = true) {
	if (const auto child_info{ GetButtonChildInfo(target) }) {
		if (const auto state{ GetButtonVisualEditState(target) }) {
			return DrawButtonChildStateVisualFeature(target, *child_info, *state);
		}
	}

	if (!HasVisualFeature(target)) {
		return false;
	}

	const bool primary_scene_target{ IsPrimarySceneRenderTarget(target) };

	FeatureHeaderResult header{
		.open	 = true,
		.changed = false,
	};

	if (draw_header) {
		header = DrawFeatureHeader(
			target, InspectorFeature::Visual, "Visual", ImGuiTreeNodeFlags_DefaultOpen,
			VisualFeatureComponents{}, !primary_scene_target
		);

		if (!header.open) {
			return header.changed;
		}
	} else {
		ImGui::SeparatorText("Visual");
	}

	std::optional<ScopedIndent> feature_indent;
	if (draw_header) {
		feature_indent.emplace();
	}
	AutoLabelWidthScope visual_label_width{ "VisualFeatureFields" };

	bool changed{ header.changed };
	const RendererRowResult renderer{ DrawRendererRow(target) };
	const std::string& visual{ renderer.visual };
	changed |= renderer.changed;

	if (renderer.changed) {
		return true;
	}

	if (visual.empty()) {
		ImGui::TextDisabled("Choose a renderer to expose its relevant components.");
		return changed;
	}

	changed |= DrawVisualEffects(target, visual);

	bool draw_tint{ false };
	const bool shape{ visual == "rect" || visual == "circle" || visual == "roundedrect" ||
					  visual == "polygon" || visual == "ellipse" || visual == "triangle" ||
					  visual == "line" || visual == "capsule" || visual == "arc" };

	if (shape) {
		changed |= DrawShapeVisual(target, visual);
	} else if (visual == "spritestack") {
		changed	  |= DrawSpriteStackPrimary(target);
		draw_tint  = true;
	} else if (visual.contains("sprite")) {
		changed	  |= DrawSpritePrimary(target);
		draw_tint  = true;
	} else if (visual.contains("text")) {
		ScopedID text_primary_scope{ "TextPrimary" };
		changed |= DrawRequiredInlineVisualComponent<Target, ::ptgn::impl::TextData>(
			target, "Text",
			[&target](::ptgn::impl::TextData& value) { return DrawTextPrimary(target, value); },
			&MarkTextLayoutDirty
		);
		draw_tint = true;
	} else if (visual.contains("particle")) {
		changed |= DrawRequiredInlineVisualComponent<Target, ::ptgn::impl::ParticleEmitterData>(
			target, "Particle Emitter", [&target](::ptgn::impl::ParticleEmitterData& value) {
				return DrawFlattenedConfig(target.ctx, value);
			}
		);
	} else if (visual.contains("light")) {
		changed |= DrawRequiredInlineVisualComponent<Target, LightData>(
			target, "Light", [&target](LightData& value) {
				return DrawRegisteredComponentContents(
					target.ctx, Hash<LightData>(), std::addressof(value)
				);
			}
		);
	} else if (visual.contains("customshader")) {
		changed |= DrawCustomShaderPrimary(target);
	} else if (visual.contains("graphics")) {
		if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
			auto before{ target.template Capture<::ptgn::impl::GraphicsData>() };
			bool graphics_changed{ false };

			if (!before) {
				target.template SetLive<::ptgn::impl::GraphicsData>(::ptgn::impl::GraphicsData{});
				graphics_changed = true;
			}

			{
				ScopedID target_scope{ target.Id() };
				ScopedID component_scope{ static_cast<int>(Hash<::ptgn::impl::GraphicsData>()) };
				auto& value{ target.entity.template Get<::ptgn::impl::GraphicsData>() };
				graphics_changed |= DrawRegisteredComponentContents(
					target.ctx, Hash<::ptgn::impl::GraphicsData>(), std::addressof(value)
				);
			}

			auto after{ target.template Capture<::ptgn::impl::GraphicsData>() };
			TrackComponentState(
				target, "Edit Graphics", std::move(before), std::move(after), graphics_changed
			);
			changed |= graphics_changed;
		} else {
			changed |= DrawRequiredInlineVisualComponent<Target, ::ptgn::impl::GraphicsData>(
				target, "Graphics", [&target](::ptgn::impl::GraphicsData& value) {
					return DrawRegisteredComponentContents(
						target.ctx, Hash<::ptgn::impl::GraphicsData>(), std::addressof(value)
					);
				}
			);
		}
	} else if (visual.contains("rendertarget")) {
		changed |= DrawRenderTargetPrimary(target);
	}

	if (primary_scene_target) {
		const Origin origin{ target.template Capture<Origin>().value_or(Origin::Center) };

		DrawReadOnlyValue(target.ctx, "Origin", origin);
	} else {
		changed |= DrawOptionalVisualComponent<Target, Origin>(target, "Origin");
	}
	changed |= DrawVisualAdditionalOptions(target, visual, draw_tint);

	return changed;
}

template <typename Target>
bool DrawReadOnlyInteractionLock(Target& target) {
	if (!target.ctx.local.settings.show_read_only_inspector_data) {
		return false;
	}

	if constexpr (!Target::template Supports<InteractionLock>()) {
		return false;
	} else {
		const auto state{ target.template Capture<InteractionLock>() };
		bool enabled{ state.has_value() };
		const InteractionLock value{ state.value_or(InteractionLock{}) };

		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<InteractionLock>()) };

		{
			ScopedDisabled disabled{ true };
			ImGui::Checkbox("##Enabled", &enabled);
		}

		DrawTooltip("Interaction Lock is managed by the runtime.");
		ImGui::SameLine();

		if (ImGui::TreeNodeEx(
				"Interaction Lock##ReadOnlyInteractionLock", ImGuiTreeNodeFlags_SpanAvailWidth
			)) {
			ScopedIndent indent;
			AutoLabelWidthScope label_width{ "InteractionLockReadOnlyFields" };
			ReadOnlyScope read_only{ true };

			[&]<typename T>(const T& reflected_value) {
				if constexpr (ReflectedMembers<T>) {
					auto members{ ReflectMembers(reflected_value) };
					std::apply(
						[&](auto&&... member) {
							(DrawReadOnlyValue(target.ctx, PrettyName(member.name), member.value),
							 ...);
						},
						members
					);
				}

				if constexpr (ReflectedReadOnlyMembers<T>) {
					auto members{ ReflectReadOnlyMembers(reflected_value) };
					std::apply(
						[&](auto&&... member) {
							(DrawReadOnlyValue(target.ctx, PrettyName(member.name), member.value),
							 ...);
						},
						members
					);
				}
			}(value);

			ImGui::TreePop();
		}

		return false;
	}
}

template <typename Target>
bool DrawInteractionFeature(Target& target) {
	if (!HasInteractionFeature(target)) {
		return false;
	}

	const auto header{ DrawFeatureHeader(
		target, InspectorFeature::Interaction, "Interaction", ImGuiTreeNodeFlags_None,
		InteractionFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;

	bool changed{ header.changed };

	changed |=
		DrawOptionalReflected<Target, ::ptgn::impl::Interactive>(target, "Interactive", true);
	changed |= DrawOptionalReflected<Target, ::ptgn::impl::Draggable>(target, "Draggable", true);
	changed |= DrawOptionalReflected<Target, ::ptgn::impl::Dropzone>(target, "Dropzone", true);
	changed |= DrawReadOnlyInteractionLock(target);

	return changed;
}

template <typename Target>
bool DrawRigidBodyWithInheritance(Target& target) {
	bool inheritance_changed{ false };

	const bool changed{ DrawOptionalComponent<Target, RigidBody>(
		target, "Rigid Body", true, [&](RigidBody& value) {
			const bool rigid_body_changed{ DrawRegisteredComponentContents(
				target.ctx, Hash<RigidBody>(), std::addressof(value)
			) };

			inheritance_changed |=
				DrawOptionalReflected<Target, ::ptgn::impl::IgnoreParentImmovable>(
					target, "Ignore Parent Immovable", false
				);

			return rigid_body_changed;
		}
	) };

	if (!target.template Capture<RigidBody>() &&
		target.template Capture<::ptgn::impl::IgnoreParentImmovable>()) {
		auto before{ target.template Capture<::ptgn::impl::IgnoreParentImmovable>() };
		target.template SetLive<::ptgn::impl::IgnoreParentImmovable>(std::nullopt);
		auto after{ target.template Capture<::ptgn::impl::IgnoreParentImmovable>() };

		TrackComponentState(
			target, "Remove Ignore Parent Immovable", std::move(before), std::move(after), true
		);

		inheritance_changed = true;
	}

	return changed || inheritance_changed;
}

template <typename Enum>
bool DrawPlatformerEnumCombo(const char* id, Enum& value) {
	const std::string preview{ PrettyName(magic_enum::enum_name(value)) };
	ImGui::SetNextItemWidth(-FLT_MIN);

	if (!ImGui::BeginCombo(id, preview.c_str())) {
		return false;
	}

	bool changed{ false };
	for (const auto candidate : magic_enum::enum_values<Enum>()) {
		const std::string label{ PrettyName(magic_enum::enum_name(candidate)) };
		if (ImGui::Selectable(label.c_str(), candidate == value)) {
			value	= candidate;
			changed = true;
		}
	}

	ImGui::EndCombo();
	return changed;
}

template <typename Target>
Entity GetInspectorTargetEntity(Target& target) {
	if constexpr (requires { target.entity; }) {
		return target.entity;
	} else {
		return {};
	}
}

bool DrawColliderMasks(std::vector<ColliderMask>& masks) {
	bool changed{ false };
	std::optional<std::size_t> remove_index;

	if (masks.empty()) {
		ImGui::TextDisabled("Any collision mask");
	}

	for (std::size_t i{ 0 }; i < masks.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		const float remove_width{ ImGui::GetFrameHeight() };
		const float spacing{ ImGui::GetStyle().ItemSpacing.x };
		ImGui::SetNextItemWidth(
			std::max(60.0f, ImGui::GetContentRegionAvail().x - remove_width - spacing)
		);
		changed |= ImGui::InputScalar("##Mask", ImGuiDataType_S64, std::addressof(masks[i]));
		ImGui::SameLine();
		if (ImGui::Button("X", ImVec2{ remove_width, remove_width })) {
			remove_index = i;
		}
		ImGui::PopID();
	}

	if (remove_index.has_value()) {
		masks.erase(masks.begin() + static_cast<std::ptrdiff_t>(remove_index.value()));
		changed = true;
	}

	if (ImGui::Button("+ Mask")) {
		masks.emplace_back(0);
		changed = true;
	}

	return changed;
}

template <typename Target>
bool DrawPlatformerMovementContents(Target& target, PlatformerMovement& value) {
	bool changed{ false };

	if (ImGui::TreeNodeEx(
			"Horizontal Movement",
			ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth
		)) {
		auto draw_float = [&](const char* label, float& field, float speed = 1.0f) {
			changed |= DrawPropertyRow(label, [&]() {
				ImGui::SetNextItemWidth(-FLT_MIN);
				return ImGui::DragFloat("##Value", &field, speed);
			});
		};

		draw_float("Max Speed", value.max_speed);
		draw_float("Acceleration", value.max_acceleration);
		draw_float("Deceleration", value.max_deceleration);
		draw_float("Turn Speed", value.max_turn_speed);
		draw_float("Air Acceleration", value.max_air_acceleration);
		draw_float("Air Deceleration", value.max_air_deceleration);
		draw_float("Air Turn Speed", value.max_air_turn_speed);
		draw_float("Friction", value.friction, 0.05f);

		changed |= DrawPropertyRow("Use Acceleration", [&]() {
			return ImGui::Checkbox("##Value", &value.use_acceleration);
		});
		changed |= DrawPropertyRow("Left Key", [&]() {
			return DrawPlatformerEnumCombo("##Value", value.left_key);
		});
		changed |= DrawPropertyRow("Right Key", [&]() {
			return DrawPlatformerEnumCombo("##Value", value.right_key);
		});

		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx(
			"Grounding", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth
		)) {
		auto& grounding{ value.grounding };

		changed |= DrawPropertyRow("Enabled", [&]() {
			return ImGui::Checkbox("##Value", &grounding.enabled);
		});
		changed |= DrawPropertyRow("Direction", [&]() {
			return DrawPlatformerEnumCombo("##Value", grounding.direction);
		});
		changed |= DrawPropertyRow("Max Slope Angle", [&]() {
			ImGui::SetNextItemWidth(-FLT_MIN);
			return ImGui::DragFloat(
				"##Value", &grounding.max_angle.value, 1.0f, 0.0f, 180.0f, "%.1f deg",
				ImGuiSliderFlags_AlwaysClamp
			);
		});
		changed |= DrawPropertyRow("Match Entity", [&]() {
			return DrawPlatformerEnumCombo("##Value", grounding.entity_scope);
		});

		if (ImGui::TreeNodeEx("Collision Masks", ImGuiTreeNodeFlags_SpanAvailWidth)) {
			changed |= DrawColliderMasks(grounding.masks);
			ImGui::TreePop();
		}

		Entity owner{ GetInspectorTargetEntity(target) };
		changed |= DrawPropertyRow("Ground Entities", [&]() {
			auto& state{ GetManualFeatureState(target.GetFeatureTargetKey()) };
			Scene* scene{ owner ? std::addressof(owner.GetScene()) : nullptr };
			return DrawEntityFilterButton(
				scene, owner, grounding.entities, state.grounding_filter_state
			);
		});

		ImGui::TreePop();
	}

	return changed;
}

template <typename Target>
bool DrawPhysicsFeature(Target& target) {
	RegisterBuiltInPlatformerJumpControllers();

	if (Entity live_entity{ GetInspectorTargetEntity(target) }) {
		SyncPlatformerJumpController(live_entity);
	}

	if (!HasPhysicsFeature(target)) {
		return false;
	}

	const auto header{ DrawFeatureHeader(
		target, InspectorFeature::Physics, "Physics & Movement", ImGuiTreeNodeFlags_None,
		PhysicsFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;

	bool changed{ header.changed };

	changed |= DrawOptionalComponent<Target, Collider>(
		target, "Collider", true,
		[&target](Collider& value) { return DrawGeometryComponent(target, value); }
	);
	changed |= DrawRigidBodyWithInheritance(target);
	changed |= DrawOptionalReflected<Target, BoundaryBehavior>(target, "Boundary Behavior", false);

	enum class MovementKind {
		TopDown,
		Platformer,
	};

	using MovementComponents =
		FeatureComponents<TopDownMovement, PlatformerMovement, PlatformerJump>;

	const bool has_top_down{ target.template Capture<TopDownMovement>().has_value() };
	const bool has_platformer{ target.template Capture<PlatformerMovement>().has_value() };
	bool movement_enabled{ has_top_down || has_platformer };
	MovementKind movement{ has_platformer ? MovementKind::Platformer : MovementKind::TopDown };

	auto apply_movement_change = [&](auto&& mutate) {
		constexpr MovementComponents components{};
		auto before{ CaptureInspectorFeatureState(target, InspectorFeature::Physics, components) };

		std::invoke(std::forward<decltype(mutate)>(mutate));

		Entity live_entity{ GetInspectorTargetEntity(target) };
		if (live_entity) {
			SyncPlatformerJumpController(live_entity);
		}

		auto after{ CaptureInspectorFeatureState(target, InspectorFeature::Physics, components) };

		auto apply{ MakeInspectorFeatureApply(target, InspectorFeature::Physics, components) };
		auto sync = [live_entity]() {
			if (live_entity) {
				SyncPlatformerJumpController(live_entity);
			}
		};

		target.ctx.undo.PushApplied(
			"Change Movement",
			[apply, before, sync]() mutable {
				apply(before);
				sync();
			},
			[apply, after, sync]() mutable {
				apply(after);
				sync();
			}
		);

		changed = true;
	};

	ScopedID movement_scope{ "Movement" };

	if (ImGui::Checkbox("##Enabled", &movement_enabled)) {
		apply_movement_change([&]() {
			if (movement_enabled) {
				target.template SetLive<TopDownMovement>(TopDownMovement{});
				target.template SetLive<PlatformerMovement>(std::nullopt);
			} else {
				SetFeatureManuallyAdded(
					target.GetFeatureTargetKey(), InspectorFeature::Physics, true
				);
				target.template SetLive<TopDownMovement>(std::nullopt);
				target.template SetLive<PlatformerMovement>(std::nullopt);
				target.template SetLive<PlatformerJump>(std::nullopt);
			}
		});

		movement = MovementKind::TopDown;
	}

	ImGui::SameLine();

	const bool movement_open{
		ImGui::TreeNodeEx("Movement##Tree", ImGuiTreeNodeFlags_SpanAvailWidth)
	};

	if (movement_open) {
		ScopedIndent movement_indent;
		ScopedPropertyLabelOffset movement_label_offset{ ImGui::GetStyle().IndentSpacing };
		AutoLabelWidthScope movement_label_width{ "MovementFields" };
		ScopedDisabled movement_disabled{ !movement_enabled };

		const char* preview{ movement == MovementKind::Platformer ? "Platformer" : "Top Down" };

		DrawPropertyRow("Type", [&]() {
			bool local_changed{ false };

			if (ImGui::BeginCombo("##MovementType", preview)) {
				auto choose = [&](MovementKind candidate, const char* label) {
					if (!ImGui::Selectable(label, movement == candidate)) {
						return;
					}

					apply_movement_change([&]() {
						if (candidate == MovementKind::TopDown) {
							target.template SetLive<TopDownMovement>(TopDownMovement{});
							target.template SetLive<PlatformerMovement>(std::nullopt);
							target.template SetLive<PlatformerJump>(std::nullopt);
						} else {
							target.template SetLive<PlatformerMovement>(PlatformerMovement{});
							target.template SetLive<TopDownMovement>(std::nullopt);
						}
					});

					movement	  = candidate;
					local_changed = true;
				};

				choose(MovementKind::TopDown, "Top Down");
				choose(MovementKind::Platformer, "Platformer");

				ImGui::EndCombo();
			}

			return local_changed;
		});

		if (movement_enabled && target.template Capture<TopDownMovement>()) {
			changed |= DrawRequiredComponent<Target, TopDownMovement>(
				target, "Top Down Controller", false,
				[&target](TopDownMovement& value) { return DrawDefaultContents(target.ctx, value); }
			);
		}

		if (movement_enabled && target.template Capture<PlatformerMovement>()) {
			changed |= DrawRequiredComponent<Target, PlatformerMovement>(
				target, "Platformer Controller", false, [&target](PlatformerMovement& value) {
					return DrawPlatformerMovementContents(target, value);
				}
			);

			if (ImGui::TreeNodeEx(
					"Jump", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth
				)) {
				auto platformer{ target.template Capture<PlatformerMovement>().value() };
				std::string selected_key{ platformer.jump_controller };

				const auto* selected_controller{
					PlatformerJumpControllerRegistry::Find(selected_key)
				};
				const std::string jump_preview{ selected_controller ? selected_controller->label
																	: "None" };

				DrawPropertyRow("Controller", [&]() {
					bool local_changed{ false };
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (!ImGui::BeginCombo("##JumpController", jump_preview.c_str())) {
						return false;
					}

					auto choose = [&](std::string_view key, const char* label) {
						const bool selected{ selected_key == key };
						if (!ImGui::Selectable(label, selected)) {
							return;
						}

						apply_movement_change([&]() {
							auto value{ target.template Capture<PlatformerMovement>().value() };
							value.jump_controller = key;
							target.template SetLive<PlatformerMovement>(value);

							if (key == "standard" && !target.template Capture<PlatformerJump>()) {
								target.template SetLive<PlatformerJump>(PlatformerJump{});
							}
						});

						selected_key  = key;
						local_changed = true;
					};

					choose({}, "None");
					ImGui::Separator();

					auto controllers{ PlatformerJumpControllerRegistry::Controllers() };
					std::ranges::sort(controllers, [](const auto& lhs, const auto& rhs) {
						if (lhs.group != rhs.group) {
							return lhs.group < rhs.group;
						}
						return lhs.label < rhs.label;
					});

					std::string current_group;
					for (const auto& controller : controllers) {
						if (controller.group != current_group) {
							if (!current_group.empty()) {
								ImGui::Separator();
							}
							current_group = controller.group;
							if (!current_group.empty()) {
								ImGui::TextDisabled("%s", current_group.c_str());
							}
						}
						choose(controller.key, controller.label.c_str());
					}

					ImGui::EndCombo();
					return local_changed;
				});

				if (selected_key == "standard" && target.template Capture<PlatformerJump>()) {
					changed |= DrawRequiredComponent<Target, PlatformerJump>(
						target, "Standard Jump", false, [&target](PlatformerJump& value) {
							return DrawDefaultContents(target.ctx, value);
						}
					);
				} else if (!selected_key.empty()) {
					Entity entity{ GetInspectorTargetEntity(target) };
					const auto* controller{ PlatformerJumpControllerRegistry::Find(selected_key) };
					if (entity && controller && controller->has(entity)) {
						if (void* data{ controller->get(entity) }) {
							changed |= DrawRegisteredComponentContents(
								target.ctx, controller->component_hash, data
							);
						}
					}
				}

				ImGui::TreePop();
			}

			if (Entity owner{ GetInspectorTargetEntity(target) };
				owner && owner.Has<PlatformerMovement>()) {
				const auto& live{ owner.Get<PlatformerMovement>() };
				ImGui::SeparatorText("Runtime");
				ImGui::TextDisabled("Grounded: %s", live.IsGrounded() ? "Yes" : "No");
				if (Entity ground{ live.GetGroundEntity() }) {
					const std::string ground_name{ ground.Has<Tag>() &&
														   !ground.Get<Tag>().value.empty()
													   ? ground.Get<Tag>().value
													   : "Entity" };
					const V2_float normal{ live.GetGroundNormal() };
					ImGui::TextDisabled("Ground Entity: %s", ground_name.c_str());
					ImGui::TextDisabled("Ground Normal: %.2f, %.2f", normal.x, normal.y);
				}
			}
		}

		ImGui::TreePop();
	}

	return changed;
}

bool DrawButtonVisualStateSelector(std::optional<ButtonVisualState>& state) {
	return DrawPropertyRow("Visual State", [&]() {
		const std::string preview{ state ? PrettyName(magic_enum::enum_name(*state))
										 : "Base Entity" };

		ImGui::SetNextItemWidth(-FLT_MIN);

		if (!ImGui::BeginCombo("##ButtonVisualState", preview.c_str())) {
			return false;
		}

		bool changed{ false };
		const bool base_selected{ !state };

		if (ImGui::Selectable("Base Entity", base_selected)) {
			state.reset();
			changed = !base_selected;
		}

		ImGui::Separator();

		for (const auto candidate : magic_enum::enum_values<ButtonVisualState>()) {
			const bool selected{ state && *state == candidate };
			const std::string label{ PrettyName(magic_enum::enum_name(candidate)) };

			if (ImGui::Selectable(label.c_str(), selected)) {
				state	= candidate;
				changed = !selected;
			}
		}

		ImGui::EndCombo();
		return changed;
	});
}

template <typename Target>
bool DrawSliderWorldPosition(
	Target& target, ::ptgn::impl::SliderData& data, std::string_view label, bool start
) {
	V2_float& position{ start ? data.line.start : data.line.end };
	const V2_float previous{ position };

	const bool changed{ DrawPropertyRow(label, [&]() {
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float pick_width{ ImGui::CalcTextSize("Pick").x +
								ImGui::GetStyle().FramePadding.x * 2.0f };
		const float available{ ImGui::GetContentRegionAvail().x };
		const float field_width{
			std::max(36.0f, (available - pick_width - spacing * 2.0f) * 0.5f)
		};
		bool local_changed{ false };

		ImGui::SetNextItemWidth(field_width);
		local_changed |= ImGui::DragFloat("##X", &position.x, 0.01f, 0.0f, 0.0f, "X: %.2f");
		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(field_width);
		local_changed |= ImGui::DragFloat("##Y", &position.y, 0.01f, 0.0f, 0.0f, "Y: %.2f");
		ImGui::SameLine(0.0f, spacing);

		ScopedDisabled disabled{ !CanPickLocalPosition<Target>() };
		auto apply{ target.template MakeApply<::ptgn::impl::SliderData>() };
		::ptgn::impl::SliderData snapshot{ data };
		PositionPicker::Convert convert{};
		std::optional<V2_float> reference_world{};

		if constexpr (requires { target.entity; }) {
			Entity slider_entity{ target.entity };
			if (slider_entity) {
				Entity basis{ slider_entity };
				if (Entity track{ Slider{ slider_entity }.GetTrack() };
					track && track.Has<::ptgn::impl::SliderTrackData>() &&
					track.Get<::ptgn::impl::SliderTrackData>().transform_enabled) {
					basis = track;
				}

				const Transform basis_world{ GetWorldTransform(basis) };
				reference_world = basis_world.Apply(position);
				convert = [basis](V2_float world) -> std::optional<V2_float> {
					if (!basis) {
						return std::nullopt;
					}
					return GetWorldTransform(basis).ApplyInverse(world);
				};
			}
		}

		DrawPositionPickButton(
			target.ctx, label, position, std::move(convert),
			PositionPicker::Apply{
				[apply, snapshot = std::move(snapshot), start](V2_float picked) mutable {
					Line line{ snapshot.line };
					if (start) {
						line.start = picked;
					} else {
						line.end = picked;
					}
					if (line.GetDirection().IsZero()) {
						return;
					}
					snapshot.line = line;
					apply(ComponentState<::ptgn::impl::SliderData>{ snapshot });
				} },
			reference_world, true
		);
		return local_changed;
	}) };

	if (changed && data.line.GetDirection().IsZero()) {
		position = previous;
		return false;
	}
	return changed;
}

template <typename Target>
bool DrawSliderValueTextConfig(Target& target, ::ptgn::impl::SliderData& data) {
	if (!data.value_text.has_value()) {
		return false;
	}

	SliderValueTextConfig config{ data.value_text.value() };
	bool changed{ false };

	const float preview_value{
		config.display_min + std::clamp(data.value, 0.0f, 1.0f) *
			(config.display_max - config.display_min)
	};
	char preview_buffer[96]{};
	std::snprintf(
		preview_buffer, sizeof(preview_buffer), "%.*f",
		static_cast<int>(std::min<std::uint32_t>(config.decimal_places, 9)),
		static_cast<double>(preview_value)
	);
	const std::array<RichTextVariableOption, 1> variables{
		RichTextVariableOption{
			.label = "Slider Value",
			.variable = "value",
			.preview = preview_buffer,
		}
	};

	bool config_changed{ DrawRichTextEditor(
		target.ctx, config.text.source, config.text.defaults,
		RichTextEditorOptions{ .variables = variables }
	) };

	const bool format_open{ ImGui::TreeNodeEx(
		"Value Formatting##SliderValueTextFormat",
		ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
	) };
	if (format_open) {
		ScopedIndent indent;
		config_changed |= DrawValue(target.ctx, "Default Offset", config.offset);
		config_changed |= DrawValue(target.ctx, "Display Min", config.display_min);
		config_changed |= DrawValue(target.ctx, "Display Max", config.display_max);

		int decimal_places{ static_cast<int>(std::min<std::uint32_t>(config.decimal_places, 9)) };
		if (DrawValue(
				target.ctx, "Decimal Places", decimal_places,
				FieldOptions{
					.speed	= 1.0f,
					.min	= 0.0f,
					.max	= 9.0f,
					.format = "%d",
					.flags	= ImGuiSliderFlags_AlwaysClamp,
				}
			)) {
			config.decimal_places = static_cast<std::uint32_t>(std::clamp(decimal_places, 0, 9));
			config_changed = true;
		}
		ImGui::TreePop();
	}

	if (config_changed) {
		data.value_text = config;
		changed = true;
	}

	if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
		Entity text_entity{ target.entity ? Slider{ target.entity }.GetValueTextEntity() : Entity{} };
		if (!text_entity) {
			return changed;
		}

		bool transform_enabled{
			text_entity.Has<::ptgn::impl::SliderValueTextData>()
				? text_entity.Get<::ptgn::impl::SliderValueTextData>().transform_enabled
				: false
		};
		ScopedID transform_scope{ "SliderValueTextTransform" };
		if (ImGui::Checkbox("##Enabled", &transform_enabled)) {
			auto before{ text_entity.Get<::ptgn::impl::SliderValueTextData>() };
			auto after{ before };
			after.transform_enabled = transform_enabled;
			text_entity.Get<::ptgn::impl::SliderValueTextData>() = after;
			target.ctx.undo.PushApplied(
				"Toggle Slider Value Text Transform",
				[text_entity, before]() mutable {
					if (text_entity) {
						text_entity.Get<::ptgn::impl::SliderValueTextData>() = before;
						::ptgn::impl::SliderSystem::SynchronizeEntity(text_entity);
					}
				},
				[text_entity, after]() mutable {
					if (text_entity) {
						text_entity.Get<::ptgn::impl::SliderValueTextData>() = after;
						::ptgn::impl::SliderSystem::SynchronizeEntity(text_entity);
					}
				}
			);
			::ptgn::impl::SliderSystem::SynchronizeEntity(text_entity);
			changed = true;
		}

		ImGui::SameLine();
		ImGui::BeginDisabled(!transform_enabled);
		const bool transform_open{ ImGui::TreeNodeEx(
			"Transform##SliderValueTextTransformTree",
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
		) };
		ImGui::EndDisabled();
		if (transform_open) {
			if (transform_enabled) {
				EntityInspectorTarget text_target{ .ctx = target.ctx, .entity = text_entity };
				changed |= DrawTransformFeature(text_target, false, true, false);
			}
			ImGui::TreePop();
		}

		// Source/default styling belongs to SliderValueTextConfig. Only layout/reveal/clip remain on
		// the generated Text entity so there is a single authority for the displayed rich text.
		EntityInspectorTarget text_target{ .ctx = target.ctx, .entity = text_entity };
		auto text_before{ text_target.Capture<::ptgn::impl::TextData>() };
		if (text_before) {
			auto text_data{ *text_before };
			bool text_changed{ false };
			const bool layout_open{ ImGui::TreeNodeEx(
				"Layout & Reveal##SliderValueTextLayout",
				ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
			) };
			if (layout_open) {
				ScopedIndent indent;
				text_changed |= DrawValue(text_target.ctx, "Text Box", text_data.box);
				text_changed |=
					DrawValue(text_target.ctx, "Reveal Glyph Count", text_data.glyph_count);
				text_changed |= DrawValue(text_target.ctx, "Clip", text_data.clip);
				ImGui::TreePop();
			}

			if (text_changed) {
				text_target.SetLive<::ptgn::impl::TextData>(text_data, &MarkTextLayoutDirty);
				auto text_after{ text_target.Capture<::ptgn::impl::TextData>() };
				TrackComponentState(
					text_target, "Edit Slider Value Text Layout", std::move(text_before),
					std::move(text_after), true, &MarkTextLayoutDirty
				);
				changed = true;
			}
		}
	}

	return changed;
}

template <typename Target>
bool DrawSliderData(Target& target, ::ptgn::impl::SliderData& data) {
	bool changed{ false };

	const float clamped_value{ std::clamp(data.value, 0.0f, 1.0f) };

	if (data.value != clamped_value) {
		data.value = clamped_value;
		changed	   = true;
	}

	changed |= DrawValue(
		target.ctx, "Slider Value", data.value,
		FieldOptions{
			.speed	= 0.01f,
			.min	= 0.0f,
			.max	= 1.0f,
			.format = "%.3f",
			.flags	= ImGuiSliderFlags_AlwaysClamp,
		}
	);

	changed |= DrawSliderWorldPosition(target, data, "Start", true);

	changed |= DrawSliderWorldPosition(target, data, "End", false);

	bool discrete{ data.discrete_positions >= 2 };

	if (DrawValue(target.ctx, "Discrete", discrete)) {
		data.discrete_positions = discrete ? 2u : 0u;
		changed					= true;
	}
	DrawTooltip("Snap the slider to fixed selectable values.");

	if (discrete) {
		int positions{ static_cast<int>(
			std::min<std::uint32_t>(std::max<std::uint32_t>(data.discrete_positions, 2), 1000)
		) };

		if (DrawValue(
				target.ctx, "Positions", positions,
				FieldOptions{
					.speed	= 1.0f,
					.min	= 2.0f,
					.max	= 1000.0f,
					.format = "%d",
					.flags	= ImGuiSliderFlags_AlwaysClamp,
				}
			)) {
			data.discrete_positions = static_cast<std::uint32_t>(std::clamp(positions, 2, 1000));

			changed = true;
		}
		DrawTooltip("Number of selectable values, including both endpoints.");
	} else if (data.discrete_positions != 0) {
		data.discrete_positions = 0;
		changed					= true;
	}

	return changed;
}

template <typename Target>
bool DrawOptionalSlider(Target& target) {
	using SliderData = ::ptgn::impl::SliderData;

	if (!Target::template Supports<SliderData>()) {
		return false;
	}

	ScopedID target_scope{ target.Id() };
	ScopedID component_scope{ static_cast<int>(Hash<SliderData>()) };

	auto before{ target.template Capture<SliderData>() };
	bool enabled{ before.has_value() };
	bool changed{ false };

	if (ImGui::Checkbox("##Enabled", &enabled)) {
		if (enabled) {
			SliderData data;
			// SliderData::line is local to the Track Transform.
			data.line = Line{ V2_float{}, V2_float{ 100.0f, 0.0f } };
			target.template SetLive<SliderData>(data);
		} else {
			target.template SetLive<SliderData>(std::nullopt);
		}

		changed = true;
	}

	ImGui::SameLine();

	SliderData data{ target.template Capture<SliderData>().value_or(SliderData{}) };
	const bool open{ ImGui::TreeNodeEx("Slider##Tree", ImGuiTreeNodeFlags_SpanAvailWidth) };

	if (open) {
		ScopedIndent indent;
		ScopedDisabled disabled{ !enabled };

		const bool contents_changed{ DrawSliderData(target, data) };

		if (enabled && contents_changed) {
			target.template SetLive<SliderData>(data);
			changed = true;
		}

		ImGui::TreePop();
	}

	auto after{ target.template Capture<SliderData>() };

	TrackComponentState(
		target, enabled ? "Edit Slider" : "Disable Slider", std::move(before), std::move(after),
		changed
	);

	return changed;
}

enum class FocusedUIControlType : std::uint8_t {
	None,
	Button,
	ToggleButton,
	Slider,
	Dropdown,
	Conflict,
};

template <typename Target>
bool DrawFocusedButtonAppearance(Target& target, FocusedUIControlType type);

template <typename Target>
bool DrawFocusedUIControlTypeSelector(Target& target, FocusedUIControlType current);

template <typename Target>
bool DrawUIControlConflict(Target& target);

template <typename Target, typename T>
[[nodiscard]] bool HasTargetComponent(const Target& target) {
	if constexpr (!Target::template Supports<T>()) {
		return false;
	} else {
		return target.template Capture<T>().has_value();
	}
}

template <typename Target>
[[nodiscard]] FocusedUIControlType GetFocusedUIControlType(const Target& target) {
	const bool slider{ HasTargetComponent<Target, ::ptgn::impl::SliderData>(target) };
	const bool toggle{ HasTargetComponent<Target, ::ptgn::impl::ToggleButtonData>(target) };
	const bool dropdown{ HasTargetComponent<Target, ::ptgn::impl::DropdownData>(target) };
	const int specialized_count{ static_cast<int>(slider) + static_cast<int>(toggle) +
								 static_cast<int>(dropdown) };

	if (specialized_count > 1) {
		return FocusedUIControlType::Conflict;
	}
	if (slider) {
		return FocusedUIControlType::Slider;
	}
	if (toggle) {
		return FocusedUIControlType::ToggleButton;
	}
	if (dropdown) {
		return FocusedUIControlType::Dropdown;
	}
	if (HasTargetComponent<Target, ::ptgn::impl::ButtonData>(target)) {
		return FocusedUIControlType::Button;
	}
	return FocusedUIControlType::None;
}

[[nodiscard]] std::string_view FocusedUIControlLabel(FocusedUIControlType type) {
	switch (type) {
		case FocusedUIControlType::Button:		 return "Button";
		case FocusedUIControlType::ToggleButton: return "Toggle Button";
		case FocusedUIControlType::Slider:		 return "Slider";
		case FocusedUIControlType::Dropdown:	 return "Dropdown";
		case FocusedUIControlType::Conflict:	 return "Invalid UI Control";
		case FocusedUIControlType::None:		 return "UI";
	}
	return "UI";
}

template <typename Target, typename T, typename Draw, typename Callback = std::nullptr_t>
bool DrawFocusedComponent(
	Target& target, std::string_view label, Draw&& draw, Callback callback = nullptr
) {
	if constexpr (!Target::template Supports<T>()) {
		return false;
	} else {
		auto before{ target.template Capture<T>() };
		if (!before) {
			return false;
		}

		T value{ *before };
		const bool changed{ std::invoke(std::forward<Draw>(draw), value) };
		if (!changed) {
			return false;
		}

		target.template SetLive<T>(value, callback);
		auto after{ target.template Capture<T>() };
		TrackComponentState(
			target, std::string{ "Edit " } + std::string{ label }, std::move(before),
			std::move(after), true, callback
		);
		return true;
	}
}

[[nodiscard]] ButtonVisualState ComposeButtonVisualState(int mode, ButtonState pointer_state) {
	using enum ButtonVisualState;

	if (mode == 2) {
		switch (pointer_state) {
			case ButtonState::Idle:	 return Disabled;
			case ButtonState::Hover: return DisabledHover;
			case ButtonState::Press: return DisabledPress;
		}
	}

	if (mode == 1) {
		switch (pointer_state) {
			case ButtonState::Idle:	 return Toggled;
			case ButtonState::Hover: return ToggledHover;
			case ButtonState::Press: return ToggledPress;
		}
	}

	switch (pointer_state) {
		case ButtonState::Idle:	 return Idle;
		case ButtonState::Hover: return Hover;
		case ButtonState::Press: return Press;
	}

	return Idle;
}

void DecomposeButtonVisualState(ButtonVisualState state, int& mode, ButtonState& pointer_state) {
	using enum ButtonVisualState;

	switch (state) {
		case Idle:
			mode		  = 0;
			pointer_state = ButtonState::Idle;
			break;
		case Hover:
			mode		  = 0;
			pointer_state = ButtonState::Hover;
			break;
		case Press:
			mode		  = 0;
			pointer_state = ButtonState::Press;
			break;
		case Toggled:
			mode		  = 1;
			pointer_state = ButtonState::Idle;
			break;
		case ToggledHover:
			mode		  = 1;
			pointer_state = ButtonState::Hover;
			break;
		case ToggledPress:
			mode		  = 1;
			pointer_state = ButtonState::Press;
			break;
		case Disabled:
			mode		  = 2;
			pointer_state = ButtonState::Idle;
			break;
		case DisabledHover:
			mode		  = 2;
			pointer_state = ButtonState::Hover;
			break;
		case DisabledPress:
			mode		  = 2;
			pointer_state = ButtonState::Press;
			break;
	}
}

bool DrawFocusedButtonStateSelector(
	EditorContext& ctx, const FeatureTargetKey& target_key,
	std::optional<ButtonVisualState>& selected_state, bool allow_toggled
) {
	if (!selected_state.has_value()) {
		selected_state = ButtonVisualState::Idle;
	}

	ButtonVisualState state{ selected_state.value() };
	int mode{ 0 };
	ButtonState pointer_state{ ButtonState::Idle };
	DecomposeButtonVisualState(state, mode, pointer_state);

	if (!allow_toggled && mode == 1) {
		mode		   = 0;
		selected_state = ComposeButtonVisualState(mode, pointer_state);
	}

	const std::optional<ButtonVisualState> before{ selected_state };
	bool changed{ false };
	changed |= DrawPropertyRow("Mode", [&]() {
		bool local_changed{ false };
		if (ImGui::RadioButton("Normal", mode == 0)) {
			mode		  = 0;
			local_changed = true;
		}
		if (allow_toggled) {
			ImGui::SameLine();
			if (ImGui::RadioButton("Toggled", mode == 1)) {
				mode		  = 1;
				local_changed = true;
			}
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Disabled", mode == 2)) {
			mode		  = 2;
			local_changed = true;
		}
		return local_changed;
	});

	changed |= DrawPropertyRow("Pointer", [&]() {
		bool local_changed{ false };
		if (ImGui::RadioButton("Idle", pointer_state == ButtonState::Idle)) {
			pointer_state = ButtonState::Idle;
			local_changed = true;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Hover", pointer_state == ButtonState::Hover)) {
			pointer_state = ButtonState::Hover;
			local_changed = true;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Pressed", pointer_state == ButtonState::Press)) {
			pointer_state = ButtonState::Press;
			local_changed = true;
		}
		return local_changed;
	});

	if (!changed) {
		return false;
	}

	const ButtonVisualState updated{ ComposeButtonVisualState(mode, pointer_state) };
	selected_state = updated;
	const std::optional<ButtonVisualState> after{ selected_state };

	ctx.undo.PushApplied(
		"Select UI Appearance",
		[target_key, before]() { GetManualFeatureState(target_key).button_visual_state = before; },
		[target_key, after]() { GetManualFeatureState(target_key).button_visual_state = after; },
		false
	);

	return true;
}

std::optional<EntityReference>& PreviewedButtonReference() {
	static std::optional<EntityReference> previewed;
	return previewed;
}

void ClearButtonPreviewIfDifferent(EditorContext& ctx, Entity keep = {}) {
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

void ApplyButtonPreview(Entity entity, ButtonVisualState state, EditorContext& ctx) {
	if (!entity || !entity.Has<::ptgn::impl::ButtonData>() || entity.GetScene().IsRuntime()) {
		ClearButtonPreviewIfDifferent(ctx);
		return;
	}

	ClearButtonPreviewIfDifferent(ctx, entity);
	Button{ entity }.PreviewVisualState(state);
	PreviewedButtonReference() = MakeEntityReference(entity);
}

[[nodiscard]] Entity FindButtonPart(Entity button, ButtonChildPart part) {
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

[[nodiscard]] std::string_view ButtonPartLabel(ButtonChildPart part) {
	switch (part) {
		case ButtonChildPart::Background: return "Background";
		case ButtonChildPart::Border:	  return "Border";
		case ButtonChildPart::Text:		  return "Text";
		case ButtonChildPart::Sprite:	  return "Sprite";
	}
	return "Part";
}

Entity RecordCreatedEntityPreservingSelection(EditorContext& ctx, Entity entity) {
	if (!entity) {
		return {};
	}

	const EditorSelection before_selection{ ctx.local.selection };
	Entity recorded{ ctx.commands.RecordCreatedEntity(entity, before_selection) };
	ApplyEditorSelection(ctx, before_selection);
	return recorded;
}

std::optional<Entity>& ButtonPartCloseOnNextDraw() {
	static std::optional<Entity> entity;
	return entity;
}

bool& ButtonAudioCloseOnNextDraw() {
	static bool close{ false };
	return close;
}

Entity CreateButtonPartForInspector(
	EditorContext& ctx, Entity button_entity, ButtonChildPart part, ButtonVisualState state
) {
	Button button{ button_entity };

	switch (part) {
		case ButtonChildPart::Background: (void)button.Background(state); break;
		case ButtonChildPart::Border:	  (void)button.Border(state); break;
		case ButtonChildPart::Text:		  (void)button.Text(state); break;
		case ButtonChildPart::Sprite:	  (void)button.Sprite(state); break;
	}

	Entity created{ FindButtonPart(button_entity, part) };
	Entity recorded{ RecordCreatedEntityPreservingSelection(ctx, created) };

	if (recorded) {
		ButtonPartCloseOnNextDraw() = recorded;
	}

	return recorded;
}

bool DrawFocusedButtonAddParts(EditorContext& ctx, Entity button_entity, ButtonVisualState state) {
	const std::array real_parts{
		ButtonChildPart::Background,
		ButtonChildPart::Border,
		ButtonChildPart::Text,
		ButtonChildPart::Sprite,
	};

	const auto state_index{
		static_cast<std::size_t>(std::to_underlying(state))
	};

	std::array<bool, 4> missing_real{};
	std::size_t missing_count{ 0 };

	for (std::size_t i{ 0 }; i < real_parts.size(); ++i) {
		missing_real[i] = !FindButtonPart(button_entity, real_parts[i]);
		missing_count += static_cast<std::size_t>(missing_real[i]);
	}

	bool audio_missing{ true };
	if (button_entity && button_entity.Has<ButtonSounds>()) {
		audio_missing = !button_entity.Get<ButtonSounds>().states[state_index].has_value();
	}
	missing_count += static_cast<std::size_t>(audio_missing);

	if (missing_count == 0) {
		return false;
	}

	bool changed{ false };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float available{ ImGui::GetContentRegionAvail().x };
	const float total_spacing{
		spacing * static_cast<float>(missing_count > 0 ? missing_count - 1 : 0)
	};
	const float button_width{
		std::max(
			1.0f,
			(available - total_spacing) / static_cast<float>(missing_count)
		)
	};

	std::size_t drawn{ 0 };
	auto same_line_if_needed = [&]() {
		++drawn;
		if (drawn < missing_count) {
			ImGui::SameLine(0.0f, spacing);
		}
	};

	for (std::size_t i{ 0 }; i < real_parts.size(); ++i) {
		if (!missing_real[i]) {
			continue;
		}

		const ButtonChildPart part{ real_parts[i] };
		std::string button_label{ "+ Add " };
		button_label += ButtonPartLabel(part);
		button_label += "##FocusedButtonAddPart";
		button_label += std::to_string(std::to_underlying(part));

		if (ImGui::Button(button_label.c_str(), ImVec2{ button_width, 0.0f })) {
			changed |= static_cast<bool>(
				CreateButtonPartForInspector(ctx, button_entity, part, state)
			);
		}
		same_line_if_needed();
	}

	if (audio_missing) {
		if (ImGui::Button(
				"+ Add Audio##FocusedButtonAddAudio",
				ImVec2{ button_width, 0.0f }
			)) {
			EntityInspectorTarget button_target{
				.ctx = ctx,
				.entity = button_entity,
			};

			auto before{ button_target.Capture<ButtonSounds>() };
			ButtonSounds sounds{ before.value_or(ButtonSounds{}) };
			sounds.states[state_index].emplace(AudioKey{});
			button_target.SetLive<ButtonSounds>(sounds);
			auto after{ button_target.Capture<ButtonSounds>() };

			TrackComponentState(
				button_target, "Add Button Audio",
				std::move(before), std::move(after), true
			);
			ButtonAudioCloseOnNextDraw() = true;
			changed = true;
		}
		same_line_if_needed();
	}

	ImGui::Spacing();
	return changed;
}

bool DrawFocusedButtonPart(
	EditorContext& ctx, Entity button_entity, ButtonChildPart part, ButtonVisualState state
) {
	const std::string_view label{ ButtonPartLabel(part) };
	Entity child{ FindButtonPart(button_entity, part) };

	if (!child) {
		return false;
	}

	bool changed{ false };
	bool remove_requested{ false };
	const std::string tree_label{ std::string{ label } + "##FocusedButtonPart" };

	auto& close_on_next_draw{ ButtonPartCloseOnNextDraw() };
	if (close_on_next_draw.has_value() && close_on_next_draw.value() == child) {
		ImGui::SetNextItemOpen(false, ImGuiCond_Always);
		close_on_next_draw.reset();
	}

	const bool open{ ImGui::TreeNodeEx(
		tree_label.c_str(),
		ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
	) };

	if (ImGui::BeginPopupContextItem()) {
		if (ImGui::MenuItem("Remove")) {
			remove_requested = true;
		}
		ImGui::EndPopup();
	}

	if (open) {
		ScopedIndent indent;

		EntityInspectorTarget child_target{
			.ctx	= ctx,
			.entity = child,
		};
		const ButtonChildInfo info{
			.child	= child,
			.button = button_entity,
			.part	= part,
		};

		changed |= DrawButtonChildStateTransformFeature(child_target, info, state);
		changed |= DrawButtonChildStateVisualFeature(child_target, info, state);

		ImGui::TreePop();
	}

	if (remove_requested) {
		ctx.commands.DeleteEntity(child);
		changed = true;
	}

	return changed;
}

template <typename Target>
bool DrawFocusedButtonSounds(Target& target, ButtonVisualState state) {
	if constexpr (!Target::template Supports<ButtonSounds>()) {
		return false;
	} else {
		auto before{ target.template Capture<ButtonSounds>() };

		if (!before) {
			return false;
		}

		ButtonSounds sounds{ *before };
		const auto index{ static_cast<std::size_t>(std::to_underlying(state)) };
		auto& sound{ sounds.states[index] };

		if (!sound.has_value()) {
			return false;
		}

		if (ButtonAudioCloseOnNextDraw()) {
			ImGui::SetNextItemOpen(false, ImGuiCond_Always);
			ButtonAudioCloseOnNextDraw() = false;
		}

		bool changed{ false };
		bool remove_requested{ false };

		const bool open{ ImGui::TreeNodeEx(
			"Audio##FocusedButtonAudio",
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
		) };

		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Remove")) {
				remove_requested = true;
			}
			ImGui::EndPopup();
		}

		if (open) {
			ScopedIndent indent;
			AutoLabelWidthScope labels{ "FocusedButtonAudioFields" };

			changed |= DrawValue(target.ctx, "Sound Key", sound.value());
			changed |= DrawValue(target.ctx, "Exclusive", sounds.exclusive);
			DrawTooltip("Stop matching button audio before replaying it.");

			ImGui::TreePop();
		}

		if (remove_requested) {
			sound.reset();
			changed = true;
		}

		if (!changed) {
			return false;
		}

		target.template SetLive<ButtonSounds>(sounds);
		auto after{ target.template Capture<ButtonSounds>() };

		TrackComponentState(
			target,
			remove_requested ? "Remove Button Audio" : "Edit Button Audio",
			std::move(before), std::move(after), true
		);

		return true;
	}
}

template <typename Target>
bool DrawFocusedButtonInteraction(Target& target) {
	return DrawFocusedComponent<Target, ::ptgn::impl::ButtonData>(
		target, "Button Interaction", [&](::ptgn::impl::ButtonData& data) {
			AutoLabelWidthScope labels{ "FocusedButtonInteractionFields" };
			bool changed{ false };
			changed |= DrawValue(target.ctx, "Press Enabled", data.press_enabled);
			changed |= DrawValue(target.ctx, "Hover Enabled", data.hover_enabled);
			return changed;
		}
	);
}

void ApplyDropdownItemIndex(
	Editor& editor, const EntityReference& dropdown_reference,
	const EntityReference& item_reference, std::size_t index
) {
	Entity dropdown_entity{ dropdown_reference.Resolve(editor) };
	Entity item{ item_reference.Resolve(editor) };
	if (!dropdown_entity || !item || !HasParent(item) || GetParent(item) != dropdown_entity) {
		return;
	}
	MoveChild(dropdown_entity, item, index);
	Dropdown{ dropdown_entity }.RefreshLayout();
}

void MoveDropdownItem(EditorContext& ctx, Entity dropdown_entity, Entity item, Entity adjacent) {
	if (!dropdown_entity || !item || !adjacent || !HasChildren(dropdown_entity)) {
		return;
	}
	const auto& children{ GetChildren(dropdown_entity) };
	const auto item_it{ std::ranges::find(children, item) };
	const auto adjacent_it{ std::ranges::find(children, adjacent) };
	if (item_it == children.end() || adjacent_it == children.end()) {
		return;
	}

	const std::size_t from{ static_cast<std::size_t>(std::distance(children.begin(), item_it)) };
	const std::size_t to{ static_cast<std::size_t>(std::distance(children.begin(), adjacent_it)) };
	if (from == to) {
		return;
	}

	const EntityReference dropdown_reference{ MakeEntityReference(dropdown_entity) };
	const EntityReference item_reference{ MakeEntityReference(item) };
	MoveChild(dropdown_entity, item, to);
	Dropdown{ dropdown_entity }.RefreshLayout();

	Editor* editor{ std::addressof(ctx.editor) };
	ctx.undo.PushApplied(
		"Reorder Dropdown Item",
		[editor, dropdown_reference, item_reference, from]() {
			ApplyDropdownItemIndex(*editor, dropdown_reference, item_reference, from);
		},
		[editor, dropdown_reference, item_reference, to]() {
			ApplyDropdownItemIndex(*editor, dropdown_reference, item_reference, to);
		}
	);
}

bool DrawDropdownItems(EntityInspectorTarget& target) {
	Dropdown dropdown{ target.entity };
	dropdown.RefreshLayout();
	auto items{ dropdown.GetButtons() };
	bool changed{ false };

	bool section_open{ false };
	const float button_size{ ImGui::GetFrameHeight() };
	if (ImGui::BeginTable(
			"##DropdownItemsHeader", 2,
			ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX |
				ImGuiTableFlags_NoSavedSettings
		)) {
		ImGui::TableSetupColumn("Section", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Add", ImGuiTableColumnFlags_WidthFixed, button_size);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, button_size);
		ImGui::TableSetColumnIndex(0);
		section_open = ImGui::TreeNodeEx(
			"Dropdown Items##DropdownItemsSection",
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
				ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_DefaultOpen
		);

		ImGui::TableSetColumnIndex(1);
		if (ImGui::Button("+##AddDropdownItem", ImVec2{ button_size, button_size })) {
			const std::string label{ "Dropdown Item " + std::to_string(items.size() + 1) };
			Button created{ dropdown.AddItem(label) };
			created.Background().Color(color::DarkGray);
			created.Background(ButtonVisualState::Hover).Color(color::Gray);
			created.Background(ButtonVisualState::Press).Color(color::Black);
			(void)RecordCreatedEntityPreservingSelection(target.ctx, created);
			dropdown.RefreshLayout();
			items	= dropdown.GetButtons();
			changed = true;
		}
		DrawTooltip("Add a dropdown item.");
		ImGui::EndTable();
	}

	if (!section_open) {
		return changed;
	}

	Entity delete_item;
	bool reordered{ false };
	std::optional<float> restore_scroll_y{};

	for (std::size_t i{ 0 }; i < items.size(); ++i) {
		Button item{ items[i] };
		if (!item) {
			continue;
		}

		ImGui::PushID(item.Get<UUID>());
		const std::string item_label{ "Dropdown Item " + std::to_string(i + 1) };
		const bool open{ ImGui::TreeNodeEx(
			"##DropdownItem",
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
				ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_DefaultOpen,
			"%s", item_label.c_str()
		) };

		if (ImGui::BeginPopupContextItem()) {
			if (i > 0 && ImGui::MenuItem("Move Up")) {
				restore_scroll_y = ImGui::GetScrollY();
				MoveDropdownItem(target.ctx, dropdown, item, items[i - 1]);
				reordered = true;
				changed	  = true;
			}
			if (i + 1 < items.size() && ImGui::MenuItem("Move Down")) {
				restore_scroll_y = ImGui::GetScrollY();
				MoveDropdownItem(target.ctx, dropdown, item, items[i + 1]);
				reordered = true;
				changed	  = true;
			}
			if (i > 0 || i + 1 < items.size()) {
				ImGui::Separator();
			}
			if (ImGui::MenuItem("Remove")) {
				delete_item = item;
			}
			ImGui::EndPopup();
		}

		if (open && !reordered && !delete_item) {
			ScopedIndent item_indent;
			EntityInspectorTarget item_target{ .ctx = target.ctx, .entity = item };

			// Dropdown item transforms are ordinary local transforms. The dropdown layout system
			// preserves the user's delta while the normal ignore-parent components control whether
			// the transform is relative to the dropdown header.
			changed |= DrawTransformFeature(item_target, false);

			FocusedUIControlType item_type{ GetFocusedUIControlType(item_target) };
			if (item_type != FocusedUIControlType::None &&
				item_type != FocusedUIControlType::Conflict) {
				if (DrawFocusedUIControlTypeSelector(item_target, item_type)) {
					changed	  = true;
					item_type = GetFocusedUIControlType(item_target);
				}
			}

			if (item_type == FocusedUIControlType::Conflict) {
				changed |= DrawUIControlConflict(item_target);
			} else if (item_type != FocusedUIControlType::None) {
				changed |= DrawFocusedButtonInteraction(item_target);
				changed |= DrawFocusedButtonAppearance(item_target, item_type);
			}
		}

		ImGui::PopID();
		if (delete_item || reordered) {
			break;
		}
	}

	if (restore_scroll_y.has_value()) {
		ImGui::SetScrollY(restore_scroll_y.value());
	}
	if (delete_item) {
		target.ctx.commands.DeleteEntity(delete_item);
		dropdown.RefreshLayout();
		changed = true;
	}
	return changed;
}

bool DrawToggleGroupMembers(EntityInspectorTarget& target) {
	ToggleButtonGroup group{ target.entity };
	auto buttons{ group.GetButtons() };
	auto& editor_state{ GetManualFeatureState(target.GetFeatureTargetKey()) };
	if (!buttons.empty()) {
		editor_state.toggle_group_item_index =
			std::min(editor_state.toggle_group_item_index, buttons.size() - 1);
	} else {
		editor_state.toggle_group_item_index = 0;
	}
	bool changed{ false };

	if (!ImGui::TreeNodeEx(
			"Members##FocusedToggleGroupMembers",
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen
		)) {
		return false;
	}

	{
		ScopedIndent indent;
		Entity delete_item;
		for (std::size_t i{ 0 }; i < buttons.size(); ++i) {
			ToggleButton button{ buttons[i] };
			const auto& item{ button.Get<::ptgn::impl::ToggleButtonGroupItem>() };
			ImGui::PushID(button.Get<UUID>());
			ImGui::AlignTextToFramePadding();
			ImGui::Text("%s", item.key.value.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton(
					editor_state.toggle_group_item_index == i ? "Selected" : "Select"
				)) {
				editor_state.toggle_group_item_index = i;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Open")) {
				target.ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(button);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Remove")) {
				delete_item = button;
			}
			ImGui::PopID();
		}

		if (delete_item) {
			target.ctx.commands.DeleteEntity(delete_item);
			changed = true;
		}

		if (ImGui::Button("+ Add Toggle", ImVec2{ -FLT_MIN, 0.0f })) {
			std::string key{ "option_" + std::to_string(buttons.size() + 1) };
			ToggleButton created{
				CreateToggleButton(target.entity.GetScene(), {}, V2_float{ 180.0f, 50.0f })
			};
			created.Add<Tag>("Toggle " + std::to_string(buttons.size() + 1));
			created.Background().Color(color::DarkGray);
			created.Background(ButtonVisualState::Hover).Color(color::Gray);
			created.Text().Content("Toggle " + std::to_string(buttons.size() + 1));
			group.Add(key, created);
			(void)target.ctx.commands.RecordCreatedEntity(created, target.ctx.local.selection);
			editor_state.toggle_group_item_index = buttons.size();
			changed								 = true;
		}

		buttons = group.GetButtons();
		if (!buttons.empty()) {
			editor_state.toggle_group_item_index =
				std::min(editor_state.toggle_group_item_index, buttons.size() - 1);
			ToggleButton selected_button{ buttons[editor_state.toggle_group_item_index] };
			ImGui::SeparatorText("Selected Toggle Appearance");
			EntityInspectorTarget item_target{ .ctx = target.ctx, .entity = selected_button };
			changed |= DrawFocusedButtonAppearance(item_target, FocusedUIControlType::ToggleButton);
		}
	}

	ImGui::TreePop();
	return changed;
}

struct ButtonAppearanceSnapshot {
	std::optional<ButtonBackgroundVisuals> backgrounds{};
	std::optional<ButtonBorderVisuals> borders{};
	std::optional<ButtonTextVisuals> texts{};
	std::optional<ButtonSpriteVisuals> sprites{};
	std::optional<ButtonSounds> sounds{};
};

[[nodiscard]] ButtonAppearanceSnapshot CaptureButtonAppearanceSnapshot(Entity button) {
	ButtonAppearanceSnapshot snapshot;

	if (Entity child{ FindButtonPart(button, ButtonChildPart::Background) };
		child && child.Has<ButtonBackgroundVisuals>()) {
		snapshot.backgrounds = child.Get<ButtonBackgroundVisuals>();
	}
	if (Entity child{ FindButtonPart(button, ButtonChildPart::Border) };
		child && child.Has<ButtonBorderVisuals>()) {
		snapshot.borders = child.Get<ButtonBorderVisuals>();
	}
	if (Entity child{ FindButtonPart(button, ButtonChildPart::Text) };
		child && child.Has<ButtonTextVisuals>()) {
		snapshot.texts = child.Get<ButtonTextVisuals>();
	}
	if (Entity child{ FindButtonPart(button, ButtonChildPart::Sprite) };
		child && child.Has<ButtonSpriteVisuals>()) {
		snapshot.sprites = child.Get<ButtonSpriteVisuals>();
	}
	if (button && button.Has<ButtonSounds>()) {
		snapshot.sounds = button.Get<ButtonSounds>();
	}

	return snapshot;
}

void ApplyButtonAppearanceSnapshot(
	Editor& editor, const EntityReference& button_reference,
	const ButtonAppearanceSnapshot& snapshot, ButtonVisualState preview_state
) {
	Entity button{ button_reference.Resolve(editor) };
	if (!button || !button.Has<::ptgn::impl::ButtonData>()) {
		return;
	}

	if (snapshot.backgrounds) {
		if (Entity child{ FindButtonPart(button, ButtonChildPart::Background) }) {
			AssignEntityComponent<ButtonBackgroundVisuals>(child, snapshot.backgrounds);
			MarkButtonBackgroundDirty(child);
		}
	}
	if (snapshot.borders) {
		if (Entity child{ FindButtonPart(button, ButtonChildPart::Border) }) {
			AssignEntityComponent<ButtonBorderVisuals>(child, snapshot.borders);
			MarkButtonBorderDirty(child);
		}
	}
	if (snapshot.texts) {
		if (Entity child{ FindButtonPart(button, ButtonChildPart::Text) }) {
			AssignEntityComponent<ButtonTextVisuals>(child, snapshot.texts);
			MarkButtonTextDirty(child);
		}
	}
	if (snapshot.sprites) {
		if (Entity child{ FindButtonPart(button, ButtonChildPart::Sprite) }) {
			AssignEntityComponent<ButtonSpriteVisuals>(child, snapshot.sprites);
			MarkButtonSpriteDirty(child);
		}
	}
	if (snapshot.sounds) {
		AssignEntityComponent<ButtonSounds>(button, snapshot.sounds);
	}

	Button{ button }.PreviewVisualState(preview_state);
}

template <typename Visuals>
bool ApplyButtonSnapshotStateOperation(
	std::optional<Visuals>& visuals, ButtonVisualState destination,
	std::optional<ButtonVisualState> source
) {
	if (!visuals) {
		return false;
	}

	const auto destination_index{ static_cast<std::size_t>(std::to_underlying(destination)) };
	if (source) {
		visuals->states[destination_index] =
			visuals->states[static_cast<std::size_t>(std::to_underlying(*source))];
	} else {
		visuals->states[destination_index] = {};
	}
	return true;
}

bool ApplyButtonStateOperation(
	EditorContext& ctx, Entity button, ButtonVisualState destination,
	std::optional<ButtonVisualState> source
) {
	if (!button) {
		return false;
	}

	const std::string action{ source ? "Copy Button State" : "Reset Button State Overrides" };
	const EntityReference button_reference{ MakeEntityReference(button) };
	const ButtonAppearanceSnapshot before{ CaptureButtonAppearanceSnapshot(button) };
	ButtonAppearanceSnapshot after{ before };
	bool changed{ false };

	changed |= ApplyButtonSnapshotStateOperation(after.backgrounds, destination, source);
	changed |= ApplyButtonSnapshotStateOperation(after.borders, destination, source);
	changed |= ApplyButtonSnapshotStateOperation(after.texts, destination, source);
	changed |= ApplyButtonSnapshotStateOperation(after.sprites, destination, source);

	if (after.sounds) {
		const auto destination_index{ static_cast<std::size_t>(std::to_underlying(destination)) };
		if (source) {
			after.sounds->states[destination_index] =
				after.sounds->states[static_cast<std::size_t>(std::to_underlying(*source))];
		} else {
			after.sounds->states[destination_index].reset();
		}
		changed = true;
	}

	if (!changed) {
		return false;
	}

	Editor* editor{ std::addressof(ctx.editor) };
	ApplyButtonAppearanceSnapshot(*editor, button_reference, after, destination);
	ctx.undo.PushApplied(
		action,
		[editor, button_reference, before, destination]() {
			ApplyButtonAppearanceSnapshot(*editor, button_reference, before, destination);
		},
		[editor, button_reference, after, destination]() {
			ApplyButtonAppearanceSnapshot(*editor, button_reference, after, destination);
		}
	);

	return true;
}

bool DrawButtonStateActions(
	EditorContext& ctx, Entity button, ButtonVisualState state, bool allow_toggled
) {
	bool changed{ false };
	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float available{ ImGui::GetContentRegionAvail().x };
	const float half_width{ std::max(1.0f, (available - spacing) * 0.5f) };

	if (ImGui::Button("Reset State Overrides", ImVec2{ half_width, 0.0f })) {
		changed |= ApplyButtonStateOperation(ctx, button, state, std::nullopt);
	}
	DrawTooltip(
		"Remove all overrides for this appearance state so each part falls back to inherited "
		"values."
	);

	ImGui::SameLine();
	ImGui::SetNextItemWidth(half_width);
	if (ImGui::BeginCombo("##CopyButtonState", "Copy From...")) {
		for (const ButtonVisualState candidate : magic_enum::enum_values<ButtonVisualState>()) {
			int mode{ 0 };
			ButtonState pointer{ ButtonState::Idle };
			DecomposeButtonVisualState(candidate, mode, pointer);
			if ((!allow_toggled && mode == 1) || candidate == state) {
				continue;
			}

			const std::string label{ PrettyName(magic_enum::enum_name(candidate)) };
			if (ImGui::Selectable(label.c_str())) {
				changed |= ApplyButtonStateOperation(ctx, button, state, candidate);
			}
		}
		ImGui::EndCombo();
	}
	DrawTooltip(
		"Copy every configured part override from another appearance state into this state."
	);

	return changed;
}

template <typename Target>
bool DrawFocusedButtonAppearance(Target& target, FocusedUIControlType type) {
	auto& editor_state{ GetManualFeatureState(target.GetFeatureTargetKey()) };
	const bool allow_toggled{ type == FocusedUIControlType::ToggleButton };

	DrawFocusedButtonStateSelector(
		target.ctx, target.GetFeatureTargetKey(), editor_state.button_visual_state, allow_toggled
	);

	const ButtonVisualState state{
		editor_state.button_visual_state.value_or(ButtonVisualState::Idle)
	};

	bool changed{ false };

	if constexpr (requires { target.entity; }) {
		ApplyButtonPreview(target.entity, state, target.ctx);

		changed |= DrawFocusedButtonAddParts(target.ctx, target.entity, state);

		changed |=
			DrawFocusedButtonPart(target.ctx, target.entity, ButtonChildPart::Background, state);
		changed |= DrawFocusedButtonPart(target.ctx, target.entity, ButtonChildPart::Border, state);
		changed |= DrawFocusedButtonPart(target.ctx, target.entity, ButtonChildPart::Text, state);
		changed |= DrawFocusedButtonPart(target.ctx, target.entity, ButtonChildPart::Sprite, state);

		changed |= DrawFocusedButtonSounds(target, state);

		ImGui::Spacing();
		changed |= DrawButtonStateActions(target.ctx, target.entity, state, allow_toggled);
	} else {
		DrawDisabledWrappedText(
			"Managed child appearance editing for prefab assets is available by enabling "
			"View > Show Managed UI Parts and selecting the part."
		);
		changed |= DrawFocusedButtonSounds(target, state);
	}

	return changed;
}

template <typename Target>
bool RepairUIControlConflict(Target& target, FocusedUIControlType keep) {
	auto before{
		CaptureInspectorFeatureState(target, InspectorFeature::UI, UIFeatureComponents{})
	};

	if (keep != FocusedUIControlType::Slider) {
		RemoveSupportedFeatureComponent<Target, ::ptgn::impl::SliderData>(target);
	}
	if (keep != FocusedUIControlType::ToggleButton) {
		RemoveSupportedFeatureComponent<Target, ::ptgn::impl::ToggleButtonData>(target);
	}
	if (keep != FocusedUIControlType::Dropdown) {
		RemoveSupportedFeatureComponent<Target, ::ptgn::impl::DropdownData>(target);
	}
	if constexpr (Target::template Supports<::ptgn::impl::ButtonData>()) {
		if (!target.template Capture<::ptgn::impl::ButtonData>()) {
			target.template SetLive<::ptgn::impl::ButtonData>(::ptgn::impl::ButtonData{});
		}
	}

	auto after{ CaptureInspectorFeatureState(target, InspectorFeature::UI, UIFeatureComponents{}) };
	TrackInspectorFeatureState(
		target, InspectorFeature::UI, "Repair UI Control Type", std::move(before), std::move(after),
		UIFeatureComponents{}
	);
	return true;
}

template <typename Target>
bool ChangeFocusedUIControlType(Target& target, FocusedUIControlType type) {
	if (type == FocusedUIControlType::None || type == FocusedUIControlType::Conflict) {
		return false;
	}

	auto before{
		CaptureInspectorFeatureState(target, InspectorFeature::UI, UIFeatureComponents{})
	};

	RemoveSupportedFeatureComponent<Target, ::ptgn::impl::SliderData>(target);
	RemoveSupportedFeatureComponent<Target, ::ptgn::impl::ToggleButtonData>(target);
	RemoveSupportedFeatureComponent<Target, ::ptgn::impl::DropdownData>(target);

	if constexpr (Target::template Supports<::ptgn::impl::ButtonData>()) {
		if (!target.template Capture<::ptgn::impl::ButtonData>()) {
			target.template SetLive<::ptgn::impl::ButtonData>(::ptgn::impl::ButtonData{});
		}
	}

	switch (type) {
		case FocusedUIControlType::Button: break;

		case FocusedUIControlType::ToggleButton:
			if constexpr (Target::template Supports<::ptgn::impl::ToggleButtonData>()) {
				target.template SetLive<::ptgn::impl::ToggleButtonData>(
					::ptgn::impl::ToggleButtonData{}
				);
			}
			break;

		case FocusedUIControlType::Slider:
			if constexpr (Target::template Supports<::ptgn::impl::SliderData>()) {
				::ptgn::impl::SliderData data;
				// SliderData::line is stored in slider-local space unless an enabled Track Transform
				// supplies the basis. Converting an existing button must not bake in world position.
				data.line = Line{ {}, V2_float{ 100.0f, 0.0f } };
				target.template SetLive<::ptgn::impl::SliderData>(data);
			}
			break;

		case FocusedUIControlType::Dropdown:
			if constexpr (Target::template Supports<::ptgn::impl::DropdownData>()) {
				target.template SetLive<::ptgn::impl::DropdownData>(::ptgn::impl::DropdownData{});
			}
			break;

		case FocusedUIControlType::None:
		case FocusedUIControlType::Conflict: break;
	}

	auto after{ CaptureInspectorFeatureState(target, InspectorFeature::UI, UIFeatureComponents{}) };
	TrackInspectorFeatureState(
		target, InspectorFeature::UI, "Change UI Control Type", std::move(before), std::move(after),
		UIFeatureComponents{}
	);
	return true;
}

template <typename Target>
bool DrawFocusedUIControlTypeSelector(Target& target, FocusedUIControlType current) {
	if (current == FocusedUIControlType::None || current == FocusedUIControlType::Conflict) {
		return false;
	}

	const bool locked_to_toggle{
		HasTargetComponent<Target, ::ptgn::impl::ToggleButtonGroupItem>(target)
	};

	auto before_visible{ target.template Capture<Visible>() };
	bool visible{ before_visible ? before_visible->visible : true };

	bool type_changed{ false };
	bool visible_changed{ false };

	DrawPropertyRow("Control Type", [&]() {
		const float spacing{ ImGui::GetStyle().ItemSpacing.x };
		const float visible_width{
			ImGui::GetFrameHeight() +
			ImGui::GetStyle().ItemInnerSpacing.x +
			ImGui::CalcTextSize("Visible").x
		};
		const float combo_width{
			std::max(
				80.0f,
				ImGui::GetContentRegionAvail().x - visible_width - spacing
			)
		};

		const std::string preview{ FocusedUIControlLabel(current) };

		ImGui::SetNextItemWidth(combo_width);
		if (ImGui::BeginCombo("##FocusedUIControlType", preview.c_str())) {
			for (const FocusedUIControlType candidate :
				 { FocusedUIControlType::Button, FocusedUIControlType::ToggleButton,
				   FocusedUIControlType::Slider, FocusedUIControlType::Dropdown }) {
				const bool disabled{
					locked_to_toggle &&
					candidate != FocusedUIControlType::ToggleButton
				};
				ScopedDisabled disabled_scope{ disabled };
				const bool selected{ candidate == current };

				if (ImGui::Selectable(
						std::string{ FocusedUIControlLabel(candidate) }.c_str(),
						selected
					) &&
					!disabled && !selected) {
					type_changed = ChangeFocusedUIControlType(target, candidate);
				}

				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}

		ImGui::SameLine(0.0f, spacing);
		visible_changed = ImGui::Checkbox("Visible##UIControlVisible", &visible);

		return type_changed || visible_changed;
	});

	if (visible_changed) {
		Visible updated{ before_visible.value_or(Visible{}) };
		updated.visible = visible;
		target.template SetLive<Visible>(ComponentState<Visible>{ updated });
	}

	auto after_visible{ target.template Capture<Visible>() };

	TrackComponentState(
		target, "Toggle UI Control Visibility",
		std::move(before_visible), std::move(after_visible), visible_changed
	);

	if (locked_to_toggle) {
		DrawDisabledWrappedText(
			"Toggle-group members must remain Toggle Buttons. Remove the group membership first to "
			"change type."
		);
	}

	return type_changed || visible_changed;
}

template <typename Target>
bool DrawUIControlConflict(Target& target) {
	ImGui::TextColored(
		ImVec4{ 1.0f, 0.45f, 0.2f, 1.0f },
		"This entity has multiple mutually-exclusive UI control types."
	);
	DrawDisabledWrappedText(
		"Keep exactly one specialized control type. ButtonData is the shared selectable base and "
		"is not a conflict."
	);

	bool changed{ false };
	if (HasTargetComponent<Target, ::ptgn::impl::SliderData>(target) &&
		ImGui::Button("Keep Slider")) {
		changed |= RepairUIControlConflict(target, FocusedUIControlType::Slider);
	}
	if (HasTargetComponent<Target, ::ptgn::impl::ToggleButtonData>(target)) {
		if (changed) {
			ImGui::SameLine();
		}
		if (ImGui::Button("Keep Toggle Button")) {
			changed |= RepairUIControlConflict(target, FocusedUIControlType::ToggleButton);
		}
	}
	if (HasTargetComponent<Target, ::ptgn::impl::DropdownData>(target)) {
		if (changed) {
			ImGui::SameLine();
		}
		if (ImGui::Button("Keep Dropdown")) {
			changed |= RepairUIControlConflict(target, FocusedUIControlType::Dropdown);
		}
	}
	return changed;
}

template <typename Marker>
[[nodiscard]] Entity FindSliderTrackPart(Entity track) {
	if (!track || !HasChildren(track)) {
		return {};
	}
	for (Entity child : GetChildren(track)) {
		if (child.Has<Marker>()) {
			return child;
		}
	}
	return {};
}

bool DrawSliderTrackTransform(EntityInspectorTarget& slider_target, Entity track) {
	if (!track || !track.Has<::ptgn::impl::SliderTrackData>()) {
		return false;
	}

	auto before{ track.Get<::ptgn::impl::SliderTrackData>() };
	bool enabled{ before.transform_enabled };
	bool changed{ false };
	ScopedID scope{ "SliderTrackTransform" };

	if (ImGui::Checkbox("##Enabled", &enabled)) {
		auto after{ before };
		after.transform_enabled					   = enabled;
		track.Get<::ptgn::impl::SliderTrackData>() = after;
		slider_target.ctx.undo.PushApplied(
			"Toggle Track Transform",
			[track, before]() mutable {
				if (track) {
					track.Get<::ptgn::impl::SliderTrackData>() = before;
					::ptgn::impl::SliderSystem::SynchronizeEntity(track);
				}
			},
			[track, after]() mutable {
				if (track) {
					track.Get<::ptgn::impl::SliderTrackData>() = after;
					::ptgn::impl::SliderSystem::SynchronizeEntity(track);
				}
			}
		);
		::ptgn::impl::SliderSystem::SynchronizeEntity(track);
		changed = true;
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!enabled);
	const bool open{ ImGui::TreeNodeEx(
		"Track Transform##SliderTrackTransformTree",
		ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
	) };
	ImGui::EndDisabled();

	if (open) {
		if (enabled) {
			EntityInspectorTarget track_target{ .ctx = slider_target.ctx, .entity = track };
			changed |= DrawTransformFeature(track_target, false, true, false);
		}
		ImGui::TreePop();
	}

	return changed;
}

[[nodiscard]] bool HasSliderTrackVisualParts(Entity track) {
	return FindSliderTrackPart<::ptgn::impl::SliderTrackBackgroundData>(track) ||
		   FindSliderTrackPart<::ptgn::impl::SliderTrackBorderData>(track) ||
		   FindSliderTrackPart<::ptgn::impl::SliderTrackSpriteData>(track);
}

void SynchronizeSliderTrackVisualEnabled(Entity track) {
	if (!track || !track.Has<::ptgn::impl::SliderTrackData>()) {
		return;
	}

	track.Get<::ptgn::impl::SliderTrackData>().visual_enabled = HasSliderTrackVisualParts(track);
	::ptgn::impl::SliderSystem::SynchronizeEntity(track);
}

template <typename Marker, typename Create, typename Draw>
bool DrawSliderTrackVisualPart(
	EntityInspectorTarget& slider_target, Entity track, std::string_view label, Create&& create,
	Draw&& draw
) {
	Entity child{ FindSliderTrackPart<Marker>(track) };
	bool enabled{ static_cast<bool>(child) };
	bool changed{ false };
	ScopedID scope{ label };

	if (ImGui::Checkbox("##Enabled", &enabled)) {
		if (enabled) {
			child = std::invoke(std::forward<Create>(create));
			if (child) {
				(void)RecordCreatedEntityPreservingSelection(slider_target.ctx, child);
			}
		} else if (child) {
			slider_target.ctx.commands.DeleteEntity(child);
			child = {};
		}
		SynchronizeSliderTrackVisualEnabled(track);
		changed = true;
	}

	ImGui::SameLine();
	const std::string tree_label{ std::string{ label } + "##SliderTrackVisualPart" };
	ImGui::BeginDisabled(!enabled);
	const bool open{ ImGui::TreeNodeEx(
		tree_label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
	) };
	ImGui::EndDisabled();

	if (open) {
		if (enabled && child) {
			EntityInspectorTarget child_target{ .ctx = slider_target.ctx, .entity = child };
			changed |= std::invoke(std::forward<Draw>(draw), child_target, child);
		}
		ImGui::TreePop();
	}

	return changed;
}

bool DrawSliderTrackBackgroundFields(EntityInspectorTarget& target) {
	bool changed{ false };

	if (auto before{ target.Capture<Color>() }) {
		Color value{ *before };
		const bool local_changed{ DrawValue(target.ctx, "Color", value) };
		if (local_changed) {
			target.SetLive<Color>(value);
		}
		auto after{ target.Capture<Color>() };
		TrackComponentState(
			target, "Edit Track Background Color", std::move(before), std::move(after), local_changed
		);
		changed |= local_changed;
	}

	if (auto before{ target.Capture<Origin>() }) {
		Origin value{ *before };
		const bool local_changed{ DrawValue(target.ctx, "Origin", value) };
		if (local_changed) {
			target.SetLive<Origin>(value);
		}
		auto after{ target.Capture<Origin>() };
		TrackComponentState(
			target, "Edit Track Background Origin", std::move(before), std::move(after), local_changed
		);
		changed |= local_changed;
	}

	return changed;
}

bool DrawSliderTrackSpriteFields(EntityInspectorTarget& target) {
	bool changed{ DrawSpritePrimary(target) };

	if (auto before{ target.Capture<Origin>() }) {
		Origin value{ *before };
		const bool local_changed{ DrawValue(target.ctx, "Origin", value) };
		if (local_changed) {
			target.SetLive<Origin>(value);
		}
		auto after{ target.Capture<Origin>() };
		TrackComponentState(
			target, "Edit Track Sprite Origin", std::move(before), std::move(after), local_changed
		);
		changed |= local_changed;
	}

	if (auto before{ target.Capture<Tint>() }) {
		Tint value{ *before };
		const bool local_changed{ DrawValue(target.ctx, "Tint", value) };
		if (local_changed) {
			target.SetLive<Tint>(value);
		}
		auto after{ target.Capture<Tint>() };
		TrackComponentState(
			target, "Edit Track Sprite Tint", std::move(before), std::move(after), local_changed
		);
		changed |= local_changed;
	}

	return changed;
}

bool DrawSliderTrackBorderFields(EntityInspectorTarget& target, Entity border) {
	bool changed{ false };

	if (auto before{ target.Capture<Color>() }) {
		Color value{ *before };
		const bool local_changed{ DrawValue(target.ctx, "Color", value) };
		if (local_changed) {
			target.SetLive<Color>(value);
		}
		auto after{ target.Capture<Color>() };
		TrackComponentState(
			target, "Edit Track Border Color", std::move(before), std::move(after), local_changed
		);
		changed |= local_changed;
	}

	if (auto before{ target.Capture<Origin>() }) {
		Origin value{ *before };
		const bool local_changed{ DrawValue(target.ctx, "Origin", value) };
		if (local_changed) {
			target.SetLive<Origin>(value);
		}
		auto after{ target.Capture<Origin>() };
		TrackComponentState(
			target, "Edit Track Border Origin", std::move(before), std::move(after), local_changed
		);
		changed |= local_changed;
	}

	auto fill_before{ target.Capture<FillStyle>() };
	if (fill_before) {
		FillStyle fill{ *fill_before };
		float width{ fill.GetLineWidth().value_or(kInspectorMinLineWidth) };
		float maximum_width{ kInspectorMinLineWidth };
		if (border.Has<Rect>()) {
			const auto size{ border.Get<Rect>().GetSize() };
			maximum_width = std::max(
				kInspectorMinLineWidth, std::min(std::abs(size.x), std::abs(size.y)) * 0.5f
			);
		} else if (border.Has<Circle>()) {
			maximum_width = std::max(kInspectorMinLineWidth, std::abs(border.Get<Circle>().radius));
		}
		width = std::clamp(width, kInspectorMinLineWidth, maximum_width);

		const bool local_changed{ DrawValue(
			target.ctx, "Line Width", width,
			FieldOptions{ .speed  = 0.05f,
						  .min	  = kInspectorMinLineWidth,
						  .max	  = maximum_width,
						  .format = "%.2f",
						  .flags  = ImGuiSliderFlags_AlwaysClamp }
		) };
		if (local_changed) {
			fill = FillStyle{ width };
			target.SetLive<FillStyle>(fill);
		}
		auto fill_after{ target.Capture<FillStyle>() };
		TrackComponentState(
			target, "Edit Track Border Width", std::move(fill_before), std::move(fill_after),
			local_changed
		);
		changed |= local_changed;
	}

	return changed;
}

bool DrawSliderTrackVisual(EntityInspectorTarget& target, Slider slider, Entity track) {
	if (!track || !track.Has<::ptgn::impl::SliderTrackData>()) {
		return false;
	}

	bool changed{ false };
	changed |= DrawSliderTrackVisualPart<::ptgn::impl::SliderTrackBackgroundData>(
		target, track, "Background",
		[&]() {
			if (Entity sprite{ FindSliderTrackPart<::ptgn::impl::SliderTrackSpriteData>(track) }) {
				target.ctx.commands.DeleteEntity(sprite);
			}
			(void)slider.TrackShape();
			return FindSliderTrackPart<::ptgn::impl::SliderTrackBackgroundData>(track);
		},
		[](EntityInspectorTarget& child_target, Entity) {
			return DrawSliderTrackBackgroundFields(child_target);
		}
	);

	changed |= DrawSliderTrackVisualPart<::ptgn::impl::SliderTrackSpriteData>(
		target, track, "Sprite",
		[&]() {
			if (Entity background{
					FindSliderTrackPart<::ptgn::impl::SliderTrackBackgroundData>(track) }) {
				target.ctx.commands.DeleteEntity(background);
			}
			(void)slider.TrackSprite({}, V2_float{ 100.0f, 16.0f });
			return FindSliderTrackPart<::ptgn::impl::SliderTrackSpriteData>(track);
		},
		[](EntityInspectorTarget& child_target, Entity) {
			return DrawSliderTrackSpriteFields(child_target);
		}
	);

	changed |= DrawSliderTrackVisualPart<::ptgn::impl::SliderTrackBorderData>(
		target, track, "Border",
		[&]() {
			Entity border{
				CreateRect(target.entity.GetScene(), {}, V2_float{ 100.0f, 16.0f }, color::White)
			};
			border.Add<::ptgn::impl::SliderTrackBorderData>();
			border.Add<FillStyle>(FillStyle{ kInspectorMinLineWidth });
			SetParent(border, track);
			SetUI(border, IsUI(target.entity));
			return border;
		},
		[](EntityInspectorTarget& child_target, Entity border) {
			return DrawSliderTrackBorderFields(child_target, border);
		}
	);

	SynchronizeSliderTrackVisualEnabled(track);
	return changed;
}

template <typename Target>
bool DrawFocusedControlSpecific(Target& target, FocusedUIControlType type) {
	bool changed{ false };

	switch (type) {
		case FocusedUIControlType::Button: break;

		case FocusedUIControlType::ToggleButton:
			changed |= DrawFocusedComponent<Target, ::ptgn::impl::ToggleButtonData>(
				target, "Toggle Button", [&](::ptgn::impl::ToggleButtonData& data) {
					AutoLabelWidthScope labels{ "FocusedToggleFields" };
					return DrawValue(target.ctx, "Toggled", data.toggled);
				}
			);
			break;

		case FocusedUIControlType::Slider: {
			if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
				Slider slider{ target.entity };
				Button thumb{ slider.GetThumb() };
				if (!thumb) {
					// SliderSystem owns creation/migration of the mandatory thumb. Ensure it exists so
					// the editor always exposes the same controls as an ordinary button.
					::ptgn::impl::SliderSystem::Prepare(target.entity.GetScene());
					thumb = slider.GetThumb();
				}
				if (thumb) {
					EntityInspectorTarget thumb_target{ .ctx = target.ctx, .entity = thumb };
					changed |= DrawFocusedButtonInteraction(thumb_target);
				}
			}

			changed |= DrawFocusedComponent<Target, ::ptgn::impl::SliderData>(
				target, "Slider", [&](::ptgn::impl::SliderData& data) {
					AutoLabelWidthScope labels{ "FocusedSliderFields" };
					return DrawSliderData(target, data);
				}
			);

			if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
				Slider slider{ target.entity };

				// Value Text is an optional managed slider part, using the same checkbox/tree row
				// pattern as optional button appearance parts.
				{
					ScopedID value_text_scope{ "SliderValueTextPart" };
					auto before{ target.template Capture<::ptgn::impl::SliderData>() };
					bool enabled{ before && before->value_text.has_value() };
					if (ImGui::Checkbox("##Enabled", &enabled) && before) {
						auto data{ *before };
						data.value_text = enabled
							? std::optional<SliderValueTextConfig>{ SliderValueTextConfig{} }
							: std::nullopt;
						target.template SetLive<::ptgn::impl::SliderData>(data);
						auto after{ target.template Capture<::ptgn::impl::SliderData>() };
						TrackComponentState(
							target, enabled ? "Enable Slider Value Text" : "Disable Slider Value Text",
							std::move(before), std::move(after), true
						);
						changed = true;
					}
					DrawTooltip(enabled ? "Disable slider value text." : "Enable slider value text.");
					ImGui::SameLine();
					ImGui::BeginDisabled(!enabled);
					const bool open{ ImGui::TreeNodeEx(
						"Value Text##SliderValueTextPartTree",
						ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
					) };
					ImGui::EndDisabled();
					if (open) {
						if (enabled) {
							auto data_before{ target.template Capture<::ptgn::impl::SliderData>() };
							if (data_before && data_before->value_text.has_value()) {
								auto data{ *data_before };
								if (DrawSliderValueTextConfig(target, data)) {
									target.template SetLive<::ptgn::impl::SliderData>(data);
									auto data_after{
										target.template Capture<::ptgn::impl::SliderData>()
									};
									TrackComponentState(
										target, "Edit Slider Value Text", std::move(data_before),
										std::move(data_after), true
									);
									changed = true;
								}
							}
						}
						ImGui::TreePop();
					}
				}

				// Track itself is optional. Its transform and visual parts live inside this tree.
				{
					ScopedID track_scope{ "SliderTrackPart" };
					Entity track{ slider.GetTrack() };
					bool enabled{ static_cast<bool>(track) };
					if (ImGui::Checkbox("##Enabled", &enabled)) {
						if (enabled) {
							track = slider.EnsureTrack();
							if (track) {
								(void)RecordCreatedEntityPreservingSelection(target.ctx, track);
							}
						} else if (track) {
							target.ctx.commands.DeleteEntity(track);
							track = {};
						}
						::ptgn::impl::SliderSystem::SynchronizeEntity(target.entity);
						changed = true;
					}
					DrawTooltip(enabled ? "Remove the slider track." : "Add a slider track.");
					ImGui::SameLine();
					ImGui::BeginDisabled(!enabled);
					const bool open{ ImGui::TreeNodeEx(
						"Track##SliderTrackPartTree",
						ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
					) };
					ImGui::EndDisabled();
					if (open) {
						if (enabled && track) {
							changed |= DrawSliderTrackTransform(target, track);
							changed |= DrawSliderTrackVisual(target, slider, track);
						}
						ImGui::TreePop();
					}
				}

				ImGui::SeparatorText("Thumb");
				if (Button thumb{ slider.GetThumb() }) {
					EntityInspectorTarget thumb_target{ .ctx = target.ctx, .entity = thumb };
					changed |= DrawFocusedButtonAppearance(thumb_target, FocusedUIControlType::Button);
				}
			}
			break;
		}

		case FocusedUIControlType::Dropdown:
			changed |= DrawFocusedComponent<Target, ::ptgn::impl::DropdownData>(
				target, "Dropdown",
				[&](::ptgn::impl::DropdownData& data) {
					AutoLabelWidthScope labels{ "FocusedDropdownFields" };
					bool local_changed{ false };

					local_changed |= DrawValue(target.ctx, "Starts Open", data.start_open);

					{
						ScopedID item_size_scope{ "DropdownItemSize" };
						bool enabled{ data.button_size.has_value() };
						V2_float displayed{ data.button_size.value_or(V2_float{}) };
						bool fields_changed{ false };
						const bool row_changed{ DrawOptionalPropertyRow(
							"Item Size", enabled, false, [&]() {
								if (!enabled) {
									ImGui::BeginDisabled();
									DrawUnsetOptionalInlineValue();
									ImGui::EndDisabled();
									return false;
								}

								const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
								const float width{
									std::max(1.0f, (ImGui::GetContentRegionAvail().x - spacing) * 0.5f)
								};
								ImGui::SetNextItemWidth(width);
								fields_changed |= ImGui::DragFloat(
									"##W", &displayed.x, kInspectorSizeDragSpeed, 0.0f, FLT_MAX,
									"W: %.3f", ImGuiSliderFlags_AlwaysClamp
								);
								ImGui::SameLine(0.0f, spacing);
								ImGui::SetNextItemWidth(width);
								fields_changed |= ImGui::DragFloat(
									"##H", &displayed.y, kInspectorSizeDragSpeed, 0.0f, FLT_MAX,
									"H: %.3f", ImGuiSliderFlags_AlwaysClamp
								);
								displayed.x = std::max(0.0f, displayed.x);
								displayed.y = std::max(0.0f, displayed.y);
								return fields_changed;
							}
						) };
						if (row_changed) {
							displayed.x = std::max(0.0f, displayed.x);
							displayed.y = std::max(0.0f, displayed.y);
							data.button_size =
								enabled ? std::optional<V2_float>{ displayed } : std::nullopt;
							local_changed = true;
						}
					}
					DrawTooltip("Override dropdown item size.");

					local_changed |= DrawValue(target.ctx, "Item Offset", data.button_offset);
					DrawTooltip("Offset dropdown item positions.");

					local_changed |= DrawValue(target.ctx, "Open Direction", data.direction);
					DrawTooltip("Direction the item list expands.");

					local_changed |= DrawValue(target.ctx, "Origin", data.origin);
					DrawTooltip("Anchor point used to place the item list.");

					return local_changed;
				},
				+[](Entity entity) {
					if (entity && entity.Has<::ptgn::impl::DropdownData>()) {
						Dropdown{ entity }.RefreshLayout();
					}
				}
			);

			if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
				changed |= DrawDropdownItems(target);
			}
			break;

		case FocusedUIControlType::Conflict: changed |= DrawUIControlConflict(target); break;

		case FocusedUIControlType::None:	 break;
	}

	return changed;
}

bool DrawManagedVisualChild(EditorContext& ctx, Entity child, std::string_view label) {
	if (!child) {
		ImGui::TextDisabled(
			"%.*s part is not present.", static_cast<int>(label.size()), label.data()
		);
		return false;
	}

	const std::string tree_label{ std::string{ label } + "##ManagedVisualChild" };
	if (!ImGui::TreeNodeEx(
			tree_label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen
		)) {
		return false;
	}

	EntityInspectorTarget child_target{ .ctx = ctx, .entity = child };
	bool changed{ false };
	{
		ScopedIndent indent;
		changed |= DrawVisualFeature(child_target);
		changed |= DrawTransformFeature(child_target);
	}
	ImGui::TreePop();
	return changed;
}

[[nodiscard]] Entity FindTooltipManagedPart(Entity tooltip, bool text) {
	if (!tooltip || !HasChildren(tooltip)) {
		return {};
	}
	for (Entity child : GetChildren(tooltip)) {
		if (text ? child.Has<::ptgn::impl::TooltipTextPart>()
				 : child.Has<::ptgn::impl::TooltipBackgroundPart>()) {
			return child;
		}
	}
	return {};
}

[[nodiscard]] Entity FindDialogueManagedPart(Entity dialogue, DialoguePartRole role) {
	if (!dialogue || !HasChildren(dialogue)) {
		return {};
	}
	for (Entity child : GetChildren(dialogue)) {
		if (auto part{ child.TryGet<::ptgn::impl::DialoguePart>() }; part && part->role == role) {
			return child;
		}
	}
	return {};
}

template <typename Target>
[[nodiscard]] bool HasDialogueControl(const Target& target) {
	if constexpr (requires { target.entity; }) {
		return target.entity && target.entity.template Has<DialogueData>();
	} else {
		return false;
	}
}

template <typename Target>
bool DrawUIFeature(Target& target) {
	if (!HasUIFeature(target)) {
		return false;
	}

	if (const auto child_info{ GetButtonChildInfo(target) }) {
		const bool open{
			ImGui::CollapsingHeader("Managed UI Part##ButtonChildUI", ImGuiTreeNodeFlags_None)
		};

		if (!open) {
			return false;
		}

		ScopedIndent feature_indent;
		auto& editor_state{ GetManualFeatureState(target.GetFeatureTargetKey()) };
		DrawButtonVisualStateSelector(editor_state.button_visual_state);
		return false;
	}

	const FocusedUIControlType control_type{ GetFocusedUIControlType(target) };
	const bool has_toggle_group{
		HasTargetComponent<Target, ::ptgn::impl::ToggleButtonGroupData>(target)
	};
	const bool has_tooltip{ HasTargetComponent<Target, ::ptgn::impl::TooltipData>(target) };
	const bool has_tooltip_hover{
		HasTargetComponent<Target, ::ptgn::impl::TooltipHoverData>(target)
	};
	const bool has_dialogue{ HasDialogueControl(target) };

	const auto header{ DrawFeatureHeader(
		target, InspectorFeature::UI, "UI", ImGuiTreeNodeFlags_DefaultOpen, UIFeatureComponents{},
		false
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;
	bool changed{ header.changed };

	if (control_type != FocusedUIControlType::None) {
		if (control_type != FocusedUIControlType::Conflict &&
			DrawFocusedUIControlTypeSelector(target, control_type)) {
			return true;
		}
		if (control_type != FocusedUIControlType::Conflict) {
			const bool has_button_base{
				HasTargetComponent<Target, ::ptgn::impl::ButtonData>(target)
			};

			if (!has_button_base) {
				ImGui::TextColored(
					ImVec4{ 1.0f, 0.45f, 0.2f, 1.0f },
					"This specialized control is missing its ButtonData base."
				);
				if constexpr (Target::template Supports<::ptgn::impl::ButtonData>()) {
					if (ImGui::Button("Repair Button Base", ImVec2{ -FLT_MIN, 0.0f })) {
						auto before{ target.template Capture<::ptgn::impl::ButtonData>() };
						target.template SetLive<::ptgn::impl::ButtonData>(
							::ptgn::impl::ButtonData{}
						);
						auto after{ target.template Capture<::ptgn::impl::ButtonData>() };
						TrackComponentState(
							target, "Repair Button Base", std::move(before), std::move(after), true
						);
						changed = true;
					}
				}
			} else {
				if (control_type == FocusedUIControlType::Slider) {
					changed |= DrawFocusedControlSpecific(target, control_type);
				} else {
					changed |= DrawFocusedButtonInteraction(target);

					if (control_type == FocusedUIControlType::Dropdown) {
						changed |= DrawFocusedControlSpecific(target, control_type);
						const bool appearance_open{ ImGui::TreeNodeEx(
							"Header Appearance##DropdownHeaderAppearance",
							ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
								ImGuiTreeNodeFlags_NoTreePushOnOpen
						) };
						if (appearance_open) {
							changed |= DrawFocusedButtonAppearance(target, control_type);
						}
					} else {
						changed |= DrawFocusedControlSpecific(target, control_type);
						ImGui::SeparatorText(
							control_type == FocusedUIControlType::Slider ? "Thumb Appearance"
																		 : "Appearance"
						);
						changed |= DrawFocusedButtonAppearance(target, control_type);
					}
				}
			}
		} else {
			changed |= DrawUIControlConflict(target);
		}
	}

	if (has_toggle_group) {
		ImGui::SeparatorText("Toggle Group");
		changed |= DrawFocusedComponent<Target, ::ptgn::impl::ToggleButtonGroupData>(
			target, "Toggle Group", [&](::ptgn::impl::ToggleButtonGroupData& data) {
				AutoLabelWidthScope labels{ "FocusedToggleGroupFields" };
				bool local_changed{ false };
				local_changed |= DrawValue(target.ctx, "Always One Active", data.always_active);
				local_changed |= DrawValue(target.ctx, "Active Key", data.active);
				return local_changed;
			}
		);
		if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
			changed |= DrawToggleGroupMembers(target);
		}
	}

	if (HasTargetComponent<Target, ::ptgn::impl::ToggleButtonGroupItem>(target)) {
		ImGui::SeparatorText("Toggle Group Membership");
		changed |= DrawFocusedComponent<Target, ::ptgn::impl::ToggleButtonGroupItem>(
			target, "Toggle Group Item", [&](::ptgn::impl::ToggleButtonGroupItem& item) {
				return DrawValue(target.ctx, "Key", item.key);
			}
		);
	}

	if (has_tooltip || has_tooltip_hover) {
		ImGui::SeparatorText("Tooltip");
		if (has_tooltip) {
			changed |= DrawFocusedComponent<Target, ::ptgn::impl::TooltipData>(
				target, "Tooltip", [&](::ptgn::impl::TooltipData& data) {
					AutoLabelWidthScope labels{ "FocusedTooltipFields" };
					bool local_changed{ false };
					local_changed |=
						DrawValue(target.ctx, "Fade In Duration", data.fade_in_duration);
					local_changed |=
						DrawValue(target.ctx, "Fade Out Duration", data.fade_out_duration);
					local_changed |= DrawValue(target.ctx, "Fade In Ease", data.fade_in_ease);
					local_changed |= DrawValue(target.ctx, "Fade Out Ease", data.fade_out_ease);
					return local_changed;
				}
			);
			if constexpr (requires { target.entity; }) {
				changed |= DrawManagedVisualChild(
					target.ctx, FindTooltipManagedPart(target.entity, true), "Text"
				);
				changed |= DrawManagedVisualChild(
					target.ctx, FindTooltipManagedPart(target.entity, false), "Background"
				);
			}
		}
		if (has_tooltip_hover) {
			changed |= DrawOptionalReflected<Target, ::ptgn::impl::TooltipHoverData>(
				target, "Show Tooltip On Hover", true
			);
		}
	}

	if (has_dialogue) {
		ImGui::SeparatorText("Dialogue");
		if constexpr (requires { target.entity; }) {
			EntityInspectorTarget& dialogue_target{ target };
			changed |= DrawFocusedComponent<EntityInspectorTarget, DialogueData>(
				dialogue_target, "Dialogue", [&](DialogueData& data) {
					AutoLabelWidthScope labels{ "FocusedDialogueFields" };
					const bool local_changed{
						DrawValue(target.ctx, "Continue Key", data.continue_key)
					};
					ImGui::TextDisabled("Current Dialogue: %s", data.current_dialogue.c_str());
					ImGui::TextDisabled(
						"Runtime: line %zu, page %zu, %s", data.current_line, data.current_page,
						data.open ? "open" : "closed"
					);
					return local_changed;
				}
			);
			changed |= DrawManagedVisualChild(
				target.ctx, FindDialogueManagedPart(target.entity, DialoguePartRole::Text), "Text"
			);
			changed |= DrawManagedVisualChild(
				target.ctx, FindDialogueManagedPart(target.entity, DialoguePartRole::Background),
				"Background"
			);
		}
	}

	return changed;
}

template <typename Target>
bool DrawCameraParentRenderTarget(Target& target) {
	if constexpr (!requires { target.entity; }) {
		return false;
	} else {
		Entity camera_entity{ target.entity };

		if (!camera_entity) {
			return false;
		}

		struct RenderTargetOption {
			UUID uuid{};
			std::string label{};
		};

		auto uuid_text = []<typename T>(const T& value) {
			if constexpr (JsonSerializable<T>) {
				json serialized = value;

				if (serialized.is_string()) {
					return serialized.template get<std::string>();
				}

				return serialized.dump();
			} else if constexpr (requires { std::to_string(value.value); }) {
				return std::to_string(value.value);
			} else if constexpr (requires { std::to_string(value.value()); }) {
				return std::to_string(value.value());
			} else {
				return std::to_string(Hash(value));
			}
		};

		auto& scene{ camera_entity.GetScene() };
		const Entity scene_target{ scene.GetRenderTarget() };
		const Entity main_camera{ scene.GetCamera() };
		const Entity fixed_camera{ scene.GetFixedCamera() };
		const bool parent_target_read_only{ camera_entity == main_camera ||
											camera_entity == fixed_camera };

		std::string scene_target_label{ "Scene Target" };
		if (scene_target && scene_target.Has<Tag>()) {
			scene_target_label = std::string{ scene_target.Get<Tag>() };
		}

		std::vector<RenderTargetOption> render_targets;
		for (auto [entity, _target] : scene.EntitiesWith<::ptgn::impl::RenderTargetDesc>()) {
			if (!entity || entity == scene_target || !entity.Has<Tag, UUID>()) {
				continue;
			}

			const UUID uuid{ entity.Get<UUID>() };
			std::string label{ std::string{ entity.Get<Tag>() } };
			label += " [";
			label += uuid_text(uuid);
			label += "]";

			render_targets.push_back(
				RenderTargetOption{
					.uuid  = uuid,
					.label = std::move(label),
				}
			);
		}

		std::ranges::sort(render_targets, {}, &RenderTargetOption::label);

		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<::ptgn::impl::ParentRenderTarget>()) };

		auto before{ target.template Capture<::ptgn::impl::ParentRenderTarget>() };
		auto value{ before };
		bool changed{ false };

		changed |= DrawPropertyRow("Parent Render Target", [&]() {
			bool row_changed{ false };
			const char* preview{ scene_target_label.c_str() };
			std::string missing_preview;

			if (value) {
				const auto selected{ std::ranges::find(
					render_targets, value->render_target, &RenderTargetOption::uuid
				) };

				if (selected != render_targets.end()) {
					preview = selected->label.c_str();
				} else {
					missing_preview	 = "Missing Render Target [";
					missing_preview += uuid_text(value->render_target);
					missing_preview += "]";
					preview			 = missing_preview.c_str();
				}
			}

			ImGui::BeginDisabled(parent_target_read_only);
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo("##RenderTarget", preview)) {
				const bool scene_selected{ !value.has_value() };
				if (ImGui::Selectable(scene_target_label.c_str(), scene_selected)) {
					value.reset();
					row_changed = true;
				}

				if (scene_selected) {
					ImGui::SetItemDefaultFocus();
				}

				if (!render_targets.empty()) {
					ImGui::Separator();
				}

				for (const auto& option : render_targets) {
					const bool selected{ value && value->render_target == option.uuid };

					if (ImGui::Selectable(option.label.c_str(), selected)) {
						value = ::ptgn::impl::ParentRenderTarget{
							.render_target = option.uuid,
						};
						row_changed = true;
					}

					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}
			ImGui::EndDisabled();

			if (parent_target_read_only) {
				DrawTooltip("The main and fixed cameras cannot change their parent render target.");
			}

			return row_changed;
		});

		if (changed) {
			target.template SetLive<::ptgn::impl::ParentRenderTarget>(value);
		}

		auto after{ target.template Capture<::ptgn::impl::ParentRenderTarget>() };
		TrackComponentState(
			target, "Edit Parent Render Target", std::move(before), std::move(after), changed
		);

		return changed;
	}
}

template <typename Target>
bool DrawCameraFeature(Target& target) {
	if (!HasCameraFeature(target)) {
		return false;
	}

	const auto header{ DrawFeatureHeader(
		target, InspectorFeature::Camera, "Camera", ImGuiTreeNodeFlags_None,
		CameraFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;
	AutoLabelWidthScope camera_label_width{ "CameraFeatureFields" };

	bool changed{ header.changed };
	changed |= DrawCameraParentRenderTarget(target);
	changed |= DrawRequiredComponent<Target, ::ptgn::impl::CameraData>(
		target, "Camera", false, [&target](::ptgn::impl::CameraData& value) {
			return DrawRegisteredComponentContents(
				target.ctx, Hash<::ptgn::impl::CameraData>(), std::addressof(value)
			);
		}
	);
	changed |= DrawRequiredComponent<Target, ::ptgn::impl::CameraMask>(
		target, "Layers", false, [](::ptgn::impl::CameraMask& value) {
			bool masks_changed{ DrawLayerMaskValue("Include Layer", value.include) };
			masks_changed |= DrawLayerMaskValue("Exclude Layer", value.exclude);
			return masks_changed;
		}
	);

	return changed;
}

struct ScriptEntryEditorSnapshot {
	bool enabled{ true };
	TypeHashValue type_hash{ 0 };
	std::string name{};
	json value	  = json::object();
	json sequence = json::object();
	std::function<std::unique_ptr<Script>()> runtime_factory{};
	std::vector<std::function<std::unique_ptr<Script>()>> step_runtime_factories{};
	std::vector<std::function<std::unique_ptr<Script>()>> lifecycle_runtime_factories{};

	[[nodiscard]] bool HasSameAuthoredState(const ScriptEntryEditorSnapshot& other) const {
		return enabled == other.enabled && type_hash == other.type_hash && name == other.name &&
			   value == other.value && sequence == other.sequence;
	}
};

using ScriptEntriesEditorSnapshot = std::vector<ScriptEntryEditorSnapshot>;

[[nodiscard]] ScriptEntriesEditorSnapshot CaptureScriptEntriesEditorSnapshot(
	const std::vector<ScriptEntry>& entries
) {
	ScriptEntriesEditorSnapshot snapshot;
	snapshot.reserve(entries.size());

	for (const auto& entry : entries) {
		const ScriptSequence& sequence{ entry.instance ? entry.instance->sequence
													   : entry.sequence };

		ScriptEntryEditorSnapshot entry_snapshot{
			.enabled		 = entry.enabled,
			.type_hash		 = entry.type_hash,
			.name			 = entry.name,
			.value			 = entry.value,
			.sequence		 = sequence,
			.runtime_factory = entry.runtime_factory,
		};

		entry_snapshot.step_runtime_factories.reserve(sequence.steps.size());
		for (const auto& step : sequence.steps) {
			entry_snapshot.step_runtime_factories.push_back(step.runtime_factory);
		}

		entry_snapshot.lifecycle_runtime_factories.reserve(sequence.lifecycle_actions.size());
		for (const auto& lifecycle : sequence.lifecycle_actions) {
			entry_snapshot.lifecycle_runtime_factories.push_back(lifecycle.action.runtime_factory);
		}

		snapshot.push_back(std::move(entry_snapshot));
	}

	return snapshot;
}

[[nodiscard]] bool HasSameAuthoredState(
	const ScriptEntriesEditorSnapshot& lhs, const ScriptEntriesEditorSnapshot& rhs
) {
	if (lhs.size() != rhs.size()) {
		return false;
	}

	for (std::size_t i{ 0 }; i < lhs.size(); ++i) {
		if (!lhs[i].HasSameAuthoredState(rhs[i])) {
			return false;
		}
	}

	return true;
}

struct ScriptsEditorSnapshot {
	ScriptEntriesEditorSnapshot scripts{};
	ScriptEntriesEditorSnapshot pending_additions{};
	std::vector<SequenceId> pending_removals{};
};

[[nodiscard]] ScriptsEditorSnapshot CaptureScriptsEditorSnapshot(
	const ::ptgn::impl::Scripts& scripts
) {
	return ScriptsEditorSnapshot{
		.scripts		   = CaptureScriptEntriesEditorSnapshot(scripts.scripts),
		.pending_additions = CaptureScriptEntriesEditorSnapshot(scripts.pending_additions),
		.pending_removals  = scripts.pending_removals,
	};
}

[[nodiscard]] bool HasSameAuthoredState(
	const ScriptsEditorSnapshot& lhs, const ScriptsEditorSnapshot& rhs
) {
	return HasSameAuthoredState(lhs.scripts, rhs.scripts) &&
		   HasSameAuthoredState(lhs.pending_additions, rhs.pending_additions) &&
		   lhs.pending_removals == rhs.pending_removals;
}

[[nodiscard]] std::vector<ScriptEntry> RestoreScriptEntriesEditorSnapshot(
	const ScriptEntriesEditorSnapshot& snapshot
) {
	std::vector<ScriptEntry> entries;
	entries.reserve(snapshot.size());

	for (const auto& entry_snapshot : snapshot) {
		ScriptSequence sequence;
		entry_snapshot.sequence.get_to(sequence);

		for (std::size_t i{ 0 };
			 i < sequence.steps.size() && i < entry_snapshot.step_runtime_factories.size(); ++i) {
			sequence.steps[i].runtime_factory = entry_snapshot.step_runtime_factories[i];
		}

		for (std::size_t i{ 0 }; i < sequence.lifecycle_actions.size() &&
								 i < entry_snapshot.lifecycle_runtime_factories.size();
			 ++i) {
			sequence.lifecycle_actions[i].action.runtime_factory =
				entry_snapshot.lifecycle_runtime_factories[i];
		}

		ScriptEntry entry;
		entry.enabled		  = entry_snapshot.enabled;
		entry.type_hash		  = entry_snapshot.type_hash;
		entry.name			  = entry_snapshot.name;
		entry.value			  = entry_snapshot.value;
		entry.sequence		  = std::move(sequence);
		entry.runtime_factory = entry_snapshot.runtime_factory;
		entries.push_back(std::move(entry));
	}

	return entries;
}

void RestoreScriptsEditorSnapshot(Entity entity, const ScriptsEditorSnapshot& snapshot) {
	if (!entity) {
		return;
	}

	auto& scripts{ entity.TryAdd<::ptgn::impl::Scripts>() };
	scripts.scripts			  = RestoreScriptEntriesEditorSnapshot(snapshot.scripts);
	scripts.pending_additions = RestoreScriptEntriesEditorSnapshot(snapshot.pending_additions);
	scripts.pending_removals  = snapshot.pending_removals;
	scripts.channels.clear();
	scripts.create_event_dispatched = false;
	scripts.Attach(entity);
}

struct SharedScriptSequenceEditorSnapshot {
	SequenceId id{ 0 };
	json sequence = json::object();
	std::vector<std::function<std::unique_ptr<Script>()>> step_runtime_factories{};
	std::vector<std::function<std::unique_ptr<Script>()>> lifecycle_runtime_factories{};

	[[nodiscard]] bool HasSameAuthoredState(const SharedScriptSequenceEditorSnapshot& other) const {
		return id == other.id && sequence == other.sequence;
	}
};

using SharedScriptSequencesEditorSnapshot = std::vector<SharedScriptSequenceEditorSnapshot>;

[[nodiscard]] SharedScriptSequencesEditorSnapshot CaptureSharedScriptSequencesEditorSnapshot(
	const SharedScriptSequenceRegistry& registry
) {
	SharedScriptSequencesEditorSnapshot snapshot;
	snapshot.reserve(registry.sequences.size());

	for (const auto& sequence : registry.sequences) {
		SharedScriptSequenceEditorSnapshot sequence_snapshot{
			.id		  = sequence.id,
			.sequence = sequence,
		};

		sequence_snapshot.step_runtime_factories.reserve(sequence.steps.size());
		for (const auto& step : sequence.steps) {
			sequence_snapshot.step_runtime_factories.push_back(step.runtime_factory);
		}

		sequence_snapshot.lifecycle_runtime_factories.reserve(sequence.lifecycle_actions.size());
		for (const auto& lifecycle : sequence.lifecycle_actions) {
			sequence_snapshot.lifecycle_runtime_factories.push_back(
				lifecycle.action.runtime_factory
			);
		}

		snapshot.push_back(std::move(sequence_snapshot));
	}

	return snapshot;
}

[[nodiscard]] bool HasSameAuthoredState(
	const SharedScriptSequencesEditorSnapshot& lhs, const SharedScriptSequencesEditorSnapshot& rhs
) {
	if (lhs.size() != rhs.size()) {
		return false;
	}

	for (std::size_t i{ 0 }; i < lhs.size(); ++i) {
		if (!lhs[i].HasSameAuthoredState(rhs[i])) {
			return false;
		}
	}

	return true;
}

void RestoreSharedScriptSequencesEditorSnapshot(
	SharedScriptSequenceRegistry& registry, const SharedScriptSequencesEditorSnapshot& snapshot
) {
	registry.sequences.clear();
	registry.sequences.reserve(snapshot.size());

	for (const auto& sequence_snapshot : snapshot) {
		ScriptSequence sequence;
		sequence_snapshot.sequence.get_to(sequence);
		sequence.id = sequence_snapshot.id;

		for (std::size_t i{ 0 };
			 i < sequence.steps.size() && i < sequence_snapshot.step_runtime_factories.size();
			 ++i) {
			sequence.steps[i].runtime_factory = sequence_snapshot.step_runtime_factories[i];
		}

		for (std::size_t i{ 0 }; i < sequence.lifecycle_actions.size() &&
								 i < sequence_snapshot.lifecycle_runtime_factories.size();
			 ++i) {
			sequence.lifecycle_actions[i].action.runtime_factory =
				sequence_snapshot.lifecycle_runtime_factories[i];
		}

		registry.sequences.push_back(std::move(sequence));
	}
}

struct EntityScriptsEditorSnapshot {
	EntityReference reference{};
	ScriptsEditorSnapshot scripts{};
};

struct TimerReferenceSceneSnapshot {
	std::vector<EntityScriptsEditorSnapshot> entities{};
	SharedScriptSequencesEditorSnapshot shared_sequences{};
};

[[nodiscard]] TimerReferenceSceneSnapshot CaptureTimerReferenceSceneSnapshot(Scene& scene) {
	TimerReferenceSceneSnapshot snapshot{
		.shared_sequences =
			CaptureSharedScriptSequencesEditorSnapshot(scene.ctx().shared_script_sequences),
	};

	for (Entity entity : scene.Entities()) {
		const auto* scripts{ entity.TryGet<::ptgn::impl::Scripts>() };
		if (!scripts) {
			continue;
		}

		snapshot.entities.push_back(
			EntityScriptsEditorSnapshot{
				.reference = MakeEntityReference(entity),
				.scripts   = CaptureScriptsEditorSnapshot(*scripts),
			}
		);
	}

	return snapshot;
}

void RestoreTimerReferenceSceneSnapshot(
	Editor& editor, const EntityReference& anchor_reference,
	const TimerReferenceSceneSnapshot& snapshot
) {
	Entity anchor{ anchor_reference.Resolve(editor) };
	if (!anchor) {
		return;
	}

	auto& scene{ anchor.GetScene() };
	RestoreSharedScriptSequencesEditorSnapshot(
		scene.ctx().shared_script_sequences, snapshot.shared_sequences
	);

	for (const auto& entity_snapshot : snapshot.entities) {
		RestoreScriptsEditorSnapshot(
			entity_snapshot.reference.Resolve(editor), entity_snapshot.scripts
		);
	}
}

[[nodiscard]] bool RenameTimerJsonReference(
	json& value, const TimerKey& old_key, const TimerKey& new_key
) {
	if (!value.is_object()) {
		return false;
	}

	auto timer_it{ value.find("timer") };
	if (timer_it == value.end()) {
		return false;
	}

	TimerKey timer;
	try {
		timer_it->get_to(timer);
	} catch (...) {
		return false;
	}

	if (timer != old_key) {
		return false;
	}

	value["timer"] = new_key;
	return true;
}

[[nodiscard]] bool ScriptStepTargetsEntity(Entity owner, const ScriptStep& step, Entity target) {
	if (!owner || !target) {
		return false;
	}

	if (!step.target) {
		return owner == target;
	}

	const auto targets{ ResolveEntityFilter(*step.target, owner.GetScene(), owner) };
	return std::ranges::contains(targets, target);
}

bool RenameTimerActionReference(
	Entity owner, ScriptStep& step, Entity timer_entity, const TimerKey& old_key,
	const TimerKey& new_key
) {
	if (step.type_hash != Hash<TimerActionScript>() ||
		!ScriptStepTargetsEntity(owner, step, timer_entity)) {
		return false;
	}

	if (!RenameTimerJsonReference(step.value, old_key, new_key)) {
		return false;
	}

	step.runtime_factory = {};
	return true;
}

bool RenameTimerReferencesInSequence(
	Entity owner, ScriptSequence& sequence, Entity timer_entity, const TimerKey& old_key,
	const TimerKey& new_key
) {
	bool changed{ false };

	if (owner == timer_entity) {
		auto rename_event = [&](EventCondition& condition) {
			if (condition.type_hash == Hash<event::TimerElapsed>()) {
				changed |= RenameTimerJsonReference(condition.value, old_key, new_key);
			}
		};

		for (auto& condition : sequence.start_events) {
			rename_event(condition);
		}
		for (auto& condition : sequence.stop_events) {
			rename_event(condition);
		}
	}

	for (auto& step : sequence.steps) {
		changed |= RenameTimerActionReference(owner, step, timer_entity, old_key, new_key);
	}

	for (auto& lifecycle : sequence.lifecycle_actions) {
		changed |=
			RenameTimerActionReference(owner, lifecycle.action, timer_entity, old_key, new_key);
	}

	return changed;
}

bool RenameTimerReferences(Entity timer_entity, const TimerKey& old_key, const TimerKey& new_key) {
	if (!timer_entity || old_key == new_key) {
		return false;
	}

	auto& scene{ timer_entity.GetScene() };
	bool changed{ false };

	for (Entity owner : scene.Entities()) {
		auto* scripts{ owner.TryGet<::ptgn::impl::Scripts>() };
		if (!scripts) {
			continue;
		}

		for (auto& entry : scripts->scripts) {
			ScriptSequence& binding{ entry.instance ? entry.instance->sequence : entry.sequence };

			ScriptSequence* sequence{ std::addressof(binding) };
			if (binding.shared_reference) {
				sequence = scene.ctx().shared_script_sequences.Find(binding.shared_sequence_id);
			}

			if (!sequence || !RenameTimerReferencesInSequence(
								 owner, *sequence, timer_entity, old_key, new_key
							 )) {
				continue;
			}

			changed = true;

			if (!binding.shared_reference && entry.instance) {
				SequenceId sequence_id{ binding.id };
				entry.sequence		   = binding;
				entry.sequence.id	   = sequence_id;
				entry.sequence.runtime = ScriptSequenceRuntime{};
			}
		}
	}

	return changed;
}

template <typename Target>
bool DrawScriptsFeature(Target& target) {
	if (!HasScriptsFeature(target)) {
		return false;
	}

	const auto header{ DrawFeatureHeader(
		target, InspectorFeature::Scripts, "Scripts", ImGuiTreeNodeFlags_None,
		ScriptsFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;

	bool changed{ header.changed };

	if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
		if (!target.entity.template Has<::ptgn::impl::Scripts>()) {
			return changed;
		}

		auto& scripts{ target.entity.template Get<::ptgn::impl::Scripts>() };
		auto& shared_sequences{ target.entity.GetScene().ctx().shared_script_sequences };
		auto before{ CaptureScriptsEditorSnapshot(scripts) };
		auto before_shared{ CaptureSharedScriptSequencesEditorSnapshot(shared_sequences) };
		const bool scripts_changed{ DrawScriptsComponent(target.ctx, scripts) };

		if (!scripts_changed) {
			return changed;
		}

		auto after{ CaptureScriptsEditorSnapshot(scripts) };
		auto after_shared{ CaptureSharedScriptSequencesEditorSnapshot(shared_sequences) };

		if (HasSameAuthoredState(before, after) &&
			HasSameAuthoredState(before_shared, after_shared)) {
			return changed;
		}

		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<::ptgn::impl::Scripts>()) };
		const ImGuiID key{ ImGui::GetID("##ComponentEdit") };
		Editor* editor{ std::addressof(target.ctx.editor) };
		const EntityReference reference{ MakeEntityReference(target.entity) };

		TrackUndoableInteraction(
			target.ctx, key, "Edit Scripts", true,
			[editor, reference, before = std::move(before),
			 before_shared = std::move(before_shared)]() {
				Entity entity{ reference.Resolve(*editor) };
				if (!entity) {
					return;
				}

				RestoreSharedScriptSequencesEditorSnapshot(
					entity.GetScene().ctx().shared_script_sequences, before_shared
				);
				RestoreScriptsEditorSnapshot(entity, before);
			},
			[editor, reference, after = std::move(after),
			 after_shared = std::move(after_shared)]() {
				Entity entity{ reference.Resolve(*editor) };
				if (!entity) {
					return;
				}

				RestoreSharedScriptSequencesEditorSnapshot(
					entity.GetScene().ctx().shared_script_sequences, after_shared
				);
				RestoreScriptsEditorSnapshot(entity, after);
			}
		);

		changed = true;
	} else {
		changed |= DrawRequiredComponent<Target, ::ptgn::impl::Scripts>(
			target, "Scripts", false, [&target](::ptgn::impl::Scripts& value) {
				return DrawRegisteredComponentContents(
					target.ctx, Hash<::ptgn::impl::Scripts>(), std::addressof(value)
				);
			}
		);
	}

	return changed;
}

[[nodiscard]] TimerKey MakeUniqueTimerKey(const ::ptgn::impl::Timers& timers) {
	for (std::size_t index{ 1 };; ++index) {
		TimerKey candidate{ index == 1 ? std::string{ "Timer" }
									   : std::string{ "Timer " } + std::to_string(index) };

		bool exists{ std::ranges::any_of(timers.timers, [&candidate](const TimerEntry& entry) {
			return entry.config.key == candidate;
		}) };

		if (!exists) {
			return candidate;
		}
	}
}

[[nodiscard]] bool HasDuplicateTimerKey(const ::ptgn::impl::Timers& timers, std::size_t index) {
	if (index >= timers.timers.size()) {
		return false;
	}

	const auto& key{ timers.timers[index].config.key };
	if (key.value.empty()) {
		return false;
	}

	for (std::size_t other{ 0 }; other < timers.timers.size(); ++other) {
		if (other != index && timers.timers[other].config.key == key) {
			return true;
		}
	}

	return false;
}

[[nodiscard]] bool IsTimerRuntimeActive(EditorContext& ctx) {
	return ctx.editor.IsPlaying() || ctx.editor.IsDirectRuntime();
}

[[nodiscard]] std::string FormatTimerRuntimeDuration(millisecondsf value) {
	float milliseconds{ std::abs(value.count()) };
	std::string_view unit{ "ms" };

	if (milliseconds >= 604800000.0f) {
		unit = "w";
	} else if (milliseconds >= 86400000.0f) {
		unit = "d";
	} else if (milliseconds >= 3600000.0f) {
		unit = "h";
	} else if (milliseconds >= 60000.0f) {
		unit = "m";
	} else if (milliseconds >= 1000.0f) {
		unit = "s";
	}

	return FormatInspectorDuration(value, unit);
}

void SyncTimerRuntimeSnapshot(Entity entity, const TimerKey& live_key, TimerEntry& edited_entry) {
	if (!entity) {
		return;
	}

	const auto* live_timers{ entity.TryGet<::ptgn::impl::Timers>() };
	if (!live_timers) {
		return;
	}

	const auto it{ std::ranges::find_if(live_timers->timers, [&live_key](const TimerEntry& entry) {
		return entry.config.key == live_key;
	}) };
	if (it != live_timers->timers.end()) {
		edited_entry.runtime = it->runtime;
	}
}

millisecondsf timer_runtime_adjustment{ 100.0f };

void DrawTimerRuntimeControls(Entity entity, const TimerKey& live_key, TimerEntry& edited_entry) {
	TimerHandle timer{ GetTimer(entity, live_key) };
	if (!timer) {
		ImGui::TextDisabled("Runtime timer is unavailable.");
		return;
	}

	const char* state{ timer.IsPaused()		 ? "Paused"
					   : timer.IsRunning()	 ? "Running"
					   : timer.IsCompleted() ? "Complete"
											 : "Stopped" };

	const std::string elapsed{ FormatTimerRuntimeDuration(timer.Elapsed()) };
	const std::string duration{ FormatTimerRuntimeDuration(timer.Duration()) };
	ImGui::Text("%s  %s / %s", state, elapsed.c_str(), duration.c_str());
	ImGui::ProgressBar(timer.Progress(), ImVec2{ -FLT_MIN, 0.0f });
	ImGui::TextDisabled(
		"Elapsed count: %llu", static_cast<unsigned long long>(timer.ElapsedCount())
	);

	bool runtime_changed{ false };
	if (ImGui::Button("Start")) {
		runtime_changed |= timer.Start();
	}
	ImGui::SameLine();
	if (ImGui::Button("Restart")) {
		runtime_changed |= timer.Restart();
	}
	ImGui::SameLine();

	if (timer.IsPaused()) {
		if (ImGui::Button("Resume")) {
			runtime_changed |= timer.Resume();
		}
	} else {
		ImGui::BeginDisabled(!timer.IsRunning());
		if (ImGui::Button("Pause")) {
			runtime_changed |= timer.Pause();
		}
		ImGui::EndDisabled();
	}

	ImGui::SameLine();
	if (ImGui::Button("Stop")) {
		runtime_changed |= timer.Stop();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset")) {
		runtime_changed |= timer.Reset();
	}

	float spacing{ ImGui::GetStyle().ItemSpacing.x };
	float advance_width{ ImGui::CalcTextSize("Advance").x +
						 ImGui::GetStyle().FramePadding.x * 2.0f };
	float rewind_width{ ImGui::CalcTextSize("Rewind").x + ImGui::GetStyle().FramePadding.x * 2.0f };
	float adjustment_width{ std::max(
		60.0f, ImGui::GetContentRegionAvail().x - advance_width - rewind_width - spacing * 2.0f
	) };

	if (DrawDurationTextInput(
			"##TimerRuntimeAdjustment", timer_runtime_adjustment, adjustment_width, false,
			"Positive duration to advance or rewind."
		)) {
		timer_runtime_adjustment =
			millisecondsf{ std::max(0.001f, timer_runtime_adjustment.count()) };
	}

	ImGui::SameLine(0.0f, spacing);
	if (ImGui::Button("Advance", ImVec2{ advance_width, 0.0f })) {
		runtime_changed |= timer.Advance(timer_runtime_adjustment);
	}

	ImGui::SameLine(0.0f, spacing);
	if (ImGui::Button("Rewind", ImVec2{ rewind_width, 0.0f })) {
		runtime_changed |= timer.Rewind(timer_runtime_adjustment);
	}

	if (runtime_changed) {
		SyncTimerRuntimeSnapshot(entity, live_key, edited_entry);
	}
}

struct TimerRename {
	TimerKey old_key{};
	TimerKey new_key{};
};

template <typename Target>
bool DrawTimersContents(
	Target& target, ::ptgn::impl::Timers& timers, std::vector<TimerRename>* renames = nullptr,
	std::string* undo_label = nullptr, std::optional<ImGuiID>* undo_key = nullptr
) {
	bool changed{ false };

	auto set_undo = [undo_label, undo_key](std::string_view label, const char* id) {
		if (undo_label) {
			*undo_label = label;
		}
		if (undo_key) {
			*undo_key = ImGui::GetID(id);
		}
	};

	if (ImGui::Button("+ Timer", ImVec2{ -FLT_MIN, 0.0f })) {
		timers.timers.push_back(
			TimerEntry{
				.config =
					TimerConfig{
						.key = MakeUniqueTimerKey(timers),
					},
			}
		);
		changed = true;
		set_undo("Add Timer", "##AddTimerEdit");
	}

	std::optional<std::size_t> remove_index;

	for (std::size_t index{ 0 }; index < timers.timers.size(); ++index) {
		ScopedID timer_scope{ static_cast<int>(index) };
		auto& entry{ timers.timers[index] };
		const TimerKey live_key{ entry.config.key };
		std::string label{ entry.config.key.value.empty()
							   ? std::string{ "Timer " } + std::to_string(index + 1)
							   : entry.config.key.value };

		if constexpr (requires { target.entity; }) {
			if (target.entity && IsTimerRuntimeActive(target.ctx) && !live_key.value.empty()) {
				TimerHandle runtime_timer{ GetTimer(target.entity, live_key) };
				if (runtime_timer) {
					const char* state{ runtime_timer.IsPaused()		 ? "Paused"
									   : runtime_timer.IsRunning()	 ? "Running"
									   : runtime_timer.IsCompleted() ? "Complete"
																	 : "Stopped" };
					label += "    ";
					label += state;
					label += " ";
					label += FormatTimerRuntimeDuration(runtime_timer.Elapsed());
					label += " / ";
					label += FormatTimerRuntimeDuration(runtime_timer.Duration());
				}
			}
		}

		float button_size{ ImGui::GetFrameHeight() };
		bool open{ false };

		if (ImGui::BeginTable(
				"##TimerHeader", 2,
				ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings |
					ImGuiTableFlags_NoPadOuterX
			)) {
			ImGui::TableSetupColumn("Timer", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, button_size);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, button_size);

			ImGui::TableSetColumnIndex(0);
			open = ImGui::TreeNodeEx(
				(label + "##Timer").c_str(),
				ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen |
					ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_NoTreePushOnOpen
			);

			ImGui::TableSetColumnIndex(1);
			if (ImGui::Button("-##RemoveTimer", ImVec2{ button_size, button_size })) {
				remove_index = index;
				set_undo("Remove Timer", "##RemoveTimerEdit");
			}
			DrawTooltip("Remove this timer.");

			ImGui::EndTable();
		}

		if (!open) {
			continue;
		}

		ScopedIndent timer_indent;

		bool name_changed{ DrawPropertyRow("Name", [&]() {
			ImGui::SetNextItemWidth(-FLT_MIN);
			return ImGui::InputTextWithHint("##TimerName", "Timer name", &entry.config.key.value);
		}) };

		if (name_changed) {
			if (renames) {
				renames->push_back(
					TimerRename{
						.old_key = live_key,
						.new_key = entry.config.key,
					}
				);
			}
			changed = true;
			set_undo("Rename Timer", "##RenameTimerEdit");
		}

		if (entry.config.key.value.empty()) {
			ImGui::TextDisabled("Timer names must not be empty.");
		} else if (HasDuplicateTimerKey(timers, index)) {
			ImGui::TextDisabled("Timer names must be unique on an entity.");
		}

		if (DrawValue(target.ctx, "Duration", entry.config.duration)) {
			changed = true;
			set_undo("Change Timer Duration", "##TimerDurationEdit");
		}

		if (DrawValue(target.ctx, "Mode", entry.config.mode)) {
			changed = true;
			set_undo("Change Timer Mode", "##TimerModeEdit");
		}

		if (DrawValue(target.ctx, "Start Automatically", entry.config.start_automatically)) {
			changed = true;
			set_undo("Change Timer Auto Start", "##TimerAutoStartEdit");
		}
		DrawTooltip("Unchecked: start this timer manually or with a Timer Action.");

		if constexpr (requires { target.entity; }) {
			if (target.entity && IsTimerRuntimeActive(target.ctx)) {
				ImGui::SeparatorText("Runtime");
				DrawTimerRuntimeControls(target.entity, live_key, entry);
			}
		}
	}

	if (remove_index) {
		timers.timers.erase(timers.timers.begin() + static_cast<std::ptrdiff_t>(*remove_index));
		changed = true;
	}

	if (timers.timers.empty()) {
		ImGui::TextDisabled("No timers.");
	}

	return changed;
}

bool DrawGroupContents(Group& group) {
	bool changed{ false };

	if (ImGui::Button("+ Group", ImVec2{ -FLT_MIN, 0.0f })) {
		group.groups.emplace_back();
		changed = true;
	}

	std::optional<std::size_t> remove_index;

	for (std::size_t index{ 0 }; index < group.groups.size(); ++index) {
		ScopedID group_scope{ static_cast<int>(index) };
		std::string label{ "Group " + std::to_string(index + 1) };

		changed |= DrawPropertyRow(label, [&]() {
			float remove_width{ ImGui::GetFrameHeight() };
			float spacing{ ImGui::GetStyle().ItemSpacing.x };
			float available{ ImGui::GetContentRegionAvail().x };
			float field_width{ std::max(60.0f, available - remove_width - spacing) };

			ImGui::SetNextItemWidth(field_width);
			bool row_changed{ ImGui::InputText("##Value", &group.groups[index]) };

			ImVec2 field_min{ ImGui::GetItemRectMin() };
			ImVec2 field_max{ ImGui::GetItemRectMax() };

			bool duplicate{ false };

			if (!group.groups[index].empty()) {
				for (std::size_t earlier_index{ 0 }; earlier_index < index; ++earlier_index) {
					if (!group.groups[earlier_index].empty() &&
						group.groups[earlier_index] == group.groups[index]) {
						duplicate = true;
						break;
					}
				}
			}

			if (duplicate) {
				ImGui::GetWindowDrawList()->AddRect(
					field_min, field_max, IM_COL32(255, 200, 0, 255),
					ImGui::GetStyle().FrameRounding, 0, 1.0f
				);

				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Duplicate group; this entry is redundant.");
				}
			}

			ImGui::SameLine(0.0f, spacing);

			if (ImGui::Button("X", ImVec2{ remove_width, remove_width })) {
				remove_index = index;
			}

			return row_changed;
		});
	}

	if (remove_index) {
		group.groups.erase(group.groups.begin() + static_cast<std::ptrdiff_t>(*remove_index));
		changed = true;
	}

	return changed;
}

template <typename Target>
bool DrawTimersComponent(Target& target) {
	using Timers = ::ptgn::impl::Timers;

	ScopedID target_scope{ target.Id() };
	ScopedID component_scope{ static_cast<int>(Hash<Timers>()) };

	auto before{ target.template Capture<Timers>() };
	bool enabled{ before.has_value() };
	bool changed{ false };
	std::string undo_label{ "Edit Timers" };
	std::optional<ImGuiID> undo_key;

	if (ImGui::Checkbox("##Enabled", &enabled)) {
		target.template SetLive<Timers>(
			enabled ? ComponentState<Timers>{ Timers{} } : std::nullopt
		);
		changed	   = true;
		undo_label = enabled ? "Enable Timers" : "Disable Timers";
		undo_key   = ImGui::GetID("##TimersEnabledEdit");
	}

	ImGui::SameLine();

	Timers value{ target.template Capture<Timers>().value_or(Timers{}) };
	std::vector<TimerRename> renames;

	bool open{ ImGui::TreeNodeEx("Timers##Tree", ImGuiTreeNodeFlags_SpanAvailWidth) };

	if (open) {
		ScopedIndent indent;
		ScopedDisabled disabled{ !enabled };

		bool contents_changed{
			DrawTimersContents(target, value, &renames, &undo_label, &undo_key)
		};

		if (enabled && contents_changed) {
			target.template SetLive<Timers>(value);
			changed = true;
		}

		ImGui::TreePop();
	}

	auto after{ target.template Capture<Timers>() };
	if (!changed) {
		return false;
	}

	if (!undo_key) {
		undo_key = ImGui::GetID("##TimersComponentEdit");
	}

	if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
		bool references_changed{ false };
		std::optional<TimerReferenceSceneSnapshot> before_references;
		std::optional<TimerReferenceSceneSnapshot> after_references;

		if (target.entity && !renames.empty()) {
			before_references = CaptureTimerReferenceSceneSnapshot(target.entity.GetScene());

			for (const auto& rename : renames) {
				references_changed |=
					RenameTimerReferences(target.entity, rename.old_key, rename.new_key);
			}

			if (references_changed) {
				after_references = CaptureTimerReferenceSceneSnapshot(target.entity.GetScene());
			}
		}

		if (references_changed) {
			Editor* editor{ std::addressof(target.ctx.editor) };
			EntityReference reference{ MakeEntityReference(target.entity) };
			auto apply_timers{ target.template MakeApply<Timers>() };

			TrackUndoableInteraction(
				target.ctx, *undo_key, undo_label, true,
				[editor, reference, apply_timers, before = std::move(before),
				 before_references = std::move(*before_references)]() mutable {
					apply_timers(before);
					RestoreTimerReferenceSceneSnapshot(*editor, reference, before_references);
				},
				[editor, reference, apply_timers, after = std::move(after),
				 after_references = std::move(*after_references)]() mutable {
					apply_timers(after);
					RestoreTimerReferenceSceneSnapshot(*editor, reference, after_references);
				}
			);

			return true;
		}
	}

	auto apply{ target.template MakeApply<Timers>() };

	TrackUndoableInteraction(
		target.ctx, *undo_key, undo_label, true,
		[apply, before = std::move(before)]() mutable { apply(before); },
		[apply, after = std::move(after)]() mutable { apply(after); }
	);

	return true;
}

template <typename Target>
bool DrawUtilitiesFeature(Target& target) {
	if (!HasUtilitiesFeature(target)) {
		return false;
	}

	const auto header{ DrawFeatureHeader(
		target, InspectorFeature::Utilities, "Utilities", ImGuiTreeNodeFlags_None,
		UtilitiesFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;

	bool changed{ header.changed };

	changed |= DrawTimersComponent(target);

	changed |= DrawOptionalComponent<Target, Group>(target, "Groups", true, [](Group& value) {
		return DrawGroupContents(value);
	});
	changed |= DrawOptionalReflected<Target, Lifetime>(target, "Lifetime", true);

	return changed;
}

template <typename Target>
bool DrawAddFeatureMenu(Target& target) {
	bool changed{ false };

	if (ImGui::Button("Add Feature", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddFeaturePopup");
	}

	if (!ImGui::BeginPopup("AddFeaturePopup")) {
		return false;
	}

	auto item_with_default = [&]<typename Default, typename... T>(
								 InspectorFeature feature, const char* label, bool feature_exists,
								 FeatureComponents<T...> components
							 ) {
		if constexpr (!Target::template Supports<Default>()) {
			return;
		}

		ScopedDisabled disabled{ feature_exists };

		if (ImGui::MenuItem(label)) {
			changed |= AddInspectorFeatureWithDefault<Default>(target, feature, label, components);
		}
	};

	item_with_default.template operator()<Transform>(
		InspectorFeature::Transform, "Transform", HasTransformFeature(target),
		TransformFeatureComponents{}
	);
	const bool primary_render_target{ IsPrimarySceneRenderTarget(target) };
	const bool fixed_camera{ IsReservedFixedCamera(target) };
	const bool visual_exists{ HasVisualFeature(target) };
	const bool camera_exists{ HasCameraFeature(target) };

	if (!primary_render_target && !fixed_camera && !camera_exists) {
		item_with_default.template operator()<::ptgn::impl::IDrawable>(
			InspectorFeature::Visual, "Visual", visual_exists, VisualFeatureComponents{}
		);
	}
	item_with_default.template operator()<::ptgn::impl::Interactive>(
		InspectorFeature::Interaction, "Interaction",
		HasInteractionFeature(target) ||
			HasFeatureComponent<Target, ::ptgn::impl::ButtonData>(target),
		InteractionFeatureComponents{}
	);
	item_with_default.template operator()<Collider>(
		InspectorFeature::Physics, "Physics & Movement", HasPhysicsFeature(target),
		PhysicsFeatureComponents{}
	);
	item_with_default.template operator()<::ptgn::impl::ButtonData>(
		InspectorFeature::UI, "UI", HasFeatureComponent<Target, ::ptgn::impl::ButtonData>(target),
		UIFeatureComponents{}
	);
	if (!primary_render_target && !visual_exists) {
		item_with_default.template operator()<::ptgn::impl::CameraData>(
			InspectorFeature::Camera, "Camera", camera_exists, CameraFeatureComponents{}
		);
	}
	item_with_default.template operator()<::ptgn::impl::Scripts>(
		InspectorFeature::Scripts, "Scripts", HasScriptsFeature(target), ScriptsFeatureComponents{}
	);
	{
		ScopedDisabled disabled{ HasUtilitiesFeature(target) };

		if (ImGui::MenuItem("Utilities")) {
			changed |= AddInspectorFeature(
				target, InspectorFeature::Utilities, "Utilities", UtilitiesFeatureComponents{}
			);
		}
	}

	ImGui::EndPopup();
	return changed;
}

template <typename Target>
bool DrawFeatureInspectorImpl(Target& target) {
	if constexpr (requires { target.entity; }) {
		ClearButtonPreviewIfDifferent(target.ctx, target.entity);
	} else {
		ClearButtonPreviewIfDifferent(target.ctx);
	}

	bool changed{ false };
	if (GetButtonChildInfo(target)) {
		// Advanced managed-part editing chooses the state before Transform/Visual consume it.
		changed |= DrawUIFeature(target);
		changed |= DrawTransformFeature(target);
		changed |= DrawVisualFeature(target);
	} else {
		changed |= DrawTransformFeature(target);
		changed |= DrawUIFeature(target);
		changed |= DrawVisualFeature(target);
	}
	changed |= DrawInteractionFeature(target);
	changed |= DrawPhysicsFeature(target);
	changed |= DrawCameraFeature(target);
	changed |= DrawScriptsFeature(target);
	changed |= DrawUtilitiesFeature(target);

	ImGui::Separator();
	changed |= DrawAddFeatureMenu(target);
	return changed;
}

} // namespace

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

	std::ranges::sort(keys.begin() + std::min<std::size_t>(1, keys.size()), keys.end());
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

bool DrawFeatureInspector(EntityInspectorTarget& target) {
	return DrawFeatureInspectorImpl(target);
}

bool DrawFeatureInspector(PrefabInspectorTarget& target) {
	return DrawFeatureInspectorImpl(target);
}

} // namespace ptgn::editor::inspector
