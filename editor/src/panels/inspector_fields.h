#pragma once

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/graphics/color.h"
#include "core/math/angle.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "core/util/type_info.h"
#include "panels/content_browser.h"
#include "platform/platform.h"
#include "renderer/text/font_style.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/text/font_system.h"

namespace ptgn::editor {

namespace inspector {

inline constexpr float kDefaultLabelWidth{ 180.0f };
inline constexpr float kLabelValueSpacing{ 12.0f };

struct AutoLabelWidthData {
	float start_x{ 0.0f };
	float width{ kDefaultLabelWidth };
	float measured_width{ kDefaultLabelWidth };
};

inline std::unordered_map<ImGuiID, AutoLabelWidthData>& AutoLabelWidths() {
	static std::unordered_map<ImGuiID, AutoLabelWidthData> widths;
	return widths;
}

inline std::vector<AutoLabelWidthData*>& AutoLabelWidthStack() {
	static std::vector<AutoLabelWidthData*> stack;
	return stack;
}

inline float GetPropertyLabelWidth() {
	auto& stack{ AutoLabelWidthStack() };

	if (stack.empty()) {
		return kDefaultLabelWidth;
	}

	return stack.back()->width;
}

inline void MeasurePropertyLabel(std::string_view label, float label_x) {
	auto& stack{ AutoLabelWidthStack() };

	if (stack.empty()) {
		return;
	}

	auto& data{ *stack.back() };

	auto label_offset{ std::max(0.0f, label_x - data.start_x) };
	auto text_width{ ImGui::CalcTextSize(label.data(), label.data() + label.size()).x };
	auto width{ label_offset + text_width + ImGui::GetStyle().FramePadding.x * 2.0f +
				kLabelValueSpacing };

	data.measured_width = std::max(data.measured_width, width);
}

class AutoLabelWidthScope {
public:
	explicit AutoLabelWidthScope(std::string_view label) {
		ImGui::PushID(label.data(), label.data() + label.size());
		id_ = ImGui::GetID("##auto_label_width");

		auto& data{ AutoLabelWidths()[id_] };
		data.start_x		= ImGui::GetCursorPosX();
		data.measured_width = kDefaultLabelWidth;

		AutoLabelWidthStack().push_back(&data);
	}

	~AutoLabelWidthScope() {
		auto& stack{ AutoLabelWidthStack() };

		if (!stack.empty()) {
			auto& data{ *stack.back() };
			data.width = data.measured_width;
			stack.pop_back();
		}

		ImGui::PopID();
	}

	AutoLabelWidthScope(const AutoLabelWidthScope&)			   = delete;
	AutoLabelWidthScope& operator=(const AutoLabelWidthScope&) = delete;

	AutoLabelWidthScope(AutoLabelWidthScope&&)			  = delete;
	AutoLabelWidthScope& operator=(AutoLabelWidthScope&&) = delete;

private:
	ImGuiID id_{ 0 };
};

struct FieldOptions {
	float speed{ 0.1f };
	float min{ 0.0 };
	float max{ 0.0 };
	const char* format{ nullptr };
	ImGuiSliderFlags flags{ ImGuiSliderFlags_None };
	bool multiline{ false };
	bool default_open{ true };
	bool read_only{ false };
	std::string_view array_item_name{ "Item" };
};

template <typename T>
inline constexpr FieldOptions kDefaultFieldOptions{};

template <typename T>
inline constexpr FieldOptions kDefaultFieldOptions<std::optional<T>>{ kDefaultFieldOptions<T> };

template <>
inline constexpr FieldOptions kDefaultFieldOptions<float>{
	.speed	= 0.1f,
	.format = "%.3f",
};

template <>
inline constexpr FieldOptions kDefaultFieldOptions<int>{
	.speed	= 1.0f,
	.format = "%d",
};

template <>
inline constexpr FieldOptions kDefaultFieldOptions<std::int64_t>{
	.speed	= 1.0f,
	.format = "%lld",
};

template <>
inline constexpr FieldOptions kDefaultFieldOptions<std::size_t>{
	.speed	= 1.0f,
	.format = "%llu",
};

template <>
inline constexpr FieldOptions kDefaultFieldOptions<V2_float>{
	.speed	= 0.1f,
	.format = "%.3f",
};

template <>
inline constexpr FieldOptions kDefaultFieldOptions<V2_int>{
	.speed	= 1.0f,
	.format = "%d",
};

template <>
inline constexpr FieldOptions kDefaultFieldOptions<Degrees>{
	.speed	= 1.0f,
	.format = "%.1f deg",
};

template <>
inline constexpr FieldOptions kDefaultFieldOptions<Radians>{
	.speed	= 1.0f,
	.format = "%.1f deg",
};

template <typename Rep, typename Period>
inline constexpr FieldOptions kDefaultFieldOptions<std::chrono::duration<Rep, Period>>{
	.speed = std::floating_point<Rep> ? 0.01f : 1.0f,
};

template <>
inline constexpr FieldOptions kDefaultFieldOptions<Matrix4>{
	.default_open = false,
};

template <typename T>
concept ReflectedMembers = requires(T& value) { ReflectMembers(value); };

template <typename T>
concept ReflectedReadOnlyMembers = requires(const T& value) { ReflectReadOnlyMembers(value); };

template <typename T>
concept ReflectedValue = requires(T& value) { ReflectValue(value); };

template <typename T>
struct IsVector : std::false_type {};

template <typename T, typename Allocator>
struct IsVector<std::vector<T, Allocator>> : std::true_type {};

template <typename T>
inline constexpr bool kIsVector{ IsVector<T>::value };

template <typename T>
struct IsArray : std::false_type {};

template <typename T, std::size_t N>
struct IsArray<std::array<T, N>> : std::true_type {};

template <typename T>
inline constexpr bool kIsArray{ IsArray<T>::value };

template <typename T>
struct IsVariant : std::false_type {};

template <typename... T>
struct IsVariant<std::variant<T...>> : std::true_type {};

template <typename T>
inline constexpr bool kIsVariant{ IsVariant<T>::value };

template <typename T>
struct IsOptional : std::false_type {};

template <typename T>
struct IsOptional<std::optional<T>> : std::true_type {};

template <typename T>
inline constexpr bool kIsOptional{ IsOptional<T>::value };

template <typename T>
concept AssetKeyType = std::derived_from<std::remove_cvref_t<T>, AssetKey>;


template <typename T>
consteval bool HasDefaultInspectorDrawer();

template <
	typename TTuple,
	std::size_t... TIndex
>
consteval bool ReflectedTupleHasDefaultInspectorDrawer(
	std::index_sequence<TIndex...>
) {
	return (
		HasDefaultInspectorDrawer<
			std::remove_cvref_t<
				decltype(
					std::get<TIndex>(
						std::declval<TTuple&>()
					).value
				)
			>
		>() &&
		...
	);
}

template <
	typename TVariant,
	std::size_t... TIndex
>
consteval bool VariantHasDefaultInspectorDrawer(
	std::index_sequence<TIndex...>
) {
	return (
		HasDefaultInspectorDrawer<
			std::variant_alternative_t<
				TIndex,
				TVariant
			>
		>() &&
		...
	);
}

template <typename T>
consteval bool HasDefaultInspectorDrawer() {
	using Value =
		std::remove_cvref_t<T>;

	if constexpr (
		std::same_as<Value, bool> ||
		std::same_as<Value, float> ||
		std::same_as<Value, int> ||
		std::same_as<Value, std::size_t> ||
		DurationType<Value> ||
		std::same_as<Value, std::string> ||
		std::same_as<Value, Color> ||
		std::same_as<Value, V2_float> ||
		std::same_as<Value, V2_int> ||
		std::same_as<Value, Degrees> ||
		std::same_as<Value, Radians> ||
		std::same_as<Value, FontStyle> ||
		std::same_as<Value, Matrix4> ||
		std::is_enum_v<Value>
	) {
		return true;
	} else if constexpr (
		kIsOptional<Value> ||
		kIsArray<Value> ||
		kIsVector<Value>
	) {
		return HasDefaultInspectorDrawer<
			typename Value::value_type
		>();
	} else if constexpr (
		kIsVariant<Value>
	) {
		return VariantHasDefaultInspectorDrawer<Value>(
			std::make_index_sequence<
				std::variant_size_v<Value>
			>{}
		);
	} else if constexpr (
		ReflectedValue<Value>
	) {
		using Member =
			decltype(
				ReflectValue(
					std::declval<Value&>()
				)
			);
		using MemberValue =
			std::remove_cvref_t<
				decltype(
					std::declval<Member&>().value
				)
			>;

		return HasDefaultInspectorDrawer<
			MemberValue
		>();
	} else if constexpr (
		ReflectedMembers<Value>
	) {
		using Members =
			decltype(
				ReflectMembers(
					std::declval<Value&>()
				)
			);

		if constexpr (
			std::tuple_size_v<Members> == 0
		) {
			return false;
		} else {
			return ReflectedTupleHasDefaultInspectorDrawer<
				Members
			>(
				std::make_index_sequence<
					std::tuple_size_v<Members>
				>{}
			);
		}
	} else if constexpr (
		ReflectedReadOnlyMembers<Value>
	) {
		using Members =
			decltype(
				ReflectReadOnlyMembers(
					std::declval<const Value&>()
				)
			);

		if constexpr (
			std::tuple_size_v<Members> == 0
		) {
			return false;
		} else {
			return ReflectedTupleHasDefaultInspectorDrawer<
				Members
			>(
				std::make_index_sequence<
					std::tuple_size_v<Members>
				>{}
			);
		}
	} else {
		return false;
	}
}

template <typename T>
inline constexpr bool kHasDefaultInspectorDrawer{
	HasDefaultInspectorDrawer<T>()
};

inline std::string PrettyName(std::string_view name) {
	while (!name.empty() && name.back() == '_') {
		name.remove_suffix(1);
	}

	std::string result;
	result.reserve(name.size() + 4);

	bool capitalize{ true };
	char previous{ '\0' };

	for (char c : name) {
		if (c == '_') {
			result.push_back(' ');
			capitalize = true;
			previous   = c;
			continue;
		}

		if (!result.empty() && std::isupper(static_cast<unsigned char>(c)) != 0 &&
			std::islower(static_cast<unsigned char>(previous)) != 0) {
			result.push_back(' ');
		}

		if (capitalize) {
			result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
			capitalize = false;
		} else {
			result.push_back(c);
		}

		previous = c;
	}

	return result;
}

template <typename T>
	requires std::is_enum_v<T>
std::string EnumLabel(T value) {
	auto name{ magic_enum::enum_name(value) };
	return name.empty() ? "Unknown" : PrettyName(name);
}

template <typename T>
std::string TypeLabel() {
	if constexpr (std::is_enum_v<T>) {
		auto name{ magic_enum::enum_type_name<T>() };
		if (!name.empty()) {
			return PrettyName(name);
		}
	}

	return PrettyName(type_name_without_namespaces<T>());
}

inline bool HasBounds(const FieldOptions& options) {
	return options.min < options.max;
}

inline float GetPropertyValueX(float fallback_start_x) {
	auto& stack{ AutoLabelWidthStack() };

	if (stack.empty()) {
		return fallback_start_x + kDefaultLabelWidth;
	}

	return stack.back()->start_x + stack.back()->width;
}

template <typename F>
bool DrawPropertyRow(std::string_view label, F&& draw) {
	auto id{ std::string{ label } };
	float start_x{ ImGui::GetCursorPosX() };

	ImGui::PushID(id.c_str());
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label.data(), label.data() + label.size());
	ImGui::SameLine();
	MeasurePropertyLabel(label, start_x);
	ImGui::SetCursorPosX(GetPropertyValueX(start_x));
	ImGui::SetNextItemWidth(-FLT_MIN);
	bool changed{ std::invoke(std::forward<F>(draw)) };
	ImGui::PopID();

