#include "panels/inspector_archetype_inspector.h"
#include "panels/inspector_geometry.h"
#include "panels/rich_text_editor.h"
#include "editor/renamable_item.h"

#include <imgui_internal.h>


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
		const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
		const bool pick_inline{ available >= pick_width + spacing * 2.0f + 96.0f };
		const float fields_width{
			pick_inline ? std::max(1.0f, available - pick_width - spacing) : available
		};
		const float field_width{ InspectorSplitWidth(2, fields_width, spacing) };
		bool local_changed{ false };

		ImGui::SetNextItemWidth(field_width);
		local_changed |= ImGui::DragFloat("##X", &position.x, 0.01f, 0.0f, 0.0f, "X: %.2f");
		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(field_width);
		local_changed |= ImGui::DragFloat("##Y", &position.y, 0.01f, 0.0f, 0.0f, "Y: %.2f");
		if (pick_inline) {
			ImGui::SameLine(0.0f, spacing);
		}

		ScopedDisabled disabled{ !CanPickLocalPosition<Target>() };
		auto apply{ target.template MakeApply<::ptgn::impl::SliderData>() };
		::ptgn::impl::SliderData snapshot{ data };
		PositionPicker::Convert convert{};
		std::optional<V2_float> reference_world{};

		if constexpr (requires { target.entity; }) {
			Entity slider_entity{ target.entity };
			if (slider_entity) {
				// Slider endpoints are authored in the slider entity's local space, regardless of
				// whether the optional track transform is enabled.
				const Transform basis_world{ GetWorldTransform(slider_entity) };
				reference_world = basis_world.Apply(position);
				convert = [slider_entity](V2_float world) -> std::optional<V2_float> {
					if (!slider_entity) {
						return std::nullopt;
					}
					return GetWorldTransform(slider_entity).ApplyInverse(world);
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
	bool config_changed{ false };

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

	Entity text_entity{};
	std::optional<TextBox> preview_box{};
	if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
		text_entity = target.entity ? Slider{ target.entity }.GetValueTextEntity() : Entity{};
		if (text_entity && text_entity.Has<::ptgn::impl::TextData>()) {
			preview_box = text_entity.Get<::ptgn::impl::TextData>().box;
		}
	}

	const bool text_open{ ImGui::TreeNodeEx(
		"Text##SliderValueTextText",
		ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
	) };
	if (text_open) {
		ScopedIndent indent;
		config_changed |= DrawRichTextEditor(
			target.ctx, config.text.source, config.text.defaults,
			RichTextEditorOptions{
				.variables = variables,
				.preview_box = preview_box ? std::addressof(*preview_box) : nullptr,
			}
		);
		ImGui::TreePop();
	}

	const bool format_open{ ImGui::TreeNodeEx(
		"Value Format##SliderValueTextFormat",
		ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
	) };
	if (format_open) {
		ScopedIndent indent;
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

	if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
		if (text_entity && text_entity.Has<::ptgn::impl::SliderValueTextData>()) {
			bool transform_enabled{
				text_entity.Get<::ptgn::impl::SliderValueTextData>().transform_enabled
			};

			ScopedID transform_scope{ "SliderValueTextTransform" };
			const auto transform_header{ DrawInspectorTreeToggleRow(
				"Transform", "##SliderValueTextTransformTree", transform_enabled
			) };
			const bool transform_open{ transform_header.open };
			const bool transform_toggle_changed{ transform_header.toggle_changed };
			if (transform_toggle_changed) {
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
			DrawTooltip("Use an editable transform for the value text.");

			if (transform_open) {
				ScopedIndent indent;
				ScopedDisabled disabled{ !transform_enabled };
				EntityInspectorTarget text_target{ .ctx = target.ctx, .entity = text_entity };
				changed |= DrawTransformSection(text_target, false, true, false);
			}
		} else {
			ImGui::TextDisabled("Value text entity is not available.");
		}
	}

	const bool layout_open{ ImGui::TreeNodeEx(
		"Layout##SliderValueTextLayout",
		ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
	) };
	if (layout_open) {
		ScopedIndent indent;
		if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
			if (text_entity) {
				EntityInspectorTarget text_target{ .ctx = target.ctx, .entity = text_entity };
				auto text_before{ text_target.Capture<::ptgn::impl::TextData>() };
				if (text_before) {
					auto text_data{ *text_before };
					bool text_changed{ false };

					const bool rectangle_open{ ImGui::TreeNodeEx(
						"Text Rectangle##SliderValueTextRectangle",
						ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
					) };
					if (rectangle_open) {
						ScopedIndent rectangle_indent;
						text_changed |= DrawInspectorValueContents(
							text_target.ctx, Hash<Rect>(), std::addressof(text_data.box.rect)
						);
						ImGui::TreePop();
					}

					const bool style_open{ ImGui::TreeNodeEx(
						"Text Style##SliderValueTextStyle",
						ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
					) };
					if (style_open) {
						ScopedIndent style_indent;
						text_changed |= DrawDefaultContents(
							text_target.ctx, text_data.box.style
						);
						ImGui::TreePop();
					}

					text_changed |= DrawValue(
						text_target.ctx, "Reveal Glyph Count", text_data.glyph_count
					);

					{
						ScopedID clip_scope{ "SliderValueTextClip" };
						bool clip_enabled{ text_data.clip.has_value() };
						const auto clip_header{ DrawInspectorTreeToggleRow(
							"Clip", "##SliderValueTextClipTree", clip_enabled, false,
							InspectorTreeToggleSide::Left
						) };
						const bool clip_open{ clip_header.open };
						const bool clip_toggle_changed{ clip_header.toggle_changed };

						if (clip_toggle_changed) {
							text_data.clip = clip_enabled
								? std::optional<TextClip>{ TextClip{} }
								: std::nullopt;
							text_changed = true;
						}

						if (clip_open) {
							ScopedIndent clip_indent;
							TextClip clip{ text_data.clip.value_or(TextClip{}) };
							bool clip_changed{ false };
							{
								ScopedDisabled disabled{ !clip_enabled };
								clip_changed |= DrawValue(text_target.ctx, "Mode", clip.mode);
								clip_changed |= DrawInspectorValueContents(
									text_target.ctx, Hash<Rect>(), std::addressof(clip.rect)
								);
							}
							if (clip_enabled && clip_changed) {
								text_data.clip = clip;
								text_changed = true;
							}
						}
					}

					if (text_changed) {
						text_target.SetLive<::ptgn::impl::TextData>(
							text_data, &MarkTextLayoutDirty
						);
						auto text_after{ text_target.Capture<::ptgn::impl::TextData>() };
						TrackComponentState(
							text_target, "Edit Slider Value Text Layout", std::move(text_before),
							std::move(text_after), true, &MarkTextLayoutDirty
						);
						changed = true;
					}
				}
			} else {
				ImGui::TextDisabled("Value text entity is not available.");
			}
		} else {
			ImGui::TextDisabled("Layout is generated on the runtime value-text entity.");
		}
		ImGui::TreePop();
	}

	if (config_changed) {
		data.value_text = std::move(config);
		changed = true;
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
	int positions{ static_cast<int>(
		std::min<std::uint32_t>(std::max<std::uint32_t>(data.discrete_positions, 2), 1000)
	) };

	changed |= DrawPropertyRow("Discrete", [&]() {
		bool row_changed{ false };
		if (ImGui::Checkbox("##Discrete", &discrete)) {
			data.discrete_positions = discrete ? 2u : 0u;
			positions = discrete ? 2 : positions;
			row_changed = true;
		}
		DrawTooltip("Snap the slider to fixed selectable values.");

		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::BeginDisabled(!discrete);
		if (ImGui::DragInt(
				"##Positions", &positions, 1.0f, 2, 1000, "%d",
				ImGuiSliderFlags_AlwaysClamp
			)) {
			data.discrete_positions = static_cast<std::uint32_t>(std::clamp(positions, 2, 1000));
			row_changed = true;
		}
		ImGui::EndDisabled();
		DrawTooltip("Number of selectable values, including both endpoints.");
		return row_changed;
	});

	if (!discrete && data.discrete_positions != 0) {
		data.discrete_positions = 0;
		changed = true;
	}

	return changed;
}

enum class FocusedUIControlType : std::uint8_t {
	None,
	Button,
	ToggleButton,
	Slider,
	Dropdown,
	Dialogue,
	Conflict,
};

template <typename Target>
bool DrawFocusedButtonAppearance(Target& target, FocusedUIControlType type);

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
	const bool dialogue{ HasTargetComponent<Target, ::ptgn::impl::DialogueData>(target) };
	const bool button{ HasTargetComponent<Target, ::ptgn::impl::ButtonData>(target) };

	const int specialized_count{
		static_cast<int>(slider) +
		static_cast<int>(toggle) +
		static_cast<int>(dropdown)
	};

	if (specialized_count > 1 || (dialogue && (button || specialized_count > 0))) {
		return FocusedUIControlType::Conflict;
	}
	if (dialogue) {
		return FocusedUIControlType::Dialogue;
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
	if (button) {
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
		case FocusedUIControlType::Dialogue:	 return "Dialogue";
		case FocusedUIControlType::Conflict:	 return "Invalid UI Control";
		case FocusedUIControlType::None:		 return "UI";
	}
	return "UI";
}

template <typename Target, typename T, typename Draw, typename Callback = std::nullptr_t>
bool DrawFocusedComponent(
	Target& target, std::string_view label, Draw&& draw, Callback callback = nullptr
) {
	return EditComponent<Target, T>(
		target, std::string{ "Edit " } + std::string{ label },
		std::forward<Draw>(draw), callback
	);
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
	EditorContext& ctx, const InspectorTargetKey& target_key,
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
		std::vector<InspectorChoice> choices;
		choices.reserve(allow_toggled ? 3 : 2);
		choices.push_back(InspectorChoice{
			.label = "Normal",
			.selected = mode == 0,
			.invoke = [&]() { mode = 0; },
		});
		if (allow_toggled) {
			choices.push_back(InspectorChoice{
				.label = "Toggled",
				.selected = mode == 1,
				.invoke = [&]() { mode = 1; },
			});
		}
		choices.push_back(InspectorChoice{
			.label = "Disabled",
			.selected = mode == 2,
			.invoke = [&]() { mode = 2; },
		});
		return DrawInspectorChoiceBar(choices, "##ButtonPreviewMode");
	});

	changed |= DrawPropertyRow("Pointer", [&]() {
		const std::array choices{
			InspectorChoice{
				.label = "Idle",
				.selected = pointer_state == ButtonState::Idle,
				.invoke = [&]() { pointer_state = ButtonState::Idle; },
			},
			InspectorChoice{
				.label = "Hover",
				.selected = pointer_state == ButtonState::Hover,
				.invoke = [&]() { pointer_state = ButtonState::Hover; },
			},
			InspectorChoice{
				.label = "Pressed",
				.selected = pointer_state == ButtonState::Press,
				.invoke = [&]() { pointer_state = ButtonState::Press; },
			},
		};
		return DrawInspectorChoiceBar(choices, "##ButtonPointerState");
	});

	if (!changed) {
		return false;
	}

	const ButtonVisualState updated{ ComposeButtonVisualState(mode, pointer_state) };
	selected_state = updated;
	const std::optional<ButtonVisualState> after{ selected_state };

	ctx.undo.PushApplied(
		"Select UI Appearance",
		[target_key, before]() { GetInspectorUiState(target_key).button_visual_state = before; },
		[target_key, after]() { GetInspectorUiState(target_key).button_visual_state = after; },
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

[[maybe_unused]] bool DrawFocusedButtonAddParts(EditorContext& ctx, Entity button_entity, ButtonVisualState state) {
	const auto state_index{
		static_cast<std::size_t>(std::to_underlying(state))
	};

	std::vector<InspectorAction> actions;
	actions.reserve(5);
	bool changed{ false };

	auto add_part_action = [&](ButtonChildPart part, std::string_view label) {
		if (FindButtonPart(button_entity, part)) {
			return;
		}
		actions.push_back(InspectorAction{
			.label = label,
			.invoke = [&, part]() {
				changed |= static_cast<bool>(
					CreateButtonPartForInspector(ctx, button_entity, part, state)
				);
			},
		});
	};

	add_part_action(ButtonChildPart::Background, "+ Background");
	add_part_action(ButtonChildPart::Border, "+ Border");
	add_part_action(ButtonChildPart::Text, "+ Text");
	add_part_action(ButtonChildPart::Sprite, "+ Sprite");

	bool audio_missing{ true };
	if (button_entity && button_entity.Has<ButtonSounds>()) {
		audio_missing = !button_entity.Get<ButtonSounds>().states[state_index].has_value();
	}
	if (audio_missing) {
		actions.push_back(InspectorAction{
			.label = "+ Audio",
			.invoke = [&]() {
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
					button_target, "Add Button Audio", std::move(before), std::move(after), true
				);
				ButtonAudioCloseOnNextDraw() = true;
				changed = true;
			},
		});
	}

	if (actions.empty()) {
		return false;
	}

	DrawInspectorActionBar(actions, { .id = "ButtonAddParts" });
	ImGui::Spacing();
	return changed;
}

void SynchronizeFocusedButtonVisualPart(Entity child) {
	if (!child || !HasParent(child)) {
		return;
	}

	Entity button_entity{ GetParent(child) };
	if (!button_entity || !button_entity.Has<::ptgn::impl::ButtonData>()) {
		return;
	}

	Button{ button_entity }.RefreshVisualState();
}

template <typename Visuals, typename Draw>
bool DrawFocusedButtonVisualComponent(
	EditorContext& ctx, Entity, Entity child, std::string_view action, Draw&& draw
) {
	EntityInspectorTarget child_target{
		.ctx = ctx,
		.entity = child,
	};
	auto before{ child_target.template Capture<Visuals>() };
	if (!before) {
		return false;
	}

	Visuals visuals{ *before };
	if (!std::invoke(std::forward<Draw>(draw), visuals.states)) {
		return false;
	}

	child_target.template SetLive<Visuals>(visuals, &SynchronizeFocusedButtonVisualPart);
	auto after{ child_target.template Capture<Visuals>() };
	TrackComponentState(
		child_target, std::string{ action }, std::move(before), std::move(after), true,
		&SynchronizeFocusedButtonVisualPart
	);
	return true;
}

bool DrawFocusedButtonPartContents(
	EditorContext& ctx, Entity button_entity, ButtonChildPart part, ButtonVisualState state
) {
	Entity child{ FindButtonPart(button_entity, part) };
	if (!child) {
		return false;
	}

	bool changed{ false };
	switch (part) {
		case ButtonChildPart::Background: {
			EntityInspectorTarget child_target{ .ctx = ctx, .entity = child };
			const ButtonChildInfo info{ .child = child, .button = button_entity, .part = part };
			changed |= DrawButtonChildStateTransformSection(child_target, info, state);
			changed |= DrawFocusedButtonVisualComponent<ButtonBackgroundVisuals>(
				ctx, button_entity, child, "Edit Button Background",
				[&](auto& states) {
					return DrawButtonShapeVisualFields(
						ctx, button_entity, states, state, false, std::nullopt, false
					);
				}
			);
			break;
		}
		case ButtonChildPart::Border: {
			EntityInspectorTarget child_target{ .ctx = ctx, .entity = child };
			const ButtonChildInfo info{ .child = child, .button = button_entity, .part = part };
			changed |= DrawButtonChildStateTransformSection(child_target, info, state);
			changed |= DrawFocusedButtonVisualComponent<ButtonBorderVisuals>(
				ctx, button_entity, child, "Edit Button Border",
				[&](auto& states) {
					return DrawButtonShapeVisualFields(
						ctx, button_entity, states, state, true, std::nullopt, false
					);
				}
			);
			break;
		}
		case ButtonChildPart::Sprite: {
			EntityInspectorTarget child_target{ .ctx = ctx, .entity = child };
			const ButtonChildInfo info{ .child = child, .button = button_entity, .part = part };
			changed |= DrawButtonChildStateTransformSection(child_target, info, state);
			changed |= DrawFocusedButtonVisualComponent<ButtonSpriteVisuals>(
				ctx, button_entity, child, "Edit Button Sprite",
				[&](auto& states) {
					return DrawButtonSpriteVisualFields(ctx, states, state, button_entity, false);
				}
			);
			break;
		}
		case ButtonChildPart::Text: {
			EntityInspectorTarget child_target{ .ctx = ctx, .entity = child };
			const ButtonChildInfo info{ .child = child, .button = button_entity, .part = part };
			changed |= DrawButtonChildStateTransformSection(child_target, info, state);
			changed |= DrawButtonChildStateVisualSection(child_target, info, state);
			break;
		}
	}
	return changed;
}

bool DrawFocusedButtonPartsTabs(
	EditorContext& ctx, Entity button_entity, ButtonVisualState state
) {
	bool changed{ false };
	Entity remove_after_tabs{};

	constexpr std::array parts{
		ButtonChildPart::Background,
		ButtonChildPart::Border,
		ButtonChildPart::Text,
		ButtonChildPart::Sprite,
	};

	auto part_count = [&]() {
		return static_cast<std::size_t>(std::ranges::count_if(parts, [&](ButtonChildPart part) {
			return static_cast<bool>(FindButtonPart(button_entity, part));
		}));
	};

	auto draw_add_popup = [&]() {
		if (!ImGui::BeginPopup("##AddButtonPartPopup")) {
			return;
		}
		auto add_part = [&](ButtonChildPart part, const char* label) {
			if (!FindButtonPart(button_entity, part) && ImGui::MenuItem(label)) {
				changed |= static_cast<bool>(CreateButtonPartForInspector(ctx, button_entity, part, state));
			}
		};
		add_part(ButtonChildPart::Background, "Background");
		add_part(ButtonChildPart::Border, "Border");
		add_part(ButtonChildPart::Text, "Text");
		add_part(ButtonChildPart::Sprite, "Sprite");
		ImGui::EndPopup();
	};

	const std::size_t existing_count{ part_count() };
	const bool has_all_parts{ existing_count == parts.size() };
	const bool add_requested{ DrawInspectorTabCollection(
		existing_count == 0,
		InspectorTabCollectionOptions{
			.scope_id = "##ButtonPartsTabStrip",
			.tab_bar_id = "##ButtonParts",
			.add_tab_id = "+##AddButtonPart",
			.empty_add_label = "Add Button Part",
			.add_tooltip = "Add button part",
			.show_add = !has_all_parts,
		},
		[&]() {
			for (const ButtonChildPart part : parts) {
				Entity child{ FindButtonPart(button_entity, part) };
				if (!child) {
					continue;
				}
				ScopedID part_scope{ static_cast<int>(part) };
				const std::string label{ ButtonPartLabel(part) };
				const bool selected{ ImGui::BeginTabItem(label.c_str()) };
				const auto context{ DrawInspectorTabContextMenu(
					"##ButtonPartContext", false, false, true, "Remove Part"
				) };
				if (context.remove_requested) {
					remove_after_tabs = child;
				}
				if (selected) {
					if (child && child != remove_after_tabs) {
						changed |= DrawFocusedButtonPartContents(ctx, button_entity, part, state);
					}
					ImGui::EndTabItem();
				}
			}
		}
	) };

	if (add_requested) {
		ImGui::OpenPopup("##AddButtonPartPopup");
	}
	draw_add_popup();

	if (remove_after_tabs) {
		ctx.commands.DeleteEntity(remove_after_tabs);
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

			changed |= DrawValue(target.ctx, "Sound Key", sound.value());

			changed |= DrawValue(
				target.ctx,
				"Exclusive",
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

void RefreshEditedButton(Entity entity) {
	if (entity && entity.Has<::ptgn::impl::ButtonData>()) {
		Button{ entity }.RefreshVisualState();
	}
}

template <typename Target>
bool DrawFocusedButtonInteraction(Target& target, FocusedUIControlType type) {
	if constexpr (!Target::template Supports<::ptgn::impl::ButtonData>()) {
		return false;
	} else {
		auto button_before{ target.template Capture<::ptgn::impl::ButtonData>() };
		if (!button_before) {
			return false;
		}

		auto button{ *button_before };
		auto toggle_before{ target.template Capture<::ptgn::impl::ToggleButtonData>() };
		auto dropdown_before{ target.template Capture<::ptgn::impl::DropdownData>() };
		auto toggle{ toggle_before.value_or(::ptgn::impl::ToggleButtonData{}) };
		auto dropdown{ dropdown_before.value_or(::ptgn::impl::DropdownData{}) };

		const bool show_toggle{ type == FocusedUIControlType::ToggleButton && toggle_before.has_value() };
		const bool show_dropdown{ type == FocusedUIControlType::Dropdown && dropdown_before.has_value() };

		bool button_changed{ false };
		bool toggle_changed{ false };
		bool dropdown_changed{ false };

		const std::string press_label{ button.press_enabled ? "Press Enabled" : "Press Disabled" };
		const std::string hover_label{ button.hover_enabled ? "Hover Enabled" : "Hover Disabled" };
		const std::string state_label{
			show_toggle ? (toggle.toggled ? "Toggled" : "Untoggled")
				: show_dropdown ? (dropdown.start_open ? "Starts Open" : "Starts Closed")
								: std::string{}
		};

		std::vector<InspectorAction> actions;
		actions.reserve((show_toggle || show_dropdown) ? 3u : 2u);
		actions.push_back(InspectorAction{
			.label = press_label,
			.tooltip = "Whether this control responds to presses.",
			.invoke = [&]() {
				button.press_enabled = !button.press_enabled;
				button_changed = true;
			},
		});
		actions.push_back(InspectorAction{
			.label = hover_label,
			.tooltip = "Whether this control responds to hover.",
			.invoke = [&]() {
				button.hover_enabled = !button.hover_enabled;
				button_changed = true;
			},
		});
		if (show_toggle) {
			actions.push_back(InspectorAction{
				.label = state_label,
				.tooltip = "Initial/current toggle state.",
				.invoke = [&]() {
					toggle.toggled = !toggle.toggled;
					toggle_changed = true;
				},
			});
		} else if (show_dropdown) {
			actions.push_back(InspectorAction{
				.label = state_label,
				.tooltip = "Whether the dropdown starts open.",
				.invoke = [&]() {
					dropdown.start_open = !dropdown.start_open;
					dropdown_changed = true;
				},
			});
		}

		ScopedID scope{ "FocusedButtonInteractionButtons" };
		DrawInspectorActionBar(
			actions,
			InspectorActionBarOptions{
				.id = "FocusedButtonInteraction",
				.equal_width = true,
			}
		);

		bool changed{ false };
		if (button_changed) {
			target.template SetLive<::ptgn::impl::ButtonData>(button, &RefreshEditedButton);
			auto button_after{ target.template Capture<::ptgn::impl::ButtonData>() };
			TrackComponentState(
				target, "Edit Button Interaction", std::move(button_before),
				std::move(button_after), true, &RefreshEditedButton
			);
			changed = true;
		}

		if (toggle_changed) {
			target.template SetLive<::ptgn::impl::ToggleButtonData>(toggle, &RefreshEditedButton);
			auto toggle_after{ target.template Capture<::ptgn::impl::ToggleButtonData>() };
			TrackComponentState(
				target, "Toggle Button State", std::move(toggle_before), std::move(toggle_after), true,
				&RefreshEditedButton
			);
			changed = true;
		}

		if (dropdown_changed) {
			target.template SetLive<::ptgn::impl::DropdownData>(dropdown);
			auto dropdown_after{ target.template Capture<::ptgn::impl::DropdownData>() };
			TrackComponentState(
				target, "Change Dropdown Start State", std::move(dropdown_before),
				std::move(dropdown_after), true
			);
			changed = true;
		}

		return changed;
	}
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
			changed |= DrawTransformSection(item_target, false);

			// Dropdown items keep their authored identity. Structural type conversion is an explicit
			// Convert To... operation rather than an ordinary inspector field.
			FocusedUIControlType item_type{ GetFocusedUIControlType(item_target) };

			if (item_type == FocusedUIControlType::Conflict) {
				changed |= DrawUIControlConflict(item_target);
			} else if (item_type != FocusedUIControlType::None) {
				changed |= DrawFocusedButtonInteraction(item_target, item_type);
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
	auto& editor_state{ GetInspectorUiState(target.GetInspectorTargetKey()) };
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
			ImGui::SeparatorText("Selected Toggle");
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

	// Snapshot application replaces the authored per-state visual arrays wholesale. Rebuild the
	// currently previewed state after every replacement so inherited visibility, transforms, text
	// data, and other resolved child state are immediately reconstructed from the new fallbacks.
	Button preview_button{ button };
	preview_button.PreviewVisualState(preview_state);
	preview_button.RefreshVisualState();
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
	if (!source) {
		visuals->states[destination_index] = {};
		return true;
	}

	// Copy what this part actually resolves to. A source state may have no authored visual of its
	// own and instead inherit the whole part from an earlier state (for example Hover -> Idle).
	// Copying the raw undefined source slot would incorrectly erase a defined destination such as
	// Idle, causing the managed child to disappear even though the source looked identical.
	for (const ButtonVisualState fallback : GetVisualStateFallbacks(*source)) {
		const auto& source_visual{
			visuals->states[static_cast<std::size_t>(std::to_underlying(fallback))]
		};
		if (!source_visual.defined) {
			continue;
		}

		visuals->states[destination_index] = source_visual;
		return true;
	}

	// There is no visual anywhere in this part's fallback chain, so the resolved source really is
	// undefined. Preserve that result rather than accidentally retaining the destination override.
	visuals->states[destination_index] = {};
	return true;
}

bool ApplyButtonStateOperation(
	EditorContext& ctx, Entity button, ButtonVisualState destination,
	std::optional<ButtonVisualState> source
) {
	if (!button) {
		return false;
	}

	// Normal/Idle is the root appearance state. It has no fallback state, so clearing it would
	// undefine managed visuals entirely until some later edit defines them again. Copying another
	// state into Idle is still valid; only resetting Idle is forbidden.
	if (!source && destination == ButtonVisualState::Idle) {
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
	const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
	const bool compact{ available < 260.0f };
	const bool can_reset_overrides{ state != ButtonVisualState::Idle };

	auto draw_copy_items = [&]() {
		for (const ButtonVisualState candidate : magic_enum::enum_values<ButtonVisualState>()) {
			int mode{ 0 };
			ButtonState pointer{ ButtonState::Idle };
			DecomposeButtonVisualState(candidate, mode, pointer);
			if ((!allow_toggled && mode == 1) || candidate == state) {
				continue;
			}

			const std::string label{ PrettyName(magic_enum::enum_name(candidate)) };
			if (ImGui::MenuItem(label.c_str())) {
				changed |= ApplyButtonStateOperation(ctx, button, state, candidate);
			}
		}
	};

	if (compact) {
		if (ImGui::Button("...##ButtonStateActions", ImVec2{ -FLT_MIN, 0.0f })) {
			ImGui::OpenPopup("##ButtonStateActionsPopup");
		}
		DrawTooltip("State actions");
		if (ImGui::BeginPopup("##ButtonStateActionsPopup")) {
			if (ImGui::MenuItem(
					"Reset State Overrides", nullptr, false, can_reset_overrides
				)) {
				changed |= ApplyButtonStateOperation(ctx, button, state, std::nullopt);
			}
			if (!can_reset_overrides && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip(
					"Normal / Idle is the base appearance state and has no fallback state."
				);
			}
			if (ImGui::BeginMenu("Copy From")) {
				draw_copy_items();
				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}
		return changed;
	}

	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float half_width{ std::max(1.0f, (available - spacing) * 0.5f) };
	{
		ScopedDisabled disabled{ !can_reset_overrides };
		if (ImGui::Button("Reset State Overrides", ImVec2{ half_width, 0.0f })) {
			changed |= ApplyButtonStateOperation(ctx, button, state, std::nullopt);
		}
	}
	DrawTooltip(
		can_reset_overrides
			? "Remove all overrides for this appearance state so each part falls back to inherited "
			  "values."
			: "Normal / Idle is the base appearance state and has no fallback state."
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
		"Copy each part from another appearance state. If that part is inherited in the source "
		"state, its resolved fallback visual is copied instead."
	);
	return changed;
}

template <typename Target>
bool DrawFocusedButtonAppearance(Target& target, FocusedUIControlType type) {
	auto& editor_state{ GetInspectorUiState(target.GetInspectorTargetKey()) };
	const bool allow_toggled{ type == FocusedUIControlType::ToggleButton };

	DrawFocusedButtonStateSelector(
		target.ctx, target.GetInspectorTargetKey(), editor_state.button_visual_state, allow_toggled
	);

	const ButtonVisualState state{
		editor_state.button_visual_state.value_or(ButtonVisualState::Idle)
	};

	bool changed{ false };

	if constexpr (requires { target.entity; }) {
		ApplyButtonPreview(target.entity, state, target.ctx);

		changed |= DrawFocusedButtonPartsTabs(target.ctx, target.entity, state);

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
		CaptureComponentSetState(target, UISectionComponents{})
	};

	if (keep != FocusedUIControlType::Slider) {
		RemoveSupportedComponent<Target, ::ptgn::impl::SliderData>(target);
	}
	if (keep != FocusedUIControlType::ToggleButton) {
		RemoveSupportedComponent<Target, ::ptgn::impl::ToggleButtonData>(target);
	}
	if (keep != FocusedUIControlType::Dropdown) {
		RemoveSupportedComponent<Target, ::ptgn::impl::DropdownData>(target);
	}
	if (keep != FocusedUIControlType::Dialogue) {
		RemoveSupportedComponent<Target, ::ptgn::impl::DialogueData>(target);
	}

	if (keep == FocusedUIControlType::Dialogue) {
		RemoveSupportedComponent<Target, ::ptgn::impl::ButtonData>(target);
	} else if constexpr (Target::template Supports<::ptgn::impl::ButtonData>()) {
		if (!target.template Capture<::ptgn::impl::ButtonData>()) {
			target.template SetLive<::ptgn::impl::ButtonData>(::ptgn::impl::ButtonData{});
		}
	}

	auto after{ CaptureComponentSetState(target, UISectionComponents{}) };
	TrackComponentSetState(
		target, "Repair UI Control Type", std::move(before), std::move(after),
		UISectionComponents{}
	);
	return true;
}

template <typename Target>
bool DrawUIControlConflict(Target& target) {
	ImGui::TextColored(
		ImVec4{ 1.0f, 0.45f, 0.2f, 1.0f },
		"This entity has multiple mutually-exclusive UI control types."
	);
	DrawDisabledWrappedText(
		"Keep exactly one UI control type. Dialogue is standalone; ButtonData is the shared base "
		"for Button, Toggle Button, Slider and Dropdown."
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
	if (HasTargetComponent<Target, ::ptgn::impl::DialogueData>(target)) {
		if (changed) {
			ImGui::SameLine();
		}
		if (ImGui::Button("Keep Dialogue")) {
			changed |= RepairUIControlConflict(target, FocusedUIControlType::Dialogue);
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
	const auto header{ DrawInspectorTreeToggleRow(
		"Track Transform", "##SliderTrackTransformTree", enabled
	) };
	const bool open{ header.open };
	const bool toggle_changed{ header.toggle_changed };
	if (toggle_changed) {
		auto after{ before };
		after.transform_enabled = enabled;
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
	if (open) {
		ScopedDisabled disabled{ !enabled };
		EntityInspectorTarget track_target{ .ctx = slider_target.ctx, .entity = track };
		changed |= DrawTransformSection(track_target, false, true, false);
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

void SynchronizeSliderTrackPart(Entity entity) {
	::ptgn::impl::SliderSystem::SynchronizeEntity(entity);
}

template <typename Marker, typename Draw>
[[maybe_unused]] bool DrawSliderTrackPartTree(
	EntityInspectorTarget& slider_target, Entity track, std::string_view label, Draw&& draw
) {
	Entity child{ FindSliderTrackPart<Marker>(track) };
	if (!child) {
		return false;
	}

	bool changed{ false };
	bool remove_requested{ false };
	ScopedID scope{ label };
	const std::string tree_label{ std::string{ label } + "##SliderTrackVisualPart" };
	const bool open{ ImGui::TreeNodeEx(
		tree_label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
	) };

	if (ImGui::BeginPopupContextItem()) {
		if (ImGui::MenuItem("Remove")) {
			remove_requested = true;
		}
		ImGui::EndPopup();
	}

	if (open) {
		ScopedIndent indent;
		EntityInspectorTarget child_target{ .ctx = slider_target.ctx, .entity = child };
		changed |= std::invoke(std::forward<Draw>(draw), child_target, child);
		ImGui::TreePop();
	}

	if (remove_requested) {
		slider_target.ctx.commands.DeleteEntity(child);
		SynchronizeSliderTrackVisualEnabled(track);
		changed = true;
	}

	return changed;
}

template <typename Marker>
bool DrawSliderTrackPartTransform(EntityInspectorTarget& target, std::string_view part_label) {
	auto before{ target.Capture<Marker>() };
	if (!before) {
		return false;
	}

	Marker marker{ *before };
	marker.initialized = true;
	marker.visual.defined = true;
	bool enabled{ marker.visual.transform.has_value() };
	bool changed{ false };
	ScopedID scope{ "SliderTrackPartTransform" };
	const auto header{ DrawInspectorTreeToggleRow(
		"Transform", "##SliderTrackPartTransformTree", enabled
	) };
	const bool open{ header.open };
	const bool toggle_changed{ header.toggle_changed };
	if (toggle_changed) {
		if (enabled) {
			marker.visual.transform = target.Capture<Transform>().value_or(Transform{});
			marker.visual.depth = target.Capture<Depth>().value_or(Depth{}).value;
			marker.visual.inherit_position = !target.Capture<::ptgn::impl::IgnoreParentPosition>().has_value();
			marker.visual.inherit_rotation = !target.Capture<::ptgn::impl::IgnoreParentRotation>().has_value();
			marker.visual.inherit_scale = !target.Capture<::ptgn::impl::IgnoreParentScale>().has_value();
			marker.visual.inherit_depth = !target.Capture<::ptgn::impl::IgnoreParentDepth>().has_value();
		} else {
			marker.visual.transform.reset();
			marker.visual.depth.reset();
			marker.visual.inherit_position.reset();
			marker.visual.inherit_rotation.reset();
			marker.visual.inherit_scale.reset();
			marker.visual.inherit_depth.reset();
		}
		target.SetLive<Marker>(marker, &SynchronizeSliderTrackPart);
		auto after{ target.Capture<Marker>() };
		TrackComponentState(
			target, std::string{ enabled ? "Enable " : "Disable " } + std::string{ part_label } + " Transform",
			std::move(before), std::move(after), true, &SynchronizeSliderTrackPart
		);
		changed = true;
	}
	DrawTooltip("Override this track part's transform, or leave it unchecked to use automatic placement.");
	if (open) {
		ScopedDisabled disabled{ !enabled };
		changed |= DrawTransformSection(target, false, false, false);
	}

	return changed;
}

float SliderTrackPartMaximumLineWidth(Entity border) {
	if (border.Has<Rect>()) {
		const auto size{ border.Get<Rect>().GetSize() };
		return std::max(
			kInspectorMinLineWidth, std::min(std::abs(size.x), std::abs(size.y)) * 0.5f
		);
	}
	if (border.Has<Circle>()) {
		return std::max(kInspectorMinLineWidth, std::abs(border.Get<Circle>().radius));
	}
	return 1000.0f;
}

bool DrawSliderTrackBorderLineWidth(ButtonShapeVisual& visual, Entity border) {
	auto& value{ visual.fill_style };
	const bool was_enabled{ value.has_value() };
	bool enabled{ was_enabled };
	const float maximum_width{ SliderTrackPartMaximumLineWidth(border) };
	float width{
		std::clamp(
			value.value_or(FillStyle{ kInspectorMinLineWidth })
				.GetLineWidth()
				.value_or(kInspectorMinLineWidth),
			kInspectorMinLineWidth, maximum_width
		)
	};
	bool field_changed{ false };

	const bool row_changed{ DrawOptionalPropertyRow("Line Width", enabled, false, [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		field_changed = ImGui::DragFloat(
			"##LineWidth", &width, 0.05f, kInspectorMinLineWidth, maximum_width, "%.2f",
			ImGuiSliderFlags_AlwaysClamp
		);
		return field_changed;
	}) };

	if (!row_changed) {
		return false;
	}
	if (!enabled) {
		value.reset();
	} else {
		value = FillStyle{ width };
	}
	return true;
}

template <typename Marker>
bool DrawSliderTrackShapeFields(EntityInspectorTarget& target, Entity part, bool border) {
	auto before{ target.Capture<Marker>() };
	if (!before) {
		return false;
	}

	bool changed{ DrawSliderTrackPartTransform<Marker>(
		target, border ? "Slider Track Border" : "Slider Track Background"
	) };

	// Transform editing writes the marker live, so recapture before editing the remaining fields.
	before = target.Capture<Marker>();
	if (!before) {
		return changed;
	}

	Marker data{ *before };
	data.initialized = true;
	data.visual.defined = true;
	std::array<ButtonShapeVisual, 1> states{ data.visual };
	constexpr ButtonVisualState state{ ButtonVisualState::Idle };
	bool visual_changed{ false };

	const bool had_size_override{ states[0].size.has_value() };
	std::optional<std::variant<V2_float, float>> current_size{};
	if (auto rect{ part.TryGet<Rect>() }) {
		current_size = rect->GetSize();
	} else if (auto circle{ part.TryGet<Circle>() }) {
		current_size = circle->radius;
	}
	const bool size_changed{ DrawButtonVisualOverrideValue(
		target.ctx, "Size", states, state, &ButtonShapeVisual::size
	) };
	if (size_changed && !had_size_override && states[0].size.has_value() && current_size) {
		states[0].size = current_size;
	}
	visual_changed |= size_changed;
	visual_changed |= DrawButtonVisualOverrideValue(
		target.ctx, "Color", states, state, &ButtonShapeVisual::color
	);

	if (border) {
		visual_changed |= DrawSliderTrackBorderLineWidth(states[0], part);
	}

	visual_changed |= DrawButtonVisualOverrideValue(
		target.ctx, "Origin", states, state, &ButtonShapeVisual::origin
	);
	DrawTooltip("Local origin used by this track part.");
	visual_changed |= DrawButtonVisualOverrideValue(
		target.ctx, "Anchor", states, state, &ButtonShapeVisual::anchor
	);
	DrawTooltip("Point on the automatic track rectangle used as this part's anchor.");

	if (!visual_changed) {
		return changed;
	}

	data.visual = states[0];
	target.SetLive<Marker>(data, &SynchronizeSliderTrackPart);
	auto after{ target.Capture<Marker>() };
	TrackComponentState(
		target, border ? "Edit Slider Track Border" : "Edit Slider Track Background",
		std::move(before), std::move(after), true, &SynchronizeSliderTrackPart
	);
	return true;
}

bool DrawSliderTrackBackgroundFields(EntityInspectorTarget& target, Entity background) {
	return DrawSliderTrackShapeFields<::ptgn::impl::SliderTrackBackgroundData>(
		target, background, false
	);
}

bool DrawSliderTrackBorderFields(EntityInspectorTarget& target, Entity border) {
	return DrawSliderTrackShapeFields<::ptgn::impl::SliderTrackBorderData>(target, border, true);
}

bool DrawSliderTrackSpriteFields(EntityInspectorTarget& target, Entity sprite) {
	auto before{ target.Capture<::ptgn::impl::SliderTrackSpriteData>() };
	if (!before) {
		return false;
	}

	bool changed{ DrawSliderTrackPartTransform<::ptgn::impl::SliderTrackSpriteData>(
		target, "Slider Track Sprite"
	) };

	// Transform editing writes the marker live, so recapture before editing the remaining fields.
	before = target.Capture<::ptgn::impl::SliderTrackSpriteData>();
	if (!before) {
		return changed;
	}

	auto data{ *before };
	data.initialized = true;
	data.visual.defined = true;
	std::array<ButtonSpriteVisual, 1> states{ data.visual };
	constexpr ButtonVisualState state{ ButtonVisualState::Idle };
	bool visual_changed{ false };

	visual_changed |= DrawButtonVisualOverrideValue(
		target.ctx, "Texture Key", states, state, &ButtonSpriteVisual::texture
	);
	const bool had_size_override{ states[0].size.has_value() };
	const std::optional<V2_float> current_size{ GetDisplaySize(sprite) };
	const bool size_changed{ DrawButtonVisualOverrideValue(
		target.ctx, "Texture Size", states, state, &ButtonSpriteVisual::size
	) };
	if (size_changed && !had_size_override && states[0].size.has_value() && current_size) {
		states[0].size = current_size;
	}
	visual_changed |= size_changed;
	visual_changed |= DrawButtonVisualOverrideValue(
		target.ctx, "Origin", states, state, &ButtonSpriteVisual::origin
	);
	DrawTooltip("Local origin used by this track sprite.");
	visual_changed |= DrawButtonVisualOverrideValue(
		target.ctx, "Anchor", states, state, &ButtonSpriteVisual::anchor
	);
	DrawTooltip("Point on the automatic track rectangle used as this sprite's anchor.");
	visual_changed |= DrawButtonVisualOverrideValue(
		target.ctx, "Tint", states, state, &ButtonSpriteVisual::tint
	);
	visual_changed |= DrawButtonVisualOverrideTree(
		target.ctx, "Animation", states, state, &ButtonSpriteVisual::animation,
		[&target](AnimationConfig& animation) {
			return DrawInspectorValueContents(
				target.ctx, Hash<AnimationConfig>(), std::addressof(animation)
			);
		}
	);
	visual_changed |= DrawButtonVisualOverrideTree(
		target.ctx, "Animation Options", states, state, &ButtonSpriteVisual::animation_options
	);

	if (!visual_changed) {
		return changed;
	}

	data.visual = states[0];
	target.SetLive<::ptgn::impl::SliderTrackSpriteData>(data, &SynchronizeSliderTrackPart);
	auto after{ target.Capture<::ptgn::impl::SliderTrackSpriteData>() };
	TrackComponentState(
		target, "Edit Slider Track Sprite", std::move(before), std::move(after), true,
		&SynchronizeSliderTrackPart
	);
	return true;
}

Entity CreateSliderTrackBackground(Entity slider_entity, Entity track) {
	Entity background{
		CreateRect(slider_entity.GetScene(), {}, V2_float{ 100.0f, 16.0f }, color::Gray)
	};
	background.Add<Tag>("Slider Track Background");
	auto& part{ background.Add<::ptgn::impl::SliderTrackBackgroundData>() };
	part.initialized = true;
	part.visual.defined = true;
	part.visual.color = color::Gray;
	SetParent(background, track);
	SetUI(background, IsUI(slider_entity));
	return background;
}

Entity CreateSliderTrackBorder(Entity slider_entity, Entity track) {
	Entity border{
		CreateRect(slider_entity.GetScene(), {}, V2_float{ 100.0f, 16.0f }, color::White)
	};
	border.Add<Tag>("Slider Track Border");
	auto& part{ border.Add<::ptgn::impl::SliderTrackBorderData>() };
	part.initialized = true;
	part.visual.defined = true;
	part.visual.color = color::White;
	part.visual.fill_style = FillStyle{ kInspectorMinLineWidth };
	border.Add<FillStyle>(FillStyle{ kInspectorMinLineWidth });
	SetParent(border, track);
	SetUI(border, IsUI(slider_entity));
	return border;
}

Entity CreateSliderTrackSprite(Entity slider_entity, Entity track) {
	Entity sprite{ CreateSprite(slider_entity.GetScene(), {}, {}, Origin::Center) };
	sprite.Add<Tag>("Slider Track Sprite");
	auto& part{ sprite.Add<::ptgn::impl::SliderTrackSpriteData>() };
	part.initialized = true;
	part.visual.defined = true;
	part.visual.tint = color::White;
	SetParent(sprite, track);
	SetUI(sprite, IsUI(slider_entity));
	return sprite;
}

bool DrawSliderTrackVisual(EntityInspectorTarget& target, Slider slider, Entity& track) {
	bool changed{ false };
	Entity remove_after_tabs{};
	auto ensure_track = [&]() -> Entity {
		if (track) {
			return track;
		}
		track = slider.EnsureTrack();
		if (track) {
			(void)RecordCreatedEntityPreservingSelection(target.ctx, track);
		}
		return track;
	};

	auto add_popup = [&]() {
		if (!ImGui::BeginPopup("##AddSliderTrackPartPopup")) {
			return;
		}
		Entity parent{ ensure_track() };
		if (parent) {
			if (!FindSliderTrackPart<::ptgn::impl::SliderTrackBackgroundData>(parent) &&
				ImGui::MenuItem("Background")) {
				Entity child{ CreateSliderTrackBackground(target.entity, parent) };
				(void)RecordCreatedEntityPreservingSelection(target.ctx, child);
				changed = true;
			}
			if (!FindSliderTrackPart<::ptgn::impl::SliderTrackBorderData>(parent) &&
				ImGui::MenuItem("Border")) {
				Entity child{ CreateSliderTrackBorder(target.entity, parent) };
				(void)RecordCreatedEntityPreservingSelection(target.ctx, child);
				changed = true;
			}
			if (!FindSliderTrackPart<::ptgn::impl::SliderTrackSpriteData>(parent) &&
				ImGui::MenuItem("Sprite")) {
				Entity child{ CreateSliderTrackSprite(target.entity, parent) };
				(void)RecordCreatedEntityPreservingSelection(target.ctx, child);
				changed = true;
			}
			SynchronizeSliderTrackVisualEnabled(parent);
		}
		ImGui::EndPopup();
	};

	const bool has_any_track_part{ track && HasSliderTrackVisualParts(track) };
	const bool all_track_parts{
		track && FindSliderTrackPart<::ptgn::impl::SliderTrackBackgroundData>(track) &&
		FindSliderTrackPart<::ptgn::impl::SliderTrackBorderData>(track) &&
		FindSliderTrackPart<::ptgn::impl::SliderTrackSpriteData>(track)
	};

	const bool add_requested{ DrawInspectorTabCollection(
		!has_any_track_part,
		InspectorTabCollectionOptions{
			.scope_id = "##SliderTrackPartStrip",
			.tab_bar_id = "##SliderTrackParts",
			.add_tab_id = "+##AddSliderTrackPart",
			.empty_add_label = "Add Track Part",
			.add_tooltip = "Add track visual part",
			.show_add = !all_track_parts,
		},
		[&]() {
			auto draw_part = [&](Entity child, std::string_view fallback, auto&& draw) {
				if (!child) {
					return;
				}
				ScopedID part_scope{ fallback };
				const std::string label{ fallback };
				const bool selected{ ImGui::BeginTabItem(label.c_str()) };
				const auto context{ DrawInspectorTabContextMenu(
					"##SliderTrackPartContext", false, false, true, "Remove Part"
				) };
				if (context.remove_requested) {
					remove_after_tabs = child;
				}
				if (selected) {
					if (child && child != remove_after_tabs) {
						EntityInspectorTarget child_target{ .ctx = target.ctx, .entity = child };
						changed |= std::invoke(draw, child_target, child);
					}
					ImGui::EndTabItem();
				}
			};

			draw_part(
				FindSliderTrackPart<::ptgn::impl::SliderTrackBackgroundData>(track), "Background",
				[](EntityInspectorTarget& child_target, Entity child) {
					return DrawSliderTrackBackgroundFields(child_target, child);
				}
			);
			draw_part(
				FindSliderTrackPart<::ptgn::impl::SliderTrackBorderData>(track), "Border",
				[](EntityInspectorTarget& child_target, Entity child) {
					return DrawSliderTrackBorderFields(child_target, child);
				}
			);
			draw_part(
				FindSliderTrackPart<::ptgn::impl::SliderTrackSpriteData>(track), "Sprite",
				[](EntityInspectorTarget& child_target, Entity child) {
					return DrawSliderTrackSpriteFields(child_target, child);
				}
			);
		}
	) };

	if (add_requested) {
		ImGui::OpenPopup("##AddSliderTrackPartPopup");
	}
	add_popup();

	if (remove_after_tabs) {
		target.ctx.commands.DeleteEntity(remove_after_tabs);
		if (track) {
			SynchronizeSliderTrackVisualEnabled(track);
		}
		changed = true;
	}
	if (track) {
		SynchronizeSliderTrackVisualEnabled(track);
	}
	return changed;
}


struct DialogueEditorVariantDraft {
	std::string name{};
	std::string source{};
};

struct DialogueEditorEntryDraft {
	std::string name{};
	std::size_t initial_variant{ 0 };
	bool repeatable{ true };
	DialogueBehavior behavior{ DialogueBehavior::Sequential };
	std::optional<bool> typewriter{};
	std::optional<milliseconds> typewriter_duration{};
	std::string next{};
	DialogueAppearance appearance{};
	std::vector<DialogueEditorVariantDraft> variants{};
};

struct DialogueEditorDocument {
	std::string continue_keys{ "Enter" };
	std::string start{};
	bool typewriter{ true };
	DialoguePageProperties defaults{};
	DialoguePortraitActorMap portrait_actors{};
	std::vector<DialogueEditorEntryDraft> dialogues{};
};

void NormalizeDialogueEditorEntry(DialogueEditorEntryDraft& entry) {
	if (entry.variants.empty()) {
		entry.variants.emplace_back(DialogueEditorVariantDraft{ .name = "Variant 1" });
	}

	for (std::size_t i{ 0 }; i < entry.variants.size(); ++i) {
		auto& variant{ entry.variants[i] };
		if (variant.name.empty()) {
			variant.name = "Variant " + std::to_string(i + 1);
		}
		const std::string base{ variant.name };
		std::size_t suffix{ 2 };
		auto duplicate_before = [&]() {
			for (std::size_t previous{ 0 }; previous < i; ++previous) {
				if (entry.variants[previous].name == variant.name) {
					return true;
				}
			}
			return false;
		};
		while (duplicate_before()) {
			variant.name = base + " " + std::to_string(suffix++);
		}
	}

	entry.initial_variant = std::min(entry.initial_variant, entry.variants.size() - 1);

	// Inherit means inherit the entity's complete typewriter configuration, including duration.
	// A dialogue-local duration only exists alongside an explicit Enabled/Disabled mode.
	if (!entry.typewriter.has_value()) {
		entry.typewriter_duration.reset();
	}
}

void NormalizeDialogueEditorDocument(DialogueEditorDocument& document) {
	if (document.dialogues.empty()) {
		document.dialogues.emplace_back(DialogueEditorEntryDraft{
			.name = "dialogue",
			.variants = { DialogueEditorVariantDraft{ .name = "Variant 1" } },
		});
	}

	for (auto& entry : document.dialogues) {
		NormalizeDialogueEditorEntry(entry);
	}

	for (auto& [actor_key, actor] : document.portrait_actors) {
		if (actor.display_name.empty()) {
			actor.display_name = actor_key;
		}
		for (auto& [expression_key, expression] : actor.expressions) {
			if (expression.display_name.empty()) {
				expression.display_name = expression_key;
			}
		}
		if (!actor.expressions.empty() && !actor.expressions.contains(actor.default_expression)) {
			actor.default_expression = actor.expressions.begin()->first;
		}
	}

	const auto start_it{ std::ranges::find_if(
		document.dialogues,
		[&](const DialogueEditorEntryDraft& entry) {
			return entry.name == document.start;
		}
	) };
	if (start_it == document.dialogues.end()) {
		document.start = document.dialogues.front().name;
	}
}

[[nodiscard]] std::string DialogueSourceFromJson(
	const json& value,
	const DialoguePageProperties& root_defaults
) {
	if (value.is_string()) {
		return value.get<std::string>();
	}

	if (!value.is_object()) {
		return {};
	}

	if (value.contains("source") && value.at("source").is_string()) {
		return value.at("source").get<std::string>();
	}

	if (value.contains("text")) {
		const auto& text{ value.at("text") };
		if (text.is_string()) {
			return text.get<std::string>();
		}
		if (text.is_object() && text.contains("source")) {
			return text.at("source").get<std::string>();
		}
	}

	if (value.contains("content") && value.at("content").is_string()) {
		return value.at("content").get<std::string>();
	}

	if (!value.contains("pages")) {
		return {};
	}

	std::string source;
	const auto append_page = [&](const json& page_json, std::string& destination) {
		const bool instant{
			page_json.is_object() && page_json.value("instant", false)
		};

		std::optional<milliseconds> duration_override{};
		if (!instant && page_json.is_object()) {
			if (page_json.contains("scroll_duration")) {
				duration_override = page_json.at("scroll_duration").get<milliseconds>();
			} else if (page_json.contains("properties") &&
				page_json.at("properties").is_object() &&
				page_json.at("properties").contains("scroll_duration")) {
				duration_override =
					page_json.at("properties").at("scroll_duration").get<milliseconds>();
			}

			if (duration_override == root_defaults.scroll_duration) {
				duration_override.reset();
			}
		}

		std::string control_line;
		if (instant) {
			control_line = ::ptgn::impl::kDialogueInstantPageTag;
		} else if (duration_override.has_value()) {
			control_line = std::string{ ::ptgn::impl::kDialogueDurationPageTagPrefix } +
				std::to_string(duration_override->count()) + "ms" +
				std::string{ ::ptgn::impl::kDialogueDurationPageTagSuffix };
		}

		if (!destination.empty()) {
			destination += control_line.empty()
				? "\n\n"
				: "\n" + control_line + "\n";
		} else if (!control_line.empty()) {
			destination += control_line;
			destination.push_back('\n');
		}

		if (page_json.is_string()) {
			destination += page_json.get<std::string>();
			return;
		}

		if (!page_json.is_object()) {
			return;
		}

		if (page_json.contains("text")) {
			const auto& text{ page_json.at("text") };
			if (text.is_string()) {
				destination += text.get<std::string>();
				return;
			}
			if (text.is_object() && text.contains("source")) {
				destination += text.at("source").get<std::string>();
				return;
			}
		}

		if (page_json.contains("content") && page_json.at("content").is_string()) {
			destination += page_json.at("content").get<std::string>();
			return;
		}

		if (page_json.contains("styled_text")) {
			DialoguePageProperties page_defaults{ root_defaults };
			if (page_json.contains("properties")) {
				page_defaults = page_defaults.InheritProperties(page_json.at("properties"));
			}
			destination += SerializeStyledTextToRichText(
				page_json.at("styled_text").get<StyledText>(),
				page_defaults.text_defaults
			);
		}
	};

	const auto& pages{ value.at("pages") };
	if (pages.is_array()) {
		for (const auto& page_json : pages) {
			append_page(page_json, source);
		}
	} else {
		append_page(pages, source);
	}
	return source;
}

[[nodiscard]] DialogueEditorDocument ParseDialogueEditorDocument(
	const ::ptgn::impl::DialogueData& data
) {
	const json root =
		data.Definition().is_object() && !data.Definition().empty()
			? data.Definition()
			: ::ptgn::impl::DialogueData::MakeDefaultDefinition();

	DialogueEditorDocument document;
	document.defaults = DialoguePageProperties{}.InheritProperties(root);

	if (root.contains("continue_key")) {
		const auto& continue_json{ root.at("continue_key") };
		document.continue_keys = continue_json.is_string()
			? continue_json.get<std::string>()
			: ::ptgn::impl::DialogueKeyName(continue_json.get<Key>());
	}

	document.start = root.value("start", std::string{});
	document.typewriter = root.value("scroll", true);
	document.portrait_actors = root.value("portrait_actors", DialoguePortraitActorMap{});

	if (root.contains("dialogues") && root.at("dialogues").is_object()) {
		for (const auto& [name, value] : root.at("dialogues").items()) {
			if (!value.is_object()) {
				continue;
			}

			DialogueEditorEntryDraft entry;
			entry.name = name;
			entry.initial_variant = value.value(
				"initial_variant",
				value.value("index", 0uz)
			);
			entry.repeatable = value.value("repeatable", true);
			entry.behavior = value.value(
				"behavior",
				DialogueBehavior::Sequential
			);
			if (value.contains("scroll")) {
				entry.typewriter = value.at("scroll").get<bool>();
			}
			if (value.contains("scroll_duration")) {
				entry.typewriter_duration = value.at("scroll_duration").get<milliseconds>();
			}
			entry.next = value.value("next", std::string{});
			entry.appearance = value.value("appearance", DialogueAppearance{});

			const json* variants{ nullptr };
			if (value.contains("variants")) {
				variants = std::addressof(value.at("variants"));
			} else if (value.contains("lines")) {
				variants = std::addressof(value.at("lines"));
			}

			if (variants) {
				DialoguePageProperties source_defaults{ document.defaults };
				if (entry.typewriter_duration.has_value()) {
					source_defaults.scroll_duration = *entry.typewriter_duration;
				}

				auto append_variant = [&](const json& variant_json, std::size_t index) {
					std::string variant_name{ "Variant " + std::to_string(index + 1) };
					if (variant_json.is_object()) {
						variant_name = variant_json.value("name", variant_name);
					}
					entry.variants.emplace_back(DialogueEditorVariantDraft{
						.name = std::move(variant_name),
						.source = DialogueSourceFromJson(variant_json, source_defaults),
					});
				};

				if (variants->is_array()) {
					std::size_t index{ 0 };
					for (const auto& variant_json : *variants) {
						append_variant(variant_json, index++);
					}
				} else {
					append_variant(*variants, 0);
				}
			}

			NormalizeDialogueEditorEntry(entry);
			document.dialogues.emplace_back(std::move(entry));
		}
	}

	NormalizeDialogueEditorDocument(document);
	return document;
}

[[nodiscard]] json BuildDialogueEditorDefinition(
	const DialogueEditorDocument& document
) {
	json root = document.defaults;
	root["continue_key"] = document.continue_keys;
	root["start"] = document.start;
	root["scroll"] = document.typewriter;
	if (!document.portrait_actors.empty()) {
		root["portrait_actors"] = document.portrait_actors;
	} else {
		root.erase("portrait_actors");
	}
	root["dialogues"] = json::object();

	for (const auto& entry : document.dialogues) {
		json value{
			{ "repeatable", entry.repeatable },
			{ "next", entry.next },
			{ "behavior", entry.behavior },
			{ "initial_variant", entry.initial_variant },
			{ "variants", json::array() },
		};
		if (entry.typewriter.has_value()) {
			value["scroll"] = *entry.typewriter;
		}
		if (entry.typewriter_duration.has_value()) {
			value["scroll_duration"] = *entry.typewriter_duration;
		}
		if (!entry.appearance.Empty()) {
			value["appearance"] = entry.appearance;
		}

		for (const auto& variant : entry.variants) {
			value["variants"].push_back(json{
				{ "name", variant.name },
				{ "source", variant.source },
			});
		}

		root["dialogues"][entry.name] = std::move(value);
	}

	return root;
}

[[nodiscard]] bool DialogueEditorNameExists(
	const DialogueEditorDocument& document,
	std::string_view name,
	std::optional<std::size_t> ignore = std::nullopt
) {
	for (std::size_t i{ 0 }; i < document.dialogues.size(); ++i) {
		if (ignore == i) {
			continue;
		}
		if (document.dialogues[i].name == name) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] std::string MakeUniqueDialogueEditorName(
	const DialogueEditorDocument& document,
	std::string_view base = "dialogue"
) {
	if (!DialogueEditorNameExists(document, base)) {
		return std::string{ base };
	}

	for (std::size_t suffix{ 2 };; ++suffix) {
		const std::string candidate{
			std::string{ base } + "_" + std::to_string(suffix)
		};
		if (!DialogueEditorNameExists(document, candidate)) {
			return candidate;
		}
	}
}

[[nodiscard]] bool DialogueVariantNameExists(
	const DialogueEditorEntryDraft& entry,
	std::string_view name,
	std::optional<std::size_t> ignore = std::nullopt
) {
	for (std::size_t i{ 0 }; i < entry.variants.size(); ++i) {
		if (ignore == i) {
			continue;
		}
		if (entry.variants[i].name == name) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] std::string MakeUniqueDialogueVariantName(
	const DialogueEditorEntryDraft& entry, std::string_view base = "Variant"
) {
	if (!DialogueVariantNameExists(entry, base)) {
		return std::string{ base };
	}
	for (std::size_t suffix{ 2 };; ++suffix) {
		const std::string candidate{ std::string{ base } + " " + std::to_string(suffix) };
		if (!DialogueVariantNameExists(entry, candidate)) {
			return candidate;
		}
	}
}

void RenameDialogueEditorEntry(
	DialogueEditorDocument& document,
	std::size_t index,
	std::string_view new_name
) {
	if (index >= document.dialogues.size() || new_name.empty()) {
		return;
	}

	const std::string old_name{ document.dialogues[index].name };
	if (old_name == new_name) {
		return;
	}

	document.dialogues[index].name = std::string{ new_name };

	if (document.start == old_name) {
		document.start = std::string{ new_name };
	}

	for (auto& entry : document.dialogues) {
		if (entry.next == old_name) {
			entry.next = std::string{ new_name };
		}
	}
}

[[nodiscard]] std::size_t DialogueEditorSelectedIndex(
	DialogueEditorDocument& document,
	InspectorUiState& state
) {
	NormalizeDialogueEditorDocument(document);

	const auto it{ std::ranges::find_if(
		document.dialogues,
		[&](const DialogueEditorEntryDraft& entry) {
			return entry.name == state.dialogue_key;
		}
	) };

	if (it != document.dialogues.end()) {
		return static_cast<std::size_t>(
			std::distance(document.dialogues.begin(), it)
		);
	}

	const auto start_it{ std::ranges::find_if(
		document.dialogues,
		[&](const DialogueEditorEntryDraft& entry) {
			return entry.name == document.start;
		}
	) };

	const std::size_t index{
		start_it != document.dialogues.end()
			? static_cast<std::size_t>(
				std::distance(document.dialogues.begin(), start_it)
			)
			: 0
	};
	state.dialogue_key = document.dialogues[index].name;
	state.dialogue_variant_index = 0;
	state.dialogue_preview_page = 0;
	return index;
}

struct DialoguePageNumberPreview {
	std::string source{};
	std::vector<std::size_t> line_page_numbers{};
};

void AppendDialoguePagePreviewLine(
	DialoguePageNumberPreview& preview,
	std::string_view line,
	std::size_t page_number
) {
	if (!preview.line_page_numbers.empty()) {
		preview.source.push_back('\n');
	}

	preview.source.append(line);
	const bool blank{
		std::ranges::all_of(
			line,
			[](unsigned char c) {
				return std::isspace(c) != 0;
			}
		)
	};
	preview.line_page_numbers.emplace_back(blank ? 0 : page_number);
}

[[nodiscard]] DialoguePageNumberPreview BuildDialoguePageNumberPreview(
	EditorContext& ctx,
	std::string_view source,
	const DialoguePageProperties& properties
) {
	DialoguePageNumberPreview preview;
	auto pages{ ::ptgn::impl::PaginateDialogueSource(
		ctx.editor.GetAssetManager(),
		source,
		properties
	) };

	for (std::size_t page_index{ 0 }; page_index < pages.size(); ++page_index) {
		if (page_index > 0) {
			AppendDialoguePagePreviewLine(preview, {}, 0);
		}

		const std::string page_source{
			SerializeStyledTextToRichText(
				pages[page_index].styled_text,
				pages[page_index].properties.text_defaults
			)
		};

		if (page_source.empty()) {
			AppendDialoguePagePreviewLine(preview, {}, 0);
			continue;
		}

		std::size_t line_begin{ 0 };
		while (line_begin <= page_source.size()) {
			const std::size_t newline{
				page_source.find('\n', line_begin)
			};
			const std::size_t line_end{
				newline == std::string::npos
					? page_source.size()
					: newline
			};
			AppendDialoguePagePreviewLine(
				preview,
				std::string_view{ page_source }.substr(
					line_begin,
					line_end - line_begin
				),
				page_index + 1
			);

			if (newline == std::string::npos) {
				break;
			}
			line_begin = newline + 1;
		}
	}

	return preview;
}

void DrawDialoguePreviewNavigation(
	InspectorUiState& state,
	std::size_t page_count
) {
	if (page_count == 0) {
		state.dialogue_preview_page = 0;
		return;
	}

	state.dialogue_preview_page = std::min(
		state.dialogue_preview_page,
		page_count - 1
	);

	const bool has_previous{ state.dialogue_preview_page > 0 };
	const bool has_next{ state.dialogue_preview_page + 1 < page_count };

	ImGui::BeginDisabled(!has_previous);
	if (ImGui::Button("<##PreviousDialoguePage")) {
		--state.dialogue_preview_page;
	}
	ImGui::EndDisabled();

	const std::size_t range_begin{
		state.dialogue_preview_page > 2
			? state.dialogue_preview_page - 2
			: 0
	};
	const std::size_t range_end{
		std::min(page_count, state.dialogue_preview_page + 3)
	};

	for (std::size_t i{ range_begin }; i < range_end; ++i) {
		ImGui::SameLine();
		const bool selected{ i == state.dialogue_preview_page };
		if (selected) {
			ImGui::PushStyleColor(
				ImGuiCol_Button,
				ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)
			);
		}

		const std::string label{
			std::to_string(i + 1) + "##DialoguePreviewPage"
		};
		if (ImGui::Button(label.c_str())) {
			state.dialogue_preview_page = i;
		}

		if (selected) {
			ImGui::PopStyleColor();
		}
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!has_next);
	if (ImGui::Button(">##NextDialoguePage")) {
		++state.dialogue_preview_page;
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	const float page_input_width{
		ImGui::CalcTextSize("000").x +
		ImGui::GetStyle().FramePadding.x * 2.0f +
		4.0f
	};
	ImGui::SetNextItemWidth(page_input_width);
	int requested_page{
		static_cast<int>(state.dialogue_preview_page + 1)
	};
	if (ImGui::InputInt(
			"##DialoguePreviewPageNumber",
			&requested_page,
			0,
			0,
			ImGuiInputTextFlags_CharsDecimal
		)) {
		requested_page = std::clamp(
			requested_page,
			1,
			static_cast<int>(page_count)
		);
		state.dialogue_preview_page =
			static_cast<std::size_t>(requested_page - 1);
	}

	ImGui::SameLine();
	ImGui::TextDisabled("of %zu", page_count);
}


[[nodiscard]] std::string MakeUniqueDialoguePortraitActorKey(
	const DialoguePortraitActorMap& actors
) {
	for (std::size_t index{ 1 };; ++index) {
		const std::string key{ "speaker_" + std::to_string(index) };
		if (!actors.contains(key)) {
			return key;
		}
	}
}

[[nodiscard]] std::string MakeUniqueDialoguePortraitExpressionKey(
	const DialoguePortraitActor& actor
) {
	for (std::size_t index{ 1 };; ++index) {
		const std::string key{ "expression_" + std::to_string(index) };
		if (!actor.expressions.contains(key)) {
			return key;
		}
	}
}

[[nodiscard]] std::vector<std::string> SortedDialoguePortraitActorKeys(
	const DialoguePortraitActorMap& actors
) {
	std::vector<std::string> keys;
	keys.reserve(actors.size());
	for (const auto& [key, _] : actors) {
		keys.emplace_back(key);
	}
	std::ranges::sort(keys, [&](const std::string& lhs, const std::string& rhs) {
		const auto& left{ actors.at(lhs) };
		const auto& right{ actors.at(rhs) };
		const std::string_view left_name{ left.display_name.empty() ? lhs : left.display_name };
		const std::string_view right_name{ right.display_name.empty() ? rhs : right.display_name };
		return left_name < right_name;
	});
	return keys;
}

[[nodiscard]] std::vector<std::string> SortedDialoguePortraitExpressionKeys(
	const DialoguePortraitActor& actor
) {
	std::vector<std::string> keys;
	keys.reserve(actor.expressions.size());
	for (const auto& [key, _] : actor.expressions) {
		keys.emplace_back(key);
	}
	std::ranges::sort(keys, [&](const std::string& lhs, const std::string& rhs) {
		const auto& left{ actor.expressions.at(lhs) };
		const auto& right{ actor.expressions.at(rhs) };
		const std::string_view left_name{ left.display_name.empty() ? lhs : left.display_name };
		const std::string_view right_name{ right.display_name.empty() ? rhs : right.display_name };
		return left_name < right_name;
	});
	return keys;
}

bool DrawDialoguePortraitSpriteVisual(
	EditorContext& ctx,
	Entity relative_to,
	ButtonSpriteVisual& visual
) {
	std::array<ButtonSpriteVisual, kButtonVisualStateCount> states{};
	states[static_cast<std::size_t>(std::to_underlying(ButtonVisualState::Idle))] = visual;
	const bool changed{
		DrawButtonSpriteVisualFields(ctx, states, ButtonVisualState::Idle, relative_to)
	};
	if (changed) {
		visual = states[static_cast<std::size_t>(std::to_underlying(ButtonVisualState::Idle))];
	}
	return changed;
}

bool DrawDialoguePortraitDefinitions(
	EditorContext& ctx,
	Entity relative_to,
	DialogueEditorDocument& document,
	InspectorUiState& state,
	std::string& reason
) {
	bool changed{ false };
	const bool open{ ImGui::TreeNodeEx(
		"Portraits##DialoguePortraitDefinitions",
		ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
	) };
	if (!open) {
		return false;
	}

	ScopedIndent indent;

	if (document.portrait_actors.empty()) {
		if (ImGui::Button("Add Speaker", ImVec2{ -FLT_MIN, 0.0f })) {
			const std::string key{ MakeUniqueDialoguePortraitActorKey(document.portrait_actors) };
			DialoguePortraitActor actor;
			actor.display_name = "Speaker 1";
			const std::string expression_key{ MakeUniqueDialoguePortraitExpressionKey(actor) };
			actor.default_expression = expression_key;
			actor.expressions.emplace(
				expression_key,
				DialoguePortraitExpression{ .display_name = "Neutral" }
			);
			document.portrait_actors.emplace(key, std::move(actor));
			state.dialogue_portrait_actor = key;
			state.dialogue_portrait_expression = expression_key;
			changed = true;
			reason = "Add Dialogue Portrait Speaker";
		}
		ImGui::TreePop();
		return changed;
	}

	const auto actor_keys{ SortedDialoguePortraitActorKeys(document.portrait_actors) };
	if (!document.portrait_actors.contains(state.dialogue_portrait_actor)) {
		state.dialogue_portrait_actor = actor_keys.front();
	}

	(void)DrawPropertyRow("Speaker", [&]() {
		bool local_changed{ false };
		const auto& current{ document.portrait_actors.at(state.dialogue_portrait_actor) };
		const std::string_view preview{
			current.display_name.empty() ? state.dialogue_portrait_actor : current.display_name
		};
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::BeginCombo("##DialoguePortraitSpeakerDefinition", preview.data())) {
			for (const auto& key : actor_keys) {
				const auto& actor{ document.portrait_actors.at(key) };
				const std::string_view label{ actor.display_name.empty() ? key : actor.display_name };
				if (ImGui::Selectable(label.data(), state.dialogue_portrait_actor == key)) {
					state.dialogue_portrait_actor = key;
					state.dialogue_portrait_expression.clear();
					local_changed = true;
				}
			}
			ImGui::EndCombo();
		}
		return local_changed;
	});

	auto& actor{ document.portrait_actors.at(state.dialogue_portrait_actor) };
	changed |= DrawPropertyRow("Speaker Name", [&]() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (!ImGui::InputText("##DialoguePortraitSpeakerName", &actor.display_name)) {
			return false;
		}
		reason = "Rename Dialogue Portrait Speaker";
		return true;
	});

	{
		const std::array actions{
			InspectorAction{
				.label = "Add Speaker",
				.invoke = [&]() {
					const std::string key{ MakeUniqueDialoguePortraitActorKey(document.portrait_actors) };
					DialoguePortraitActor new_actor;
					new_actor.display_name =
						"Speaker " + std::to_string(document.portrait_actors.size() + 1);
					const std::string expression_key{
						MakeUniqueDialoguePortraitExpressionKey(new_actor)
					};
					new_actor.default_expression = expression_key;
					new_actor.expressions.emplace(
						expression_key, DialoguePortraitExpression{ .display_name = "Neutral" }
					);
					document.portrait_actors.emplace(key, std::move(new_actor));
					state.dialogue_portrait_actor = key;
					state.dialogue_portrait_expression = expression_key;
					changed = true;
					reason = "Add Dialogue Portrait Speaker";
				},
			},
			InspectorAction{
				.label = "Remove Speaker",
				.enabled = document.portrait_actors.size() > 1,
				.invoke = [&]() {
					document.portrait_actors.erase(state.dialogue_portrait_actor);
					const auto remaining{ SortedDialoguePortraitActorKeys(document.portrait_actors) };
					state.dialogue_portrait_actor =
						remaining.empty() ? std::string{} : remaining.front();
					state.dialogue_portrait_expression.clear();
					changed = true;
					reason = "Remove Dialogue Portrait Speaker";
				},
			},
		};
		DrawInspectorActionBar(actions, { .id = "DialogueSpeakerActions" });
	}

	if (!document.portrait_actors.contains(state.dialogue_portrait_actor)) {
		ImGui::TreePop();
		return changed;
	}
	auto& selected_actor{ document.portrait_actors.at(state.dialogue_portrait_actor) };
	if (selected_actor.expressions.empty()) {
		selected_actor.default_expression.clear();
		state.dialogue_portrait_expression.clear();

		(void)DrawPropertyRow("Default Expression", [&]() {
			ScopedDisabled disabled{ true };
			ImGui::SetNextItemWidth(-FLT_MIN);
			return ImGui::BeginCombo("##DialoguePortraitDefaultExpressionEmpty", "None")
				? (ImGui::EndCombo(), false)
				: false;
		});

		(void)DrawPropertyRow("Expression", [&]() {
			ScopedDisabled disabled{ true };
			ImGui::SetNextItemWidth(-FLT_MIN);
			return ImGui::BeginCombo("##DialoguePortraitExpressionDefinitionEmpty", "None")
				? (ImGui::EndCombo(), false)
				: false;
		});

		if (ImGui::Button("Add Expression", ImVec2{ -FLT_MIN, 0.0f })) {
			const std::string key{ MakeUniqueDialoguePortraitExpressionKey(selected_actor) };
			selected_actor.default_expression = key;
			selected_actor.expressions.emplace(
				key,
				DialoguePortraitExpression{ .display_name = "Expression 1" }
			);
			state.dialogue_portrait_expression = key;
			changed = true;
			reason = "Add Dialogue Portrait Expression";
		}

		ImGui::TreePop();
		return changed;
	}
	const auto expression_keys{ SortedDialoguePortraitExpressionKeys(selected_actor) };
	if (!selected_actor.expressions.contains(selected_actor.default_expression)) {
		selected_actor.default_expression = expression_keys.front();
	}
	if (!selected_actor.expressions.contains(state.dialogue_portrait_expression)) {
		state.dialogue_portrait_expression = selected_actor.default_expression;
		if (!selected_actor.expressions.contains(state.dialogue_portrait_expression)) {
			state.dialogue_portrait_expression = expression_keys.front();
		}
	}

	changed |= DrawPropertyRow("Default Expression", [&]() {
		bool local_changed{ false };
		const auto& current{ selected_actor.expressions.at(selected_actor.default_expression) };
		const std::string_view preview{
			current.display_name.empty() ? selected_actor.default_expression : current.display_name
		};
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::BeginCombo("##DialoguePortraitDefaultExpression", preview.data())) {
			for (const auto& key : expression_keys) {
				const auto& expression{ selected_actor.expressions.at(key) };
				const std::string_view label{ expression.display_name.empty() ? key : expression.display_name };
				if (ImGui::Selectable(label.data(), selected_actor.default_expression == key)) {
					selected_actor.default_expression = key;
					local_changed = true;
					reason = "Change Dialogue Portrait Default Expression";
				}
			}
			ImGui::EndCombo();
		}
		return local_changed;
	});

	(void)DrawPropertyRow("Expression", [&]() {
		bool local_changed{ false };
		const auto& current{ selected_actor.expressions.at(state.dialogue_portrait_expression) };
		const std::string_view preview{
			current.display_name.empty() ? state.dialogue_portrait_expression : current.display_name
		};
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::BeginCombo("##DialoguePortraitExpressionDefinition", preview.data())) {
			for (const auto& key : expression_keys) {
				const auto& expression{ selected_actor.expressions.at(key) };
				const std::string_view label{ expression.display_name.empty() ? key : expression.display_name };
				if (ImGui::Selectable(label.data(), state.dialogue_portrait_expression == key)) {
					state.dialogue_portrait_expression = key;
					local_changed = true;
				}
			}
			ImGui::EndCombo();
		}
		return local_changed;
	});

	changed |= DrawPropertyRow("Expression Name", [&]() {
		auto& expression{
			selected_actor.expressions.at(state.dialogue_portrait_expression)
		};
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (!ImGui::InputText("##DialoguePortraitExpressionName", &expression.display_name)) {
			return false;
		}
		reason = "Rename Dialogue Portrait Expression";
		return true;
	});

	{
		const std::array actions{
			InspectorAction{
				.label = "Add Expression",
				.invoke = [&]() {
					const std::string key{ MakeUniqueDialoguePortraitExpressionKey(selected_actor) };
					selected_actor.expressions.emplace(
						key,
						DialoguePortraitExpression{
							.display_name = "Expression " +
								std::to_string(selected_actor.expressions.size() + 1)
						}
					);
					state.dialogue_portrait_expression = key;
					changed = true;
					reason = "Add Dialogue Portrait Expression";
				},
			},
			InspectorAction{
				.label = "Remove Expression",
				.enabled = !selected_actor.expressions.empty(),
				.invoke = [&]() {
					const std::string removed{ state.dialogue_portrait_expression };
					selected_actor.expressions.erase(removed);
					const auto remaining{ SortedDialoguePortraitExpressionKeys(selected_actor) };
					if (remaining.empty()) {
						state.dialogue_portrait_expression.clear();
						selected_actor.default_expression.clear();
					} else {
						state.dialogue_portrait_expression = remaining.front();
						if (selected_actor.default_expression == removed ||
							!selected_actor.expressions.contains(selected_actor.default_expression)) {
							selected_actor.default_expression = state.dialogue_portrait_expression;
						}
					}
					changed = true;
					reason = "Remove Dialogue Portrait Expression";
				},
			},
		};
		DrawInspectorActionBar(actions, { .id = "DialogueExpressionActions" });
	}

	if (selected_actor.expressions.empty() ||
		!selected_actor.expressions.contains(state.dialogue_portrait_expression)) {
		ImGui::TreePop();
		return changed;
	}

	auto& expression{ selected_actor.expressions.at(state.dialogue_portrait_expression) };

	if (ImGui::TreeNodeEx(
			"Idle Visual##DialoguePortraitIdle",
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
		)) {
		ScopedIndent idle_indent;
		if (DrawDialoguePortraitSpriteVisual(ctx, relative_to, expression.idle)) {
			changed = true;
			reason = "Edit Dialogue Portrait Idle Visual";
		}
		ImGui::TreePop();
	}

	{
		ScopedID talking_scope{ "DialoguePortraitTalkingVisual" };
		bool enabled{ expression.talking.has_value() };
		bool talking_open{ false };

		const auto talking_header{ DrawInspectorTreeToggleRow(
			"Talking Visual", "##DialoguePortraitTalking", enabled
		) };
		talking_open = talking_header.open;
		const bool toggle_changed{ talking_header.toggle_changed };

		if (toggle_changed) {
			if (enabled) {
				expression.talking.emplace(expression.idle);
			} else {
				expression.talking.reset();
			}
			changed = true;
			reason = "Toggle Dialogue Portrait Talking Visual";
		}

		if (talking_open) {
			ScopedIndent talking_indent;
			auto displayed{ expression.talking.value_or(expression.idle) };
			ScopedDisabled disabled{ !enabled };
			const bool visual_changed{
				DrawDialoguePortraitSpriteVisual(ctx, relative_to, displayed)
			};
			if (enabled && visual_changed) {
				expression.talking = std::move(displayed);
				changed = true;
				reason = "Edit Dialogue Portrait Talking Visual";
			}
		}
	}

	ImGui::TreePop();
	return changed;
}

bool DrawDialogueAppearanceControls(
	EditorContext& ctx,
	Entity relative_to,
	DialogueEditorDocument& document,
	std::size_t selected_index,
	std::string& reason
) {
	if (selected_index >= document.dialogues.size()) {
		return false;
	}

	auto& entry{ document.dialogues[selected_index] };
	auto& appearance{ entry.appearance };
	bool changed{ false };

	auto remove_context = [&](const char* id, auto& value, std::string_view action) {
		const auto context{ DrawInspectorTabContextMenu(id, false, false, true, "Remove Part") };
		if (context.remove_requested) {
			value.reset();
			changed = true;
			reason = std::string{ action };
			return true;
		}
		return false;
	};

	InspectorTabStripScope strip{ "##DialogueAppearanceTabStrip" };
	if (ImGui::BeginTabBar("##DialogueAppearanceTabs", InspectorTabBarFlags())) {
		if (appearance.background.has_value()) {
			if (ImGui::BeginTabItem("Background")) {
				const bool removed{ remove_context(
					"##DialogueBackgroundContext", appearance.background,
					"Remove Dialogue Background Override"
				) };
				if (!removed && appearance.background.has_value()) {
					std::array<ButtonShapeVisual, 1> states{ *appearance.background };
					if (DrawButtonShapeVisualFields(
							ctx, relative_to, states, ButtonVisualState::Idle, false,
							std::variant<V2_float, float>{ document.defaults.box_size }
						)) {
						*appearance.background = states[0];
						changed = true;
						reason = "Edit Dialogue Background Override";
					}
				}
				ImGui::EndTabItem();
			}
		}

		if (appearance.border.has_value()) {
			if (ImGui::BeginTabItem("Border")) {
				const bool removed{ remove_context(
					"##DialogueBorderContext", appearance.border,
					"Remove Dialogue Border Override"
				) };
				if (!removed && appearance.border.has_value()) {
					std::array<ButtonShapeVisual, 1> states{ *appearance.border };
					if (DrawButtonShapeVisualFields(
							ctx, relative_to, states, ButtonVisualState::Idle, true,
							std::variant<V2_float, float>{ document.defaults.box_size }
						)) {
						*appearance.border = states[0];
						changed = true;
						reason = "Edit Dialogue Border Override";
					}
				}
				ImGui::EndTabItem();
			}
		}

		if (appearance.sprite.has_value()) {
			if (ImGui::BeginTabItem("Sprite")) {
				const bool removed{ remove_context(
					"##DialogueSpriteContext", appearance.sprite,
					"Remove Dialogue Sprite Override"
				) };
				if (!removed && appearance.sprite.has_value()) {
					std::array<ButtonSpriteVisual, 1> states{ *appearance.sprite };
					if (DrawButtonSpriteVisualFields(ctx, states, ButtonVisualState::Idle, relative_to)) {
						*appearance.sprite = states[0];
						changed = true;
						reason = "Edit Dialogue Sprite Override";
					}
				}
				ImGui::EndTabItem();
			}
		}

		if (appearance.audio.has_value()) {
			if (ImGui::BeginTabItem("Audio")) {
				const bool removed{ remove_context(
					"##DialogueAudioContext", appearance.audio,
					"Remove Dialogue Audio Override"
				) };
				if (!removed && appearance.audio.has_value()) {
					AudioKey open_sound{ appearance.audio->open.value_or(AudioKey{}) };
					if (DrawValue(ctx, "Open Sound", open_sound)) {
						if (open_sound.value.empty()) appearance.audio->open.reset();
						else appearance.audio->open = open_sound;
						changed = true;
						reason = "Edit Dialogue Open Sound";
					}

					AudioKey typewriter_sound{ appearance.audio->typewriter.value_or(AudioKey{}) };
					const bool effective_typewriter{ entry.typewriter.value_or(document.typewriter) };
					ImGui::BeginDisabled(!effective_typewriter);
					if (DrawValue(ctx, "Typewriter Sound", typewriter_sound)) {
						if (typewriter_sound.value.empty()) appearance.audio->typewriter.reset();
						else appearance.audio->typewriter = typewriter_sound;
						changed = true;
						reason = "Edit Dialogue Typewriter Sound";
					}
					ImGui::EndDisabled();
					DrawTooltip(effective_typewriter
						? "Played while typewriter text is being revealed."
						: "Enable typewriter text for this dialogue key (or through inheritance) to use this sound.");
				}
				ImGui::EndTabItem();
			}
		}

		const bool all_present{
			appearance.background.has_value() && appearance.border.has_value() &&
			appearance.sprite.has_value() && appearance.audio.has_value()
		};
		if (!all_present && DrawInspectorAddTabButton("+##AddDialogueAppearancePart", "Add dialogue part")) {
			ImGui::OpenPopup("##AddDialogueAppearancePartPopup");
		}
		if (!all_present && ImGui::BeginPopup("##AddDialogueAppearancePartPopup")) {
			if (!appearance.background.has_value() && ImGui::MenuItem("Background")) {
				ButtonShapeVisual visual;
				visual.defined = true;
				visual.color = color::Black.WithAlpha(180);
				visual.fill_style = FillStyle{ Solid{} };
				appearance.background = std::move(visual);
				changed = true;
				reason = "Add Dialogue Background Override";
			}
			if (!appearance.border.has_value() && ImGui::MenuItem("Border")) {
				ButtonShapeVisual visual;
				visual.defined = true;
				visual.color = color::White;
				visual.fill_style = FillStyle{ 2.0f };
				appearance.border = std::move(visual);
				changed = true;
				reason = "Add Dialogue Border Override";
			}
			if (!appearance.sprite.has_value() && ImGui::MenuItem("Sprite")) {
				ButtonSpriteVisual visual;
				visual.defined = true;
				visual.tint = color::White;
				appearance.sprite = std::move(visual);
				changed = true;
				reason = "Add Dialogue Sprite Override";
			}
			if (!appearance.audio.has_value() && ImGui::MenuItem("Audio")) {
				appearance.audio = DialogueSounds{};
				changed = true;
				reason = "Add Dialogue Audio Override";
			}
			ImGui::EndPopup();
		}

		ApplyInspectorTabBarHorizontalWheel();
		ImGui::EndTabBar();
	}

	return changed;
}

template <typename Target>
bool DrawFocusedDialogueControl(Target& target) {
	if constexpr (!Target::template Supports<::ptgn::impl::DialogueData>()) {
		return false;
	} else {
		auto before{ target.template Capture<::ptgn::impl::DialogueData>() };
		if (!before) {
			return false;
		}

		DialogueEditorDocument document{
			ParseDialogueEditorDocument(*before)
		};
		NormalizeDialogueEditorDocument(document);

		auto& state{
			GetInspectorUiState(target.GetInspectorTargetKey())
		};

		Entity dialogue_entity{};
		if constexpr (requires { target.entity; }) {
			dialogue_entity = target.entity;
		}

		std::size_t selected_index{
			DialogueEditorSelectedIndex(document, state)
		};

		bool changed{ false };
		std::string reason{ "Edit Dialogue" };

		auto mark_changed = [&](std::string_view label) {
			changed = true;
			reason = std::string{ label };
		};

		{

			changed |= DrawPropertyRow("Start Dialogue", [&]() {
			bool local_changed{ false };
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo(
					"##DialogueStart",
					document.start.c_str()
				)) {
				for (const auto& candidate : document.dialogues) {
					const bool selected{
						document.start == candidate.name
					};
					if (ImGui::Selectable(
							candidate.name.c_str(),
							selected
						)) {
						document.start = candidate.name;
						local_changed = true;
						reason = "Change Start Dialogue";
					}
					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			DrawTooltip(
				"Dialogue key opened when this dialogue control starts without an explicit key."
			);
			return local_changed;
		});

		changed |= DrawPropertyRow("Continue Key", [&]() {
			const bool local_changed{
				DrawKeyExpression(
					document.continue_keys,
					"##DialogueContinueKey",
					"Enter, Space or Left Ctrl + Enter"
				)
			};
			if (local_changed) {
				reason = "Edit Dialogue Continue Key";
			}
			return local_changed;
		});

		changed |= DrawPropertyRow("Typewriter Text", [&]() {
			bool local_changed{ false };
			bool typewriter{ document.typewriter };
			if (ImGui::Checkbox("##DialogueTypewriter", &typewriter)) {
				document.typewriter = typewriter;
				local_changed = true;
				reason = "Toggle Dialogue Typewriter Text";
			}
			DrawTooltip(
				"Reveal dialogue text over time. Pages marked Instant still appear immediately."
			);

			ImGui::SameLine();
			const float duration_width{
				std::max(1.0f, ImGui::GetContentRegionAvail().x)
			};
			if (DrawDurationTextInput(
					"##DialogueTypewriterDuration",
					document.defaults.scroll_duration,
					duration_width,
					!document.typewriter,
					"Duration used to reveal a typewriter page."
				)) {
				local_changed = true;
				reason = "Edit Dialogue Typewriter Duration";
			}
			return local_changed;
		});
		}

		if (ImGui::TreeNodeEx(
				"Layout##DialogueLayout",
				ImGuiTreeNodeFlags_SpanAvailWidth |
					ImGuiTreeNodeFlags_FramePadding
			)) {
			auto draw_root = [&](std::string_view label, auto& value) {
				if (DrawValue(target.ctx, label, value)) {
					mark_changed("Edit Dialogue Layout");
				}
			};
			draw_root("Box Size", document.defaults.box_size);
			draw_root("Padding", document.defaults.padding);
			draw_root(
				"Horizontal Align",
				document.defaults.horizontal_align
			);
			draw_root(
				"Vertical Align",
				document.defaults.vertical_align
			);
			draw_root("Wrap Mode", document.defaults.wrap_mode);
			draw_root(
				"Overflow Mode",
				document.defaults.overflow_mode
			);
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx(
				"Text Defaults##DialogueTextDefaults",
				ImGuiTreeNodeFlags_SpanAvailWidth |
					ImGuiTreeNodeFlags_FramePadding
			)) {
			auto& defaults{ document.defaults.text_defaults };
			auto draw_default = [&](std::string_view label, auto& value) {
				if (DrawValue(target.ctx, label, value)) {
					mark_changed("Edit Dialogue Text Defaults");
				}
			};
			draw_default("Font", defaults.font);
			draw_default("Color", defaults.style.color);
			draw_default("Size", defaults.style.size);
			draw_default(
				"Bold Weight",
				defaults.style.bold_weight
			);
			draw_default("Kerning", defaults.style.kerning);
			draw_default("Tracking", defaults.style.tracking);
			draw_default(
				"Line Spacing",
				defaults.style.line_spacing
			);
			draw_default("Flags", defaults.style.flags);
			draw_default("Distance Field", defaults.style.sdf);
			draw_default("Effect", defaults.style.effect);
			ImGui::TreePop();
		}

		if (DrawDialoguePortraitDefinitions(target.ctx, dialogue_entity, document, state, reason)) {
			changed = true;
		}

		ImGui::Spacing();

		std::optional<std::size_t> rename_dialogue;
		std::optional<std::size_t> duplicate_dialogue;
		std::optional<std::size_t> delete_dialogue;
		bool add_dialogue{ false };

		{
			InspectorTabStripScope strip{ "##DialogueKeyTabStrip" };
			if (ImGui::BeginTabBar("##DialogueKeys", InspectorTabBarFlags())) {
				for (std::size_t i{ 0 }; i < document.dialogues.size(); ++i) {
					ImGui::PushID(static_cast<int>(i));
					const bool active{ ImGui::BeginTabItem(document.dialogues[i].name.c_str()) };
					const auto context{ DrawInspectorTabContextMenu(
						"##DialogueTabContext",
						true,
						true,
						document.dialogues.size() > 1,
						"Delete"
					) };
					if (context.rename_requested) {
						rename_dialogue = i;
					}
					if (context.duplicate_requested) {
						duplicate_dialogue = i;
					}
					if (context.remove_requested) {
						delete_dialogue = i;
					}

					if (active) {
						if (state.dialogue_key != document.dialogues[i].name) {
							state.dialogue_key = document.dialogues[i].name;
							state.dialogue_variant_index = 0;
							state.dialogue_preview_page = 0;
						}
						selected_index = i;
						ImGui::EndTabItem();
					}
					ImGui::PopID();
				}

				if (DrawInspectorAddTabButton("+##AddDialogueKey", "Add dialogue key")) {
					add_dialogue = true;
				}
				ApplyInspectorTabBarHorizontalWheel();
				ImGui::EndTabBar();
			}
		}

		if (rename_dialogue.has_value()) {
			const std::size_t index{ *rename_dialogue };
			state.dialogue_rename_key = document.dialogues[index].name;
			state.dialogue_rename.Begin(document.dialogues[index].name);
		}

		if (state.dialogue_rename.active && state.dialogue_rename_key.has_value()) {
			const auto rename_it{ std::ranges::find_if(
				document.dialogues,
				[&](const DialogueEditorEntryDraft& candidate) {
					return candidate.name == state.dialogue_rename_key.value();
				}
			) };

			if (rename_it == document.dialogues.end()) {
				state.dialogue_rename.Cancel();
				state.dialogue_rename_key.reset();
			} else {
				const std::size_t rename_index{ static_cast<std::size_t>(
					std::distance(document.dialogues.begin(), rename_it)
				) };
				const auto result{ DrawInspectorTabRenameModal(
					state.dialogue_rename,
					"Rename Dialogue Key",
					"##RenameDialogueKey",
					[&](std::string_view value) -> std::string {
						if (value.empty()) {
							return "Dialogue key cannot be empty.";
						}
						if (DialogueEditorNameExists(document, value, rename_index)) {
							return "A dialogue key with this name already exists.";
						}
						return {};
					},
					[&](std::string_view value) {
						const std::string renamed{ value };
						RenameDialogueEditorEntry(document, rename_index, renamed);
						state.dialogue_key = renamed;
						mark_changed("Rename Dialogue Key");
					},
					"Rename Dialogue Key"
				) };
				if (result != RenameResult::None) {
					state.dialogue_rename_key.reset();
				}
			}
		}

		if (add_dialogue) {
			const std::string name{
				MakeUniqueDialogueEditorName(document)
			};
			document.dialogues.emplace_back(
				DialogueEditorEntryDraft{
					.name = name,
					.variants = {
						DialogueEditorVariantDraft{ .name = "Variant 1" }
					},
				}
			);
			state.dialogue_key = name;
			state.dialogue_variant_index = 0;
			state.dialogue_preview_page = 0;
			selected_index = document.dialogues.size() - 1;
			mark_changed("Add Dialogue Key");
		}

		if (duplicate_dialogue.has_value()) {
			const std::size_t index{ *duplicate_dialogue };
			auto duplicate{ document.dialogues[index] };
			duplicate.name = MakeUniqueDialogueEditorName(
				document,
				duplicate.name + "_copy"
			);
			document.dialogues.insert(
				document.dialogues.begin() + static_cast<std::ptrdiff_t>(index + 1),
				std::move(duplicate)
			);
			state.dialogue_key = document.dialogues[index + 1].name;
			state.dialogue_variant_index = 0;
			state.dialogue_preview_page = 0;
			selected_index = index + 1;
			mark_changed("Duplicate Dialogue Key");
		}

		if (delete_dialogue.has_value() && document.dialogues.size() > 1) {
			const std::size_t index{ *delete_dialogue };
			const std::string deleted{ document.dialogues[index].name };
			document.dialogues.erase(
				document.dialogues.begin() + static_cast<std::ptrdiff_t>(index)
			);

			if (document.start == deleted) {
				document.start = document.dialogues.front().name;
			}
			for (auto& candidate : document.dialogues) {
				if (candidate.next == deleted) {
					candidate.next.clear();
				}
			}

			selected_index = std::min(index, document.dialogues.size() - 1);
			state.dialogue_key = document.dialogues[selected_index].name;
			state.dialogue_variant_index = 0;
			state.dialogue_preview_page = 0;
			mark_changed("Delete Dialogue Key");
		}

		NormalizeDialogueEditorDocument(document);
		selected_index = DialogueEditorSelectedIndex(
			document,
			state
		);
		auto& entry{ document.dialogues[selected_index] };

		{

			changed |= DrawPropertyRow("Next Dialogue", [&]() {
				bool local_changed{ false };
				const std::string preview{
					entry.next.empty() ? std::string{ "(none)" } : entry.next
				};

				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::BeginCombo("##DialogueNext", preview.c_str())) {
					if (ImGui::Selectable("(none)", entry.next.empty())) {
						entry.next.clear();
						local_changed = true;
						reason = "Clear Next Dialogue";
					}

					for (const auto& candidate : document.dialogues) {
						const bool selected{ entry.next == candidate.name };
						if (ImGui::Selectable(candidate.name.c_str(), selected)) {
							entry.next = candidate.name;
							local_changed = true;
							reason = "Change Next Dialogue";
						}
					}
					ImGui::EndCombo();
				}
				DrawTooltip(
					"Optional key selected by SetNextDialogue() for this dialogue key."
				);
				return local_changed;
			});

			NormalizeDialogueEditorEntry(entry);
			state.dialogue_variant_index = std::min(
				state.dialogue_variant_index,
				entry.variants.size() - 1
			);

			changed |= DrawPropertyRow("Repeatable", [&]() {
				bool repeatable{ entry.repeatable };
				if (!ImGui::Checkbox("##DialogueRepeatable", &repeatable)) {
					return false;
				}
				entry.repeatable = repeatable;
				reason = "Toggle Dialogue Repeatable";
				return true;
			});
			DrawTooltip(
				"If disabled, this dialogue key can only open once. Stored variants are preserved but hidden."
			);

			if (entry.repeatable) {
				changed |= DrawPropertyRow("Variant Behavior", [&]() {
					bool local_changed{ false };
					const char* preview{
						entry.behavior == DialogueBehavior::Sequential ? "Sequential" : "Random"
					};
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::BeginCombo("##DialogueBehavior", preview)) {
						for (const DialogueBehavior behavior : {
							DialogueBehavior::Sequential,
							DialogueBehavior::Random,
						}) {
							const char* label{
								behavior == DialogueBehavior::Sequential ? "Sequential" : "Random"
							};
							if (ImGui::Selectable(label, entry.behavior == behavior)) {
								entry.behavior = behavior;
								local_changed = true;
								reason = "Change Dialogue Variant Behavior";
							}
						}
						ImGui::EndCombo();
					}
					DrawTooltip(
						"Random still uses Initial Variant deterministically on the first open."
					);
					return local_changed;
				});

				changed |= DrawPropertyRow("Initial Variant", [&]() {
					bool local_changed{ false };
					const std::string& preview{ entry.variants[entry.initial_variant].name };
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::BeginCombo("##DialogueInitialVariant", preview.c_str())) {
						for (std::size_t i{ 0 }; i < entry.variants.size(); ++i) {
							if (ImGui::Selectable(
								entry.variants[i].name.c_str(), entry.initial_variant == i
							)) {
								entry.initial_variant = i;
								local_changed = true;
								reason = "Change Initial Dialogue Variant";
							}
						}
						ImGui::EndCombo();
					}
					return local_changed;
				});
			}

			changed |= DrawPropertyRow("Typewriter", [&]() {
				bool local_changed{ false };
				const float spacing{ ImGui::GetStyle().ItemSpacing.x };
				const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
				const float mode_width{ std::max(1.0f, (available - spacing) * 0.5f) };

				const char* mode_preview{
					!entry.typewriter.has_value()
						? "Inherit"
						: (*entry.typewriter ? "Enabled" : "Disabled")
				};
				ImGui::SetNextItemWidth(mode_width);
				if (ImGui::BeginCombo("##DialogueTypewriterOverride", mode_preview)) {
					if (ImGui::Selectable("Inherit", !entry.typewriter.has_value())) {
						entry.typewriter.reset();
						entry.typewriter_duration.reset();
						local_changed = true;
						reason = "Inherit Dialogue Typewriter Setting";
					}
					for (const bool enabled : { true, false }) {
						if (ImGui::Selectable(
								enabled ? "Enabled" : "Disabled",
								entry.typewriter == enabled
							)) {
							entry.typewriter = enabled;
							local_changed = true;
							reason = "Override Dialogue Typewriter Setting";
						}
					}
					ImGui::EndCombo();
				}
				DrawTooltip(
					"Inherit the entity setting, or explicitly enable/disable typewriter text for this dialogue key."
				);

				ImGui::SameLine(0.0f, spacing);
				const bool inheriting{ !entry.typewriter.has_value() };
				const bool effective_typewriter{
					inheriting ? document.typewriter : *entry.typewriter
				};

				if (inheriting) {
					std::string inherited_duration{
						std::to_string(document.defaults.scroll_duration.count()) + "ms"
					};
					ScopedDisabled disabled{ !document.typewriter };
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputText(
						"##DialogueTypewriterDurationInherited",
						&inherited_duration,
						ImGuiInputTextFlags_ReadOnly
					);
					DrawTooltip(
						document.typewriter
							? "Inherited from the dialogue entity's typewriter duration."
							: "The inherited duration is unavailable because typewriter text is disabled on the dialogue entity."
					);
				} else {
					milliseconds duration{
						entry.typewriter_duration.value_or(document.defaults.scroll_duration)
					};
					const bool duration_changed{ DrawDurationTextInput(
						"##DialogueTypewriterDurationOverrideValue",
						duration,
						std::max(1.0f, ImGui::GetContentRegionAvail().x),
						!effective_typewriter,
						entry.typewriter_duration.has_value()
							? "Custom duration for this dialogue key. Right-click to restore the entity duration."
							: "Entity duration used as the starting value. Edit to create a dialogue-specific duration override."
					) };
					if (duration_changed && effective_typewriter) {
						entry.typewriter_duration = duration;
						local_changed = true;
						reason = "Edit Dialogue Typewriter Duration Override";
					}

					if (ImGui::BeginPopupContextItem("##DialogueTypewriterDurationContext")) {
						ImGui::BeginDisabled(!entry.typewriter_duration.has_value());
						if (ImGui::MenuItem("Use Entity Duration")) {
							entry.typewriter_duration.reset();
							local_changed = true;
							reason = "Use Entity Dialogue Typewriter Duration";
						}
						ImGui::EndDisabled();
						ImGui::EndPopup();
					}
				}
				return local_changed;
			});
		}

		if (entry.repeatable) {
			std::optional<std::size_t> rename_variant;
			std::optional<std::size_t> duplicate_variant;
			std::optional<std::size_t> delete_variant;
			bool add_variant{ false };

			{
				InspectorTabStripScope strip{ "##DialogueVariantTabStrip" };
				if (ImGui::BeginTabBar("##DialogueVariants", InspectorTabBarFlags())) {
					for (std::size_t i{ 0 }; i < entry.variants.size(); ++i) {
						ImGui::PushID(static_cast<int>(i));
						const bool active{ ImGui::BeginTabItem(entry.variants[i].name.c_str()) };
						const auto context{ DrawInspectorTabContextMenu(
							"##VariantTabContext",
							true,
							true,
							entry.variants.size() > 1,
							"Delete"
						) };
						if (context.rename_requested) {
							rename_variant = i;
						}
						if (context.duplicate_requested) {
							duplicate_variant = i;
						}
						if (context.remove_requested) {
							delete_variant = i;
						}

						if (active) {
							if (state.dialogue_variant_index != i) {
								state.dialogue_variant_index = i;
								state.dialogue_preview_page = 0;
							}
							ImGui::EndTabItem();
						}
						ImGui::PopID();
					}

					if (DrawInspectorAddTabButton("+##AddDialogueVariant", "Add dialogue variant")) {
						add_variant = true;
					}
					ApplyInspectorTabBarHorizontalWheel();
					ImGui::EndTabBar();
				}
			}

			if (rename_variant.has_value()) {
				state.dialogue_variant_rename_index = *rename_variant;
				state.dialogue_variant_rename.Begin(entry.variants[*rename_variant].name);
			}

			if (state.dialogue_variant_rename.active &&
				state.dialogue_variant_rename_index.has_value()) {
				const std::size_t rename_index{ *state.dialogue_variant_rename_index };
				if (rename_index >= entry.variants.size()) {
					state.dialogue_variant_rename.Cancel();
					state.dialogue_variant_rename_index.reset();
				} else {
					const auto result{ DrawInspectorTabRenameModal(
						state.dialogue_variant_rename,
						"Rename Dialogue Variant",
						"##RenameDialogueVariant",
						[&](std::string_view value) -> std::string {
							if (value.empty()) {
								return "Variant name cannot be empty.";
							}
							if (DialogueVariantNameExists(entry, value, rename_index)) {
								return "A variant with this name already exists.";
							}
							return {};
						},
						[&](std::string_view value) {
							entry.variants[rename_index].name = std::string{ value };
							mark_changed("Rename Dialogue Variant");
						},
						"Rename Dialogue Variant"
					) };
					if (result != RenameResult::None) {
						state.dialogue_variant_rename_index.reset();
					}
				}
			}

			if (add_variant) {
				entry.variants.emplace_back(DialogueEditorVariantDraft{
					.name = MakeUniqueDialogueVariantName(entry, "Variant " + std::to_string(entry.variants.size() + 1)),
				});
				state.dialogue_variant_index = entry.variants.size() - 1;
				state.dialogue_preview_page = 0;
				mark_changed("Add Dialogue Variant");
			}

			if (duplicate_variant.has_value()) {
				const std::size_t index{ *duplicate_variant };
				auto duplicate{ entry.variants[index] };
				duplicate.name = MakeUniqueDialogueVariantName(entry, duplicate.name + " Copy");
				entry.variants.insert(
					entry.variants.begin() +
						static_cast<std::ptrdiff_t>(index + 1),
					std::move(duplicate)
				);
				state.dialogue_variant_index = index + 1;
				state.dialogue_preview_page = 0;
				mark_changed("Duplicate Dialogue Variant");
			}

			if (delete_variant.has_value() &&
				entry.variants.size() > 1) {
				const std::size_t index{ *delete_variant };
				entry.variants.erase(
					entry.variants.begin() +
						static_cast<std::ptrdiff_t>(index)
				);
				NormalizeDialogueEditorEntry(entry);
				state.dialogue_variant_index = std::min(
					state.dialogue_variant_index,
					entry.variants.size() - 1
				);
				state.dialogue_preview_page = 0;
				mark_changed("Delete Dialogue Variant");
			}
		} else {
			state.dialogue_variant_index = 0;
		}

		NormalizeDialogueEditorEntry(entry);
		const std::size_t variant_index{
			std::min(
				state.dialogue_variant_index,
				entry.variants.size() - 1
			)
		};
		auto& variant{ entry.variants[variant_index] };

		DialoguePageProperties preview_properties{ document.defaults };
		if (entry.typewriter_duration.has_value()) {
			preview_properties.scroll_duration = *entry.typewriter_duration;
		}

		const DialoguePageNumberPreview page_number_preview{
			BuildDialoguePageNumberPreview(
				target.ctx,
				variant.source,
				preview_properties
			)
		};


		std::vector<std::vector<RichTextPortraitExpressionOption>> portrait_expression_options;
		std::vector<RichTextPortraitSpeakerOption> portrait_speaker_options;
		const auto portrait_actor_keys{ SortedDialoguePortraitActorKeys(document.portrait_actors) };
		portrait_expression_options.reserve(portrait_actor_keys.size());
		portrait_speaker_options.reserve(portrait_actor_keys.size());
		for (const auto& actor_key : portrait_actor_keys) {
			const auto& actor{ document.portrait_actors.at(actor_key) };
			portrait_expression_options.emplace_back();
			auto& expression_options{ portrait_expression_options.back() };
			const auto expression_keys{ SortedDialoguePortraitExpressionKeys(actor) };
			expression_options.reserve(expression_keys.size());
			for (const auto& expression_key : expression_keys) {
				const auto& expression{ actor.expressions.at(expression_key) };
				expression_options.emplace_back(RichTextPortraitExpressionOption{
					.key = expression_key,
					.label = expression.display_name,
				});
			}
			portrait_speaker_options.emplace_back(RichTextPortraitSpeakerOption{
				.key = actor_key,
				.label = actor.display_name,
				.default_expression = actor.default_expression,
				.expressions = std::span<const RichTextPortraitExpressionOption>{ expression_options },
			});
		}

		ImGui::PushID(static_cast<int>(selected_index));
		ImGui::PushID(static_cast<int>(variant_index));
		if (DrawRichTextEditor(
				target.ctx,
				variant.source,
				document.defaults.text_defaults,
				RichTextEditorOptions{
					.show_preview = false,
					.line_count = 8,
					.show_defaults = false,
					.show_page_numbers_button = true,
					.line_page_numbers = std::span<const std::size_t>{
						page_number_preview.line_page_numbers
					},
					.page_number_preview_source =
						page_number_preview.source,
					.show_page_duration_button = true,
					.page_instant_tag = ::ptgn::impl::kDialogueInstantPageTag,
					.page_duration_tag_prefix = ::ptgn::impl::kDialogueDurationPageTagPrefix,
					.page_duration_tag_suffix = ::ptgn::impl::kDialogueDurationPageTagSuffix,
					.page_duration_default = entry.typewriter_duration.value_or(
						document.defaults.scroll_duration
					),
					.show_portrait_button = true,
					.portrait_speakers = std::span<const RichTextPortraitSpeakerOption>{
						portrait_speaker_options
					},
					.portrait_tag_prefix = ::ptgn::impl::kDialoguePortraitPageTagPrefix,
					.portrait_tag_suffix = ::ptgn::impl::kDialoguePortraitPageTagSuffix,
				}
			)) {
			mark_changed("Edit Dialogue Variant");
		}

		auto pages{ ::ptgn::impl::PaginateDialogueSource(
			target.ctx.editor.GetAssetManager(),
			variant.source,
			preview_properties
		) };

		if (pages.empty()) {
			state.dialogue_preview_page = 0;
			ImGui::TextDisabled("No preview page.");
		} else {
			state.dialogue_preview_page = std::min(
				state.dialogue_preview_page,
				pages.size() - 1
			);

			ImGui::SeparatorText("Preview");

			TextBox preview_box{
				pages[state.dialogue_preview_page]
					.properties
					.ToTextBox()
			};
			const TextLayout preview_layout{
				::ptgn::impl::BuildTextLayout(
					target.ctx.editor.GetAssetManager(),
					pages[state.dialogue_preview_page]
						.styled_text,
					preview_box
				)
			};
			constexpr float preview_height{ 190.0f };
			const float preview_padding{ 24.0f };
			const float preview_available_width{
				std::max(
					1.0f,
					ImGui::GetContentRegionAvail().x
				)
			};
			const float preview_available_height{
				std::max(
					1.0f,
					preview_height -
						ImGui::GetStyle().WindowPadding.y * 2.0f
				)
			};
			const bool horizontal_overflow{
				preview_layout.size.x + preview_padding >
				preview_available_width
			};
			const bool vertical_overflow{
				preview_layout.size.y + preview_padding >
				preview_available_height
			};
			ImGuiWindowFlags preview_flags{ ImGuiWindowFlags_None };
			if (!horizontal_overflow && !vertical_overflow) {
				preview_flags |=
					ImGuiWindowFlags_NoScrollbar |
					ImGuiWindowFlags_NoScrollWithMouse;
			} else if (horizontal_overflow) {
				preview_flags |= ImGuiWindowFlags_HorizontalScrollbar;
			}

			ImGui::BeginChild(
				"##DialoguePagePreview",
				ImVec2{ -FLT_MIN, preview_height },
				ImGuiChildFlags_Borders |
					ImGuiChildFlags_ResizeY,
				preview_flags
			);
			DrawRichTextPreview(
				target.ctx,
				pages[state.dialogue_preview_page]
					.styled_text,
				&preview_box
			);
			ImGui::EndChild();

			DrawDialoguePreviewNavigation(
				state,
				pages.size()
			);
		}

		changed |= DrawDialogueAppearanceControls(
			target.ctx, dialogue_entity, document, selected_index, reason
		);


		ImGui::PopID();
		ImGui::PopID();

		if (!changed) {
			return false;
		}

		NormalizeDialogueEditorDocument(document);
		::ptgn::impl::DialogueData updated{ *before };
		updated.SetDefinition(
			BuildDialogueEditorDefinition(document)
		);
		target.template SetLive<::ptgn::impl::DialogueData>(updated);

		auto after{ target.template Capture<::ptgn::impl::DialogueData>() };
		TrackComponentState(
			target,
			reason,
			std::move(before),
			std::move(after),
			true
		);
		return true;
	}
}


bool SetSliderValueTextPartEnabled(EntityInspectorTarget& target, bool enabled) {
	auto before{ target.template Capture<::ptgn::impl::SliderData>() };
	if (!before || before->value_text.has_value() == enabled) {
		return false;
	}

	auto data{ *before };
	data.value_text = enabled
		? std::optional<SliderValueTextConfig>{ SliderValueTextConfig{} }
		: std::nullopt;
	target.template SetLive<::ptgn::impl::SliderData>(data);
	::ptgn::impl::SliderSystem::SynchronizeEntity(target.entity);
	auto after{ target.template Capture<::ptgn::impl::SliderData>() };
	TrackComponentState(
		target,
		enabled ? "Add Slider Value Text" : "Remove Slider Value Text",
		std::move(before),
		std::move(after),
		true
	);
	return true;
}

bool DrawSliderPartsTabs(EntityInspectorTarget& target, Slider slider) {
	bool changed{ false };

	Entity track{ slider.GetTrack() };
	Button thumb{ slider.GetThumb() };
	const auto slider_data{ target.template Capture<::ptgn::impl::SliderData>() };
	const bool has_value_text{ slider_data && slider_data->value_text.has_value() };
	const bool has_any_part{ static_cast<bool>(track) || static_cast<bool>(thumb) || has_value_text };
	const bool has_all_parts{ track && thumb && has_value_text };

	enum class RemovePart {
		None,
		Track,
		Thumb,
		ValueText,
	};
	RemovePart remove_after_tabs{ RemovePart::None };

	auto draw_add_popup = [&]() {
		if (!ImGui::BeginPopup("##AddSliderPartPopup")) {
			return;
		}

		if (!track && ImGui::MenuItem("Track")) {
			Entity created{ slider.EnsureTrack() };
			if (created) {
				(void)RecordCreatedEntityPreservingSelection(target.ctx, created);
				changed = true;
			}
		}
		if (!thumb && ImGui::MenuItem("Thumb")) {
			Button created{ slider.EnsureThumb() };
			if (created) {
				(void)RecordCreatedEntityPreservingSelection(target.ctx, created);
				::ptgn::impl::SliderSystem::SynchronizeEntity(created);
				changed = true;
			}
		}
		if (!has_value_text && ImGui::MenuItem("Value Text")) {
			changed |= SetSliderValueTextPartEnabled(target, true);
		}

		ImGui::EndPopup();
	};

	const bool add_requested{ DrawInspectorTabCollection(
		!has_any_part,
		InspectorTabCollectionOptions{
			.scope_id = "##SliderPartsTabStrip",
			.tab_bar_id = "##SliderParts",
			.add_tab_id = "+##AddSliderPart",
			.empty_add_label = "Add Slider Part",
			.add_tooltip = "Add slider part",
			.show_add = !has_all_parts,
		},
		[&]() {
			if (track) {
				ScopedID scope{ "TrackPartTab" };
				const bool selected{ ImGui::BeginTabItem("Track") };
				const auto context{ DrawInspectorTabContextMenu(
					"##SliderTrackContext", false, false, true, "Remove Part"
				) };
				if (context.remove_requested) {
					remove_after_tabs = RemovePart::Track;
				}
				if (selected) {
					if (remove_after_tabs != RemovePart::Track) {
						changed |= DrawSliderTrackTransform(target, track);
						changed |= DrawSliderTrackVisual(target, slider, track);
					}
					ImGui::EndTabItem();
				}
			}

			if (thumb) {
				ScopedID scope{ "ThumbPartTab" };
				const bool selected{ ImGui::BeginTabItem("Thumb") };
				const auto context{ DrawInspectorTabContextMenu(
					"##SliderThumbContext", false, false, true, "Remove Part"
				) };
				if (context.remove_requested) {
					remove_after_tabs = RemovePart::Thumb;
				}
				if (selected) {
					if (remove_after_tabs != RemovePart::Thumb) {
						EntityInspectorTarget thumb_target{ .ctx = target.ctx, .entity = thumb };
						changed |= DrawFocusedButtonInteraction(
							thumb_target, FocusedUIControlType::Button
						);
						if (ImGui::TreeNodeEx(
								"Transform##SliderThumbTransform",
								ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
							)) {
							ScopedIndent thumb_transform_indent;
							auto slider_before{
								target.template Capture<::ptgn::impl::SliderData>()
							};
							const bool transform_changed{
								DrawTransformSection(thumb_target, false, true, false)
							};
							if (transform_changed) {
								::ptgn::impl::SliderSystem::SynchronizeEntity(thumb);
								auto slider_after{
									target.template Capture<::ptgn::impl::SliderData>()
								};
								TrackComponentState(
									target, "Move Slider Thumb", std::move(slider_before),
									std::move(slider_after), true
								);
								changed = true;
							}
							ImGui::TreePop();
						}
						changed |= DrawFocusedButtonAppearance(
							thumb_target, FocusedUIControlType::Button
						);
					}
					ImGui::EndTabItem();
				}
			}

			if (has_value_text) {
				ScopedID scope{ "ValueTextPartTab" };
				const bool selected{ ImGui::BeginTabItem("Value Text") };
				const auto context{ DrawInspectorTabContextMenu(
					"##SliderValueTextContext", false, false, true, "Remove Part"
				) };
				if (context.remove_requested) {
					remove_after_tabs = RemovePart::ValueText;
				}
				if (selected) {
					if (remove_after_tabs != RemovePart::ValueText) {
						auto before{ target.template Capture<::ptgn::impl::SliderData>() };
						if (before) {
							auto data{ *before };
							if (!data.value_text.has_value()) {
								data.value_text = SliderValueTextConfig{};
							}
							if (DrawSliderValueTextConfig(target, data)) {
								target.template SetLive<::ptgn::impl::SliderData>(data);
								::ptgn::impl::SliderSystem::SynchronizeEntity(target.entity);
								auto after{ target.template Capture<::ptgn::impl::SliderData>() };
								TrackComponentState(
									target,
									"Edit Slider Value Text",
									std::move(before),
									std::move(after),
									true
								);
								changed = true;
							}
						}
					}
					ImGui::EndTabItem();
				}
			}
		}
	) };

	if (add_requested) {
		ImGui::OpenPopup("##AddSliderPartPopup");
	}
	draw_add_popup();

	switch (remove_after_tabs) {
		case RemovePart::Track:
			if (track) {
				target.ctx.commands.DeleteEntity(track);
				changed = true;
			}
			break;
		case RemovePart::Thumb:
			if (thumb) {
				target.ctx.commands.DeleteEntity(thumb);
				changed = true;
			}
			break;
		case RemovePart::ValueText:
			changed |= SetSliderValueTextPartEnabled(target, false);
			break;
		case RemovePart::None:
			break;
	}

	return changed;
}

template <typename Target>
bool DrawFocusedControlSpecific(Target& target, FocusedUIControlType type) {
	bool changed{ false };

	switch (type) {
		case FocusedUIControlType::Button: break;

		case FocusedUIControlType::ToggleButton: break;

		case FocusedUIControlType::Slider: {
			changed |= DrawFocusedComponent<Target, ::ptgn::impl::SliderData>(
				target, "Slider", [&](::ptgn::impl::SliderData& data) {
					return DrawSliderData(target, data);
				}
			);

			if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
				Slider slider{ target.entity };
				changed |= DrawSliderPartsTabs(target, slider);
			}
			break;
		}

		case FocusedUIControlType::Dropdown:
			changed |= DrawFocusedComponent<Target, ::ptgn::impl::DropdownData>(
				target, "Dropdown",
				[&](::ptgn::impl::DropdownData& data) {
					bool local_changed{ false };


					{
						ScopedID item_size_scope{ "DropdownItemSize" };
						bool enabled{ data.button_size.has_value() };
						V2_float displayed{ data.button_size.value_or(V2_float{}) };
						bool fields_changed{ false };
						const bool row_changed{ DrawOptionalPropertyRow(
							"Item Size", enabled, false, [&]() {
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
					DrawTooltip("Size used by each item. When disabled, uses the dropdown/header size.");

					local_changed |= DrawValue(target.ctx, "Item Offset", data.button_offset);
					DrawTooltip("Offset added to each item position after automatic layout.");

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

		case FocusedUIControlType::Dialogue:
			changed |= DrawFocusedDialogueControl(target);
			break;

		case FocusedUIControlType::Conflict: changed |= DrawUIControlConflict(target); break;

		case FocusedUIControlType::None:	 break;
	}

	return changed;
}

bool DrawManagedVisualChildContents(EditorContext& ctx, Entity child) {
	if (!child) {
		return false;
	}
	EntityInspectorTarget child_target{ .ctx = ctx, .entity = child };
	bool changed{ DrawVisualSection(child_target) };
	changed |= DrawTransformSection(child_target);
	return changed;
}

bool DrawManagedVisualPartsTabs(
	EditorContext& ctx,
	std::span<const std::pair<std::string_view, Entity>> parts,
	const char* id,
	std::span<const InspectorAction> add_actions = {}
) {
	bool changed{ false };
	Entity remove_after_tabs{};

	const bool has_any_part{
		std::ranges::any_of(parts, [](const auto& part) {
			return static_cast<bool>(part.second);
		})
	};

	auto draw_add_popup = [&]() {
		if (!ImGui::BeginPopup("##AddManagedVisualPartPopup")) {
			return;
		}
		for (std::size_t action_index{ 0 }; action_index < add_actions.size(); ++action_index) {
			const auto& action{ add_actions[action_index] };
			ScopedID action_scope{ static_cast<int>(action_index) };
			ImGui::BeginDisabled(!action.enabled);
			const std::string action_label{ action.label };
			if (ImGui::MenuItem(action_label.c_str()) && action.invoke) {
				action.invoke();
				changed = true;
			}
			ImGui::EndDisabled();
			if (!action.tooltip.empty() &&
				ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip(
					"%.*s", static_cast<int>(action.tooltip.size()), action.tooltip.data()
				);
			}
		}
		ImGui::EndPopup();
	};

	const bool add_requested{ DrawInspectorTabCollection(
		!has_any_part,
		InspectorTabCollectionOptions{
			.scope_id = id,
			.tab_bar_id = "##ManagedVisualParts",
			.add_tab_id = "+##AddManagedVisualPart",
			.empty_add_label = "Add UI Part",
			.add_tooltip = "Add UI part",
			.show_add = !add_actions.empty(),
		},
		[&]() {
			for (std::size_t index{ 0 }; index < parts.size(); ++index) {
				const auto& [fallback, child]{ parts[index] };
				if (!child) {
					continue;
				}
				ScopedID part_scope{ static_cast<int>(index) };
				const std::string label{ fallback };
				const bool selected{ ImGui::BeginTabItem(label.c_str()) };
				const auto context{ DrawInspectorTabContextMenu(
					"##ManagedVisualPartContext", false, false, true, "Remove Part"
				) };
				if (context.remove_requested) {
					remove_after_tabs = child;
				}
				if (selected) {
					if (child != remove_after_tabs) {
						changed |= DrawManagedVisualChildContents(ctx, child);
					}
					ImGui::EndTabItem();
				}
			}
		}
	) };

	if (add_requested) {
		ImGui::OpenPopup("##AddManagedVisualPartPopup");
	}
	draw_add_popup();

	if (remove_after_tabs) {
		ctx.commands.DeleteEntity(remove_after_tabs);
		changed = true;
	}

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

Entity CreateTooltipPartForInspector(EditorContext& ctx, Entity tooltip, bool text) {
	if (!tooltip) {
		return {};
	}

	Entity created;
	if (text) {
		Text text_entity{ CreateText(tooltip.GetScene()) };
		text_entity.Add<Tag>("Tooltip Text");
		text_entity.Add<::ptgn::impl::TooltipTextPart>();
		text_entity.Add<Tint>(color::Transparent);
		SetParent(text_entity, tooltip);
		created = text_entity;
	} else {
		Entity background{ CreateSprite(tooltip.GetScene(), {}, {}, Origin::Center) };
		background.Add<Tag>("Tooltip Sprite");
		background.Add<::ptgn::impl::TooltipBackgroundPart>();
		background.Add<Tint>(color::Transparent);
		SetParent(background, tooltip);
		created = background;
	}

	return RecordCreatedEntityPreservingSelection(ctx, created);
}

template <typename Target>
bool DrawUISectionImpl(Target& target) {
	if (!HasUISection(target)) {
		return false;
	}

	if (const auto child_info{ GetButtonChildInfo(target) }) {
		const bool open{
			ImGui::CollapsingHeader("Managed UI Part##ButtonChildUI", ImGuiTreeNodeFlags_None)
		};

		if (!open) {
			return false;
		}

		ScopedIndent section_indent;
		auto& editor_state{ GetInspectorUiState(target.GetInspectorTargetKey()) };
		DrawButtonVisualStateSelector(editor_state.button_visual_state);
		return false;
	}

	const FocusedUIControlType control_type{ GetFocusedUIControlType(target) };
	const bool has_toggle_group{
		HasTargetComponent<Target, ::ptgn::impl::ToggleButtonGroupData>(target)
	};
	const bool has_tooltip{ HasTargetComponent<Target, ::ptgn::impl::TooltipData>(target) };
	std::string ui_label{ "UI" };
	if (control_type != FocusedUIControlType::None && control_type != FocusedUIControlType::Conflict) {
		ui_label = std::string{ FocusedUIControlLabel(control_type) };
	} else if (has_toggle_group) {
		ui_label = "Toggle Group";
	} else if (has_tooltip) {
		ui_label = "Tooltip";
	}

	const bool archetype_owned{ ArchetypeOwnsUI(ResolveInspectorArchetype(target)) };
	const auto header{ DrawInspectorSectionHeader(
		ui_label,
		"UISection",
		InspectorSectionOptions{
			.default_open = true,
			.removable = !archetype_owned,
		}
	) };

	if (header.remove_requested) {
		return RemoveComponentSet(target, ui_label, UISectionComponents{});
	}

	if (!header.open) {
		return false;
	}

	ScopedIndent section_indent;
	bool changed{ false };

	if (control_type != FocusedUIControlType::None) {
		// UI control identity is owned by the entity archetype and is not editable here.
		if (control_type == FocusedUIControlType::Dialogue) {
			changed |= DrawFocusedControlSpecific(target, control_type);
		} else if (control_type != FocusedUIControlType::Conflict) {
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
					changed |= DrawFocusedButtonInteraction(target, control_type);

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
						changed |= DrawFocusedButtonAppearance(target, control_type);
					}
				}
			}
		} else {
			changed |= DrawUIControlConflict(target);
		}
	}

	if (has_toggle_group) {
		if (ui_label != "Toggle Group") {
			ImGui::SeparatorText("Toggle Group");
		}
		changed |= DrawFocusedComponent<Target, ::ptgn::impl::ToggleButtonGroupData>(
			target, "Toggle Group", [&](::ptgn::impl::ToggleButtonGroupData& data) {
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

	if (has_tooltip) {
		if (ui_label != "Tooltip") {
			ImGui::SeparatorText("Tooltip");
		}
		{
			changed |= DrawFocusedComponent<Target, ::ptgn::impl::TooltipData>(
				target, "Tooltip", [&](::ptgn::impl::TooltipData& data) {
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
				const Entity text_part{ FindTooltipManagedPart(target.entity, true) };
				const Entity background_part{ FindTooltipManagedPart(target.entity, false) };
				const std::array tooltip_parts{
					std::pair<std::string_view, Entity>{ "Text", text_part },
					std::pair<std::string_view, Entity>{ "Background", background_part },
				};
				std::vector<InspectorAction> add_actions;
				if (!text_part) {
					add_actions.push_back(InspectorAction{
						.label = "Text",
						.tooltip = "Add the tooltip text part.",
						.invoke = [&]() {
							(void)CreateTooltipPartForInspector(target.ctx, target.entity, true);
						},
					});
				}
				if (!background_part) {
					add_actions.push_back(InspectorAction{
						.label = "Background",
						.tooltip = "Add the tooltip background part.",
						.invoke = [&]() {
							(void)CreateTooltipPartForInspector(target.ctx, target.entity, false);
						},
					});
				}
				changed |= DrawManagedVisualPartsTabs(
					target.ctx, tooltip_parts, "##TooltipPartTabs", add_actions
				);
			}
		}
	}



	return changed;
}


} // namespace

bool DrawUISection(EntityInspectorTarget& target) {
	return DrawUISectionImpl(target);
}

bool DrawUISection(PrefabInspectorTarget& target) {
	return DrawUISectionImpl(target);
}

} // namespace ptgn::editor::inspector
