#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/util/hash.h"
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
};

struct EventEditorRegistrationDefinition {
	EventEditorOptions options;
	int inline_fields{ 0 };
	std::function<bool(json&)> draw;
};

struct EventEditorRegistration {
	TypeHashValue type_hash{ 0 };
	EventEditorOptions options;
	int inline_fields{ 0 };
	std::function<bool(json&)> draw;
};

class EventEditorRegistry {
public:
	template <typename TEvent>
	static bool Register(EventEditorRegistrationDefinition registration) {
		return Register<TEvent>(
			std::move(registration.options), registration.inline_fields,
			std::move(registration.draw)
		);
	}

	template <typename TEvent, typename F>
	static bool Register(EventEditorOptions options, int inline_fields, F&& draw) {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ Hash<TEvent>() };
		const bool inserted{ std::ranges::none_of(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		}) };
		std::erase_if(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		});
		entries.push_back(EventEditorRegistration{
			.type_hash = type_hash,
			.options = std::move(options),
			.inline_fields = std::max(0, inline_fields),
			.draw = [fn = std::forward<F>(draw)](json& value) mutable {
				if (value.is_null()) {
					value = json::object();
				}
				const json previous{ value };
				const bool changed{ std::invoke(fn, value) };
				return changed || value != previous;
			},
		});
		return inserted;
	}

	[[nodiscard]] static const EventEditorRegistration* Find(TypeHashValue type_hash);
	[[nodiscard]] static const std::vector<EventEditorRegistration>& Entries();

private:
	[[nodiscard]] static std::vector<EventEditorRegistration>& MutableEntries();
};

struct SequenceStepEditorOptions {
	std::string label;
	std::string group;
	std::string description;
	int menu_order{ 100 };
	bool separator_after{ false };
};

struct ScriptEditorContext {
	Entity owner;
	SharedScriptSequenceRegistry& shared_sequences;
};

struct SequenceStepEditorRegistration {
	TypeHashValue type_hash{ 0 };
	SequenceStepEditorOptions options;
	std::function<bool(json&, ScriptEditorContext&)> draw_inline;
	std::function<bool(json&, ScriptEditorContext&)> draw;
};

template <typename T>
struct TypedJsonEditorState {
	T value{};
	json synchronized_value;
	bool initialized{ false };
};

template <typename T, typename F>
bool DrawTypedJsonEditor(json& input, ScriptEditorContext& context, F& fn) {
	static std::unordered_map<const json*, TypedJsonEditorState<T>> states;
	auto& state{ states[&input] };
	if (!state.initialized || state.synchronized_value != input) {
		state.value = T{};
		TryReadScriptJson(input, state.value);
		state.synchronized_value = input;
		state.initialized = true;
	}

	const bool changed{ std::invoke(fn, state.value, context) };
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
}

class SequenceStepEditorRegistry {
public:
	template <ScriptClass T, typename F>
	static bool Register(SequenceStepEditorOptions options, F&& draw) {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ Hash<T>() };
		const bool inserted{ std::ranges::none_of(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		}) };
		std::erase_if(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		});
		entries.push_back(SequenceStepEditorRegistration{
			.type_hash = type_hash,
			.options = std::move(options),
			.draw_inline = {},
			.draw = [fn = std::forward<F>(draw)](
				json& input, ScriptEditorContext& context
			) mutable {
				return DrawTypedJsonEditor<T>(input, context, fn);
			},
		});
		return inserted;
	}

	template <ScriptClass T, typename FInline, typename FDetails>
	static bool RegisterInline(
		SequenceStepEditorOptions options, FInline&& draw_inline, FDetails&& draw_details
	) {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ Hash<T>() };
		const bool inserted{ std::ranges::none_of(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		}) };
		std::erase_if(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		});
		entries.push_back(SequenceStepEditorRegistration{
			.type_hash = type_hash,
			.options = std::move(options),
			.draw_inline = [fn = std::forward<FInline>(draw_inline)](
				json& input, ScriptEditorContext& context
			) mutable {
				return DrawTypedJsonEditor<T>(input, context, fn);
			},
			.draw = [fn = std::forward<FDetails>(draw_details)](
				json& input, ScriptEditorContext& context
			) mutable {
				return DrawTypedJsonEditor<T>(input, context, fn);
			},
		});
		return inserted;
	}

	[[nodiscard]] static const SequenceStepEditorRegistration* Find(TypeHashValue type_hash);
	[[nodiscard]] static const std::vector<SequenceStepEditorRegistration>& Entries();

