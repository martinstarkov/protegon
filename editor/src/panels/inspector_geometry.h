#pragma once

#include "panels/inspector_features.h"

namespace ptgn::editor::inspector {


template <typename Target>
[[nodiscard]] std::optional<V2_float> GetGeometryTargetWorldReferencePosition(
	const Target& target, V2_float local_position
) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		if (entity && entity.Has<Transform>()) {
			return GetDrawTransform(entity).Apply(local_position);
		}
	}

	return std::nullopt;
}

template <typename Target>
[[nodiscard]] PositionPicker::Convert MakeLocalPositionConverter(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		return [entity](V2_float world_position) mutable -> std::optional<V2_float> {
			if (!entity || !entity.Has<Transform>()) {
				return std::nullopt;
			}

			return GetDrawTransform(entity).ApplyInverse(world_position);
		};
	} else {
		return [](V2_float) -> std::optional<V2_float> {
			return std::nullopt;
		};
	}
}

template <typename Target>
[[nodiscard]] constexpr bool CanPickLocalPosition() {
	return requires(Target target) { target.entity; };
}

template <typename Target, typename Component, typename Locator, typename Callback>
bool DrawPickableLocalPosition(
	Target& target, Component& component, std::string_view label, Locator locator,
	Callback callback, bool* remove_requested = nullptr
) {
	V2_float& position{ locator(component) };

	const bool changed{ DrawPropertyRow(label, [&]() {
		const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
		const float pick_width{ ImGui::CalcTextSize("Pick").x +
								ImGui::GetStyle().FramePadding.x * 2.0f };
		const float remove_width{ remove_requested ? ImGui::GetFrameHeight() : 0.0f };
		const float remove_spacing{ remove_requested ? spacing : 0.0f };
		const float available{ ImGui::GetContentRegionAvail().x };
		const float field_width{ std::max(
			36.0f, (available - pick_width - remove_width - remove_spacing - spacing * 2.0f) * 0.5f
		) };

		bool local_changed{ false };

		ImGui::SetNextItemWidth(field_width);
		local_changed |= ImGui::DragFloat(
			"##X", &position.x, kInspectorPositionDragSpeed, 0.0f, 0.0f, "X: %.2f"
		);

		ImGui::SameLine(0.0f, spacing);
		ImGui::SetNextItemWidth(field_width);
		local_changed |= ImGui::DragFloat(
			"##Y", &position.y, kInspectorPositionDragSpeed, 0.0f, 0.0f, "Y: %.2f"
		);

		ImGui::SameLine(0.0f, spacing);

		{
			ScopedDisabled disabled{ !CanPickLocalPosition<Target>() };

			auto apply{ target.template MakeApply<Component>(callback) };
			Component snapshot{ component };

			DrawPositionPickButton(
				target.ctx, label, position, MakeLocalPositionConverter(target),
				[apply, snapshot = std::move(snapshot), locator](V2_float picked) mutable {
					locator(snapshot) = picked;
					apply(ComponentState<Component>{ snapshot });
				},
				GetGeometryTargetWorldReferencePosition(target, position), true
			);

			if constexpr (!CanPickLocalPosition<Target>()) {
				DrawTooltip("Position picking is available for scene entities.");
			}
		}

		if (remove_requested) {
			ImGui::SameLine(0.0f, spacing);
			if (ImGui::Button("-", ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() })) {
				*remove_requested = true;
			}
			DrawTooltip("Delete this vertex.");
		}

		return local_changed;
	}) };

	return changed;
}

template <typename Target, typename Component, typename Value, typename Locator, typename Callback>
bool DrawGeometryValue(
	Target& target, Component& component, Value& value, std::string_view label, Locator locator,
	Callback callback
);

template <
	std::size_t I, typename Target, typename Component, typename Parent, typename Locator,
	typename Callback>
