#pragma once

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
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
#include <utility>
#include <variant>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/angle.h"
#include "core/math/vector2.h"
#include "core/util/type_info.h"
#include "renderer/text/font_style.h"

namespace ptgn::editor::inspector {

inline constexpr float kLabelWidth{ 105.0f };

struct FieldOptions {
	float speed{ 0.1f };
	double min{ 0.0 };
	double max{ 0.0 };
	const char* format{ nullptr };
	ImGuiSliderFlags flags{ ImGuiSliderFlags_None };
	bool multiline{ false };
};

template <typename T>
inline constexpr FieldOptions kDefaultFieldOptions{};

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
std::string TypeLabel() {
	return PrettyName(type_name_without_namespaces<T>());
}

inline bool HasBounds(const FieldOptions& options) {
	return options.min < options.max;
}

template <typename F>
bool DrawPropertyRow(std::string_view label, F&& draw) {
	auto id{ std::string{ label } };
	float start_x{ ImGui::GetCursorPosX() };

	ImGui::PushID(id.c_str());
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label.data(), label.data() + label.size());
	ImGui::SameLine();
	ImGui::SetCursorPosX(start_x + kLabelWidth);
	ImGui::SetNextItemWidth(-FLT_MIN);
	bool changed{ std::invoke(std::forward<F>(draw)) };
	ImGui::PopID();

	return changed;
}

template <typename T>
concept ReflectedMembers = requires(T& value) { ReflectMembers(value); };

template <typename T>
concept ReflectedValue = requires(T& value) { ReflectValue(value); };

template <typename T>
struct IsVector : std::false_type {};

template <typename T, typename Allocator>
struct IsVector<std::vector<T, Allocator>> : std::true_type {};

template <typename T>
inline constexpr bool kIsVector{ IsVector<T>::value };

template <typename T>
struct IsVariant : std::false_type {};

template <typename... T>
struct IsVariant<std::variant<T...>> : std::true_type {};

template <typename T>
inline constexpr bool kIsVariant{ IsVariant<T>::value };

template <typename T>
bool DrawValue(
	std::string_view label, T& value,
	FieldOptions options = kDefaultFieldOptions<std::remove_cvref_t<T>>
);

template <typename T>
bool DrawDefaultContents(T& value);

template <typename T>
struct Contents {
	static bool Draw(T& value) {
		return DrawDefaultContents(value);
	}
};

template <typename T>
bool DrawContents(T& value) {
	return Contents<std::remove_cvref_t<T>>::Draw(value);
}

template <typename T>
bool DrawMembers(T& value) {
	auto members{ ReflectMembers(value) };
	bool changed{ false };

	std::apply(
		[&]<typename... TMember>(TMember&&... member) {
			((changed |= DrawValue(PrettyName(member.name), member.value)), ...);
		},
		members
	);

	return changed;
}

inline bool DrawFloat(std::string_view label, float& value, const FieldOptions& options) {
	return DrawPropertyRow(label, [&]() {
		float min{ static_cast<float>(options.min) };
		float max{ static_cast<float>(options.max) };

		return ImGui::DragFloat(
			"##value", &value, options.speed, HasBounds(options) ? min : 0.0f,
			HasBounds(options) ? max : 0.0f, options.format ? options.format : "%.3f", options.flags
		);
	});
}

inline bool DrawInt(std::string_view label, int& value, const FieldOptions& options) {
	return DrawPropertyRow(label, [&]() {
		int min{ static_cast<int>(options.min) };
		int max{ static_cast<int>(options.max) };

		return ImGui::DragInt(
			"##value", &value, options.speed, HasBounds(options) ? min : 0,
			HasBounds(options) ? max : 0, options.format ? options.format : "%d", options.flags
		);
	});
}

inline bool DrawSize(std::string_view label, std::size_t& value, const FieldOptions& options) {
	return DrawPropertyRow(label, [&]() {
		std::uint64_t temporary{ value };
		std::uint64_t min{ static_cast<std::uint64_t>(std::max(0.0, options.min)) };
		std::uint64_t max{ static_cast<std::uint64_t>(std::max(0.0, options.max)) };
		bool changed{ ImGui::DragScalar(
			"##value", ImGuiDataType_U64, &temporary, options.speed,
			HasBounds(options) ? &min : nullptr, HasBounds(options) ? &max : nullptr,
			options.format ? options.format : "%llu", options.flags
		) };

		if (changed) {
			value = static_cast<std::size_t>(temporary);
		}
		return changed;
	});
}

