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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <functional>
#include <limits>
#ifndef MAGIC_ENUM_RANGE_MAX
#define MAGIC_ENUM_RANGE_MAX 512
#endif
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <optional>
#include <ratio>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "editor/editor.h"
#include "editor/editor_context.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/input/key.h"
#include "core/math/angle.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/time.h"
#include "core/util/type_info.h"
#include "panels/content_browser.h"
#include "platform/platform.h"
#include "renderer/text/font_style.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/text/font_system.h"

namespace ptgn {

struct ComponentReflectionVisitor;

} // namespace ptgn

namespace ptgn::editor {

namespace inspector {

inline constexpr float kDefaultLabelWidth{ 220.0f };
inline constexpr float kLabelValueSpacing{ 12.0f };
inline constexpr float kInspectorMinLineWidth{ 1.0f };
inline constexpr float kInspectorScalarDragSpeed{ 0.1f };
inline constexpr float kInspectorPositionDragSpeed{ 0.25f };
inline constexpr float kInspectorSizeDragSpeed{ 0.25f };

using ReflectedValueVisitCallback =
	void (*)(void* value, ::ptgn::ComponentReflectionVisitor visitor);

bool DrawReflectedContents(
	EditorContext& ctx,
	std::string_view label,
	void* value,
	ReflectedValueVisitCallback visit
);

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

inline float& PropertyLabelOffsetCompensation() {
	static float offset{ 0.0f };
	return offset;
}

class ScopedPropertyLabelOffset {
public:
	explicit ScopedPropertyLabelOffset(float offset) : offset_{ offset } {
		PropertyLabelOffsetCompensation() += offset_;
	}

	~ScopedPropertyLabelOffset() {
		PropertyLabelOffsetCompensation() -= offset_;
	}