	return changed;
}
template <typename T>
bool DrawValue(
	EditorContext& ctx, std::string_view label, T& value,
	FieldOptions options = kDefaultFieldOptions<std::remove_cvref_t<T>>
);

template <typename T>
bool DrawReadOnlyValue(
	EditorContext& ctx, std::string_view label, const T& value,
	FieldOptions options = kDefaultFieldOptions<std::remove_cvref_t<T>>
);

template <typename T>
bool DrawDefaultContents(EditorContext& ctx, T& value);

template <typename T>
struct Contents {
	static bool Draw(EditorContext& ctx, T& value) {
		return DrawDefaultContents(ctx, value);
	}
};

template <typename T>
bool DrawContents(EditorContext& ctx, T& value) {
	return Contents<std::remove_cvref_t<T>>::Draw(ctx, value);
}

template <typename T>
bool DrawComponentContents(EditorContext& ctx, T& value) {
	auto label{ TypeLabel<std::remove_cvref_t<T>>() };
	AutoLabelWidthScope label_width{ label };

	bool changed{ DrawContents(ctx, value) };

	return changed;
}

template <typename T>
bool DrawDefaultComponentContents(EditorContext& ctx, T& value) {
	auto label{ TypeLabel<std::remove_cvref_t<T>>() };
	AutoLabelWidthScope label_width{ label };

	return DrawDefaultContents(ctx, value);
}

inline int& ReadOnlyDepth() {
	static int depth{ 0 };
	return depth;
}

class ReadOnlyScope {
public:
	explicit ReadOnlyScope(bool enabled) : enabled_{ enabled } {
		if (enabled_) {
			++ReadOnlyDepth();
		}
	}

	~ReadOnlyScope() {
		if (enabled_) {
			--ReadOnlyDepth();
		}
	}

	ReadOnlyScope(const ReadOnlyScope&)			   = delete;
	ReadOnlyScope& operator=(const ReadOnlyScope&) = delete;

