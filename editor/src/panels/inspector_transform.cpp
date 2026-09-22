#include "panels/inspector_archetype_inspector.h"

namespace ptgn::editor::inspector {

namespace {

template <typename Target>
struct TransformSectionState {
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
TransformSectionState<Target> CaptureTransformSection(const Target& target) {
	const bool ignore_transform{
		target.template Capture<::ptgn::impl::IgnoreParentTransform>().has_value()
	};

	return TransformSectionState<Target>{
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
auto MakeTransformSectionApply(Target& target) {
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
			apply_slider_track_sprite](TransformSectionState<Target> state) mutable {
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
void SetTransformSectionLive(Target& target, const TransformSectionState<Target>& state) {
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
	const Transform& before, const Transform& after, const Depth& depth,
	bool ignore_position, bool ignore_rotation, bool ignore_scale, bool ignore_depth
) {
	if (!visuals || !selected_state) {
		return;
	}

	const auto index{ static_cast<std::size_t>(std::to_underlying(*selected_state)) };
	auto& visual{ visuals->states[index] };

	if (!visual.transform) {
		return;
	}

	if (before != after) {
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

	visual.depth = depth.value;
	visual.inherit_position = !ignore_position;
	visual.inherit_rotation = !ignore_rotation;
	visual.inherit_scale = !ignore_scale;
	visual.inherit_depth = !ignore_depth;
}

template <typename Target>
void ApplyButtonVisualTransformDelta(
	TransformSectionState<Target>& state, std::optional<ButtonVisualState> selected_state,
	const Transform& before
) {
	auto apply = [&](auto& visuals) {
		ApplyButtonVisualTransformDelta(
			visuals, selected_state, before, state.transform, state.depth,
			state.ignore_position, state.ignore_rotation, state.ignore_scale, state.ignore_depth
		);
	};
	apply(state.button_backgrounds);
	apply(state.button_borders);
	apply(state.button_texts);
	apply(state.button_sprites);
}

template <typename Marker>
void ApplySliderTrackVisualTransformDelta(
	ComponentState<Marker>& marker, const Transform& before, const Transform& after,
	const Depth& depth, bool ignore_position, bool ignore_rotation, bool ignore_scale,
	bool ignore_depth
) {
	if (!marker) {
		return;
	}

	marker->initialized = true;
	marker->visual.defined = true;
	if (!marker->visual.transform.has_value()) {
		marker->visual.transform = Transform{};
	}
	if (before != after) {
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
	marker->visual.depth = depth.value;
	marker->visual.inherit_position = !ignore_position;
	marker->visual.inherit_rotation = !ignore_rotation;
	marker->visual.inherit_scale = !ignore_scale;
	marker->visual.inherit_depth = !ignore_depth;
}

template <typename Target>
void ApplySliderTrackVisualTransformDelta(
	TransformSectionState<Target>& state, const Transform& before
) {
	auto apply = [&](auto& marker) {
		ApplySliderTrackVisualTransformDelta(
			marker, before, state.transform, state.depth, state.ignore_position,
			state.ignore_rotation, state.ignore_scale, state.ignore_depth
		);
	};
	apply(state.slider_track_background);
	apply(state.slider_track_border);
	apply(state.slider_track_sprite);
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
bool DrawTransformSectionFields(
	Target& target, TransformSectionState<Target>& state, Apply apply_state
) {
	bool changed{ false };
	auto& editor_state{ GetInspectorUiState(target.GetInspectorTargetKey()) };
	const auto selected_button_state{ GetButtonVisualEditState(target) };

	auto draw_ignore = [&](bool& value, const char* tooltip) {
		const bool local_changed{ ImGui::Checkbox("##IgnoreParent", &value) };
		DrawTooltip(tooltip);
		return local_changed;
	};

	changed |= DrawPropertyRow("Position", [&]() {
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
		const float pick_width{ ImGui::CalcTextSize("Pick").x +
			ImGui::GetStyle().FramePadding.x * 2.0f };
		const float checkbox_width{ ImGui::GetFrameHeight() };
		const float actions_width{ pick_width + checkbox_width + spacing };
		const bool actions_inline{ available >= actions_width + spacing + 104.0f };
		const float fields_width{
			actions_inline ? std::max(1.0f, available - actions_width - spacing) : available
		};
		const float field_width{ InspectorSplitWidth(2, fields_width, spacing) };
		bool local_changed{ false };

		ImGui::SetNextItemWidth(field_width);
		local_changed |= ImGui::DragFloat(
			"##X", &state.transform.position.x, 1.0f, 0.0f, 0.0f, "X: %.0f"
		);
		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(field_width);
		local_changed |= ImGui::DragFloat(
			"##Y", &state.transform.position.y, 1.0f, 0.0f, 0.0f, "Y: %.0f"
		);

		if (actions_inline) {
			ImGui::SameLine(0.0f, spacing);
		}
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
		ImGui::SameLine(0.0f, spacing);
		local_changed |= draw_ignore(state.ignore_position, "Ignore parent position.");
		return local_changed;
	});

	changed |= DrawPropertyRow("Depth", [&]() {
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
		const float checkbox_width{ ImGui::GetFrameHeight() };
		const bool inline_ignore{ available >= checkbox_width + spacing + 72.0f };
		ImGui::SetNextItemWidth(
			inline_ignore ? std::max(1.0f, available - checkbox_width - spacing) : -FLT_MIN
		);
		bool local_changed{ ImGui::DragFloat(
			"##Value", &state.depth.value, 0.05f, -1000.0f, 1000.0f, "%.2f",
			ImGuiSliderFlags_AlwaysClamp
		) };
		if (inline_ignore) {
			ImGui::SameLine(0.0f, spacing);
		}
		local_changed |= draw_ignore(state.ignore_depth, "Ignore parent depth.");
		return local_changed;
	});

	changed |= DrawPropertyRow("Rotation", [&]() {
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
		const float checkbox_width{ ImGui::GetFrameHeight() };
		const bool inline_ignore{ available >= checkbox_width + spacing + 72.0f };
		ImGui::SetNextItemWidth(
			inline_ignore ? std::max(1.0f, available - checkbox_width - spacing) : -FLT_MIN
		);

		Degrees rotation{ state.transform.rotation };
		float degrees{ rotation.value };
		bool local_changed{ false };
		if (ImGui::DragFloat(
				"##Value", &degrees, 1.0f, 0.0f, 360.0f, "%.1f deg",
				ImGuiSliderFlags_AlwaysClamp
			)) {
			state.transform.rotation = Radians{ Degrees{ degrees } };
			local_changed = true;
		}
		if (inline_ignore) {
			ImGui::SameLine(0.0f, spacing);
		}
		local_changed |= draw_ignore(state.ignore_rotation, "Ignore parent rotation.");
		return local_changed;
	});

	changed |= DrawPropertyRow("Scale", [&]() {
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
		const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
		const float action_width{ ImGui::GetFrameHeight() * 2.0f + spacing };
		const bool actions_inline{ available >= action_width + spacing + 104.0f };
		const float fields_width{
			actions_inline ? std::max(1.0f, available - action_width - spacing) : available
		};
		const float field_width{ InspectorSplitWidth(2, fields_width, spacing) };

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

		if (actions_inline) {
			ImGui::SameLine(0.0f, spacing);
		}
		ImGui::Checkbox("##LockRatio", &editor_state.scale_ratio_locked);
		DrawTooltip(
			"Lock the scale ratio. Editing either axis changes the other by the same proportional "
			"factor."
		);
		ImGui::SameLine(0.0f, spacing);
		bool local_changed{ x_changed || y_changed };
		local_changed |= draw_ignore(state.ignore_scale, "Ignore parent scale.");
		return local_changed;
	});

	return changed;
}


template <typename Target>
bool DrawTransformSectionImpl(
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

		bool enabled{ visual.transform.has_value() };
		return DrawOptionalTransformTree(
			"ButtonVisualStateTransform", enabled,
			[&](bool transform_enabled) {
				if (transform_enabled) {
					visual.transform = resolved_transform;
					visual.depth = target.template Capture<Depth>().value_or(Depth{}).value;
					visual.inherit_position =
						!target.template Capture<::ptgn::impl::IgnoreParentPosition>().has_value();
					visual.inherit_rotation =
						!target.template Capture<::ptgn::impl::IgnoreParentRotation>().has_value();
					visual.inherit_scale =
						!target.template Capture<::ptgn::impl::IgnoreParentScale>().has_value();
					visual.inherit_depth =
						!target.template Capture<::ptgn::impl::IgnoreParentDepth>().has_value();
				} else {
					visual.transform.reset();
					visual.depth.reset();
					visual.inherit_position.reset();
					visual.inherit_rotation.reset();
					visual.inherit_scale.reset();
					visual.inherit_depth.reset();
				}

				// Transform is only an optional property of an enabled part. Toggling it must not
				// toggle the part itself.
				visual.defined = true;
				target.template SetLive<Visuals>(ComponentState<Visuals>{ visuals }, callback);
				auto after{ target.template Capture<Visuals>() };
				TrackComponentState(
					target, std::string{ transform_enabled ? "Enable " : "Disable " } +
						std::string{ part_label } + " Transform",
					std::move(before), std::move(after), true, callback
				);
				return true;
			},
			[&]() {
				// Reuse the ordinary Transform section so managed button parts get the exact same
				// position picker, scale-ratio lock, depth, and ignore-parent controls as entities.
				auto& editor_state{ GetInspectorUiState(target.GetInspectorTargetKey()) };
				const auto previous_state{ editor_state.button_visual_state };
				editor_state.button_visual_state = state;
				const bool transform_changed{
					DrawTransformSectionImpl(target, false, false, false)
				};
				editor_state.button_visual_state = previous_state;
				return transform_changed;
			},
			"Override this state's transform, or leave it unchecked to inherit."
		);
	}
}

template <typename Target>
bool DrawButtonChildStateTransformSectionImpl(
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
bool DrawTransformSectionImpl(
	Target& target, bool draw_header, bool redirect_button_part, bool draw_inline_separator
) {
	if (redirect_button_part) {
		if (const auto child_info{ GetButtonChildInfo(target) }) {
			if (const auto state{ GetButtonVisualEditState(target) }) {
				return DrawButtonChildStateTransformSectionImpl(target, *child_info, *state);
			}
		}
	}

	if (!HasTransformSection(target)) {
		return false;
	}

	InspectorSectionResult header{ .open = true };

	if (draw_header) {
		const bool required_by_archetype{
			ArchetypeRequiresTransform(ResolveInspectorArchetype(target))
		};
		header = DrawInspectorSectionHeader(
			"Transform", "TransformSection",
			InspectorSectionOptions{
				.default_open = true,
				.removable = !required_by_archetype,
			}
		);
		if (header.remove_requested) {
			return RemoveComponentSet(target, "Transform", TransformSectionComponents{});
		}
		if (!header.open) {
			return false;
		}
	} else if (draw_inline_separator) {
		ImGui::SeparatorText("Transform");
	}

	std::optional<ScopedIndent> section_indent;
	if (draw_header) {
		section_indent.emplace();
	}

	const auto before{ CaptureTransformSection(target) };
	auto state{ before };
	auto apply{ MakeTransformSectionApply(target) };
	bool changed{ false };

	changed |= DrawTransformSectionFields(target, state, apply);

	if (changed) {
		state.ignore_transform = false;
		state.transform.ClampScale();
		ApplyButtonVisualTransformDelta(state, GetButtonVisualEditState(target), before.transform);
		ApplySliderTrackVisualTransformDelta(state, before.transform);
		SetTransformSectionLive(target, state);
	}

	if (changed) {
		ScopedID target_scope{ target.Id() };
		const ImGuiID key{ ImGui::GetID("##TransformSection") };

		TrackUndoableInteraction(
			target.ctx, key, "Edit Transform", true, [apply, before]() mutable { apply(before); },
			[apply, state]() mutable { apply(state); }
		);
	}

	return changed;
}


} // namespace

bool DrawTransformSection(
	EntityInspectorTarget& target, bool draw_header, bool redirect_button_part,
	bool draw_inline_separator
) {
	return DrawTransformSectionImpl(
		target, draw_header, redirect_button_part, draw_inline_separator
	);
}

bool DrawTransformSection(
	PrefabInspectorTarget& target, bool draw_header, bool redirect_button_part,
	bool draw_inline_separator
) {
	return DrawTransformSectionImpl(
		target, draw_header, redirect_button_part, draw_inline_separator
	);
}


bool DrawButtonChildStateTransformSection(
	EntityInspectorTarget& target, const ButtonChildInfo& child_info, ButtonVisualState state
) {
	return DrawButtonChildStateTransformSectionImpl(target, child_info, state);
}

} // namespace ptgn::editor::inspector
