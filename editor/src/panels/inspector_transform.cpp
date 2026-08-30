#include "panels/inspector_features.h"

namespace ptgn::editor::inspector {

namespace {

template <typename Target>
struct TransformFeatureState {
	Transform transform{};
	Depth depth{};
	ComponentState<ButtonBackgroundVisuals> button_backgrounds{};
	ComponentState<ButtonBorderVisuals> button_borders{};
	ComponentState<ButtonTextVisuals> button_texts{};
	ComponentState<ButtonSpriteVisuals> button_sprites{};
	ComponentState<::ptgn::impl::SliderTrackBackgroundData> slider_track_background{};
	ComponentState<::ptgn::impl::SliderTrackBorderData> slider_track_border{};
	ComponentState<::ptgn::impl::SliderTrackSpriteData> slider_track_sprite{};
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
		.slider_track_background =
			target.template Capture<::ptgn::impl::SliderTrackBackgroundData>(),
		.slider_track_border = target.template Capture<::ptgn::impl::SliderTrackBorderData>(),
		.slider_track_sprite = target.template Capture<::ptgn::impl::SliderTrackSpriteData>(),
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
	auto apply_slider_track_background{
		target.template MakeApply<::ptgn::impl::SliderTrackBackgroundData>()
	};
	auto apply_slider_track_border{
		target.template MakeApply<::ptgn::impl::SliderTrackBorderData>()
	};
	auto apply_slider_track_sprite{
		target.template MakeApply<::ptgn::impl::SliderTrackSpriteData>()
	};

	return [apply_transform, apply_depth, apply_position, apply_rotation, apply_scale,
			apply_depth_ignore, apply_transform_ignore, apply_backgrounds, apply_borders,
			apply_texts, apply_sprites, apply_slider_track_background, apply_slider_track_border,
			apply_slider_track_sprite](TransformFeatureState<Target> state) mutable {
		apply_backgrounds(state.button_backgrounds);
		apply_borders(state.button_borders);
		apply_texts(state.button_texts);
		apply_sprites(state.button_sprites);
		apply_slider_track_background(state.slider_track_background);
		apply_slider_track_border(state.slider_track_border);
		apply_slider_track_sprite(state.slider_track_sprite);
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
	target.template SetLive<::ptgn::impl::SliderTrackBackgroundData>(state.slider_track_background);
	target.template SetLive<::ptgn::impl::SliderTrackBorderData>(state.slider_track_border);
	target.template SetLive<::ptgn::impl::SliderTrackSpriteData>(state.slider_track_sprite);
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

template <typename Marker>
void ApplySliderTrackVisualTransformDelta(
	ComponentState<Marker>& marker, const Transform& before, const Transform& after
) {
	if (!marker || before == after) {
		return;
	}

	marker->initialized = true;
	marker->visual.defined = true;
	if (!marker->visual.transform.has_value()) {
		marker->visual.transform = Transform{};
	}
	auto& transform{ marker->visual.transform.value() };

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
void ApplySliderTrackVisualTransformDelta(
	TransformFeatureState<Target>& state, const Transform& before
) {
	ApplySliderTrackVisualTransformDelta(
		state.slider_track_background, before, state.transform
	);
	ApplySliderTrackVisualTransformDelta(state.slider_track_border, before, state.transform);
	ApplySliderTrackVisualTransformDelta(state.slider_track_sprite, before, state.transform);
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
				ApplySliderTrackVisualTransformDelta(state, before_transform);
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


template <typename Target>
bool DrawTransformFeatureImpl(
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
				changed |= DrawTransformFeatureImpl(target, false, false, false);
				editor_state.button_visual_state = previous_state;
			}
			ImGui::TreePop();
		}

		return changed;
	}
}

template <typename Target>
bool DrawButtonChildStateTransformFeatureImpl(
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
bool DrawTransformFeatureImpl(
	Target& target, bool draw_header, bool redirect_button_part, bool draw_inline_separator
) {
	if (redirect_button_part) {
		if (const auto child_info{ GetButtonChildInfo(target) }) {
			if (const auto state{ GetButtonVisualEditState(target) }) {
				return DrawButtonChildStateTransformFeatureImpl(target, *child_info, *state);
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
		ApplySliderTrackVisualTransformDelta(state, before.transform);
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


} // namespace

bool DrawTransformFeature(
	EntityInspectorTarget& target, bool draw_header, bool redirect_button_part,
	bool draw_inline_separator
) {
	return DrawTransformFeatureImpl(
		target, draw_header, redirect_button_part, draw_inline_separator
	);
}

bool DrawTransformFeature(
	PrefabInspectorTarget& target, bool draw_header, bool redirect_button_part,
	bool draw_inline_separator
) {
	return DrawTransformFeatureImpl(
		target, draw_header, redirect_button_part, draw_inline_separator
	);
}


bool DrawButtonChildStateTransformFeature(
	EntityInspectorTarget& target, const ButtonChildInfo& child_info, ButtonVisualState state
) {
	return DrawButtonChildStateTransformFeatureImpl(target, child_info, state);
}

} // namespace ptgn::editor::inspector