	ReadOnlyScope(ReadOnlyScope&&)			  = delete;
	ReadOnlyScope& operator=(ReadOnlyScope&&) = delete;

private:
	bool enabled_{ false };
};

[[nodiscard]] inline bool IsReadOnly() {
	return ReadOnlyDepth() > 0;
}

[[nodiscard]] inline bool IsReadOnly(const FieldOptions& options) {
	return options.read_only || IsReadOnly();
}

template <typename F>
bool DrawDisabledIf(bool disabled, F&& draw) {
	ImGui::BeginDisabled(disabled);
	bool changed{ std::invoke(std::forward<F>(draw)) };
	ImGui::EndDisabled();
	return changed;
}

template <typename T>
bool DrawReadOnlyValue(EditorContext& ctx, std::string_view label, const T& value, FieldOptions options) {
	using Value = std::remove_cvref_t<T>;

	static_assert(
		std::copy_constructible<Value>,
		"Read only reflected inspector values must be copy constructible"
	);

	Value display_value{ value };
	options.read_only = true;

	ImGui::PushID(&value);
	DrawValue(ctx, label, display_value, options);
	ImGui::PopID();

	return false;
}

template <typename T>
bool DrawMembers(EditorContext& ctx, T& value) {
	bool changed{ false };

	if constexpr (ReflectedMembers<T>) {
		auto members{ ReflectMembers(value) };

		std::apply(
			[&]<typename... TMember>(TMember&&... member) {
				((changed |= DrawValue(ctx, PrettyName(member.name), member.value)), ...);
			},
			members
		);
	}

	if constexpr (ReflectedReadOnlyMembers<T>) {
		if (ctx.local.settings.show_read_only_inspector_data) {
			auto members{ ReflectReadOnlyMembers(value) };

			std::apply(
				[&ctx]<typename... TMember>(TMember&&... member) {
					(DrawReadOnlyValue(ctx, PrettyName(member.name), member.value), ...);
				},
				members
			);
		}
	}

	return changed;
}

template <AssetKeyType T>
bool IsValidAssetKey(EditorContext& ctx, const T& key) {
	if (key.value.empty()) {
		return true;
	}

	auto& assets{ ctx.editor.GetAssetManager() };

	return assets.Has(key);
}

template <AssetKeyType T>
bool DrawAssetKeyInline(
	EditorContext& ctx,
	T& value,
	const FieldOptions& options,
	std::string_view hint = {}
) {
	const bool read_only{ IsReadOnly(options) };

	std::string resolved_hint{ hint };

	if constexpr (std::same_as<T, FontKey>) {
		if (resolved_hint.empty() && value.value == kDefaultFont) {
			resolved_hint = "Default Font";
		}
	}

	bool changed{ DrawDisabledIf(read_only, [&]() {
		return ImGui::InputTextWithHint(
			"##value",
			resolved_hint.c_str(),
			&value.value
		);
	}) };

	const ImVec2 input_min{ ImGui::GetItemRectMin() };
	const ImVec2 input_max{ ImGui::GetItemRectMax() };
	const bool input_hovered{ ImGui::IsItemHovered() };

	if (!read_only) {
		changed |= ptgn::editor::AcceptAssetKeyDragDrop(value);
	}

	if (IsValidAssetKey(ctx, value)) {
		return changed;
	}

	const ImU32 error_color{
		ImGui::GetColorU32(
			ImVec4{ 1.0f, 0.25f, 0.25f, 1.0f }
		)
	};

	ImGui::GetWindowDrawList()->AddRect(
		input_min,
		input_max,
		error_color,
		ImGui::GetStyle().FrameRounding
	);

	if (input_hovered) {
		using Value = std::remove_cvref_t<T>;

		if constexpr (std::same_as<Value, AssetKey>) {
			ImGui::SetTooltip(
				"No asset exists with key \"%s\".",
				value.value.c_str()
			);
		} else {
			const auto kind_name{
				magic_enum::enum_name(Value::kind)
			};

			ImGui::SetTooltip(
				"No %.*s asset exists with key \"%s\".",
				static_cast<int>(kind_name.size()),
				kind_name.data(),
				value.value.c_str()
			);
		}
	}

	return changed;
}

template <AssetKeyType T>
bool DrawAssetKey(EditorContext& ctx, std::string_view label, T& value, const FieldOptions& options) {
	return DrawPropertyRow(label, [&]() { return DrawAssetKeyInline(ctx, value, options); });
}

inline bool DrawFloat(std::string_view label, float& value, const FieldOptions& options) {
	return DrawPropertyRow(label, [&]() {
		float min{ static_cast<float>(options.min) };
		float max{ static_cast<float>(options.max) };

		return DrawDisabledIf(IsReadOnly(options), [&]() {
			return ImGui::DragFloat(
				"##value", &value, options.speed, HasBounds(options) ? min : 0.0f,
				HasBounds(options) ? max : 0.0f, options.format ? options.format : "%.3f",
				options.flags
			);
		});
	});
}

inline bool DrawInt64(std::string_view label, std::int64_t& value, const FieldOptions& options) {
	return DrawPropertyRow(label, [&]() {
		std::int64_t min{ static_cast<std::int64_t>(options.min) };
		std::int64_t max{ static_cast<std::int64_t>(options.max) };

		return DrawDisabledIf(IsReadOnly(options), [&]() {
			return ImGui::DragScalar(
				"##value", ImGuiDataType_S64, &value, options.speed,
				HasBounds(options) ? &min : nullptr, HasBounds(options) ? &max : nullptr,
				options.format ? options.format : "%lld", options.flags
			);
		});
	});
}

inline bool DrawInt(std::string_view label, int& value, const FieldOptions& options) {
	return DrawPropertyRow(label, [&]() {
		int min{ static_cast<int>(options.min) };
		int max{ static_cast<int>(options.max) };

		return DrawDisabledIf(IsReadOnly(options), [&]() {
			return ImGui::DragInt(
				"##value", &value, options.speed, HasBounds(options) ? min : 0,
				HasBounds(options) ? max : 0, options.format ? options.format : "%d", options.flags
			);
		});
	});
}

inline bool DrawUInt64(std::string_view label, std::uint64_t& value, const FieldOptions& options) {
	return DrawPropertyRow(label, [&]() {
		std::uint64_t temporary{ value };
		std::uint64_t min{ static_cast<std::uint64_t>(std::max(0.0f, options.min)) };
		std::uint64_t max{ static_cast<std::uint64_t>(std::max(0.0f, options.max)) };

		bool changed{ DrawDisabledIf(IsReadOnly(options), [&]() {
			return ImGui::DragScalar(
				"##value", ImGuiDataType_U64, &temporary, options.speed,
				HasBounds(options) ? &min : nullptr, HasBounds(options) ? &max : nullptr,
				options.format ? options.format : "%llu", options.flags
			);
		}) };

		if (changed) {
			value = temporary;
		}

		return changed;
	});
}

inline bool DrawSize(std::string_view label, std::size_t& value, const FieldOptions& options) {
	std::uint64_t temporary{ static_cast<std::uint64_t>(value) };

	if (!DrawUInt64(label, temporary, options)) {
		return false;
	}

	value = static_cast<std::size_t>(temporary);
	return true;
}

template <typename Rep, typename Period>
bool DrawDuration(
	std::string_view label, std::chrono::duration<Rep, Period>& value, const FieldOptions& options
) {
	auto unit{ DurationUnit<Period>() };

	if constexpr (std::integral<Rep>) {
		return DrawPropertyRow(label, [&]() {
			std::int64_t temporary{ static_cast<std::int64_t>(value.count()) };
			std::int64_t min{ static_cast<std::int64_t>(options.min) };
			std::int64_t max{ static_cast<std::int64_t>(options.max) };

			auto default_format{ std::string{ "%lld " } + std::string{ unit } };
			auto format{ options.format ? options.format : default_format.c_str() };

			bool changed{ DrawDisabledIf(IsReadOnly(options), [&]() {
				return ImGui::DragScalar(
					"##value", ImGuiDataType_S64, &temporary, options.speed,
					HasBounds(options) ? &min : nullptr, HasBounds(options) ? &max : nullptr,
					format, options.flags
				);
			}) };

			if (changed) {
				value = std::chrono::duration<Rep, Period>{ static_cast<Rep>(temporary) };
			}

			return changed;
		});
	} else {
		return DrawPropertyRow(label, [&]() {
			float temporary{ static_cast<float>(value.count()) };
			float min{ static_cast<float>(options.min) };
			float max{ static_cast<float>(options.max) };

			auto default_format{ std::string{ "%.3f " } + std::string{ unit } };
			auto format{ options.format ? options.format : default_format.c_str() };

			bool changed{ DrawDisabledIf(IsReadOnly(options), [&]() {
				return ImGui::DragScalar(
					"##value", ImGuiDataType_Float, &temporary, options.speed,
					HasBounds(options) ? &min : nullptr, HasBounds(options) ? &max : nullptr,
					format, options.flags
				);
			}) };

			if (changed) {
				value = std::chrono::duration<Rep, Period>{ static_cast<Rep>(temporary) };
			}

			return changed;
		});
	}
}

template <typename T, std::size_t N, typename Label>
bool DrawArrayEditorItems(EditorContext& ctx, std::array<T, N>& values, Label&& get_item_label) {
	bool changed{ false };

	for (auto i{ 0uz }; i < N; ++i) {
		ImGui::PushID(static_cast<int>(i));

		auto item_label{ std::invoke(get_item_label, i) };
		changed |= DrawValue(ctx, item_label, values[i]);

		ImGui::PopID();
	}

	return changed;
}

template <typename T, std::size_t N, typename Label>
bool DrawArrayEditor(
	EditorContext& ctx, std::string_view label, std::array<T, N>& values, Label&& get_item_label,
	FieldOptions options = {}
) {
	auto header{ std::string{ label } + " (" + std::to_string(N) + ")" };

	ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_SpanAvailWidth };

