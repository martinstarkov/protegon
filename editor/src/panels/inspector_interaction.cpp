#include "panels/inspector_archetype_inspector.h"

#include <algorithm>
#include <string>

#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "editor/renamable_item.h"
#include "panels/inspector_tabs.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/interaction/interactive.h"

namespace ptgn::editor::inspector {

namespace {

template <typename Target>
bool DrawReadOnlyInteractionLock(Target& target) {
	if (!target.ctx.local.settings.show_read_only_inspector_data) {
		return false;
	}

	if constexpr (!Target::template Supports<InteractionLock>()) {
		return false;
	} else {
		const auto state{ target.template Capture<InteractionLock>() };
		bool enabled{ state.has_value() };
		const InteractionLock value{ state.value_or(InteractionLock{}) };

		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<InteractionLock>()) };

		bool open{ false };
		DrawInspectorCustomPropertyRow(
			"Interaction Lock",
			[&]() {
				open = ImGui::TreeNodeEx(
					"Interaction Lock##ReadOnlyInteractionLock",
					ImGuiTreeNodeFlags_SpanAvailWidth |
						ImGuiTreeNodeFlags_FramePadding |
						ImGuiTreeNodeFlags_NoTreePushOnOpen
				);
			},
			[&]() {
				ScopedDisabled disabled{ true };
				ImGui::Checkbox("##Enabled", &enabled);
				DrawTooltip("Interaction Lock is managed by the runtime.");
				return false;
			}
		);

		if (open) {
			ScopedIndent indent;
			ReadOnlyScope read_only{ true };

			[&]<typename T>(const T& reflected_value) {
				if constexpr (ReflectedMembers<T>) {
					auto members{ ReflectMembers(reflected_value) };
					std::apply(
						[&](auto&&... member) {
							(DrawReadOnlyValue(target.ctx, PrettyName(member.name), member.value),
							 ...);
						},
						members
					);
				}

				if constexpr (ReflectedReadOnlyMembers<T>) {
					auto members{ ReflectReadOnlyMembers(reflected_value) };
					std::apply(
						[&](auto&&... member) {
							(DrawReadOnlyValue(target.ctx, PrettyName(member.name), member.value),
							 ...);
						},
						members
					);
				}
			}(value);

		}

		return false;
	}
}

enum class InteractionMode : std::uint8_t {
	Basic,
	Draggable,
	Dropzone,
	Conflict,
};

template <typename Target>
[[nodiscard]] InteractionMode GetInteractionMode(const Target& target) {
	const bool draggable{ target.template Capture<::ptgn::impl::Draggable>().has_value() };
	const bool dropzone{ target.template Capture<::ptgn::impl::Dropzone>().has_value() };
	if (draggable && dropzone) {
		return InteractionMode::Conflict;
	}
	if (draggable) {
		return InteractionMode::Draggable;
	}
	if (dropzone) {
		return InteractionMode::Dropzone;
	}
	return InteractionMode::Basic;
}

template <typename Target>
bool SetInteractionMode(Target& target, InteractionMode mode) {
	if (mode == InteractionMode::Conflict) {
		return false;
	}

	using Components = ComponentSet<
		::ptgn::impl::Interactive,
		::ptgn::impl::Draggable,
		::ptgn::impl::Dropzone
	>;
	constexpr Components components{};
	auto before{ CaptureComponentSetState(target, components) };

	if (!target.template Capture<::ptgn::impl::Interactive>()) {
		target.template SetLive<::ptgn::impl::Interactive>(::ptgn::impl::Interactive{});
	}
	target.template SetLive<::ptgn::impl::Draggable>(std::nullopt);
	target.template SetLive<::ptgn::impl::Dropzone>(std::nullopt);

	if (mode == InteractionMode::Draggable) {
		target.template SetLive<::ptgn::impl::Draggable>(::ptgn::impl::Draggable{});
	} else if (mode == InteractionMode::Dropzone) {
		target.template SetLive<::ptgn::impl::Dropzone>(::ptgn::impl::Dropzone{});
	}
		auto after{ CaptureComponentSetState(target, components) };
	TrackComponentSetState(
		target, "Change Interaction Mode", std::move(before), std::move(after), components
	);
	return true;
}

template <typename Target>
bool RemoveInteractionSection(Target& target) {
	using Components = ComponentSet<
		::ptgn::impl::Interactive,
		::ptgn::impl::Draggable,
		::ptgn::impl::Dropzone
	>;
	return RemoveComponentSet(target, "Interaction", Components{});
}

