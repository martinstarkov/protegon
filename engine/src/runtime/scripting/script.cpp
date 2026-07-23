#include "runtime/scripting/script.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <ranges>
#include <utility>

#include "core/assert.h"
#include "core/math/math_utils.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

namespace {

[[nodiscard]] bool HasSequenceDefinitionPublic(const ScriptSequence& sequence) {
	return sequence.shared_reference || !sequence.steps.empty() ||
		!sequence.start_events.empty() || !sequence.stop_events.empty() ||
		!sequence.lifecycle_actions.empty();
}

} // namespace

Script& Script::operator=(const Script& other) {
	if (this != &other) {
		sequence = other.sequence;
		entity = {};
		delta_seconds_ = 0.0f;
		linear_progress_ = 0.0f;
		progress_ = 0.0f;
		repeat_ = 0;
		reversed_ = false;
		completion_requested_ = false;
	}
	return *this;
}

ScriptSequenceRuntime::ScriptSequenceRuntime() = default;
ScriptSequenceRuntime::~ScriptSequenceRuntime() = default;
ScriptSequenceRuntime::ScriptSequenceRuntime(ScriptSequenceRuntime&&) noexcept = default;
ScriptSequenceRuntime& ScriptSequenceRuntime::operator=(ScriptSequenceRuntime&&) noexcept = default;

void ScriptSequenceRuntime::ClearActiveScript() {
	script_instance.reset();
	elapsed_ms = 0.0f;
	current_repeat = 0;
	currently_reversed = false;
}

SequenceId ScriptSequence::NextSequenceId() {
	static SequenceId next{ 1 };
	return next++;
}

ScriptSequence::ScriptSequence(const ScriptSequence& other) :
	id{ NextSequenceId() },
	enabled{ other.enabled },
	shared_reference{ other.shared_reference },
	shared_sequence_id{ other.shared_sequence_id },
	name{ other.name },
	reentry{ other.reentry },
	channel{ other.channel },
	transient{ other.transient },
	remove_binding_on_complete{ other.remove_binding_on_complete },
	destroy_owner_on_complete{ other.destroy_owner_on_complete },
	start_events{ other.start_events },
	stop_events{ other.stop_events },
	steps{ other.steps },
	lifecycle_actions{ other.lifecycle_actions } {}

ScriptSequence& ScriptSequence::operator=(const ScriptSequence& other) {
	if (this != &other) {
		ScriptSequence copy{ other };
		*this = std::move(copy);
	}
	return *this;
}

ScriptSequence& ScriptSequence::Reentry(ReentryMode value) {
	reentry = value;
	return *this;
}

ScriptSequence& ScriptSequence::Channel(SequenceChannelKey value) {
	channel = std::move(value);
	return *this;
}

ScriptSequence& ScriptSequence::Transient(bool remove_on_complete) {
	transient = true;
	remove_binding_on_complete = remove_on_complete;
	return *this;
}

ScriptSequence& ScriptSequence::DestroyOwnerOnComplete(bool value) {
	destroy_owner_on_complete = value;
	return *this;
}

ScriptTiming& ScriptSequence::LatestDuringTiming() {
	auto it{ std::ranges::find_if(
		steps.rbegin(), steps.rend(), [](const ScriptStep& action) {
			return action.timing.has_value() &&
				action.completion == ScriptCompletion::Duration &&
				action.type_hash != Hash<WaitScript>();
		}
	) };
	PTGN_ASSERT(
		it != steps.rend(),
		"Ease, Repeat, Infinite, Reversed, and Yoyo require a preceding During call"
	);
	return *it->timing;
}

ScriptSequence& ScriptSequence::Ease(Ease value) {
	LatestDuringTiming().ease = value;
	return *this;
}

ScriptSequence& ScriptSequence::Repeat(int additional_repeats) {
	auto& timing{ LatestDuringTiming() };
	timing.additional_repeats = std::max(0, additional_repeats);
	timing.infinite_repeats = false;
	return *this;
}

ScriptSequence& ScriptSequence::Infinite() {
	LatestDuringTiming().infinite_repeats = true;
	return *this;
}

ScriptSequence& ScriptSequence::Reversed(bool value) {
	LatestDuringTiming().reversed = value;
	return *this;
}

ScriptSequence& ScriptSequence::Yoyo(bool value) {
	LatestDuringTiming().yoyo = value;
	return *this;
}

ScriptSequence& ScriptSequence::Wait(float duration_ms) {
	ScriptRegistry::EnsureRegistered<WaitScript>();
	ScriptStep action{ ScriptRegistry::MakeStep<WaitScript>() };
	action.completion = ScriptCompletion::Duration;
	action.timing = action.timing.value_or(ScriptTiming{});
	action.timing->duration_ms = std::max(0.0f, duration_ms);
	steps.push_back(std::move(action));
	return *this;
}

std::vector<ScriptRegistration>& ScriptRegistry::MutableEntries() {
	static std::vector<ScriptRegistration> entries;
	return entries;
}

const ScriptRegistration* ScriptRegistry::Find(TypeHashValue type_hash) {
	const auto& entries{ Entries() };
	const auto it{ std::ranges::find_if(entries, [type_hash](const auto& entry) {
		return entry.type_hash == type_hash;
	}) };
	return it == entries.end() ? nullptr : &*it;
}

const std::vector<ScriptRegistration>& ScriptRegistry::Entries() {
	return MutableEntries();
}