bool DrawReflectedGeometryMember(
	Target& target, Component& component, Parent& parent, Locator locator, Callback callback
) {
	auto members{ ReflectMembers(parent) };
	auto& member{ std::get<I>(members) };

	auto member_locator = [locator](Component& root) -> decltype(auto) {
		auto reflected{ ReflectMembers(locator(root)) };
		return std::get<I>(reflected).value;
	};

	return DrawGeometryValue(
		target, component, member.value, PrettyName(member.name), member_locator, callback
	);
}

template <
	typename Target, typename Component, typename Parent, typename Locator, typename Callback,
	std::size_t... I>
bool DrawReflectedGeometryMembers(
	Target& target, Component& component, Parent& parent, Locator locator, Callback callback,
	std::index_sequence<I...>
) {
	bool changed{ false };
	((changed |= DrawReflectedGeometryMember<I>(target, component, parent, locator, callback)),
	 ...);
	return changed;
}

template <
	typename Target, typename Component, typename Variant, typename Locator, typename Callback,
	std::size_t... I>
bool DrawGeometryVariant(
	Target& target, Component& component, Variant& value, std::string_view label, Locator locator,
	Callback callback, std::index_sequence<I...>
) {
	ScopedID variant_scope{ std::addressof(value) };
	bool changed{ false };
	std::string preview{ "None" };
	std::optional<std::size_t> requested_index;

	auto update_preview = [&]<std::size_t Index>() {
		if (value.index() != Index) {
			return;
		}

		using Alternative = std::variant_alternative_t<Index, Variant>;

		if constexpr (!std::same_as<Alternative, std::monostate>) {
			preview = VariantTypeLabel<Alternative>();
		}
	};

	(update_preview.template operator()<I>(), ...);

	changed |= DrawPropertyRow(label, [&]() {
		bool local_changed{ false };

		if (ImGui::BeginCombo("##value", preview.c_str())) {
			auto draw_option = [&]<std::size_t Index>() {
				using Alternative = std::variant_alternative_t<Index, Variant>;

				const std::string option{ std::same_as<Alternative, std::monostate>
											  ? "None"
											  : VariantTypeLabel<Alternative>() };

				if constexpr (std::default_initializable<Alternative>) {
					const bool selected{ value.index() == Index };

					if (ImGui::Selectable(option.c_str(), selected) && !selected) {
						requested_index = Index;
						local_changed	= true;
					}
				}
			};

			(draw_option.template operator()<I>(), ...);
			ImGui::EndCombo();
		}

		return local_changed;
	});

	if (requested_index) {
		auto apply_requested = [&]<std::size_t Index>() {
			if (*requested_index == Index) {
				value.template emplace<Index>();
			}
		};

		(apply_requested.template operator()<I>(), ...);
		return true;
	}

	auto draw_selected = [&]<std::size_t Index>() {
		if (value.index() != Index) {
			return;
		}

		using Alternative = std::variant_alternative_t<Index, Variant>;

		if constexpr (!std::same_as<Alternative, std::monostate>) {
			auto alternative_locator = [locator](Component& root) -> Alternative& {
				return std::get<Index>(locator(root));
			};

			changed |= DrawGeometryValue(
				target, component, std::get<Index>(value), VariantTypeLabel<Alternative>(),
				alternative_locator, callback
			);
		}
	};

	(draw_selected.template operator()<I>(), ...);
	return changed;
}

template <typename Mask>
	requires std::integral<Mask>
