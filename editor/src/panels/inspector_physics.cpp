#include "panels/inspector_features.h"
#include "panels/inspector_geometry.h"

namespace ptgn::editor::inspector {

namespace {

template <typename Target>
bool DrawRigidBodyWithInheritance(Target& target) {
	bool inheritance_changed{ false };

	const bool changed{ DrawOptionalComponent<Target, RigidBody>(
		target, "Rigid Body", true, [&](RigidBody& value) {
			const bool rigid_body_changed{ DrawRegisteredComponentContents(
				target.ctx, Hash<RigidBody>(), std::addressof(value)
			) };

			inheritance_changed |=
				DrawOptionalReflected<Target, ::ptgn::impl::IgnoreParentImmovable>(
					target, "Ignore Parent Immovable", false
				);

			return rigid_body_changed;
		}
	) };

	if (!target.template Capture<RigidBody>() &&
		target.template Capture<::ptgn::impl::IgnoreParentImmovable>()) {
		auto before{ target.template Capture<::ptgn::impl::IgnoreParentImmovable>() };
		target.template SetLive<::ptgn::impl::IgnoreParentImmovable>(std::nullopt);
		auto after{ target.template Capture<::ptgn::impl::IgnoreParentImmovable>() };

		TrackComponentState(
			target, "Remove Ignore Parent Immovable", std::move(before), std::move(after), true
		);

		inheritance_changed = true;
	}

	return changed || inheritance_changed;
}

template <typename Enum>
bool DrawPlatformerEnumCombo(const char* id, Enum& value) {
	const std::string preview{ PrettyName(magic_enum::enum_name(value)) };
	ImGui::SetNextItemWidth(-FLT_MIN);

	if (!ImGui::BeginCombo(id, preview.c_str())) {
		return false;
	}

	bool changed{ false };
	for (const auto candidate : magic_enum::enum_values<Enum>()) {
		const std::string label{ PrettyName(magic_enum::enum_name(candidate)) };
		if (ImGui::Selectable(label.c_str(), candidate == value)) {
			value	= candidate;
			changed = true;
		}
	}

	ImGui::EndCombo();
	return changed;
}

template <typename Target>
Entity GetInspectorTargetEntity(Target& target) {
	if constexpr (requires { target.entity; }) {
		return target.entity;
	} else {
		return {};
	}
}

bool DrawColliderMasks(std::vector<ColliderMask>& masks) {
	bool changed{ false };
	std::optional<std::size_t> remove_index;

	if (masks.empty()) {
		ImGui::TextDisabled("Any collision mask");
	}

	for (std::size_t i{ 0 }; i < masks.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		const float remove_width{ ImGui::GetFrameHeight() };
		const float spacing{ ImGui::GetStyle().ItemSpacing.x };
		ImGui::SetNextItemWidth(
			std::max(60.0f, ImGui::GetContentRegionAvail().x - remove_width - spacing)
		);
		changed |= ImGui::InputScalar("##Mask", ImGuiDataType_S64, std::addressof(masks[i]));
		ImGui::SameLine();
		if (ImGui::Button("X", ImVec2{ remove_width, remove_width })) {
			remove_index = i;
		}
		ImGui::PopID();
	}

	if (remove_index.has_value()) {
		masks.erase(masks.begin() + static_cast<std::ptrdiff_t>(remove_index.value()));
		changed = true;
	}

	if (ImGui::Button("+ Mask")) {
		masks.emplace_back(0);
		changed = true;
	}

	return changed;
}

template <typename Target>
bool DrawPlatformerMovementContents(Target& target, PlatformerMovement& value) {
	bool changed{ false };

	if (ImGui::TreeNodeEx(
			"Horizontal Movement",
			ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth
		)) {
		auto draw_float = [&](const char* label, float& field, float speed = 1.0f) {
			changed |= DrawPropertyRow(label, [&]() {
				ImGui::SetNextItemWidth(-FLT_MIN);
				return ImGui::DragFloat("##Value", &field, speed);
			});
		};

		draw_float("Max Speed", value.max_speed);
		draw_float("Acceleration", value.max_acceleration);
		draw_float("Deceleration", value.max_deceleration);
		draw_float("Turn Speed", value.max_turn_speed);
		draw_float("Air Acceleration", value.max_air_acceleration);
		draw_float("Air Deceleration", value.max_air_deceleration);
		draw_float("Air Turn Speed", value.max_air_turn_speed);
		draw_float("Friction", value.friction, 0.05f);

		changed |= DrawPropertyRow("Use Acceleration", [&]() {
			return ImGui::Checkbox("##Value", &value.use_acceleration);
		});
		changed |= DrawPropertyRow("Left Key", [&]() {
			return DrawPlatformerEnumCombo("##Value", value.left_key);
		});
		changed |= DrawPropertyRow("Right Key", [&]() {
			return DrawPlatformerEnumCombo("##Value", value.right_key);
		});

		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx(
			"Grounding", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth
		)) {
		auto& grounding{ value.grounding };

		changed |= DrawPropertyRow("Enabled", [&]() {
			return ImGui::Checkbox("##Value", &grounding.enabled);
		});
		changed |= DrawPropertyRow("Direction", [&]() {
			return DrawPlatformerEnumCombo("##Value", grounding.direction);
		});
		changed |= DrawPropertyRow("Max Slope Angle", [&]() {
			ImGui::SetNextItemWidth(-FLT_MIN);
			return ImGui::DragFloat(
				"##Value", &grounding.max_angle.value, 1.0f, 0.0f, 180.0f, "%.1f deg",
				ImGuiSliderFlags_AlwaysClamp
			);
		});
		changed |= DrawPropertyRow("Match Entity", [&]() {
			return DrawPlatformerEnumCombo("##Value", grounding.entity_scope);
		});

		if (ImGui::TreeNodeEx("Collision Masks", ImGuiTreeNodeFlags_SpanAvailWidth)) {
			changed |= DrawColliderMasks(grounding.masks);
			ImGui::TreePop();
		}

		Entity owner{ GetInspectorTargetEntity(target) };
		changed |= DrawPropertyRow("Ground Entities", [&]() {
			auto& state{ GetManualFeatureState(target.GetFeatureTargetKey()) };
			Scene* scene{ owner ? std::addressof(owner.GetScene()) : nullptr };
			return DrawEntityFilterButton(
				scene, owner, grounding.entities, state.grounding_filter_state
			);
		});

		ImGui::TreePop();
	}

	return changed;
}

template <typename Target>
bool DrawPhysicsFeatureImpl(Target& target) {
	RegisterBuiltInPlatformerJumpControllers();

	if (Entity live_entity{ GetInspectorTargetEntity(target) }) {
		SyncPlatformerJumpController(live_entity);
	}

	if (!HasPhysicsFeature(target)) {
		return false;
	}

	const auto header{ DrawFeatureHeader(
		target, InspectorFeature::Physics, "Physics & Movement", ImGuiTreeNodeFlags_None,
		PhysicsFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;

	bool changed{ header.changed };

	changed |= DrawOptionalComponent<Target, Collider>(
		target, "Collider", true,
		[&target](Collider& value) { return DrawGeometryComponent(target, value); }
	);
	changed |= DrawRigidBodyWithInheritance(target);
	changed |= DrawOptionalReflected<Target, BoundaryBehavior>(target, "Boundary Behavior", false);

	enum class MovementKind {
		TopDown,
		Platformer,
	};

	using MovementComponents =
		FeatureComponents<TopDownMovement, PlatformerMovement, PlatformerJump>;

	const bool has_top_down{ target.template Capture<TopDownMovement>().has_value() };
	const bool has_platformer{ target.template Capture<PlatformerMovement>().has_value() };
	bool movement_enabled{ has_top_down || has_platformer };
	MovementKind movement{ has_platformer ? MovementKind::Platformer : MovementKind::TopDown };

	auto apply_movement_change = [&](auto&& mutate) {
		constexpr MovementComponents components{};
		auto before{ CaptureInspectorFeatureState(target, InspectorFeature::Physics, components) };

		std::invoke(std::forward<decltype(mutate)>(mutate));

		Entity live_entity{ GetInspectorTargetEntity(target) };
		if (live_entity) {
			SyncPlatformerJumpController(live_entity);
		}

		auto after{ CaptureInspectorFeatureState(target, InspectorFeature::Physics, components) };

		auto apply{ MakeInspectorFeatureApply(target, InspectorFeature::Physics, components) };
		auto sync = [live_entity]() {
			if (live_entity) {
				SyncPlatformerJumpController(live_entity);
			}
		};

		target.ctx.undo.PushApplied(
			"Change Movement",
			[apply, before, sync]() mutable {
				apply(before);
				sync();
			},
			[apply, after, sync]() mutable {
				apply(after);
				sync();
			}
		);

		changed = true;
	};

	ScopedID movement_scope{ "Movement" };

	if (ImGui::Checkbox("##Enabled", &movement_enabled)) {
		apply_movement_change([&]() {
			if (movement_enabled) {
				target.template SetLive<TopDownMovement>(TopDownMovement{});
				target.template SetLive<PlatformerMovement>(std::nullopt);
			} else {
				SetFeatureManuallyAdded(
					target.GetFeatureTargetKey(), InspectorFeature::Physics, true
				);
				target.template SetLive<TopDownMovement>(std::nullopt);
				target.template SetLive<PlatformerMovement>(std::nullopt);
				target.template SetLive<PlatformerJump>(std::nullopt);
			}
		});

		movement = MovementKind::TopDown;
	}

	ImGui::SameLine();

	const bool movement_open{
		ImGui::TreeNodeEx("Movement##Tree", ImGuiTreeNodeFlags_SpanAvailWidth)
	};

	if (movement_open) {
		ScopedIndent movement_indent;
		ScopedPropertyLabelOffset movement_label_offset{ ImGui::GetStyle().IndentSpacing };
		AutoLabelWidthScope movement_label_width{ "MovementFields" };
		ScopedDisabled movement_disabled{ !movement_enabled };

		const char* preview{ movement == MovementKind::Platformer ? "Platformer" : "Top Down" };

		DrawPropertyRow("Type", [&]() {
			bool local_changed{ false };

			if (ImGui::BeginCombo("##MovementType", preview)) {
				auto choose = [&](MovementKind candidate, const char* label) {
					if (!ImGui::Selectable(label, movement == candidate)) {
						return;
					}

					apply_movement_change([&]() {
						if (candidate == MovementKind::TopDown) {
							target.template SetLive<TopDownMovement>(TopDownMovement{});
							target.template SetLive<PlatformerMovement>(std::nullopt);
							target.template SetLive<PlatformerJump>(std::nullopt);
						} else {
							target.template SetLive<PlatformerMovement>(PlatformerMovement{});
							target.template SetLive<TopDownMovement>(std::nullopt);
						}
					});

					movement	  = candidate;
					local_changed = true;
				};

				choose(MovementKind::TopDown, "Top Down");
				choose(MovementKind::Platformer, "Platformer");

				ImGui::EndCombo();
			}

			return local_changed;
		});

		if (movement_enabled && target.template Capture<TopDownMovement>()) {
			changed |= DrawRequiredComponent<Target, TopDownMovement>(
				target, "Top Down Controller", false,
				[&target](TopDownMovement& value) { return DrawDefaultContents(target.ctx, value); }
			);
		}

		if (movement_enabled && target.template Capture<PlatformerMovement>()) {
			changed |= DrawRequiredComponent<Target, PlatformerMovement>(
				target, "Platformer Controller", false, [&target](PlatformerMovement& value) {
					return DrawPlatformerMovementContents(target, value);
				}
			);

			if (ImGui::TreeNodeEx(
					"Jump", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth
				)) {
				auto platformer{ target.template Capture<PlatformerMovement>().value() };
				std::string selected_key{ platformer.jump_controller };

				const auto* selected_controller{
					PlatformerJumpControllerRegistry::Find(selected_key)
				};
				const std::string jump_preview{ selected_controller ? selected_controller->label
																	: "None" };

				DrawPropertyRow("Controller", [&]() {
					bool local_changed{ false };
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (!ImGui::BeginCombo("##JumpController", jump_preview.c_str())) {
						return false;
					}

					auto choose = [&](std::string_view key, const char* label) {
						const bool selected{ selected_key == key };
						if (!ImGui::Selectable(label, selected)) {
							return;
						}

						apply_movement_change([&]() {
							auto value{ target.template Capture<PlatformerMovement>().value() };
							value.jump_controller = key;
							target.template SetLive<PlatformerMovement>(value);

							if (key == "standard" && !target.template Capture<PlatformerJump>()) {
								target.template SetLive<PlatformerJump>(PlatformerJump{});
							}
						});

						selected_key  = key;
						local_changed = true;
					};

					choose({}, "None");
					ImGui::Separator();

					auto controllers{ PlatformerJumpControllerRegistry::Controllers() };
					std::ranges::sort(controllers, [](const auto& lhs, const auto& rhs) {
						if (lhs.group != rhs.group) {
							return lhs.group < rhs.group;
						}
						return lhs.label < rhs.label;
					});

					std::string current_group;
					for (const auto& controller : controllers) {
						if (controller.group != current_group) {
							if (!current_group.empty()) {
								ImGui::Separator();
							}
							current_group = controller.group;
							if (!current_group.empty()) {
								ImGui::TextDisabled("%s", current_group.c_str());
							}
						}
						choose(controller.key, controller.label.c_str());
					}

					ImGui::EndCombo();
					return local_changed;
				});

				if (selected_key == "standard" && target.template Capture<PlatformerJump>()) {
					changed |= DrawRequiredComponent<Target, PlatformerJump>(
						target, "Standard Jump", false, [&target](PlatformerJump& value) {
							return DrawDefaultContents(target.ctx, value);
						}
					);
				} else if (!selected_key.empty()) {
					Entity entity{ GetInspectorTargetEntity(target) };
					const auto* controller{ PlatformerJumpControllerRegistry::Find(selected_key) };
					if (entity && controller && controller->has(entity)) {
						if (void* data{ controller->get(entity) }) {
							changed |= DrawRegisteredComponentContents(
								target.ctx, controller->component_hash, data
							);
						}
					}
				}

				ImGui::TreePop();
			}

			if (Entity owner{ GetInspectorTargetEntity(target) };
				owner && owner.Has<PlatformerMovement>()) {
				const auto& live{ owner.Get<PlatformerMovement>() };
				ImGui::SeparatorText("Runtime");
				ImGui::TextDisabled("Grounded: %s", live.IsGrounded() ? "Yes" : "No");
				if (Entity ground{ live.GetGroundEntity() }) {
					const std::string ground_name{ ground.Has<Tag>() &&
														   !ground.Get<Tag>().value.empty()
													   ? ground.Get<Tag>().value
													   : "Entity" };
					const V2_float normal{ live.GetGroundNormal() };
					ImGui::TextDisabled("Ground Entity: %s", ground_name.c_str());
					ImGui::TextDisabled("Ground Normal: %.2f, %.2f", normal.x, normal.y);
				}
			}
		}

		ImGui::TreePop();
	}

	return changed;
}


} // namespace

bool DrawPhysicsFeature(EntityInspectorTarget& target) {
	return DrawPhysicsFeatureImpl(target);
}

bool DrawPhysicsFeature(PrefabInspectorTarget& target) {
	return DrawPhysicsFeatureImpl(target);
}

} // namespace ptgn::editor::inspector