	if (options.default_open) {
		flags |= ImGuiTreeNodeFlags_DefaultOpen;
	}

	bool open{ ImGui::TreeNodeEx(header.c_str(), flags) };

	if (!open) {
		return false;
	}

	bool changed{ DrawArrayEditorItems(ctx, values, std::forward<Label>(get_item_label)) };

	ImGui::TreePop();

	return changed;
}

template <typename T, std::size_t N>
bool DrawArrayEditor(EditorContext& ctx, std::string_view label, std::array<T, N>& values, FieldOptions options = {}) {
	return DrawArrayEditor(ctx,
		label, values,
		[&](std::size_t index) {
			return std::string{ options.array_item_name } + " " + std::to_string(index);
		},
		options
	);
}

template <typename T, std::size_t N, typename Label>
bool DrawArrayEditor(EditorContext& ctx, std::array<T, N>& values, Label&& get_item_label) {
	return DrawArrayEditorItems(ctx, values, std::forward<Label>(get_item_label));
}

template <typename T, std::size_t N>
bool DrawArrayEditor(EditorContext& ctx, std::array<T, N>& values, FieldOptions options = {}) {
	return DrawArrayEditor(ctx,
		values,
		[&](std::size_t index) {
			return std::string{ options.array_item_name } + " " + std::to_string(index);
		},
		options
	);
}

template <typename TEnum, typename T, std::size_t N>
	requires std::is_enum_v<TEnum>
bool DrawEnumArrayEditor(EditorContext& ctx,
	std::string_view label, std::array<T, N>& values, FieldOptions options = {}
) {
	constexpr auto entries{ magic_enum::enum_entries<TEnum>() };

#ifndef PTGN_PLATFORM_MACOS
	static_assert(
		magic_enum::enum_count<TEnum>() == N,
		"Enum indexed array size must match the number of reflected enum values"
	);
#endif

	return DrawArrayEditor(ctx,
		label, values, [&](std::size_t index) { return PrettyName(entries[index].second); }, options
	);
}

template <typename TEnum, typename T, std::size_t N>
	requires std::is_enum_v<TEnum>
bool DrawEnumArrayEditor(EditorContext& ctx, std::array<T, N>& values) {
	constexpr auto entries{ magic_enum::enum_entries<TEnum>() };

#ifndef PTGN_PLATFORM_MACOS
	static_assert(
		magic_enum::enum_count<TEnum>() == N,
		"Enum indexed array size must match the number of reflected enum values"
	);
#endif

	return DrawArrayEditor(ctx, values, [&](std::size_t index) {
		return PrettyName(entries[index].second);
	});
}

inline bool DrawVector(std::string_view label, V2_float& value, const FieldOptions& options) {
	return DrawPropertyRow(label, [&]() {
		float values[2]{ value.x, value.y };
		float min{ static_cast<float>(options.min) };
		float max{ static_cast<float>(options.max) };

		bool changed{ DrawDisabledIf(IsReadOnly(options), [&]() {
			return ImGui::DragFloat2(
				"##value", values, options.speed, HasBounds(options) ? min : 0.0f,
				HasBounds(options) ? max : 0.0f, options.format ? options.format : "%.3f",
				options.flags
			);
		}) };

		if (changed) {
			value = { values[0], values[1] };
		}
		return changed;
	});
}

inline bool DrawVector(std::string_view label, V2_int& value, const FieldOptions& options) {
	return DrawPropertyRow(label, [&]() {
		int values[2]{ value.x, value.y };
		int min{ static_cast<int>(options.min) };
		int max{ static_cast<int>(options.max) };

		bool changed{ DrawDisabledIf(IsReadOnly(options), [&]() {
			return ImGui::DragInt2(
				"##value", values, options.speed, HasBounds(options) ? min : 0,
				HasBounds(options) ? max : 0, options.format ? options.format : "%d", options.flags
			);
		}) };

		if (changed) {
			value = { values[0], values[1] };
		}
		return changed;
	});
}

inline bool DrawColor(std::string_view label, Color& value) {
	return DrawPropertyRow(label, [&]() {
		float rgba[4]{
			static_cast<float>(value.r) / 255.0f,
			static_cast<float>(value.g) / 255.0f,
			static_cast<float>(value.b) / 255.0f,
			static_cast<float>(value.a) / 255.0f,
		};

		bool changed{ DrawDisabledIf(IsReadOnly(), [&]() {
			return ImGui::ColorEdit4(
				"##value", rgba,
				ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_AlphaBar |
					ImGuiColorEditFlags_AlphaPreviewHalf
			);
		}) };

		if (changed) {
			auto to_byte = [](float channel) {
				return static_cast<std::uint8_t>(
					std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f)
				);
			};

			value.r = to_byte(rgba[0]);
			value.g = to_byte(rgba[1]);
			value.b = to_byte(rgba[2]);
			value.a = to_byte(rgba[3]);
		}
		return changed;
	});
}

inline void DrawCenteredTableText(
	std::string_view text, float cell_height = ImGui::GetFrameHeight()
) {
	auto text_size{ ImGui::CalcTextSize(text.data(), text.data() + text.size()) };

	auto offset_x{ std::max(0.0f, (ImGui::GetColumnWidth() - text_size.x) * 0.5f) };
	auto offset_y{ std::max(0.0f, (cell_height - text_size.y) * 0.5f) };

	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset_y);
	ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

inline bool DrawMatrix4(std::string_view label, Matrix4& value, const FieldOptions& options) {
	ImGui::PushID(&value);

	ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_SpanAvailWidth };

	if (options.default_open) {
		flags |= ImGuiTreeNodeFlags_DefaultOpen;
	}

	auto title{ std::string{ label } + " (Column Major 4x4)" };

	bool open{ ImGui::TreeNodeEx(title.c_str(), flags) };

	if (!open) {
		ImGui::PopID();
		return false;
	}

	bool changed{ false };

	if (ImGui::BeginTable(
			"##matrix", 5,
			ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame |
				ImGuiTableFlags_NoSavedSettings
		)) {
		ImGui::TableSetupColumn("");
		ImGui::TableSetupColumn("Col 0");
		ImGui::TableSetupColumn("Col 1");
		ImGui::TableSetupColumn("Col 2");
		ImGui::TableSetupColumn("Col 3");

		auto cell_height{ ImGui::GetFrameHeight() };

		ImGui::TableNextRow(ImGuiTableRowFlags_Headers, cell_height);

		ImGui::TableSetColumnIndex(0);
		DrawCenteredTableText("", cell_height);

		for (auto column{ 0uz }; column < 4uz; ++column) {
			ImGui::TableSetColumnIndex(static_cast<int>(column + 1uz));
			DrawCenteredTableText("Col " + std::to_string(column), cell_height);
		}

		for (auto row{ 0uz }; row < 4uz; ++row) {
			ImGui::TableNextRow(ImGuiTableRowFlags_None, cell_height);

			ImGui::TableSetColumnIndex(0);
			DrawCenteredTableText("Row " + std::to_string(row), cell_height);

			for (auto column{ 0uz }; column < 4uz; ++column) {
				ImGui::TableSetColumnIndex(static_cast<int>(column + 1uz));
				ImGui::PushID(static_cast<int>(row + column * 4uz));

				ImGui::SetNextItemWidth(-FLT_MIN);

				changed |= DrawDisabledIf(IsReadOnly(options), [&]() {
					return ImGui::DragFloat(
						"##value", &value(row, column), options.speed,
						HasBounds(options) ? static_cast<float>(options.min) : 0.0f,
						HasBounds(options) ? static_cast<float>(options.max) : 0.0f,
						options.format ? options.format : "%.3f", options.flags
					);
				});

				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(
						"Index %zu\nm(%zu, %zu)\nColumn major offset: row + column * 4",
						row + column * 4uz, row, column
					);
				}

				ImGui::PopID();
			}
		}

		ImGui::EndTable();
	}

	ImGui::TreePop();
	ImGui::PopID();

	return changed;
}

