#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <string_view>

#include "core/util/hash.h"
#include "core/util/type_info.h"
#include "runtime/ecs/entity.h"
#include "runtime/scripting/script.h"
#include "serialization/json/json.h"

namespace ptgn::editor {

class EditorContext;

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
	std::string name;
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
				const json previous = value;
				const bool changed{ std::invoke(fn, value) };
				return changed || value != previous;
			};
		}

		entries.push_back(EventEditorRegistration{
			.type_hash = type_hash,
			.name = std::string{ type_name_without_namespaces<TEvent>() },
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
	EditorContext& ctx;
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
	std::function<bool(ScriptEditorContext&, T&)> draw_inline;
	std::function<bool(ScriptEditorContext&, T&)> draw;
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
	std::string name;
	RegisteredScriptEditorOptions options;
	bool has_contents{ false };
	std::function<bool(ScriptEditorContext&, json&)> draw_inline;
	std::function<bool(ScriptEditorContext&, json&)> draw;
};

template <typename T>
concept TypedScriptJsonEditable =
	std::default_initializable<T> &&
	JsonSerializable<T> &&
	JsonDeserializable<T>;

template <typename T, typename F>
	requires TypedScriptJsonEditable<T>
bool DrawTypedJsonEditor(
	ScriptEditorContext& context,
	json& input,
	F& draw
) {
	T value{};

	json normalized = value;

	try {
		if (normalized.is_object() && input.is_object()) {
			normalized.update(input, true);
		} else if (!input.is_null()) {
			normalized = input;
		}

		normalized.get_to(value);
	} catch (...) {
		value = T{};
	}

	const bool changed{
		std::invoke(
			draw,
			context,
			value
		)
	};

	if (!changed) {
		return false;
	}

	json updated = value;

	if (updated == input) {
		return false;
	}

	input = std::move(updated);

	return true;
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
			.name = std::string{ type_name_without_namespaces<T>() },
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

		if constexpr (TypedScriptJsonEditable<T>) {
			if (options.draw_inline) {
				registration.draw_inline =
					[
						fn = std::move(options.draw_inline)
					](
						ScriptEditorContext& context,
						json& input
					) mutable {
						return DrawTypedJsonEditor<T>(
							context,
							input,
							fn
						);
					};
			}

			if (options.draw) {
				registration.draw =
					[
						fn = std::move(options.draw)
					](
						ScriptEditorContext& context,
						json& input
					) mutable {
						return DrawTypedJsonEditor<T>(
							context,
							input,
							fn
						);
					};
			}
		} else {
			PTGN_ASSERT(
				!options.draw_inline && !options.draw,
				"Script has a typed editor but does not support JSON parameter serialization: ",
				type_name_without_namespaces<T>()
			);
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