ScriptStep ScriptRegistry::MakeStep(TypeHashValue type_hash) {
	const auto* registration{ Find(type_hash) };
	if (!registration) {
		return {};
	}
	ScriptStep step;
	step.enabled = true;
	step.type_hash = registration->type_hash;
	step.value = registration->make_default();
	step.timing = registration->default_timing;
	return step;
}

std::vector<SequenceEventRegistration>& SequenceEventRegistry::MutableEntries() {
	static std::vector<SequenceEventRegistration> entries;
	return entries;
}

EventCondition SequenceEventRegistry::MakeCondition(TypeHashValue type_hash, json value) {
	const auto* entry{ Find(type_hash) };
	if (!entry) {
		return {};
	}
	EventCondition condition{
		.enabled = true,
		.consume = false,
		.type_hash = entry->type_hash,
	};
	if (value.is_null()) {
		entry->set_defaults(condition);
	} else {
		condition.value = std::move(value);
	}
	if (condition.value.is_null()) {
		condition.value = json::object();
	}
	return condition;
}

const SequenceEventRegistration* SequenceEventRegistry::Find(TypeHashValue type_hash) {
	const auto& entries{ Entries() };
	const auto it{ std::ranges::find_if(entries, [type_hash](const auto& entry) {
		return entry.type_hash == type_hash;
	}) };
	return it == entries.end() ? nullptr : &*it;
}

const std::vector<SequenceEventRegistration>& SequenceEventRegistry::Entries() {
	return MutableEntries();
}

ScriptSequence* SharedScriptSequenceRegistry::Find(SequenceId id) {
	const auto it{ std::ranges::find_if(sequences, [id](const auto& sequence) {
		return sequence.id == id;
	}) };
	return it == sequences.end() ? nullptr : &*it;
}

const ScriptSequence* SharedScriptSequenceRegistry::Find(SequenceId id) const {
	return const_cast<SharedScriptSequenceRegistry*>(this)->Find(id);
}

void impl::Scripts::AddEntryDeferred(ScriptEntry entry) {
	if (entry.type_hash != 0) {
		pending_additions.push_back(std::move(entry));
	}
}

void impl::Scripts::RemoveDeferred(SequenceId sequence_id) {
	if (!std::ranges::contains(pending_removals, sequence_id)) {
		pending_removals.push_back(sequence_id);
	}
}

void impl::Scripts::Attach(Entity owner) {
	owner_ = owner;
	for (auto& entry : scripts) {
		if (entry.instance) {
			impl_ScriptAccess::Attach(*entry.instance, owner);
		}
	}
	for (auto& entry : pending_additions) {
		if (entry.instance) {
			impl_ScriptAccess::Attach(*entry.instance, owner);
		}
	}
}

void impl::Scripts::ApplyPending() {
	if (owner_) {
		script_runtime::ApplyPending(owner_.GetScene());
	}
}

void impl::Scripts::CancelAll(SequenceCancelReason reason) {
	if (!owner_) {
		return;
	}
	for (auto& entry : scripts) {
		entry.enabled = false;
		if (entry.instance) {
			(void)script_runtime::Stop(owner_, entry.instance->sequence.id, reason);
			entry.instance->OnCancel(reason);
		}
	}
	for (auto& entry : pending_additions) {
		entry.enabled = false;
		if (entry.instance) {
			entry.instance->OnCancel(reason);
		}
	}
}

void impl::Scripts::OnEvent(Event event) {
	if (owner_) {
		(void)script_runtime::DispatchEvent(owner_, event);
	}
}

void from_json(const json& input, impl::Scripts& scripts) {
	if (input.is_object() && input.contains("scripts")) {
		input.at("scripts").get_to(scripts.scripts);
	} else if (input.is_array()) {
		input.get_to(scripts.scripts);
	}
	scripts.channels.clear();
	scripts.pending_additions.clear();
	scripts.pending_removals.clear();
	scripts.Attach({});
}

namespace {

[[nodiscard]] bool IsSerializableStep(const ScriptStep& step) {
	const auto* registration{ ScriptRegistry::Find(step.type_hash) };
	return registration && registration->serializable;
}

[[nodiscard]] bool IsSerializableSequence(const ScriptSequence& sequence) {
	if (sequence.transient) {
		return false;
	}
	return std::ranges::all_of(sequence.steps, &IsSerializableStep) &&
		std::ranges::all_of(sequence.lifecycle_actions, [](const LifecycleScript& action) {
			return IsSerializableStep(action.action);
		});
}

} // namespace

void to_json(json& output, const impl::Scripts& scripts) {
	output = json{ { "scripts", json::array() } };
	auto& serialized{ output["scripts"] };
	for (const auto& entry : scripts.scripts) {
		const auto* registration{ ScriptRegistry::Find(entry.type_hash) };
		const auto& sequence{ entry.instance ? entry.instance->sequence : entry.sequence };
		if (!registration || !registration->serializable || !IsSerializableSequence(sequence)) {
			continue;
		}

		// Serialize the authoring snapshot, updating only the sequence definition when a live
		// instance exists. Runtime state/factories/instances remain excluded by ScriptEntry reflection.
		if (entry.instance) {
			json serialized_entry;
			serialized_entry["enabled"] = entry.enabled;
			serialized_entry["type_hash"] = entry.type_hash;
			serialized_entry["value"] = entry.value;
			serialized_entry["sequence"] = sequence;
			serialized.push_back(std::move(serialized_entry));
		} else {
			serialized.push_back(entry);
		}
	}
}