template <typename T>
	requires std::is_enum_v<T>
constexpr bool IsSingleEnumFlag(T value) {
	using Underlying = std::underlying_type_t<T>;
	using Unsigned	 = std::make_unsigned_t<Underlying>;

	auto bits{ static_cast<Unsigned>(magic_enum::enum_underlying(value)) };
	return bits != 0 && (bits & (bits - 1)) == 0;
}

inline std::string FontStylePreview(FontStyle style) {
	if (style == FontStyle::Normal) {
		return EnumLabel(style);
	}

	std::string result;

	for (auto [flag, name] : magic_enum::enum_entries<FontStyle>()) {
		if (!IsSingleEnumFlag(flag) || !HasFontFlag(style, flag)) {
			continue;
		}

		if (!result.empty()) {
			result += " | ";
		}

		result += PrettyName(name);
	}

	return result.empty() ? "Unknown" : result;
}

inline bool DrawFontStyle(std::string_view label, FontStyle& value) {
	return DrawPropertyRow(label, [&]() {
		return DrawDisabledIf(IsReadOnly(), [&]() {
			auto preview{ FontStylePreview(value) };
			bool changed{ false };

			if (ImGui::BeginCombo("##value", preview.c_str())) {
				for (auto [flag, name] : magic_enum::enum_entries<FontStyle>()) {
					if (!IsSingleEnumFlag(flag)) {
						continue;
					}

					auto item_label{ PrettyName(name) };
					bool enabled{ HasFontFlag(value, flag) };

					if (ImGui::Selectable(
							item_label.c_str(), enabled, ImGuiSelectableFlags_DontClosePopups
						)) {
						value	= SetFontFlag(value, flag, !enabled);
						changed = true;
					}
				}

				ImGui::EndCombo();
			}

			return changed;
		});
	});
}

template <typename T>
	requires std::is_enum_v<T>
bool DrawEnum(std::string_view label, T& value) {
	return DrawPropertyRow(label, [&]() {
		return DrawDisabledIf(IsReadOnly(), [&]() {
			auto preview{ EnumLabel(value) };
			bool changed{ false };

			if (ImGui::BeginCombo("##value", preview.c_str())) {
				for (auto [candidate, name] : magic_enum::enum_entries<T>()) {
					auto item_label{ PrettyName(name) };
					bool selected{ candidate == value };

					if (ImGui::Selectable(item_label.c_str(), selected)) {
						value	= candidate;
						changed = true;
					}

					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			return changed;
		});
	});
}

inline bool DrawFixedWidthCollapsingHeader(std::string_view label, float width, bool default_open) {
	auto& style{ ImGui::GetStyle() };
	auto size{ ImVec2{ std::max(1.0f, width), ImGui::GetFrameHeight() } };

	ImGui::PushID("CollapsingHeader");

	auto id{ ImGui::GetID("##open") };
	auto* storage{ ImGui::GetStateStorage() };

	bool open{ storage->GetBool(id, default_open) };

	auto pos{ ImGui::GetCursorScreenPos() };

	if (ImGui::InvisibleButton("##button", size)) {
		open = !open;
		storage->SetBool(id, open);
	}

	auto hovered{ ImGui::IsItemHovered() };
	auto active{ ImGui::IsItemActive() };

	auto color{ ImGui::GetColorU32(
		active	  ? ImGuiCol_HeaderActive
		: hovered ? ImGuiCol_HeaderHovered
				  : ImGuiCol_Header
	) };

	auto text_color{ ImGui::GetColorU32(ImGuiCol_Text) };
	auto* draw_list{ ImGui::GetWindowDrawList() };

	auto max{ ImVec2{ pos.x + size.x, pos.y + size.y } };

	draw_list->AddRectFilled(pos, max, color, style.FrameRounding);

	if (style.FrameBorderSize > 0.0f) {
		draw_list->AddRect(pos, max, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding);
	}

	auto arrow_half_size{ ImGui::GetFontSize() * 0.35f };
	auto arrow_center{ ImVec2{
		pos.x + style.FramePadding.x + arrow_half_size,
		pos.y + size.y * 0.5f,
	} };

	if (open) {
		draw_list->AddTriangleFilled(
			ImVec2{ arrow_center.x - arrow_half_size, arrow_center.y - arrow_half_size * 0.5f },
			ImVec2{ arrow_center.x + arrow_half_size, arrow_center.y - arrow_half_size * 0.5f },
			ImVec2{ arrow_center.x, arrow_center.y + arrow_half_size * 0.5f }, text_color
		);
	} else {
		draw_list->AddTriangleFilled(
			ImVec2{ arrow_center.x - arrow_half_size * 0.5f, arrow_center.y - arrow_half_size },
			ImVec2{ arrow_center.x - arrow_half_size * 0.5f, arrow_center.y + arrow_half_size },
			ImVec2{ arrow_center.x + arrow_half_size * 0.5f, arrow_center.y }, text_color
		);
	}

	auto text_pos{ ImVec2{
		pos.x + style.FramePadding.x + ImGui::GetFontSize() + style.ItemInnerSpacing.x,
		pos.y + (size.y - ImGui::GetTextLineHeight()) * 0.5f,
	} };

	draw_list->PushClipRect(pos, max, true);
	draw_list->AddText(text_pos, text_color, label.data(), label.data() + label.size());
	draw_list->PopClipRect();

	ImGui::PopID();

	return open;
}

struct VectorOptions {
	std::string item_name{ "Item" };
	bool default_open{ true };
	bool reorderable{ true };
};

template <typename T, typename Draw>
bool DrawVectorEditorItems(std::vector<T>& values, VectorOptions options, Draw&& draw) {
	ImGui::PushID(&values);

	bool changed{ false };
	std::optional<std::size_t> remove_index;
	std::optional<std::pair<std::size_t, std::size_t>> move;

	auto& style{ ImGui::GetStyle() };
	bool read_only{ IsReadOnly() };

	for (auto i{ 0uz }; i < values.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));

		auto item_label{ options.item_name + " " + std::to_string(i + 1) };

		auto spacing{ style.ItemInnerSpacing.x };
		auto button_size{ ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() } };

		auto action_count{ options.reorderable ? 3 : 1 };
		auto action_button_width{ static_cast<float>(action_count) * button_size.x +
								  static_cast<float>(action_count - 1) * spacing };

		auto available_width{ ImGui::GetContentRegionAvail().x };
		auto header_width{ available_width - action_button_width - spacing };
		header_width = std::max(1.0f, header_width);

		bool item_open{
			DrawFixedWidthCollapsingHeader(item_label, header_width, options.default_open)
		};

		ImGui::SameLine(0.0f, spacing);

		if (options.reorderable) {
			ImGui::BeginDisabled(read_only || i == 0);
			if (ImGui::ArrowButton("##up", ImGuiDir_Up)) {
				move = std::pair{ i, i - 1 };
			}
			ImGui::EndDisabled();

			ImGui::SameLine(0.0f, spacing);

			ImGui::BeginDisabled(read_only || i + 1 >= values.size());
			if (ImGui::ArrowButton("##down", ImGuiDir_Down)) {
				move = std::pair{ i, i + 1 };
			}
			ImGui::EndDisabled();

			ImGui::SameLine(0.0f, spacing);
		}

		ImGui::BeginDisabled(read_only);
		if (ImGui::Button("X##remove", button_size)) {
			remove_index = i;
		}
		ImGui::EndDisabled();

		if (item_open) {
			ImGui::Indent();
			changed |= std::invoke(draw, values[i], i);
			ImGui::Unindent();
		}

		ImGui::PopID();
	}

	if (move.has_value()) {
		std::ranges::iter_swap(
			values.begin() + static_cast<std::ptrdiff_t>(move->first),
			values.begin() + static_cast<std::ptrdiff_t>(move->second)
		);
		changed = true;
	}

	if (remove_index.has_value()) {
		values.erase(values.begin() + static_cast<std::ptrdiff_t>(*remove_index));
		changed = true;
	}

	if (!values.empty()) {
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + style.FramePadding.y);
	}

	auto add_label{ "Add " + options.item_name };

	ImGui::BeginDisabled(read_only);
	if (ImGui::Button(add_label.c_str(), ImVec2{ -FLT_MIN, 0.0f })) {
		values.emplace_back();
		changed = true;
	}
	ImGui::EndDisabled();

	ImGui::PopID();

	return changed;
}

