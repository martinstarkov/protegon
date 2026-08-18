#include "runtime/timer/timer.h"

#include <algorithm>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>

#include "core/log.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/script.h"
#include "runtime/timer/timer_event.h"

namespace ptgn {

namespace {

[[nodiscard]] millisecondsf ClampTimerDuration(millisecondsf duration) {
	return millisecondsf{ std::max(0.0f, duration.count()) };
}

[[nodiscard]] TimerEntry* FindTimerEntry(Entity entity, const TimerKey& key) {
	if (!entity) {
		return nullptr;
	}

	auto* timers{ entity.TryGet<impl::Timers>() };
	if (!timers) {
		return nullptr;
	}

	const auto it{ std::ranges::find_if(
		timers->timers,
		[&key](const TimerEntry& entry) {
			return entry.config.key == key;
		}
	) };
	return it == timers->timers.end() ? nullptr : std::addressof(*it);
}

void InitializeTimerRuntime(TimerEntry& entry, bool apply_auto_start) {
	if (entry.runtime.initialized) {
		return;
	}

	entry.runtime.timer.Reset();
	entry.runtime.elapsed_count = 0;
	entry.runtime.completed = false;
	entry.runtime.initialized = true;

	if (apply_auto_start && entry.config.start_automatically) {
		entry.runtime.timer.Start();
	}
}

[[nodiscard]] TimerEntry* ResolveHandle(const TimerHandle& handle) {
	return FindTimerEntry(handle.owner, handle.key);
}

[[nodiscard]] const TimerEntry* ResolveHandleConst(const TimerHandle& handle) {
	return FindTimerEntry(handle.owner, handle.key);
}

} // namespace

namespace impl {

void to_json(json& output, const Timers& timers) {
	output = json::object();
	output["timers"] = json::array();

	for (const auto& entry : timers.timers) {
		output["timers"].push_back(entry.config);
	}
}

void from_json(const json& input, Timers& timers) {
	timers.timers.clear();

	const json* entries{ nullptr };
	if (input.is_array()) {
		entries = std::addressof(input);
	} else if (input.is_object()) {
		const auto it{ input.find("timers") };
		if (it != input.end() && it->is_array()) {
			entries = std::addressof(*it);
		}
	}

	if (!entries) {
		return;
	}

	for (const auto& serialized : *entries) {
		TimerEntry entry;
		try {
			serialized.get_to(entry.config);
		} catch (...) {
			continue;
		}

		entry.config.duration = ClampTimerDuration(entry.config.duration);
		entry.runtime = TimerRuntime{};
		timers.timers.push_back(std::move(entry));
	}
}

} // namespace impl

TimerHandle::operator bool() const {
	return owner && !key.value.empty() && HasTimer(owner, key);
}

bool TimerHandle::Start() const {
	auto* entry{ ResolveHandle(*this) };
	if (!entry) {
		return false;
	}

	InitializeTimerRuntime(*entry, false);
	entry->runtime.completed = false;
	return entry->runtime.timer.Start(false);
}

bool TimerHandle::Restart() const {
	auto* entry{ ResolveHandle(*this) };
	if (!entry) {
		return false;
	}

	InitializeTimerRuntime(*entry, false);
	entry->runtime.completed = false;
	return entry->runtime.timer.Restart();
}

bool TimerHandle::Stop() const {
	auto* entry{ ResolveHandle(*this) };
	if (!entry || !entry->runtime.initialized) {
		return false;
	}

	bool active{ entry->runtime.timer.IsRunning() || entry->runtime.timer.IsPaused() };
	entry->runtime.timer.Stop();
	return active;
}

bool TimerHandle::Reset() const {
	auto* entry{ ResolveHandle(*this) };
	if (!entry) {
		return false;
	}

	entry->runtime.timer.Reset();
	entry->runtime.elapsed_count = 0;
	entry->runtime.completed = false;
	entry->runtime.initialized = true;
	return true;
}

bool TimerHandle::Pause() const {
	auto* entry{ ResolveHandle(*this) };
	if (!entry || !entry->runtime.initialized || !entry->runtime.timer.IsRunning()) {
		return false;
	}

	entry->runtime.timer.Pause();
	return true;
}

bool TimerHandle::Resume() const {
	auto* entry{ ResolveHandle(*this) };
	if (!entry || !entry->runtime.initialized || !entry->runtime.timer.IsPaused()) {
		return false;
	}

	entry->runtime.timer.Resume();
	return true;
}

bool TimerHandle::TogglePaused() const {
	if (IsPaused()) {
		return Resume();
	}
	if (IsRunning()) {
		return Pause();
	}
	return false;
}

bool TimerHandle::Advance(millisecondsf amount) const {
	auto* entry{ ResolveHandle(*this) };
	if (!entry || amount <= millisecondsf{ 0.0f }) {
		return false;
	}

	InitializeTimerRuntime(*entry, false);
	entry->runtime.timer.AddElapsed(amount);
	return true;
}

bool TimerHandle::Rewind(millisecondsf amount) const {
	auto* entry{ ResolveHandle(*this) };
	if (!entry || amount <= millisecondsf{ 0.0f }) {
		return false;
	}

	InitializeTimerRuntime(*entry, false);
	entry->runtime.timer.RemoveElapsed(amount);
	entry->runtime.completed = false;
	return true;
}

bool TimerHandle::SetDuration(millisecondsf duration) const {
	auto* entry{ ResolveHandle(*this) };
	if (!entry) {
		return false;
	}

	const millisecondsf updated{ ClampTimerDuration(duration) };
	if (entry->config.duration == updated) {
		return false;
	}

	entry->config.duration = updated;
	entry->runtime.completed = false;
	return true;
}

bool TimerHandle::AddDuration(millisecondsf amount) const {
	if (amount <= millisecondsf{ 0.0f }) {
		return false;
	}
	return SetDuration(Duration() + amount);
}

bool TimerHandle::RemoveDuration(millisecondsf amount) const {
	if (amount <= millisecondsf{ 0.0f }) {
		return false;
	}
	return SetDuration(Duration() - amount);
}

millisecondsf TimerHandle::Elapsed() const {
	const auto* entry{ ResolveHandleConst(*this) };
	if (!entry || !entry->runtime.initialized) {
		return millisecondsf{ 0.0f };
	}
	return entry->runtime.timer.ElapsedDuration<millisecondsf>();
}

millisecondsf TimerHandle::Remaining() const {
	return millisecondsf{
		std::max(0.0f, Duration().count() - Elapsed().count())
	};
}

millisecondsf TimerHandle::Duration() const {
	const auto* entry{ ResolveHandleConst(*this) };
	return entry ? ClampTimerDuration(entry->config.duration) : millisecondsf{ 0.0f };
}

float TimerHandle::Progress() const {
	const auto* entry{ ResolveHandleConst(*this) };
	if (!entry) {
		return 0.0f;
	}

	const millisecondsf duration{ ClampTimerDuration(entry->config.duration) };
	if (duration <= millisecondsf{ 0.0f }) {
		return entry->runtime.completed || entry->runtime.timer.HasRun() ? 1.0f : 0.0f;
	}

	return entry->runtime.timer.ElapsedFraction(duration);
}

bool TimerHandle::IsRunning() const {
	const auto* entry{ ResolveHandleConst(*this) };
	return entry && entry->runtime.initialized && entry->runtime.timer.IsRunning();
}

bool TimerHandle::IsPaused() const {
	const auto* entry{ ResolveHandleConst(*this) };
	return entry && entry->runtime.initialized && entry->runtime.timer.IsPaused();
}

bool TimerHandle::IsCompleted() const {
	const auto* entry{ ResolveHandleConst(*this) };
	return entry && entry->runtime.completed;
}

std::uint64_t TimerHandle::ElapsedCount() const {
	const auto* entry{ ResolveHandleConst(*this) };
	return entry ? entry->runtime.elapsed_count : 0;
}

TimerHandle AddTimer(
	Entity entity,
	TimerKey key,
	millisecondsf duration,
	TimerMode mode,
	bool start_automatically
) {
	if (!entity || key.value.empty()) {
		PTGN_WARN("Cannot add a timer without an entity and a non-empty key");
		return {};
	}

	auto& timers{ entity.TryAdd<impl::Timers>() };
	if (auto* existing{ FindTimerEntry(entity, key) }) {
		existing->config.duration = ClampTimerDuration(duration);
		existing->config.mode = mode;
		existing->config.start_automatically = start_automatically;
		existing->runtime = TimerRuntime{};
		InitializeTimerRuntime(*existing, true);
		return TimerHandle{ .owner = entity, .key = std::move(key) };
	}

	TimerEntry entry{
		.config = TimerConfig{
			.key = key,
			.duration = ClampTimerDuration(duration),
			.mode = mode,
			.start_automatically = start_automatically,
		},
	};
	InitializeTimerRuntime(entry, true);
	timers.timers.push_back(std::move(entry));
	return TimerHandle{ .owner = entity, .key = std::move(key) };
}

TimerHandle GetTimer(Entity entity, TimerKey key) {
	return HasTimer(entity, key)
		? TimerHandle{ .owner = entity, .key = std::move(key) }
		: TimerHandle{};
}

bool HasTimer(Entity entity, const TimerKey& key) {
	return FindTimerEntry(entity, key) != nullptr;
}

bool RemoveTimer(Entity entity, const TimerKey& key) {
	if (!entity) {
		return false;
	}

	auto* timers{ entity.TryGet<impl::Timers>() };
	if (!timers) {
		return false;
	}

	return std::erase_if(timers->timers, [&key](const TimerEntry& entry) {
		return entry.config.key == key;
	}) > 0;
}

namespace timer_runtime {

void Update(Scene& scene, secondsf delta_time) {
	const secondsf dt{ std::max(0.0f, delta_time.count()) };
	const auto entities{ scene.EntitiesWith<impl::Timers>().GetVector() };

	for (Entity entity : entities) {
		if (!entity) {
			continue;
		}

		auto* timers{ entity.TryGet<impl::Timers>() };
		if (!timers) {
			continue;
		}

		std::vector<TimerKey> keys;
		keys.reserve(timers->timers.size());
		for (const auto& entry : timers->timers) {
			keys.push_back(entry.config.key);
		}

		for (const auto& key : keys) {
			auto* entry{ FindTimerEntry(entity, key) };
			if (!entry) {
				continue;
			}

			InitializeTimerRuntime(*entry, true);
			if (!entry->runtime.timer.IsRunning()) {
				continue;
			}

			entry->runtime.timer.Update(dt);
			const millisecondsf duration{ ClampTimerDuration(entry->config.duration) };
			std::size_t fire_count{ 0 };

			if (entry->config.mode == TimerMode::Once) {
				if (duration <= millisecondsf{ 0.0f } || entry->runtime.timer.Completed(duration)) {
					entry->runtime.timer.Stop();
					entry->runtime.completed = true;
					fire_count = 1;
				}
			} else {
				entry->runtime.completed = false;
				if (duration <= millisecondsf{ 0.0f }) {
					entry->runtime.timer.Restart();
					fire_count = 1;
				} else {
					fire_count = entry->runtime.timer.ConsumeAll(duration);
				}
			}

			if (fire_count == 0) {
				continue;
			}

			std::uint64_t first_count{ entry->runtime.elapsed_count + 1 };
			entry->runtime.elapsed_count += static_cast<std::uint64_t>(fire_count);

			for (std::size_t index{ 0 }; index < fire_count; ++index) {
				script_runtime::Dispatch<event::TimerElapsed>(
					entity,
					event::TimerElapsed{
						.timer = key,
						.count = first_count + static_cast<std::uint64_t>(index),
					}
				);
			}
		}
	}
}

} // namespace timer_runtime

std::ostream& operator<<(std::ostream& os, const TimerConfig& timer) {
	return os << "{ key: " << timer.key.value << ", duration: " << timer.duration
			  << ", mode: " << (timer.mode == TimerMode::Repeat ? "repeat" : "once")
			  << ", auto_start: " << timer.start_automatically << " }";
}

std::ostream& operator<<(std::ostream& os, const impl::Timers& timers) {
	return os << "{ timer_count: " << timers.timers.size() << " }";
}

} // namespace ptgn