namespace script_runtime {
namespace {


[[nodiscard]] bool HasSequenceDefinition(const ScriptSequence& sequence) {
	return sequence.shared_reference || !sequence.steps.empty() ||
		!sequence.start_events.empty() || !sequence.stop_events.empty() ||
		!sequence.lifecycle_actions.empty();
}

void CopySequenceDefinition(ScriptSequence& destination, const ScriptSequence& source) {
	const SequenceId id{ source.id };
	destination = source;
	destination.id = id;
	destination.runtime = ScriptSequenceRuntime{};
}

[[nodiscard]] bool StartBinding(
	Entity owner,
	ScriptSequence& binding,
	bool force
);
void UpdateSequence(Entity owner, ScriptSequence& binding, float delta_seconds);

[[nodiscard]] Script* EnsureInstance(Entity owner, ScriptEntry& entry) {
	if (!entry.instance) {
		const auto* registration{ ScriptRegistry::Find(entry.type_hash) };
		if (!registration) {
			return nullptr;
		}

		entry.instance = entry.runtime_factory
			? entry.runtime_factory()
			: (registration->instantiate ? registration->instantiate(entry.value) : nullptr);
		if (!entry.instance) {
			return nullptr;
		}
		CopySequenceDefinition(entry.instance->sequence, entry.sequence);
	}

	impl_ScriptAccess::Attach(*entry.instance, owner);
	if (!entry.initialized) {
		entry.instance->OnCreate();
		entry.instance->OnStart();
		entry.initialized = true;

		if (entry.instance->sequence.enabled &&
			entry.instance->sequence.start_events.empty() &&
			HasSequenceDefinition(entry.instance->sequence)) {
			(void)StartBinding(owner, entry.instance->sequence, false);
		}
	}
	return entry.instance.get();
}

[[nodiscard]] ScriptSequence* FindSequenceInScript(
	Script& script, SequenceId id
) {
	if (script.sequence.id == id) {
		return &script.sequence;
	}
	if (script.sequence.runtime.script_instance) {
		return FindSequenceInScript(*script.sequence.runtime.script_instance, id);
	}
	return nullptr;
}

[[nodiscard]] const ScriptSequence* FindSequenceInScript(
	const Script& script, SequenceId id
) {
	return FindSequenceInScript(const_cast<Script&>(script), id);
}

[[nodiscard]] ScriptEntry* FindSequenceEntry(Entity owner, SequenceId id) {
	if (!owner) {
		return nullptr;
	}
	auto* scripts{ owner.TryGet<impl::Scripts>() };
	if (!scripts) {
		return nullptr;
	}

	auto find = [&](auto& entries) -> ScriptEntry* {
		for (auto& entry : entries) {
			const SequenceId sequence_id{
				entry.instance ? entry.instance->sequence.id : entry.sequence.id
			};
			if (sequence_id == id) {
				return &entry;
			}
		}
		return nullptr;
	};

	if (auto* entry{ find(scripts->scripts) }) {
		return entry;
	}
	return find(scripts->pending_additions);
}

[[nodiscard]] ScriptSequence* FindBinding(Entity owner, SequenceId id) {
	if (!owner) {
		return nullptr;
	}
	auto* scripts{ owner.TryGet<impl::Scripts>() };
	if (!scripts) {
		return nullptr;
	}

	auto find = [&](auto& entries) -> ScriptSequence* {
		for (auto& entry : entries) {
			if (auto* script{ EnsureInstance(owner, entry) }) {
				if (auto* sequence{ FindSequenceInScript(*script, id) }) {
					return sequence;
				}
			}
		}
		return nullptr;
	};

	if (auto* sequence{ find(scripts->scripts) }) {
		return sequence;
	}
	return find(scripts->pending_additions);
}

[[nodiscard]] SequenceChannelRuntime* FindChannel(
	impl::Scripts& scripts, const SequenceChannelKey& key
) {
	const auto it{ std::ranges::find_if(
		scripts.channels,
		[&](const SequenceChannelRuntime& channel) {
			return channel.key == key;
		}
	) };
	return it == scripts.channels.end() ? nullptr : &*it;
}

[[nodiscard]] SequenceChannelRuntime& FindOrCreateChannel(
	impl::Scripts& scripts, const SequenceChannelKey& key
) {
	if (auto* channel{ FindChannel(scripts, key) }) {
		return *channel;
	}
	scripts.channels.push_back(SequenceChannelRuntime{ .key = key });
	return scripts.channels.back();
}

void ExecuteInstantStep(Entity owner, const ScriptStep& action);
void ProcessImmediateSteps(Entity owner, ScriptSequence& binding);
void CompleteSequence(Entity owner, ScriptSequence& binding);

void InvokeLifecycle(
	Entity owner,
	ScriptSequence& binding,
	SequenceLifecycle lifecycle
) {
	const auto* sequence{ Resolve(owner, binding) };
	if (!sequence) {
		return;
	}
	for (const auto& callback : sequence->lifecycle_actions) {
		if (callback.enabled && callback.lifecycle == lifecycle) {
			ExecuteInstantStep(owner, callback.action);
		}
	}
}

void PromoteNextInChannel(
	Entity owner, SequenceChannelRuntime& channel
) {
	while (!channel.waiting.empty()) {
		const SequenceId id{ channel.waiting.front() };
		channel.waiting.pop_front();
		if (auto* binding{ FindBinding(owner, id) }) {
			binding->runtime.waiting_for_channel = false;
			if (Start(owner, id, true)) {
				return;
			}
		}
	}
}

void ReleaseChannel(
	Entity owner,
	ScriptSequence& binding,
	bool promote
) {
	const auto* sequence{ Resolve(owner, binding) };
	if (!sequence || !sequence->channel || !owner.Has<impl::Scripts>()) {
		return;
	}

	auto& scripts{ owner.Get<impl::Scripts>() };
	auto* channel{ FindChannel(scripts, *sequence->channel) };
	if (!channel) {
		return;
	}
	if (channel->active == binding.id) {
		channel->active.reset();
	}
	std::erase(channel->waiting, binding.id);
	if (promote) {
		PromoteNextInChannel(owner, *channel);
	}
}

[[nodiscard]] bool CancelBinding(
	Entity owner,
	ScriptSequence& binding,
	SequenceCancelReason reason,
	bool log,
	bool promote_channel,
	bool remove_transient = true
) {
	const auto* sequence{ Resolve(owner, binding) };
	const bool active{
		binding.runtime.running || binding.runtime.waiting_for_channel
	};

	if (binding.runtime.script_instance) {
		auto& script{ *binding.runtime.script_instance };
		impl_ScriptAccess::SetFrame(
			script,
			0.0f, 0.0f, 0.0f,
			binding.runtime.current_repeat,
			binding.runtime.currently_reversed
		);
		if (script.sequence.runtime.running || script.sequence.runtime.waiting_for_channel) {
			(void)CancelBinding(
				owner, script.sequence, reason,
				false, false, false
			);
		}
		script.OnCancel(reason);
		InvokeLifecycle(owner, binding, SequenceLifecycle::ScriptCancel);
	}
	if (active) {
		InvokeLifecycle(owner, binding, SequenceLifecycle::Stop);
	}
	(void)log;

	ReleaseChannel(owner, binding, promote_channel);
	const int completed_runs{ binding.runtime.completed_runs };
	binding.runtime = ScriptSequenceRuntime{};
	binding.runtime.completed_runs = completed_runs;

	if (remove_transient && sequence &&
		(sequence->transient || sequence->remove_binding_on_complete) &&
		reason != SequenceCancelReason::Reset &&
		reason != SequenceCancelReason::BindingRemoved &&
		reason != SequenceCancelReason::OwnerDestroyed &&
		owner.Has<impl::Scripts>() && FindSequenceEntry(owner, binding.id)) {
		owner.Get<impl::Scripts>().RemoveDeferred(binding.id);
	}
	return active;
}

[[nodiscard]] bool StartBinding(
	Entity owner,
	ScriptSequence& binding,
	bool force
) {
	const auto* sequence{ Resolve(owner, binding) };
	if (!sequence || !sequence->enabled || !binding.enabled) {
		return false;
	}

	auto& runtime{ binding.runtime };
	if (runtime.running) {
		const ReentryMode mode{ force ? ReentryMode::Restart : sequence->reentry };
		switch (mode) {
			case ReentryMode::IgnoreWhileRunning:
				return false;
			case ReentryMode::Restart:
				(void)CancelBinding(
					owner, binding, SequenceCancelReason::Replaced,
					false, false, false
				);
				break;
			case ReentryMode::Queue:
				++runtime.queued_runs;
				return true;
		}
	}

	if (sequence->channel) {
		auto& scripts{ owner.TryAdd<impl::Scripts>() };
		scripts.Attach(owner);
		auto& channel{ FindOrCreateChannel(scripts, *sequence->channel) };
		if (channel.active && *channel.active != binding.id) {
			const ReentryMode mode{ force ? ReentryMode::Restart : sequence->reentry };
			switch (mode) {
				case ReentryMode::IgnoreWhileRunning:
					return false;
				case ReentryMode::Restart:
					script_runtime::StopChannel(
						owner, *sequence->channel, SequenceStopMode::All
					);
					break;
				case ReentryMode::Queue:
					if (!std::ranges::contains(channel.waiting, binding.id)) {
						channel.waiting.push_back(binding.id);
					}
					runtime.waiting_for_channel = true;
					return true;
			}
		}
		channel.active = binding.id;
		runtime.waiting_for_channel = false;
	}

	const int completed_runs{ runtime.completed_runs };
	const int queued_runs{ runtime.queued_runs };
	runtime = ScriptSequenceRuntime{};
	runtime.completed_runs = completed_runs;
	runtime.queued_runs = queued_runs;
	runtime.running = true;

	InvokeLifecycle(owner, binding, SequenceLifecycle::Start);
	ProcessImmediateSteps(owner, binding);
	return true;
}

[[nodiscard]] std::unique_ptr<Script> InstantiateStepScript(
	Entity owner, const ScriptStep& action
) {
	const auto* registration{ ScriptRegistry::Find(action.type_hash) };
	if (!registration) {
		return nullptr;
	}
	auto instance{
		action.runtime_factory
			? action.runtime_factory()
			: registration->instantiate(action.value)
	};
	if (instance) {
		impl_ScriptAccess::Attach(*instance, owner);
	}
	return instance;
}

void StartChildScript(
	Entity owner,
	Script& script,
	float linear,
	float progress,
	int repeat,
	bool reversed
) {
	impl_ScriptAccess::SetFrame(script, 0.0f, linear, progress, repeat, reversed);
	script.OnCreate();
	script.OnStart();
	if (script.sequence.enabled && script.sequence.start_events.empty() &&
		HasSequenceDefinition(script.sequence)) {
		(void)StartBinding(owner, script.sequence, false);
	}
}

[[nodiscard]] ScriptStatus UpdateChildScript(
	Entity owner,
	Script& script,
	float delta_seconds
) {
	ScriptStatus status{ script.OnUpdate() };
	UpdateSequence(owner, script.sequence, delta_seconds);
	if (impl_ScriptAccess::TakeCompletionRequest(script) ||
		(HasSequenceDefinition(script.sequence) && script.sequence.runtime.completed)) {
		status = ScriptStatus::Complete;
	}
	return status;
}

void ExecuteInstantStep(Entity owner, const ScriptStep& action) {
	if (!action.enabled) {
		return;
	}
	auto instance{ InstantiateStepScript(owner, action) };
	if (!instance) {
		return;
	}
	StartChildScript(owner, *instance, 1.0f, 1.0f, 0, false);
	impl_ScriptAccess::SetFrame(*instance, 0.0f, 1.0f, 1.0f, 0, false);
	(void)UpdateChildScript(owner, *instance, 0.0f);
	instance->OnComplete();
}

void ProcessImmediateSteps(
	Entity owner, ScriptSequence& binding
) {
	const auto* sequence{ Resolve(owner, binding) };
	if (!sequence) {
		return;
	}
	auto& runtime{ binding.runtime };
	while (runtime.running && runtime.step_index < sequence->steps.size()) {
		const auto& action{ sequence->steps[runtime.step_index] };
		if (!action.enabled) {
			++runtime.step_index;
			continue;
		}
		const auto* registration{ ScriptRegistry::Find(action.type_hash) };
		if (!registration) {
			++runtime.step_index;
			continue;
		}
		const ScriptCompletion completion{
			action.completion.value_or(registration->completion)
		};
		if (completion != ScriptCompletion::Instant) {
			return;
		}
		InvokeLifecycle(owner, binding, SequenceLifecycle::ScriptStart);
		ExecuteInstantStep(owner, action);
		InvokeLifecycle(owner, binding, SequenceLifecycle::ScriptComplete);
		++runtime.step_index;
	}
	if (runtime.running && runtime.step_index >= sequence->steps.size()) {
		CompleteSequence(owner, binding);
	}
}

void CompleteCurrentStep(
	Entity owner, ScriptSequence& binding
) {
	auto& runtime{ binding.runtime };
	if (runtime.script_instance) {
		impl_ScriptAccess::SetFrame(
			*runtime.script_instance,
			0.0f, 1.0f, 1.0f,
			runtime.current_repeat,
			runtime.currently_reversed
		);
		runtime.script_instance->OnComplete();
	}
	InvokeLifecycle(owner, binding, SequenceLifecycle::ScriptComplete);
	++runtime.step_index;
	runtime.ClearActiveScript();
	ProcessImmediateSteps(owner, binding);
}

void CompleteSequence(
	Entity owner, ScriptSequence& binding
) {
	const auto* sequence{ Resolve(owner, binding) };
	if (!sequence) {
		return;
	}
	const bool remove_binding{
		sequence->transient || sequence->remove_binding_on_complete
	};
	const bool destroy_owner{ sequence->destroy_owner_on_complete };
	const int queued_runs{ binding.runtime.queued_runs };
	const int completed_runs{ binding.runtime.completed_runs + 1 };

	binding.runtime.running = false;
	binding.runtime.paused = false;
	binding.runtime.completed = true;
	binding.runtime.completed_runs = completed_runs;
	binding.runtime.script_instance.reset();

	InvokeLifecycle(owner, binding, SequenceLifecycle::Complete);

	if (queued_runs > 0 && !destroy_owner) {
		binding.runtime.queued_runs = queued_runs - 1;
		(void)StartBinding(owner, binding, true);
		return;
	}
	ReleaseChannel(owner, binding, true);
	if (destroy_owner) {
		owner.Destroy();
		return;
	}
	if (remove_binding && owner.Has<impl::Scripts>()) {
		if (auto* entry{ FindSequenceEntry(owner, binding.id) }) {
			if (entry->instance) {
				entry->instance->OnComplete();
			}
			owner.Get<impl::Scripts>().RemoveDeferred(binding.id);
		}
	}
}

void UpdateSequence(
	Entity owner,
	ScriptSequence& binding,
	float delta_seconds
) {
	auto& runtime{ binding.runtime };
	if (!runtime.running || runtime.paused || runtime.waiting_for_channel) {
		return;
	}
	const auto* sequence{ Resolve(owner, binding) };
	if (!sequence || runtime.step_index >= sequence->steps.size()) {
		CompleteSequence(owner, binding);
		return;
	}

	const auto& action{ sequence->steps[runtime.step_index] };
	if (!action.enabled) {
		++runtime.step_index;
		ProcessImmediateSteps(owner, binding);
		return;
	}
	const auto* registration{ ScriptRegistry::Find(action.type_hash) };
	if (!registration) {
		++runtime.step_index;
		ProcessImmediateSteps(owner, binding);
		return;
	}
	const ScriptCompletion completion{
		action.completion.value_or(registration->completion)
	};
	if (completion == ScriptCompletion::Instant) {
		ProcessImmediateSteps(owner, binding);
		return;
	}

	if (!runtime.script_instance) {
		runtime.script_instance = InstantiateStepScript(owner, action);
		if (!runtime.script_instance) {
			++runtime.step_index;
			ProcessImmediateSteps(owner, binding);
			return;
		}
		runtime.currently_reversed = action.timing && action.timing->reversed;
		StartChildScript(
			owner, *runtime.script_instance,
			0.0f, 0.0f, 0, runtime.currently_reversed
		);
		InvokeLifecycle(owner, binding, SequenceLifecycle::ScriptStart);
	}

	const float delta_ms{ std::max(0.0f, delta_seconds) * 1000.0f };
	if (action.timing) {
		runtime.elapsed_ms += delta_ms;
	}
	float linear{ 0.0f };
	float progress{ 0.0f };
	if (action.timing) {
		const auto& timing{ *action.timing };
		linear = timing.duration_ms <= 0.0f
			? 1.0f
			: std::clamp(runtime.elapsed_ms / timing.duration_ms, 0.0f, 1.0f);
		const float directed{ runtime.currently_reversed ? 1.0f - linear : linear };
		progress = ptgn::ApplyEase(directed, timing.ease);
	}

	impl_ScriptAccess::SetFrame(
		*runtime.script_instance,
		delta_seconds, linear, progress,
		runtime.current_repeat, runtime.currently_reversed
	);
	const ScriptStatus status{
		UpdateChildScript(owner, *runtime.script_instance, delta_seconds)
	};

	bool complete{ false };
	switch (completion) {
		case ScriptCompletion::Instant:
			complete = true;
			break;
		case ScriptCompletion::Duration:
			complete = status == ScriptStatus::Complete || linear >= 1.0f;
			break;
		case ScriptCompletion::ScriptControlled:
			complete = status == ScriptStatus::Complete;
			break;
		case ScriptCompletion::Infinite:
			// Infinite disables automatic time-based completion, but the Script may
			// still explicitly call Complete()/MoveOn().
			complete = status == ScriptStatus::Complete;
			break;
	}
	if (!complete) {
		return;
	}

	const bool repeat{
		action.timing &&
		(action.timing->infinite_repeats ||
		 runtime.current_repeat < action.timing->additional_repeats)
	};
	if (repeat) {
		++runtime.current_repeat;
		runtime.elapsed_ms = 0.0f;
		InvokeLifecycle(owner, binding, SequenceLifecycle::Repeat);
		if (action.timing->yoyo) {
			runtime.currently_reversed = !runtime.currently_reversed;
			InvokeLifecycle(owner, binding, SequenceLifecycle::Yoyo);
		}
		impl_ScriptAccess::SetFrame(
			*runtime.script_instance,
			0.0f, 0.0f, 0.0f,
			runtime.current_repeat, runtime.currently_reversed
		);
		runtime.script_instance->OnRepeat();
		return;
	}
	CompleteCurrentStep(owner, binding);
}

void HandleSequenceEvent(
	Entity owner,
	ScriptSequence& binding,
	Event event
) {
	const auto* definition{ Resolve(owner, binding) };
	if (!definition) {
		return;
	}

	for (const auto& condition : definition->stop_events) {
		if (!condition.enabled) {
			continue;
		}
		const auto* registration{ SequenceEventRegistry::Find(condition.type_hash) };
		if (registration && registration->matches(
				owner, event, condition, condition.consume
			)) {
			(void)CancelBinding(
				owner, binding, SequenceCancelReason::Stopped,
				true, true
			);
			return;
		}
	}

	for (const auto& condition : definition->start_events) {
		if (!condition.enabled) {
			continue;
		}
		const auto* registration{ SequenceEventRegistry::Find(condition.type_hash) };
		if (registration && registration->matches(
				owner, event, condition, condition.consume
			)) {
			(void)StartBinding(owner, binding, false);
			return;
		}
	}
}

void DispatchToScript(
	Entity owner,
	Script& script,
	Event event
) {
	script.OnEvent(event);
	if (event.IsHandled()) {
		return;
	}
	HandleSequenceEvent(owner, script.sequence, event);
	if (event.IsHandled()) {
		return;
	}
	if (script.sequence.runtime.script_instance) {
		DispatchToScript(owner, *script.sequence.runtime.script_instance, event);
	}
}

} // namespace

void AttachEntry(Entity entity, ScriptEntry& entry) {
	if (!entity || !entry.type_hash) {
		return;
	}
	(void)EnsureInstance(entity, entry);
}

void AttachAll(Entity entity) {
	RegisterEngineScriptTypes();
	if (!entity) {
		return;
	}
	auto* scripts{ entity.TryGet<impl::Scripts>() };
	if (!scripts) {
		return;
	}
	for (auto& entry : scripts->scripts) {
		AttachEntry(entity, entry);
	}
	for (auto& entry : scripts->pending_additions) {
		AttachEntry(entity, entry);
	}
}

void ApplyPending(Scene& scene) {
	const auto entities{ scene.EntitiesWith<impl::Scripts>().GetVector() };
	for (Entity entity : entities) {
		auto* scripts_ptr{ entity.TryGet<impl::Scripts>() };
		if (!scripts_ptr) {
			continue;
		}
		auto& scripts{ *scripts_ptr };
		scripts.Attach(entity);
		for (const SequenceId id : scripts.pending_removals) {
			bool completed{ false };
			if (auto* binding{ FindBinding(entity, id) }) {
				completed = binding->runtime.completed;
				if (!completed) {
					(void)CancelBinding(
						entity, *binding, SequenceCancelReason::BindingRemoved,
						false, true, false
					);
				}
			}

			const auto remove_matching_entry = [id, completed](ScriptEntry& entry) {
				const SequenceId sequence_id{
					entry.instance ? entry.instance->sequence.id : entry.sequence.id
				};
				if (sequence_id != id) {
					return false;
				}
				entry.enabled = false;
				if (entry.instance && !completed) {
					entry.instance->OnCancel(SequenceCancelReason::BindingRemoved);
				}
				return true;
			};
			std::erase_if(scripts.scripts, remove_matching_entry);
			std::erase_if(scripts.pending_additions, remove_matching_entry);
		}
		scripts.pending_removals.clear();
		for (auto& entry : scripts.pending_additions) {
			scripts.scripts.push_back(std::move(entry));
			AttachEntry(entity, scripts.scripts.back());
		}
		scripts.pending_additions.clear();
	}
}

void Update(Scene& scene, secondsf delta_time) {
	RegisterEngineScriptTypes();
	const float delta_seconds{ std::max(0.0f, delta_time.count()) };
	ApplyPending(scene);
	const auto entities{ scene.EntitiesWith<impl::Scripts>().GetVector() };
	for (Entity entity : entities) {
		auto* scripts{ entity.TryGet<impl::Scripts>() };
		if (!scripts) {
			continue;
		}
		scripts->Attach(entity);
		for (auto& entry : scripts->scripts) {
			auto* script{ EnsureInstance(entity, entry) };
			if (!script || !entry.enabled) {
				continue;
			}
			impl_ScriptAccess::SetFrame(
				*script, delta_seconds, 0.0f, 0.0f, 0, false
			);
			const ScriptStatus status{ script->OnUpdate() };
			UpdateSequence(entity, script->sequence, delta_seconds);
			if (status == ScriptStatus::Complete ||
				impl_ScriptAccess::TakeCompletionRequest(*script)) {
				script->OnComplete();
				entry.enabled = false;
			}
		}
	}
}


bool DispatchEvent(Entity entity, Event event) {
	RegisterEngineScriptTypes();
	if (!entity || !entity.Has<impl::Scripts>()) {
		return false;
	}
	auto& scripts{ entity.Get<impl::Scripts>() };
	scripts.Attach(entity);
	for (auto& entry : scripts.scripts) {
		if (!entry.enabled) {
			continue;
		}
		if (auto* script{ EnsureInstance(entity, entry) }) {
			DispatchToScript(entity, *script, event);
			if (event.IsHandled()) {
				break;
			}
		}
	}
	return event.IsHandled();
}

bool DispatchGlobalEvent(Scene& scene, Event event) {
	RegisterEngineScriptTypes();
	const auto entities{ scene.EntitiesWith<impl::Scripts>().GetVector() };
	for (Entity entity : entities) {
		auto* scripts{ entity.TryGet<impl::Scripts>() };
		if (!scripts) {
			continue;
		}
		for (auto& entry : scripts->scripts) {
			if (!entry.enabled) {
				continue;
			}
			if (auto* script{ EnsureInstance(entity, entry) }) {
				DispatchToScript(entity, *script, event);
				if (event.IsHandled()) {
					return true;
				}
			}
		}
	}
	return event.IsHandled();
}

ScriptSequence* Resolve(
	Entity owner, ScriptSequence& binding
) {
	return binding.shared_reference
		? owner.GetScene().ctx().shared_script_sequences.Find(binding.shared_sequence_id)
		: &binding;
}

const ScriptSequence* Resolve(
	Entity owner, const ScriptSequence& binding
) {
	return binding.shared_reference
		? owner.GetScene().ctx().shared_script_sequences.Find(binding.shared_sequence_id)
		: &binding;
}

SequenceHandle RunSequence(
	Entity owner, ScriptSequence sequence
) {
	if (!owner) {
		return {};
	}
	auto& scripts{ owner.TryAdd<impl::Scripts>() };
	scripts.Attach(owner);
	const SequenceId id{ sequence.id };
	ScriptRegistry::EnsureRegistered<WaitScript>();
	ScriptEntry entry;
	entry.enabled = true;
	entry.type_hash = Hash<Script>();
	entry.value = json::object();
	entry.sequence = std::move(sequence);
	entry.instance = std::make_unique<Script>();
	const SequenceId sequence_id{ entry.sequence.id };
	entry.instance->sequence = entry.sequence;
	entry.instance->sequence.id = sequence_id;
	entry.sequence.id = sequence_id;
	scripts.pending_additions.push_back(std::move(entry));
	AttachEntry(owner, scripts.pending_additions.back());
	return SequenceHandle{ .owner = owner, .binding_id = id };
}

SequenceHandle RunInChannel(
	Entity owner,
	SequenceChannelKey channel,
	ScriptSequence sequence,
	ReentryMode reentry
) {
	sequence.channel = std::move(channel);
	sequence.reentry = reentry;
	return RunSequence(owner, std::move(sequence));
}

bool Start(Entity owner, SequenceId id, bool force) {
	auto* binding{ FindBinding(owner, id) };
	return binding && StartBinding(owner, *binding, force);
}

void StopChannel(
	Entity owner,
	SequenceChannelKey channel_key,
	SequenceStopMode mode
) {
	if (!owner || !owner.Has<impl::Scripts>()) {
		return;
	}
	auto& scripts{ owner.Get<impl::Scripts>() };
	auto* channel{ FindChannel(scripts, channel_key) };
	if (!channel) {
		return;
	}
	std::vector<SequenceId> ids;
	if (channel->active) {
		ids.push_back(*channel->active);
	}
	if (mode == SequenceStopMode::All) {
		ids.insert(ids.end(), channel->waiting.begin(), channel->waiting.end());
	}
	channel->active.reset();
	channel->waiting.clear();
	for (SequenceId id : ids) {
		if (auto* binding{ FindBinding(owner, id) }) {
			(void)CancelBinding(
				owner, *binding, SequenceCancelReason::Replaced,
				false, false
			);
		}
	}
}

bool Stop(Entity owner, SequenceId id, SequenceCancelReason reason) {
	auto* binding{ FindBinding(owner, id) };
	return binding && CancelBinding(owner, *binding, reason, true, true);
}

bool Reset(Entity owner, SequenceId id) {
	auto* binding{ FindBinding(owner, id) };
	if (!binding) {
		return false;
	}
	(void)CancelBinding(
		owner, *binding, SequenceCancelReason::Reset,
		false, true
	);
	InvokeLifecycle(owner, *binding, SequenceLifecycle::Reset);
	return true;
}

bool Clear(Entity owner, SequenceId id) {
	auto* binding{ FindBinding(owner, id) };
	if (!binding) {
		return false;
	}
	(void)CancelBinding(
		owner, *binding, SequenceCancelReason::Cleared,
		false, true
	);
	if (binding->shared_reference || binding->transient) {
		owner.Get<impl::Scripts>().RemoveDeferred(id);
	} else {
		binding->steps.clear();
		binding->start_events.clear();
		binding->stop_events.clear();
		binding->lifecycle_actions.clear();
	}
	return true;
}

bool Skip(Entity owner, SequenceId id) {
	auto* binding{ FindBinding(owner, id) };
	if (!binding || !binding->runtime.running) {
		return false;
	}
	if (binding->runtime.script_instance) {
		binding->runtime.script_instance->OnCancel(SequenceCancelReason::Skipped);
		InvokeLifecycle(owner, *binding, SequenceLifecycle::ScriptCancel);
	}
	++binding->runtime.step_index;
	binding->runtime.ClearActiveScript();
	ProcessImmediateSteps(owner, *binding);
	return true;
}

bool Seek(Entity owner, SequenceId id, float progress) {
	auto* binding{ FindBinding(owner, id) };
	if (!binding) {
		return false;
	}
	if (!binding->runtime.running && !StartBinding(owner, *binding, true)) {
		return false;
	}
	const auto* sequence{ Resolve(owner, *binding) };
	if (!sequence || binding->runtime.step_index >= sequence->steps.size()) {
		return false;
	}
	const auto& action{ sequence->steps[binding->runtime.step_index] };
	if (!action.timing) {
		return false;
	}
	binding->runtime.elapsed_ms =
		std::clamp(progress, 0.0f, 1.0f) *
		std::max(0.0f, action.timing->duration_ms);
	UpdateSequence(owner, *binding, 0.0f);
	return true;
}

bool SetPaused(Entity owner, SequenceId id, bool paused) {
	auto* binding{ FindBinding(owner, id) };
	if (!binding || !binding->runtime.running || binding->runtime.paused == paused) {
		return false;
	}
	binding->runtime.paused = paused;
	InvokeLifecycle(
		owner, *binding,
		paused ? SequenceLifecycle::Pause : SequenceLifecycle::Resume
	);
	return true;
}

float Progress(Entity owner, SequenceId id) {
	const auto* binding{ FindBinding(owner, id) };
	if (!binding) {
		return 0.0f;
	}
	const auto* sequence{ Resolve(owner, *binding) };
	if (!sequence || !binding->runtime.running ||
		binding->runtime.step_index >= sequence->steps.size()) {
		return binding->runtime.completed ? 1.0f : 0.0f;
	}
	const auto& action{ sequence->steps[binding->runtime.step_index] };
	if (!action.timing || action.timing->duration_ms <= 0.0f) {
		return 0.0f;
	}
	return std::clamp(
		binding->runtime.elapsed_ms / action.timing->duration_ms,
		0.0f, 1.0f
	);
}

bool IsRunning(Entity owner, SequenceId id) {
	const auto* binding{ FindBinding(owner, id) };
	return binding && binding->runtime.running;
}

bool IsPaused(Entity owner, SequenceId id) {
	const auto* binding{ FindBinding(owner, id) };
	return binding && binding->runtime.paused;
}

bool IsCompleted(Entity owner, SequenceId id) {
	const auto* binding{ FindBinding(owner, id) };
	return binding && binding->runtime.completed;
}

} // namespace script_runtime


