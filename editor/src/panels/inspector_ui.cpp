#include "panels/inspector_features.h"
#include "panels/inspector_geometry.h"
#include "panels/rich_text_editor.h"

namespace ptgn::editor::inspector {

namespace {

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

void ApplyButtonPreview(Entity entity, ButtonVisualState state, EditorContext& ctx) {
	if (!entity || !entity.Has<::ptgn::impl::ButtonData>() || entity.GetScene().IsRuntime()) {
		ClearButtonPreviewIfDifferent(ctx);
		return;
	}

	ClearButtonPreviewIfDifferent(ctx, entity);
	Button{ entity }.PreviewVisualState(state);
	PreviewedButtonReference() = MakeEntityReference(entity);
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

			changed |= DrawValue(
				target.ctx,
				"State Exclusive",
				sounds.state_exclusive[index]
			);
			DrawTooltip(
				"Stop this state's sound before replaying it."
			);

			changed |= DrawValue(
				target.ctx,
				"Global Exclusive",
				sounds.exclusive
			);
			DrawTooltip(
				"Stop sounds from other button states before playing this state's sound."
			);

			ImGui::TreePop();
		}

		if (remove_requested) {
			sound.reset();
			sounds.state_exclusive[index] = false;
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
		const auto destination_index{
			static_cast<std::size_t>(std::to_underlying(destination))
		};

		if (source) {
			const auto source_index{
				static_cast<std::size_t>(std::to_underlying(*source))
			};

			after.sounds->states[destination_index] =
				after.sounds->states[source_index];

			after.sounds->state_exclusive[destination_index] =
				after.sounds->state_exclusive[source_index];
		} else {
			after.sounds->states[destination_index].reset();
			after.sounds->state_exclusive[destination_index] = false;
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
bool DrawUIFeatureImpl(Target& target) {
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


} // namespace

bool DrawUIFeature(EntityInspectorTarget& target) {
	return DrawUIFeatureImpl(target);
}

bool DrawUIFeature(PrefabInspectorTarget& target) {
	return DrawUIFeatureImpl(target);
}

} // namespace ptgn::editor::inspector