bool DrawColliderMaskList(std::vector<Mask>& masks) {
	bool changed{ false };
	std::optional<std::size_t> remove_index;
	std::optional<std::pair<std::size_t, std::size_t>> move;

	ImGui::SeparatorText("Collides with Masks");

	if (ImGui::Button("+ Mask", ImVec2{ -FLT_MIN, 0.0f })) {
		masks.emplace_back();
		changed = true;
	}

	for (std::size_t index{ 0 }; index < masks.size(); ++index) {
		ScopedID item_scope{ static_cast<int>(index) };

		int displayed{ 0 };
		if constexpr (std::signed_integral<Mask>) {
			displayed = static_cast<int>(
				std::clamp<long long>(static_cast<long long>(masks[index]), 0, 64)
			);
		} else {
			displayed = static_cast<int>(
				std::min<std::uint64_t>(static_cast<std::uint64_t>(masks[index]), 64)
			);
		}

		const std::string label{ "Mask " + std::to_string(index + 1) };

		changed |= DrawPropertyRow(label, [&]() {
			bool local_changed{ false };
			const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
			const float button_width{ ImGui::GetFrameHeight() };
			const float actions_width{ button_width * 3.0f + spacing * 3.0f };
			const float field_width{
				std::max(36.0f, ImGui::GetContentRegionAvail().x - actions_width)
			};

			ImGui::SetNextItemWidth(field_width);
			if (ImGui::DragInt(
					"##value", &displayed, 1.0f, 0, 64, "%d", ImGuiSliderFlags_AlwaysClamp
				)) {
				masks[index]  = static_cast<Mask>(std::clamp(displayed, 0, 64));
				local_changed = true;
			}

			ImGui::SameLine(0.0f, spacing);
			{
				ScopedDisabled disabled{ index == 0 };
				if (ImGui::ArrowButton("##up", ImGuiDir_Up)) {
					move = std::pair{ index, index - 1 };
				}
			}

			ImGui::SameLine(0.0f, spacing);
			{
				ScopedDisabled disabled{ index + 1 >= masks.size() };
				if (ImGui::ArrowButton("##down", ImGuiDir_Down)) {
					move = std::pair{ index, index + 1 };
				}
			}

			ImGui::SameLine(0.0f, spacing);
			if (ImGui::Button("X##remove", ImVec2{ button_width, button_width })) {
				remove_index = index;
			}

			return local_changed;
		});
	}

	if (move) {
		std::ranges::iter_swap(
			masks.begin() + static_cast<std::ptrdiff_t>(move->first),
			masks.begin() + static_cast<std::ptrdiff_t>(move->second)
		);
		changed = true;
	} else if (remove_index) {
		masks.erase(masks.begin() + static_cast<std::ptrdiff_t>(*remove_index));
		changed = true;
	}

	return changed;
}