SequenceHandle ScriptSequence::Start(Entity owner, bool force) const {
	ScriptSequence definition{ *this };
	definition.runtime = ScriptSequenceRuntime{};
	auto handle{ script_runtime::RunSequence(owner, std::move(definition)) };
	(void)handle.Start(force);
	return handle;
}

bool SequenceHandle::Start(bool force) const {
	return script_runtime::Start(owner, binding_id, force);
}

bool SequenceHandle::Stop() const {
	return script_runtime::Stop(owner, binding_id);
}

bool SequenceHandle::Pause() const {
	return script_runtime::SetPaused(owner, binding_id, true);
}

bool SequenceHandle::Resume() const {
	return script_runtime::SetPaused(owner, binding_id, false);
}

bool SequenceHandle::TogglePaused() const {
	return IsPaused() ? Resume() : Pause();
}

bool SequenceHandle::Toggle() const {
	return IsRunning() ? Stop() : Start();
}

bool SequenceHandle::Reset() const {
	return script_runtime::Reset(owner, binding_id);
}

bool SequenceHandle::Clear() const {
	return script_runtime::Clear(owner, binding_id);
}

bool SequenceHandle::Skip() const {
	return script_runtime::Skip(owner, binding_id);
}

bool SequenceHandle::Seek(float progress) const {
	return script_runtime::Seek(owner, binding_id, progress);
}

bool SequenceHandle::IsRunning() const {
	return script_runtime::IsRunning(owner, binding_id);
}

bool SequenceHandle::IsPaused() const {
	return script_runtime::IsPaused(owner, binding_id);
}

bool SequenceHandle::IsCompleted() const {
	return script_runtime::IsCompleted(owner, binding_id);
}

float SequenceHandle::Progress() const {
	return script_runtime::Progress(owner, binding_id);
}

} // namespace ptgn
