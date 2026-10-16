#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "core/util/strong_string.h"
#include "runtime/ecs/entity.h"
#include "serialization/json/json.h"
#include "serialization/serialize.h"

namespace ptgn {

struct CounterKey : StrongString<CounterKey> {
	using StrongString::StrongString;
	constexpr CounterKey() = default;
	PTGN_REFLECT_VALUE(CounterKey, value)
};

struct BooleanKey : StrongString<BooleanKey> {
	using StrongString::StrongString;
	constexpr BooleanKey() = default;
	PTGN_REFLECT_VALUE(BooleanKey, value)
};

struct CounterConfig {
	CounterKey key{ "Counter" };
	std::int64_t initial_value{ 0 };
	std::optional<std::int64_t> minimum{};
	std::optional<std::int64_t> maximum{};

	bool operator==(const CounterConfig&) const = default;
	PTGN_REFLECT(CounterConfig, key, initial_value, minimum, maximum)
};

struct CounterEntry {
	CounterConfig config{};
	std::int64_t value{ 0 };

	bool operator==(const CounterEntry&) const = default;
	PTGN_REFLECT(CounterEntry, config)
	PTGN_REFLECT_READONLY(CounterEntry, value)
};

struct BooleanConfig {
	BooleanKey key{ "Boolean" };
	bool initial_value{ false };

	bool operator==(const BooleanConfig&) const = default;
	PTGN_REFLECT(BooleanConfig, key, initial_value)
};

struct BooleanEntry {
	BooleanConfig config{};
	bool value{ false };

	bool operator==(const BooleanEntry&) const = default;
	PTGN_REFLECT(BooleanEntry, config)
	PTGN_REFLECT_READONLY(BooleanEntry, value)
};

namespace impl {

struct Counters {
	std::vector<CounterEntry> counters{};
	bool operator==(const Counters&) const = default;
	PTGN_REFLECT_VALUE(Counters, counters)
};

struct Booleans {
	std::vector<BooleanEntry> booleans{};
	bool operator==(const Booleans&) const = default;
	PTGN_REFLECT_VALUE(Booleans, booleans)
};

void to_json(json& output, const Counters& counters);
void from_json(const json& input, Counters& counters);
void to_json(json& output, const Booleans& booleans);
void from_json(const json& input, Booleans& booleans);

} // namespace impl

struct CounterHandle {
	Entity owner{};
	CounterKey key{};

	[[nodiscard]] explicit operator bool() const;
	[[nodiscard]] std::int64_t Value() const;
	[[nodiscard]] std::int64_t InitialValue() const;
	[[nodiscard]] std::optional<std::int64_t> Minimum() const;
	[[nodiscard]] std::optional<std::int64_t> Maximum() const;

	// Returns true only if the effective value changes. Bounds are inclusive.
	bool Set(std::int64_t value) const;
	bool Add(std::int64_t amount = 1) const;
	bool Subtract(std::int64_t amount = 1) const;
	bool Reset() const;
	bool SetBounds(std::optional<std::int64_t> minimum, std::optional<std::int64_t> maximum) const;
};

struct BooleanHandle {
	Entity owner{};
	BooleanKey key{};

	[[nodiscard]] explicit operator bool() const;
	[[nodiscard]] bool Value() const;
	[[nodiscard]] bool InitialValue() const;

	// Returns true only if the effective value changes.
	bool Set(bool value) const;
	bool Toggle() const;
	bool Reset() const;
};

CounterHandle AddCounter(
	Entity entity, CounterKey key, std::int64_t initial_value = 0,
	std::optional<std::int64_t> minimum = {}, std::optional<std::int64_t> maximum = {}
);
[[nodiscard]] CounterHandle GetCounter(Entity entity, CounterKey key);
[[nodiscard]] bool HasCounter(Entity entity, const CounterKey& key);
bool RemoveCounter(Entity entity, const CounterKey& key);

BooleanHandle AddBoolean(Entity entity, BooleanKey key, bool initial_value = false);
[[nodiscard]] BooleanHandle GetBoolean(Entity entity, BooleanKey key);
[[nodiscard]] bool HasBoolean(Entity entity, const BooleanKey& key);
bool RemoveBoolean(Entity entity, const BooleanKey& key);

} // namespace ptgn