	ScopedPropertyLabelOffset(const ScopedPropertyLabelOffset&) = delete;
	ScopedPropertyLabelOffset& operator=(const ScopedPropertyLabelOffset&) = delete;

private:
	float offset_{ 0.0f };
};

inline float GetPropertyLabelWidth() {
	auto& stack{ AutoLabelWidthStack() };

	if (stack.empty()) {
		return kDefaultLabelWidth;
	}

	return stack.back()->width;
}

inline void MeasurePropertyLabel(
	std::string_view label,
	float label_x,
	float leading_width = 0.0f
) {
	auto& stack{ AutoLabelWidthStack() };

	if (stack.empty()) {
		return;
	}

	auto& data{ *stack.back() };

	(void)label_x;
	auto text_width{ ImGui::CalcTextSize(label.data(), label.data() + label.size()).x };
	auto width{
		leading_width +
		text_width +
		ImGui::GetStyle().FramePadding.x * 2.0f +
		kLabelValueSpacing
	};

	data.measured_width = std::max(data.measured_width, width);
}

class AutoLabelWidthScope {
public:
	explicit AutoLabelWidthScope(std::string_view label) {
		ImGui::PushID(label.data(), label.data() + label.size());
		id_ = ImGui::GetID("##auto_label_width");

		auto& data{ AutoLabelWidths()[id_] };
		data.start_x =
			ImGui::GetCursorPosX() -
			PropertyLabelOffsetCompensation();
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

	/// @brief Line count in a multiline text box.
	std::size_t line_count{ 4 };

	/// @brief Allow the multiline input to be resized by dragging its bottom edge.
	bool resizable_y{ false };

	/// @brief Add a context menu option for opening a larger text editor.
	bool large_editor{ false };

	bool default_open{ true };
	bool read_only{ false };
	std::string_view array_item_name{ "Item" };
};

template <typename T>
inline const FieldOptions kDefaultFieldOptions{};

template <typename T>
inline const FieldOptions kDefaultFieldOptions<std::optional<T>>{ kDefaultFieldOptions<T> };

template <>
inline const FieldOptions kDefaultFieldOptions<float>{
	.speed	= kInspectorScalarDragSpeed,
	.format = "%.3f",
};

template <>
inline const FieldOptions kDefaultFieldOptions<int>{
	.speed	= 1.0f,
	.format = "%d",
};

template <>
inline const FieldOptions kDefaultFieldOptions<std::int64_t>{
	.speed	= 1.0f,
	.format = "%lld",
};

template <>
inline const FieldOptions kDefaultFieldOptions<std::size_t>{
	.speed	= 1.0f,
	.format = "%llu",
};

template <>
inline const FieldOptions kDefaultFieldOptions<V2_float>{
	.speed	= kInspectorPositionDragSpeed,
	.format = "%.3f",
};

template <>
inline const FieldOptions kDefaultFieldOptions<V2_int>{
	.speed	= 1.0f,
	.format = "%d",
};

template <>
inline const FieldOptions kDefaultFieldOptions<Degrees>{
	.speed	= 1.0f,
	.format = "%.1f deg",
};

template <>
inline const FieldOptions kDefaultFieldOptions<Radians>{
	.speed	= 1.0f,
	.format = "%.1f deg",
};

template <typename Rep, typename Period>
inline const FieldOptions kDefaultFieldOptions<std::chrono::duration<Rep, Period>>{
	.speed = std::floating_point<Rep> ? 0.01f : 1.0f,
};

template <>
inline const FieldOptions kDefaultFieldOptions<Matrix4>{
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
		std::integral<Value> ||
		std::same_as<Value, float> ||
		DurationType<Value> ||
		std::same_as<Value, std::string> ||
		std::same_as<Value, Color> ||
		std::same_as<Value, FillStyle> ||
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
	while (!name.empty() && (name.back() == '_' || name.back() == ']')) {
		name.remove_suffix(1);
	}

	std::string result;
	result.reserve(name.size() + 4);

	bool capitalize{ true };

	for (std::size_t i{ 0 }; i < name.size(); ++i) {
		const char c{ name[i] };

		if (c == '_') {
			result.push_back(' ');
			capitalize = true;
			continue;
		}

		if (!result.empty() && std::isupper(static_cast<unsigned char>(c)) != 0) {
			const char previous{ i > 0 ? name[i - 1] : '\0' };
			const char next{ i + 1 < name.size() ? name[i + 1] : '\0' };

			const bool previous_is_lower{
				std::islower(static_cast<unsigned char>(previous)) != 0
			};
			const bool acronym_boundary{
				std::isupper(static_cast<unsigned char>(previous)) != 0 &&
				std::islower(static_cast<unsigned char>(next)) != 0
			};

			if (previous_is_lower || acronym_boundary) {
				result.push_back(' ');
			}
		}

		if (capitalize) {
			result.push_back(
				static_cast<char>(
					std::toupper(static_cast<unsigned char>(c))
				)
			);
			capitalize = false;
		} else {
			result.push_back(c);
		}
	}

	return result;
}

template <typename T>
	requires std::is_enum_v<T>
std::string EnumLabel(T value) {
	auto name{ magic_enum::enum_name(value) };
	return name.empty() ? "Unknown" : PrettyName(name);
}

inline std::string KeyDisplayLabel(Key key) {
	auto name{ magic_enum::enum_name(key) };

	if (name.size() == 3 && name[0] == 'K' && name[1] == '_' && std::isdigit(static_cast<unsigned char>(name[2]))) {
		return std::string{ 1, name[2] };
	}

	return name.empty() ? "Unknown" : PrettyName(name);
}

inline std::string NormalizeKeySearchText(std::string_view text) {
	std::string result;
	result.reserve(text.size());

	for (char c : text) {
		if (std::isalnum(static_cast<unsigned char>(c))) {
			result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
		}
	}

	return result;
}

inline bool DrawKeyCombo(Key& value, const char* id = "##value") {
	struct SearchState {
		std::string filter{};
	};

	static std::unordered_map<ImGuiID, SearchState> states;
	const ImGuiID combo_id{ ImGui::GetID(id) };
	auto& state{ states[combo_id] };
	const std::string preview{ KeyDisplayLabel(value) };
	bool changed{ false };

	if (ImGui::BeginCombo(id, preview.c_str())) {
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##KeySearch", "Search keys...", &state.filter);
		ImGui::Separator();

		const std::string normalized_filter{ NormalizeKeySearchText(state.filter) };

		for (auto [candidate, name] : magic_enum::enum_entries<Key>()) {
			const std::string item_label{ KeyDisplayLabel(candidate) };
			const std::string normalized_name{ NormalizeKeySearchText(name) };
			const std::string normalized_label{ NormalizeKeySearchText(item_label) };

			if (!normalized_filter.empty() &&
			normalized_name.find(normalized_filter) == std::string::npos &&
			normalized_label.find(normalized_filter) == std::string::npos) {
				continue;
			}

			const bool selected{ candidate == value };
			if (ImGui::Selectable(item_label.c_str(), selected)) {
				value = candidate;
				changed = true;
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}

		ImGui::EndCombo();
	}

	return changed;
}

template <typename T>
std::string TypeLabel() {
	if constexpr (std::is_enum_v<T>) {
		auto name{ magic_enum::enum_type_name<T>() };
		if (!name.empty()) {
			return PrettyName(name);
		}
	}

	std::string label{ PrettyName(type_name_without_namespaces<T>()) };

	while (!label.empty() && label.back() == ']') {
		label.pop_back();
	}

	return label;
}

template <typename T>
std::string VariantTypeLabel() {
	using Value = std::remove_cvref_t<T>;

	if constexpr (std::same_as<Value, V2_float>) {
		return "Point";
	}

	std::string label{ TypeLabel<Value>() };

	if constexpr (std::default_initializable<Value> && ReflectedMembers<Value>) {
		Value value{};
		auto members{ ReflectMembers(value) };

		if constexpr (std::tuple_size_v<decltype(members)> == 2) {
			if (
				label.find('>') != std::string::npos &&
				std::get<0>(members).name == "min" &&
				std::get<1>(members).name == "max"
			) {
				return "Range";
			}
		}
	}

	while (!label.empty() && (label.back() == ']' || label.back() == '>')) {
		label.pop_back();
	}

	return label;
}

inline bool HasBounds(const FieldOptions& options) {
	return options.min < options.max;
}

inline float GetPropertyValueX(float fallback_start_x) {
	auto& stack{ AutoLabelWidthStack() };

	if (stack.empty()) {
		return fallback_start_x - PropertyLabelOffsetCompensation() + kDefaultLabelWidth;
	}

	return stack.back()->start_x + stack.back()->width;
}

inline ImVec2 GetResizableMultilineSize(
	const char* id,
	ImVec2 default_size
) {
	const ImVec2 cursor_position{
		ImGui::GetCursorScreenPos()
	};

	ImGui::PushStyleVar(
		ImGuiStyleVar_FramePadding,
		ImVec2{ 0.0f, 0.0f }
	);

	ImGui::BeginChild(
		id,
		default_size,
		ImGuiChildFlags_ResizeY |
			ImGuiChildFlags_FrameStyle |
			ImGuiChildFlags_Borders
	);

	const ImVec2 actual_size{
		ImGui::GetWindowSize()
	};

	ImGui::EndChild();
	ImGui::PopStyleVar();

	// Draw the actual input over the temporary resizable child.
	ImGui::SetCursorScreenPos(cursor_position);

	return actual_size;
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

inline bool DrawWHValue(
	std::string_view label,
	V2_float& value,
	float speed = 0.1f,
	float minimum = 0.0f,
	float maximum = 0.0f,
	const char* format = "%.3f",
	ImGuiSliderFlags flags = ImGuiSliderFlags_None,
	bool disabled = false
) {
	ImGui::PushID(&value);

	const bool changed{ DrawPropertyRow(label, [&]() {
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float width{
			std::max(
				1.0f,
				(ImGui::GetContentRegionAvail().x - spacing) * 0.5f
			)
		};
		const std::string width_format{
			std::string{ "W: " } + format
		};
		const std::string height_format{
			std::string{ "H: " } + format
		};

		return DrawDisabledIf(
			disabled || IsReadOnly(),
			[&]() {
				bool local_changed{ false };

				ImGui::SetNextItemWidth(width);
				local_changed |= ImGui::DragFloat(
					"##W",
					&value.x,
					speed,
					minimum,
					maximum,
					width_format.c_str(),
					flags
				);

				ImGui::SameLine(0.0f, spacing);

				ImGui::SetNextItemWidth(width);
				local_changed |= ImGui::DragFloat(
					"##H",
					&value.y,
					speed,
					minimum,
					maximum,
					height_format.c_str(),
					flags
				);

				return local_changed;
			}
		);
	}) };

	if (changed && minimum < maximum) {
		value = Clamp(value, minimum, maximum);
	}

	ImGui::PopID();

	return changed;
}

inline bool DrawWHValue(
	std::string_view label,
	V2_int& value,
	float speed = 1.0f,
	int minimum = 0,
	int maximum = 0,
	ImGuiSliderFlags flags = ImGuiSliderFlags_None,
	bool disabled = false
) {
	ImGui::PushID(&value);

	const bool changed{ DrawPropertyRow(label, [&]() {
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float width{
			std::max(
				1.0f,
				(ImGui::GetContentRegionAvail().x - spacing) * 0.5f
			)
		};

		return DrawDisabledIf(
			disabled || IsReadOnly(),
			[&]() {
				bool local_changed{ false };

				ImGui::SetNextItemWidth(width);
				local_changed |= ImGui::DragInt(
					"##W",
					&value.x,
					speed,
					minimum,
					maximum,
					"W: %d",
					flags
				);

				ImGui::SameLine(0.0f, spacing);

				ImGui::SetNextItemWidth(width);
				local_changed |= ImGui::DragInt(
					"##H",
					&value.y,
					speed,
					minimum,
					maximum,
					"H: %d",
					flags
				);

				return local_changed;
			}
		);
	}) };

	if (changed && minimum < maximum) {
		value = Clamp(value, minimum, maximum);
	}

	ImGui::PopID();

	return changed;
}

inline bool DrawRValue(
	std::string_view label,
	float& value,
	float speed = 0.1f,
	float minimum = 0.0f,
	float maximum = 0.0f,
	const char* format = "%.3f",
	ImGuiSliderFlags flags = ImGuiSliderFlags_None,
	bool disabled = false
) {
	ImGui::PushID(&value);

	const std::string radius_format{
		std::string{ "R: " } + format
	};

	const bool changed{
		DrawPropertyRow(
			label,
			[&]() {
				return DrawDisabledIf(
					disabled || IsReadOnly(),
					[&]() {
						ImGui::SetNextItemWidth(-FLT_MIN);

						return ImGui::DragFloat(
							"##R",
							&value,
							speed,
							minimum,
							maximum,
							radius_format.c_str(),
							flags
						);
					}
				);
			}
		)
	};

	if (changed && minimum < maximum) {
		value = std::clamp(value, minimum, maximum);
	}

	ImGui::PopID();

	return changed;
}

inline bool DrawOptionalWHValue(
	std::string_view label,
	std::optional<V2_float>& value,
	float speed = 0.1f,
	float minimum = 0.0f,
	float maximum = 0.0f,
	const char* format = "%.3f",
	ImGuiSliderFlags flags = ImGuiSliderFlags_None,
	bool disabled = false,
	std::optional<V2_float> enable_default = std::nullopt
) {
	ImGui::PushID(&value);

	bool enabled{ value.has_value() };
	V2_float displayed{ value.value_or(enable_default.value_or(V2_float{})) };

	const bool changed{
		DrawPropertyRow(
			label,
			[&]() {
				const bool read_only{
					disabled || IsReadOnly()
				};

				const float spacing{
					ImGui::GetStyle().ItemInnerSpacing.x
				};
				const float checkbox_width{
					ImGui::GetFrameHeight()
				};
				const float field_width{
					std::max(
						1.0f,
						(
							ImGui::GetContentRegionAvail().x -
							checkbox_width -
							spacing * 2.0f
						) *
							0.5f
					)
				};

				bool local_changed{ false };

				local_changed |= DrawDisabledIf(
					read_only,
					[&]() {
						return ImGui::Checkbox(
							"##Enabled",
							&enabled
						);
					}
				);

				ImGui::SameLine(
					0.0f,
					spacing
				);

				ImGui::BeginDisabled(
					!enabled || read_only
				);

				const std::string width_format{
					std::string{ "W: " } + format
				};
				const std::string height_format{
					std::string{ "H: " } + format
				};

				ImGui::SetNextItemWidth(
					field_width
				);

				local_changed |= ImGui::DragFloat(
					"##W",
					&displayed.x,
					speed,
					minimum,
					maximum,
					width_format.c_str(),
					flags
				);

				ImGui::SameLine(
					0.0f,
					spacing
				);

				ImGui::SetNextItemWidth(
					field_width
				);

				local_changed |= ImGui::DragFloat(
					"##H",
					&displayed.y,
					speed,
					minimum,
					maximum,
					height_format.c_str(),
					flags
				);

				ImGui::EndDisabled();

				return local_changed;
			}
		)
	};

	if (changed) {
		if (minimum < maximum) {
			displayed = Clamp(displayed, minimum, maximum);
		}

		if (enabled) {
			value = displayed;
		} else {
			value.reset();
		}
	}

	ImGui::PopID();

	return changed;
}

inline bool DrawString(
	std::string_view label,
	std::string& value,
	const FieldOptions& options
) {
	return DrawPropertyRow(
		label,
		[&]() {
			const bool read_only{
				IsReadOnly(options)
			};

			if (!options.multiline) {
				return DrawDisabledIf(
					read_only,
					[&]() {
						return ImGui::InputText(
							"##value",
							&value
						);
					}
				);
			}

			const std::string popup_name{
				std::string{ "Edit " } +
				std::string{ label } +
				"##LargeTextEditor"
			};

			const float default_height{
				ImGui::GetTextLineHeightWithSpacing() *
				static_cast<float>(
					std::max<std::size_t>(
						options.line_count,
						1
					)
				)
			};

			ImVec2 input_size{
				-FLT_MIN,
				default_height
			};

			bool open_large_editor{ false };

			// DrawPropertyRow() has already placed the cursor at the
			// normal beginning of the value column.
			const ImVec2 input_position{
				ImGui::GetCursorScreenPos()
			};

			if (options.large_editor) {
				const float button_size{
					ImGui::GetFrameHeight()
				};

				const float spacing{
					ImGui::GetStyle().ItemInnerSpacing.x
				};

				// Draw the button immediately before the value column
				// without shifting the multiline input.
				ImGui::SetCursorScreenPos(
					ImVec2{
						input_position.x -
							button_size -
							spacing,
						input_position.y
					}
				);

				if (
					ImGui::Button(
						"...##OpenLargeEditor",
						ImVec2{
							button_size,
							button_size
						}
					)
				) {
					open_large_editor = true;
				}

				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(
						"Open the large text editor."
					);
				}

				// Restore the regular value column position.
				ImGui::SetCursorScreenPos(
					input_position
				);
			}

			if (options.resizable_y) {
				input_size = GetResizableMultilineSize(
					"##resize",
					input_size
				);
			}

			constexpr ImGuiInputTextFlags input_flags{
				ImGuiInputTextFlags_AllowTabInput |
				ImGuiInputTextFlags_WordWrap
			};

			// The button or resize child may consume the item width
			// configured by DrawPropertyRow().
			ImGui::SetNextItemWidth(
				-FLT_MIN
			);

			bool changed{
				DrawDisabledIf(
					read_only,
					[&]() {
						return ImGui::InputTextMultiline(
							"##value",
							&value,
							input_size,
							input_flags
						);
					}
				)
			};

			// This must remain immediately after InputTextMultiline(),
			// because BeginPopupContextItem() acts on the last item.
			if (
				options.large_editor &&
				ImGui::BeginPopupContextItem(
					"##MultilineContext"
				)
			) {
				if (
					ImGui::MenuItem(
						"Edit in Large Window..."
					)
				) {
					open_large_editor = true;
				}

				ImGui::EndPopup();
			}

			// Both the button and context menu reach this path.
			if (open_large_editor) {
				ImGui::OpenPopup(
					popup_name.c_str()
				);
			}

			if (options.large_editor) {
				ImGui::SetNextWindowSize(
					ImVec2{
						700.0f,
						500.0f
					},
					ImGuiCond_Appearing
				);

				ImGui::SetNextWindowSizeConstraints(
					ImVec2{
						350.0f,
						250.0f
					},
					ImVec2{
						FLT_MAX,
						FLT_MAX
					}
				);

				bool popup_open{ true };

				if (
					ImGui::BeginPopupModal(
						popup_name.c_str(),
						&popup_open,
						ImGuiWindowFlags_NoSavedSettings
					)
				) {
					const bool close_requested{
						!popup_open ||
						ImGui::IsKeyPressed(
							ImGuiKey_Escape,
							false
						)
					};

					if (close_requested) {
						ImGui::CloseCurrentPopup();
					} else {
						const ImVec2 available{
							ImGui::GetContentRegionAvail()
						};

						const ImVec2 editor_size{
							std::max(
								1.0f,
								available.x
							),
							std::max(
								100.0f,
								available.y
							)
						};

						changed |= DrawDisabledIf(
							read_only,
							[&]() {
								return ImGui::InputTextMultiline(
									"##LargeValue",
									&value,
									editor_size,
									input_flags
								);
							}
						);
					}

					ImGui::EndPopup();
				}
			}

			return changed;
		}
	);
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

		auto draw_member = [&](auto&& member) {
			ImGui::PushID(member.name.data(), member.name.data() + member.name.size());
			changed |= DrawValue(ctx, PrettyName(member.name), member.value);
			ImGui::PopID();
		};

		std::apply(
			[&](auto&&... member) {
				(draw_member(member), ...);
			},
			members
		);
	}

	if constexpr (ReflectedReadOnlyMembers<T>) {
		if (ctx.local.settings.show_read_only_inspector_data) {
			auto members{ ReflectReadOnlyMembers(value) };

			auto draw_member = [&](auto&& member) {
				ImGui::PushID(member.name.data(), member.name.data() + member.name.size());
				DrawReadOnlyValue(ctx, PrettyName(member.name), member.value);
				ImGui::PopID();
			};

			std::apply(
				[&](auto&&... member) {
					(draw_member(member), ...);
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
	using Value = std::remove_cvref_t<T>;
	if constexpr (std::same_as<Value, AssetKey>) {
		return assets.HasCatalogAsset(key) || assets.Has(key);
	} else if constexpr (std::same_as<Value, ShaderKey>) {
		return assets.HasCatalogAsset(key, Value::kind) ||
			assets.GetEngineShaderSource(key).has_value() || assets.Has(key);
	} else {
		return assets.HasCatalogAsset(key, Value::kind) || assets.Has(key);
	}
}

template <AssetKeyType T>
bool DrawAssetKeyInline(
	EditorContext& ctx,
	T& value,
	const FieldOptions& options,
	std::string_view hint = {},
	bool input_enabled = true
) {
	using Value = std::remove_cvref_t<T>;
	const bool read_only{ IsReadOnly(options) };
	const bool input_disabled{ read_only || !input_enabled };
	constexpr bool shader_key{ std::same_as<Value, ShaderKey> };

	std::string resolved_hint{ hint };
	std::string displayed_value{ value.value };
	if constexpr (std::same_as<Value, FontKey>) {
		if (value.value == kDefaultFont) {
			displayed_value = "Default Font";
		}
	}

	float shader_button_width{ 0.0f };
	if constexpr (shader_key) {
		shader_button_width =
			ImGui::CalcTextSize("[..]").x + ImGui::GetStyle().FramePadding.x * 2.0f;
		const float input_width{
			std::max(
				1.0f,
				ImGui::GetContentRegionAvail().x - shader_button_width -
					ImGui::GetStyle().ItemInnerSpacing.x
			)
		};
		ImGui::SetNextItemWidth(input_width);
	}

	bool input_changed{ DrawDisabledIf(input_disabled, [&]() {
		return ImGui::InputTextWithHint(
			"##value",
			resolved_hint.c_str(),
			&displayed_value
		);
	}) };

	if (input_changed) {
		if constexpr (std::same_as<Value, FontKey>) {
			value.value = displayed_value == "Default Font"
				? std::string{ kDefaultFont }
				: displayed_value;
		} else {
			value.value = displayed_value;
		}
	}
	bool changed{ input_changed };

	const ImVec2 input_min{ ImGui::GetItemRectMin() };
	const ImVec2 input_max{ ImGui::GetItemRectMax() };
	const bool input_hovered{
		ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)
	};

	if constexpr (shader_key) {
		if (input_hovered) {
			ImGui::SetTooltip(
				"Built-in engine shader keys start with '$', e.g. $color."
			);
		}
	}

	if (!read_only) {
		if constexpr (
			!std::same_as<Value, AssetKey> &&
			requires { Value::kind; }
		) {
			changed |= ptgn::editor::AcceptAssetKeyDragDrop(
				static_cast<AssetKey&>(value),
				Value::kind
			);
		} else {
			changed |= ptgn::editor::AcceptAssetKeyDragDrop(
				static_cast<AssetKey&>(value)
			);
		}
	}

	if constexpr (shader_key) {
		auto& assets{ ctx.editor.GetAssetManager() };
		const bool shader_exists{
			!value.value.empty() &&
			(assets.HasCatalogAsset(value, AssetKind::Shader) ||
			 assets.GetEngineShaderSource(value).has_value() ||
			 assets.Has(value))
		};
		if (input_hovered && shader_exists &&
			ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
			ptgn::editor::RequestShaderEditorOpen(value);
		}

		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::BeginDisabled(!shader_exists);
		if (ImGui::Button("[..]##OpenShaderEditor", ImVec2{ shader_button_width, 0.0f })) {
			ptgn::editor::RequestShaderEditorOpen(value);
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip(
				shader_exists
					? "Open this shader in the shader editor."
					: "The shader asset does not exist."
			);
		}
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
		if constexpr (std::same_as<Value, AssetKey>) {
			ImGui::SetTooltip(
				"No asset exists with key \"%s\".",
				value.value.c_str()
			);
		} else {
			const auto kind_name{ magic_enum::enum_name(Value::kind) };
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

enum class DurationInputUnit {
	Nanoseconds,
	Microseconds,
	Milliseconds,
	Seconds,
	Minutes,
	Hours,
	Days,
	Weeks
};

struct ParsedInspectorDuration {
	long double count{ 0.0L };
	DurationInputUnit unit{ DurationInputUnit::Milliseconds };
};

[[nodiscard]] inline std::string TrimDurationInputText(std::string_view text) {
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
		text.remove_prefix(1);
	}

	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
		text.remove_suffix(1);
	}

	return std::string{ text };
}

[[nodiscard]] inline std::string NormalizeDurationInputUnit(std::string_view unit) {
	std::string normalized{ TrimDurationInputText(unit) };

	// Accept the common UTF-8 micro signs as aliases for ASCII 'u'.
	if (normalized.starts_with("\xC2\xB5") || normalized.starts_with("\xCE\xBC")) {
		normalized.replace(0, 2, "u");
	}

	std::ranges::transform(normalized, normalized.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});

	return normalized;
}

template <typename... Aliases>
[[nodiscard]] constexpr bool IsDurationInputUnit(std::string_view unit, Aliases... aliases) {
	return ((unit == aliases) || ...);
}

[[nodiscard]] inline std::optional<DurationInputUnit> ParseDurationInputUnit(
	std::string_view unit
) {
	const std::string normalized{ NormalizeDurationInputUnit(unit) };

	if (IsDurationInputUnit(
			normalized, "ns", "nsec", "nsecs", "nanosecond", "nanoseconds"
		)) {
		return DurationInputUnit::Nanoseconds;
	}

	if (IsDurationInputUnit(
			normalized, "us", "usec", "usecs", "microsecond", "microseconds"
		)) {
		return DurationInputUnit::Microseconds;
	}

	if (IsDurationInputUnit(
			normalized, "ms", "msec", "msecs", "millisecond", "milliseconds"
		)) {
		return DurationInputUnit::Milliseconds;
	}

	if (IsDurationInputUnit(normalized, "s", "sec", "secs", "second", "seconds")) {
		return DurationInputUnit::Seconds;
	}

	if (IsDurationInputUnit(normalized, "m", "min", "mins", "minute", "minutes")) {
		return DurationInputUnit::Minutes;
	}

	if (IsDurationInputUnit(normalized, "h", "hr", "hrs", "hour", "hours")) {
		return DurationInputUnit::Hours;
	}

	if (IsDurationInputUnit(normalized, "d", "day", "days")) {
		return DurationInputUnit::Days;
	}

	if (IsDurationInputUnit(normalized, "w", "wk", "wks", "week", "weeks")) {
		return DurationInputUnit::Weeks;
	}

	return std::nullopt;
}

[[nodiscard]] constexpr std::string_view DurationInputUnitSuffix(DurationInputUnit unit) {
	switch (unit) {
		case DurationInputUnit::Nanoseconds: return "ns";
		case DurationInputUnit::Microseconds: return "us";
		case DurationInputUnit::Milliseconds: return "ms";
		case DurationInputUnit::Seconds: return "s";
		case DurationInputUnit::Minutes: return "m";
		case DurationInputUnit::Hours: return "h";
		case DurationInputUnit::Days: return "d";
		case DurationInputUnit::Weeks: return "w";
	}

	return "ms";
}

[[nodiscard]] inline std::optional<ParsedInspectorDuration> ParseInspectorDuration(
	std::string_view text,
	DurationInputUnit default_unit = DurationInputUnit::Milliseconds
) {
	const std::string input{ TrimDurationInputText(text) };

	if (input.empty()) {
		return ParsedInspectorDuration{
			.count = 0.0L,
			.unit  = default_unit,
		};
	}

	char* end{ nullptr };
	const long double count{ std::strtold(input.c_str(), &end) };

	if (end == input.c_str() || !std::isfinite(count)) {
		return std::nullopt;
	}

	const std::string entered_suffix{ TrimDurationInputText(std::string_view{ end }) };
	const std::optional<DurationInputUnit> unit{
		entered_suffix.empty() ? std::optional<DurationInputUnit>{ default_unit }
						   : ParseDurationInputUnit(entered_suffix)
	};

	if (!unit) {
		return std::nullopt;
	}

	return ParsedInspectorDuration{
		.count = count,
		.unit  = *unit,
	};
}

template <DurationType T>
[[nodiscard]] std::optional<T> ConvertInspectorDuration(
	const ParsedInspectorDuration& parsed
) {
	using Duration         = std::remove_cvref_t<T>;
	using Rep              = typename Duration::rep;
	using Period           = typename Duration::period;
	using FloatingDuration = duration<long double, Period>;

	const auto convert = [&]<typename SourcePeriod>() {
		using SourceDuration = duration<long double, SourcePeriod>;
		return std::chrono::duration_cast<FloatingDuration>(
			SourceDuration{ parsed.count }
		).count();
	};

	long double count{ 0.0L };

	switch (parsed.unit) {
		case DurationInputUnit::Nanoseconds:
			count = convert.template operator()<nanoseconds::period>();
			break;
		case DurationInputUnit::Microseconds:
			count = convert.template operator()<microseconds::period>();
			break;
		case DurationInputUnit::Milliseconds:
			count = convert.template operator()<milliseconds::period>();
			break;
		case DurationInputUnit::Seconds:
			count = convert.template operator()<seconds::period>();
			break;
		case DurationInputUnit::Minutes:
			count = convert.template operator()<minutes::period>();
			break;
		case DurationInputUnit::Hours:
			count = convert.template operator()<hours::period>();
			break;
		case DurationInputUnit::Days:
			count = convert.template operator()<days::period>();
			break;
		case DurationInputUnit::Weeks:
			count = convert.template operator()<weeks::period>();
			break;
	}

	if (!std::isfinite(count) ||
		count < static_cast<long double>(std::numeric_limits<Rep>::lowest()) ||
		count > static_cast<long double>(std::numeric_limits<Rep>::max())) {
		return std::nullopt;
	}

	return Duration{ static_cast<Rep>(count) };
}

template <DurationType T>
[[nodiscard]] long double InspectorDurationCount(T value, DurationInputUnit unit) {
	const auto convert = [&]<DurationType Destination>() {
		using FloatingDestination = duration<long double, typename Destination::period>;
		return std::chrono::duration_cast<FloatingDestination>(value).count();
	};

	switch (unit) {
		case DurationInputUnit::Nanoseconds:
			return convert.template operator()<nanosecondsf>();
		case DurationInputUnit::Microseconds:
			return convert.template operator()<microsecondsf>();
		case DurationInputUnit::Milliseconds:
			return convert.template operator()<millisecondsf>();
		case DurationInputUnit::Seconds:
			return convert.template operator()<secondsf>();
		case DurationInputUnit::Minutes:
			return convert.template operator()<minutesf>();
		case DurationInputUnit::Hours:
			return convert.template operator()<hoursf>();
		case DurationInputUnit::Days:
			return convert.template operator()<daysf>();
		case DurationInputUnit::Weeks:
			return convert.template operator()<weeksf>();
	}

	return 0.0L;
}

template <DurationType T>
[[nodiscard]] std::string FormatInspectorDuration(T value, std::string_view suffix = {}) {
	std::string displayed_suffix{ suffix };
	std::optional<DurationInputUnit> unit;

	if (!displayed_suffix.empty()) {
		unit = ParseDurationInputUnit(displayed_suffix);
	}

	if (!unit) {
		displayed_suffix = std::string{ DurationUnit<T>() };
		unit             = ParseDurationInputUnit(displayed_suffix);
	}

	if (!unit) {
		unit = DurationInputUnit::Milliseconds;
	}

	displayed_suffix = DurationInputUnitSuffix(*unit);

	return std::format(
		"{:.6g}{}", InspectorDurationCount(value, *unit), displayed_suffix
	);
}

struct DurationTextEditState {
	std::string buffer{};
	std::string display_unit{};
	bool initialized{ false };
	bool was_active{ false };
};

inline std::unordered_map<ImGuiID, DurationTextEditState>& DurationTextEditStates() {
	static std::unordered_map<ImGuiID, DurationTextEditState> states;
	return states;
}

template <DurationType T>
bool DrawDurationTextInput(
	const char* label, T& value, float width, bool disabled, const char* tooltip,
	const FieldOptions* options = nullptr
) {
	using Duration = std::remove_cvref_t<T>;
	using Rep      = typename Duration::rep;

	const ImGuiID id{ ImGui::GetID(label) };
	auto& state{ DurationTextEditStates()[id] };

	if (!state.initialized) {
		state.display_unit = std::string{ DurationUnit<Duration>() };
		if (!ParseDurationInputUnit(state.display_unit)) {
			state.display_unit = "ms";
		}
		state.initialized = true;
	}

	if (!state.was_active) {
		state.buffer = FormatInspectorDuration(value, state.display_unit);
	}

	ImGui::SetNextItemWidth(width);
	const bool submitted{ DrawDisabledIf(disabled, [&]() {
		return ImGui::InputText(label, &state.buffer, ImGuiInputTextFlags_EnterReturnsTrue);
	}) };
	const bool active{ ImGui::IsItemActive() };
	const bool commit{ submitted || ImGui::IsItemDeactivatedAfterEdit() };
	bool changed{ false };

	if (commit) {
		const auto parsed{ ParseInspectorDuration(state.buffer) };

		if (parsed && parsed->count >= 0.0L) {
			if (auto updated{ ConvertInspectorDuration<Duration>(*parsed) }) {
				long double count{ static_cast<long double>(updated->count()) };

				if (options && HasBounds(*options)) {
					count = std::clamp(
						count, static_cast<long double>(options->min),
						static_cast<long double>(options->max)
					);
				}

				const Duration bounded{ static_cast<Rep>(count) };
				state.display_unit = std::string{ DurationInputUnitSuffix(parsed->unit) };

				if (bounded != value) {
					value   = bounded;
					changed = true;
				}
			}
		}

		state.buffer = FormatInspectorDuration(value, state.display_unit);
	}

	state.was_active = active;

	if (tooltip && *tooltip &&
		ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip("%s", tooltip);
	}

	return changed;
}

inline bool DrawDurationInput(
	const char* label, float& milliseconds, float width, const char* tooltip = nullptr
) {
	millisecondsf duration{ milliseconds };
	const bool changed{ DrawDurationTextInput(
		label, duration, width, false, tooltip
	) };

	if (changed) {
		milliseconds = duration.count();
	}

	return changed;
}

template <typename Rep, typename Period>
bool DrawDuration(
	std::string_view label, std::chrono::duration<Rep, Period>& value,
	const FieldOptions& options
) {
	return DrawPropertyRow(label, [&]() {
		return DrawDurationTextInput(
			"##value", value, -FLT_MIN, IsReadOnly(options), nullptr,
			std::addressof(options)
		);
	});
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

template <typename T>
inline constexpr bool kInspectorVectorType =
	std::same_as<T, V2_float> ||
	std::same_as<T, V3_float> ||
	std::same_as<T, V4_float> ||
	std::same_as<T, V2_int> ||
	std::same_as<T, V3_int> ||
	std::same_as<T, V4_int>;

template <typename T>
	requires kInspectorVectorType<std::remove_cvref_t<T>>
inline bool DrawVector(std::string_view label, T& value, const FieldOptions& options) {
	using Vector = std::remove_cvref_t<T>;
	using Scalar = std::remove_cvref_t<decltype(value.x)>;

	constexpr int component_count{
		requires(Vector vector) { vector.w; }
			? 4
			: requires(Vector vector) { vector.z; }
				  ? 3
				  : 2
	};

	return DrawPropertyRow(label, [&]() {
		std::array<Scalar, static_cast<std::size_t>(component_count)> values{};
		values[0] = value.x;
		values[1] = value.y;

		if constexpr (component_count >= 3) {
			values[2] = value.z;
		}
		if constexpr (component_count >= 4) {
			values[3] = value.w;
		}

		Scalar minimum{ static_cast<Scalar>(options.min) };
		Scalar maximum{ static_cast<Scalar>(options.max) };
		const void* minimum_value{ HasBounds(options) ? &minimum : nullptr };
		const void* maximum_value{ HasBounds(options) ? &maximum : nullptr };

		constexpr ImGuiDataType data_type{
			std::same_as<Scalar, float>
				? ImGuiDataType_Float
				: ImGuiDataType_S32
		};

		const char* base_format{
			options.format
				? options.format
				: std::same_as<Scalar, float>
					  ? "%.3f"
					  : "%d"
		};

		constexpr std::array<std::string_view, 4> axes{
			"X", "Y", "Z", "W"
		};

		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float available{ ImGui::GetContentRegionAvail().x };
		const float width{
			std::max(
				1.0f,
				(
					available -
					spacing * static_cast<float>(component_count - 1)
				) /
					static_cast<float>(component_count)
			)
		};

		bool changed{ false };

		DrawDisabledIf(IsReadOnly(options), [&]() {
			for (int i{ 0 }; i < component_count; ++i) {
				if (i > 0) {
					ImGui::SameLine(0.0f, spacing);
				}

				const std::string id{
					"##" + std::string{ axes[static_cast<std::size_t>(i)] }
				};
				const std::string format{
					std::string{ axes[static_cast<std::size_t>(i)] } +
					": " +
					base_format
				};

				ImGui::SetNextItemWidth(width);

				changed |= ImGui::DragScalar(
					id.c_str(),
					data_type,
					std::addressof(values[static_cast<std::size_t>(i)]),
					options.speed,
					minimum_value,
					maximum_value,
					format.c_str(),
					options.flags
				);
			}

			return changed;
		});

		if (!changed) {
			return false;
		}

		value.x = values[0];
		value.y = values[1];

		if constexpr (component_count >= 3) {
			value.z = values[2];
		}
		if constexpr (component_count >= 4) {
			value.w = values[3];
		}

		return true;
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

inline bool DrawFillStyle(
	EditorContext& ctx,
	std::string_view label,
	FillStyle& style,
	float maximum_line_width = 1000.0f
) {
	bool hollow{ style.GetLineWidth().has_value() };
	bool changed{
		DrawPropertyRow(
			label,
			[&]() {
				const char* preview{ hollow ? "Hollow" : "Solid" };
				bool local_changed{ false };

				if (ImGui::BeginCombo("##value", preview)) {
					if (ImGui::Selectable("Solid", !hollow)) {
						style = FillStyle{ Solid{} };
						hollow = false;
						local_changed = true;
					}

					if (ImGui::Selectable("Hollow", hollow)) {
						const float width{
							style.GetLineWidth().value_or(kInspectorMinLineWidth)
						};
						style = FillStyle{ width };
						hollow = true;
						local_changed = true;
					}

					ImGui::EndCombo();
				}

				return local_changed;
			}
		)
	};

	if (!hollow) {
		return changed;
	}

	float line_width{
		style.GetLineWidth().value_or(kInspectorMinLineWidth)
	};

	const float line_width_maximum{
		std::max(
			kInspectorMinLineWidth,
			maximum_line_width
		)
	};

	const float clamped_line_width{
		std::clamp(
			line_width,
			kInspectorMinLineWidth,
			line_width_maximum
		)
	};
	if (line_width != clamped_line_width) {
		line_width = clamped_line_width;
		style = FillStyle{ line_width };
		changed = true;
	}

	ImGui::Indent();
	{
		ScopedPropertyLabelOffset label_offset{
			ImGui::GetStyle().IndentSpacing
		};

		if (DrawValue(
				ctx,
				"Line Width",
				line_width,
				FieldOptions{
					.speed = kInspectorScalarDragSpeed,
					.min = kInspectorMinLineWidth,
					.max = line_width_maximum,
					.format = "%.2f",
					.flags = ImGuiSliderFlags_AlwaysClamp,
				}
			)) {
			style = FillStyle{
				std::clamp(
					line_width,
					kInspectorMinLineWidth,
					line_width_maximum
				)
			};
			changed = true;
		}
	}
	ImGui::Unindent();

	return changed;
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
			if constexpr (std::same_as<T, Key>) {
				return DrawKeyCombo(value);
			}

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
	std::string add_label{};
	bool default_open{ true };
	bool reorderable{ true };
	bool add_first{ false };
	bool copy_last_on_add{ false };
	bool reset_last_on_remove{ false };
	std::size_t minimum_items{ 0 };
};

template <typename T, typename Draw>
bool DrawVectorEditorItems(std::vector<T>& values, VectorOptions options, Draw&& draw) {
	ImGui::PushID(&values);

	bool changed{ false };
	std::optional<std::size_t> remove_index;
	std::optional<std::pair<std::size_t, std::size_t>> move;

	auto& style{ ImGui::GetStyle() };
	bool read_only{ IsReadOnly() };

	auto draw_add = [&](std::string_view prefix) {
		const std::string add_label{
			options.add_label.empty()
				? std::string{ prefix } + options.item_name
				: options.add_label
		};

		ImGui::BeginDisabled(read_only);
		if (ImGui::Button(add_label.c_str(), ImVec2{ -FLT_MIN, 0.0f })) {
			if (options.copy_last_on_add && !values.empty()) {
				values.push_back(values.back());
			} else {
				values.emplace_back();
			}
			changed = true;
		}
		ImGui::EndDisabled();
	};

	if (options.add_first) {
		draw_add("+ ");

		if (!values.empty()) {
			ImGui::SetCursorPosY(
				ImGui::GetCursorPosY() + style.FramePadding.y
			);
		}
	}

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

		ImGui::BeginDisabled(read_only || values.size() <= options.minimum_items);
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
		bool reset_last{ false };

		if constexpr (
			std::default_initializable<T> &&
			std::assignable_from<T&, T>
		) {
			reset_last =
				options.reset_last_on_remove &&
				values.size() == 1 &&
				*remove_index == 0;
		}

		if (reset_last) {
			values.front() = T{};
		} else {
			values.erase(
				values.begin() +
				static_cast<std::ptrdiff_t>(*remove_index)
			);
		}

		changed = true;
	}

	if (!options.add_first) {
		if (!values.empty()) {
			ImGui::SetCursorPosY(
				ImGui::GetCursorPosY() + style.FramePadding.y
			);
		}

		draw_add("Add ");
	}

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
		[&ctx]<typename TValue>(TValue& value, std::size_t) { return DrawDefaultContents(ctx, value); }
	);
}

template <typename T>
bool DrawVectorEditor(EditorContext& ctx, std::vector<T>& values, VectorOptions options = {}) {
	return DrawVectorEditor(
		values, std::move(options),
		[&ctx]<typename TValue>(TValue& value, std::size_t) { return DrawDefaultContents(ctx, value); }
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
		VariantTypeLabel<std::variant_alternative_t<I, Variant>>()...
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

	ImGui::PushID(&value);
	std::optional<std::size_t> requested_index;

	bool changed{ DrawPropertyRow(label, [&]() {
		return DrawDisabledIf(IsReadOnly(), [&]() {
			auto index{ value.index() };

			if (ImGui::BeginCombo("##value", names[index].c_str())) {
				for (auto i{ 0uz }; i < names.size(); ++i) {
					bool selected{ i == index };

					if (ImGui::Selectable(names[i].c_str(), selected)) {
						if (!selected) {
							requested_index = i;
						}
					}

					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			return requested_index.has_value();
		});
	}) };

	if (requested_index) {
		EmplaceVariant(
			value,
			*requested_index
		);
		ImGui::PopID();
		return true;
	}

	std::visit(
		[&]<typename TValue>(TValue& active) {
			using Value = std::remove_cvref_t<TValue>;

			if constexpr (!std::is_empty_v<Value> || !kHasNoReflectedContents<Value>) {
				ImGui::Indent();

				if (value_label.empty()) {
					changed |= DrawDefaultContents(ctx, active);
				} else {
					changed |= DrawValue(ctx, value_label, active, options);
				}

				ImGui::Unindent();
			}
		},
		value
	);

	ImGui::PopID();
	return changed;
}

inline bool DrawOptionalLabelRow(
	std::string_view label,
	bool& enabled,
	bool read_only
) {
	const float start_x{ ImGui::GetCursorPosX() };

	const bool changed{
		DrawDisabledIf(
			read_only,
			[&]() {
				return ImGui::Checkbox(
					"##enabled",
					&enabled
				);
			}
		)
	};

	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(
		label.data(),
		label.data() + label.size()
	);
	MeasurePropertyLabel(
		label,
		start_x,
		ImGui::GetFrameHeight() +
			ImGui::GetStyle().ItemSpacing.x
	);

	return changed;
}

template <typename Draw>
bool DrawOptionalPropertyRow(
	std::string_view label,
	bool& enabled,
	bool read_only,
	Draw&& draw_value
) {
	const float start_x{ ImGui::GetCursorPosX() };

	bool changed{
		DrawDisabledIf(
			read_only,
			[&]() {
				return ImGui::Checkbox(
					"##enabled",
					&enabled
				);
			}
		)
	};

	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(
		label.data(),
		label.data() + label.size()
	);
	MeasurePropertyLabel(
		label,
		start_x,
		ImGui::GetFrameHeight() +
			ImGui::GetStyle().ItemSpacing.x
	);
	ImGui::SameLine();
	ImGui::SetCursorPosX(
		GetPropertyValueX(start_x)
	);
	ImGui::SetNextItemWidth(-FLT_MIN);

	changed |= std::invoke(
		std::forward<Draw>(draw_value)
	);

	return changed;
}

inline bool DrawOptionalBool(std::string_view label, std::optional<bool>& value) {
	ImGui::PushID(&value);

	bool enabled{ value.has_value() };
	bool displayed{ value.value_or(false) };
	const bool read_only{ IsReadOnly() };

	bool changed{
		DrawOptionalPropertyRow(
			label,
			enabled,
			read_only,
			[&]() {
				return DrawDisabledIf(
					!enabled || read_only,
					[&]() {
						return ImGui::Checkbox(
							"##value",
							&displayed
						);
					}
				);
			}
		)
	};

	if (enabled) {
		if (!value.has_value() || value.value() != displayed) {
			value = displayed;
			changed = true;
		}
	} else if (value.has_value()) {
		value.reset();
		changed = true;
	}

	ImGui::PopID();
	return changed;
}

template <typename T>
	requires std::is_enum_v<T>
bool DrawOptionalEnum(std::string_view label, std::optional<T>& value) {
	ImGui::PushID(&value);

	bool enabled{ value.has_value() };
	const bool read_only{ IsReadOnly() };

	if (enabled && !value.has_value()) {
		value = magic_enum::enum_values<T>().front();
	}

	T displayed{
		value.value_or(
			magic_enum::enum_values<T>().front()
		)
	};

	bool changed{
		DrawOptionalPropertyRow(
			label,
			enabled,
			read_only,
			[&]() {
				return DrawDisabledIf(
					!enabled || read_only,
					[&]() {
						if constexpr (std::same_as<T, Key>) {
							return DrawKeyCombo(displayed);
						}

						bool local_changed{ false };
						auto preview{ EnumLabel(displayed) };

						if (ImGui::BeginCombo("##value", preview.c_str())) {
							for (auto candidate : magic_enum::enum_values<T>()) {
								auto item_label{ EnumLabel(candidate) };
								const bool selected{ displayed == candidate };

								if (ImGui::Selectable(item_label.c_str(), selected)) {
									displayed = candidate;
									local_changed = true;
								}

								if (selected) {
									ImGui::SetItemDefaultFocus();
								}
							}

							ImGui::EndCombo();
						}

						return local_changed;
					}
				);
			}
		)
	};

	if (enabled) {
		if (!value.has_value() || value.value() != displayed) {
			value = displayed;
			changed = true;
		}
	} else if (value.has_value()) {
		value.reset();
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
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("Unset");
}

template <typename Rep, typename Period>
bool DrawDurationInlineValue(
	std::chrono::duration<Rep, Period>& value, const FieldOptions& options
) {
	return DrawDurationTextInput(
		"##value", value, -FLT_MIN, IsReadOnly(options), nullptr,
		std::addressof(options)
	);
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
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float available{ ImGui::GetContentRegionAvail().x };
		const float width{ std::max(1.0f, (available - spacing) * 0.5f) };
		const float minimum{ static_cast<float>(options.min) };
		const float maximum{ static_cast<float>(options.max) };
		const std::string x_format{
			std::string{ "X: " } + (options.format ? options.format : "%.3f")
		};
		const std::string y_format{
			std::string{ "Y: " } + (options.format ? options.format : "%.3f")
		};

		return DrawDisabledIf(IsReadOnly(options), [&]() {
			bool changed{ false };
			ImGui::SetNextItemWidth(width);
			changed |= ImGui::DragFloat(
				"##X",
				&value.x,
				options.speed,
				HasBounds(options) ? minimum : 0.0f,
				HasBounds(options) ? maximum : 0.0f,
				x_format.c_str(),
				options.flags
			);
			ImGui::SameLine(0.0f, spacing);
			ImGui::SetNextItemWidth(width);
			changed |= ImGui::DragFloat(
				"##Y",
				&value.y,
				options.speed,
				HasBounds(options) ? minimum : 0.0f,
				HasBounds(options) ? maximum : 0.0f,
				y_format.c_str(),
				options.flags
			);
			return changed;
		});
	} else if constexpr (std::same_as<Value, V2_int>) {
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float available{ ImGui::GetContentRegionAvail().x };
		const float width{ std::max(1.0f, (available - spacing) * 0.5f) };
		const int minimum{ static_cast<int>(options.min) };
		const int maximum{ static_cast<int>(options.max) };
		const std::string x_format{
			std::string{ "X: " } + (options.format ? options.format : "%d")
		};
		const std::string y_format{
			std::string{ "Y: " } + (options.format ? options.format : "%d")
		};

		return DrawDisabledIf(IsReadOnly(options), [&]() {
			bool changed{ false };
			ImGui::SetNextItemWidth(width);
			changed |= ImGui::DragInt(
				"##X",
				&value.x,
				options.speed,
				HasBounds(options) ? minimum : 0,
				HasBounds(options) ? maximum : 0,
				x_format.c_str(),
				options.flags
			);
			ImGui::SameLine(0.0f, spacing);
			ImGui::SetNextItemWidth(width);
			changed |= ImGui::DragInt(
				"##Y",
				&value.y,
				options.speed,
				HasBounds(options) ? minimum : 0,
				HasBounds(options) ? maximum : 0,
				y_format.c_str(),
				options.flags
			);
			return changed;
		});
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

template <AssetKeyType T>
bool DrawOptionalAssetKeyInline(
	EditorContext& ctx,
	std::string_view label,
	std::optional<T>& value,
	FieldOptions options
) {
	ImGui::PushID(&value);

	bool enabled{ value.has_value() };
	const bool read_only{ IsReadOnly(options) };
	T displayed{ value.value_or(T{}) };

	bool value_changed{ false };
	bool changed{
		DrawOptionalPropertyRow(
			label,
			enabled,
			read_only,
			[&]() {
				value_changed = DrawAssetKeyInline(
					ctx,
					displayed,
					options,
					{},
					enabled
				);
				return value_changed;
			}
		)
	};

	if (value_changed && !enabled && !read_only) {
		enabled = true;
		changed = true;
	}

	if (enabled) {
		if (!value.has_value() || value.value() != displayed) {
			value = displayed;
			changed = true;
		}
	} else if (value.has_value()) {
		value.reset();
		changed = true;
	}

	ImGui::PopID();
	return changed;
}

template <typename T>
bool DrawOptionalInline(
	EditorContext& ctx,
	std::string_view label,
	std::optional<T>& value,
	FieldOptions options
) {
	if constexpr (AssetKeyType<std::remove_cvref_t<T>>) {
		return DrawOptionalAssetKeyInline(ctx, label, value, options);
	}

	ImGui::PushID(&value);

	bool enabled{ value.has_value() };
	const bool read_only{ IsReadOnly(options) };
	T displayed{ value.value_or(T{}) };

	bool changed{
		DrawOptionalPropertyRow(
			label,
			enabled,
			read_only,
			[&]() {
				if (!enabled) {
					ImGui::BeginDisabled();
					DrawUnsetOptionalInlineValue();
					ImGui::EndDisabled();
					return false;
				}

				return DrawDisabledIf(
					read_only,
					[&]() {
						return DrawOptionalInlineValue(
							ctx,
							displayed,
							options
						);
					}
				);
			}
		)
	};

	if (enabled) {
		if (!value.has_value() || value.value() != displayed) {
			value = displayed;
			changed = true;
		}
	} else if (value.has_value()) {
		value.reset();
		changed = true;
	}

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

		bool changed{
			DrawOptionalLabelRow(
				label,
				enabled,
				IsReadOnly(options)
			)
		};

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
			ScopedPropertyLabelOffset label_offset{
				ImGui::GetStyle().IndentSpacing
			};

			if constexpr (kIsVariant<Value>) {
				changed |= DrawVariant(ctx, "Variant", value.value(), "Value", options);
			} else if constexpr (
				ReflectedValue<Value> || ReflectedMembers<Value> ||
				ReflectedReadOnlyMembers<Value>
			) {
				changed |= DrawDefaultContents(ctx, value.value());
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
	} else if constexpr (std::signed_integral<Value>) {
		std::int64_t temporary{ static_cast<std::int64_t>(value) };

		if (!DrawInt64(label, temporary, options)) {
			return false;
		}

		value = static_cast<Value>(temporary);
		return true;
	} else if constexpr (std::unsigned_integral<Value>) {
		std::uint64_t temporary{ static_cast<std::uint64_t>(value) };

		if (!DrawUInt64(label, temporary, options)) {
			return false;
		}

		value = static_cast<Value>(temporary);
		return true;
	} else if constexpr (DurationType<Value>) {
		return DrawDuration(label, value, options);
	} else if constexpr (AssetKeyType<Value>) {
		return DrawAssetKey(ctx, label, value, options);
	} else if constexpr (std::same_as<Value, std::string>) {
		return DrawString(label, value, options);
	} else if constexpr (std::same_as<Value, Color>) {
		return DrawColor(label, value);
	} else if constexpr (std::same_as<Value, FillStyle>) {
		return DrawFillStyle(ctx, label, value);
	} else if constexpr (kInspectorVectorType<Value>) {
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
			changed = DrawDefaultContents(ctx, value);
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

	if constexpr (std::same_as<Value, FillStyle>) {
		return DrawValue(ctx, "Style", value);
	} else if constexpr (AssetKeyType<Value>) {
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