template <typename T>
bool DrawInteractionComponentFields(
	EditorContext& ctx,
	T& value,
	std::string_view enabled_label
) {
	bool changed{ false };
	changed |= DrawPropertyRow(enabled_label, [&]() {
		return ImGui::Checkbox("##Enabled", &value.enabled);
	});

	if constexpr (ReflectedMembers<T>) {
		auto members{ ReflectMembers(value) };
		std::apply(
			[&](auto&&... member) {
				auto draw_member = [&](auto&& reflected) {
					if (std::string_view{ reflected.name } == "enabled") {
						return;
					}
					changed |= DrawValue(ctx, PrettyName(reflected.name), reflected.value);
				};
				(draw_member(member), ...);
			},
			members
		);
	}

	if constexpr (ReflectedReadOnlyMembers<T>) {
		auto members{ ReflectReadOnlyMembers(value) };
		std::apply(
			[&](auto&&... member) {
				if (ctx.local.settings.show_read_only_inspector_data) {
					(DrawReadOnlyValue(ctx, PrettyName(member.name), member.value), ...);
				}
			},
			members
		);
	}
	return changed;
}

[[nodiscard]] std::string InteractiveShapeLabel(Entity shape, std::size_t index) {
	if (const auto* tag{ shape.TryGet<Tag>() }; tag && !tag->value.empty()) {
		return tag->value;
	}
	if (shape.Has<Rect>()) {
		return "Rectangle " + std::to_string(index + 1);
	}
	if (shape.Has<Circle>()) {
		return "Circle " + std::to_string(index + 1);
	}
	return "Hit Area " + std::to_string(index + 1);
}

Entity CreateManagedInteractiveShape(Entity owner, bool circle) {
	if (!owner) {
		return {};
	}
	Entity shape{ owner.GetScene().CreateEntity() };
	shape.Add<Tag>(circle ? "Interactive Circle" : "Interactive Rect");
	shape.Add<Transform>();
	if (circle) {
		shape.Add<Circle>(32.0f);
	} else {
		shape.Add<Rect>(V2_float{ 64.0f, 64.0f });
		shape.Add<Origin>(Origin::Center);
	}
	AddInteractiveShape(owner, shape);
	return shape;
}

bool DrawManagedInteractiveShapes(EntityInspectorTarget& target) {
	if (!target.entity) {
		return false;
	}

	bool changed{ false };
	auto shapes{ GetInteractiveShapes(target.entity) };
	static RenameModalState rename_state{};
	static Entity rename_entity{};
	Entity remove_after_tabs{};

	ImGui::SeparatorText("Hit Areas");

	const bool add_requested{ DrawInspectorTabCollection(
		shapes.empty(),
		InspectorTabCollectionOptions{
			.scope_id = "##InteractionHitAreaStrip",
			.tab_bar_id = "##InteractionHitAreas",
			.add_tab_id = "+##AddInteractionHitArea",
			.empty_add_label = "Add Hit Area",
			.add_tooltip = "Add hit area",
		},
		[&]() {
			for (std::size_t index{ 0 }; index < shapes.size(); ++index) {
				Entity shape{ shapes[index] };
				if (!shape) {
					continue;
				}

				ScopedID shape_scope{ static_cast<int>(index) };
				const std::string label{ InteractiveShapeLabel(shape, index) };
				const bool open{ ImGui::BeginTabItem(label.c_str()) };
				const auto context{ DrawInspectorTabContextMenu(
					"##HitAreaContext", true, false, true, "Remove Hit Area"
				) };

				if (context.rename_requested) {
					rename_entity = shape;
					rename_state.Begin(label);
				}
				if (context.remove_requested) {
					// Never destroy the entity while its tab item is open. Doing so can invalidate
					// `shape` before EndTabItem(), leaving Dear ImGui's tab/ID stacks unbalanced.
					remove_after_tabs = shape;
				}

				if (open) {
					if (shape && shape != remove_after_tabs) {
						EntityInspectorTarget child_target{ .ctx = target.ctx, .entity = shape };
						if (shape.Has<Rect>()) {
							changed |= EditComponent<EntityInspectorTarget, Rect>(
								child_target, "Edit Interactive Rectangle", [&target](Rect& value) {
									return DrawRegisteredComponentContents(
										target.ctx, Hash<Rect>(), std::addressof(value)
									);
								}
							);
						} else if (shape.Has<Circle>()) {
							changed |= EditComponent<EntityInspectorTarget, Circle>(
								child_target, "Edit Interactive Circle", [&target](Circle& value) {
									return DrawRegisteredComponentContents(
										target.ctx, Hash<Circle>(), std::addressof(value)
									);
								}
							);
						}
						changed |= DrawTransformSection(child_target, false, false, false);
					}

					// BeginTabItem() returning true must always be paired with EndTabItem(), even
					// when the context menu requested removal this frame.
					ImGui::EndTabItem();
				}
			}
		}
	) };

	if (add_requested) {
		ImGui::OpenPopup("##AddInteractionHitAreaPopup");
	}
	if (ImGui::BeginPopup("##AddInteractionHitAreaPopup")) {
		if (ImGui::MenuItem("Rectangle")) {
			const EditorSelection before_selection{ target.ctx.local.selection };
			Entity created{ CreateManagedInteractiveShape(target.entity, false) };
			if (created) {
				(void)target.ctx.commands.RecordCreatedEntity(created, before_selection);
				target.ctx.local.selection = before_selection;
				changed = true;
			}
		}
		if (ImGui::MenuItem("Circle")) {
			const EditorSelection before_selection{ target.ctx.local.selection };
			Entity created{ CreateManagedInteractiveShape(target.entity, true) };
			if (created) {
				(void)target.ctx.commands.RecordCreatedEntity(created, before_selection);
				target.ctx.local.selection = before_selection;
				changed = true;
			}
		}
		ImGui::EndPopup();
	}

	if (remove_after_tabs) {
		if (rename_entity == remove_after_tabs) {
			rename_entity = {};
			rename_state.Cancel();
		}
		target.ctx.commands.DeleteEntity(remove_after_tabs);
		changed = true;
	}

	if (rename_state.active) {
		(void)DrawInspectorTabRenameModal(
			rename_state,
			"Rename Hit Area##Interaction",
			"##RenameInteractionHitArea",
			[](std::string_view value) {
				return value.empty() ? std::string{ "Name cannot be empty." } : std::string{};
			},
			[&](std::string_view value) {
				if (rename_entity) {
					target.ctx.commands.RenameEntity(rename_entity, value);
					changed = true;
				}
				rename_entity = {};
			},
			"Rename Hit Area"
		);
	}
	return changed;
}

