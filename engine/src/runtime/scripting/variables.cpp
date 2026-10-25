#include "runtime/scripting/variables.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <ranges>
#include <utility>

#include "runtime/scene/scene_event.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/variable_event.h"

namespace ptgn {

namespace {

void NormalizeBounds(CounterConfig& config) {
	if (config.minimum && config.maximum && *config.minimum > *config.maximum) {
		std::swap(config.minimum, config.maximum);
	}
}

[[nodiscard]] std::int64_t ClampCounter(std::int64_t value, const CounterConfig& config) {
	if (config.minimum) {
		value = std::max(value, *config.minimum);
	}

	if (config.maximum) {
		value = std::min(value, *config.maximum);
	}

	return value;
}

[[nodiscard]] std::int64_t SaturatingAdd(std::int64_t left, std::int64_t right) {
	constexpr auto maximum{ std::numeric_limits<std::int64_t>::max() };
	constexpr auto minimum{ std::numeric_limits<std::int64_t>::min() };

	if (right > 0 && left > maximum - right) {
		return maximum;
	}

	if (right < 0 && left < minimum - right) {
		return minimum;
	}

	return left + right;
}

[[nodiscard]] CounterEntry* FindCounter(Entity entity, const CounterKey& key) {
	if (!entity || key.value.empty()) {
		return nullptr;
	}

	auto* component{ entity.TryGet<impl::Counters>() };

	if (!component) {
		return nullptr;
	}

	auto it{ std::ranges::find_if(component->counters, [&key](const CounterEntry& entry) {
		return entry.config.key == key;
	}) };

	return it == component->counters.end() ? nullptr : std::addressof(*it);
}

[[nodiscard]] BooleanEntry* FindBoolean(Entity entity, const BooleanKey& key) {
	if (!entity || key.value.empty()) {
		return nullptr;
	}

	auto* component{ entity.TryGet<impl::Booleans>() };

	if (!component) {
		return nullptr;
	}

	auto it{ std::ranges::find_if(component->booleans, [&key](const BooleanEntry& entry) {
		return entry.config.key == key;
	}) };

	return it == component->booleans.end() ? nullptr : std::addressof(*it);
}

} // namespace

namespace impl {

void to_json(json& output, const Counters& component) {
	output			   = json::object();
	output["counters"] = json::array();

	for (const auto& entry : component.counters) {
		output["counters"].push_back(entry.config);
	}
}

void from_json(const json& input, Counters& component) {
	component.counters.clear();
	const json* entries{ nullptr };

	if (input.is_array()) {
		entries = std::addressof(input);
	} else if (input.is_object()) {
		auto it{ input.find("counters") };

		if (it != input.end() && it->is_array()) {
			entries = std::addressof(*it);
		}
	}

	if (!entries) {
		return;
	}

	for (const auto& value : *entries) {
		CounterEntry entry;

		try {
			value.get_to(entry.config);
		} catch (...) {
			continue;
		}

		NormalizeBounds(entry.config);
		entry.value = ClampCounter(entry.config.initial_value, entry.config);

		if (entry.config.key.value.empty() ||
			std::ranges::any_of(component.counters, [&entry](const CounterEntry& existing) {
				return existing.config.key == entry.config.key;
			})) {
			continue;
		}

		component.counters.push_back(std::move(entry));
	}
}

void to_json(json& output, const Booleans& component) {
	output			   = json::object();
	output["booleans"] = json::array();

	for (const auto& entry : component.booleans) {
		output["booleans"].push_back(entry.config);
	}
}

void from_json(const json& input, Booleans& component) {
	component.booleans.clear();
	const json* entries{ nullptr };

	if (input.is_array()) {
		entries = std::addressof(input);
	} else if (input.is_object()) {
		auto it{ input.find("booleans") };

		if (it != input.end() && it->is_array()) {
			entries = std::addressof(*it);
		}
	}

	if (!entries) {
		return;
	}

	for (const auto& value : *entries) {
		BooleanEntry entry;

		try {
			value.get_to(entry.config);
		} catch (...) {
			continue;
		}

		entry.value = entry.config.initial_value;

		if (entry.config.key.value.empty() ||
			std::ranges::any_of(component.booleans, [&entry](const BooleanEntry& existing) {
				return existing.config.key == entry.config.key;
			})) {
			continue;
		}

		component.booleans.push_back(std::move(entry));
	}
}

} // namespace impl

CounterHandle::operator bool() const {
	return FindCounter(owner, key) != nullptr;
}

std::int64_t CounterHandle::Value() const {
	auto* entry{ FindCounter(owner, key) };

	return entry ? entry->value : 0;
}

std::int64_t CounterHandle::InitialValue() const {
	auto* entry{ FindCounter(owner, key) };

	return entry ? entry->config.initial_value : 0;
}

std::optional<std::int64_t> CounterHandle::Minimum() const {
	auto* entry{ FindCounter(owner, key) };

	return entry ? entry->config.minimum : std::nullopt;
}

std::optional<std::int64_t> CounterHandle::Maximum() const {
	auto* entry{ FindCounter(owner, key) };

	return entry ? entry->config.maximum : std::nullopt;
}

bool CounterHandle::Set(std::int64_t requested) const {
	auto* entry{ FindCounter(owner, key) };

	if (!entry) {
		return false;
	}

	std::int64_t previous{ entry->value };
	std::int64_t next{ ClampCounter(requested, entry->config) };

	if (next == previous) {
		return false;
	}

	entry->value = next;

	PushEvent<event::CounterChanged>(
		owner, event::CounterChanged{ .counter = key, .previous_value = previous, .value = next }
	);
	return true;
}

bool CounterHandle::Add(std::int64_t amount) const {
	return *this && Set(SaturatingAdd(Value(), amount));
}

bool CounterHandle::Subtract(std::int64_t amount) const {
	// Do not negate INT64_MIN. Saturation must work across the entire signed range.
	if (!*this) {
		return false;
	}

	if (amount == std::numeric_limits<std::int64_t>::min()) {
		std::int64_t current{ Value() };

		return Set(
			current >= 0
				? std::numeric_limits<std::int64_t>::max()
				: SaturatingAdd(SaturatingAdd(current, std::numeric_limits<std::int64_t>::max()), 1)
		);
	}

	return Add(-amount);
}

bool CounterHandle::Reset() const {
	return *this && Set(InitialValue());
}

bool CounterHandle::SetBounds(
	std::optional<std::int64_t> minimum, std::optional<std::int64_t> maximum
) const {
	auto* entry{ FindCounter(owner, key) };

	if (!entry) {
		return false;
	}

	CounterConfig new_config{ entry->config };
	new_config.minimum = minimum;
	new_config.maximum = maximum;

	NormalizeBounds(new_config);

	if (new_config == entry->config) {
		return false;
	}

	entry->config = std::move(new_config);
	Set(entry->value);

	return true;
}

BooleanHandle::operator bool() const {
	return FindBoolean(owner, key) != nullptr;
}

bool BooleanHandle::Value() const {
	auto* entry{ FindBoolean(owner, key) };

	return entry && entry->value;
}

bool BooleanHandle::InitialValue() const {
	auto* entry{ FindBoolean(owner, key) };

	return entry && entry->config.initial_value;
}

bool BooleanHandle::Set(bool value) const {
	auto* entry{ FindBoolean(owner, key) };

	if (!entry || entry->value == value) {
		return false;
	}

	bool previous{ entry->value };

	entry->value = value;

	PushEvent<event::BooleanChanged>(
		owner, event::BooleanChanged{ .boolean = key, .previous_value = previous, .value = value }
	);

	return true;
}

bool BooleanHandle::Toggle() const {
	return *this && Set(!Value());
}

bool BooleanHandle::Reset() const {
	return *this && Set(InitialValue());
}

CounterHandle AddCounter(
	Entity entity, CounterKey key, std::int64_t initial_value, std::optional<std::int64_t> minimum,
	std::optional<std::int64_t> maximum
) {
	if (!entity || key.value.empty() || HasCounter(entity, key)) {
		return {};
	}

	auto* component{ entity.TryGet<impl::Counters>() };

	if (!component) {
		entity.Add<impl::Counters>();
		component = entity.TryGet<impl::Counters>();
	}

	CounterEntry entry;
	entry.config = CounterConfig{
		.key = key, .initial_value = initial_value, .minimum = minimum, .maximum = maximum
	};

	NormalizeBounds(entry.config);

	entry.value = ClampCounter(entry.config.initial_value, entry.config);
	component->counters.push_back(std::move(entry));

	return CounterHandle{ .owner = entity, .key = std::move(key) };
}

CounterHandle GetCounter(Entity entity, CounterKey key) {
	return HasCounter(entity, key) ? CounterHandle{ .owner = entity, .key = std::move(key) }
								   : CounterHandle{};
}

bool HasCounter(Entity entity, const CounterKey& key) {
	return FindCounter(entity, key) != nullptr;
}

bool RemoveCounter(Entity entity, const CounterKey& key) {
	auto* component{ entity ? entity.TryGet<impl::Counters>() : nullptr };

	return component && std::erase_if(component->counters, [&key](const CounterEntry& entry) {
							return entry.config.key == key;
						}) > 0;
}

BooleanHandle AddBoolean(Entity entity, BooleanKey key, bool initial_value) {
	if (!entity || key.value.empty() || HasBoolean(entity, key)) {
		return {};
	}

	auto* component{ entity.TryGet<impl::Booleans>() };

	if (!component) {
		entity.Add<impl::Booleans>();
		component = entity.TryGet<impl::Booleans>();
	}

	component->booleans.push_back(
		BooleanEntry{ .config = BooleanConfig{ .key = key, .initial_value = initial_value },
					  .value  = initial_value }
	);

	return BooleanHandle{ .owner = entity, .key = std::move(key) };
}

BooleanHandle GetBoolean(Entity entity, BooleanKey key) {
	return HasBoolean(entity, key) ? BooleanHandle{ .owner = entity, .key = std::move(key) }
								   : BooleanHandle{};
}

bool HasBoolean(Entity entity, const BooleanKey& key) {
	return FindBoolean(entity, key) != nullptr;
}

bool RemoveBoolean(Entity entity, const BooleanKey& key) {
	auto* component{ entity ? entity.TryGet<impl::Booleans>() : nullptr };

	return component && std::erase_if(component->booleans, [&key](const BooleanEntry& entry) {
							return entry.config.key == key;
						}) > 0;
}

} // namespace ptgn
