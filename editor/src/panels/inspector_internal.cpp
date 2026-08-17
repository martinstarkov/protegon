#include "panels/inspector_internal.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <magic_enum/magic_enum.hpp>

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
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "commands/entity/entity_reference.h"
#include "editor/editor.h"
#include "editor/editor_context.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/angle.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/util/hash.h"
#include "panels/inspector_feature_helpers.h"
#include "panels/inspector_fields.h"
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
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/bloom.h"
#include "runtime/graphics/fx/blur.h"
#include "runtime/graphics/fx/gaussian_blur.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/custom_shader.h"
#include "runtime/graphics/graphics.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/sprite_stack.h"
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
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/slider.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"

namespace ptgn::editor::inspector {

namespace {

void DrawDisabledWrappedText(std::string_view text) {
	ImGui::PushStyleColor(
		ImGuiCol_Text,
		ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)
	);
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
	::ptgn::impl::IDrawable, Visible, ::ptgn::impl::IgnoreParentVisibility, Origin, BlendMode,
	Rect, Circle, RoundedRect, Polygon, Ellipse, Triangle, Line, Capsule, Arc, Color, FillStyle,
	TextureKey, ::ptgn::impl::TextureSize, ::ptgn::impl::TextureCrop,
	::ptgn::impl::AnimationData, ::ptgn::SpriteStackData, ::ptgn::impl::Offsets, Tint,
	::ptgn::impl::IgnoreParentTint, ::ptgn::impl::TextData,
	::ptgn::impl::ParticleEmitterData, LightData, ::ptgn::impl::ShadowCaster,
	::ptgn::impl::GraphicsData, ::ptgn::Material, ShaderKey, ::ptgn::impl::RenderTargetDesc,
	::ptgn::impl::RenderMask, ::ptgn::impl::UILayer, ::ptgn::impl::EffectTag,
	::ptgn::impl::HDREffectTag, EffectMargin, Bloom, Blur, GaussianBlur,
	::ptgn::impl::ClearColor, ::ptgn::impl::ClearDepth, ::ptgn::impl::ClearStencil>;

using InteractionFeatureComponents = FeatureComponents<
	::ptgn::impl::Interactive, ::ptgn::impl::Draggable, ::ptgn::impl::Dropzone,
	InteractionLock, ::ptgn::impl::InteractiveTag>;

using PhysicsFeatureComponents = FeatureComponents<
	Collider, RigidBody, ::ptgn::impl::IgnoreParentImmovable, BoundaryBehavior,
	TopDownMovement, PlatformerMovement, PlatformerJump>;

using UIFeatureComponents = FeatureComponents<
	::ptgn::impl::ButtonData, ::ptgn::impl::ButtonAnimationPart, ButtonBackgroundVisuals,
	ButtonBorderVisuals, ButtonSpriteVisuals, ButtonTextVisuals, ButtonSounds,
	::ptgn::impl::SliderData, ::ptgn::impl::ToggleButtonData,
	::ptgn::impl::ToggleButtonGroupData, ::ptgn::impl::ToggleButtonGroupItem,
	::ptgn::impl::DropdownData, ::ptgn::impl::DropdownItem,
	::ptgn::impl::TooltipData, ::ptgn::impl::TooltipHoverData,
	::ptgn::impl::TooltipBackgroundPart, ::ptgn::impl::TooltipTextPart>;

using CameraFeatureComponents = FeatureComponents<
	::ptgn::impl::CameraData,
	::ptgn::impl::CameraMask,
	::ptgn::impl::ParentRenderTarget>;

using ScriptsFeatureComponents = FeatureComponents<::ptgn::impl::Scripts>;

using UtilitiesFeatureComponents = FeatureComponents<Lifetime>;

struct ManualFeatureState {
	FeatureTargetKey target;
	std::array<bool, static_cast<std::size_t>(InspectorFeature::Count)> features{};
	bool text_box_state_initialized{ false };
	bool text_box_enabled{ false };
	bool scale_ratio_locked{ true };
	std::optional<ButtonVisualState> button_visual_state;
};

std::vector<ManualFeatureState>& ManualFeatureStates() {
	static std::vector<ManualFeatureState> states;
	return states;
}

ManualFeatureState& GetManualFeatureState(const FeatureTargetKey& target) {
	auto& states{ ManualFeatureStates() };
	const auto it{ std::ranges::find(states, target, &ManualFeatureState::target) };

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
	Entity child;
	Entity button;
	ButtonChildPart part{ ButtonChildPart::Background };
};

[[nodiscard]] bool IsButtonRoot(Entity entity) {
	return entity &&
		(
			entity.Has<::ptgn::impl::ButtonData>() ||
			entity.Has<::ptgn::impl::SliderData>() ||
			entity.Has<::ptgn::impl::ToggleButtonData>() ||
			entity.Has<::ptgn::impl::DropdownData>()
		);
}

template <typename Target>
[[nodiscard]] std::optional<ButtonChildInfo> GetButtonChildInfo(
	const Target& target
) {
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
				.child = child,
				.button = button,
				.part = ButtonChildPart::Background,
			};
		}

		if (child.Has<ButtonBorderVisuals>()) {
			return ButtonChildInfo{
				.child = child,
				.button = button,
				.part = ButtonChildPart::Border,
			};
		}

		if (child.Has<ButtonTextVisuals>()) {
			return ButtonChildInfo{
				.child = child,
				.button = button,
				.part = ButtonChildPart::Text,
			};
		}

		if (child.Has<ButtonSpriteVisuals>()) {
			return ButtonChildInfo{
				.child = child,
				.button = button,
				.part = ButtonChildPart::Sprite,
			};
		}

		return std::nullopt;
	}
}

template <typename Target>
[[nodiscard]] std::optional<ButtonVisualState> GetButtonVisualEditState(
	const Target& target
) {
	if (!GetButtonChildInfo(target)) {
		return std::nullopt;
	}

	return GetManualFeatureState(
		target.GetFeatureTargetKey()
	).button_visual_state;
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
	static constexpr std::array disabled_press{
		DisabledPress, DisabledHover, Disabled, Press, Hover, Idle
	};
	static constexpr std::array toggled{ Toggled, Idle };
	static constexpr std::array toggled_hover{ ToggledHover, Toggled, Hover, Idle };
	static constexpr std::array toggled_press{
		ToggledPress, ToggledHover, Toggled, Press, Hover, Idle
	};

	switch (state) {
		case Idle: return idle;
		case Hover: return hover;
		case Press: return press;
		case Disabled: return disabled;
		case DisabledHover: return disabled_hover;
		case DisabledPress: return disabled_press;
		case Toggled: return toggled;
		case ToggledHover: return toggled_hover;
		case ToggledPress: return toggled_press;
	}

	return idle;
}

