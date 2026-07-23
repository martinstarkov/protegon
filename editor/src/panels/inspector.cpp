#include "panels/inspector.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>
#ifndef MAGIC_ENUM_RANGE_MAX
#define MAGIC_ENUM_RANGE_MAX 512
#endif
#include <magic_enum/magic_enum.hpp>
#include <string>
#include <string_view>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "runtime/graphics/render_target.h"
#include "core/math/transform.h"
#include "panels/component_editor_registry.h"
#include "panels/inspector_fields.h"
#include "scripting/script_editor_registry.h"
#include "panels/scene_hierarchy.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "runtime/scripting/script.h"
#include "runtime/animation/offsets.h"
#include "runtime/ecs/component_registration.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/bloom.h"
#include "runtime/graphics/fx/blur.h"
#include "runtime/graphics/fx/gaussian_blur.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/graphics.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/sprite.h"
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
#include "runtime/scene/scene_camera.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "core/util/hash.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scripting/script.h"

namespace ptgn::editor::inspector {

namespace {

void DrawTooltip(const char* text) {
	if (text && *text && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
		ImGui::SetTooltip("%s", text);
	}
}

const char* CompletionLabel(ScriptCompletion completion) {
	switch (completion) {
		case ScriptCompletion::Instant: return "Action";
		case ScriptCompletion::Duration: return "Tween";
		case ScriptCompletion::ScriptControlled: return "Until Complete";
		case ScriptCompletion::Infinite: return "Forever";
	}
	return "Unknown";
}

const char* ReentryLabel(ReentryMode mode) {
	switch (mode) {
		case ReentryMode::IgnoreWhileRunning: return "Ignore";
		case ReentryMode::Restart: return "Restart";
		case ReentryMode::Queue: return "Queue";
	}
	return "Unknown";
}

const char* LifecycleLabel(SequenceLifecycle lifecycle) {
	switch (lifecycle) {
		case SequenceLifecycle::Start: return "On Start";
		case SequenceLifecycle::Complete: return "On Complete";
		case SequenceLifecycle::Reset: return "On Reset";
		case SequenceLifecycle::Stop: return "On Stop";
		case SequenceLifecycle::Pause: return "On Pause";
		case SequenceLifecycle::Resume: return "On Resume";
		case SequenceLifecycle::ScriptStart: return "On Script Start";
		case SequenceLifecycle::ScriptComplete: return "On Script Complete";
		case SequenceLifecycle::ScriptCancel: return "On Script Cancel";
		case SequenceLifecycle::Repeat: return "On Repeat";
		case SequenceLifecycle::Yoyo: return "On Yoyo";
	}
	return "Unknown";
}

bool DrawTiming(ScriptStep& step, const ScriptRegistration& registration) {
	if (!registration.supports_timing && !registration.requires_timing) {
		return false;
	}

	bool changed{ false };
	bool timed{ step.timing.has_value() };
	if (registration.requires_timing) {
		timed = true;
	}
	if (!registration.requires_timing) {
		changed |= ImGui::Checkbox("Timed", &timed);
		ImGui::SameLine();
	}
	if (timed && !step.timing) {
		step.timing = registration.default_timing.value_or(ScriptTiming{});
		changed = true;
	} else if (!timed && step.timing) {
		step.timing.reset();
		changed = true;
	}
	if (!step.timing) {
		return changed;
	}

	auto& timing{ *step.timing };
	ImGui::SetNextItemWidth(100.0f);
	changed |= ImGui::DragFloat(
		"Duration", &timing.duration_ms, 10.0f, 0.0f, 3600000.0f, "%.0f ms"
	);

	ImGui::SameLine();
	ImGui::SetNextItemWidth(130.0f);
	if (ImGui::BeginCombo("Ease", std::string{ magic_enum::enum_name(timing.ease) }.c_str())) {
		for (const auto ease : magic_enum::enum_values<Ease>()) {
			const bool selected{ ease == timing.ease };
			if (ImGui::Selectable(std::string{ magic_enum::enum_name(ease) }.c_str(), selected)) {
				timing.ease = ease;
				changed = true;
			}
		}
		ImGui::EndCombo();
	}

	changed |= ImGui::InputInt("Additional Repeats", &timing.additional_repeats);
	timing.additional_repeats = std::max(0, timing.additional_repeats);
	changed |= ImGui::Checkbox("Infinite Repeats", &timing.infinite_repeats);
	ImGui::SameLine();
	changed |= ImGui::Checkbox("Reversed", &timing.reversed);
	ImGui::SameLine();
	changed |= ImGui::Checkbox("Yoyo", &timing.yoyo);
	return changed;
}

bool DrawStepEditor(Entity owner, ScriptStep& step, const char* id_prefix) {
	bool changed{ false };
	const auto* registration{ ScriptRegistry::Find(step.type_hash) };
	const auto* editor{ SequenceStepEditorRegistry::Find(step.type_hash) };
	const char* label{ editor ? editor->options.label.c_str() : "Missing Script" };

	ImGui::PushID(&step);
	bool open{ ImGui::TreeNodeEx(
		id_prefix,
		ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth,
		"%s", label
	) };
	if (editor) {
		DrawTooltip(editor->options.description.c_str());
	}
	ImGui::SameLine();
	changed |= ImGui::Checkbox("Enabled", &step.enabled);

	if (open) {
		if (registration) {
			ScriptCompletion completion{ step.completion.value_or(registration->completion) };
			ImGui::SetNextItemWidth(150.0f);
			if (ImGui::BeginCombo("Completion", CompletionLabel(completion))) {
				for (const auto candidate : {
					ScriptCompletion::Instant,
					ScriptCompletion::Duration,
					ScriptCompletion::ScriptControlled,
					ScriptCompletion::Infinite,
				}) {
					if (ImGui::Selectable(CompletionLabel(candidate), candidate == completion)) {
						completion = candidate;
						step.completion = candidate;
						if (candidate == ScriptCompletion::Duration && !step.timing) {
							step.timing = registration->default_timing.value_or(ScriptTiming{});
						} else if (candidate == ScriptCompletion::Instant &&
							!registration->requires_timing) {
							step.timing.reset();
						}
						changed = true;
					}
				}
				ImGui::EndCombo();
			}
			changed |= DrawTiming(step, *registration);
		}

		if (editor && editor->draw) {
			ScriptEditorContext context{ owner, owner.GetScene().ctx().shared_script_sequences };
			changed |= editor->draw(step.value, context);
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
	return changed;
}

bool DrawEventCondition([[maybe_unused]] Entity owner, EventCondition& condition, const char* id_prefix) {
	bool changed{ false };
	const auto* editor{ EventEditorRegistry::Find(condition.type_hash) };
	const char* label{ editor ? editor->options.label.c_str() : "Missing Event" };

	ImGui::PushID(&condition);
	bool open{ ImGui::TreeNodeEx(
		id_prefix,
		ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth,
		"%s", label
	) };
	if (editor) {
		DrawTooltip(editor->options.description.c_str());
	}
	ImGui::SameLine();
	changed |= ImGui::Checkbox("Enabled", &condition.enabled);
	ImGui::SameLine();
	changed |= ImGui::Checkbox("Consume", &condition.consume);
	if (open) {
		if (editor && editor->draw) {
			changed |= editor->draw(condition.value);
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
	return changed;
}

void DrawAddEventPopup(Entity owner, std::vector<EventCondition>& conditions, const char* popup_id) {
	if (!ImGui::BeginPopup(popup_id)) {
		return;
	}
	std::string group;
	for (const auto& entry : EventEditorRegistry::Entries()) {
		const auto* runtime{ SequenceEventRegistry::Find(entry.type_hash) };
		if (!runtime || (runtime->available && !runtime->available(owner))) {
			continue;
		}
		if (entry.options.group != group) {
			if (!group.empty()) {
				ImGui::Separator();
			}
			group = entry.options.group;
			if (!group.empty()) {
				ImGui::TextDisabled("%s", group.c_str());
			}
		}
		if (ImGui::MenuItem(entry.options.label.c_str())) {
			conditions.push_back(SequenceEventRegistry::MakeCondition(entry.type_hash));
		}
	}
	ImGui::EndPopup();
}

bool DrawEventList(Entity owner, const char* label, std::vector<EventCondition>& conditions) {
	bool changed{ false };
	if (!ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen)) {
		return false;
	}

	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(conditions.size()); ++i) {
		ImGui::PushID(i);
		changed |= DrawEventCondition(owner, conditions[static_cast<std::size_t>(i)], "##Event");
		ImGui::SameLine();
		if (ImGui::SmallButton("x")) {
			remove = i;
		}
		ImGui::PopID();
	}
	if (remove >= 0) {
		conditions.erase(conditions.begin() + remove);
		changed = true;
	}

	const std::string popup{ std::string{ "Add" } + label };
	if (ImGui::Button((std::string{ "+ " } + label).c_str(), ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup(popup.c_str());
	}
	DrawAddEventPopup(owner, conditions, popup.c_str());
	ImGui::TreePop();
	return changed;
}

void DrawAddStepPopup(std::vector<ScriptStep>& steps, const char* popup_id, bool instant_only) {
	if (!ImGui::BeginPopup(popup_id)) {
		return;
	}

	std::vector<const SequenceStepEditorRegistration*> entries;
	for (const auto& entry : SequenceStepEditorRegistry::Entries()) {
		const auto* registration{ ScriptRegistry::Find(entry.type_hash) };
		if (!registration || !registration->serializable) {
			continue;
		}
		if (instant_only && registration->completion != ScriptCompletion::Instant) {
			continue;
		}
		entries.push_back(&entry);
	}
	std::ranges::sort(entries, [](const auto* a, const auto* b) {
		if (a->options.group != b->options.group) {
			return a->options.group < b->options.group;
		}
		if (a->options.menu_order != b->options.menu_order) {
			return a->options.menu_order < b->options.menu_order;
		}
		return a->options.label < b->options.label;
	});

	std::string group;
	for (const auto* entry : entries) {
		if (entry->options.group != group) {
			if (!group.empty()) {
				ImGui::Separator();
			}
			group = entry->options.group;
			if (!group.empty()) {
				ImGui::TextDisabled("%s", group.c_str());
			}
		}
		if (ImGui::MenuItem(entry->options.label.c_str())) {
			steps.push_back(ScriptRegistry::MakeStep(entry->type_hash));
		}
		if (entry->options.separator_after) {
			ImGui::Separator();
		}
	}
	ImGui::EndPopup();
}

bool DrawSteps(Entity owner, ScriptSequence& sequence) {
	bool changed{ false };
	if (!ImGui::TreeNodeEx("Scripts", ImGuiTreeNodeFlags_DefaultOpen)) {
		return false;
	}

	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(sequence.steps.size()); ++i) {
		ImGui::PushID(i);
		changed |= DrawStepEditor(owner, sequence.steps[static_cast<std::size_t>(i)], "##Step");
		ImGui::SameLine();
		if (ImGui::SmallButton("x")) {
			remove = i;
		}
		ImGui::PopID();
	}
	if (remove >= 0) {
		sequence.steps.erase(sequence.steps.begin() + remove);
		changed = true;
	}

	if (ImGui::Button("+ Script", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddSequenceScript");
	}
	DrawAddStepPopup(sequence.steps, "AddSequenceScript", false);
	ImGui::TreePop();
	return changed;
}

bool DrawLifecycleActions(Entity owner, ScriptSequence& sequence) {
	bool changed{ false };
	if (!ImGui::TreeNode("Lifecycle")) {
		return false;
	}

	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(sequence.lifecycle_actions.size()); ++i) {
		auto& lifecycle{ sequence.lifecycle_actions[static_cast<std::size_t>(i)] };
		ImGui::PushID(i);
		ImGui::SetNextItemWidth(150.0f);
		if (ImGui::BeginCombo("##Lifecycle", LifecycleLabel(lifecycle.lifecycle))) {
			for (const auto candidate : magic_enum::enum_values<SequenceLifecycle>()) {
				if (ImGui::Selectable(
						LifecycleLabel(candidate), candidate == lifecycle.lifecycle
					)) {
					lifecycle.lifecycle = candidate;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		changed |= DrawStepEditor(owner, lifecycle.action, "##LifecycleScript");
		ImGui::SameLine();
		if (ImGui::SmallButton("x")) {
			remove = i;
		}
		ImGui::PopID();
	}
	if (remove >= 0) {
		sequence.lifecycle_actions.erase(sequence.lifecycle_actions.begin() + remove);
		changed = true;
	}

	if (ImGui::Button("+ Lifecycle Script", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddLifecycleScript");
	}
	if (ImGui::BeginPopup("AddLifecycleScript")) {
		for (const auto& entry : SequenceStepEditorRegistry::Entries()) {
			const auto* registration{ ScriptRegistry::Find(entry.type_hash) };
			if (!registration || !registration->serializable ||
				registration->completion != ScriptCompletion::Instant) {
				continue;
			}
			if (ImGui::MenuItem(entry.options.label.c_str())) {
				sequence.lifecycle_actions.push_back(LifecycleScript{
					.enabled = true,
					.lifecycle = SequenceLifecycle::Complete,
					.action = ScriptRegistry::MakeStep(entry.type_hash),
				});
			}
		}
		ImGui::EndPopup();
	}
	ImGui::TreePop();
	return changed;
}

bool DrawSequence(Entity owner, ScriptSequence& binding) {
	ScriptSequence* sequence{ script_runtime::Resolve(owner, binding) };
	if (!sequence) {
		ImGui::TextDisabled("Missing shared Script Sequence");
		return false;
	}

	bool changed{ false };
	ImGui::PushID(static_cast<int>(binding.id));
	bool open{ ImGui::TreeNodeEx(
		"##Sequence",
		ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth,
		"%s", sequence->name.c_str()
	) };
	ImGui::SameLine();
	changed |= ImGui::Checkbox("Enabled", &binding.enabled);
	ImGui::SameLine();
	if (ImGui::SmallButton(binding.runtime.running ? "Restart" : "Play")) {
		script_runtime::Start(owner, binding.id, binding.runtime.running);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton(binding.runtime.paused ? "Resume" : "Pause")) {
		script_runtime::SetPaused(owner, binding.id, !binding.runtime.paused);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Stop")) {
		script_runtime::Stop(owner, binding.id);
	}

	if (open) {
		auto& shared_registry{ owner.GetScene().ctx().shared_script_sequences };
		if (binding.shared_reference) {
			const char* preview{ sequence ? sequence->name.c_str() : "Missing Shared Sequence" };
			ImGui::SetNextItemWidth(220.0f);
			if (ImGui::BeginCombo("Shared Sequence", preview)) {
				for (const auto& shared : shared_registry.sequences) {
					const bool selected{ shared.id == binding.shared_sequence_id };
					if (ImGui::Selectable(shared.name.c_str(), selected)) {
						binding.shared_sequence_id = shared.id;
						binding.runtime = ScriptSequenceRuntime{};
						changed = true;
					}
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			if (ImGui::Button("Make Local")) {
				if (const auto* shared{ shared_registry.Find(binding.shared_sequence_id) }) {
					ScriptSequence local{ *shared };
					const SequenceId binding_id{ binding.id };
					const bool enabled{ binding.enabled };
					binding = std::move(local);
					binding.id = binding_id;
					binding.enabled = enabled;
					binding.shared_reference = false;
					binding.shared_sequence_id = 0;
					binding.runtime = ScriptSequenceRuntime{};
					changed = true;
				}
			}
		} else if (ImGui::Button("Make Shared")) {
			ScriptSequence shared{ binding };
			shared.shared_reference = false;
			shared.shared_sequence_id = 0;
			shared.runtime = ScriptSequenceRuntime{};
			const SequenceId shared_id{ shared.id };
			shared_registry.sequences.push_back(std::move(shared));
			binding.shared_reference = true;
			binding.shared_sequence_id = shared_id;
			binding.runtime = ScriptSequenceRuntime{};
			changed = true;
		}

		sequence = script_runtime::Resolve(owner, binding);
		if (!sequence) {
			ImGui::TextDisabled("Missing shared Script Sequence");
			ImGui::TreePop();
			ImGui::PopID();
			return changed;
		}
		changed |= ImGui::InputText("Name", &sequence->name);

		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::BeginCombo("Reentry", ReentryLabel(sequence->reentry))) {
			for (const auto mode : {
				ReentryMode::IgnoreWhileRunning, ReentryMode::Restart, ReentryMode::Queue
			}) {
				if (ImGui::Selectable(ReentryLabel(mode), mode == sequence->reentry)) {
					sequence->reentry = mode;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}

		bool has_channel{ sequence->channel.has_value() };
		changed |= ImGui::Checkbox("Channel", &has_channel);
		if (has_channel && !sequence->channel) {
			sequence->channel.emplace("Default");
			changed = true;
		} else if (!has_channel && sequence->channel) {
			sequence->channel.reset();
			changed = true;
		}
		if (sequence->channel) {
			changed |= ImGui::InputText("Channel Name", &sequence->channel->value);
		}

		changed |= ImGui::Checkbox("Remove Binding On Complete", &sequence->remove_binding_on_complete);
		changed |= ImGui::Checkbox("Destroy Owner On Complete", &sequence->destroy_owner_on_complete);

		changed |= DrawEventList(owner, "Start Triggers", sequence->start_events);
		changed |= DrawEventList(owner, "Stop Triggers", sequence->stop_events);
		changed |= DrawSteps(owner, *sequence);
		changed |= DrawLifecycleActions(owner, *sequence);

		if (binding.runtime.running || binding.runtime.completed) {
			ImGui::ProgressBar(script_runtime::Progress(owner, binding.id), ImVec2{ -FLT_MIN, 0.0f });
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
	return changed;
}

ScriptEntry MakeRootEntry(TypeHashValue type_hash) {
	const auto* registration{ ScriptRegistry::Find(type_hash) };
	if (!registration) {
		return {};
	}
	ScriptEntry entry;
	entry.enabled = true;
	entry.type_hash = type_hash;
	entry.value = registration->make_default ? registration->make_default() : json::object();
	entry.sequence = registration->make_default_sequence
		? registration->make_default_sequence()
		: ScriptSequence{};
	return entry;
}

void DrawAddRootScriptPopup(::ptgn::impl::Scripts& scripts) {
	if (!ImGui::BeginPopup("AddRootScript")) {
		return;
	}
	std::string group;
	for (const auto& entry : ScriptEditorRegistry::Entries()) {
		if (entry.options.hidden) {
			continue;
		}
		if (entry.options.group != group) {
			if (!group.empty()) {
				ImGui::Separator();
			}
			group = entry.options.group;
			if (!group.empty()) {
				ImGui::TextDisabled("%s", group.c_str());
			}
		}
		if (ImGui::MenuItem(entry.options.label.c_str())) {
			scripts.AddEntryDeferred(MakeRootEntry(entry.type_hash));
		}
	}
	ImGui::EndPopup();
}

bool DrawScriptsComponent(Entity entity) {
	if (!entity) {
		return false;
	}

	auto* scripts{ entity.TryGet<::ptgn::impl::Scripts>() };
	if (!scripts) {
		return false;
	}
	scripts->Attach(entity);

	bool changed{ false };
	if (ImGui::Button("+ Script", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddRootScript");
	}
	DrawAddRootScriptPopup(*scripts);

	int remove{ -1 };
	for (int i{ 0 }; i < static_cast<int>(scripts->scripts.size()); ++i) {
		auto& entry{ scripts->scripts[static_cast<std::size_t>(i)] };
		script_runtime::AttachEntry(entity, entry);
		ImGui::PushID(&entry);

		if (entry.type_hash == Hash<Script>()) {
			if (entry.instance) {
				entry.instance->sequence.enabled = entry.enabled;
				changed |= DrawSequence(entity, entry.instance->sequence);
				entry.enabled = entry.instance->sequence.enabled;
				const SequenceId id{ entry.instance->sequence.id };
				entry.sequence = entry.instance->sequence;
				entry.sequence.id = id;
				entry.sequence.runtime = ScriptSequenceRuntime{};
			}
		} else {
			const auto* editor{ ScriptEditorRegistry::Find(entry.type_hash) };
			const auto* registration{ ScriptRegistry::Find(entry.type_hash) };
			const char* label{ editor ? editor->options.label.c_str() : "Missing Script" };
			bool open{ ImGui::TreeNodeEx(
				"##RootScript",
				ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
					ImGuiTreeNodeFlags_SpanAvailWidth,
				"%s", label
			) };
			if (editor) {
				DrawTooltip(editor->options.description.c_str());
			}
			ImGui::SameLine();
			changed |= ImGui::Checkbox("Enabled", &entry.enabled);
			ImGui::SameLine();
			if (ImGui::SmallButton("x")) {
				remove = i;
			}
			if (open) {
				if (editor && editor->draw) {
					ScriptEditorContext context{
						entity, entity.GetScene().ctx().shared_script_sequences
					};
					if (editor->draw(entry.value, context)) {
						changed = true;
						if (entry.instance && registration && registration->apply) {
							registration->apply(*entry.instance, entry.value);
						}
					}
				}
				ImGui::TreePop();
			}
		}
		ImGui::PopID();
	}

	if (remove >= 0) {
		auto& entry{ scripts->scripts[static_cast<std::size_t>(remove)] };
		const SequenceId id{ entry.instance ? entry.instance->sequence.id : entry.sequence.id };
		scripts->RemoveDeferred(id);
		changed = true;
	}
	return changed;
}


template <std::size_t N>
struct FixedString {
	char value[N];

	constexpr FixedString(const char (&text)[N]) {
		std::copy_n(text, N, value);
	}

	[[nodiscard]] constexpr std::string_view View() const {
		return { value, N - 1 };
	}
};

template <FixedString Reason, typename... T>
std::optional<std::string_view> HasAnyComponent(Entity entity) {
	if (entity.HasAny<T...>()) {
		return Reason.View();
	}

	return std::nullopt;
}

bool DrawOptionalViewport(
	std::string_view label, std::optional<Viewport>& value, ViewportSpace viewport_space
) {
	ImGui::PushID(&value);

	bool enabled{ value.has_value() };
	bool changed{ DrawPropertyRow(label, [&]() {
		return DrawDisabledIf(IsReadOnly(), [&]() {
			return ImGui::Checkbox("##enabled", &enabled);
		});
	}) };

	if (enabled != value.has_value()) {
		if (enabled) {
			auto& viewport{ value.emplace() };

			if (viewport_space == ViewportSpace::Normalized) {
				viewport.position = V2_float{ 0.0f, 0.0f };
				viewport.size	  = V2_float{ 1.0f, 1.0f };
			}
		} else {
			value.reset();
		}

		changed = true;
	}

	if (value.has_value()) {
		ImGui::Indent();

		if (viewport_space == ViewportSpace::Normalized) {
			FieldOptions options{
				.speed	= 0.01f,
				.min	= 0.0,
				.max	= 1.0,
				.format = "%.3f",
				.flags	= ImGuiSliderFlags_AlwaysClamp,
			};

			changed |= DrawValue("Position", value->position, options);
			changed |= DrawValue("Size", value->size, options);

			if (value->size.x > 1.0f || value->size.y > 1.0f) {
				value->size.x = std::min(1.0f, value->size.x);
				value->size.y = std::min(1.0f, value->size.y);
				changed		  = true;
			}
		} else {
			changed |= DrawValue(
				"Position", value->position,
				FieldOptions{
					.speed	= 1.0f,
					.format = "%.0f",
				}
			);

			changed |= DrawValue(
				"Size", value->size,
				FieldOptions{
					.speed	= 1.0f,
					.min	= 1.0f,
					.max	= 4096.0f,
					.format = "%.0f",
				}
			);

			if (value->size.x < 1.0f || value->size.y < 1.0f) {
				value->size.x = std::max(1.0f, value->size.x);
				value->size.y = std::max(1.0f, value->size.y);
				changed		  = true;
			}
		}

		ImGui::Unindent();
	}

	ImGui::PopID();

	return changed;
}

} // namespace

// TODO: Add Material.

template <>
struct Contents<::ptgn::impl::CameraData> {
	static bool Draw(::ptgn::impl::CameraData& camera) {
		bool changed{ false };

		// Draw this first because it controls the raw viewport's defaults and bounds.
		changed |= DrawValue("Viewport Space", camera.viewport_space);

		changed |= DrawOptionalViewport("Raw Viewport", camera.raw_viewport, camera.viewport_space);

		changed |= DrawValue("Pixel Rounding", camera.pixel_rounding);
		changed |= DrawValue("Bounding Box", camera.bounding_box);

		DrawReadOnlyValue("View Projection", camera.view_projection);

		return changed;
	}
};

template <>
struct Contents<::ptgn::impl::IDrawable> {
	static bool Draw(::ptgn::impl::IDrawable& drawable) {
		auto* current_info{ ::ptgn::impl::IDrawable::FindInfo(drawable.hash) };

		std::string preview{ current_info ? std::string{ current_info->GetDisplayName() }
										  : "<Unregistered Drawable>" };

		bool changed{ DrawPropertyRow("Drawable", [&]() {
			bool local_changed{ false };

			if (ImGui::BeginCombo("##value", preview.c_str())) {
				for (const auto& info : ::ptgn::impl::IDrawable::data()) {
					bool selected{ drawable.hash == info.hash };
					std::string display_name{ info.GetDisplayName() };

					if (ImGui::Selectable(display_name.c_str(), selected)) {
						drawable.hash = info.hash;
						local_changed = true;
					}

					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			return local_changed;
		}) };

		if (!current_info && drawable.hash != 0) {
			ImGui::TextDisabled("Stored hash: %zu", drawable.hash);
		}

		return changed;
	}
};

// Only types whose default reflected layout is not ideal need a specialization.
template <>
struct Contents<Hollow> {
	static bool Draw(Hollow& hollow) {
		return DrawValue(
			"Line Width", hollow.line_width,
			FieldOptions{
				.speed	= 0.1f,
				.min	= kMinLineWidth,
				.max	= 1000.0f,
				.format = "%.2f",
				.flags	= ImGuiSliderFlags_AlwaysClamp,
			}
		);
	}
};

template <>
struct Contents<Rect> {
	static bool Draw(Rect& rect) {
		bool changed{ false };

		auto size{ rect.max - rect.min };

		if (DrawValue(
				"Size", size,
				FieldOptions{
					.speed	= 0.1f,
					.min	= 0.0,
					.max	= 0.0,
					.format = "%.3f",
				}
			)) {
			size.x = std::max(size.x, 0.0f);
			size.y = std::max(size.y, 0.0f);

			auto center{ rect.GetCenter() };
			auto half_size{ size * 0.5f };

			rect.min = center - half_size;
			rect.max = center + half_size;

			changed = true;
		}

		changed |= DrawValue("Min", rect.min);
		changed |= DrawValue("Max", rect.max);

		return changed;
	}
};

template <>
struct Contents<TextRun> {
	static bool Draw(TextRun& run) {
		bool changed{ false };
		changed |= DrawValue("Text", run.text, FieldOptions{ .multiline = true });
		changed |= DrawValue("Font", run.font);
		changed |= DrawValue("Style", run.style);
		return changed;
	}
};

template <>
struct Contents<StyledText> {
	static bool Draw(StyledText& text) {
		return DrawVectorEditor(
			text.runs, VectorOptions{
						   .item_name	 = "Text Run",
						   .default_open = true,
						   .reorderable	 = true,
					   }
		);
	}
};

template <>
struct Contents<Polygon> {
	static bool Draw(Polygon& polygon) {
		return DrawVectorEditor(
			polygon.vertices, VectorOptions{
								  .item_name	= "Vertex",
								  .default_open = true,
								  .reorderable	= true,
							  }
		);
	}
};

template <>
struct Contents<ButtonBorderVisuals> {
	static bool Draw(ButtonBorderVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(visuals.states);
	}
};

template <>
struct Contents<ButtonBackgroundVisuals> {
	static bool Draw(ButtonBackgroundVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(visuals.states);
	}
};

template <>
struct Contents<ButtonTextVisuals> {
	static bool Draw(ButtonTextVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(visuals.states);
	}
};

template <>
struct Contents<ButtonSpriteVisuals> {
	static bool Draw(ButtonSpriteVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(visuals.states);
	}
};

template <>
struct Contents<ButtonSounds> {
	static bool Draw(ButtonSounds& sounds) {
		bool changed{ false };
		changed |= DrawEnumArrayEditor<ButtonVisualState>("Sounds", sounds.states);
		changed |= DrawValue("Exclusive Audio", sounds.exclusive);
		return changed;
	}
};

} // namespace ptgn::editor::inspector

namespace ptgn::editor {

namespace {

using namespace inspector;

template <typename T>
bool DrawRegisteredContents(Entity entity) {
	return DrawComponentContents(entity.Get<T>());
}

void MarkTextLayoutDirty(Entity entity) {
	if (entity.Has<TextLayout>()) {
		entity.Get<TextLayout>().dirty = true;
	}
}

void MarkParentButtonDirty(Entity entity, ::ptgn::impl::ButtonDirty dirty) {
	Entity parent{ GetParent(entity) };

	if (!parent || !parent.Has<::ptgn::impl::ButtonData>()) {
		return;
	}

	parent.Get<::ptgn::impl::ButtonData>().dirty |= dirty;
}

void MarkButtonTextDirty(Entity entity) {
	MarkTextLayoutDirty(entity);
	MarkParentButtonDirty(entity, ::ptgn::impl::ButtonDirty::Text);
}

void MarkButtonBorderDirty(Entity entity) {
	MarkParentButtonDirty(entity, ::ptgn::impl::ButtonDirty::Border);
}

void MarkButtonBackgroundDirty(Entity entity) {
	MarkParentButtonDirty(entity, ::ptgn::impl::ButtonDirty::Background);
}

void MarkButtonSpriteDirty(Entity entity) {
	MarkParentButtonDirty(entity, ::ptgn::impl::ButtonDirty::Sprite);
}

void DrawAddDrawableMenuItem(Entity entity, const auto& info) {
	auto name{ std::string{ info.GetDisplayName() } };

	if (ImGui::MenuItem(name.c_str())) {
		entity.Add<::ptgn::impl::IDrawable>(info.hash);
	}
}

void DrawAddDrawableMenu(Entity entity, std::string_view label) {
	auto menu_label{ std::string{ label } };

	if (!ImGui::BeginMenu(menu_label.c_str())) {
		return;
	}

	const auto& drawables{ ::ptgn::impl::IDrawable::data() };

	auto draw_menu_item = [entity](const ::ptgn::impl::IDrawable::Info& info) mutable {
		auto display_name{ std::string{ info.GetDisplayName() } };

		if (ImGui::MenuItem(display_name.c_str())) {
			entity.Add<::ptgn::impl::IDrawable>(info.hash);
		}
	};

	std::vector<std::string_view> groups;

	for (const auto& info : drawables) {
		if (!info.group.has_value() || info.group->empty()) {
			continue;
		}

		if (std::ranges::find(groups, info.group.value()) == groups.end()) {
			groups.push_back(info.group.value());
		}
	}

	std::ranges::sort(groups);

	for (auto group : groups) {
		auto group_label{ std::string{ group } };

		if (!ImGui::BeginMenu(group_label.c_str())) {
			continue;
		}

		for (const auto& info : drawables) {
			if (info.group.has_value() && info.group.value() == group) {
				draw_menu_item(info);
			}
		}

		ImGui::EndMenu();
	}

	bool has_grouped_drawables{ !groups.empty() };
	bool has_ungrouped_drawables{ std::ranges::any_of(drawables, [](const auto& info) {
		return !info.group.has_value() || info.group->empty();
	}) };

	if (has_grouped_drawables && has_ungrouped_drawables) {
		ImGui::Separator();
	}

	for (const auto& info : drawables) {
		if (!info.group.has_value() || info.group->empty()) {
			draw_menu_item(info);
		}
	}

	ImGui::EndMenu();
}

bool DrawScaleValue(Entity entity, V2_float& scale, const FieldOptions& options) {
	ImGui::PushID(entity.Get<UUID>());
	ImGui::PushID("Scale");

	ImGuiStorage* storage{ ImGui::GetStateStorage() };
	ImGuiID scale_lock_id{ ImGui::GetID("ScaleLocked") };
	bool scale_locked{ storage->GetBool(scale_lock_id, true) };

	bool changed{ DrawPropertyRow("Scale", [&]() {
		if (ImGui::Checkbox("##ScaleLocked", &scale_locked)) {
			storage->SetBool(scale_lock_id, scale_locked);
		}

		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(scale_locked ? "Unlock ratio" : "Lock ratio");
		}

		ImGui::SameLine();

		constexpr float kFieldSpacing{ 4.0f };

		float available_width{ ImGui::GetContentRegionAvail().x };
		float field_width{ (available_width - kFieldSpacing) * 0.5f };

		V2_float previous_scale{ scale };

		ImGui::SetNextItemWidth(field_width);
		bool x_changed{ ImGui::DragFloat(
			"##X", &scale.x, options.speed, static_cast<float>(options.min),
			static_cast<float>(options.max), options.format, options.flags
		) };

		ImGui::SameLine(0.0f, kFieldSpacing);

		ImGui::SetNextItemWidth(field_width);
		bool y_changed{ ImGui::DragFloat(
			"##Y", &scale.y, options.speed, static_cast<float>(options.min),
			static_cast<float>(options.max), options.format, options.flags
		) };

		bool value_changed{ x_changed || y_changed };

		if (!value_changed || !scale_locked) {
			return value_changed;
		}

		constexpr float kScaleEpsilon{ 0.000001f };

		if (x_changed && !y_changed) {
			if (std::abs(previous_scale.x) > kScaleEpsilon) {
				float factor{ scale.x / previous_scale.x };
				scale.y = previous_scale.y * factor;
			} else if (std::abs(previous_scale.y) <= kScaleEpsilon) {
				scale.y = scale.x;
			}
		} else if (y_changed && !x_changed) {
			if (std::abs(previous_scale.y) > kScaleEpsilon) {
				float factor{ scale.y / previous_scale.y };
				scale.x = previous_scale.x * factor;
			} else if (std::abs(previous_scale.x) <= kScaleEpsilon) {
				scale.x = scale.y;
			}
		}

		return true;
	}) };

	ImGui::PopID();
	ImGui::PopID();

	return changed;
}

void DrawTransformComponent(Entity entity) {
	auto& transform{ entity.TryAdd<Transform>() };
	auto& depth{ entity.TryAdd<Depth>() };

	ImGui::PushID(static_cast<int>(Hash<Transform>()));
	bool open{ ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen) };
	ImGui::PopID();

	if (!open) {
		ImGui::Spacing();
		return;
	}

	auto read_only_reason{ HasAnyComponent<
		"Controlled by Button Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals,
		ButtonSpriteVisuals, ButtonTextVisuals>(entity) };

	if (read_only_reason.has_value() && !read_only_reason->empty()) {
		ImGui::TextDisabled(
			"%.*s", static_cast<int>(read_only_reason->size()), read_only_reason->data()
		);
	}

	ImGui::Indent();

	ReadOnlyScope read_only_scope{ read_only_reason.has_value() };

	DrawValue(
		"Position", transform.position,
		FieldOptions{
			.speed	= 1.0f,
			.format = "%.0f",
		}
	);

	DrawValue(
		"Depth", depth.value,
		FieldOptions{
			.speed	= 0.05f,
			.min	= -1000.0,
			.max	= 1000.0,
			.format = "%.2f",
			.flags	= ImGuiSliderFlags_AlwaysClamp,
		}
	);

	DrawValue(
		"Rotation", transform.rotation,
		FieldOptions{
			.speed	= 1.0f,
			.min	= 0.0,
			.max	= 360.0,
			.format = "%.1f deg",
			.flags	= ImGuiSliderFlags_AlwaysClamp,
		}
	);

	if (DrawScaleValue(
			entity, transform.scale,
			FieldOptions{
				.speed	= 0.01f,
				.min	= -1000.0,
				.max	= 1000.0,
				.format = "%.2f",
				.flags	= ImGuiSliderFlags_AlwaysClamp,
			}
		)) {
		transform.ClampScale();
	}

	ImGui::Unindent();
}


bool DrawRenderTargetSizeContents(Entity entity) {
	auto& target_size{ entity.Get<::ptgn::impl::RenderTargetSize>() };

	bool changed{
		ImGui::Checkbox(
			"Follow Display Size",
			&target_size.follow_display_size
		)
	};

	ImGui::BeginDisabled(target_size.follow_display_size);

	int size[2]{
		target_size.size.x,
		target_size.size.y,
	};

	if (ImGui::DragInt2(
			"Size",
			size,
			1.0f,
			1,
			16384,
			"%d",
			ImGuiSliderFlags_AlwaysClamp
		)) {
		target_size.size = {
			std::max(size[0], 1),
			std::max(size[1], 1),
		};

		changed = true;
	}

	ImGui::EndDisabled();

	return changed;
}

} // namespace

PTGN_REGISTER_COMPONENT(::ptgn::impl::Scripts,
	{
		.draw_contents = &DrawScriptsComponent,
	}
);

PTGN_REGISTER_COMPONENT(
	::ptgn::impl::RenderTargetSize,
	{
		.label = "Render Target Size",
		.group = "Graphics",
		.removable = false,
		.addable = false,
		.draw_contents = &DrawRenderTargetSizeContents,
	}
);


PTGN_REGISTER_COMPONENT(
	::ptgn::impl::CameraData,
	{
		.draw_contents = &DrawRegisteredContents<::ptgn::impl::CameraData>,
	}
);

PTGN_REGISTER_COMPONENT(
	Rect,
	{
		.get_read_only_reason = &HasAnyComponent<
			"Controlled by Button Shape Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals>,
		.draw_contents = &DrawRegisteredContents<Rect>,
	}
);

PTGN_REGISTER_COMPONENT(
	Polygon, {
				 .draw_contents = &DrawRegisteredContents<Polygon>,
			 }
);

PTGN_REGISTER_COMPONENT(
	Circle,
	{ .get_read_only_reason = &HasAnyComponent<
		  "Controlled by Button Shape Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals> }
);

PTGN_REGISTER_COMPONENT(
	::ptgn::impl::IDrawable, { .label		  = "Drawable",
							   .draw_contents = &DrawRegisteredContents<::ptgn::impl::IDrawable>,
							   .draw_add_menu = &DrawAddDrawableMenu }
);

PTGN_REGISTER_COMPONENT(
	StyledText, {
					.on_changed = &MarkTextLayoutDirty,
					.get_read_only_reason =
						&HasAnyComponent<"Controlled by Button Text Visuals", ButtonTextVisuals>,
					.draw_contents = &DrawRegisteredContents<StyledText>,
				}
);

PTGN_REGISTER_COMPONENT(
	TextBox, {
				 .on_changed = &MarkTextLayoutDirty,
				 .get_read_only_reason =
					 &HasAnyComponent<"Controlled by Button Text Visuals", ButtonTextVisuals>,
				 .draw_contents = &DrawRegisteredContents<TextBox>,
			 }
);

PTGN_REGISTER_COMPONENT(
	Color,
	{ .get_read_only_reason = &HasAnyComponent<
		  "Controlled by Button Shape Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals> }
);

PTGN_REGISTER_COMPONENT(
	FillStyle,
	{ .get_read_only_reason = &HasAnyComponent<
		  "Controlled by Button Shape Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals>,
	  .draw_contents = &DrawRegisteredContents<FillStyle> }
);

PTGN_REGISTER_COMPONENT(
	TextureKey, { .get_read_only_reason =
					  &HasAnyComponent<"Controlled by Button Sprite Visuals", ButtonSpriteVisuals> }
);

PTGN_REGISTER_COMPONENT(
	Tint, { .get_read_only_reason =
				&HasAnyComponent<"Controlled by Button Sprite Visuals", ButtonSpriteVisuals> }
);

PTGN_REGISTER_COMPONENT(
	::ptgn::impl::AnimationData,
	{ .get_read_only_reason =
		  &HasAnyComponent<"Controlled by Button Sprite Visuals", ButtonSpriteVisuals> }
);

PTGN_REGISTER_COMPONENT(
	::ptgn::impl::TextureSize,
	{ .get_read_only_reason =
		  &HasAnyComponent<"Controlled by Button Sprite Visuals", ButtonSpriteVisuals> }
);

PTGN_REGISTER_COMPONENT(
	::ptgn::impl::ButtonAnimationPart,
	{ .get_read_only_reason =
		  &HasAnyComponent<"Controlled by Button Sprite Visuals", ButtonSpriteVisuals> }
);

PTGN_REGISTER_COMPONENT(
	Visible, { .get_read_only_reason = &HasAnyComponent<
				   "Controlled by Button Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals,
				   ButtonSpriteVisuals, ButtonTextVisuals> }
);

PTGN_REGISTER_COMPONENT(
	Origin, { .get_read_only_reason = &HasAnyComponent<
				  "Controlled by Button Visuals", ButtonBackgroundVisuals, ButtonBorderVisuals,
				  ButtonSpriteVisuals, ButtonTextVisuals> }
);

PTGN_REGISTER_COMPONENT(
	ButtonBackgroundVisuals, {
								 .removable		  = false,
								 .addable		  = false,
								 .draw_after_tags = true,
								 .on_changed	  = &MarkButtonBackgroundDirty,
								 .draw_contents = &DrawRegisteredContents<ButtonBackgroundVisuals>,
							 }
);

PTGN_REGISTER_COMPONENT(
	ButtonBorderVisuals, {
							 .removable		  = false,
							 .addable		  = false,
							 .draw_after_tags = true,
							 .on_changed	  = &MarkButtonBorderDirty,
							 .draw_contents	  = &DrawRegisteredContents<ButtonBorderVisuals>,
						 }
);

PTGN_REGISTER_COMPONENT(
	ButtonSpriteVisuals, {
							 .removable		  = false,
							 .addable		  = false,
							 .draw_after_tags = true,
							 .on_changed	  = &MarkButtonSpriteDirty,
							 .draw_contents	  = &DrawRegisteredContents<ButtonSpriteVisuals>,
						 }
);

PTGN_REGISTER_COMPONENT(
	ButtonTextVisuals, {
						   .removable		= false,
						   .addable			= false,
						   .draw_after_tags = true,
						   .on_changed		= &MarkButtonTextDirty,
						   .draw_contents	= &DrawRegisteredContents<ButtonTextVisuals>,
					   }
);

PTGN_REGISTER_COMPONENT(
	ButtonSounds, {
					  .removable	   = false,
					  .addable		   = false,
					  .draw_after_tags = true,
					  .draw_contents   = &DrawRegisteredContents<ButtonSounds>,
				  }
);

void InspectorPanel::OnRender(EditorContext& ctx) {
	inspector::InspectorAssetManagerScope asset_manager_scope{ ctx.editor.GetAssetManager() };

	ImGui::Begin("Inspector");

	auto& scene_hierarchy{ ctx.editor.GetSceneHierarchyPanel() };
	auto selected_entity{ scene_hierarchy.GetSelectedEntity() };

	if (!selected_entity) {
		ImGui::End();
		return;
	}

	auto name{ std::string{ selected_entity.Get<Tag>() } };
	if (ImGui::InputText("Name", &name)) {
		selected_entity.Add<Tag>(name);
	}

	ImGui::Separator();

	DrawTransformComponent(selected_entity);
	ComponentEditorRegistry::DrawComponents(selected_entity);
	ComponentEditorRegistry::DrawTagComponents(selected_entity);
	ComponentEditorRegistry::DrawComponents(selected_entity, true);

	ImGui::Separator();

	if (ImGui::Button("Add Component", ImVec2{ -1.0f, 0.0f })) {
		ImGui::OpenPopup("AddComponentPopup");
	}

	if (ImGui::BeginPopup("AddComponentPopup")) {
		ComponentEditorRegistry::DrawAddComponentMenu(selected_entity);
		ImGui::EndPopup();
	}

	ImGui::End();
}

} // namespace ptgn::editor