template <typename Target>
bool DrawInteractionSectionImpl(Target& target) {
	if (!HasInteractionSection(target)) {
		return false;
	}

	const auto header{ DrawInspectorSectionHeader(
		"Interaction",
		"InteractionSection",
		InspectorSectionOptions{
			.default_open = true,
			.removable = true,
			.resettable = false,
		}
	) };

	bool changed{ false };
	if (header.remove_requested) {
		return RemoveInteractionSection(target);
	}
	if (!header.open) {
		return false;
	}

	ScopedIndent section_indent;
	InteractionMode mode{ GetInteractionMode(target) };
	if (mode == InteractionMode::Conflict) {
		ImGui::TextColored(
			ImVec4{ 1.0f, 0.45f, 0.2f, 1.0f },
			"Draggable and Dropzone are mutually exclusive."
		);
	}

	changed |= DrawPropertyRow("Mode", [&]() {
		const std::array choices{
			InspectorChoice{
				.label = "Basic",
				.selected = mode == InteractionMode::Basic,
				.invoke = [&]() {
					changed |= SetInteractionMode(target, InteractionMode::Basic);
					mode = InteractionMode::Basic;
				},
			},
			InspectorChoice{
				.label = "Draggable",
				.selected = mode == InteractionMode::Draggable,
				.invoke = [&]() {
					changed |= SetInteractionMode(target, InteractionMode::Draggable);
					mode = InteractionMode::Draggable;
				},
			},
			InspectorChoice{
				.label = "Dropzone",
				.selected = mode == InteractionMode::Dropzone,
				.invoke = [&]() {
					changed |= SetInteractionMode(target, InteractionMode::Dropzone);
					mode = InteractionMode::Dropzone;
				},
			},
		};
		return DrawInspectorChoiceBar(choices, "##InteractionMode");
	});

	const bool has_interactive{
		target.template Capture<::ptgn::impl::Interactive>().has_value()
	};
	if (!has_interactive) {
		ImGui::TextColored(
			ImVec4{ 1.0f, 0.45f, 0.2f, 1.0f },
			"Interaction is missing its required Interactive component."
		);
		if (ImGui::Button("Repair Interaction", ImVec2{ -FLT_MIN, 0.0f })) {
			changed |= SetInteractionMode(
				target, mode == InteractionMode::Conflict ? InteractionMode::Basic : mode
			);
		}
	} else {
		changed |= EditComponent<Target, ::ptgn::impl::Interactive>(
			target, "Edit Interactive", [&target](::ptgn::impl::Interactive& value) {
				return DrawInteractionComponentFields(target.ctx, value, "Interactive Enabled");
			}
		);
	}

	if (mode == InteractionMode::Draggable) {
		changed |= EditComponent<Target, ::ptgn::impl::Draggable>(
			target, "Edit Draggable", [&target](::ptgn::impl::Draggable& value) {
				return DrawInteractionComponentFields(target.ctx, value, "Draggable Enabled");
			}
		);
	} else if (mode == InteractionMode::Dropzone) {
		changed |= EditComponent<Target, ::ptgn::impl::Dropzone>(
			target, "Edit Dropzone", [&target](::ptgn::impl::Dropzone& value) {
				return DrawInteractionComponentFields(target.ctx, value, "Dropzone Enabled");
			}
		);
	}

	if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
		changed |= DrawManagedInteractiveShapes(target);
	}
	changed |= DrawReadOnlyInteractionLock(target);
	return changed;
}


} // namespace

bool DrawInteractionSection(EntityInspectorTarget& target) {
	return DrawInteractionSectionImpl(target);
}

bool DrawInteractionSection(PrefabInspectorTarget& target) {
	return DrawInteractionSectionImpl(target);
}

} // namespace ptgn::editor::inspector