template <typename T, typename Draw>
bool DrawVectorEditor(
	std::string_view label, std::vector<T>& values, VectorOptions options, Draw&& draw
) {
	auto header{ std::string{ label } + " (" + std::to_string(values.size()) + ")" };

	ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_SpanAvailWidth };
	if (options.default_open) {
		flags |= ImGuiTreeNodeFlags_DefaultOpen;
	}

	bool open{ ImGui::TreeNodeEx(header.c_str(), flags) };
	if (!open) {
		return false;
	}

	bool changed{ DrawVectorEditorItems(values, std::move(options), std::forward<Draw>(draw)) };

	ImGui::TreePop();

	return changed;
}

template <typename T, typename Draw>
bool DrawVectorEditor(std::vector<T>& values, VectorOptions options, Draw&& draw) {
	return DrawVectorEditorItems(values, std::move(options), std::forward<Draw>(draw));
}

template <typename T>
bool DrawVectorEditor(EditorContext& ctx, std::string_view label, std::vector<T>& values, VectorOptions options = {}) {
	return DrawVectorEditor(
		label, values, std::move(options),
		[&ctx]<typename TValue>(TValue& value, std::size_t) { return DrawContents(ctx, value); }
	);
}

template <typename T>
bool DrawVectorEditor(EditorContext& ctx, std::vector<T>& values, VectorOptions options = {}) {
	return DrawVectorEditor(
		values, std::move(options),
		[&ctx]<typename TValue>(TValue& value, std::size_t) { return DrawContents(ctx, value); }
	);
}

template <std::size_t I = 0, typename... T>
void EmplaceVariant(std::variant<T...>& value, std::size_t index) {
	if constexpr (I < sizeof...(T)) {
		if (index == I) {
			value.template emplace<I>();
			return;
		}
		EmplaceVariant<I + 1>(value, index);
	}
}

template <typename Variant, std::size_t... I>
auto VariantNames(std::index_sequence<I...>) {
	return std::array<std::string, sizeof...(I)>{
		TypeLabel<std::variant_alternative_t<I, Variant>>()...
	};
}

template <typename T>
inline constexpr bool kHasNoReflectedContents = []() {
	if constexpr (ReflectedValue<T>) {
		return false;
	}

	constexpr bool has_editable_members = []() {
		if constexpr (ReflectedMembers<T>) {
			using Tuple = decltype(ReflectMembers(std::declval<T&>()));
			return std::tuple_size_v<Tuple> != 0;
		} else {
			return false;
		}
	}();

	constexpr bool has_read_only_members = []() {
		if constexpr (ReflectedReadOnlyMembers<T>) {
			using Tuple = decltype(ReflectReadOnlyMembers(std::declval<const T&>()));
			return std::tuple_size_v<Tuple> != 0;
		} else {
			return false;
		}
	}();

	return !has_editable_members && !has_read_only_members;
}();