template <typename Visual, typename T, std::size_t N>
[[nodiscard]] std::optional<T> ResolveButtonVisualProperty(
	const std::array<Visual, N>& states,
	ButtonVisualState state,
	const std::optional<T> Visual::* member
) {
	for (const ButtonVisualState fallback : GetButtonVisualStateFallbacks(state)) {
		const auto& visual{
			states[static_cast<std::size_t>(std::to_underlying(fallback))]
		};

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

template <typename Visual, typename T, std::size_t N>
bool DrawButtonVisualOverrideValue(
	EditorContext& ctx,
	std::string_view label,
	std::array<Visual, N>& states,
	ButtonVisualState state,
	std::optional<T> Visual::* member
) {
	auto& visual{
		states[static_cast<std::size_t>(std::to_underlying(state))]
	};
	auto& value{ visual.*member };
	const bool had_override{ value.has_value() };
	const std::optional<T> inherited{
		ResolveButtonVisualProperty(states, state, member)
	};
	const bool changed{
		[&]() {
			if constexpr (std::same_as<T, V2_float>) {
				if (NormalizeFeatureName(label) == "size") {
					return DrawOptionalWHValue(
						label,
						value,
						0.1f,
						0.0f,
						0.0f,
						"%.3f",
						ImGuiSliderFlags_None,
						false,
						inherited
					);
				}
			} else if constexpr (std::same_as<T, std::variant<V2_float, float>>) {
				const bool rect_size{
					(value && std::holds_alternative<V2_float>(*value)) ||
					(!value && inherited && std::holds_alternative<V2_float>(*inherited))
				};

				if (NormalizeFeatureName(label) == "size" && rect_size) {
					std::optional<V2_float> displayed;
					std::optional<V2_float> inherited_size;

					if (value && std::holds_alternative<V2_float>(*value)) {
						displayed = std::get<V2_float>(*value);
					}

					if (inherited && std::holds_alternative<V2_float>(*inherited)) {
						inherited_size = std::get<V2_float>(*inherited);
					}

					const bool local_changed{
						DrawOptionalWHValue(
							label,
							displayed,
							0.1f,
							0.0f,
							0.0f,
							"%.3f",
							ImGuiSliderFlags_None,
							false,
							inherited_size
						)
					};

					if (local_changed) {
						if (displayed) {
							value = T{ *displayed };
						} else {
							value.reset();
						}
					}

					return local_changed;
				}
			}

			return DrawValue(ctx, label, value);
		}()
	};

	if constexpr (
		!std::same_as<T, V2_float> &&
		!std::same_as<T, std::variant<V2_float, float>>
	) {
		if (changed && !had_override && value && inherited) {
			value = *inherited;
		}
	}

	return changed;
}

[[nodiscard]] bool HasButtonVisualOverrides(const ButtonShapeVisual& visual) {
	return visual.size || visual.origin || visual.anchor || visual.transform ||
		visual.color || visual.fill_style;
}

[[nodiscard]] bool HasButtonVisualOverrides(const ButtonTextVisual& visual) {
	return visual.styled_text || visual.box || visual.origin || visual.anchor ||
		visual.transform || visual.auto_box || visual.padding;
}

[[nodiscard]] bool HasButtonVisualOverrides(const ButtonSpriteVisual& visual) {
	return visual.texture || visual.origin || visual.anchor || visual.transform ||
		visual.size || visual.tint || visual.animation || visual.animation_options;
}

[[nodiscard]] Rect GetButtonInspectorLocalRect(Entity button_entity) {
	Button button{ button_entity };
	V2_float size;

	std::visit(
		[&size]<typename T>(const T& value) {
			if constexpr (std::same_as<T, V2_float>) {
				size = value;
			} else {
				size = V2_float{ value * 2.0f };
			}
		},
		button.GetSize()
	);

	return Rect{ size, button.GetOrDefault<Origin>() };
}

[[nodiscard]] bool IsFeatureManuallyAdded(
	const FeatureTargetKey& target, InspectorFeature feature
) {
	const auto& states{ ManualFeatureStates() };
	const auto it{ std::ranges::find(states, target, &ManualFeatureState::target) };

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
			return candidate.target == target;
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
	Target& target,
	InspectorFeature feature,
	std::string_view label,
	ImGuiTreeNodeFlags flags,
	FeatureComponents<T...> components,
	bool allow_delete = true
) {
	ScopedID feature_scope{ static_cast<int>(feature) };

	const std::string header_label{
		std::string{ label } + "##FeatureHeader"
	};

	FeatureHeaderResult result{
		.open = ImGui::CollapsingHeader(
			header_label.c_str(),
			flags
		),
	};

	if (
		allow_delete &&
		ImGui::BeginPopupContextItem("##FeatureContext")
	) {
		if (ImGui::MenuItem("Delete Feature")) {
			result.changed = DeleteInspectorFeature(
				target,
				feature,
				label,
				components
			);
			result.open = false;
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
	Transform transform;
	Depth depth;
	ComponentState<ButtonBackgroundVisuals> button_backgrounds;
	ComponentState<ButtonBorderVisuals> button_borders;
	ComponentState<ButtonTextVisuals> button_texts;
	ComponentState<ButtonSpriteVisuals> button_sprites;
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
		.transform = target.template Capture<Transform>().value_or(Transform{}),
		.depth	   = target.template Capture<Depth>().value_or(Depth{}),
		.button_backgrounds = target.template Capture<ButtonBackgroundVisuals>(),
		.button_borders = target.template Capture<ButtonBorderVisuals>(),
		.button_texts = target.template Capture<ButtonTextVisuals>(),
		.button_sprites = target.template Capture<ButtonSpriteVisuals>(),
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
	auto apply_borders{
		target.template MakeApply<ButtonBorderVisuals>(&MarkButtonBorderDirty)
	};
	auto apply_texts{
		target.template MakeApply<ButtonTextVisuals>(&MarkButtonTextDirty)
	};
	auto apply_sprites{
		target.template MakeApply<ButtonSpriteVisuals>(&MarkButtonSpriteDirty)
	};

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
		state.button_backgrounds,
		&MarkButtonBackgroundDirty
	);
	target.template SetLive<ButtonBorderVisuals>(
		state.button_borders,
		&MarkButtonBorderDirty
	);
	target.template SetLive<ButtonTextVisuals>(
		state.button_texts,
		&MarkButtonTextDirty
	);
	target.template SetLive<ButtonSpriteVisuals>(
		state.button_sprites,
		&MarkButtonSpriteDirty
	);
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
	ComponentState<Visuals>& visuals,
	std::optional<ButtonVisualState> selected_state,
	const Transform& before,
	const Transform& after
) {
	if (!visuals || !selected_state || before == after) {
		return;
	}

	const auto index{
		static_cast<std::size_t>(
			std::to_underlying(*selected_state)
		)
	};
	auto& visual{ visuals->states[index] };

	if (!visual.transform) {
		return;
	}

	auto& transform{ *visual.transform };
	transform.position += after.position - before.position;
	transform.rotation = Radians{
		transform.rotation.value +
		after.rotation.value -
		before.rotation.value
	};

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
	TransformFeatureState<Target>& state,
	std::optional<ButtonVisualState> selected_state,
	const Transform& before
) {
	ApplyButtonVisualTransformDelta(
		state.button_backgrounds,
		selected_state,
		before,
		state.transform
	);
	ApplyButtonVisualTransformDelta(
		state.button_borders,
		selected_state,
		before,
		state.transform
	);
	ApplyButtonVisualTransformDelta(
		state.button_texts,
		selected_state,
		before,
		state.transform
	);
	ApplyButtonVisualTransformDelta(
		state.button_sprites,
		selected_state,
		before,
		state.transform
	);
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

			const bool ignores_parent_position{
				entity.Has<::ptgn::impl::IgnoreParentTransform>() ||
				entity.Has<::ptgn::impl::IgnoreParentPosition>()
			};

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

		const bool ignores_parent_position{
			entity.Has<::ptgn::impl::IgnoreParentTransform>() ||
			entity.Has<::ptgn::impl::IgnoreParentPosition>()
		};

		return !ignores_parent_position && static_cast<bool>(GetParent(entity));
	}
}

template <typename Target, typename Apply>
bool DrawTransformFeatureFields(
	Target& target, TransformFeatureState<Target>& state, Apply apply_state
) {
	constexpr ImGuiTableFlags flags{
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_NoSavedSettings |
		ImGuiTableFlags_NoPadOuterX
	};

	if (!ImGui::BeginTable("##TransformFields", 5, flags)) {
		return false;
	}

	const float compact_width{ ImGui::GetFrameHeight() };
	const float pick_width{
		ImGui::CalcTextSize("Pick").x +
		ImGui::GetStyle().FramePadding.x * 2.0f
	};

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
		changed |= ImGui::DragFloat(
			"##X", &state.transform.position.x, 1.0f, 0.0f, 0.0f, "X: %.0f"
		);
		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(field_width);
		changed |= ImGui::DragFloat(
			"##Y", &state.transform.position.y, 1.0f, 0.0f, 0.0f, "Y: %.0f"
		);
	}
	ImGui::TableSetColumnIndex(3);
	const auto selected_button_state{
		GetButtonVisualEditState(target)
	};
	DrawPositionPickButton(
		target.ctx,
		"Position",
		state.transform.position,
		MakeTransformPositionConverter(target),
		PositionPicker::Apply{
			[apply_state, state, selected_button_state](V2_float picked) mutable {
				const Transform before_transform{ state.transform };
				state.transform.position = picked;
				ApplyButtonVisualTransformDelta(
					state,
					selected_button_state,
					before_transform
				);
				apply_state(state);
			}
		},
		GetTargetWorldReferencePosition(target),
		ShouldShowTransformRelativePosition(target)
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
			"##Value",
			&degrees,
			1.0f,
			0.0f,
			360.0f,
			"%.1f deg",
			ImGuiSliderFlags_AlwaysClamp
		)) {
		state.transform.rotation = Radians{ Degrees{ degrees } };
		changed = true;
	}

	changed |= draw_ignore(
		state.ignore_rotation,
		"Ignore parent rotation."
	);

	ImGui::PopID();

	begin_row("Scale", "Scale");
	ImGui::TableSetColumnIndex(1);
	ImGui::Checkbox("##LockRatio", &editor_state.scale_ratio_locked);
	DrawTooltip(
		"Lock the scale ratio. Editing either axis changes the other by the same proportional factor."
	);

	ImGui::TableSetColumnIndex(2);
	{
		constexpr float kMinScaleMagnitude{ 0.001f };
		constexpr float kScaleRatioEpsilon{ 0.000001f };

		const V2_float before_scale{ state.transform.scale };

		const auto clamp_scale{
			[](float value, float previous) {
				if (std::abs(value) >= kMinScaleMagnitude) {
					return value;
				}

				const float sign{
					value < 0.0f || (value == 0.0f && previous < 0.0f) ? -1.0f : 1.0f
				};
				return sign * kMinScaleMagnitude;
			}
		};

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
			state.transform.scale.x =
				clamp_scale(state.transform.scale.x, before_scale.x);
		}
		if (y_changed) {
			state.transform.scale.y =
				clamp_scale(state.transform.scale.y, before_scale.y);
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

		state.transform.scale.x =
			clamp_scale(state.transform.scale.x, before_scale.x);
		state.transform.scale.y =
			clamp_scale(state.transform.scale.y, before_scale.y);

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

	if (IsFeatureManuallyAdded(
			target.GetFeatureTargetKey(),
			InspectorFeature::Visual
		)) {
		return true;
	}

	return HasFeatureComponent<Target, ::ptgn::impl::IDrawable>(target);
}

template <typename Target>
[[nodiscard]] bool HasInteractionFeature(const Target& target) {
	if (IsFeatureManuallyAdded(
			target.GetFeatureTargetKey(),
			InspectorFeature::Interaction
		)) {
		return true;
	}

	const bool has_editable_component{
		HasFeatureComponent<Target, ::ptgn::impl::Interactive>(target) ||
		HasFeatureComponent<Target, ::ptgn::impl::Draggable>(target) ||
		HasFeatureComponent<Target, ::ptgn::impl::Dropzone>(target) ||
		HasFeatureComponent<Target, ::ptgn::impl::InteractiveTag>(target)
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

	return has_editable_component ||
		   has_visible_read_only_component;
}

template <typename Target>
[[nodiscard]] bool HasPhysicsFeature(const Target& target) {
	return HasInspectorFeature(target, InspectorFeature::Physics, PhysicsFeatureComponents{});
}

template <typename Target>
[[nodiscard]] bool HasUIFeature(const Target& target) {
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

template <typename Visuals, typename Visual>
[[nodiscard]] Origin ResolveButtonChildAnchor(
	const Visuals& visuals,
	ButtonVisualState state,
	Origin fallback
) {
	return ResolveButtonVisualProperty(
		visuals.states,
		state,
		&Visual::anchor
	).value_or(fallback);
}

template <typename Visuals, typename Visual>
[[nodiscard]] V2_float GetButtonChildStateWorldPosition(
	Entity child,
	Entity button,
	ButtonVisualState state,
	Origin fallback_anchor,
	V2_float relative_position
) {
	if (!child || !button || !child.Has<Visuals>()) {
		return {};
	}

	const auto& visuals{ child.Get<Visuals>() };
	const Origin anchor{
		ResolveButtonChildAnchor<Visuals, Visual>(
			visuals,
			state,
			fallback_anchor
		)
	};
	const V2_float anchor_position{
		GetButtonInspectorLocalRect(button).GetOriginPoint(anchor)
	};

	return GetDrawTransform(button).Apply(
		anchor_position + relative_position
	);
}

template <typename Visuals, typename Visual>
[[nodiscard]] PositionPicker::Convert MakeButtonChildStatePositionConverter(
	Entity child,
	Entity button,
	ButtonVisualState state,
	Origin fallback_anchor
) {
	return [child, button, state, fallback_anchor](
		V2_float world_position
	) mutable -> std::optional<V2_float> {
		if (!child || !button || !child.Has<Visuals>()) {
			return std::nullopt;
		}

		const auto& visuals{ child.Get<Visuals>() };
		const Origin anchor{
			ResolveButtonChildAnchor<Visuals, Visual>(
				visuals,
				state,
				fallback_anchor
			)
		};
		const V2_float anchor_position{
			GetButtonInspectorLocalRect(button).GetOriginPoint(anchor)
		};
		const V2_float button_local{
			GetDrawTransform(button).ApplyInverse(world_position)
		};

		return button_local - anchor_position;
	};
}

template <typename Visuals, typename Visual>
[[nodiscard]] Transform ResolveButtonChildStateLocalTransform(
	const Visuals& visuals,
	Entity button,
	ButtonVisualState state,
	Origin fallback_anchor
) {
	Transform transform{
		ResolveButtonVisualProperty(
			visuals.states,
			state,
			&Visual::transform
		).value_or(Transform{})
	};
	const Origin anchor{
		ResolveButtonChildAnchor<Visuals, Visual>(
			visuals,
			state,
			fallback_anchor
		)
	};
	transform.position +=
		GetButtonInspectorLocalRect(button).GetOriginPoint(anchor);
	return transform;
}

[[nodiscard]] PositionPicker::Convert MakeButtonTextBoxPositionConverter(
	Entity child,
	Entity button,
	ButtonVisualState state
) {
	return [child, button, state](
		V2_float world_position
	) mutable -> std::optional<V2_float> {
		if (!child || !button || !child.Has<ButtonTextVisuals>()) {
			return std::nullopt;
		}

		const auto& visuals{ child.Get<ButtonTextVisuals>() };
		const Transform local_transform{
			ResolveButtonChildStateLocalTransform<
				ButtonTextVisuals,
				ButtonTextVisual
			>(
				visuals,
				button,
				state,
				Origin::Center
			)
		};
		const V2_float button_local{
			GetDrawTransform(button).ApplyInverse(world_position)
		};
		return local_transform.ApplyInverse(button_local);
	};
}

[[nodiscard]] std::optional<V2_float> GetButtonTextBoxWorldPosition(
	Entity child,
	Entity button,
	ButtonVisualState state,
	V2_float local_position
) {
	if (!child || !button || !child.Has<ButtonTextVisuals>()) {
		return std::nullopt;
	}

	const auto& visuals{ child.Get<ButtonTextVisuals>() };
	const Transform local_transform{
		ResolveButtonChildStateLocalTransform<
			ButtonTextVisuals,
			ButtonTextVisual
		>(
			visuals,
			button,
			state,
			Origin::Center
		)
	};
	return GetDrawTransform(button).Apply(
		local_transform.Apply(local_position)
	);
}

template <
	typename Target,
	typename Visuals,
	typename Visual,
	typename Callback
>
bool DrawButtonChildStateTransformComponent(
	Target& target,
	const ButtonChildInfo& child_info,
	ButtonVisualState state,
	Origin fallback_anchor,
	std::string_view part_label,
	Callback callback
) {
	if constexpr (!Target::template Supports<Visuals>()) {
		return false;
	} else {
		const auto header_open{
			ImGui::CollapsingHeader(
				"Transform##ButtonVisualStateTransform",
				ImGuiTreeNodeFlags_DefaultOpen
			)
		};

		if (!header_open) {
			return false;
		}

		ScopedIndent feature_indent;
		AutoLabelWidthScope label_width{ "ButtonVisualStateTransformFields" };

		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<Visuals>()) };

		auto before{ target.template Capture<Visuals>() };
		Visuals visuals{ before.value_or(Visuals{}) };
		const auto index{
			static_cast<std::size_t>(std::to_underlying(state))
		};
		auto& visual{ visuals.states[index] };
		Transform transform{
			ResolveButtonVisualProperty(
				visuals.states,
				state,
				&Visual::transform
			).value_or(Transform{})
		};
		const bool had_override{ visual.transform.has_value() };
		bool changed{ false };

		DrawDisabledWrappedText(
			PrettyName(magic_enum::enum_name(state)) + " " +
			std::string{ part_label } +
			" transform. Unset values inherit from fallback states."
		);

		changed |= DrawPropertyRow(
			"Position",
			[&]() {
				const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
				const float pick_width{
					ImGui::CalcTextSize("Pick").x +
					ImGui::GetStyle().FramePadding.x * 2.0f
				};
				const float available{ ImGui::GetContentRegionAvail().x };
				const float field_width{
					std::max(36.0f, (available - pick_width - spacing * 2.0f) * 0.5f)
				};
				bool local_changed{ false };

				ImGui::SetNextItemWidth(field_width);
				local_changed |= ImGui::DragFloat(
					"##X",
					&transform.position.x,
					1.0f,
					0.0f,
					0.0f,
					"X: %.0f"
				);
				ImGui::SameLine(0.0f, spacing);
				ImGui::SetNextItemWidth(field_width);
				local_changed |= ImGui::DragFloat(
					"##Y",
					&transform.position.y,
					1.0f,
					0.0f,
					0.0f,
					"Y: %.0f"
				);
				ImGui::SameLine(0.0f, spacing);

				auto apply{ target.template MakeApply<Visuals>(callback) };
				Visuals snapshot{ visuals };

				DrawPositionPickButton(
					target.ctx,
					"ButtonVisualStatePosition",
					transform.position,
					MakeButtonChildStatePositionConverter<Visuals, Visual>(
						child_info.child,
						child_info.button,
						state,
						fallback_anchor
					),
					[apply, snapshot = std::move(snapshot), index, state](
						V2_float picked
					) mutable {
						auto& selected{ snapshot.states[index] };
						Transform updated{
							ResolveButtonVisualProperty(
								snapshot.states,
								state,
								&Visual::transform
							).value_or(Transform{})
						};
						updated.position = picked;
						selected.defined = true;
						selected.transform = updated;
						apply(ComponentState<Visuals>{ snapshot });
					},
					GetButtonChildStateWorldPosition<Visuals, Visual>(
						child_info.child,
						child_info.button,
						state,
						fallback_anchor,
						transform.position
					),
					true
				);

				return local_changed;
			}
		);
		changed |= DrawValue(
			target.ctx,
			"Rotation",
			transform.rotation,
			FieldOptions{
				.speed = 1.0f,
				.min = 0.0,
				.max = 360.0,
				.format = "%.1f deg",
				.flags = ImGuiSliderFlags_AlwaysClamp,
			}
		);
		changed |= DrawValue(
			target.ctx,
			"Scale",
			transform.scale,
			FieldOptions{
				.speed = 0.01f,
				.format = "%.3f",
			}
		);

		if (had_override) {
			if (ImGui::Button("Use Inherited Transform", ImVec2{ -FLT_MIN, 0.0f })) {
				visual.transform.reset();
				visual.defined = HasButtonVisualOverrides(visual);
				changed = true;
			}
		}

		if (changed && (!had_override || visual.transform.has_value())) {
			transform.ClampScale();
			visual.defined = true;
			visual.transform = transform;
		}

		if (changed) {
			target.template SetLive<Visuals>(
				ComponentState<Visuals>{ visuals },
				callback
			);
		}

		auto after{ target.template Capture<Visuals>() };
		TrackComponentState(
			target,
			std::string{ "Edit " } + std::string{ part_label } + " State Transform",
			std::move(before),
			std::move(after),
			changed,
			callback
		);

		return changed;
	}
}

template <typename Target>
bool DrawButtonChildStateTransformFeature(
	Target& target,
	const ButtonChildInfo& child_info,
	ButtonVisualState state
) {
	switch (child_info.part) {
		case ButtonChildPart::Background:
			return DrawButtonChildStateTransformComponent<
				Target,
				ButtonBackgroundVisuals,
				ButtonShapeVisual
			>(
				target,
				child_info,
				state,
				child_info.button.GetOrDefault<Origin>(),
				"Button Background",
				&MarkButtonBackgroundDirty
			);
		case ButtonChildPart::Border:
			return DrawButtonChildStateTransformComponent<
				Target,
				ButtonBorderVisuals,
				ButtonShapeVisual
			>(
				target,
				child_info,
				state,
				child_info.button.GetOrDefault<Origin>(),
				"Button Border",
				&MarkButtonBorderDirty
			);
		case ButtonChildPart::Text:
			return DrawButtonChildStateTransformComponent<
				Target,
				ButtonTextVisuals,
				ButtonTextVisual
			>(
				target,
				child_info,
				state,
				Origin::Center,
				"Button Text",
				&MarkButtonTextDirty
			);
		case ButtonChildPart::Sprite:
			return DrawButtonChildStateTransformComponent<
				Target,
				ButtonSpriteVisuals,
				ButtonSpriteVisual
			>(
				target,
				child_info,
				state,
				child_info.button.GetOrDefault<Origin>(),
				"Button Sprite",
				&MarkButtonSpriteDirty
			);
	}

	return false;
}

template <typename Target>
bool DrawTransformFeature(Target& target) {
	if (const auto child_info{ GetButtonChildInfo(target) }) {
		if (const auto state{ GetButtonVisualEditState(target) }) {
			return DrawButtonChildStateTransformFeature(
				target,
				*child_info,
				*state
			);
		}
	}

	if (!HasTransformFeature(target)) {
		return false;
	}

	const auto header{ DrawFeatureHeader(
		target,
		InspectorFeature::Transform,
		"Transform",
		ImGuiTreeNodeFlags_DefaultOpen,
		TransformFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;

	const auto before{ CaptureTransformFeature(target) };
	auto state{ before };
	auto apply{ MakeTransformFeatureApply(target) };
	bool changed{ header.changed };

	changed |= DrawTransformFeatureFields(
		target,
		state,
		apply
	);

	if (changed) {
		state.ignore_transform = false;
		state.transform.ClampScale();
		ApplyButtonVisualTransformDelta(
			state,
			GetButtonVisualEditState(target),
			before.transform
		);
		SetTransformFeatureLive(target, state);
	}

	if (changed) {
		ScopedID target_scope{ target.Id() };
		const ImGuiID key{ ImGui::GetID("##TransformFeature") };

		TrackUndoableInteraction(
			target.ctx,
			key,
			"Edit Transform",
			true,
			[apply, before]() mutable {
				apply(before);
			},
			[apply, state]() mutable {
				apply(state);
			}
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
	return requires(Target target) {
		target.entity;
	};
}

template <
	typename Target,
	typename Component,
	typename Locator,
	typename Callback
>
bool DrawPickableLocalPosition(
	Target& target,
	Component& component,
	std::string_view label,
	Locator locator,
	Callback callback,
	bool* remove_requested = nullptr
) {
	V2_float& position{ locator(component) };

	const bool changed{ DrawPropertyRow(label, [&]() {
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float pick_width{
			ImGui::CalcTextSize("Pick").x +
			ImGui::GetStyle().FramePadding.x * 2.0f
		};
		const float remove_width{ remove_requested ? ImGui::GetFrameHeight() : 0.0f };
		const float remove_spacing{ remove_requested ? spacing : 0.0f };
		const float available{ ImGui::GetContentRegionAvail().x };
		const float field_width{
			std::max(
				36.0f,
				(available - pick_width - remove_width - remove_spacing - spacing * 2.0f) * 0.5f
			)
		};

		bool local_changed{ false };

		ImGui::SetNextItemWidth(field_width);
		local_changed |= ImGui::DragFloat(
			"##X",
			&position.x,
			kInspectorPositionDragSpeed,
			0.0f,
			0.0f,
			"X: %.2f"
		);

		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(field_width);
		local_changed |= ImGui::DragFloat(
			"##Y",
			&position.y,
			kInspectorPositionDragSpeed,
			0.0f,
			0.0f,
			"Y: %.2f"
		);

		ImGui::SameLine(0.0f, spacing);

		{
			ScopedDisabled disabled{ !CanPickLocalPosition<Target>() };

			auto apply{ target.template MakeApply<Component>(callback) };
			Component snapshot{ component };

			DrawPositionPickButton(
				target.ctx,
				label,
				position,
				MakeLocalPositionConverter(target),
				[
					apply,
					snapshot = std::move(snapshot),
					locator
				](V2_float picked) mutable {
					locator(snapshot) = picked;
					apply(ComponentState<Component>{ snapshot });
				},
				GetTargetWorldReferencePosition(target, position),
				true
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

template <
	typename Target,
	typename Component,
	typename Value,
	typename Locator,
	typename Callback
>
bool DrawGeometryValue(
	Target& target,
	Component& component,
	Value& value,
	std::string_view label,
	Locator locator,
	Callback callback
);

template <
	std::size_t I,
	typename Target,
	typename Component,
	typename Parent,
	typename Locator,
	typename Callback
>
bool DrawReflectedGeometryMember(
	Target& target,
	Component& component,
	Parent& parent,
	Locator locator,
	Callback callback
) {
	auto members{ ReflectMembers(parent) };
	auto& member{ std::get<I>(members) };

	auto member_locator = [locator](Component& root) -> decltype(auto) {
		auto reflected{ ReflectMembers(locator(root)) };
		return std::get<I>(reflected).value;
	};

	return DrawGeometryValue(
		target,
		component,
		member.value,
		PrettyName(member.name),
		member_locator,
		callback
	);
}

template <
	typename Target,
	typename Component,
	typename Parent,
	typename Locator,
	typename Callback,
	std::size_t... I
>
bool DrawReflectedGeometryMembers(
	Target& target,
	Component& component,
	Parent& parent,
	Locator locator,
	Callback callback,
	std::index_sequence<I...>
) {
	bool changed{ false };
	((changed |= DrawReflectedGeometryMember<I>(
		target,
		component,
		parent,
		locator,
		callback
	)), ...);
	return changed;
}

template <
	typename Target,
	typename Component,
	typename Variant,
	typename Locator,
	typename Callback,
	std::size_t... I
>
bool DrawGeometryVariant(
	Target& target,
	Component& component,
	Variant& value,
	std::string_view label,
	Locator locator,
	Callback callback,
	std::index_sequence<I...>
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

				const std::string option{
					std::same_as<Alternative, std::monostate>
						? "None"
						: VariantTypeLabel<Alternative>()
				};

				if constexpr (std::default_initializable<Alternative>) {
					const bool selected{ value.index() == Index };

					if (ImGui::Selectable(option.c_str(), selected) && !selected) {
						requested_index = Index;
						local_changed = true;
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
				target,
				component,
				std::get<Index>(value),
				VariantTypeLabel<Alternative>(),
				alternative_locator,
				callback
			);
		}
	};

	(draw_selected.template operator()<I>(), ...);
	return changed;
}


template <typename Mask>
	requires std::integral<Mask>
bool DrawColliderMaskList(
	std::vector<Mask>& masks
) {
	bool changed{ false };
	std::optional<std::size_t> remove_index;
	std::optional<std::pair<std::size_t, std::size_t>> move;

	ImGui::SeparatorText(
		"Collides with Masks"
	);

	if (
		ImGui::Button(
			"+ Mask",
			ImVec2{ -FLT_MIN, 0.0f }
		)
	) {
		masks.emplace_back();
		changed = true;
	}

	for (
		std::size_t index{ 0 };
		index < masks.size();
		++index
	) {
		ScopedID item_scope{
			static_cast<int>(index)
		};

		int displayed{ 0 };
		if constexpr (std::signed_integral<Mask>) {
			displayed = static_cast<int>(
				std::clamp<long long>(
					static_cast<long long>(
						masks[index]
					),
					0,
					64
				)
			);
		} else {
			displayed = static_cast<int>(
				std::min<std::uint64_t>(
					static_cast<std::uint64_t>(
						masks[index]
					),
					64
				)
			);
		}

		const std::string label{
			"Mask " +
			std::to_string(
				index + 1
			)
		};

		changed |= DrawPropertyRow(
			label,
			[&]() {
				bool local_changed{ false };
				const float spacing{
					ImGui::GetStyle().ItemInnerSpacing.x
				};
				const float button_width{
					ImGui::GetFrameHeight()
				};
				const float actions_width{
					button_width * 3.0f +
					spacing * 3.0f
				};
				const float field_width{
					std::max(
						36.0f,
						ImGui::GetContentRegionAvail().x -
							actions_width
					)
				};

				ImGui::SetNextItemWidth(
					field_width
				);
				if (
					ImGui::DragInt(
						"##value",
						&displayed,
						1.0f,
						0,
						64,
						"%d",
						ImGuiSliderFlags_AlwaysClamp
					)
				) {
					masks[index] =
						static_cast<Mask>(
							std::clamp(
								displayed,
								0,
								64
							)
						);
					local_changed = true;
				}

				ImGui::SameLine(
					0.0f,
					spacing
				);
				{
					ScopedDisabled disabled{
						index == 0
					};
					if (
						ImGui::ArrowButton(
							"##up",
							ImGuiDir_Up
						)
					) {
						move = std::pair{
							index,
							index - 1
						};
					}
				}

				ImGui::SameLine(
					0.0f,
					spacing
				);
				{
					ScopedDisabled disabled{
						index + 1 >=
							masks.size()
					};
					if (
						ImGui::ArrowButton(
							"##down",
							ImGuiDir_Down
						)
					) {
						move = std::pair{
							index,
							index + 1
						};
					}
				}

				ImGui::SameLine(
					0.0f,
					spacing
				);
				if (
					ImGui::Button(
						"X##remove",
						ImVec2{
							button_width,
							button_width
						}
					)
				) {
					remove_index = index;
				}

				return local_changed;
			}
		);
	}

	if (move) {
		std::ranges::iter_swap(
			masks.begin() +
				static_cast<std::ptrdiff_t>(
					move->first
				),
			masks.begin() +
				static_cast<std::ptrdiff_t>(
					move->second
				)
		);
		changed = true;
	} else if (remove_index) {
		masks.erase(
			masks.begin() +
				static_cast<std::ptrdiff_t>(
					*remove_index
				)
		);
		changed = true;
	}

	return changed;
}

template <
	typename Target,
	typename Component,
	typename Value,
	typename Locator,
	typename Callback
>
bool DrawGeometryValue(
	Target& target,
	Component& component,
	Value& value,
	std::string_view label,
	Locator locator,
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
			return DrawPickableLocalPosition(
				target,
				component,
				label,
				locator,
				callback
			);
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
			changed = true;
		}

		auto min_locator = [locator](Component& root) -> V2_float& {
			return locator(root).min;
		};
		auto max_locator = [locator](Component& root) -> V2_float& {
			return locator(root).max;
		};

		changed |= DrawPickableLocalPosition(
			target,
			component,
			"Min",
			min_locator,
			callback
		);
		changed |= DrawPickableLocalPosition(
			target,
			component,
			"Max",
			max_locator,
			callback
		);

		return changed;
	} else if constexpr (kIsVector<Type>) {
		if constexpr (
			std::same_as<
				std::remove_cvref_t<Component>,
				Collider
			> &&
			std::integral<typename Type::value_type>
		) {
			const std::string normalized{
				NormalizeFeatureName(label)
			};

			if (normalized.contains("collideswith")) {
				return DrawColliderMaskList(
					value
				);
			}

			return DrawValue(
				target.ctx,
				label,
				value
			);
		} else if constexpr (std::same_as<typename Type::value_type, V2_float>) {
			bool changed{ false };
			std::optional<std::size_t> remove;

			if (ImGui::Button("+ Vertex", ImVec2{ -FLT_MIN, 0.0f })) {
				value.push_back(
					value.empty()
						? V2_float{}
						: value.back()
				);
				changed = true;
			}

			for (std::size_t index{ 0 }; index < value.size(); ++index) {
				ScopedID vertex_scope{ static_cast<int>(index) };
				auto vertex_locator = [locator, index](Component& root) -> V2_float& {
					return locator(root)[index];
				};
				bool remove_vertex{ false };

				changed |= DrawPickableLocalPosition(
					target,
					component,
					std::string{ "Vertex " } + std::to_string(index + 1),
					vertex_locator,
					callback,
					&remove_vertex
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
					target,
					component,
					std::string{ "Vertex " } + std::to_string(index + 1),
					vertex_locator,
					callback
				);
			}

			return changed;
		} else {
			return DrawValue(target.ctx, label, value);
		}
	} else if constexpr (kIsVariant<Type>) {
		return DrawGeometryVariant(
			target,
			component,
			value,
			label,
			locator,
			callback,
			std::make_index_sequence<std::variant_size_v<Type>>{}
		);
	} else if constexpr (std::integral<Type>) {
		const std::string normalized{
			NormalizeFeatureName(label)
		};

		if constexpr (
			std::same_as<
				std::remove_cvref_t<Component>,
				Collider
			>
		) {
			if (normalized == "mask") {
				return DrawValue(
					target.ctx,
					label,
					value,
					FieldOptions{
						.speed = 1.0f,
						.min = 0.0f,
						.max = 64.0f,
						.flags = ImGuiSliderFlags_AlwaysClamp,
					}
				);
			}
		}

		return DrawValue(
			target.ctx,
			label,
			value
		);
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
			target,
			component,
			reflected.value,
			label,
			value_locator,
			callback
		);
	} else if constexpr (ReflectedMembers<Type>) {
		auto members{ ReflectMembers(value) };

		return DrawReflectedGeometryMembers(
			target,
			component,
			value,
			locator,
			callback,
			std::make_index_sequence<std::tuple_size_v<decltype(members)>>{}
		);
	} else {
		return DrawValue(target.ctx, label, value);
	}
}

template <
	typename Target,
	typename Component,
	typename Callback = std::nullptr_t
>
bool DrawGeometryComponent(
	Target& target,
	Component& component,
	Callback callback = nullptr
) {
	auto root_locator = [](Component& value) -> Component& {
		return value;
	};

	return DrawGeometryValue(
		target,
		component,
		component,
		TypeLabel<Component>(),
		root_locator,
		callback
	);
}

template <typename Target, typename T, typename Draw, typename Callback = std::nullptr_t>
bool DrawRequiredInlineVisualComponent(
	Target& target,
	std::string_view label,
	Draw&& draw,
	Callback callback = nullptr
) {
	return DrawRequiredComponent<Target, T>(
		target,
		label,
		false,
		std::forward<Draw>(draw),
		callback
	);
}


template <typename Target, typename T, typename Draw, typename Callback = std::nullptr_t>
bool DrawRequiredInlineVisualComponentWithDefault(
	Target& target,
	std::string_view label,
	T default_value,
	Draw&& draw,
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
		target,
		std::string{ "Edit " } + std::string{ label },
		std::move(before),
		std::move(after),
		changed,
		callback
	);

	return changed;
}

template <typename Target, typename T>
bool DrawOptionalVisualComponent(
	Target& target,
	std::string_view label,
	bool tree = false,
	bool contents_read_only = false,
	bool toggle_read_only = false
) {
	if constexpr (std::is_empty_v<T>) {
		return DrawOptionalComponent<Target, T>(
			target,
			label,
			false,
			[](T&) {
				return false;
			},
			contents_read_only,
			toggle_read_only
		);
	} else {
		return DrawOptionalComponent<Target, T>(
			target,
			label,
			tree,
			[&target](T& value) {
				return DrawRegisteredComponentContents(
				target.ctx, Hash<T>(), std::addressof(value)
			);
			},
			contents_read_only,
			toggle_read_only
		);
	}
}

[[nodiscard]] bool IsShapeRenderer(std::string_view visual) {
	return visual == "rect" ||
		   visual == "circle" ||
		   visual == "roundedrect" ||
		   visual == "polygon" ||
		   visual == "ellipse" ||
		   visual == "triangle" ||
		   visual == "line" ||
		   visual == "capsule" ||
		   visual == "arc";
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
			{ 0.0f, -50.0f },
			{ 47.0f, -15.0f },
			{ 29.0f, 40.0f },
			{ -29.0f, 40.0f },
			{ -47.0f, -15.0f },
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
	::ptgn::impl::GraphicsData, ::ptgn::Material, ShaderKey,
	::ptgn::impl::RenderTargetDesc, ::ptgn::impl::EffectTag,
	::ptgn::impl::HDREffectTag, EffectMargin, Bloom, Blur, GaussianBlur,
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
				...
			);
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
		SetRendererOwnedComponent<TextureKey>(target);
		SetRendererOwnedComponent<SpriteStackData>(target);
	} else if (visual.contains("sprite")) {
		SetRendererOwnedComponent<TextureKey>(target);
	} else if (visual.contains("text")) {
		SetRendererOwnedComponent<::ptgn::impl::TextData>(
			target,
			MakeDefaultTextRendererData()
		);
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
void ApplyRendererSelection(
	Target& target,
	ComponentState<::ptgn::impl::IDrawable> drawable
) {
	ClearRendererOwnedComponents(target, RendererOwnedComponents{});
	target.template SetLive<::ptgn::impl::IDrawable>(drawable);

	if (!drawable) {
		return;
	}

	const auto* info{
		::ptgn::impl::IDrawable::FindInfo(drawable->hash)
	};

	if (!info) {
		return;
	}

	InitializeRendererOwnedComponents(
		target,
		NormalizeFeatureName(info->GetDisplayName())
	);
}

struct RendererRowResult {
	std::string visual;
	bool changed{ false };
};

template <typename Target>
RendererRowResult DrawRendererRow(Target& target) {
	using Drawable = ::ptgn::impl::IDrawable;

	const bool primary_scene_target{
		IsPrimarySceneRenderTarget(target)
	};

	auto drawable{ target.template Capture<Drawable>() };
	auto before_visible{ target.template Capture<Visible>() };

	bool visible{
		before_visible
			? before_visible->visible
			: true
	};

	const auto* info{
		drawable
			? Drawable::FindInfo(drawable->hash)
			: nullptr
	};

	const std::string preview{
		primary_scene_target
			? "Render Target"
			: info
				? std::string{ info->GetDisplayName() }
				: "None"
	};

	const float start_x{ ImGui::GetCursorPosX() };
	const float checkbox_slot{
		ImGui::GetFrameHeight() +
		ImGui::GetStyle().ItemSpacing.x
	};

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Renderer");
	MeasurePropertyLabel("Renderer", start_x + checkbox_slot);
	ImGui::SameLine();
	ImGui::SetCursorPosX(GetPropertyValueX(start_x));

	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float visible_width{
		ImGui::GetFrameHeight() +
		ImGui::GetStyle().ItemInnerSpacing.x +
		ImGui::CalcTextSize("Visible").x
	};
	const float combo_width{
		std::max(
			80.0f,
			ImGui::GetContentRegionAvail().x -
				visible_width -
				spacing
		)
	};

	bool renderer_changed{ false };
	auto before_renderer{
		CaptureInspectorFeatureState(
			target,
			InspectorFeature::Visual,
			VisualFeatureComponents{}
		)
	};

	auto choose_renderer = [&](ComponentState<Drawable> selected) {
		const bool same_renderer{
			selected.has_value() == drawable.has_value() &&
			(!selected || selected->hash == drawable->hash)
		};

		if (same_renderer) {
			return;
		}

		ApplyRendererSelection(target, selected);

		if (!selected) {
			SetFeatureManuallyAdded(
				target.GetFeatureTargetKey(),
				InspectorFeature::Visual,
				true
			);
		}

		drawable = target.template Capture<Drawable>();
		renderer_changed = true;
	};

	ImGui::SetNextItemWidth(combo_width);

	std::size_t renderer_popup_rows{ 1 };
	bool has_shapes{ false };
	bool has_effects{ false };
	for (const auto& candidate : Drawable::data()) {
		const std::string visual{
			NormalizeFeatureName(candidate.GetDisplayName())
		};
		if (IsShapeRenderer(visual)) {
			has_shapes = true;
		} else if (IsEffectRenderer(visual)) {
			has_effects = true;
		} else {
			++renderer_popup_rows;
		}
	}
	renderer_popup_rows += static_cast<std::size_t>(has_shapes);
	renderer_popup_rows += static_cast<std::size_t>(has_effects);

	const auto* viewport{ ImGui::GetMainViewport() };
	const float requested_popup_height{
		ImGui::GetStyle().WindowPadding.y * 2.0f +
		static_cast<float>(renderer_popup_rows) * ImGui::GetFrameHeightWithSpacing()
	};
	const float popup_height{
		std::min(
			requested_popup_height,
			std::max(120.0f, viewport->WorkSize.y - 32.0f)
		)
	};
	ImGui::SetNextWindowSizeConstraints(
		ImVec2{ 0.0f, popup_height },
		ImVec2{ FLT_MAX, popup_height }
	);

	ImGui::BeginDisabled(primary_scene_target);

	if (ImGui::BeginCombo(
		"##RendererSelector",
		preview.c_str(),
		ImGuiComboFlags_HeightLargest
	)) {
		if (!primary_scene_target) {
			if (ImGui::Selectable(
					"None",
					!drawable.has_value()
				)) {
				choose_renderer(std::nullopt);
			}

			auto draw_candidate = [&](const auto& candidate) {
				const std::string label{
					candidate.GetDisplayName()
				};

				const bool selected{
					drawable &&
					drawable->hash == candidate.hash
				};

				if (ImGui::Selectable(
						label.c_str(),
						selected
					)) {
					choose_renderer(
						Drawable{ candidate.hash }
					);
				}

				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			};

			for (const auto& candidate : Drawable::data()) {
				const std::string visual{
					NormalizeFeatureName(candidate.GetDisplayName())
				};

				if (!IsShapeRenderer(visual) &&
					!IsEffectRenderer(visual)) {
					draw_candidate(candidate);
				}
			}

			if (ImGui::BeginMenu("Shapes")) {
				for (const auto& candidate : Drawable::data()) {
					const std::string visual{
						NormalizeFeatureName(candidate.GetDisplayName())
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
						NormalizeFeatureName(candidate.GetDisplayName())
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

	if (primary_scene_target &&
		ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip(
			"The scene render target renderer cannot be changed."
		);
	}

	if (renderer_changed) {
		auto after_renderer{
			CaptureInspectorFeatureState(
				target,
				InspectorFeature::Visual,
				VisualFeatureComponents{}
			)
		};

		TrackInspectorFeatureState(
			target,
			InspectorFeature::Visual,
			"Change Renderer",
			std::move(before_renderer),
			std::move(after_renderer),
			VisualFeatureComponents{}
		);
	}

	bool visible_changed{ false };

	if (drawable || primary_scene_target) {
		ImGui::SameLine(0.0f, spacing);
		visible_changed = ImGui::Checkbox(
			"Visible##RendererVisible",
			&visible
		);
	}

	if (visible_changed) {
		Visible updated{
			before_visible.value_or(Visible{})
		};
		updated.visible = visible;
		target.template SetLive<Visible>(
			ComponentState<Visible>{ updated }
		);
	}

	auto after_visible{ target.template Capture<Visible>() };

	TrackComponentState(
		target,
		"Toggle Visibility",
		std::move(before_visible),
		std::move(after_visible),
		visible_changed
	);

	const auto* selected_info{
		drawable
			? Drawable::FindInfo(drawable->hash)
			: nullptr
	};

	return RendererRowResult{
		.visual =
			primary_scene_target
				? "rendertarget"
				: selected_info
					? NormalizeFeatureName(
						selected_info->GetDisplayName()
					)
					: std::string{},
		.changed = renderer_changed,
	};
}

template <typename Target>
bool DrawEffectMargin(Target& target) {
	return DrawOptionalComponent<Target, EffectMargin>(
		target,
		"Effect Margin",
		false,
		[&target](EffectMargin& margin) {
			const FieldOptions options{
				.speed = 1.0f,
				.min = 0.0,
				.max = 4096.0,
				.flags = ImGuiSliderFlags_AlwaysClamp,
			};

			return [&target, &options]<typename Margin>(Margin& value) {
				if constexpr (ReflectedValue<Margin>) {
					auto member{ ReflectValue(value) };
					return DrawValue(
						target.ctx,
						"Effect Margin",
						member.value,
						options
					);
				} else if constexpr (ReflectedMembers<Margin>) {
					bool changed{ false };
					auto members{ ReflectMembers(value) };

					std::apply(
						[&](auto&&... member) {
							(
								(
									changed |= DrawValue(
										target.ctx,
										PrettyName(member.name),
										member.value,
										options
									)
								),
								...
							);
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

[[nodiscard]] bool IsEffectRenderer(
	std::string_view visual
) {
	if (visual.empty()) {
		return false;
	}

	const bool standard_renderer{
		visual == "rect" ||
		visual == "circle" ||
		visual == "roundedrect" ||
		visual == "polygon" ||
		visual == "ellipse" ||
		visual == "triangle" ||
		visual == "line" ||
		visual == "capsule" ||
		visual == "arc" ||
		visual.contains("sprite") ||
		visual.contains("text") ||
		visual.contains("particle") ||
		visual.contains("light") ||
		visual.contains("graphics") ||
		visual.contains("customshader") ||
		visual.contains("rendertarget")
	};

	return !standard_renderer;
}

template <typename T>
bool DrawFlattenedConfig(EditorContext& ctx, T& value);

template <typename Target, typename Effect>
bool DrawSelectedEffectComponent(
	Target& target,
	std::string_view label
) {
	if constexpr (!Target::template Supports<Effect>()) {
		return false;
	} else {
		return DrawRequiredComponent<Target, Effect>(
			target,
			label,
			false,
			[&target](Effect& value) {
				return DrawFlattenedConfig(
					target.ctx,
					value
				);
			}
		);
	}
}

template <typename Target>
bool DrawVisualEffects(
	Target& target,
	std::string_view visual
) {
	bool changed{ false };

	if (visual.contains("gaussianblur")) {
		changed |= DrawSelectedEffectComponent<Target, GaussianBlur>(
			target,
			"Gaussian Blur"
		);
	} else if (visual.contains("blur")) {
		changed |= DrawSelectedEffectComponent<Target, Blur>(
			target,
			"Blur"
		);
	} else if (visual.contains("bloom")) {
		changed |= DrawSelectedEffectComponent<Target, Bloom>(
			target,
			"Bloom"
		);
	}

	if (IsEffectRenderer(visual)) {
		changed |= DrawEffectMargin(target);
	}

	return changed;
}

bool DrawTextRunFlat(
	EditorContext& ctx,
	TextRun& run
) {
	bool changed{ false };

	changed |= DrawValue(
		ctx,
		"Text",
		run.text,
		FieldOptions{
			.multiline = true,
			.line_count = 8,
			.resizable_y = true,
			.large_editor = true,
		}
	);
	changed |= DrawValue(ctx, "Font", run.font);
	changed |= DrawValue(ctx, "Color", run.style.color);
	changed |= DrawValue(ctx, "Size", run.style.size);

	if (ImGui::TreeNodeEx(
			"Style##TextRunStyle",
			ImGuiTreeNodeFlags_SpanAvailWidth
		)) {
		{
			ScopedUnindent align_with_style;
			changed |= DrawValue(ctx, "Bold Weight", run.style.bold_weight);
			changed |= DrawValue(ctx, "Kerning", run.style.kerning);
			changed |= DrawValue(ctx, "Tracking", run.style.tracking);
			changed |= DrawValue(ctx, "Line Spacing", run.style.line_spacing);
			changed |= DrawValue(ctx, "Flags", run.style.flags);
			changed |= DrawValue(ctx, "Distance Field", run.style.sdf);
			changed |= DrawValue(ctx, "Effect", run.style.effect);
		}

		ImGui::TreePop();
	}

	return changed;
}

bool DrawTextRunsFlat(
	EditorContext& ctx,
	StyledText& text
) {
	bool changed{ false };

	if (text.runs.empty()) {
		text.runs.emplace_back();
		changed = true;
	}

	ImGui::SeparatorText("Content");

	changed |= DrawVectorEditor(
		text.runs,
		VectorOptions{
			.item_name = "Text Run",
			.add_label = "+ Text Run",
			.default_open = true,
			.reorderable = true,
			.add_first = true,
			.reset_last_on_remove = true,
			.minimum_items = 0,
		},
		[&ctx](TextRun& run, std::size_t) {
			return DrawTextRunFlat(
				ctx,
				run
			);
		}
	);

	return changed;
}


inline bool IsTextWrapSettingName(std::string_view normalized) {
	return normalized == "allowwordbreakinoverflow" ||
		   normalized == "inserthyphenonsplit" ||
		   normalized == "preventsinglelettersplit" ||
		   normalized == "requirethreeletterremainder";
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
		ForEachTextWrapMode(
			wrap,
			plain_mode_name,
			[&](auto& mode) {
				if (!drawn) {
					changed |= DrawValue(ctx, "Wrap Mode", mode);
					drawn = true;
				}
			}
		);
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
		ForEachTextWrapSetting(
			wrap,
			inside_wrap,
			[&](bool& value, std::string_view normalized) {
				if (value) {
					selected.push_back(TextWrapSettingLabel(normalized));
				}
			}
		);

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

		return DrawPropertyRow(
			"Wrap Settings",
			[&]() {
				bool changed{ false };
				if (ImGui::BeginCombo("##WrapSettings", preview.c_str())) {
					ForEachTextWrapSetting(
						wrap,
						inside_wrap,
						[&](bool& value, std::string_view normalized) {
							const std::string item_label{ TextWrapSettingLabel(normalized) };
							if (ImGui::Selectable(
									item_label.c_str(), value,
									ImGuiSelectableFlags_DontClosePopups
								)) {
								value = !value;
								changed = true;
							}
							DrawTooltip(TextWrapSettingTooltip(normalized));
						}
					);
					ImGui::EndCombo();
				}
				return changed;
			}
		);
	}
}

template <typename Alignment>
bool DrawTextAlignment(
	EditorContext& ctx,
	Alignment& alignment
) {
	if constexpr (!ReflectedMembers<Alignment>) {
		return DrawValue(
			ctx,
			"Alignment",
			alignment
		);
	} else {
		bool changed{ false };
		auto members{
			ReflectMembers(
				alignment
			)
		};

		auto draw_member = [&](auto&& member) {
			const std::string normalized{
				NormalizeFeatureName(
					member.name
				)
			};

			if (normalized.contains("horizontal")) {
				changed |= DrawValue(
					ctx,
					"Horizontal Align",
					member.value
				);
			} else if (normalized.contains("vertical")) {
				changed |= DrawValue(
					ctx,
					"Vertical Align",
					member.value
				);
			} else {
				changed |= DrawValue(
					ctx,
					PrettyName(member.name),
					member.value
				);
			}
		};

		std::apply(
			[&](auto&&... member) {
				(draw_member(member), ...);
			},
			members
		);

		return changed;
	}
}

template <typename Shrink>
bool DrawTextShrinkScale(
	EditorContext& ctx,
	Shrink& shrink
) {
	if constexpr (!ReflectedMembers<Shrink>) {
		return DrawValue(
			ctx,
			"Shrink Scale",
			shrink
		);
	} else {
		float* minimum{ nullptr };
		float* maximum{ nullptr };
		auto members{
			ReflectMembers(
				shrink
			)
		};

		auto find_bound = [&](auto&& member) {
			using Member = std::remove_cvref_t<
				decltype(member.value)
			>;

			if constexpr (std::same_as<Member, float>) {
				const std::string normalized{
					NormalizeFeatureName(
						member.name
					)
				};

				if (normalized == "min") {
					minimum = std::addressof(
						member.value
					);
				} else if (normalized == "max") {
					maximum = std::addressof(
						member.value
					);
				}
			}
		};

		std::apply(
			[&](auto&&... member) {
				(find_bound(member), ...);
			},
			members
		);

		if (!minimum || !maximum) {
			return DrawDefaultContents(
				ctx,
				shrink
			);
		}

		bool changed{ false };

		const float normalized_minimum{
			std::max(
				0.0f,
				*minimum
			)
		};
		const float normalized_maximum{
			std::max(
				normalized_minimum,
				*maximum
			)
		};

		if (
			*minimum != normalized_minimum ||
			*maximum != normalized_maximum
		) {
			*minimum = normalized_minimum;
			*maximum = normalized_maximum;
			changed = true;
		}

		if (
			DrawValue(
				ctx,
				"Min Scale",
				*minimum,
				FieldOptions{
					.speed = 0.01f,
					.format = "%.3f",
				}
			)
		) {
			*minimum = std::clamp(
				*minimum,
				0.0f,
				*maximum
			);
			changed = true;
		}

		if (
			DrawValue(
				ctx,
				"Max Scale",
				*maximum,
				FieldOptions{
					.speed = 0.01f,
					.format = "%.3f",
				}
			)
		) {
			*maximum = std::max(
				std::max(
					0.0f,
					*maximum
				),
				*minimum
			);
			changed = true;
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
			using Member = std::remove_cvref_t<decltype(member.value)>;
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
					changed |= DrawTextWrapSettings(ctx, style, false);
					direct_wrap_settings_drawn = true;
				}
				return;
			}
			if (normalized == "shrinkscale") {
				const bool open{ ImGui::TreeNodeEx(
					"Shrink to Fit##TextShrinkToFit",
					ImGuiTreeNodeFlags_SpanAvailWidth
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

template <
	std::size_t I,
	typename Target,
	typename TextData
>
bool DrawTextBoxMember(
	Target& target,
	TextData& text_data,
	auto& box
) {
	using Box = std::remove_cvref_t<decltype(box)>;
	bool changed{ false };
	auto& editor_state{
		GetManualFeatureState(
			target.GetFeatureTargetKey()
		)
	};

	bool box_has_non_default_data{ false };

	if constexpr (
		JsonSerializable<Box> &&
		std::default_initializable<Box>
	) {
		json current = box;
		json defaults = Box{};
		box_has_non_default_data = current != defaults;
	}

	if (!editor_state.text_box_state_initialized) {
		editor_state.text_box_enabled =
			box_has_non_default_data;
		editor_state.text_box_state_initialized = true;
	} else if (box_has_non_default_data) {
		editor_state.text_box_enabled = true;
	}

	bool enabled{
		editor_state.text_box_enabled
	};

	ScopedID box_scope{ "TextBox" };

	if (ImGui::Checkbox(
			"##Enabled",
			&enabled
		)) {
		editor_state.text_box_enabled = enabled;

		if (!enabled) {
			box = Box{};
		}

		changed = true;
	}

	ImGui::SameLine();

	const bool open{
		ImGui::TreeNodeEx(
			"Text Box##Tree",
			ImGuiTreeNodeFlags_SpanAvailWidth
		)
	};

	if (!open) {
		return changed;
	}

	ScopedIndent indent;
	ScopedPropertyLabelOffset box_label_offset{
		ImGui::GetStyle().IndentSpacing
	};
	ScopedDisabled disabled{ !enabled };
	auto box_members{ ReflectMembers(box) };

	auto draw_box_member = [&](auto&& box_member) {
		const std::string box_name{
			NormalizeFeatureName(box_member.name)
		};

		if (box_name == "rect") {
			using BoxMember =
				std::remove_cvref_t<
					decltype(box_member.value)
				>;

			if constexpr (std::same_as<BoxMember, Rect>) {
				auto box_locator =
					[](TextData& value) -> Box& {
						auto reflected{
							ReflectMembers(value)
						};
						return std::get<I>(
							reflected
						).value;
					};

				auto rect_locator =
					[box_locator](TextData& value) -> Rect& {
						auto reflected{
							ReflectMembers(
								box_locator(value)
							)
						};
						constexpr std::size_t count{
							std::tuple_size_v<
								decltype(reflected)
							>
						};
						Rect* result{ nullptr };

						[&]<std::size_t... Index>(
							std::index_sequence<Index...>
						) {
							(
								[&] {
									auto& candidate{
										std::get<Index>(
											reflected
										)
									};

									if constexpr (
										std::same_as<
											std::remove_cvref_t<
												decltype(
													candidate.value
												)
											>,
											Rect
										>
									) {
										if (
											NormalizeFeatureName(
												candidate.name
											) == "rect"
										) {
											result =
												std::addressof(
													candidate.value
												);
										}
									}
								}(),
								...
							);
						}(
							std::make_index_sequence<
								count
							>{}
						);

						return *result;
					};

				changed |= DrawGeometryValue(
					target,
					text_data,
					box_member.value,
					"Rect",
					rect_locator,
					&MarkTextLayoutDirty
				);
			} else {
				changed |= DrawValue(
					target.ctx,
					"Rect",
					box_member.value
				);
			}

			return;
		}

		if (box_name == "style") {
			const bool style_open{
				ImGui::TreeNodeEx(
					"Additional Options##TextBoxAdditionalOptions",
					ImGuiTreeNodeFlags_SpanAvailWidth
				)
			};

			if (style_open) {
				changed |= DrawTextBoxAdditionalStyle(
					target.ctx,
					box_member.value
				);

				ImGui::TreePop();
			}

			return;
		}

		changed |= DrawValue(
			target.ctx,
			PrettyName(box_member.name),
			box_member.value
		);
	};

	std::apply(
		[&](auto&&... box_member) {
			(draw_box_member(box_member), ...);
		},
		box_members
	);

	ImGui::TreePop();
	return changed;
}

template <
	std::size_t I,
	typename Target,
	typename TextData
>
bool DrawTextPrimaryMember(
	Target& target,
	TextData& text_data
) {
	auto members{ ReflectMembers(text_data) };
	auto& member{ std::get<I>(members) };
	const std::string normalized{
		NormalizeFeatureName(member.name)
	};

	if (
		normalized == "text" ||
		normalized == "content"
	) {
		using Member =
			std::remove_cvref_t<
				decltype(member.value)
			>;

		if constexpr (std::same_as<Member, StyledText>) {
			return DrawTextRunsFlat(
				target.ctx,
				member.value
			);
		} else {
			return DrawValue(
				target.ctx,
				"Content",
				member.value
			);
		}
	}

	if (
		normalized.contains("glyph") ||
		normalized.contains("clip") ||
		normalized.contains("currentrun")
	) {
		return false;
	}

	if (normalized == "box") {
		using Box =
			std::remove_cvref_t<
				decltype(member.value)
			>;

		if constexpr (ReflectedMembers<Box>) {
			return DrawTextBoxMember<I>(
				target,
				text_data,
				member.value
			);
		}
	}

	return DrawValue(
		target.ctx,
		PrettyName(member.name),
		member.value
	);
}

template <typename Target, typename TextData, std::size_t... I>
bool DrawTextPrimaryMembers(
	Target& target,
	TextData& text_data,
	std::index_sequence<I...>
) {
	bool changed{ false };
	((changed |= DrawTextPrimaryMember<I>(
		target,
		text_data
	)), ...);
	return changed;
}

template <typename Target>
bool DrawTextPrimary(
	Target& target,
	::ptgn::impl::TextData& text_data
) {
	auto members{ ReflectMembers(text_data) };
	return DrawTextPrimaryMembers(
		target,
		text_data,
		std::make_index_sequence<
			std::tuple_size_v<
				decltype(members)
			>
		>{}
	);
}



template <
	std::size_t OuterIndex,
	std::size_t InnerIndex,
	typename Target,
	typename Clip
>
bool DrawTextClipMember(
	Target& target,
	::ptgn::impl::TextData& text_data,
	Clip& clip
) {
	auto members{
		ReflectMembers(
			clip
		)
	};
	auto& member{
		std::get<InnerIndex>(
			members
		)
	};
	using Member = std::remove_cvref_t<
		decltype(member.value)
	>;
	const std::string normalized{
		NormalizeFeatureName(
			member.name
		)
	};

	if constexpr (std::same_as<Member, Rect>) {
		if (normalized.contains("rect")) {
			auto locator = [](auto& root) -> Rect& {
				auto outer{
					ReflectMembers(
						root
					)
				};
				auto& clip_value{
					std::get<OuterIndex>(
						outer
					).value
				};

				if constexpr (
					kIsOptional<
						std::remove_cvref_t<
							decltype(clip_value)
						>
					>
				) {
					auto inner{
						ReflectMembers(
							*clip_value
						)
					};
					return std::get<InnerIndex>(
						inner
					).value;
				} else {
					auto inner{
						ReflectMembers(
							clip_value
						)
					};
					return std::get<InnerIndex>(
						inner
					).value;
				}
			};

			return DrawGeometryValue(
				target,
				text_data,
				member.value,
				"Clip Rect",
				locator,
				&MarkTextLayoutDirty
			);
		}
	}

	return DrawValue(
		target.ctx,
		PrettyName(member.name),
		member.value
	);
}

template <
	std::size_t OuterIndex,
	typename Target,
	typename Clip,
	std::size_t... InnerIndex
>
bool DrawTextClipMembers(
	Target& target,
	::ptgn::impl::TextData& text_data,
	Clip& clip,
	std::index_sequence<InnerIndex...>
) {
	bool changed{ false };
	(
		(
			changed |= DrawTextClipMember<
				OuterIndex,
				InnerIndex
			>(
				target,
				text_data,
				clip
			)
		),
		...
	);
	return changed;
}

template <
	std::size_t I,
	typename Target
>
bool DrawTextAdditionalMember(
	Target& target,
	::ptgn::impl::TextData& text_data
) {
	auto members{ ReflectMembers(text_data) };
	auto& member{ std::get<I>(members) };
	const std::string normalized{
		NormalizeFeatureName(
			member.name
		)
	};

	if (
		!normalized.contains("glyph") &&
		!normalized.contains("clip")
	) {
		return false;
	}

	using Member = std::remove_cvref_t<
		decltype(member.value)
	>;

	if (normalized.contains("clip")) {
		if constexpr (std::same_as<Member, Rect>) {
			ImGui::SeparatorText("Clip Rect");

			auto locator = [](auto& root) -> Rect& {
				auto reflected{
					ReflectMembers(root)
				};
				return std::get<I>(
					reflected
				).value;
			};

			return DrawGeometryValue(
				target,
				text_data,
				member.value,
				"Clip Rect",
				locator,
				&MarkTextLayoutDirty
			);
		} else if constexpr (kIsOptional<Member>) {
			using OptionalValue = typename Member::value_type;

			if constexpr (std::same_as<OptionalValue, Rect>) {
				bool changed{ false };
				bool enabled{ member.value.has_value() };

				ImGui::PushID(
					member.name.data(),
					member.name.data() + member.name.size()
				);

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
						target,
						text_data,
						*member.value,
						"Clip Rect",
						locator,
						&MarkTextLayoutDirty
					);
				}

				ImGui::PopID();
				return changed;
			} else if constexpr (ReflectedMembers<OptionalValue>) {
				bool enabled{ member.value.has_value() };
				bool changed{ false };

				ImGui::PushID(
					member.name.data(),
					member.name.data() + member.name.size()
				);

				changed |= DrawOptionalLabelRow(
					PrettyName(member.name),
					enabled,
					false
				);

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
						target,
						text_data,
						*member.value,
						std::make_index_sequence<
							std::tuple_size_v<decltype(clip_members)>
						>{}
					);
				}

				ImGui::PopID();
				return changed;
			} else {
				return DrawValue(
					target.ctx,
					PrettyName(member.name),
					member.value
				);
			}
		} else if constexpr (ReflectedMembers<Member>) {
			auto clip_members{
				ReflectMembers(
					member.value
				)
			};

			return DrawTextClipMembers<I>(
				target,
				text_data,
				member.value,
				std::make_index_sequence<
					std::tuple_size_v<
						decltype(clip_members)
					>
				>{}
			);
		}
	}

	return DrawValue(
		target.ctx,
		PrettyName(member.name),
		member.value
	);
}

template <
	typename Target,
	std::size_t... I
>
bool DrawTextAdditionalMembers(
	Target& target,
	::ptgn::impl::TextData& text_data,
	std::index_sequence<I...>
) {
	bool changed{ false };
	(
		(
			changed |= DrawTextAdditionalMember<I>(
				target,
				text_data
			)
		),
		...
	);
	return changed;
}

template <typename Target>
bool DrawTextAdditional(
	Target& target,
	::ptgn::impl::TextData& text_data
) {
	auto members{
		ReflectMembers(
			text_data
		)
	};

	return DrawTextAdditionalMembers(
		target,
		text_data,
		std::make_index_sequence<
			std::tuple_size_v<
				decltype(members)
			>
		>{}
	);
}

template <typename T>
bool DrawFlattenedConfig(
	EditorContext& ctx,
	T& value
) {
	if constexpr (!ReflectedMembers<T>) {
		return DrawDefaultContents(ctx, value);
	} else {
		bool changed{ false };
		auto members{ ReflectMembers(value) };

		auto draw_member = [&](auto&& member) {
			ScopedID member_scope{
				static_cast<const void*>(
					std::addressof(member.value)
				)
			};
			using Member =
				std::remove_cvref_t<decltype(member.value)>;
			const std::string normalized{
				NormalizeFeatureName(member.name)
			};

			if (normalized == "config" || normalized == "data") {
				if constexpr (
					ReflectedValue<Member> ||
					ReflectedMembers<Member> ||
					ReflectedReadOnlyMembers<Member>
				) {
					changed |= DrawDefaultContents(
						ctx,
						member.value
					);
					return;
				}
			}

			changed |= DrawValue(
				ctx,
				PrettyName(member.name),
				member.value
			);
		};

		std::apply(
			[&](auto&&... member) {
				(draw_member(member), ...);
			},
			members
		);

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
				enabled
					? ComponentState<FillStyle>{ FillStyle{ kInspectorMinLineWidth } }
					: std::nullopt
			);
			changed = true;
		}

		ImGui::SameLine();

		const float checkbox_offset{
			ImGui::GetFrameHeight() +
			ImGui::GetStyle().ItemSpacing.x
		};
		ScopedPropertyLabelOffset label_offset{ checkbox_offset };

		float line_width{ kInspectorMinLineWidth };
		if (const auto current{ target.template Capture<FillStyle>() }) {
			line_width = current->GetLineWidth().value_or(kInspectorMinLineWidth);
		}

		{
			ScopedDisabled disabled{ !enabled };
			if (DrawValue(
				target.ctx,
				"Line Width",
				line_width,
				FieldOptions{
					.speed = kInspectorScalarDragSpeed,
					.min = kInspectorMinLineWidth,
					.max = 1000.0f,
					.format = "%.2f",
					.flags = ImGuiSliderFlags_AlwaysClamp,
				}
			)) {
				target.template SetLive<FillStyle>(FillStyle{ line_width });
				changed = true;
			}
		}

		auto after{ target.template Capture<FillStyle>() };
		TrackComponentState(
			target,
			"Edit Line Width",
			std::move(before),
			std::move(after),
			changed
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
		return std::max(
			std::abs(value.x),
			std::abs(value.y)
		);
	} else if constexpr (kIsArray<Value> || kIsVector<Value>) {
		float limit{ 0.0f };
		for (const auto& element : value) {
			limit = std::max(
				limit,
				GetShapeRadiusLimit(element)
			);
		}
		return limit;
	} else if constexpr (ReflectedValue<Value>) {
		return GetShapeRadiusLimit(
			ReflectValue(
				const_cast<Value&>(value)
			).value
		);
	} else if constexpr (ReflectedMembers<Value>) {
		float limit{ 0.0f };
		auto members{
			ReflectMembers(
				const_cast<Value&>(value)
			)
		};

		auto inspect_member = [&](auto&& member) {
			using Member = std::remove_cvref_t<
				decltype(member.value)
			>;
			const std::string normalized{
				NormalizeFeatureName(
					member.name
				)
			};

			if (
				normalized.contains("radius") ||
				normalized.contains("radii")
			) {
				if constexpr (
					std::same_as<Member, float> ||
					std::same_as<Member, V2_float> ||
					kIsArray<Member> ||
					kIsVector<Member> ||
					ReflectedValue<Member> ||
					ReflectedMembers<Member>
				) {
					limit = std::max(
						limit,
						GetShapeRadiusLimit(member.value)
					);
				}
			} else if constexpr (
				ReflectedValue<Member> ||
				ReflectedMembers<Member>
			) {
				limit = std::max(
					limit,
					GetShapeRadiusLimit(member.value)
				);
			}
		};

		std::apply(
			[&](auto&&... member) {
				(inspect_member(member), ...);
			},
			members
		);

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
		return std::max(
			std::abs(size.x),
			std::abs(size.y)
		);
	} else if constexpr (ReflectedValue<Value>) {
		return GetShapeSizeLimit(
			ReflectValue(
				const_cast<Value&>(value)
			).value
		);
	} else if constexpr (ReflectedMembers<Value>) {
		float limit{ 0.0f };
		auto members{
			ReflectMembers(
				const_cast<Value&>(value)
			)
		};

		auto inspect_member = [&](auto&& member) {
			using Member = std::remove_cvref_t<
				decltype(member.value)
			>;
			const std::string normalized{
				NormalizeFeatureName(
					member.name
				)
			};

			if constexpr (std::same_as<Member, Rect>) {
				limit = std::max(
					limit,
					GetShapeSizeLimit(member.value)
				);
			} else if constexpr (std::same_as<Member, V2_float>) {
				if (
					normalized.contains("size") ||
					normalized.contains("dimension")
				) {
					limit = std::max(
						limit,
						std::max(
							std::abs(member.value.x),
							std::abs(member.value.y)
						)
					);
				}
			} else if constexpr (std::same_as<Member, float>) {
				if (
					normalized == "width" ||
					normalized == "height"
				) {
					limit = std::max(
						limit,
						std::abs(member.value)
					);
				}
			} else if constexpr (
				ReflectedValue<Member> ||
				ReflectedMembers<Member>
			) {
				limit = std::max(
					limit,
					GetShapeSizeLimit(member.value)
				);
			}
		};

		std::apply(
			[&](auto&&... member) {
				(inspect_member(member), ...);
			},
			members
		);

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
					limit = std::max(limit, std::min(std::abs(member.value.x), std::abs(member.value.y)));
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
float GetShapeLineWidthLimit(const T& value) {
	using Value = std::remove_cvref_t<T>;

	float limit{ 1000.0f };

	if constexpr (std::same_as<Value, Rect>) {
		limit = 1000.0f;
	} else if constexpr (std::same_as<Value, RoundedRect>) {
		limit = GetShapeRadiusLimit(value);
	} else if constexpr (std::same_as<Value, Ellipse>) {
		limit = GetEllipseLineWidthLimit(value);
	} else if constexpr (
		std::same_as<Value, Circle> ||
		std::same_as<Value, Capsule> ||
		std::same_as<Value, Arc>
	) {
		limit = GetShapeRadiusLimit(value);
	}

	return std::max(
		kInspectorMinLineWidth,
		limit
	);
}

template <typename Target, typename Shape>
bool DrawShapeFillStyle(Target& target) {
	if constexpr (
		!Target::template Supports<FillStyle>()
	) {
		return false;
	} else {
		const Shape shape{
			target.template Capture<Shape>()
				.value_or(Shape{})
		};
		const float line_width_limit{
			GetShapeLineWidthLimit(
				shape
			)
		};

		return DrawOptionalComponent<Target, FillStyle>(
			target,
			"Style",
			false,
			[&target, line_width_limit](FillStyle& style) {
				return DrawFillStyle(
					target.ctx,
					"Style",
					style,
					line_width_limit
				);
			}
		);
	}
}

template <typename Target>
bool DrawShapeVisual(
	Target& target,
	std::string_view visual
) {
	bool changed{ false };

	auto draw_shape = [&]<typename T>() {
		changed |= DrawRequiredInlineVisualComponentWithDefault<Target, T>(
			target,
			TypeLabel<T>(),
			MakeDefaultShapeGeometry<T>(),
			[&target](T& value) {
				return DrawGeometryComponent(target, value);
			}
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

	changed |= DrawOptionalVisualComponent<Target, Color>(
		target,
		"Color"
	);
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
		changed |= DrawOptionalVisualComponent<Target, FillStyle>(
			target,
			"Style"
		);
	}

	return changed;
}

bool DrawAnimationDataFlattened(
	EditorContext& ctx,
	::ptgn::impl::AnimationData& animation,
	std::optional<std::size_t> detected_frame_count,
	std::optional<V2_int> texture_size
) {
	const auto initial_frame_size{ animation.config.frame_size };
	const V2_int initial_start_pixel{ animation.config.start_pixel };

	bool changed{ false };
	auto members{ ReflectMembers(animation) };

	auto draw_member = [&](auto&& member) {
		const std::string normalized{
			NormalizeFeatureName(member.name)
		};

		using Member = std::remove_cvref_t<
			decltype(member.value)
		>;

		if (normalized == "config") {
			if constexpr (ReflectedMembers<Member>) {
				auto config_members{
					ReflectMembers(member.value)
				};

				std::apply(
					[&](auto&&... config_member) {
						auto draw_config_member = [&](auto&& member) {
							const std::string normalized{
								NormalizeFeatureName(member.name)
							};

							using Value = std::remove_cvref_t<
								decltype(member.value)
							>;

							if constexpr (std::same_as<Value, std::size_t>) {
								if (normalized == "framecount") {
									if (detected_frame_count) {
										member.value = *detected_frame_count;
									}

									{
										ScopedDisabled disabled{
											detected_frame_count.has_value()
										};

										changed |= DrawValue(
											ctx,
											PrettyName(member.name),
											member.value
										);
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

							changed |= DrawValue(
								ctx,
								PrettyName(member.name),
								member.value
							);
						};

						(draw_config_member(config_member), ...);
					},
					config_members
				);

				return;
			}
		}

		changed |= DrawValue(
			ctx,
			PrettyName(member.name),
			member.value
		);
	};

	std::apply(
		[&](auto&&... member) {
			(draw_member(member), ...);
		},
		members
	);

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

				frame_size.x = std::min(frame_size.x, size.x - start_pixel.x);
				frame_size.y = std::min(frame_size.y, size.y - start_pixel.y);
				start_pixel.x = std::min(start_pixel.x, size.x - frame_size.x);
				start_pixel.y = std::min(start_pixel.y, size.y - frame_size.y);

				available_frame_count = static_cast<std::size_t>(
					(size.x - start_pixel.x) / frame_size.x
				);
			} else if (const auto frame_size{
					   ::ptgn::impl::GetFrameSize(texture_size, animation.config.frame_count)
				   };
				   frame_size && frame_size->IsPositive()) {
				start_pixel.x = std::min(start_pixel.x, size.x - frame_size->x);
				start_pixel.y = std::min(start_pixel.y, size.y - frame_size->y);
				available_frame_count = static_cast<std::size_t>(
					(size.x - start_pixel.x) / frame_size->x
				);
			} else {
				available_frame_count = 0;
			}
		}

		const std::size_t usable_frame_count{
			std::min(animation.config.frame_count, available_frame_count)
		};

		animation.current_frame = usable_frame_count == 0
			? 0
			: std::min(animation.current_frame, usable_frame_count - 1);

		changed |= start_pixel != before_start_pixel;
		changed |= animation.config.frame_size != before_frame_size;
		changed |= animation.current_frame != before_current_frame;
	}

	if (ctx.local.settings.show_read_only_inspector_data) {
		[&]<typename T>(T& value) {
			if constexpr (ReflectedReadOnlyMembers<T>) {
				auto read_only_members{
					ReflectReadOnlyMembers(value)
				};

				std::apply(
					[&](auto&&... member) {
						(
							DrawReadOnlyValue(
								ctx,
								PrettyName(member.name),
								member.value
							),
							...
						);
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
		return V2_int{
			static_cast<int>(std::round(value.x)),
			static_cast<int>(std::round(value.y))
		};
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
		V2_int displayed{
			static_cast<int>(std::round(value.x)),
			static_cast<int>(std::round(value.y))
		};

		if (!DrawWHValue("Texture Size", displayed, 1.0f, 0, 4096)) {
			return false;
		}

		value = V2_float{
			static_cast<float>(displayed.x),
			static_cast<float>(displayed.y)
		};
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
bool SetTextureSizeFromPixels(
	Value& value,
	V2_float pixels
) {
	using Type = std::remove_cvref_t<Value>;

	if constexpr (std::same_as<Type, V2_float>) {
		value = pixels;
		return true;
	} else if constexpr (std::same_as<Type, V2_int>) {
		value = V2_int{
			static_cast<int>(std::round(pixels.x)),
			static_cast<int>(std::round(pixels.y))
		};
		return true;
	} else if constexpr (ReflectedValue<Type>) {
		auto member{ ReflectValue(value) };
		return SetTextureSizeFromPixels(
			member.value,
			pixels
		);
	} else if constexpr (ReflectedMembers<Type>) {
		auto members{ ReflectMembers(value) };

		if constexpr (std::tuple_size_v<decltype(members)> == 1) {
			return SetTextureSizeFromPixels(
				std::get<0>(members).value,
				pixels
			);
		}
	}

	return false;
}

template <typename Target>
[[nodiscard]] std::optional<std::size_t> ResolveDetectedAnimationFrameCount(
	const Target& target
) {
	if constexpr (!Target::template Supports<TextureKey>()) {
		return std::nullopt;
	} else {
		const auto texture_key{ target.template Capture<TextureKey>() };

		if (!texture_key) {
			return std::nullopt;
		}

		return ::ptgn::impl::DetectAnimationFrameCount(
			target.ctx.editor.GetAssetManager(),
			*texture_key
		);
	}
}

template <typename Target>
[[nodiscard]] std::optional<V2_int> ResolveAnimationTextureSize(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		if (entity) {
			const auto texture_size{ GetTextureSize(entity) };
			return texture_size && texture_size->IsPositive()
				? texture_size
				: std::nullopt;
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
	using TextureCrop = ::ptgn::impl::TextureCrop;

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
		TrackComponentState(
			target,
			reason,
			std::move(before_animation),
			after_animation,
			true
		);

		if constexpr (Target::template Supports<TextureCrop>()) {
			auto before_crop{ target.template Capture<TextureCrop>() };
			TextureCrop crop{ before_crop.value_or(TextureCrop{}) };
			crop.Update(animation, texture_size);
			target.template SetLive<TextureCrop>(crop);
			auto after_crop{ target.template Capture<TextureCrop>() };
			TrackComponentState(
				target,
				"Update Animation Texture Crop",
				std::move(before_crop),
				std::move(after_crop),
				true
			);
		}

		return true;
	}
}

template <typename Target>
bool SynchronizeAnimationTextureCrop(Target& target) {
	using AnimationData = ::ptgn::impl::AnimationData;
	using TextureCrop = ::ptgn::impl::TextureCrop;

	if constexpr (
		!Target::template Supports<AnimationData>() ||
		!Target::template Supports<TextureCrop>()
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
			target,
			"Update Animation Texture Crop",
			std::move(before_crop),
			std::move(after_crop),
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
			target,
			"Disable Texture Crop",
			std::move(before_crop),
			std::move(after_crop),
			true
		);
		return true;
	}
}

bool DrawSpriteStackData(
	EditorContext& ctx,
	SpriteStackData& data,
	std::optional<int> detected_slice_count
) {
	bool changed{ false };

	int displayed_slice_count{
		detected_slice_count.value_or(data.slice_count)
	};

	{
		ScopedDisabled disabled{
			detected_slice_count.has_value()
		};

		changed |= DrawValue(
			ctx,
			"Slice Count",
			displayed_slice_count,
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

	changed |= DrawValue(
		ctx,
		"Slice Order",
		data.slice_order
	);

	changed |= DrawValue(
		ctx,
		"Layer Offset",
		data.layer_offset,
		FieldOptions{
			.speed	= 0.1f,
			.min	= -1000.0f,
			.max	= 1000.0f,
			.format = "%.2f",
		}
	);

	changed |= DrawValue(
		ctx,
		"Pixel Snap",
		data.pixel_snap
	);

	return changed;
}

template <typename Target>
bool DrawSpriteStackPrimary(Target& target) {
	bool changed{ false };

	changed |= DrawRequiredInlineVisualComponent<Target, TextureKey>(
		target,
		"Texture Key",
		[&target](TextureKey& value) {
			return DrawValue(
				target.ctx,
				"Texture Key",
				value
			);
		}
	);

	const auto texture_key{
		target.template Capture<TextureKey>()
	};

	const std::optional<std::size_t> detected_slice_count{
		texture_key
			? ::ptgn::impl::DetectSpriteStackSliceCount(
				target.ctx.editor.GetAssetManager(),
				*texture_key
			)
			: std::nullopt
	};

	changed |= DrawRequiredInlineVisualComponent<
		Target,
		SpriteStackData
	>(
		target,
		"Sprite Stack",
		[&target, detected_slice_count](SpriteStackData& value) {
			return DrawSpriteStackData(
				target.ctx,
				value,
				detected_slice_count
			);
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
	const auto before_texture_size{
		target.template Capture<::ptgn::impl::TextureSize>()
	};

	std::optional<V2_float> texture_size_default;

	if (
		!before_texture_size &&
		before_animation &&
		before_animation->config.frame_size
	) {
		V2_float scale{ 1.0f };

		if constexpr (Target::template Supports<Transform>()) {
			if (const auto transform{ target.template Capture<Transform>() }) {
				scale = V2_float{
					std::abs(transform->scale.x),
					std::abs(transform->scale.y)
				};
			}
		}

		const auto frame_size{ *before_animation->config.frame_size };

		texture_size_default = V2_float{
			static_cast<float>(frame_size.x) * scale.x,
			static_cast<float>(frame_size.y) * scale.y
		};
	}

	changed |= DrawRequiredInlineVisualComponent<Target, TextureKey>(
		target,
		"Texture Key",
		[&target](TextureKey& value) {
			return DrawValue(target.ctx, "Texture Key", value);
		}
	);

	const auto detected_frame_count{
		ResolveDetectedAnimationFrameCount(target)
	};

	changed |= DrawOptionalComponent<Target, ::ptgn::impl::TextureSize>(
		target,
		"Texture Size",
		false,
		[&target, before_texture_size, texture_size_default](
			::ptgn::impl::TextureSize& value
		) {
			bool initialized{ false };

			if (!before_texture_size && texture_size_default) {
				initialized = SetTextureSizeFromPixels(
					value,
					*texture_size_default
				);
			}

			return DrawTextureSizeAsIntegers(
				target.ctx,
				value
			) || initialized;
		}
	);

	const auto animation_texture_size{
		ResolveAnimationTextureSize(target)
	};

	changed |= DrawOptionalComponent<Target, AnimationData>(
		target,
		"Animation",
		true,
		[&target, detected_frame_count, animation_texture_size](AnimationData& value) {
			const std::size_t before_frame_count{ value.config.frame_count };

			const bool local_changed{
				DrawAnimationDataFlattened(
					target.ctx,
					value,
					detected_frame_count,
					animation_texture_size
				)
			};

			const bool frame_count_changed{
				before_frame_count != value.config.frame_count
			};

			if (frame_count_changed) {
				if (!value.config.frame_size.has_value()) {
					value.config.frame_size = ::ptgn::impl::GetFrameSize(
						animation_texture_size,
						value.config.frame_count
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
	const bool frame_count_changed{
		before_animation && after_animation &&
		before_animation->config.frame_count != after_animation->config.frame_count
	};

	if (!animation_enabled && animation_was_enabled) {
		changed |= DisableAnimationTextureCrop(target);
	} else if (animation_enabled && texture_changed) {
		changed |= SynchronizeAnimationFrameData(
			target,
			"Recalculate Animation Frame Size From Texture"
		);
	} else if (animation_enabled && (!animation_was_enabled || frame_count_changed)) {
		changed |= SynchronizeAnimationTextureCrop(target);
	}

	return changed;
}

template <typename Target>
bool DrawMaterialDetails(Target& target, ::ptgn::Material& material) {
	bool changed{ false };

	changed |= DrawVectorEditor(
		target.ctx,
		material.uniforms,
		VectorOptions{
			.item_name = "Uniform",
			.add_label = "+ Add Uniform",
			.add_first = true,
		}
	);

	const std::size_t max_texture_slots{
		std::max(
			std::size_t{ 1 },
			static_cast<std::size_t>(
				target.ctx.editor.GetMaxTextureSlots()
			)
		)
	};

	changed |= DrawValue(
		target.ctx,
		"Texture Slot Capacity",
		material.texture_slot_capacity,
		FieldOptions{
			.speed = 1.0f,
			.min = 1.0f,
			.max = static_cast<float>(max_texture_slots),
			.format = "%llu",
			.flags = ImGuiSliderFlags_AlwaysClamp,
		}
	);

	if (material.texture_slot_capacity.has_value()) {
		const std::size_t clamped{
			std::clamp(
				*material.texture_slot_capacity,
				std::size_t{ 1 },
				max_texture_slots
			)
		};

		if (*material.texture_slot_capacity != clamped) {
			material.texture_slot_capacity = clamped;
			changed = true;
		}
	}

	return changed;
}

template <typename Target, AssetKeyType Key>
bool DrawOptionalAssetComponent(
	Target& target,
	std::string_view label
) {
	if constexpr (!Target::template Supports<Key>()) {
		return false;
	} else {
		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<Key>()) };

		auto before{ target.template Capture<Key>() };
		std::optional<Key> value{ before };

		const bool changed{
			DrawOptionalAssetKeyInline(
				target.ctx,
				label,
				value,
				FieldOptions{}
			)
		};

		if (changed) {
			target.template SetLive<Key>(
				value
			);
		}

		auto after{ target.template Capture<Key>() };

		TrackComponentState(
			target,
			std::string{ "Edit " } + std::string{ label },
			std::move(before),
			std::move(after),
			changed
		);

		return changed;
	}
}

template <typename Target>
bool DrawCustomShaderMaterial(
	Target& target,
	::ptgn::Material& material
) {
	bool changed{ DrawValue(target.ctx, "Shader Key", material.shader) };
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
			target,
			"Edit Material",
			std::move(before),
			std::move(after),
			material_changed
		);

		changed |= material_changed;
	} else {
		changed |= DrawRequiredComponent<Target, ::ptgn::Material>(
			target,
			"Material",
			false,
			[&target](::ptgn::Material& material) {
				return DrawCustomShaderMaterial(target, material);
			}
		);
	}

	changed |= DrawOptionalAssetComponent<Target, TextureKey>(
		target,
		"Texture Key"
	);

	changed |= DrawOptionalComponent<Target, Rect>(
		target,
		"Size",
		false,
		[](Rect& value) {
			V2_float size{ value.GetSize() };

			if (!DrawWHValue(
					"Size",
					size,
					kInspectorSizeDragSpeed,
					0.0f,
					0.0f,
					"%.3f"
				)) {
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
		false,
		false,
		nullptr,
		Rect{ V2_float{ 100.0f, 100.0f } }
	);

	return changed;
}

template <typename Target>
bool DrawSpriteAdditional(Target& target) {
	bool changed{ false };
	const bool animated{
		target.template Capture<
			::ptgn::impl::AnimationData
		>().has_value()
	};

	changed |= DrawOptionalVisualComponent<
		Target,
		::ptgn::impl::Offsets
	>(
		target,
		"Offsets",
		true
	);

	if (animated) {
		changed |= DrawReadOnlyExistingReflected<
			Target,
			::ptgn::impl::TextureCrop
		>(
			target,
			"Texture Crop",
			true
		);
	} else {
		changed |= DrawOptionalVisualComponent<
			Target,
			::ptgn::impl::TextureCrop
		>(
			target,
			"Texture Crop",
			true
		);
	}

	return changed;
}

template <typename Target, typename T>
bool DrawOptionalNamedValue(
	Target& target,
	std::string_view label
) {
	return DrawOptionalComponent<Target, T>(
		target,
		label,
		false,
		[&target, label](T& value) {
			return [&target, label]<typename Value>(Value& reflected_value) {
				if constexpr (ReflectedValue<Value>) {
					auto member{
						ReflectValue(reflected_value)
					};
					return DrawValue(
						target.ctx,
						label,
						member.value
					);
				} else if constexpr (ReflectedMembers<Value>) {
					auto members{
						ReflectMembers(reflected_value)
					};

					if constexpr (
						std::tuple_size_v<
							decltype(members)
						> == 1
					) {
						auto& member{
							std::get<0>(members)
						};
						return DrawValue(
							target.ctx,
							label,
							member.value
						);
					} else {
						return DrawMembers(
							target.ctx,
							reflected_value
						);
					}
				} else {
					return DrawDefaultContents(
						target.ctx,
						reflected_value
					);
				}
			}(value);
		}
	);
}

template <typename Target>
bool DrawRenderTargetPrimary(Target& target) {
	bool changed{ false };

	const bool primary_scene_target{
		IsPrimarySceneRenderTarget(target)
	};

	std::optional<V2_int> framebuffer_size;

	if constexpr (requires { target.entity; }) {
		Entity entity{
			target.entity
		};

		if (
			entity &&
			entity.Has<::ptgn::impl::FramebufferObject>()
		) {
			const V2_int actual_size{
				RenderTarget{ entity }.GetSize()
			};

			if (actual_size.IsPositive()) {
				framebuffer_size = actual_size;
			}
		}
	}

	if (primary_scene_target) {
		changed |= DrawRequiredInlineVisualComponent<
			Target,
			::ptgn::impl::RenderTargetDesc
		>(
			target,
			"Render Target",
			[&target, framebuffer_size](
				::ptgn::impl::RenderTargetDesc& value
			) {
				return DrawRenderTargetDesc(
					target.ctx,
					value,
					framebuffer_size,
					true
				);
			}
		);
	} else {
		changed |= DrawOptionalComponent<
			Target,
			::ptgn::impl::RenderTargetDesc
		>(
			target,
			"Render Target",
			false,
			[&target, framebuffer_size](
				::ptgn::impl::RenderTargetDesc& value
			) {
				return DrawRenderTargetDesc(
					target.ctx,
					value,
					framebuffer_size
				);
			}
		);
	}

	changed |= DrawOptionalNamedValue<
		Target,
		::ptgn::impl::ClearColor
	>(
		target,
		"Clear Color"
	);

	changed |= DrawOptionalNamedValue<
		Target,
		::ptgn::impl::ClearDepth
	>(
		target,
		"Clear Depth"
	);

	changed |= DrawOptionalNamedValue<
		Target,
		::ptgn::impl::ClearStencil
	>(
		target,
		"Clear Stencil"
	);

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

		::ptgn::impl::RenderMask mask{
			before_mask.value_or(::ptgn::impl::RenderMask{})
		};
		bool ui_layer{ before_ui.has_value() };

		ScopedID target_scope{ target.Id() };
		ScopedID layers_scope{ "VisualLayers" };

		const bool changed{
			DrawLayerMaskValue(
				"Layers",
				mask.layers,
				ui_layer
			)
		};

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

		auto apply_mask{
			target.template MakeApply<::ptgn::impl::RenderMask>()
		};
		auto apply_ui{
			target.template MakeApply<::ptgn::impl::UILayer>()
		};
		const ImGuiID key{ ImGui::GetID("##VisualLayersEdit") };

		TrackUndoableInteraction(
			target.ctx,
			key,
			"Edit Layers",
			true,
			[
				apply_mask,
				apply_ui,
				before_mask,
				before_ui
			]() mutable {
				apply_mask(before_mask);
				apply_ui(before_ui);
			},
			[
				apply_mask,
				apply_ui,
				after_mask,
				after_ui
			]() mutable {
				apply_mask(after_mask);
				apply_ui(after_ui);
			}
		);

		return true;
	}
}

template <typename Target>
bool DrawVisualAdditionalOptions(
	Target& target,
	std::string_view visual,
	bool draw_tint
) {
	const bool open{
		ImGui::TreeNodeEx(
			"Additional Options##Visual",
			ImGuiTreeNodeFlags_SpanAvailWidth
		)
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
			changed |= DrawOptionalVisualComponent<
				Target,
				::ptgn::impl::ShadowCaster
			>(
				target,
				"Shadow Caster",
				true
			);
		}

		if (visual.contains("text")) {
			ScopedID text_additional_scope{ "TextAdditional" };
			changed |= DrawRequiredComponent<
				Target,
				::ptgn::impl::TextData
			>(
				target,
				"Text Additional Options",
				false,
				[&target](::ptgn::impl::TextData& value) {
					return DrawTextAdditional(target, value);
				},
				&MarkTextLayoutDirty
			);
		}

		changed |= DrawOptionalVisualComponent<Target, BlendMode>(
			target,
			"Blend Mode"
		);

		if (draw_tint) {
			changed |= DrawOptionalVisualComponent<Target, Tint>(
				target,
				"Tint"
			);

			changed |= DrawOptionalVisualComponent<
				Target,
				::ptgn::impl::IgnoreParentTint
			>(
				target,
				"Ignore Parent Tint"
			);
		}

		if (!IsPrimarySceneRenderTarget(target)) {
			changed |= DrawOptionalVisualComponent<
				Target,
				::ptgn::impl::IgnoreParentVisibility
			>(
				target,
				"Ignore Parent Visibility"
			);

			changed |= DrawVisualLayers(target);
		}
	}

	ImGui::TreePop();
	return changed;
}

template <
	typename Target,
	typename Visuals,
	typename Draw,
	typename Callback
>
bool DrawButtonChildStateVisualComponent(
	Target& target,
	ButtonVisualState state,
	std::string_view part_label,
	Draw&& draw,
	Callback callback
) {
	if constexpr (!Target::template Supports<Visuals>()) {
		return false;
	} else {
		const bool open{
			ImGui::CollapsingHeader(
				"Visual##ButtonVisualStateVisual",
				ImGuiTreeNodeFlags_DefaultOpen
			)
		};

		if (!open) {
			return false;
		}

		ScopedIndent feature_indent;
		AutoLabelWidthScope label_width{ "ButtonVisualStateVisualFields" };
		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<Visuals>()) };

		auto before{ target.template Capture<Visuals>() };
		Visuals visuals{ before.value_or(Visuals{}) };
		const auto index{
			static_cast<std::size_t>(std::to_underlying(state))
		};
		auto& visual{ visuals.states[index] };

		DrawDisabledWrappedText(
			PrettyName(magic_enum::enum_name(state)) + " " +
			std::string{ part_label } +
			" overrides. Unset values inherit from fallback states."
		);

		const bool changed{
			std::invoke(
				std::forward<Draw>(draw),
				visuals,
				visual
			)
		};

		if (changed) {
			visual.defined = HasButtonVisualOverrides(visual);
			target.template SetLive<Visuals>(
				ComponentState<Visuals>{ visuals },
				callback
			);
		}

		auto after{ target.template Capture<Visuals>() };
		TrackComponentState(
			target,
			std::string{ "Edit " } + std::string{ part_label } + " State Visual",
			std::move(before),
			std::move(after),
			changed,
			callback
		);

		return changed;
	}
}

template <typename Target, typename Locator>
bool DrawPickableButtonTextBoxPosition(
	Target& target,
	ButtonTextVisuals& visuals,
	const ButtonChildInfo& child_info,
	ButtonVisualState state,
	std::string_view label,
	Locator locator
) {
	V2_float& position{ locator(visuals) };

	return DrawPropertyRow(label, [&]() {
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float pick_width{
			ImGui::CalcTextSize("Pick").x +
			ImGui::GetStyle().FramePadding.x * 2.0f
		};
		const float available{ ImGui::GetContentRegionAvail().x };
		const float field_width{
			std::max(36.0f, (available - pick_width - spacing * 2.0f) * 0.5f)
		};
		bool changed{ false };

		ImGui::SetNextItemWidth(field_width);
		changed |= ImGui::DragFloat(
			"##X", &position.x, kInspectorPositionDragSpeed, 0.0f, 0.0f, "X: %.2f"
		);
		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(field_width);
		changed |= ImGui::DragFloat(
			"##Y", &position.y, kInspectorPositionDragSpeed, 0.0f, 0.0f, "Y: %.2f"
		);
		ImGui::SameLine(0.0f, spacing);

		auto apply{
			target.template MakeApply<ButtonTextVisuals>(
				&MarkButtonTextDirty
			)
		};
		ButtonTextVisuals snapshot{ visuals };

		DrawPositionPickButton(
			target.ctx,
			label,
			position,
			MakeButtonTextBoxPositionConverter(
				child_info.child,
				child_info.button,
				state
			),
			[apply, snapshot = std::move(snapshot), locator](
				V2_float picked
			) mutable {
				locator(snapshot) = picked;
				apply(ComponentState<ButtonTextVisuals>{ snapshot });
			},
			GetButtonTextBoxWorldPosition(
				child_info.child,
				child_info.button,
				state,
				position
			),
			true
		);

		return changed;
	});
}

template <typename Target>
bool DrawButtonTextBoxOverride(
	Target& target,
	ButtonTextVisuals& visuals,
	ButtonTextVisual& visual,
	const ButtonChildInfo& child_info,
	ButtonVisualState state
) {
	const auto index{
		static_cast<std::size_t>(std::to_underlying(state))
	};
	const std::optional<TextBox> inherited{
		ResolveButtonVisualProperty(
			visuals.states,
			state,
			&ButtonTextVisual::box
		)
	};
	bool enabled{ visual.box.has_value() };
	bool changed{ false };

	ScopedID box_scope{ "ButtonTextBoxOverride" };

	if (ImGui::Checkbox("##Enabled", &enabled)) {
		if (enabled) {
			visual.box = inherited.value_or(TextBox{});
			visual.defined = true;
		} else {
			visual.box.reset();
		}
		changed = true;
	}

	ImGui::SameLine();
	const bool open{
		ImGui::TreeNodeEx(
			"Text Box##ButtonTextBoxTree",
			ImGuiTreeNodeFlags_SpanAvailWidth
		)
	};

	if (!open) {
		return changed;
	}

	ScopedIndent indent;
	ScopedPropertyLabelOffset box_label_offset{
		ImGui::GetStyle().IndentSpacing
	};
	TextBox displayed{
		visual.box.value_or(inherited.value_or(TextBox{}))
	};
	TextBox& box{ enabled ? visual.box.value() : displayed };
	ScopedDisabled disabled{ !enabled };

	auto members{ ReflectMembers(box) };
	auto draw_member = [&](auto&& member) {
		const std::string normalized{ NormalizeFeatureName(member.name) };

		if (normalized == "rect") {
			using Member = std::remove_cvref_t<decltype(member.value)>;
			if constexpr (std::same_as<Member, Rect>) {
				V2_float size{ member.value.GetSize() };
				if (DrawWHValue("Size", size, kInspectorSizeDragSpeed, 0.0f, 0.0f, "%.3f")) {
					size.x = std::max(size.x, 0.0f);
					size.y = std::max(size.y, 0.0f);
					const V2_float center{ member.value.GetCenter() };
					const V2_float half_size{ size * 0.5f };
					member.value.min = center - half_size;
					member.value.max = center + half_size;
					changed = true;
				}

				if (enabled) {
					auto min_locator = [index](ButtonTextVisuals& root) -> V2_float& {
						return root.states[index].box.value().rect.min;
					};
					auto max_locator = [index](ButtonTextVisuals& root) -> V2_float& {
						return root.states[index].box.value().rect.max;
					};
					changed |= DrawPickableButtonTextBoxPosition(
						target,
						visuals,
						child_info,
						state,
						"Min",
						min_locator
					);
					changed |= DrawPickableButtonTextBoxPosition(
						target,
						visuals,
						child_info,
						state,
						"Max",
						max_locator
					);
				} else {
					changed |= DrawValue(target.ctx, "Min", member.value.min);
					changed |= DrawValue(target.ctx, "Max", member.value.max);
				}
			}
			return;
		}

		if (normalized == "style") {
			if (ImGui::TreeNodeEx(
					"Additional Options##ButtonTextBoxAdditionalOptions",
					ImGuiTreeNodeFlags_SpanAvailWidth
				)) {
				{
					ScopedUnindent align_with_additional_options;
					changed |= DrawDefaultContents(
						target.ctx,
						member.value
					);
				}
				ImGui::TreePop();
			}
			return;
		}

		changed |= DrawValue(
			target.ctx,
			PrettyName(member.name),
			member.value
		);
	};

	std::apply(
		[&](auto&&... member) {
			(draw_member(member), ...);
		},
		members
	);

	ImGui::TreePop();
	return changed;
}

template <typename Target>
bool DrawButtonChildStateVisualFeature(
	Target& target,
	const ButtonChildInfo& child_info,
	ButtonVisualState state
) {
	switch (child_info.part) {
		case ButtonChildPart::Background:
			return DrawButtonChildStateVisualComponent<
				Target,
				ButtonBackgroundVisuals
			>(
				target,
				state,
				"Button Background",
				[&target, state](auto& visuals, ButtonShapeVisual&) {
					bool changed{ false };
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Size", visuals.states, state, &ButtonShapeVisual::size
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Origin", visuals.states, state, &ButtonShapeVisual::origin
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Anchor", visuals.states, state, &ButtonShapeVisual::anchor
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Color", visuals.states, state, &ButtonShapeVisual::color
					);
					return changed;
				},
				&MarkButtonBackgroundDirty
			);
		case ButtonChildPart::Border:
			return DrawButtonChildStateVisualComponent<
				Target,
				ButtonBorderVisuals
			>(
				target,
				state,
				"Button Border",
				[&target, state](auto& visuals, ButtonShapeVisual&) {
					bool changed{ false };
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Size", visuals.states, state, &ButtonShapeVisual::size
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Origin", visuals.states, state, &ButtonShapeVisual::origin
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Anchor", visuals.states, state, &ButtonShapeVisual::anchor
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Color", visuals.states, state, &ButtonShapeVisual::color
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Fill Style", visuals.states, state, &ButtonShapeVisual::fill_style
					);
					return changed;
				},
				&MarkButtonBorderDirty
			);
		case ButtonChildPart::Text:
			return DrawButtonChildStateVisualComponent<
				Target,
				ButtonTextVisuals
			>(
				target,
				state,
				"Button Text",
				[&target, &child_info, state](ButtonTextVisuals& visuals, ButtonTextVisual& visual) {
					bool changed{ false };
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Content", visuals.states, state, &ButtonTextVisual::styled_text
					);
					changed |= DrawButtonTextBoxOverride(
						target,
						visuals,
						visual,
						child_info,
						state
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Origin", visuals.states, state, &ButtonTextVisual::origin
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Anchor", visuals.states, state, &ButtonTextVisual::anchor
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Auto Box", visuals.states, state, &ButtonTextVisual::auto_box
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Padding", visuals.states, state, &ButtonTextVisual::padding
					);
					return changed;
				},
				&MarkButtonTextDirty
			);
		case ButtonChildPart::Sprite:
			return DrawButtonChildStateVisualComponent<
				Target,
				ButtonSpriteVisuals
			>(
				target,
				state,
				"Button Sprite",
				[&target, state](auto& visuals, ButtonSpriteVisual&) {
					bool changed{ false };
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Texture Key", visuals.states, state, &ButtonSpriteVisual::texture
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Origin", visuals.states, state, &ButtonSpriteVisual::origin
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Anchor", visuals.states, state, &ButtonSpriteVisual::anchor
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Size", visuals.states, state, &ButtonSpriteVisual::size
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Tint", visuals.states, state, &ButtonSpriteVisual::tint
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Animation", visuals.states, state, &ButtonSpriteVisual::animation
					);
					changed |= DrawButtonVisualOverrideValue(
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
bool DrawVisualFeature(Target& target) {
	if (const auto child_info{ GetButtonChildInfo(target) }) {
		if (const auto state{ GetButtonVisualEditState(target) }) {
			return DrawButtonChildStateVisualFeature(
				target,
				*child_info,
				*state
			);
		}
	}

	if (!HasVisualFeature(target)) {
		return false;
	}

	const bool primary_scene_target{
		IsPrimarySceneRenderTarget(target)
	};

	const auto header{ DrawFeatureHeader(
		target,
		InspectorFeature::Visual,
		"Visual",
		ImGuiTreeNodeFlags_DefaultOpen,
		VisualFeatureComponents{},
		!primary_scene_target
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;
	AutoLabelWidthScope visual_label_width{ "VisualFeatureFields" };

	bool changed{ header.changed };
	const RendererRowResult renderer{ DrawRendererRow(target) };
	const std::string& visual{ renderer.visual };
	changed |= renderer.changed;

	if (renderer.changed) {
		return true;
	}

	if (visual.empty()) {
		ImGui::TextDisabled(
			"Choose a renderer to expose its relevant components."
		);
		return changed;
	}

	changed |= DrawVisualEffects(target, visual);

	bool draw_tint{ false };
	const bool shape{
		visual == "rect" ||
		visual == "circle" ||
		visual == "roundedrect" ||
		visual == "polygon" ||
		visual == "ellipse" ||
		visual == "triangle" ||
		visual == "line" ||
		visual == "capsule" ||
		visual == "arc"
	};

	if (shape) {
		changed |= DrawShapeVisual(target, visual);
	}  else if (visual == "spritestack") {
		changed |= DrawSpriteStackPrimary(target);
		draw_tint = true;
	} else if (visual.contains("sprite")) {
		changed |= DrawSpritePrimary(target);
		draw_tint = true;
	} else if (visual.contains("text")) {
		ScopedID text_primary_scope{ "TextPrimary" };
		changed |= DrawRequiredInlineVisualComponent<
			Target,
			::ptgn::impl::TextData
		>(
			target,
			"Text",
			[&target](::ptgn::impl::TextData& value) {
				return DrawTextPrimary(target, value);
			},
			&MarkTextLayoutDirty
		);
		draw_tint = true;
	} else if (visual.contains("particle")) {
		changed |= DrawRequiredInlineVisualComponent<
			Target,
			::ptgn::impl::ParticleEmitterData
		>(
			target,
			"Particle Emitter",
			[&target](::ptgn::impl::ParticleEmitterData& value) {
				return DrawFlattenedConfig(target.ctx, value);
			}
		);
	} else if (visual.contains("light")) {
		changed |= DrawRequiredInlineVisualComponent<
			Target,
			LightData
		>(
			target,
			"Light",
			[&target](LightData& value) {
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
				target,
				"Edit Graphics",
				std::move(before),
				std::move(after),
				graphics_changed
			);
			changed |= graphics_changed;
		} else {
			changed |= DrawRequiredInlineVisualComponent<
				Target,
				::ptgn::impl::GraphicsData
			>(
				target,
				"Graphics",
				[&target](::ptgn::impl::GraphicsData& value) {
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
		const Origin origin{
			target.template Capture<Origin>()
				.value_or(Origin::Center)
		};

		DrawReadOnlyValue(
			target.ctx,
			"Origin",
			origin
		);
	} else {
		changed |= DrawOptionalVisualComponent<
			Target,
			Origin
		>(
			target,
			"Origin"
		);
	}
	changed |= DrawVisualAdditionalOptions(
		target,
		visual,
		draw_tint
	);

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
				"Interaction Lock##ReadOnlyInteractionLock",
				ImGuiTreeNodeFlags_SpanAvailWidth
			)) {
			ScopedIndent indent;
			AutoLabelWidthScope label_width{ "InteractionLockReadOnlyFields" };
			ReadOnlyScope read_only{ true };

			[&]<typename T>(const T& reflected_value) {
				if constexpr (ReflectedMembers<T>) {
					auto members{ ReflectMembers(reflected_value) };
					std::apply(
						[&](auto&&... member) {
							(DrawReadOnlyValue(
								target.ctx,
								PrettyName(member.name),
								member.value
							), ...);
						},
						members
					);
				}

				if constexpr (ReflectedReadOnlyMembers<T>) {
					auto members{ ReflectReadOnlyMembers(reflected_value) };
					std::apply(
						[&](auto&&... member) {
							(DrawReadOnlyValue(
								target.ctx,
								PrettyName(member.name),
								member.value
							), ...);
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

	const bool changed{
		DrawOptionalComponent<Target, RigidBody>(
			target,
			"Rigid Body",
			true,
			[&](RigidBody& value) {
				const bool rigid_body_changed{
					DrawRegisteredComponentContents(target.ctx, Hash<RigidBody>(), std::addressof(value))
				};

				inheritance_changed |= DrawOptionalReflected<
					Target,
					::ptgn::impl::IgnoreParentImmovable
				>(
					target,
					"Ignore Parent Immovable",
					false
				);

				return rigid_body_changed;
			}
		)
	};

	if (
		!target.template Capture<RigidBody>() &&
		target.template Capture<
			::ptgn::impl::IgnoreParentImmovable
		>()
	) {
		auto before{
			target.template Capture<
				::ptgn::impl::IgnoreParentImmovable
			>()
		};
		target.template SetLive<
			::ptgn::impl::IgnoreParentImmovable
		>(std::nullopt);
		auto after{
			target.template Capture<
				::ptgn::impl::IgnoreParentImmovable
			>()
		};

		TrackComponentState(
			target,
			"Remove Ignore Parent Immovable",
			std::move(before),
			std::move(after),
			true
		);

		inheritance_changed = true;
	}

	return changed || inheritance_changed;
}

template <typename Target>
bool DrawPhysicsFeature(Target& target) {
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
		target,
		"Collider",
		true,
		[&target](Collider& value) {
			return DrawGeometryComponent(target, value);
		}
	);
	changed |= DrawRigidBodyWithInheritance(target);
	changed |= DrawOptionalReflected<Target, BoundaryBehavior>(
		target,
		"Boundary Behavior",
		false
	);

	enum class MovementKind {
		TopDown,
		Platformer,
	};

	using MovementComponents = FeatureComponents<
		TopDownMovement,
		PlatformerMovement,
		PlatformerJump
	>;

	const bool has_top_down{
		target.template Capture<TopDownMovement>().has_value()
	};
	const bool has_platformer{
		target.template Capture<PlatformerMovement>().has_value()
	};
	bool movement_enabled{
		has_top_down || has_platformer
	};
	MovementKind movement{
		has_platformer
			? MovementKind::Platformer
			: MovementKind::TopDown
	};

	auto apply_movement_change = [&](auto&& mutate) {
		constexpr MovementComponents components{};
		auto before{
			CaptureInspectorFeatureState(
				target,
				InspectorFeature::Physics,
				components
			)
		};

		std::invoke(
			std::forward<decltype(mutate)>(mutate)
		);

		auto after{
			CaptureInspectorFeatureState(
				target,
				InspectorFeature::Physics,
				components
			)
		};

		TrackInspectorFeatureState(
			target,
			InspectorFeature::Physics,
			"Change Movement",
			std::move(before),
			std::move(after),
			components
		);

		changed = true;
	};

	ScopedID movement_scope{ "Movement" };

	if (ImGui::Checkbox("##Enabled", &movement_enabled)) {
		apply_movement_change(
			[&]() {
				if (movement_enabled) {
					target.template SetLive<TopDownMovement>(
						TopDownMovement{}
					);
					target.template SetLive<PlatformerMovement>(
						std::nullopt
					);
				} else {
					SetFeatureManuallyAdded(
						target.GetFeatureTargetKey(),
						InspectorFeature::Physics,
						true
					);
					target.template SetLive<TopDownMovement>(
						std::nullopt
					);
					target.template SetLive<PlatformerMovement>(
						std::nullopt
					);
					target.template SetLive<PlatformerJump>(
						std::nullopt
					);
				}
			}
		);

		movement = MovementKind::TopDown;
	}

	ImGui::SameLine();

	const bool movement_open{
		ImGui::TreeNodeEx(
			"Movement##Tree",
			ImGuiTreeNodeFlags_SpanAvailWidth
		)
	};

	if (movement_open) {
		ScopedIndent movement_indent;
		ScopedPropertyLabelOffset movement_label_offset{
			ImGui::GetStyle().IndentSpacing
		};
		AutoLabelWidthScope movement_label_width{ "MovementFields" };
		ScopedDisabled movement_disabled{ !movement_enabled };

		const char* preview{
			movement == MovementKind::Platformer
				? "Platformer"
				: "Top Down"
		};

		DrawPropertyRow(
			"Type",
			[&]() {
				bool local_changed{ false };

				if (ImGui::BeginCombo(
						"##MovementType",
						preview
					)) {
					auto choose = [&](MovementKind candidate, const char* label) {
						if (!ImGui::Selectable(
								label,
								movement == candidate
							)) {
							return;
						}

						apply_movement_change(
							[&]() {
								if (candidate == MovementKind::TopDown) {
									target.template SetLive<TopDownMovement>(
										TopDownMovement{}
									);
									target.template SetLive<PlatformerMovement>(
										std::nullopt
									);
									target.template SetLive<PlatformerJump>(
										std::nullopt
									);
								} else {
									target.template SetLive<PlatformerMovement>(
										PlatformerMovement{}
									);
									target.template SetLive<TopDownMovement>(
										std::nullopt
									);
								}
							}
						);

						movement = candidate;
						local_changed = true;
					};

					choose(
						MovementKind::TopDown,
						"Top Down"
					);
					choose(
						MovementKind::Platformer,
						"Platformer"
					);

					ImGui::EndCombo();
				}

				return local_changed;
			}
		);

		if (
			movement_enabled &&
			target.template Capture<TopDownMovement>()
		) {
			changed |= DrawRequiredComponent<Target, TopDownMovement>(
				target,
				"Top Down Movement",
				false,
				[&target](TopDownMovement& value) {
					return DrawDefaultContents(
						target.ctx,
						value
					);
				}
			);
		}

		if (
			movement_enabled &&
			target.template Capture<PlatformerMovement>()
		) {
			changed |= DrawRequiredComponent<Target, PlatformerMovement>(
				target,
				"Platformer Movement",
				false,
				[&target](PlatformerMovement& value) {
					return DrawDefaultContents(
						target.ctx,
						value
					);
				}
			);

			changed |= DrawOptionalComponent<Target, PlatformerJump>(
				target,
				"Platformer Jump",
				false,
				[&target](PlatformerJump& value) {
					return DrawDefaultContents(
						target.ctx,
						value
					);
				}
			);
		}

		ImGui::TreePop();
	}

	return changed;
}

bool DrawButtonVisualStateSelector(std::optional<ButtonVisualState>& state) {
	return DrawPropertyRow(
		"Visual State",
		[&]() {
			const std::string preview{
				state
					? PrettyName(magic_enum::enum_name(*state))
					: "Base Entity"
			};

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
				const std::string label{
					PrettyName(magic_enum::enum_name(candidate))
				};

				if (ImGui::Selectable(label.c_str(), selected)) {
					state = candidate;
					changed = !selected;
				}
			}

			ImGui::EndCombo();
			return changed;
		}
	);
}


template <typename Target>
bool DrawSliderWorldPosition(
	Target& target,
	::ptgn::impl::SliderData& data,
	std::string_view label,
	bool start
) {
	V2_float& position{
		start
			? data.line.start
			: data.line.end
	};

	const V2_float previous{ position };

	const bool changed{
		DrawPropertyRow(label, [&]() {
			const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
			const float pick_width{
				ImGui::CalcTextSize("Pick").x +
				ImGui::GetStyle().FramePadding.x * 2.0f
			};
			const float available{ ImGui::GetContentRegionAvail().x };
			const float field_width{
				std::max(
					36.0f,
					(available - pick_width - spacing * 2.0f) * 0.5f
				)
			};

			bool local_changed{ false };

			ImGui::SetNextItemWidth(field_width);
			local_changed |= ImGui::DragFloat(
				"##X",
				&position.x,
				0.1f,
				0.0f,
				0.0f,
				"X: %.2f"
			);

			ImGui::SameLine(0.0f, spacing);
			ImGui::SetNextItemWidth(field_width);
			local_changed |= ImGui::DragFloat(
				"##Y",
				&position.y,
				0.1f,
				0.0f,
				0.0f,
				"Y: %.2f"
			);

			ImGui::SameLine(0.0f, spacing);

			{
				ScopedDisabled disabled{ !CanPickLocalPosition<Target>() };

				auto apply{
					target.template MakeApply<::ptgn::impl::SliderData>()
				};

				::ptgn::impl::SliderData snapshot{ data };
				const V2_float reference{ position };

				DrawPositionPickButton(
					target.ctx,
					label,
					position,
					PositionPicker::Convert{},
					PositionPicker::Apply{
						[
							apply,
							snapshot = std::move(snapshot),
							start
						](V2_float picked) mutable {
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
						}
					},
					reference,
					false
				);

				if constexpr (!CanPickLocalPosition<Target>()) {
					DrawTooltip("Position picking is available for scene entities.");
				}
			}

			return local_changed;
		})
	};

	if (changed && data.line.GetDirection().IsZero()) {
		position = previous;
		return false;
	}

	return changed;
}

template <typename Target>
bool DrawSliderValueTextConfig(
	Target& target,
	::ptgn::impl::SliderData& data
) {
	bool changed{ false };
	bool enabled{ data.value_text.has_value() };

	if (
		DrawOptionalLabelRow(
			"Value Text",
			enabled,
			false
		)
	) {
		if (enabled) {
			data.value_text = SliderValueTextConfig{};
		} else {
			data.value_text.reset();
		}

		changed = true;
	}

	if (!data.value_text.has_value()) {
		return changed;
	}

	ScopedIndent indent;
	ScopedPropertyLabelOffset label_offset{
		ImGui::GetStyle().IndentSpacing
	};

	auto& config{ data.value_text.value() };

	changed |= DrawValue(target.ctx, "Prefix", config.prefix);
	changed |= DrawValue(target.ctx, "Suffix", config.suffix);
	changed |= DrawValue(target.ctx, "Display Min", config.display_min);
	changed |= DrawValue(target.ctx, "Display Max", config.display_max);

	int decimal_places{
		static_cast<int>(
			std::min<std::uint32_t>(
				config.decimal_places,
				9
			)
		)
	};

	if (DrawValue(
			target.ctx,
			"Decimal Places",
			decimal_places,
			FieldOptions{
				.speed = 1.0f,
				.min = 0.0f,
				.max = 9.0f,
				.format = "%d",
				.flags = ImGuiSliderFlags_AlwaysClamp,
			}
		)) {
		config.decimal_places =
			static_cast<std::uint32_t>(
				std::clamp(decimal_places, 0, 9)
			);

		changed = true;
	}

	return changed;
}

template <typename Target>
bool DrawSliderData(
	Target& target,
	::ptgn::impl::SliderData& data
) {
	bool changed{ false };

	const float clamped_value{
		std::clamp(data.value, 0.0f, 1.0f)
	};

	if (data.value != clamped_value) {
		data.value = clamped_value;
		changed = true;
	}

	changed |= DrawValue(
		target.ctx,
		"Value",
		data.value,
		FieldOptions{
			.speed = 0.01f,
			.min = 0.0f,
			.max = 1.0f,
			.format = "%.3f",
			.flags = ImGuiSliderFlags_AlwaysClamp,
		}
	);

	changed |= DrawSliderWorldPosition(
		target,
		data,
		"Start",
		true
	);

	changed |= DrawSliderWorldPosition(
		target,
		data,
		"End",
		false
	);

	bool discrete{
		data.discrete_positions >= 2
	};

	if (DrawValue(target.ctx, "Discrete", discrete)) {
		data.discrete_positions = discrete ? 2u : 0u;
		changed = true;
	}

	if (discrete) {
		int positions{
			static_cast<int>(
				std::min<std::uint32_t>(
					std::max<std::uint32_t>(
						data.discrete_positions,
						2
					),
					1000
				)
			)
		};

		if (DrawValue(
				target.ctx,
				"Positions",
				positions,
				FieldOptions{
					.speed = 1.0f,
					.min = 2.0f,
					.max = 1000.0f,
					.format = "%d",
					.flags = ImGuiSliderFlags_AlwaysClamp,
				}
			)) {
			data.discrete_positions =
				static_cast<std::uint32_t>(
					std::clamp(positions, 2, 1000)
				);

			changed = true;
		}
	} else if (data.discrete_positions != 0) {
		data.discrete_positions = 0;
		changed = true;
	}

	changed |= DrawSliderValueTextConfig(
		target,
		data
	);

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

			if constexpr (requires { target.entity; }) {
				Entity entity{ target.entity };
				const V2_float start{
					entity && entity.Has<Transform>()
						? GetWorldTransform(entity).position
						: V2_float{}
				};

				data.line = Line{
					start,
					start + V2_float{ 100.0f, 0.0f }
				};
			} else {
				data.line = Line{
					V2_float{},
					V2_float{ 100.0f, 0.0f }
				};
			}

			target.template SetLive<SliderData>(data);
		} else {
			target.template SetLive<SliderData>(std::nullopt);
		}

		changed = true;
	}

	ImGui::SameLine();

	SliderData data{ target.template Capture<SliderData>().value_or(SliderData{}) };
	const bool open{
		ImGui::TreeNodeEx(
			"Slider##Tree",
			ImGuiTreeNodeFlags_SpanAvailWidth
		)
	};

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
		target,
		enabled ? "Edit Slider" : "Disable Slider",
		std::move(before),
		std::move(after),
		changed
	);

	return changed;
}

template <typename Target>
bool DrawUIFeature(Target& target) {
	if (!HasUIFeature(target)) {
		return false;
	}

	if (const auto child_info{ GetButtonChildInfo(target) }) {
		const bool open{
			ImGui::CollapsingHeader(
				"UI##ButtonChildUI",
				ImGuiTreeNodeFlags_None
			)
		};

		if (!open) {
			return false;
		}

		ScopedIndent feature_indent;
		auto& editor_state{
			GetManualFeatureState(target.GetFeatureTargetKey())
		};
		DrawButtonVisualStateSelector(
			editor_state.button_visual_state
		);

		if (editor_state.button_visual_state) {
			DrawDisabledWrappedText(
				"Transform and Visual edit the selected state. Unset values inherit from fallback states."
			);
		} else {
			DrawDisabledWrappedText(
				"Transform and Visual edit the base child entity."
			);
		}

		return false;
	}

	const auto header{ DrawFeatureHeader(
		target, InspectorFeature::UI, "UI", ImGuiTreeNodeFlags_None, UIFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;

	bool changed{ header.changed };

	changed |= DrawOptionalReflected<Target, ::ptgn::impl::ButtonData>(target, "Button", true);

	changed |= DrawOptionalReflected<Target, ::ptgn::impl::ToggleButtonData>(
		target, "Toggle Button", true
	);
	changed |= DrawOptionalReflected<Target, ::ptgn::impl::ToggleButtonGroupData>(
		target, "Toggle Group", true
	);
	changed |= DrawOptionalReflected<Target, ::ptgn::impl::ToggleButtonGroupItem>(
		target, "Toggle Group Item", true
	);
	changed |= DrawOptionalReflected<Target, ::ptgn::impl::DropdownData>(target, "Dropdown", true);
	changed |=
		DrawOptionalReflected<Target, ::ptgn::impl::DropdownItem>(target, "Dropdown Item", true);

	changed |= DrawOptionalSlider(target);

	changed |= DrawOptionalReflected<Target, ::ptgn::impl::TooltipData>(target, "Tooltip", true);
	changed |= DrawOptionalReflected<Target, ::ptgn::impl::TooltipHoverData>(
		target, "Tooltip Hover", true
	);

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
			UUID uuid;
			std::string label;
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
		const bool parent_target_read_only{
			camera_entity == main_camera || camera_entity == fixed_camera
		};

		std::string scene_target_label{ "Scene Target" };
		if (scene_target && scene_target.Has<Tag>()) {
			scene_target_label = std::string{ scene_target.Get<Tag>() };
		}

		std::vector<RenderTargetOption> render_targets;
		for (auto [entity, _target] :
			 scene.EntitiesWith<::ptgn::impl::RenderTargetDesc>()) {
			if (!entity || entity == scene_target || !entity.Has<Tag, UUID>()) {
				continue;
			}

			const UUID uuid{ entity.Get<UUID>() };
			std::string label{ std::string{ entity.Get<Tag>() } };
			label += " [";
			label += uuid_text(uuid);
			label += "]";

			render_targets.push_back(RenderTargetOption{
				.uuid = uuid,
				.label = std::move(label),
			});
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
					render_targets,
					value->render_target,
					&RenderTargetOption::uuid
				) };

				if (selected != render_targets.end()) {
					preview = selected->label.c_str();
				} else {
					missing_preview = "Missing Render Target [";
					missing_preview += uuid_text(value->render_target);
					missing_preview += "]";
					preview = missing_preview.c_str();
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
			target,
			"Edit Parent Render Target",
			std::move(before),
			std::move(after),
			changed
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
		target,
		InspectorFeature::Camera,
		"Camera",
		ImGuiTreeNodeFlags_None,
		CameraFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;
	AutoLabelWidthScope camera_label_width{ "CameraFeatureFields" };

	bool changed{ header.changed };
	changed |= DrawCameraParentRenderTarget(target);
	changed |= DrawRequiredComponent<
		Target,
		::ptgn::impl::CameraData
	>(
		target,
		"Camera",
		false,
		[&target](::ptgn::impl::CameraData& value) {
			return DrawRegisteredComponentContents(
				target.ctx, Hash<::ptgn::impl::CameraData>(), std::addressof(value)
			);
		}
	);
	changed |= DrawRequiredComponent<
		Target,
		::ptgn::impl::CameraMask
	>(
		target,
		"Layers",
		false,
		[](::ptgn::impl::CameraMask& value) {
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
	json value;
	json sequence;
	std::function<std::unique_ptr<Script>()> runtime_factory;
	std::vector<std::function<std::unique_ptr<Script>()>> step_runtime_factories;
	std::vector<std::function<std::unique_ptr<Script>()>> lifecycle_runtime_factories;
};

using ScriptsEditorSnapshot = std::vector<ScriptEntryEditorSnapshot>;

[[nodiscard]] ScriptsEditorSnapshot CaptureScriptsEditorSnapshot(
	const ::ptgn::impl::Scripts& scripts
) {
	ScriptsEditorSnapshot snapshot;
	snapshot.reserve(scripts.scripts.size());

	for (const auto& entry : scripts.scripts) {
		const ScriptSequence& sequence{
			entry.instance ? entry.instance->sequence : entry.sequence
		};

		ScriptEntryEditorSnapshot entry_snapshot{
			.enabled = entry.enabled,
			.type_hash = entry.type_hash,
			.value = entry.value,
			.sequence = sequence,
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

void RestoreScriptsEditorSnapshot(Entity entity, const ScriptsEditorSnapshot& snapshot) {
	if (!entity) {
		return;
	}

	std::vector<ScriptEntry> entries;
	entries.reserve(snapshot.size());

	for (const auto& entry_snapshot : snapshot) {
		ScriptSequence sequence;
		entry_snapshot.sequence.get_to(sequence);

		for (std::size_t i{ 0 };
			 i < sequence.steps.size() && i < entry_snapshot.step_runtime_factories.size(); ++i) {
			sequence.steps[i].runtime_factory = entry_snapshot.step_runtime_factories[i];
		}

		for (std::size_t i{ 0 };
			 i < sequence.lifecycle_actions.size() &&
			 i < entry_snapshot.lifecycle_runtime_factories.size(); ++i) {
			sequence.lifecycle_actions[i].action.runtime_factory =
				entry_snapshot.lifecycle_runtime_factories[i];
		}

		ScriptEntry entry;
		entry.enabled = entry_snapshot.enabled;
		entry.type_hash = entry_snapshot.type_hash;
		entry.value = entry_snapshot.value;
		entry.sequence = std::move(sequence);
		entry.runtime_factory = entry_snapshot.runtime_factory;
		entries.push_back(std::move(entry));
	}

	auto& scripts{ entity.TryAdd<::ptgn::impl::Scripts>() };
	scripts.scripts = std::move(entries);
	scripts.channels.clear();
	scripts.pending_additions.clear();
	scripts.pending_removals.clear();
	scripts.Attach(entity);
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
		auto before{ CaptureScriptsEditorSnapshot(scripts) };
		const bool scripts_changed{ DrawScriptsComponent(target.ctx, scripts) };

		if (!scripts_changed) {
			return changed;
		}

		auto after{ CaptureScriptsEditorSnapshot(scripts) };
		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<::ptgn::impl::Scripts>()) };
		const ImGuiID key{ ImGui::GetID("##ComponentEdit") };
		Editor* editor{ std::addressof(target.ctx.editor) };
		const EntityReference reference{ MakeEntityReference(target.entity) };

		TrackUndoableInteraction(
			target.ctx,
			key,
			"Edit Scripts",
			true,
			[editor, reference, before = std::move(before)]() {
				Entity entity{ reference.Resolve(*editor) };
				RestoreScriptsEditorSnapshot(entity, before);
			},
			[editor, reference, after = std::move(after)]() {
				Entity entity{ reference.Resolve(*editor) };
				RestoreScriptsEditorSnapshot(entity, after);
			}
		);

		changed = true;
	} else {
		changed |= DrawRequiredComponent<Target, ::ptgn::impl::Scripts>(
			target, "Scripts", false,
			[&target](::ptgn::impl::Scripts& value) {
				return DrawRegisteredComponentContents(
					target.ctx, Hash<::ptgn::impl::Scripts>(), std::addressof(value)
				);
			}
		);
	}

	return changed;
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

	changed |= DrawOptionalReflected<Target, Lifetime>(target, "Lifetime", true);

	return changed;
}

template <typename Target, typename... T>
[[nodiscard]] consteval bool SupportsAnyFeatureComponent(FeatureComponents<T...>) {
	return (Target::template Supports<T>() || ...);
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

	auto item_without_default = [&]<typename... T>(
									InspectorFeature feature, const char* label,
									bool feature_exists, FeatureComponents<T...> components
								) {
		if constexpr (!SupportsAnyFeatureComponent<Target>(components)) {
			return;
		}

		ScopedDisabled disabled{ feature_exists };

		if (ImGui::MenuItem(label)) {
			changed |= AddInspectorFeature(target, feature, label, components);
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
		InspectorFeature::Interaction, "Interaction", HasInteractionFeature(target),
		InteractionFeatureComponents{}
	);
	item_with_default.template operator()<Collider>(
		InspectorFeature::Physics, "Physics & Movement", HasPhysicsFeature(target),
		PhysicsFeatureComponents{}
	);
	item_with_default.template operator()<::ptgn::impl::ButtonData>(
		InspectorFeature::UI, "UI", HasUIFeature(target), UIFeatureComponents{}
	);
	if (!primary_render_target && !visual_exists) {
		item_with_default.template operator()<::ptgn::impl::CameraData>(
			InspectorFeature::Camera, "Camera", camera_exists, CameraFeatureComponents{}
		);
	}
	item_with_default.template operator()<::ptgn::impl::Scripts>(
		InspectorFeature::Scripts, "Scripts", HasScriptsFeature(target), ScriptsFeatureComponents{}
	);
	item_without_default(
		InspectorFeature::Utilities, "Utilities", HasUtilitiesFeature(target),
		UtilitiesFeatureComponents{}
	);

	ImGui::EndPopup();
	return changed;
}

template <typename Target>
bool DrawFeatureInspectorImpl(Target& target) {
	bool changed{ false };
	changed |= DrawTransformFeature(target);
	changed |= DrawVisualFeature(target);
	changed |= DrawInteractionFeature(target);
	changed |= DrawPhysicsFeature(target);
	changed |= DrawUIFeature(target);
	changed |= DrawCameraFeature(target);
	changed |= DrawScriptsFeature(target);
	changed |= DrawUtilitiesFeature(target);

	ImGui::Separator();
	changed |= DrawAddFeatureMenu(target);
	return changed;
}

} // namespace

bool DrawFeatureInspector(EntityInspectorTarget& target) {
	return DrawFeatureInspectorImpl(target);
}

bool DrawFeatureInspector(PrefabInspectorTarget& target) {
	return DrawFeatureInspectorImpl(target);
}

} // namespace ptgn::editor::inspector