template <typename Target, typename Component, typename Value, typename Locator, typename Callback>
bool DrawGeometryValue(
	Target& target, Component& component, Value& value, std::string_view label, Locator locator,
	Callback callback
) {
	using Type = std::remove_cvref_t<Value>;

	if constexpr (std::same_as<Type, V2_float>) {
		const std::string normalized{ NormalizeFeatureName(label) };
		if constexpr (std::same_as<std::remove_cvref_t<Component>, Ellipse>) {
			return DrawWHValue(label, value, kInspectorSizeDragSpeed, 0.0f, 0.0f, "%.3f");
		} else if (normalized == "size" || normalized == "dimensions") {
			return DrawWHValue(label, value, kInspectorSizeDragSpeed, 0.0f, 0.0f, "%.3f");
		} else {
			return DrawPickableLocalPosition(target, component, label, locator, callback);
		}
	} else if constexpr (std::same_as<Type, Rect>) {
		bool changed{ false };
		V2_float size{ value.GetSize() };

		if (DrawWHValue("Size", size, kInspectorSizeDragSpeed, 0.0f, 0.0f, "%.3f")) {
			size.x = std::max(size.x, 0.0f);
			size.y = std::max(size.y, 0.0f);

			const V2_float center{ value.GetCenter() };
			const V2_float half_size{ size * 0.5f };
			value.min = center - half_size;
			value.max = center + half_size;
			changed	  = true;
		}

		auto min_locator = [locator](Component& root) -> V2_float& {
			return locator(root).min;
		};
		auto max_locator = [locator](Component& root) -> V2_float& {
			return locator(root).max;
		};

		changed |= DrawPickableLocalPosition(target, component, "Min", min_locator, callback);
		changed |= DrawPickableLocalPosition(target, component, "Max", max_locator, callback);

		return changed;
	} else if constexpr (kIsVector<Type>) {
		if constexpr (
			std::same_as<std::remove_cvref_t<Component>, Collider> &&
			std::integral<typename Type::value_type>
		) {
			const std::string normalized{ NormalizeFeatureName(label) };

			if (normalized.contains("collideswith")) {
				return DrawColliderMaskList(value);
			}

			return DrawValue(target.ctx, label, value);
		} else if constexpr (std::same_as<typename Type::value_type, V2_float>) {
			bool changed{ false };
			std::optional<std::size_t> remove;

			if (ImGui::Button("+ Vertex", ImVec2{ -FLT_MIN, 0.0f })) {
				value.push_back(value.empty() ? V2_float{} : value.back());
				changed = true;
			}

			for (std::size_t index{ 0 }; index < value.size(); ++index) {
				ScopedID vertex_scope{ static_cast<int>(index) };
				auto vertex_locator = [locator, index](Component& root) -> V2_float& {
					return locator(root)[index];
				};
				bool remove_vertex{ false };

				changed |= DrawPickableLocalPosition(
					target, component, std::string{ "Vertex " } + std::to_string(index + 1),
					vertex_locator, callback, &remove_vertex
				);

				if (remove_vertex) {
					remove = index;
				}
			}

			if (remove) {
				value.erase(value.begin() + static_cast<std::ptrdiff_t>(*remove));
				changed = true;
			}

			return changed;
		} else {
			return DrawValue(target.ctx, label, value);
		}
	} else if constexpr (kIsArray<Type>) {
		if constexpr (std::same_as<typename Type::value_type, V2_float>) {
			bool changed{ false };

			for (std::size_t index{ 0 }; index < value.size(); ++index) {
				ScopedID vertex_scope{ static_cast<int>(index) };
				auto vertex_locator = [locator, index](Component& root) -> V2_float& {
					return locator(root)[index];
				};

				changed |= DrawPickableLocalPosition(
					target, component, std::string{ "Vertex " } + std::to_string(index + 1),
					vertex_locator, callback
				);
			}

			return changed;
		} else {
			return DrawValue(target.ctx, label, value);
		}
	} else if constexpr (kIsVariant<Type>) {
		return DrawGeometryVariant(
			target, component, value, label, locator, callback,
			std::make_index_sequence<std::variant_size_v<Type>>{}
		);
	} else if constexpr (std::integral<Type>) {
		const std::string normalized{ NormalizeFeatureName(label) };

		if constexpr (std::same_as<std::remove_cvref_t<Component>, Collider>) {
			if (normalized == "mask") {
				return DrawValue(
					target.ctx, label, value,
					FieldOptions{
						.speed = 1.0f,
						.min   = 0.0f,
						.max   = 64.0f,
						.flags = ImGuiSliderFlags_AlwaysClamp,
					}
				);
			}
		}

		return DrawValue(target.ctx, label, value);
	} else if constexpr (std::same_as<Type, float>) {
		const std::string normalized{ NormalizeFeatureName(label) };
		if (normalized.contains("radius") || normalized.contains("radii")) {
			return DrawRValue(label, value, 0.1f, 0.0f, 0.0f, "%.3f");
		}
		return DrawValue(target.ctx, label, value);
	} else if constexpr (ReflectedValue<Type>) {
		auto reflected{ ReflectValue(value) };
		auto value_locator = [locator](Component& root) -> decltype(auto) {
			return ReflectValue(locator(root)).value;
		};

		return DrawGeometryValue(
			target, component, reflected.value, label, value_locator, callback
		);
	} else if constexpr (ReflectedMembers<Type>) {
		auto members{ ReflectMembers(value) };

		return DrawReflectedGeometryMembers(
			target, component, value, locator, callback,
			std::make_index_sequence<std::tuple_size_v<decltype(members)>>{}
		);
	} else {
		return DrawValue(target.ctx, label, value);
	}
}

template <typename Target, typename Component, typename Callback = std::nullptr_t>
bool DrawGeometryComponent(Target& target, Component& component, Callback callback = nullptr) {
	auto root_locator = [](Component& value) -> Component& {
		return value;
	};

	return DrawGeometryValue(
		target, component, component, TypeLabel<Component>(), root_locator, callback
	);
}


} // namespace ptgn::editor::inspector
