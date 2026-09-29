#include "panels/inspector_archetype_inspector.h"
#include "panels/inspector_scripts.h"

#include "editor/editor_icons.h"

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

    bool runtime_changed{ false };

    if (DrawEditorIconButton("##TimerStart", EditorIcon::Play, "Start timer")) {
        runtime_changed |= timer.Start();
    }
    ImGui::SameLine();

    if (DrawEditorIconButton("##TimerRestart", EditorIcon::Restart, "Restart timer")) {
        runtime_changed |= timer.Restart();
    }
    ImGui::SameLine();

    const bool can_pause_or_resume{ timer.IsPaused() || timer.IsRunning() };
    ImGui::BeginDisabled(!can_pause_or_resume);
    if (DrawEditorIconButton(
            "##TimerPauseResume", timer.IsPaused() ? EditorIcon::Play : EditorIcon::Pause,
            timer.IsPaused() ? "Resume timer" : "Pause timer"
        )) {
        runtime_changed |= timer.IsPaused() ? timer.Resume() : timer.Pause();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();

    if (DrawEditorIconButton("##TimerStop", EditorIcon::Stop, "Stop timer")) {
        runtime_changed |= timer.Stop();
    }
    ImGui::SameLine();

    if (DrawEditorIconButton("##TimerReset", EditorIcon::Reset, "Reset timer")) {
        runtime_changed |= timer.Reset();
    }

    const char* state{ timer.IsPaused()      ? "Paused"
                       : timer.IsRunning()   ? "Running"
                       : timer.IsCompleted() ? "Complete"
                                             : "Stopped" };
    const std::string elapsed{ FormatTimerRuntimeDuration(timer.Elapsed()) };
    const std::string duration{ FormatTimerRuntimeDuration(timer.Duration()) };

    ImGui::Text("%s  %s / %s", state, elapsed.c_str(), duration.c_str());
    ImGui::ProgressBar(timer.Progress(), ImVec2{ -FLT_MIN, 0.0f });
    ImGui::TextDisabled(
        "Elapsed count: %llu", static_cast<unsigned long long>(timer.ElapsedCount())
    );

    DrawPropertyRow("Adjustment", [&]() {
        const ImGuiStyle& style{ ImGui::GetStyle() };
        const float rewind_width{
            ImGui::CalcTextSize("Rewind").x + style.FramePadding.x * 2.0f
        };
        const float advance_width{
            ImGui::CalcTextSize("Advance").x + style.FramePadding.x * 2.0f
        };
        const float spacing{ style.ItemSpacing.x };
        const float input_width{
            std::max(
                1.0f,
                ImGui::GetContentRegionAvail().x - rewind_width - advance_width - spacing * 2.0f
            )
        };

        bool changed{ DrawDurationTextInput(
            "##TimerRuntimeAdjustment", timer_runtime_adjustment, input_width, false,
            "Positive duration to advance or rewind."
        ) };
        timer_runtime_adjustment =
            millisecondsf{ std::max(0.001f, timer_runtime_adjustment.count()) };

        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Button("Rewind", ImVec2{ rewind_width, 0.0f })) {
            runtime_changed |= timer.Rewind(timer_runtime_adjustment);
        }

        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Button("Advance", ImVec2{ advance_width, 0.0f })) {
            runtime_changed |= timer.Advance(timer_runtime_adjustment);
        }

        return changed;
    });

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

    static RenameModalState rename_state{};
    static std::optional<std::size_t> rename_index{};
    std::optional<std::size_t> remove_index;

    const bool add_requested{ DrawInspectorTabCollection(
        timers.timers.empty(),
        InspectorTabCollectionOptions{
            .scope_id = "##TimerTabStrip",
            .tab_bar_id = "##TimerTabs",
            .add_tab_id = "+##AddTimer",
            .empty_add_label = "Add Timer",
            .add_tooltip = "Add timer",
        },
        [&]() {
            for (std::size_t index{ 0 }; index < timers.timers.size(); ++index) {
                ScopedID timer_scope{ static_cast<int>(index) };

                auto& entry{ timers.timers[index] };
                const TimerKey live_key{ entry.config.key };

                std::string label{ entry.config.key.value.empty()
                                       ? std::string{ "Timer " } + std::to_string(index + 1)
                                       : entry.config.key.value };
                const bool selected{ BeginInspectorTabItem(label.c_str()) };
                const InspectorTabContextResult context_menu{
                    DrawInspectorTabContextMenu(
                        "##TimerTabContext", true, false, true, "Delete"
                    )
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
                                target.ctx, "Start Automatically",
                                entry.config.start_automatically
                            )) {
                            changed = true;
                            set_undo("Change Timer Auto Start", "##TimerAutoStartEdit");
                        }
                        DrawTooltip(
                            "Unchecked: start this timer manually or with a Timer Action."
                        );
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
        const std::size_t index{ *remove_index };
        if (index < timers.timers.size()) {
            timers.timers.erase(
                timers.timers.begin() + static_cast<std::ptrdiff_t>(index)
            );
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
        const std::size_t index{ *rename_index };
        if (index >= timers.timers.size()) {
            rename_state.Cancel();
            rename_index.reset();
        } else {
            (void)DrawInspectorTabRenameModal(
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
                    const TimerKey old_key{ entry.config.key };
                    const TimerKey new_key{ std::string{ name } };

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
            const float remove_width{ ImGui::GetFrameHeight() };
            const float spacing{ ImGui::GetStyle().ItemSpacing.x };
            const float available{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
            const bool remove_inline{ available >= remove_width + spacing + 80.0f };
            const float field_width{
                remove_inline ? std::max(1.0f, available - remove_width - spacing) : available
            };

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

            if (remove_inline) {
                ImGui::SameLine(0.0f, spacing);
            }

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
    if (!before) {
        return false;
    }

    const auto header{ DrawInspectorSectionHeader(
        "Timers", "TimersSection",
        InspectorSectionOptions{ .default_open = true, .removable = true, .resettable = true }
    ) };
    if (header.remove_requested || header.reset_requested) {
        target.template SetLive<Timers>(
            header.remove_requested ? std::nullopt : ComponentState<Timers>{ Timers{} }
        );
        auto after{ target.template Capture<Timers>() };
        TrackComponentState(
            target, header.remove_requested ? "Remove Timers" : "Reset Timers",
            std::move(before), std::move(after), true
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
    const bool changed{ DrawTimersContents(target, value, &renames, &undo_label, &undo_key) };
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
                references_changed |= RenameTimerReferences(target.entity, rename.old_key, rename.new_key);
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
bool DrawUtilitiesSectionImpl(Target& target) {
    if (!HasUtilitiesSection(target)) {
        return false;
    }

    bool changed{ false };
    changed |= DrawTimersComponent(target);
    changed |= DrawComponentSection<Target, Group>(
        target, "Groups", [](Group& value) { return DrawGroupContents(value); }
    );
    changed |= DrawComponentSection<Target, Lifetime>(
        target, "Lifetime", [&target](Lifetime& value) {
            return DrawRegisteredComponentContents(
                target.ctx, Hash<Lifetime>(), std::addressof(value)
            );
        }
    );
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
