#include "panels/inspector_features.h"

namespace ptgn::editor::inspector {

namespace {

template <typename Target>
bool DrawCameraParentRenderTarget(Target& target) {
	if constexpr (!requires { target.entity; }) {
		return false;
	} else {
		Entity camera_entity{ target.entity };

		if (!camera_entity) {
			return false;
		}

		struct RenderTargetOption {
			UUID uuid{};
			std::string label{};
		};

		auto uuid_text = []<typename T>(const T& value) {
			if constexpr (JsonSerializable<T>) {
				json serialized = value;

				if (serialized.is_string()) {
					return serialized.template get<std::string>();
				}

				return serialized.dump();
			} else if constexpr (requires { std::to_string(value.value); }) {
				return std::to_string(value.value);
			} else if constexpr (requires { std::to_string(value.value()); }) {
				return std::to_string(value.value());
			} else {
				return std::to_string(Hash(value));
			}
		};

		auto& scene{ camera_entity.GetScene() };
		const Entity scene_target{ scene.GetRenderTarget() };
		const Entity main_camera{ scene.GetCamera() };
		const Entity fixed_camera{ scene.GetFixedCamera() };
		const bool parent_target_read_only{ camera_entity == main_camera ||
											camera_entity == fixed_camera };

		std::string scene_target_label{ "Scene Target" };
		if (scene_target && scene_target.Has<Tag>()) {
			scene_target_label = std::string{ scene_target.Get<Tag>() };
		}

		std::vector<RenderTargetOption> render_targets;
		for (auto [entity, _target] : scene.EntitiesWith<::ptgn::impl::RenderTargetDesc>()) {
			if (!entity || entity == scene_target || !entity.Has<Tag, UUID>()) {
				continue;
			}

			const UUID uuid{ entity.Get<UUID>() };
			std::string label{ std::string{ entity.Get<Tag>() } };
			label += " [";
			label += uuid_text(uuid);
			label += "]";

			render_targets.push_back(
				RenderTargetOption{
					.uuid  = uuid,
					.label = std::move(label),
				}
			);
		}

		std::ranges::sort(render_targets, {}, &RenderTargetOption::label);

		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<::ptgn::impl::ParentRenderTarget>()) };

		auto before{ target.template Capture<::ptgn::impl::ParentRenderTarget>() };
		auto value{ before };
		bool changed{ false };

		changed |= DrawPropertyRow("Parent Render Target", [&]() {
			bool row_changed{ false };
			const char* preview{ scene_target_label.c_str() };
			std::string missing_preview;

			if (value) {
				const auto selected{ std::ranges::find(
					render_targets, value->render_target, &RenderTargetOption::uuid
				) };

				if (selected != render_targets.end()) {
					preview = selected->label.c_str();
				} else {
					missing_preview	 = "Missing Render Target [";
					missing_preview += uuid_text(value->render_target);
					missing_preview += "]";
					preview			 = missing_preview.c_str();
				}
			}

			ImGui::BeginDisabled(parent_target_read_only);
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo("##RenderTarget", preview)) {
				const bool scene_selected{ !value.has_value() };
				if (ImGui::Selectable(scene_target_label.c_str(), scene_selected)) {
					value.reset();
					row_changed = true;
				}

				if (scene_selected) {
					ImGui::SetItemDefaultFocus();
				}

				if (!render_targets.empty()) {
					ImGui::Separator();
				}

				for (const auto& option : render_targets) {
					const bool selected{ value && value->render_target == option.uuid };

					if (ImGui::Selectable(option.label.c_str(), selected)) {
						value = ::ptgn::impl::ParentRenderTarget{
							.render_target = option.uuid,
						};
						row_changed = true;
					}

					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}
			ImGui::EndDisabled();

			if (parent_target_read_only) {
				DrawTooltip("The main and fixed cameras cannot change their parent render target.");
			}

			return row_changed;
		});

		if (changed) {
			target.template SetLive<::ptgn::impl::ParentRenderTarget>(value);
		}

		auto after{ target.template Capture<::ptgn::impl::ParentRenderTarget>() };
		TrackComponentState(
			target, "Edit Parent Render Target", std::move(before), std::move(after), changed
		);

		return changed;
	}
}

template <typename Target>
bool DrawCameraFeatureImpl(Target& target) {
	if (!HasCameraFeature(target)) {
		return false;
	}

	const auto header{ DrawFeatureHeader(
		target, InspectorFeature::Camera, "Camera", ImGuiTreeNodeFlags_None,
		CameraFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;
	AutoLabelWidthScope camera_label_width{ "CameraFeatureFields" };

	bool changed{ header.changed };
	changed |= DrawCameraParentRenderTarget(target);
	changed |= DrawRequiredComponent<Target, ::ptgn::impl::CameraData>(
		target, "Camera", false, [&target](::ptgn::impl::CameraData& value) {
			return DrawRegisteredComponentContents(
				target.ctx, Hash<::ptgn::impl::CameraData>(), std::addressof(value)
			);
		}
	);
	changed |= DrawRequiredComponent<Target, ::ptgn::impl::CameraMask>(
		target, "Layers", false, [](::ptgn::impl::CameraMask& value) {
			bool masks_changed{ DrawLayerMaskValue("Include Layer", value.include) };
			masks_changed |= DrawLayerMaskValue("Exclude Layer", value.exclude);
			return masks_changed;
		}
	);

	return changed;
}


} // namespace

bool DrawCameraFeature(EntityInspectorTarget& target) {
	return DrawCameraFeatureImpl(target);
}

bool DrawCameraFeature(PrefabInspectorTarget& target) {
	return DrawCameraFeatureImpl(target);
}

} // namespace ptgn::editor::inspector
