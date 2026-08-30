#include "panels/inspector_features.h"
#include "panels/inspector_scripts.h"

namespace ptgn::editor::inspector {

namespace {

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
bool DrawUtilitiesFeatureImpl(Target& target) {
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


} // namespace

bool DrawUtilitiesFeature(EntityInspectorTarget& target) {
	return DrawUtilitiesFeatureImpl(target);
}

bool DrawUtilitiesFeature(PrefabInspectorTarget& target) {
	return DrawUtilitiesFeatureImpl(target);
}

} // namespace ptgn::editor::inspector
