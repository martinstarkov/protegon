#include "panels/inspector_features.h"

namespace ptgn::editor::inspector {

namespace {

template <typename Target>
bool DrawAddFeatureMenu(Target& target) {
	bool changed{ false };

	if (ImGui::Button("Add Feature", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddFeaturePopup");
	}

	if (!ImGui::BeginPopup("AddFeaturePopup")) {
		return false;
	}

	auto item_with_default = [&]<typename Default, typename... T>(
								 InspectorFeature feature, const char* label, bool feature_exists,
								 FeatureComponents<T...> components
							 ) {
		if constexpr (!Target::template Supports<Default>()) {
			return;
		}

		ScopedDisabled disabled{ feature_exists };

		if (ImGui::MenuItem(label)) {
			changed |= AddInspectorFeatureWithDefault<Default>(target, feature, label, components);
		}
	};

	item_with_default.template operator()<Transform>(
		InspectorFeature::Transform, "Transform", HasTransformFeature(target),
		TransformFeatureComponents{}
	);
	const bool primary_render_target{ IsPrimarySceneRenderTarget(target) };
	const bool fixed_camera{ IsReservedFixedCamera(target) };
	const bool visual_exists{ HasVisualFeature(target) };
	const bool camera_exists{ HasCameraFeature(target) };

	if (!primary_render_target && !fixed_camera && !camera_exists) {
		item_with_default.template operator()<::ptgn::impl::IDrawable>(
			InspectorFeature::Visual, "Visual", visual_exists, VisualFeatureComponents{}
		);
	}
	item_with_default.template operator()<::ptgn::impl::Interactive>(
		InspectorFeature::Interaction, "Interaction",
		HasInteractionFeature(target) ||
			HasFeatureComponent<Target, ::ptgn::impl::ButtonData>(target),
		InteractionFeatureComponents{}
	);
	item_with_default.template operator()<Collider>(
		InspectorFeature::Physics, "Physics & Movement", HasPhysicsFeature(target),
		PhysicsFeatureComponents{}
	);
	item_with_default.template operator()<::ptgn::impl::ButtonData>(
		InspectorFeature::UI, "UI", HasFeatureComponent<Target, ::ptgn::impl::ButtonData>(target),
		UIFeatureComponents{}
	);
	if (!primary_render_target && !visual_exists) {
		item_with_default.template operator()<::ptgn::impl::CameraData>(
			InspectorFeature::Camera, "Camera", camera_exists, CameraFeatureComponents{}
		);
	}
	item_with_default.template operator()<::ptgn::impl::Scripts>(
		InspectorFeature::Scripts, "Scripts", HasScriptsFeature(target), ScriptsFeatureComponents{}
	);
	{
		ScopedDisabled disabled{ HasUtilitiesFeature(target) };

		if (ImGui::MenuItem("Utilities")) {
			changed |= AddInspectorFeature(
				target, InspectorFeature::Utilities, "Utilities", UtilitiesFeatureComponents{}
			);
		}
	}

	ImGui::EndPopup();
	return changed;
}

template <typename Target>
bool DrawFeatureInspectorImpl(Target& target) {
	if constexpr (requires { target.entity; }) {
		ClearButtonPreviewIfDifferent(target.ctx, target.entity);
	} else {
		ClearButtonPreviewIfDifferent(target.ctx);
	}

	bool changed{ false };
	if (GetButtonChildInfo(target)) {
		// Advanced managed-part editing chooses the state before Transform/Visual consume it.
		changed |= DrawUIFeature(target);
		changed |= DrawTransformFeature(target);
		changed |= DrawVisualFeature(target);
	} else {
		changed |= DrawTransformFeature(target);
		changed |= DrawUIFeature(target);
		changed |= DrawVisualFeature(target);
	}
	changed |= DrawInteractionFeature(target);
	changed |= DrawPhysicsFeature(target);
	changed |= DrawCameraFeature(target);
	changed |= DrawScriptsFeature(target);
	changed |= DrawUtilitiesFeature(target);

	ImGui::Separator();
	changed |= DrawAddFeatureMenu(target);
	return changed;
}

} // namespace

bool DrawFeatureInspector(EntityInspectorTarget& target) {
	return DrawFeatureInspectorImpl(target);
}

bool DrawFeatureInspector(PrefabInspectorTarget& target) {
	return DrawFeatureInspectorImpl(target);
}

} // namespace ptgn::editor::inspector
