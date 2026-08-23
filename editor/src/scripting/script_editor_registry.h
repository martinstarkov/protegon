#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <functional>
#include <optional>
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

class EditorContext;

namespace impl {

/// @brief Ensures the translation unit containing the built-in editor registrations is linked.
void EnsureEngineScriptEditorsRegistered();

} // namespace impl

struct EventEditorOptions {
	std::string label{};
	std::string group{};
	std::string description{};
	int inline_fields{ 0 };
	std::function<bool(json&)> draw{};
};

struct EventEditorRegistration {
	TypeHashValue type_hash{ 0 };
	EventEditorOptions options{};
};

class EventEditorRegistry {
public:
	template <typename TEvent>
	static bool Register() {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ Hash<TEvent>() };

		if (std::ranges::any_of(
				entries,
				[type_hash](const auto& entry) {
					return entry.type_hash == type_hash;
				}
			)) {
			return false;
		}

		return Register<TEvent>(EventEditorOptions{});
	}

	template <typename TEvent>
	static bool Register(EventEditorOptions options) {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ Hash<TEvent>() };
		const bool inserted{
			std::ranges::none_of(
				entries,
				[type_hash](const auto& entry) {
					return entry.type_hash == type_hash;
				}
			)
		};

		std::erase_if(
			entries,
			[type_hash](const auto& entry) {
				return entry.type_hash == type_hash;
			}
		);

		if (options.label.empty()) {
			options.label =
				std::string{
					type_name_without_namespaces<TEvent>()
				};
		}

		options.inline_fields =
			std::max(0, options.inline_fields);

		if (options.draw) {
			auto draw{ std::move(options.draw) };

			options.draw =
				[fn = std::move(draw)](json& value) mutable {
					if (value.is_null()) {
						value = json::object();
					}

					const json previous{ value };
					const bool changed{
						std::invoke(fn, value)
					};

					return changed || value != previous;
				};
		}

		entries.push_back(
			EventEditorRegistration{
				.type_hash = type_hash,
				.options = std::move(options),
			}
		);

		return inserted;
	}

	[[nodiscard]] static const EventEditorRegistration* Find(
		TypeHashValue type_hash
	);

	[[nodiscard]] static const std::vector<
		EventEditorRegistration
	>& Entries();

private:
	[[nodiscard]] static std::vector<
		EventEditorRegistration
	>& MutableEntries();
};

enum class ScriptType : std::uint8_t {
	None	 = 0,
	Resident = 1 << 0,
	Sequence = 1 << 1,
	Both	 = (1 << 0) | (1 << 1)
};

[[nodiscard]] constexpr ScriptType operator|(
	ScriptType lhs,
	ScriptType rhs
) {
	return static_cast<ScriptType>(
		static_cast<std::uint8_t>(lhs) |
		static_cast<std::uint8_t>(rhs)
	);
}

[[nodiscard]] constexpr ScriptType operator&(
	ScriptType lhs,
	ScriptType rhs
) {
	return static_cast<ScriptType>(
		static_cast<std::uint8_t>(lhs) &
		static_cast<std::uint8_t>(rhs)
	);
}

[[nodiscard]] constexpr bool HasScriptType(
	ScriptType value,
	ScriptType type
) {
	return (value & type) != ScriptType::None;
}

struct ScriptEditorContext {
	EditorContext& ctx;
	Entity owner{};
	SharedScriptSequenceRegistry& shared_sequences;
	const std::optional<EntityFilter>* sequence_target_filter{ nullptr };
};

template <ScriptClass T>
struct ScriptEditorOptions {
	std::string label{};
	std::string group{};
	std::string description{};
	ScriptType type{ ScriptType::Resident };
	int menu_order{ 100 };
	bool separator_after{ false };
	bool hidden{ false };
	std::function<bool(ScriptEditorContext&, T&)> draw_inline{};
	std::function<bool(ScriptEditorContext&, T&)> draw{};
};

struct RegisteredScriptEditorOptions {
	std::string label{};
	std::string group{};
	std::string description{};
	ScriptType type{ ScriptType::Resident };
	int menu_order{ 100 };
	bool separator_after{ false };
	bool hidden{ false };
};

struct ScriptEditorRegistration {
	TypeHashValue type_hash{ 0 };
	RegisteredScriptEditorOptions options{};
	bool has_contents{ false };
	std::function<bool(ScriptEditorContext&, json&)> draw_inline{};
	std::function<bool(ScriptEditorContext&, json&)> draw{};
};

template <typename T>
struct TypedJsonEditorState {
	T value{};
	json synchronized_value = json::object();
	bool initialized{ false };
};

template <typename T, typename F>
bool DrawTypedJsonEditor(
	ScriptEditorContext& context,
	json& input,
	F& draw
) {
	if constexpr (!std::default_initializable<T>) {
		return false;
	} else {
		T value{};

		if constexpr (
			requires(const json& json_value, T& typed_value) {
				json_value.get_to(typed_value);
			}
		) {
			(void)TryReadScriptJson(input, value);
		}

		bool changed{
			std::invoke(
				draw,
				context,
				value
			)
		};

		if constexpr (
			requires(json& json_value, const T& typed_value) {
				json_value = typed_value;
			}
		) {
			json updated = json::object();

			try {
				updated = value;
			} catch (...) {
				return changed;
			}

			bool serialized_changed{ updated != input };

			if (serialized_changed) {
				input = std::move(updated);
			}

			return changed || serialized_changed;
		}

		return changed;
	}
}

class ScriptEditorRegistry {
public:
	template <ScriptClass T>
	static bool Register() {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ Hash<T>() };

		if (std::ranges::any_of(
				entries,
				[type_hash](const auto& entry) {
					return entry.type_hash == type_hash;
				}
			)) {
			return false;
		}

		return Register<T>(
			ScriptEditorOptions<T>{}
		);
	}

	template <ScriptClass T>
	static bool Register(
		ScriptEditorOptions<T> options
	) {
		auto& entries{ MutableEntries() };
		const TypeHashValue type_hash{ Hash<T>() };
		const bool inserted{
			std::ranges::none_of(
				entries,
				[type_hash](const auto& entry) {
					return entry.type_hash == type_hash;
				}
			)
		};

		std::erase_if(
			entries,
			[type_hash](const auto& entry) {
				return entry.type_hash == type_hash;
			}
		);

		if (options.label.empty()) {
			options.label =
				std::string{
					type_name_without_namespaces<T>()
				};
		}

		ScriptEditorRegistration registration{
			.type_hash = type_hash,
			.options = {
				.label = std::move(options.label),
				.group = std::move(options.group),
				.description =
					std::move(options.description),
				.type = options.type,
				.menu_order = options.menu_order,
				.separator_after =
					options.separator_after,
				.hidden = options.hidden,
			},
			.has_contents =
				static_cast<bool>(options.draw),
		};

		if (options.draw_inline) {
			registration.draw_inline =
				[
					fn =
						std::move(
							options.draw_inline
						)
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
					fn =
						std::move(
							options.draw
						)
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

		entries.push_back(
			std::move(registration)
		);

		return inserted;
	}

	[[nodiscard]] static const ScriptEditorRegistration* Find(
		TypeHashValue type_hash
	);

	[[nodiscard]] static const std::vector<
		ScriptEditorRegistration
	>& Entries();

private:
	[[nodiscard]] static std::vector<
		ScriptEditorRegistration
	>& MutableEntries();
};

template <ScriptClass T>
bool RegisterScript(
	ScriptEditorOptions<T> options = {}
) {
	return ScriptEditorRegistry::Register<T>(
		std::move(options)
	);
}

} // namespace ptgn::editor
