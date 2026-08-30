#include "panels/inspector_features.h"

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

		{
			ScopedDisabled disabled{ true };
			ImGui::Checkbox("##Enabled", &enabled);
		}

		DrawTooltip("Interaction Lock is managed by the runtime.");
		ImGui::SameLine();

		if (ImGui::TreeNodeEx(
				"Interaction Lock##ReadOnlyInteractionLock", ImGuiTreeNodeFlags_SpanAvailWidth
			)) {
			ScopedIndent indent;
			AutoLabelWidthScope label_width{ "InteractionLockReadOnlyFields" };
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

			ImGui::TreePop();
		}

		return false;
	}
}

template <typename Target>
bool DrawInteractionFeatureImpl(Target& target) {
	if (!HasInteractionFeature(target)) {
		return false;
	}

	const auto header{ DrawFeatureHeader(
		target, InspectorFeature::Interaction, "Interaction", ImGuiTreeNodeFlags_None,
		InteractionFeatureComponents{}
	) };

	if (!header.open) {
		return header.changed;
	}

	ScopedIndent feature_indent;

	bool changed{ header.changed };

	changed |=
		DrawOptionalReflected<Target, ::ptgn::impl::Interactive>(target, "Interactive", true);
	changed |= DrawOptionalReflected<Target, ::ptgn::impl::Draggable>(target, "Draggable", true);
	changed |= DrawOptionalReflected<Target, ::ptgn::impl::Dropzone>(target, "Dropzone", true);
	changed |= DrawReadOnlyInteractionLock(target);

	return changed;
}


} // namespace

bool DrawInteractionFeature(EntityInspectorTarget& target) {
	return DrawInteractionFeatureImpl(target);
}

bool DrawInteractionFeature(PrefabInspectorTarget& target) {
	return DrawInteractionFeatureImpl(target);
}

} // namespace ptgn::editor::inspector
