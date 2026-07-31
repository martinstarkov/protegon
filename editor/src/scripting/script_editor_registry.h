#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/util/hash.h"
#include "core/util/type_info.h"
#include "panels/inspector_fields.h"
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
	None = 0,
	Resident = 1 << 0,
	Sequence = 1 << 1,
	Both = (1 << 0) | (1 << 1)
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
	// Runtime registration options.
	ScriptCompletion completion{ ScriptCompletion::ScriptControlled };
	bool supports_timing{ false };
	bool requires_timing{ false };
	bool serializable{ true };
	std::optional<ScriptTiming> default_timing;

	// Editor registration options.
	std::string label;
	std::string group;
	std::string description;
	ScriptType type{ ScriptType::Resident };
	int menu_order{ 100 };
	bool separator_after{ false };
	bool hidden{ false };
	std::function<bool(ScriptEditorContext&, T&)> draw_inline;
	std::function<bool(ScriptEditorContext&, T&)> draw;

	[[nodiscard]] ScriptRegistrationOptions RuntimeOptions() const {
		return ScriptRegistrationOptions{
			.completion = completion,
			.supports_timing = supports_timing,
			.requires_timing = requires_timing,
			.serializable = serializable,
			.default_timing = default_timing,
		};
	}

	/// @return Whether this registration contains non-default runtime metadata.
	[[nodiscard]] bool HasRuntimeOptions() const {
		return completion != ScriptCompletion::ScriptControlled ||
			supports_timing ||
			requires_timing ||
			!serializable ||
			default_timing.has_value();
	}
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

/// The default script editor is only available when every reflected member
/// has a supported inspector drawer. This prevents broad reflected types such
/// as Script from instantiating drawers for raw json members.
template <typename T>
concept ReflectedScriptInspectorDrawable =
	inspector::kHasDefaultInspectorDrawer<T>;

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

	const bool changed{ std::invoke(draw, context, value) };

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

		// Supply the standard reflected inspector only when no custom editor
		// was registered. No component-editor registry participates here.
		if (!options.draw && !options.draw_inline) {
			if constexpr (
				TypedScriptJsonEditable<T> &&
				ReflectedScriptInspectorDrawable<T>
			) {
				options.draw = [](ScriptEditorContext& context, T& script) {
					return inspector::DrawComponentContents(context.ctx, script);
				};
			}
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

		if (options.draw_inline) {
			registration.draw_inline = [fn = std::move(options.draw_inline)](
				ScriptEditorContext& context,
				json& input
			) mutable {
				return DrawTypedJsonEditor<T>(context, input, fn);
			};
		}

		if (options.draw) {
			registration.draw = [fn = std::move(options.draw)](
				ScriptEditorContext& context,
				json& input
			) mutable {
				return DrawTypedJsonEditor<T>(context, input, fn);
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

template <ScriptClass T>
bool RegisterScript(ScriptEditorOptions<T> options) {
	const bool runtime_registered{
		options.HasRuntimeOptions()
			? ScriptRegistry::Register<T>(options.RuntimeOptions())
			: ScriptRegistry::Register<T>()
	};

	const bool editor_registered{
		ScriptEditorRegistry::Register<T>(std::move(options))
	};

	return runtime_registered || editor_registered;
}

} // namespace ptgn::editor
