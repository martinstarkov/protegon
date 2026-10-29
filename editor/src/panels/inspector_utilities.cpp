#include <cmath>
#include <limits>
#include <type_traits>

#include "core/util/time.h"
#include "editor/editor_icons.h"
#include "panels/inspector_archetype_inspector.h"
#include "panels/inspector_scripts.h"
#include "runtime/physics/lifetime.h"
#include "runtime/scripting/variables.h"

namespace ptgn::editor::inspector {

namespace {

[[nodiscard]] TimerKey MakeUniqueTimerKey(const ::ptgn::impl::Timers& timers) {
	for (std::size_t index{ 1 };; ++index) {
		TimerKey candidate{ std::string{ "Timer " } + std::to_string(index) };

		bool exists{ std::ranges::any_of(timers.timers, [&candidate](const TimerEntry& entry) {
			return entry.config.key == candidate;
		}) };

		if (!exists) {
			return candidate;
		}
	}
}

[[nodiscard]] bool IsTimerRuntimeActive(EditorContext& ctx) {
	return ctx.editor.IsPlaying() || ctx.editor.IsDirectRuntime();
}

void SyncTimerRuntimeSnapshot(Entity entity, const TimerKey& live_key, TimerEntry& edited_entry) {
	if (!entity) {
		return;
	}

	const auto* live_timers{ entity.TryGet<::ptgn::impl::Timers>() };
	if (!live_timers) {
		return;
	}

	auto it{ std::ranges::find_if(live_timers->timers, [&live_key](const TimerEntry& entry) {
		return entry.config.key == live_key;
	}) };
	if (it != live_timers->timers.end()) {
		edited_entry.runtime = it->runtime;
	}
}

void DrawRuntimeTooltipAboveLastItem(const char* tooltip) {
	if (!tooltip || !*tooltip || !ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		return;
	}

	ImVec2 min{ ImGui::GetItemRectMin() };
	ImVec2 max{ ImGui::GetItemRectMax() };
	ImVec2 position{ (min.x + max.x) * 0.5f, min.y - ImGui::GetStyle().ItemSpacing.y };
	ImGui::SetNextWindowPos(position, ImGuiCond_Always, ImVec2{ 0.5f, 1.0f });
	ImGui::BeginTooltip();
	ImGui::TextUnformatted(tooltip);
	ImGui::EndTooltip();
}

bool DrawRuntimeIconButton(const char* id, EditorIcon icon, const char* tooltip) {
	bool pressed{ DrawEditorIconButton(id, icon, "") };
	DrawRuntimeTooltipAboveLastItem(tooltip);
	return pressed;
}

milliseconds timer_runtime_adjustment{ 100 };

void DrawTimerRuntimeControls(Entity entity, const TimerKey& live_key, TimerEntry& edited_entry) {
	TimerHandle timer{ GetTimer(entity, live_key) };
	if (!timer) {
		ImGui::TextDisabled("Runtime timer is unavailable.");
		return;
	}

	bool runtime_changed{ false };
	const ImGuiStyle& style{ ImGui::GetStyle() };
	float spacing{ style.ItemInnerSpacing.x };

	bool active{ timer.IsRunning() || timer.IsPaused() };
	if (DrawRuntimeIconButton(
			"##TimerStartStop", active ? EditorIcon::Stop : EditorIcon::Play,
			active ? "Stop and reset timer" : "Start timer"
		)) {
		runtime_changed |=
			active ? timer.Stop() : (timer.IsCompleted() ? timer.Restart() : timer.Start());
	}

	ImGui::SameLine(0.0f, spacing);

	bool paused{ timer.IsPaused() };
	bool stopped{ !timer.IsRunning() && !paused };
	ImGui::BeginDisabled(stopped);
	if (DrawRuntimeIconButton(
			"##TimerPauseResume", paused ? EditorIcon::Play : EditorIcon::Pause,
			paused ? "Resume timer" : "Pause timer"
		)) {
		runtime_changed |= paused ? timer.Resume() : timer.Pause();
	}
	ImGui::EndDisabled();

	ImGui::SameLine(0.0f, spacing);
	if (DrawRuntimeIconButton("##TimerRewind", EditorIcon::StepBackward, "Rewind timer")) {
		runtime_changed |= timer.Rewind(timer_runtime_adjustment);
	}

	ImGui::SameLine(0.0f, spacing);
	float adjustment_width{ ImGui::CalcTextSize("1000ms").x + style.FramePadding.x * 2.0f };
	ImGui::BeginGroup();
	DrawDurationTextInput(
		"##TimerRuntimeAdjustment", timer_runtime_adjustment, adjustment_width, false, ""
	);
	ImGui::EndGroup();
	DrawRuntimeTooltipAboveLastItem("Amount to rewind or advance.");

	timer_runtime_adjustment = std::max(0ms, timer_runtime_adjustment);

	ImGui::SameLine(0.0f, spacing);
	if (DrawRuntimeIconButton("##TimerAdvance", EditorIcon::StepForward, "Advance timer")) {
		runtime_changed |= timer.Advance(timer_runtime_adjustment);
	}

	ImGui::SameLine(0.0f, spacing);
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled(
		"Elapsed count: %llu", static_cast<unsigned long long>(timer.ElapsedCount())
	);

	const char* state{ timer.IsPaused()		 ? "Paused"
					   : timer.IsRunning()	 ? "Running"
					   : timer.IsCompleted() ? "Complete"
											 : "Stopped" };
	std::string elapsed{ FormatDurationCompact(timer.Elapsed()) };
	std::string duration{ FormatDurationCompact(timer.Duration()) };

	ImGui::Text("%s  %s / %s", state, elapsed.c_str(), duration.c_str());
	ImGui::ProgressBar(timer.Progress(), ImVec2{ -FLT_MIN, 0.0f });

	if (runtime_changed) {
		SyncTimerRuntimeSnapshot(entity, live_key, edited_entry);
	}
}

milliseconds lifetime_runtime_adjustment{ 100 };

void DrawLifetimeRuntimeControls(Entity entity) {
	auto* lifetime{ entity ? entity.TryGet<Lifetime>() : nullptr };
	if (!lifetime) {
		ImGui::TextDisabled("Runtime lifetime is unavailable.");
		return;
	}

	const ImGuiStyle& style{ ImGui::GetStyle() };
	float spacing{ style.ItemInnerSpacing.x };
	bool active{ lifetime->IsRunning() || lifetime->IsPaused() };

	if (DrawRuntimeIconButton(
			"##LifetimeStartStop", active ? EditorIcon::Stop : EditorIcon::Play,
			active ? "Stop and reset lifetime timer" : "Start lifetime timer"
		)) {
		if (active) {
			lifetime->Stop();
		} else {
			lifetime->Start();
		}
	}

	ImGui::SameLine(0.0f, spacing);

	bool paused{ lifetime->IsPaused() };
	bool stopped{ !lifetime->IsRunning() && !paused };
	ImGui::BeginDisabled(stopped);
	if (DrawRuntimeIconButton(
			"##LifetimePauseResume", paused ? EditorIcon::Play : EditorIcon::Pause,
			paused ? "Resume lifetime timer" : "Pause lifetime timer"
		)) {
		if (paused) {
			lifetime->Resume();
		} else {
			lifetime->Pause();
		}
	}
	ImGui::EndDisabled();

	ImGui::SameLine(0.0f, spacing);
	if (DrawRuntimeIconButton(
			"##LifetimeRewind", EditorIcon::StepBackward, "Rewind lifetime timer"
		)) {
		lifetime->Rewind(lifetime_runtime_adjustment);
	}

	ImGui::SameLine(0.0f, spacing);
	float adjustment_width{ ImGui::CalcTextSize("1000ms").x + style.FramePadding.x * 2.0f };
	ImGui::BeginGroup();
	DrawDurationTextInput(
		"##LifetimeRuntimeAdjustment", lifetime_runtime_adjustment, adjustment_width, false, ""
	);
	ImGui::EndGroup();
	DrawRuntimeTooltipAboveLastItem("Amount to rewind or advance the lifetime timer.");

	lifetime_runtime_adjustment = std::max(0ms, lifetime_runtime_adjustment);

	ImGui::SameLine(0.0f, spacing);
	if (DrawRuntimeIconButton(
			"##LifetimeAdvance", EditorIcon::StepForward, "Advance lifetime timer"
		)) {
		lifetime->Advance(entity, lifetime_runtime_adjustment);
		if (!entity || !entity.Has<Lifetime>()) {
			return;
		}
	}

	milliseconds elapsed{ lifetime->Elapsed() };
	milliseconds duration{ lifetime->duration };
	bool completed{ lifetime->HasRun() && elapsed >= duration };
	const char* state{ lifetime->IsPaused()	   ? "Paused"
					   : lifetime->IsRunning() ? "Running"
					   : completed			   ? "Complete"
											   : "Stopped" };
	std::string elapsed_text{ FormatDurationCompact(elapsed) };
	std::string duration_text{ FormatDurationCompact(duration) };

	ImGui::Text("%s  %s / %s", state, elapsed_text.c_str(), duration_text.c_str());
	ImGui::ProgressBar(lifetime->Progress(), ImVec2{ -FLT_MIN, 0.0f });
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

	static RenameModalState rename_state{};
	static std::optional<std::size_t> rename_index{};
	std::optional<std::size_t> remove_index;

	bool add_requested{ DrawInspectorTabCollection(
		timers.timers.empty(),
		InspectorTabCollectionOptions{
			.scope_id		 = "##TimerTabStrip",
			.tab_bar_id		 = "##TimerTabs",
			.add_tab_id		 = "+##AddTimer",
			.empty_add_label = "Add Timer",
			.add_tooltip	 = "Add timer",
		},
		[&]() {
			for (std::size_t index{ 0 }; index < timers.timers.size(); ++index) {
				ScopedID timer_scope{ static_cast<int>(index) };

				auto& entry{ timers.timers[index] };
				TimerKey live_key{ entry.config.key };

				std::string label{ entry.config.key.value.empty()
									   ? std::string{ "Timer " } + std::to_string(index + 1)
									   : entry.config.key.value };
				bool selected{ BeginInspectorTabItem(label.c_str()) };
				InspectorTabContextResult context_menu{
					DrawInspectorTabContextMenu("##TimerTabContext", true, false, true, "Delete")
				};

				if (context_menu.rename_requested) {
					rename_index = index;
					rename_state.Begin(entry.config.key.value);
				}

				if (context_menu.remove_requested) {
					remove_index = index;
					set_undo("Remove Timer", "##RemoveTimerEdit");
				}

				if (selected) {
					if (!remove_index.has_value()) {
						if constexpr (requires { target.entity; }) {
							if (target.entity && IsTimerRuntimeActive(target.ctx)) {
								DrawTimerRuntimeControls(target.entity, live_key, entry);
							}
						}

						if (DrawValue(target.ctx, "Duration", entry.config.duration)) {
							changed = true;
							set_undo("Change Timer Duration", "##TimerDurationEdit");
						}

						if (DrawValue(target.ctx, "Mode", entry.config.mode)) {
							changed = true;
							set_undo("Change Timer Mode", "##TimerModeEdit");
						}

						if (DrawValue(
								target.ctx, "Start Automatically", entry.config.start_automatically
							)) {
							changed = true;
							set_undo("Change Timer Auto Start", "##TimerAutoStartEdit");
						}
						DrawTooltip("Unchecked: start this timer manually or with a Timer Action.");
					}

					ImGui::EndTabItem();
				}
			}
		}
	) };

	if (add_requested) {
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

	if (remove_index.has_value()) {
		std::size_t index{ *remove_index };
		if (index < timers.timers.size()) {
			timers.timers.erase(timers.timers.begin() + static_cast<std::ptrdiff_t>(index));
			changed = true;
		}

		if (rename_index.has_value()) {
			if (*rename_index == index) {
				rename_state.Cancel();
				rename_index.reset();
			} else if (*rename_index > index) {
				--*rename_index;
			}
		}
	}

	if (rename_state.active && rename_index.has_value()) {
		std::size_t index{ *rename_index };
		if (index >= timers.timers.size()) {
			rename_state.Cancel();
			rename_index.reset();
		} else {
			DrawInspectorTabRenameModal(
				rename_state, "Rename Timer", "##RenameTimer",
				[&](std::string_view name) {
					if (name.empty()) {
						return std::string{ "Timer name cannot be empty." };
					}

					for (std::size_t other{ 0 }; other < timers.timers.size(); ++other) {
						if (other != index && timers.timers[other].config.key.value == name) {
							return std::string{ "Timer names must be unique on an entity." };
						}
					}

					return std::string{};
				},
				[&](std::string_view name) {
					if (index >= timers.timers.size()) {
						return;
					}

					auto& entry{ timers.timers[index] };
					TimerKey old_key{ entry.config.key };
					TimerKey new_key{ std::string{ name } };

					if (old_key == new_key) {
						return;
					}

					entry.config.key = new_key;
					if (renames) {
						renames->push_back(
							TimerRename{
								.old_key = old_key,
								.new_key = new_key,
							}
						);
					}

					changed = true;
					set_undo("Rename Timer", "##RenameTimerEdit");
				},
				""
			);

			if (!rename_state.active) {
				rename_index.reset();
			}
		}
	}

	return changed;
}

template <typename Target>
bool DrawTimersComponent(Target& target) {
	using Timers = ::ptgn::impl::Timers;

	ScopedID target_scope{ target.Id() };
	ScopedID component_scope{ static_cast<int>(Hash<Timers>()) };
	auto before{ target.template Capture<Timers>() };
	if (!before) {
		return false;
	}

	auto header{ DrawInspectorSectionHeader(
		"Timers", "TimersSection",
		InspectorSectionOptions{ .default_open = true, .removable = true, .resettable = true }
	) };
	if (header.remove_requested || header.reset_requested) {
		target.template SetLive<Timers>(
			header.remove_requested ? std::nullopt : ComponentState<Timers>{ Timers{} }
		);
		auto after{ target.template Capture<Timers>() };
		TrackComponentState(
			target, header.remove_requested ? "Remove Timers" : "Reset Timers", std::move(before),
			std::move(after), true
		);
		return true;
	}
	if (!header.open) {
		return false;
	}

	ScopedIndent indent;
	Timers value{ *before };
	std::vector<TimerRename> renames;
	std::string undo_label{ "Edit Timers" };
	std::optional<ImGuiID> undo_key;
	bool changed{ DrawTimersContents(target, value, &renames, &undo_label, &undo_key) };
	if (!changed) {
		return false;
	}

	target.template SetLive<Timers>(value);
	auto after{ target.template Capture<Timers>() };
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

template <typename Component>
[[nodiscard]] auto& VariableEntries(Component& component) {
	if constexpr (std::same_as<std::remove_cvref_t<Component>, ::ptgn::impl::Counters>) {
		return component.counters;
	} else {
		return component.booleans;
	}
}

template <typename Component>
[[nodiscard]] const char* VariableName() {
	if constexpr (std::same_as<Component, ::ptgn::impl::Counters>) {
		return "Counter";
	} else {
		return "Boolean";
	}
}

template <typename Component>
[[nodiscard]] bool IsVariableRuntimeActive(EditorContext& ctx) {
	return ctx.editor.IsPlaying() || ctx.editor.IsDirectRuntime();
}

template <typename Component>
[[nodiscard]] auto MakeUniqueVariableKey(const Component& component) {
	const auto& entries{ VariableEntries(component) };
	for (std::size_t index{ 1 };; ++index) {
		std::string candidate{ std::string{ VariableName<Component>() } + " " +
							   std::to_string(index) };
		if (std::ranges::none_of(entries, [&candidate](const auto& entry) {
				return entry.config.key.value == candidate;
			})) {
			if constexpr (std::same_as<Component, ::ptgn::impl::Counters>) {
				return CounterKey{ std::move(candidate) };
			} else {
				return BooleanKey{ std::move(candidate) };
			}
		}
	}
}

[[nodiscard]] std::int64_t ClampCounterInspectorValue(
	std::int64_t value, const CounterConfig& config
) {
	return std::clamp(
		value, config.minimum.value_or(std::numeric_limits<std::int64_t>::min()),
		config.maximum.value_or(std::numeric_limits<std::int64_t>::max())
	);
}

[[nodiscard]] float CounterDragSpeed(const CounterConfig* config) {
	if (!config || !config->minimum || !config->maximum) {
		return 0.2f;
	}

	long double range{ static_cast<long double>(*config->maximum) -
					   static_cast<long double>(*config->minimum) };
	return std::clamp(
		0.06f + 0.13f * static_cast<float>(std::log10(std::max(0.0L, range) + 1.0L)), 0.08f, 0.7f
	);
}

bool DrawCounterNumberControls(
	const char* id, std::int64_t& value, const CounterConfig* config = nullptr,
	const char* reset_tooltip = nullptr, bool* reset_requested = nullptr, bool show_value = true,
	bool enforce_bounds = true
) {
	ScopedID scope{ std::string_view{ id } };

	const ImGuiStyle& style{ ImGui::GetStyle() };
	float spacing{ style.ItemInnerSpacing.x };
	float button_extent{ ImGui::GetFrameHeight() };
	bool show_reset{ reset_requested != nullptr };
	int button_count{ show_reset ? 3 : 2 };
	float controls_width{ static_cast<float>(button_count) * (button_extent + spacing) };
	float input_width{ std::max(1.0f, ImGui::GetContentRegionAvail().x - controls_width) };

	std::int64_t minimum{ config && enforce_bounds
							  ? config->minimum.value_or(std::numeric_limits<std::int64_t>::min())
							  : std::numeric_limits<std::int64_t>::min() };
	std::int64_t maximum{ config && enforce_bounds
							  ? config->maximum.value_or(std::numeric_limits<std::int64_t>::max())
							  : std::numeric_limits<std::int64_t>::max() };

	ImGui::SetNextItemWidth(input_width);
	bool changed{ false };
	if (show_value) {
		changed = ImGui::DragScalar(
			"##Value", ImGuiDataType_S64, &value, CounterDragSpeed(config),
			config && enforce_bounds && config->minimum ? &minimum : nullptr,
			config && enforce_bounds && config->maximum ? &maximum : nullptr
		);
		if (changed) {
			value = std::clamp(value, minimum, maximum);
		}
	} else {
		char blank[1]{};
		ImGui::InputText("##Value", blank, sizeof(blank), ImGuiInputTextFlags_ReadOnly);
	}

	ImGui::SameLine(0.0f, spacing);
	ImGui::BeginDisabled(!show_value || value <= minimum);
	if (ImGui::Button("-##Decrement", ImVec2{ button_extent, button_extent })) {
		value	= std::max(minimum, value - 1);
		changed = true;
	}
	ImGui::EndDisabled();

	ImGui::SameLine(0.0f, spacing);
	ImGui::BeginDisabled(!show_value || value >= maximum);
	if (ImGui::Button("+##Increment", ImVec2{ button_extent, button_extent })) {
		value	= std::min(maximum, value + 1);
		changed = true;
	}
	ImGui::EndDisabled();

	if (show_reset) {
		ImGui::SameLine(0.0f, spacing);
		if (DrawEditorIconButton("##Reset", EditorIcon::Reset, reset_tooltip)) {
			*reset_requested = true;
		}
	}

	return changed;
}

bool DrawCounterBoundControls(
	const char* id, std::optional<std::int64_t>& bound, std::int64_t default_value,
	const CounterConfig& config
) {
	ScopedID scope{ std::string_view{ id } };
	float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
	bool enabled{ bound.has_value() };
	bool changed{ false };

	if (ImGui::Checkbox("##Enabled", &enabled)) {
		bound	= enabled ? std::optional<std::int64_t>{ default_value } : std::nullopt;
		changed = true;
	}

	std::int64_t value{ bound.value_or(default_value) };
	ImGui::SameLine(0.0f, spacing);
	ImGui::BeginDisabled(!enabled);
	if (DrawCounterNumberControls("##Bound", value, &config, nullptr, nullptr, enabled, false)) {
		bound	= value;
		changed = true;
	}
	ImGui::EndDisabled();

	return changed;
}

template <typename Component>
[[nodiscard]] bool DrawCounterOrBooleanEntry(
	EditorContext& ctx, Entity entity,
	std::conditional_t<std::same_as<Component, ::ptgn::impl::Counters>, CounterEntry, BooleanEntry>&
		entry
) {
	bool changed{ false };
	bool runtime{ entity && IsVariableRuntimeActive<Component>(ctx) };
	if constexpr (std::same_as<Component, ::ptgn::impl::Counters>) {
		if (runtime) {
			CounterHandle handle{ GetCounter(entity, entry.config.key) };
			if (handle) {
				std::int64_t current{ handle.Value() };
				std::int64_t reset_value{
					ClampCounterInspectorValue(entry.config.initial_value, entry.config)
				};
				std::string reset_tooltip{ "Reset to initial value: " +
										   std::to_string(reset_value) };
				bool reset_requested{ false };

				if (DrawPropertyRow("Current Value", [&]() {
						return DrawCounterNumberControls(
							"##CurrentCounter", current, &entry.config, reset_tooltip.c_str(),
							&reset_requested
						);
					})) {
					handle.Set(current);
				}
				if (reset_requested) {
					handle.Reset();
				}
				entry.value = handle.Value();
			}
		} else {
			changed |= DrawPropertyRow("Initial Value", [&]() {
				return DrawCounterNumberControls(
					"##InitialCounter", entry.config.initial_value, &entry.config
				);
			});
		}

		bool minimum_changed{ DrawPropertyRow("Minimum Value", [&]() {
			return DrawCounterBoundControls(
				"##MinimumCounter", entry.config.minimum, 0, entry.config
			);
		}) };
		bool maximum_changed{ DrawPropertyRow("Maximum Value", [&]() {
			return DrawCounterBoundControls(
				"##MaximumCounter", entry.config.maximum, 100, entry.config
			);
		}) };

		if (entry.config.minimum && entry.config.maximum &&
			*entry.config.minimum > *entry.config.maximum) {
			if (maximum_changed) {
				entry.config.minimum = *entry.config.maximum;
			} else {
				entry.config.maximum = *entry.config.minimum;
			}
		}

		changed |= minimum_changed || maximum_changed;

		if (changed) {
			entry.config.initial_value =
				ClampCounterInspectorValue(entry.config.initial_value, entry.config);
			if (!runtime) {
				entry.value = entry.config.initial_value;
			}
		}
	} else {
		if (runtime) {
			BooleanHandle handle{ GetBoolean(entity, entry.config.key) };
			if (handle) {
				bool current{ handle.Value() };
				std::string reset_tooltip{ std::string{ "Reset to initial value: " } +
										   (entry.config.initial_value ? "true" : "false") };
				bool reset_requested{ false };

				if (DrawPropertyRow("Current Value", [&]() {
						bool value_changed{ ImGui::Checkbox("##BooleanCurrent", &current) };
						DrawTooltip(current ? "true" : "false");
						ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
						reset_requested = DrawEditorIconButton(
							"##BooleanReset", EditorIcon::Reset, reset_tooltip.c_str()
						);
						return value_changed;
					})) {
					handle.Set(current);
				}
				if (reset_requested) {
					handle.Reset();
				}
				entry.value = handle.Value();
			}
		} else {
			changed |= DrawPropertyRow("Initial Value", [&]() {
				bool value_changed{
					ImGui::Checkbox("##BooleanInitial", &entry.config.initial_value)
				};
				DrawTooltip(entry.config.initial_value ? "true" : "false");
				return value_changed;
			});
			if (changed) {
				entry.value = entry.config.initial_value;
			}
		}
	}
	return changed;
}

template <typename Key>
struct VariableRename {
	Key old_key{};
	Key new_key{};
};

template <typename Target, typename Component>
bool DrawNamedVariableComponent(Target& target) {
	using Key =
		std::conditional_t<std::same_as<Component, ::ptgn::impl::Counters>, CounterKey, BooleanKey>;

	ScopedID target_scope{ target.Id() };
	ScopedID component_scope{ static_cast<int>(Hash<Component>()) };
	auto before{ target.template Capture<Component>() };
	if (!before) {
		return false;
	}
	const char* plural{ std::same_as<Component, ::ptgn::impl::Counters> ? "Counters" : "Booleans" };
	const char* section_id{ std::same_as<Component, ::ptgn::impl::Counters> ? "CountersSection"
																			: "BooleansSection" };
	auto header{ DrawInspectorSectionHeader(
		plural, section_id,
		InspectorSectionOptions{ .default_open = true, .removable = true, .resettable = true }
	) };
	if (header.remove_requested || header.reset_requested) {
		target.template SetLive<Component>(
			header.remove_requested ? std::nullopt : ComponentState<Component>{ Component{} }
		);
		auto after{ target.template Capture<Component>() };
		TrackComponentState(
			target, header.remove_requested ? "Remove Variables" : "Reset Variables",
			std::move(before), std::move(after), true
		);
		return true;
	}
	if (!header.open) {
		return false;
	}
	ScopedIndent indent;
	Component value{ *before };
	auto& entries{ VariableEntries(value) };
	std::vector<VariableRename<Key>> renames;
	static RenameModalState rename_state{};
	static std::optional<std::size_t> rename_index{};
	std::optional<std::size_t> remove_index;
	bool changed{ false };
	std::string undo_label{ std::string{ "Edit " } + plural };
	std::optional<ImGuiID> undo_key;
	auto set_undo = [&](std::string_view name, const char* id) {
		undo_label = name;
		undo_key   = ImGui::GetID(id);
	};
	bool add_requested{ DrawInspectorTabCollection(
		entries.empty(),
		InspectorTabCollectionOptions{
			.scope_id		 = "##VariableTabStrip",
			.tab_bar_id		 = "##VariableTabs",
			.add_tab_id		 = "+##AddVariable",
			.empty_add_label = "Add Variable",
			.add_tooltip	 = "Add a named variable",
		},
		[&]() {
			for (std::size_t index{ 0 }; index < entries.size(); ++index) {
				ScopedID entry_scope{ static_cast<int>(index) };
				auto& entry{ entries[index] };
				std::string label{ entry.config.key.value.empty()
									   ? std::string{ VariableName<Component>() } + " " +
											 std::to_string(index + 1)
									   : entry.config.key.value };
				bool selected{ BeginInspectorTabItem(label.c_str()) };
				auto menu{
					DrawInspectorTabContextMenu("##VariableTabContext", true, false, true, "Delete")
				};
				if (menu.rename_requested) {
					rename_index = index;
					rename_state.Begin(entry.config.key.value);
				}
				if (menu.remove_requested) {
					remove_index = index;
					set_undo("Remove Variable", "##RemoveVariable");
				}
				if (selected) {
					if (!remove_index.has_value()) {
						Entity entity{};
						if constexpr (requires { target.entity; }) {
							entity = target.entity;
						}
						if (DrawCounterOrBooleanEntry<Component>(target.ctx, entity, entry)) {
							changed = true;
							set_undo("Change Variable", "##ChangeVariable");
						}
					}
					ImGui::EndTabItem();
				}
			}
		}
	) };
	if (add_requested) {
		auto key{ MakeUniqueVariableKey(value) };
		if constexpr (std::same_as<Component, ::ptgn::impl::Counters>) {
			entries.push_back(CounterEntry{ .config = CounterConfig{ .key = key } });
		} else {
			entries.push_back(BooleanEntry{ .config = BooleanConfig{ .key = key } });
		}
		changed = true;
		set_undo("Add Variable", "##AddVariable");
	}
	if (remove_index && *remove_index < entries.size()) {
		entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(*remove_index));
		changed = true;
		if (rename_index) {
			if (*rename_index == *remove_index) {
				rename_state.Cancel();
				rename_index.reset();
			} else if (*rename_index > *remove_index) {
				--*rename_index;
			}
		}
	}
	if (rename_state.active && rename_index) {
		std::size_t index{ *rename_index };
		if (index >= entries.size()) {
			rename_state.Cancel();
			rename_index.reset();
		} else {
			DrawInspectorTabRenameModal(
				rename_state, "Rename Variable", "##RenameVariable",
				[&](std::string_view name) {
					if (name.empty()) {
						return std::string{ "Variable name cannot be empty." };
					}
					for (std::size_t i{ 0 }; i < entries.size(); ++i) {
						if (i != index && entries[i].config.key.value == name) {
							return std::string{ "Names must be unique within this component." };
						}
					}
					return std::string{};
				},
				[&](std::string_view name) {
					Key old_key{ entries[index].config.key };
					Key new_key{ std::string{ name } };
					if (old_key == new_key) {
						return;
					}
					entries[index].config.key = new_key;
					renames.push_back(
						VariableRename<Key>{ .old_key = old_key, .new_key = new_key }
					);
					changed = true;
					set_undo("Rename Variable", "##RenameVariable");
				},
				""
			);
			if (!rename_state.active) {
				rename_index.reset();
			}
		}
	}
	if (!changed) {
		return false;
	}
	// Keep the current runtime values when editing configuration.
	if constexpr (requires { target.entity; }) {
		if (target.entity && IsVariableRuntimeActive<Component>(target.ctx)) {
			for (auto& entry : entries) {
				if constexpr (std::same_as<Component, ::ptgn::impl::Counters>) {
					if (auto handle{ GetCounter(target.entity, entry.config.key) }) {
						entry.value = handle.Value();
					}
				} else {
					if (auto handle{ GetBoolean(target.entity, entry.config.key) }) {
						entry.value = handle.Value();
					}
				}
			}
		}
	}
	target.template SetLive<Component>(value);
	// Applying new bounds can immediately clamp an existing runtime value. Use the
	// handle API so crossing or change triggers are dispatched exactly once.
	if constexpr (std::same_as<Component, ::ptgn::impl::Counters>) {
		if constexpr (requires { target.entity; }) {
			if (target.entity && IsVariableRuntimeActive<Component>(target.ctx)) {
				for (const auto& entry : value.counters) {
					if (auto handle{ GetCounter(target.entity, entry.config.key) }) {
						handle.Set(entry.value);
					}
				}
			}
		}
	}
	auto after{ target.template Capture<Component>() };
	ImGuiID edit_id{ undo_key.value_or(ImGui::GetID("##VariableComponentEdit")) };
	if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
		bool references_changed{ false };
		std::optional<TimerReferenceSceneSnapshot> before_references;
		std::optional<TimerReferenceSceneSnapshot> after_references;
		if (target.entity && !renames.empty()) {
			before_references = CaptureTimerReferenceSceneSnapshot(target.entity.GetScene());
			for (const auto& rename : renames) {
				if constexpr (std::same_as<Component, ::ptgn::impl::Counters>) {
					references_changed |=
						RenameCounterReferences(target.entity, rename.old_key, rename.new_key);
				} else {
					references_changed |=
						RenameBooleanReferences(target.entity, rename.old_key, rename.new_key);
				}
			}
			if (references_changed) {
				after_references = CaptureTimerReferenceSceneSnapshot(target.entity.GetScene());
			}
		}
		if (references_changed) {
			Editor* editor{ std::addressof(target.ctx.editor) };
			EntityReference reference{ MakeEntityReference(target.entity) };
			auto apply{ target.template MakeApply<Component>() };
			TrackUndoableInteraction(
				target.ctx, edit_id, undo_label, true,
				[editor, reference, apply, before = std::move(before),
				 before_references = std::move(*before_references)]() mutable {
					apply(before);
					RestoreTimerReferenceSceneSnapshot(*editor, reference, before_references);
				},
				[editor, reference, apply, after = std::move(after),
				 after_references = std::move(*after_references)]() mutable {
					apply(after);
					RestoreTimerReferenceSceneSnapshot(*editor, reference, after_references);
				}
			);
			return true;
		}
	}
	auto apply{ target.template MakeApply<Component>() };
	TrackUndoableInteraction(
		target.ctx, edit_id, undo_label, true,
		[apply, before = std::move(before)]() mutable { apply(before); },
		[apply, after = std::move(after)]() mutable { apply(after); }
	);
	return true;
}

template <typename Target>
bool DrawUtilitiesSectionImpl(Target& target) {
	if (!HasUtilitiesSection(target)) {
		return false;
	}

	bool changed{ false };
	changed |= DrawTimersComponent(target);
	changed |= DrawNamedVariableComponent<Target, ::ptgn::impl::Counters>(target);
	changed |= DrawNamedVariableComponent<Target, ::ptgn::impl::Booleans>(target);

	changed |=
		DrawComponentSection<Target, Lifetime>(target, "Lifetime", [&target](Lifetime& value) {
			if constexpr (requires { target.entity; }) {
				if (target.entity && IsTimerRuntimeActive(target.ctx)) {
					DrawLifetimeRuntimeControls(target.entity);
					if (!target.entity || !target.entity.template Has<Lifetime>()) {
						return false;
					}
					if (auto* live_lifetime{ target.entity.template TryGet<Lifetime>() }) {
						value = *live_lifetime;
					}
				}
			}

			return DrawValue(target.ctx, "Duration", value.duration);
		});
	return changed;
}

} // namespace

bool DrawUtilitiesSection(EntityInspectorTarget& target) {
	return DrawUtilitiesSectionImpl(target);
}

bool DrawUtilitiesSection(PrefabInspectorTarget& target) {
	return DrawUtilitiesSectionImpl(target);
}

} // namespace ptgn::editor::inspector