template <typename... T>
bool DrawVariant(
	EditorContext& ctx, std::string_view label, std::variant<T...>& value, std::string_view value_label = {},
	FieldOptions options = {}
) {
	using Variant = std::variant<T...>;
	static auto names{ VariantNames<Variant>(std::index_sequence_for<T...>{}) };

	bool changed{ DrawPropertyRow(label, [&]() {
		return DrawDisabledIf(IsReadOnly(), [&]() {
			bool local_changed{ false };
			auto index{ value.index() };

			if (ImGui::BeginCombo("##value", names[index].c_str())) {
				for (auto i{ 0uz }; i < names.size(); ++i) {
					bool selected{ i == index };

					if (ImGui::Selectable(names[i].c_str(), selected)) {
						EmplaceVariant(value, i);
						local_changed = true;
					}

					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			return local_changed;
		});
	}) };

	std::visit(
		[&]<typename TValue>(TValue& active) {
			using Value = std::remove_cvref_t<TValue>;

			if constexpr (!std::is_empty_v<Value> || !kHasNoReflectedContents<Value>) {
				ImGui::Indent();

				if (value_label.empty()) {
					changed |= DrawContents(ctx, active);
				} else {
					changed |= DrawValue(ctx, value_label, active, options);
				}

				ImGui::Unindent();
			}
		},
		value
	);

	return changed;
}

inline bool DrawOptionalBool(std::string_view label, std::optional<bool>& value) {
	return DrawPropertyRow(label, [&]() {
		return DrawDisabledIf(IsReadOnly(), [&]() {
			auto preview{ value.has_value() ? (value.value() ? "True" : "False") : "Unset" };

			bool changed{ false };

			if (ImGui::BeginCombo("##value", preview)) {
				if (ImGui::Selectable("Unset", !value.has_value())) {
					value.reset();
					changed = true;
				}

				if (ImGui::Selectable("False", value.has_value() && !value.value())) {
					value	= false;
					changed = true;
				}

				if (ImGui::Selectable("True", value.has_value() && value.value())) {
					value	= true;
					changed = true;
				}

				ImGui::EndCombo();
			}

			return changed;
		});
	});
}

template <typename T>
	requires std::is_enum_v<T>
bool DrawOptionalEnum(std::string_view label, std::optional<T>& value) {
	ImGui::PushID(&value);

	bool enabled{ value.has_value() };

	bool changed{ DrawPropertyRow(label, [&]() {
		return DrawDisabledIf(IsReadOnly(), [&]() {
			bool local_changed{ ImGui::Checkbox("##enabled", &enabled) };

			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

			if (enabled && !value.has_value()) {
				value		  = magic_enum::enum_values<T>().front();
				local_changed = true;
			}

			auto preview{ value.has_value() ? EnumLabel(value.value()) : "Unset" };

			ImGuiComboFlags combo_flags{ ImGuiComboFlags_None };

			if (!enabled) {
				combo_flags |= ImGuiComboFlags_NoArrowButton;
			}

			ImGui::BeginDisabled(!enabled);
			ImGui::SetNextItemWidth(-FLT_MIN);

			if (ImGui::BeginCombo("##value", preview.c_str(), combo_flags)) {
				for (auto candidate : magic_enum::enum_values<T>()) {
					auto item_label{ EnumLabel(candidate) };
					bool selected{ value.has_value() && value.value() == candidate };

					if (ImGui::Selectable(item_label.c_str(), selected)) {
						value		  = candidate;
						local_changed = true;
					}

					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			ImGui::EndDisabled();

			return local_changed;
		});
	}) };

	if (enabled != value.has_value()) {
		if (enabled) {
			value = magic_enum::enum_values<T>().front();
		} else {
			value.reset();
		}

		changed = true;
	}

	ImGui::PopID();

	return changed;
}

template <typename T>
inline constexpr bool kCanDrawOptionalInlineValue{
	AssetKeyType<T> || std::same_as<T, float> || std::same_as<T, int> ||
	std::same_as<T, std::int64_t> || std::same_as<T, std::size_t> || DurationType<T> ||
	std::same_as<T, std::string> || std::same_as<T, V2_float> || std::same_as<T, V2_int> ||
	std::same_as<T, Color> || std::same_as<T, Degrees> || std::same_as<T, Radians>
};

template <typename T>
bool CanDrawOptionalInlineValue(const FieldOptions& options) {
	using Value = std::remove_cvref_t<T>;

	if constexpr (std::same_as<Value, std::string>) {
		return !options.multiline;
	} else {
		return kCanDrawOptionalInlineValue<Value>;
	}
}

inline void DrawUnsetOptionalInlineValue() {
	std::string unset{ "Unset" };
	ImGui::InputText("##unset", &unset, ImGuiInputTextFlags_ReadOnly);
}

template <typename Rep, typename Period>
bool DrawDurationInlineValue(
	std::chrono::duration<Rep, Period>& value, const FieldOptions& options
) {
	auto unit{ DurationUnit<Period>() };

	if constexpr (std::integral<Rep>) {
		std::int64_t temporary{ static_cast<std::int64_t>(value.count()) };
		std::int64_t min{ static_cast<std::int64_t>(options.min) };
		std::int64_t max{ static_cast<std::int64_t>(options.max) };

		auto default_format{ std::string{ "%lld " } + std::string{ unit } };
		auto format{ options.format ? options.format : default_format.c_str() };

		bool changed{ DrawDisabledIf(IsReadOnly(options), [&]() {
			return ImGui::DragScalar(
				"##value", ImGuiDataType_S64, &temporary, options.speed,
				HasBounds(options) ? &min : nullptr, HasBounds(options) ? &max : nullptr, format,
				options.flags
			);
		}) };

		if (changed) {
			value = std::chrono::duration<Rep, Period>{ static_cast<Rep>(temporary) };
		}

		return changed;
	} else {
		float temporary{ static_cast<float>(value.count()) };
		float min{ static_cast<float>(options.min) };
		float max{ static_cast<float>(options.max) };

		auto default_format{ std::string{ "%.3f " } + std::string{ unit } };
		auto format{ options.format ? options.format : default_format.c_str() };

		bool changed{ DrawDisabledIf(IsReadOnly(options), [&]() {
			return ImGui::DragScalar(
				"##value", ImGuiDataType_Float, &temporary, options.speed,
				HasBounds(options) ? &min : nullptr, HasBounds(options) ? &max : nullptr, format,
				options.flags
			);
		}) };

		if (changed) {
			value = std::chrono::duration<Rep, Period>{ static_cast<Rep>(temporary) };
		}

		return changed;
	}
}

template <typename T>
bool DrawOptionalInlineValue(EditorContext& ctx, T& value, const FieldOptions& options) {
	using Value = std::remove_cvref_t<T>;

	if constexpr (std::same_as<Value, float>) {
		float min{ static_cast<float>(options.min) };
		float max{ static_cast<float>(options.max) };

		return DrawDisabledIf(IsReadOnly(options), [&]() {
			return ImGui::DragFloat(
				"##value", &value, options.speed, HasBounds(options) ? min : 0.0f,
				HasBounds(options) ? max : 0.0f, options.format ? options.format : "%.3f",
				options.flags
			);
		});
	} else if constexpr (std::same_as<Value, int>) {
		int min{ static_cast<int>(options.min) };
		int max{ static_cast<int>(options.max) };

		return DrawDisabledIf(IsReadOnly(options), [&]() {
			return ImGui::DragInt(
				"##value", &value, options.speed, HasBounds(options) ? min : 0,
				HasBounds(options) ? max : 0, options.format ? options.format : "%d", options.flags
			);
		});
	} else if constexpr (std::same_as<Value, std::int64_t>) {
		std::int64_t min{ static_cast<std::int64_t>(options.min) };
		std::int64_t max{ static_cast<std::int64_t>(options.max) };

		return DrawDisabledIf(IsReadOnly(options), [&]() {
			return ImGui::DragScalar(
				"##value", ImGuiDataType_S64, &value, options.speed,
				HasBounds(options) ? &min : nullptr, HasBounds(options) ? &max : nullptr,
				options.format ? options.format : "%lld", options.flags
			);
		});
	} else if constexpr (std::same_as<Value, std::size_t>) {
		std::uint64_t temporary{ value };
		std::uint64_t min{ static_cast<std::uint64_t>(std::max(0.0f, options.min)) };
		std::uint64_t max{ static_cast<std::uint64_t>(std::max(0.0f, options.max)) };

		bool changed{ DrawDisabledIf(IsReadOnly(options), [&]() {
			return ImGui::DragScalar(
				"##value", ImGuiDataType_U64, &temporary, options.speed,
				HasBounds(options) ? &min : nullptr, HasBounds(options) ? &max : nullptr,
				options.format ? options.format : "%llu", options.flags
			);
		}) };

		if (changed) {
			value = static_cast<std::size_t>(temporary);
		}

		return changed;
	} else if constexpr (DurationType<Value>) {
		return DrawDurationInlineValue(value, options);
	} else if constexpr (AssetKeyType<Value>) {
		return DrawAssetKeyInline(ctx, value, options);
	} else if constexpr (std::same_as<Value, std::string>) {
		return DrawDisabledIf(IsReadOnly(options), [&]() {
			return ImGui::InputText("##value", &value);
		});
	} else if constexpr (std::same_as<Value, V2_float>) {
		float values[2]{ value.x, value.y };
		float min{ static_cast<float>(options.min) };
		float max{ static_cast<float>(options.max) };

		bool changed{ DrawDisabledIf(IsReadOnly(options), [&]() {
			return ImGui::DragFloat2(
				"##value", values, options.speed, HasBounds(options) ? min : 0.0f,
				HasBounds(options) ? max : 0.0f, options.format ? options.format : "%.3f",
				options.flags
			);
		}) };

		if (changed) {
			value = { values[0], values[1] };
		}

		return changed;
	} else if constexpr (std::same_as<Value, V2_int>) {
		int values[2]{ value.x, value.y };
		int min{ static_cast<int>(options.min) };
		int max{ static_cast<int>(options.max) };

		bool changed{ DrawDisabledIf(IsReadOnly(options), [&]() {
			return ImGui::DragInt2(
				"##value", values, options.speed, HasBounds(options) ? min : 0,
				HasBounds(options) ? max : 0, options.format ? options.format : "%d", options.flags
			);
		}) };

		if (changed) {
			value = { values[0], values[1] };
		}

		return changed;
	} else if constexpr (std::same_as<Value, Color>) {
		float rgba[4]{
			static_cast<float>(value.r) / 255.0f,
			static_cast<float>(value.g) / 255.0f,
			static_cast<float>(value.b) / 255.0f,
			static_cast<float>(value.a) / 255.0f,
		};

		bool changed{ DrawDisabledIf(IsReadOnly(options), [&]() {
			return ImGui::ColorEdit4(
				"##value", rgba,
				ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_AlphaBar |
					ImGuiColorEditFlags_AlphaPreviewHalf
			);
		}) };

		if (changed) {
			auto to_byte = [](float channel) {
				return static_cast<std::uint8_t>(
					std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f)
				);
			};

			value.r = to_byte(rgba[0]);
			value.g = to_byte(rgba[1]);
			value.b = to_byte(rgba[2]);
			value.a = to_byte(rgba[3]);
		}

		return changed;
	} else if constexpr (std::same_as<Value, Degrees>) {
		return DrawOptionalInlineValue(ctx, value.value, options);
	} else if constexpr (std::same_as<Value, Radians>) {
		float degrees{ value.ToDeg().value };

		if (!DrawOptionalInlineValue(ctx, degrees, options)) {
			return false;
		}

		value = Degrees{ degrees }.ToRad();
		return true;
	} else {
		static_assert(
			std::is_same_v<Value, void>, "No inline optional drawer exists for this type"
		);
	}
}

template <typename T>
bool DrawOptionalInline(EditorContext& ctx, std::string_view label, std::optional<T>& value, FieldOptions options) {
	ImGui::PushID(&value);

	bool enabled{ value.has_value() };
	bool read_only{ IsReadOnly(options) };

	bool changed{ DrawPropertyRow(label, [&]() {
		bool local_changed{ DrawDisabledIf(read_only, [&]() {
			return ImGui::Checkbox("##enabled", &enabled);
		}) };

		if (enabled && !value.has_value()) {
			value.emplace();
			local_changed = true;
		} else if (!enabled && value.has_value()) {
			value.reset();
			local_changed = true;
		}

		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

		ImGui::BeginDisabled(!enabled || read_only);
		ImGui::SetNextItemWidth(-FLT_MIN);

		if (value.has_value()) {
			local_changed |= DrawOptionalInlineValue(ctx, value.value(), options);
		} else {
			DrawUnsetOptionalInlineValue();
		}

		ImGui::EndDisabled();

		return local_changed;
	}) };

	ImGui::PopID();

	return changed;
}

template <typename T>
bool DrawOptional(EditorContext& ctx, std::string_view label, std::optional<T>& value, FieldOptions options) {
	if constexpr (std::is_enum_v<T>) {
		return DrawOptionalEnum(label, value);
	} else if constexpr (std::same_as<T, bool>) {
		return DrawOptionalBool(label, value);
	} else {
		using Value = std::remove_cvref_t<T>;

		if constexpr (kCanDrawOptionalInlineValue<Value>) {
			if (CanDrawOptionalInlineValue<Value>(options)) {
				return DrawOptionalInline(ctx, label, value, options);
			}
		}

		ImGui::PushID(&value);

		bool enabled{ value.has_value() };

		bool changed{ DrawPropertyRow(label, [&]() {
			return DrawDisabledIf(IsReadOnly(options), [&]() {
				return ImGui::Checkbox("##enabled", &enabled);
			});
		}) };

		if (enabled != value.has_value()) {
			if (enabled) {
				value.emplace();
			} else {
				value.reset();
			}

			changed = true;
		}

		if (value.has_value()) {
			ImGui::Indent();

			if constexpr (kIsVariant<Value>) {
				changed |= DrawVariant(ctx, "Variant", value.value(), "Value", options);
			} else if constexpr (
				ReflectedValue<Value> || ReflectedMembers<Value> ||
				ReflectedReadOnlyMembers<Value>
			) {
				changed |= DrawContents(ctx, value.value());
			} else {
				changed |= DrawValue(ctx, "Value", value.value(), options);
			}

			ImGui::Unindent();
		}

		ImGui::PopID();

		return changed;
	}
}

template <typename T>
bool DrawValue(EditorContext& ctx, std::string_view label, T& value, FieldOptions options) {
	using Value = std::remove_cvref_t<T>;

	static_assert(!std::is_empty_v<T>, "Value type cannot be empty struct/class");

	ReadOnlyScope read_only_scope{ options.read_only };

	if constexpr (std::same_as<Value, bool>) {
		return DrawPropertyRow(label, [&]() {
			return DrawDisabledIf(IsReadOnly(), [&]() {
				return ImGui::Checkbox("##value", &value);
			});
		});
	} else if constexpr (std::same_as<Value, float>) {
		return DrawFloat(label, value, options);
	} else if constexpr (std::same_as<Value, int>) {
		return DrawInt(label, value, options);
	} else if constexpr (std::same_as<Value, std::int64_t>) {
		return DrawInt64(label, value, options);
	} else if constexpr (std::same_as<Value, std::uint64_t>) {
		return DrawUInt64(label, value, options);
	} else if constexpr (std::same_as<Value, std::size_t>) {
		return DrawSize(label, value, options);
	} else if constexpr (DurationType<Value>) {
		return DrawDuration(label, value, options);
	} else if constexpr (AssetKeyType<Value>) {
		return DrawAssetKey(ctx, label, value, options);
	} else if constexpr (std::same_as<Value, std::string>) {
		return DrawPropertyRow(label, [&]() {
			return DrawDisabledIf(IsReadOnly(options), [&]() {
				if (options.multiline) {
					return ImGui::InputTextMultiline(
						"##value", &value,
						ImVec2{
							-FLT_MIN,
							ImGui::GetTextLineHeightWithSpacing() * 4.0f,
						}
					);
				}

				return ImGui::InputText("##value", &value);
			});
		});
	} else if constexpr (std::same_as<Value, Color>) {
		return DrawColor(label, value);
	} else if constexpr (std::same_as<Value, V2_float>) {
		return DrawVector(label, value, options);
	} else if constexpr (std::same_as<Value, V2_int>) {
		return DrawVector(label, value, options);
	} else if constexpr (std::same_as<Value, Degrees>) {
		return DrawFloat(label, value.value, options);
	} else if constexpr (std::same_as<Value, Radians>) {
		float degrees{ value.ToDeg().value };
		if (!DrawFloat(label, degrees, options)) {
			return false;
		}
		value = Degrees{ degrees }.ToRad();
		return true;
	} else if constexpr (std::same_as<Value, FontStyle>) {
		return DrawFontStyle(label, value);
	} else if constexpr (std::is_enum_v<Value>) {
		return DrawEnum(label, value);
	} else if constexpr (kIsOptional<Value>) {
		return DrawOptional(ctx, label, value, options);
	} else if constexpr (kIsArray<Value>) {
		return DrawArrayEditor(ctx, label, value, options);
	} else if constexpr (kIsVector<Value>) {
		using Element = typename Value::value_type;
		return DrawVectorEditor(
			ctx, label, value,
			VectorOptions{
				.item_name = TypeLabel<Element>(),
			}
		);
	} else if constexpr (kIsVariant<Value>) {
		return DrawVariant(ctx, label, value);
	} else if constexpr (std::same_as<Value, Matrix4>) {
		return DrawMatrix4(label, value, options);
	} else if constexpr (ReflectedValue<Value>) {
		auto member{ ReflectValue(value) };
		return DrawValue(ctx, label, member.value, options);
	} else if constexpr (ReflectedMembers<Value> || ReflectedReadOnlyMembers<Value>) {
		auto title{ std::string{ label } };
		bool changed{ false };
		if (ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth)) {
			changed = DrawContents(ctx, value);
			ImGui::TreePop();
		}
		return changed;
	} else {
		static_assert(std::is_same_v<Value, void>, "No inspector drawer exists for this type");
	}
}

template <typename T>
bool DrawDefaultContents(EditorContext& ctx, T& value) {
	using Value = std::remove_cvref_t<T>;

	if constexpr (AssetKeyType<Value>) {
		return DrawValue(ctx, TypeLabel<Value>(), value);
	} else if constexpr (ReflectedMembers<Value> || ReflectedReadOnlyMembers<Value>) {
		return DrawMembers(ctx, value);
	} else if constexpr (ReflectedValue<Value>) {
		auto member{ ReflectValue(value) };
		auto label{ member.name == "value" ? TypeLabel<Value>() : PrettyName(member.name) };
		return DrawValue(ctx, label, member.value);
	} else {
		return DrawValue(ctx, TypeLabel<Value>(), value);
	}
}

} // namespace inspector

} // namespace ptgn::editor