private:
	[[nodiscard]] static std::vector<SequenceStepEditorRegistration>& MutableEntries();
};

struct ScriptEditorOptions {
	std::string label;
	std::string group;
	std::string description;
	bool hidden{ false };
};

struct ScriptEditorRegistration {
	TypeHashValue type_hash{ 0 };
	ScriptEditorOptions options;
	bool has_contents{ false };
	std::function<bool(json&, ScriptEditorContext&)> draw;
};

class ScriptEditorRegistry {
public:
	template <ScriptClass T, typename F>
	static bool Register(ScriptEditorOptions options, F&& draw) {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ Hash<T>() };
		const bool inserted{ std::ranges::none_of(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		}) };
		std::erase_if(entries, [type_hash](const auto& entry) {
			return entry.type_hash == type_hash;
		});
		entries.push_back(ScriptEditorRegistration{
			.type_hash = type_hash,
			.options = std::move(options),
			.has_contents = !std::is_empty_v<T>,
			.draw = [fn = std::forward<F>(draw)](
				json& input, ScriptEditorContext& context
			) mutable {
				return DrawTypedJsonEditor<T>(input, context, fn);
			},
		});
		return inserted;
	}

	[[nodiscard]] static const ScriptEditorRegistration* Find(TypeHashValue type_hash);
	[[nodiscard]] static const std::vector<ScriptEditorRegistration>& Entries();

private:
	[[nodiscard]] static std::vector<ScriptEditorRegistration>& MutableEntries();
};

template <typename F>
struct SequenceStepEditorDefinition {
	SequenceStepEditorOptions options;
	F draw;
};

template <typename FInline, typename FDetails>
struct InlineSequenceStepEditorDefinition {
	SequenceStepEditorOptions options;
	FInline draw_inline;
	FDetails draw_details;
};

template <typename F>
[[nodiscard]] auto SequenceStepEditor(SequenceStepEditorOptions options, F&& draw) {
	return SequenceStepEditorDefinition<std::decay_t<F>>{
		.options = std::move(options),
		.draw = std::forward<F>(draw),
	};
}

template <typename FInline, typename FDetails>
[[nodiscard]] auto SequenceStepEditor(
	SequenceStepEditorOptions options, FInline&& draw_inline, FDetails&& draw_details
) {
	return InlineSequenceStepEditorDefinition<
		std::decay_t<FInline>, std::decay_t<FDetails>
	>{
		.options = std::move(options),
		.draw_inline = std::forward<FInline>(draw_inline),
		.draw_details = std::forward<FDetails>(draw_details),
	};
}

template <typename F>
struct RootScriptEditorDefinition {
	ScriptEditorOptions options;
	F draw;
};

template <typename F>
[[nodiscard]] auto RootScriptEditor(ScriptEditorOptions options, F&& draw) {
	return RootScriptEditorDefinition<std::decay_t<F>>{
		.options = std::move(options),
		.draw = std::forward<F>(draw),
	};
}

namespace impl {

template <ScriptClass T, typename F>
bool RegisterScriptEditorDefinition(SequenceStepEditorDefinition<F> definition) {
	return SequenceStepEditorRegistry::Register<T>(
		std::move(definition.options), std::move(definition.draw)
	);
}

template <ScriptClass T, typename FInline, typename FDetails>
bool RegisterScriptEditorDefinition(
	InlineSequenceStepEditorDefinition<FInline, FDetails> definition
) {
	return SequenceStepEditorRegistry::RegisterInline<T>(
		std::move(definition.options), std::move(definition.draw_inline),
		std::move(definition.draw_details)
	);
}

template <ScriptClass T, typename F>
bool RegisterScriptEditorDefinition(RootScriptEditorDefinition<F> definition) {
	return ScriptEditorRegistry::Register<T>(
		std::move(definition.options), std::move(definition.draw)
	);
}

} // namespace impl

template <ScriptClass T, typename... TDefinition>
bool RegisterScriptEditors(TDefinition&&... definition) {
	bool inserted{ false };
	((inserted = impl::RegisterScriptEditorDefinition<T>(
		  std::forward<TDefinition>(definition)
	  ) || inserted),
	 ...);
	return inserted;
}

} // namespace ptgn::editor
