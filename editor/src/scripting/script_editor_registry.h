#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/util/hash.h"
#include "core/util/type_info.h"
#include "runtime/ecs/entity.h"
#include "runtime/scripting/script.h"
#include "serialization/json/json.h"

namespace ptgn::editor {

namespace impl {

/// @brief Ensures the translation unit containing the built-in editor registrations is linked.
void EnsureEngineScriptEditorsRegistered();

} // namespace impl

struct EventEditorOptions {
	std::string label;
	std::string group;
	std::string description;
	int inline_fields{ 0 };
	std::function<bool(json&)> draw;
};

struct EventEditorRegistration {
	TypeHashValue type_hash{ 0 };
	EventEditorOptions options;
};

class EventEditorRegistry {
public:
	template <typename TEvent>
	static bool Register() {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ Hash<TEvent>() };
		if (std::ranges::any_of(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		})) {
			return false;
		}
		return Register<TEvent>(EventEditorOptions{});
	}

	template <typename TEvent>
	static bool Register(EventEditorOptions options) {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ Hash<TEvent>() };
		const bool inserted{ std::ranges::none_of(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		}) };

		std::erase_if(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		});

		if (options.label.empty()) {
			options.label = std::string{ type_name_without_namespaces<TEvent>() };
		}
		options.inline_fields = std::max(0, options.inline_fields);

		if (options.draw) {
			auto draw{ std::move(options.draw) };
			options.draw = [fn = std::move(draw)](json& value) mutable {
				if (value.is_null()) {
					value = json::object();
				}
				const json previous{ value };
				const bool changed{ std::invoke(fn, value) };
				return changed || value != previous;
			};
		}

		entries.push_back(EventEditorRegistration{
			.type_hash = type_hash,
			.options = std::move(options),
		});
		return inserted;
	}

	[[nodiscard]] static const EventEditorRegistration* Find(TypeHashValue type_hash);
	[[nodiscard]] static const std::vector<EventEditorRegistration>& Entries();

private:
	[[nodiscard]] static std::vector<EventEditorRegistration>& MutableEntries();
};

enum class ScriptType : std::uint8_t {
	None	 = 0,
	Resident = 1 << 0,
	Sequence = 1 << 1,
	Both	 = (1 << 0) | (1 << 1)
};

[[nodiscard]] constexpr ScriptType operator|(ScriptType lhs, ScriptType rhs) {
	return static_cast<ScriptType>(
		static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs)
	);
}

[[nodiscard]] constexpr ScriptType operator&(ScriptType lhs, ScriptType rhs) {
	return static_cast<ScriptType>(
		static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs)
	);
}

[[nodiscard]] constexpr bool HasScriptType(ScriptType value, ScriptType type) {
	return (value & type) != ScriptType::None;
}

struct ScriptEditorContext {
	Entity owner;
	SharedScriptSequenceRegistry& shared_sequences;
};

template <ScriptClass T>
struct ScriptEditorOptions {
	std::string label;
	std::string group;
	std::string description;
	ScriptType type{ ScriptType::Resident };
	int menu_order{ 100 };
	bool separator_after{ false };
	bool hidden{ false };
	std::function<bool(T&, ScriptEditorContext&)> draw_inline;
	std::function<bool(T&, ScriptEditorContext&)> draw;
};

struct RegisteredScriptEditorOptions {
	std::string label;
	std::string group;
	std::string description;
	ScriptType type{ ScriptType::Resident };
	int menu_order{ 100 };
	bool separator_after{ false };
	bool hidden{ false };
};

struct ScriptEditorRegistration {
	TypeHashValue type_hash{ 0 };
	RegisteredScriptEditorOptions options;
	bool has_contents{ false };
	std::function<bool(json&, ScriptEditorContext&)> draw_inline;
	std::function<bool(json&, ScriptEditorContext&)> draw;
};

template <typename T>
struct TypedJsonEditorState {
	T value{};
	json synchronized_value = json::object();
	bool initialized{ false };
};

template <typename T, typename F>
bool DrawTypedJsonEditor(json& input, ScriptEditorContext& context, F& fn) {
	static std::unordered_map<const json*, TypedJsonEditorState<T>> states;
	auto& state{ states[&input] };
	if (!state.initialized || state.synchronized_value != input) {
		state.value = T{};
		if constexpr (requires(const json& value, T& output) { value.get_to(output); }) {
			TryReadScriptJson(input, state.value);
		}
		state.synchronized_value = input;
		state.initialized = true;
	}

	const bool changed{ std::invoke(fn, state.value, context) };

	if constexpr (requires(json& output, const T& value) { output = value; }) {
		json updated;
		try {
			updated = state.value;
		} catch (...) {
			updated = input;
		}
		const bool serialized_changed{ updated != input };
		input = std::move(updated);
		state.synchronized_value = input;
		return changed || serialized_changed;
	} else {
		// Metadata-only Script registrations do not require a JSON adapter. A custom drawer for a
		// non-serializable Script may still run, but there is no payload to write back.
		state.synchronized_value = input;
		return changed;
	}
}

class ScriptEditorRegistry {
public:
	template <ScriptClass T>
	static bool Register() {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ Hash<T>() };
		if (std::ranges::any_of(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		})) {
			return false;
		}
		return Register<T>(ScriptEditorOptions<T>{});
	}

	template <ScriptClass T>
	static bool Register(ScriptEditorOptions<T> options) {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ Hash<T>() };
		const bool inserted{ std::ranges::none_of(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		}) };

		std::erase_if(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		});

		if (options.label.empty()) {
			options.label = std::string{ type_name_without_namespaces<T>() };
		}

		ScriptEditorRegistration registration{
			.type_hash = type_hash,
			.options = {
				.label = std::move(options.label),
				.group = std::move(options.group),
				.description = std::move(options.description),
				.type = options.type,
				.menu_order = options.menu_order,
				.separator_after = options.separator_after,
				.hidden = options.hidden,
			},
			.has_contents = static_cast<bool>(options.draw),
		};

		if (options.draw_inline) {
			registration.draw_inline =
				[fn = std::move(options.draw_inline)](
					json& input, ScriptEditorContext& context
				) mutable {
					return DrawTypedJsonEditor<T>(input, context, fn);
				};
		}
		if (options.draw) {
			registration.draw =
				[fn = std::move(options.draw)](
					json& input, ScriptEditorContext& context
				) mutable {
					return DrawTypedJsonEditor<T>(input, context, fn);
				};
		}

		entries.push_back(std::move(registration));
		return inserted;
	}

	[[nodiscard]] static const ScriptEditorRegistration* Find(TypeHashValue type_hash);
	[[nodiscard]] static const std::vector<ScriptEditorRegistration>& Entries();

private:
	[[nodiscard]] static std::vector<ScriptEditorRegistration>& MutableEntries();
};

} // namespace ptgn::editor