inline bool DrawVector(std::string_view label, V2_float& value, const FieldOptions& options) {
	return DrawPropertyRow(label, [&]() {
		float values[2]{ value.x, value.y };
		float min{ static_cast<float>(options.min) };
		float max{ static_cast<float>(options.max) };
		bool changed{ ImGui::DragFloat2(
			"##value", values, options.speed, HasBounds(options) ? min : 0.0f,
			HasBounds(options) ? max : 0.0f, options.format ? options.format : "%.3f", options.flags
		) };

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
		bool changed{ ImGui::DragInt2(
			"##value", values, options.speed, HasBounds(options) ? min : 0,
			HasBounds(options) ? max : 0, options.format ? options.format : "%d", options.flags
		) };

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

		bool changed{ ImGui::ColorEdit4(
			"##value", rgba,
			ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_AlphaBar |
				ImGuiColorEditFlags_AlphaPreviewHalf
		) };

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

inline std::string FontStylePreview(FontStyle style) {
	if (style == FontStyle::Normal) {
		return "Normal";
	}

	std::string result;

	auto append = [&](FontStyle flag, std::string_view name) {
		if (!HasFontFlag(style, flag)) {
			return;
		}
		if (!result.empty()) {
			result += " | ";
		}
		result += name;
	};

	append(FontStyle::Bold, "Bold");
	append(FontStyle::Italic, "Italic");
	append(FontStyle::Underline, "Underline");
	append(FontStyle::Strikethrough, "Strikethrough");

	return result.empty() ? "Unknown" : result;
}

inline bool DrawFontStyle(std::string_view label, FontStyle& value) {
	return DrawPropertyRow(label, [&]() {
		auto preview{ FontStylePreview(value) };
		bool changed{ false };

		if (ImGui::BeginCombo("##value", preview.c_str())) {
			for (auto [flag, name] : std::array{
					 std::pair{ FontStyle::Bold, "Bold" },
					 std::pair{ FontStyle::Italic, "Italic" },
					 std::pair{ FontStyle::Underline, "Underline" },
					 std::pair{ FontStyle::Strikethrough, "Strikethrough" },
				 }) {
				bool enabled{ HasFontFlag(value, flag) };
				if (ImGui::Selectable(name, enabled, ImGuiSelectableFlags_DontClosePopups)) {
					value	= SetFontFlag(value, flag, !enabled);
					changed = true;
				}
			}
			ImGui::EndCombo();
		}

		return changed;
	});
}

template <typename T>
	requires std::is_enum_v<T>
bool DrawEnum(std::string_view label, T& value) {
	return DrawPropertyRow(label, [&]() {
		auto current_name{ magic_enum::enum_name(value) };
		std::string preview{ current_name.empty() ? "Unknown" : current_name };
		bool changed{ false };

		if (ImGui::BeginCombo("##value", preview.c_str())) {
			for (T candidate : magic_enum::enum_values<T>()) {
				auto candidate_name{ magic_enum::enum_name(candidate) };
				bool selected{ candidate == value };
				if (ImGui::Selectable(candidate_name.data(), selected)) {
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
}

struct VectorOptions {
	std::string item_name{ "Item" };
	bool default_open{ true };
	bool reorderable{ true };
};

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

	bool changed{ false };
	std::optional<std::size_t> remove_index;
	std::optional<std::pair<std::size_t, std::size_t>> move;

	for (auto i{ 0uz }; i < values.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));

		auto item_label{ options.item_name + " " + std::to_string(i + 1) };
		bool item_open{ ImGui::TreeNodeEx(
			"##item", ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen, "%s",
			item_label.c_str()
		) };

		if (options.reorderable) {
			ImGui::SameLine();
			if (ImGui::ArrowButton("##up", ImGuiDir_Up) && i > 0) {
				move = std::pair{ i, i - 1 };
			}
			ImGui::SameLine();
			if (ImGui::ArrowButton("##down", ImGuiDir_Down) && i + 1 < values.size()) {
				move = std::pair{ i, i + 1 };
			}
		}

		ImGui::SameLine();
		if (ImGui::SmallButton("X")) {
			remove_index = i;
		}

		if (item_open) {
			changed |= std::invoke(draw, values[i], i);
			ImGui::TreePop();
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

	auto add_label{ "Add " + options.item_name };
	if (ImGui::Button(add_label.c_str(), ImVec2{ -FLT_MIN, 0.0f })) {
		values.emplace_back();
		changed = true;
	}

	ImGui::TreePop();
	return changed;
}

template <typename T>
bool DrawVectorEditor(std::string_view label, std::vector<T>& values, VectorOptions options = {}) {
	return DrawVectorEditor(
		label, values, std::move(options),
		[]<typename TValue>(TValue& value, std::size_t) { return DrawContents(value); }
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
inline constexpr bool kHasNoReflectedMembers = []() {
	if constexpr (ReflectedMembers<T>) {
		using Tuple = decltype(ReflectMembers(std::declval<T&>()));
		return std::tuple_size_v<Tuple> == 0;
	} else {
		return false;
	}
}();

template <typename... T>
bool DrawVariant(std::string_view label, std::variant<T...>& value) {
	using Variant = std::variant<T...>;
	static auto names{ VariantNames<Variant>(std::index_sequence_for<T...>{}) };

	bool changed{ DrawPropertyRow(label, [&]() {
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
	}) };

	std::visit(
		[&]<typename TValue>(TValue& active) {
			if constexpr (!kHasNoReflectedMembers<TValue>) {
				ImGui::Indent();
				changed |= DrawContents(active);
				ImGui::Unindent();
			}
		},
		value
	);

	return changed;
}

template <typename T>
bool DrawValue(std::string_view label, T& value, FieldOptions options) {
	using Value = std::remove_cvref_t<T>;

	if constexpr (std::same_as<Value, bool>) {
		return DrawPropertyRow(label, [&]() { return ImGui::Checkbox("##value", &value); });
	} else if constexpr (std::same_as<Value, float>) {
		return DrawFloat(label, value, options);
	} else if constexpr (std::same_as<Value, int>) {
		return DrawInt(label, value, options);
	} else if constexpr (std::same_as<Value, std::size_t>) {
		return DrawSize(label, value, options);
	} else if constexpr (std::same_as<Value, std::string>) {
		return DrawPropertyRow(label, [&]() {
			if (options.multiline) {
				return ImGui::InputTextMultiline(
					"##value", &value,
					ImVec2{ -FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * 4.0f }
				);
			}
			return ImGui::InputText("##value", &value);
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
	} else if constexpr (kIsVector<Value>) {
		using Element = typename Value::value_type;
		return DrawVectorEditor(
			label, value,
			VectorOptions{
				.item_name = TypeLabel<Element>(),
			}
		);
	} else if constexpr (kIsVariant<Value>) {
		return DrawVariant(label, value);
	} else if constexpr (ReflectedValue<Value>) {
		auto member{ ReflectValue(value) };
		return DrawValue(label, member.value, options);
	} else if constexpr (ReflectedMembers<Value>) {
		auto title{ std::string{ label } };
		bool changed{ false };
		if (ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth)) {
			changed = DrawContents(value);
			ImGui::TreePop();
		}
		return changed;
	} else {
		static_assert(std::is_same_v<Value, void>, "No inspector drawer exists for this type");
	}
}

template <typename T>
bool DrawDefaultContents(T& value) {
	using Value = std::remove_cvref_t<T>;

	if constexpr (ReflectedMembers<Value>) {
		return DrawMembers(value);
	} else if constexpr (ReflectedValue<Value>) {
		auto member{ ReflectValue(value) };
		auto label{ member.name == "value" ? TypeLabel<Value>() : PrettyName(member.name) };
		return DrawValue(label, member.value);
	} else {
		return DrawValue(TypeLabel<Value>(), value);
	}
}

} // namespace ptgn::editor::inspector
