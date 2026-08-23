#pragma once

#include <cstdint>
#include <ostream>
#include <utility>
#include <vector>

#include "core/util/strong_string.h"
#include "core/util/timer.h"
#include "runtime/ecs/entity.h"
#include "serialization/json/json.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;

struct TimerKey : StrongString<TimerKey> {
	using StrongString::StrongString;
	
	constexpr TimerKey() = default;

	PTGN_REFLECT_VALUE(TimerKey, value)
};

enum class TimerMode : std::uint8_t {
	Once,
	Repeat
};
PTGN_REFLECT_ENUM(TimerMode);

struct TimerConfig {
	TimerKey key{ "Timer" };
	millisecondsf duration{ 1000.0f };
	TimerMode mode{ TimerMode::Once };
	bool start_automatically{ true };

	constexpr bool operator==(const TimerConfig&) const = default;

	PTGN_REFLECT(TimerConfig, key, duration, mode, start_automatically)
};

struct TimerRuntime {
	ManualTimer timer{};
	std::uint64_t elapsed_count{ 0 };
	bool completed{ false };
	bool initialized{ false };

	constexpr bool operator==(const TimerRuntime&) const = default;

	PTGN_REFLECT_READONLY(TimerRuntime, timer, elapsed_count, completed, initialized)
};

struct TimerEntry {
	TimerConfig config{};
	TimerRuntime runtime{};

	constexpr bool operator==(const TimerEntry&) const = default;

	PTGN_REFLECT(TimerEntry, config)
	PTGN_REFLECT_READONLY(TimerEntry, runtime)
};

namespace impl {

struct Timers {
	std::vector<TimerEntry> timers{};

	bool operator==(const Timers&) const = default;

	PTGN_REFLECT_VALUE(Timers, timers)
};

void to_json(json& output, const Timers& timers);
void from_json(const json& input, Timers& timers);

} // namespace impl

struct TimerHandle {
	Entity owner{};
	TimerKey key{};

	[[nodiscard]] explicit operator bool() const;

	bool Start() const;
	bool Restart() const;
	bool Stop() const;
	bool Reset() const;
	bool Pause() const;
	bool Resume() const;
	bool TogglePaused() const;

	bool Advance(millisecondsf amount) const;
	bool Rewind(millisecondsf amount) const;
	bool SetDuration(millisecondsf duration) const;
	bool AddDuration(millisecondsf amount) const;
	bool RemoveDuration(millisecondsf amount) const;

	template <DurationType D>
	bool Advance(D amount) const {
		return Advance(duration_cast<millisecondsf>(amount));
	}

	template <DurationType D>
	bool Rewind(D amount) const {
		return Rewind(duration_cast<millisecondsf>(amount));
	}

	template <DurationType D>
	bool SetDuration(D duration) const {
		return SetDuration(duration_cast<millisecondsf>(duration));
	}

	template <DurationType D>
	bool AddDuration(D amount) const {
		return AddDuration(duration_cast<millisecondsf>(amount));
	}

	template <DurationType D>
	bool RemoveDuration(D amount) const {
		return RemoveDuration(duration_cast<millisecondsf>(amount));
	}

	[[nodiscard]] millisecondsf Elapsed() const;
	[[nodiscard]] millisecondsf Remaining() const;
	[[nodiscard]] millisecondsf Duration() const;

	template <DurationType D>
	[[nodiscard]] D Elapsed() const {
		return duration_cast<D>(Elapsed());
	}

	template <DurationType D>
	[[nodiscard]] D Remaining() const {
		return duration_cast<D>(Remaining());
	}

	template <DurationType D>
	[[nodiscard]] D Duration() const {
		return duration_cast<D>(Duration());
	}

	[[nodiscard]] float Progress() const;
	[[nodiscard]] bool IsRunning() const;
	[[nodiscard]] bool IsPaused() const;
	[[nodiscard]] bool IsCompleted() const;
	[[nodiscard]] std::uint64_t ElapsedCount() const;
};

TimerHandle AddTimer(
	Entity entity,
	TimerKey key,
	millisecondsf duration,
	TimerMode mode = TimerMode::Once,
	bool start_automatically = true
);

template <DurationType D>
TimerHandle AddTimer(
	Entity entity,
	TimerKey key,
	D duration,
	TimerMode mode = TimerMode::Once,
	bool start_automatically = true
) {
	return AddTimer(
		entity,
		std::move(key),
		duration_cast<millisecondsf>(duration),
		mode,
		start_automatically
	);
}

[[nodiscard]] TimerHandle GetTimer(Entity entity, TimerKey key);
[[nodiscard]] bool HasTimer(Entity entity, const TimerKey& key);
bool RemoveTimer(Entity entity, const TimerKey& key);

namespace timer_runtime {

void Update(Scene& scene, secondsf delta_time);

} // namespace timer_runtime

std::ostream& operator<<(std::ostream& os, const TimerConfig& timer);
std::ostream& operator<<(std::ostream& os, const impl::Timers& timers);

} // namespace ptgn
