#include <imgui.h>
#include <imgui_stdlib.h>

#include <cfloat>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "editor/editor.h"
#include "editor/editor_context.h"
#include "editor/editor_selection.h"
#include "panels/inspector_fields.h"
#include "runtime/graphics/fx/bloom.h"
#include "runtime/graphics/fx/blur.h"
#include "runtime/graphics/fx/gaussian_blur.h"
#include "runtime/graphics/fx/grayscale.h"
#include "runtime/graphics/fx/inverse_color.h"
#include "runtime/graphics/fx/sharpen.h"
#include "runtime/graphics/fx/edge_detection.h"
#include "runtime/graphics/fx/effect_registry.h"
#include "runtime/graphics/fx/effect_registration.h"
#include "runtime/graphics/fx/screen_effect_stack.h"
#include "serialization/json/json.h"

namespace ptgn::editor::inspector {

namespace {

bool DrawScreenEffectJsonValue(const char* label, json& value) {
	bool changed{ false };

	if (value.is_boolean()) {
		bool current{ value.get<bool>() };
		if (ImGui::Checkbox(label, &current)) {
			value = current;
			changed = true;
		}
	} else if (value.is_number_integer()) {
		std::int64_t current{ value.get<std::int64_t>() };
		if (ImGui::InputScalar(label, ImGuiDataType_S64, &current)) {
			value = current;
			changed = true;
		}
	} else if (value.is_number_unsigned()) {
		std::uint64_t current{ value.get<std::uint64_t>() };
		if (ImGui::InputScalar(label, ImGuiDataType_U64, &current)) {
			value = current;
			changed = true;
		}
	} else if (value.is_number_float()) {
		float current{ value.get<float>() };
		if (ImGui::DragFloat(label, &current, 0.01f)) {
			value = current;
			changed = true;
		}
	} else if (value.is_string()) {
		std::string current{ value.get<std::string>() };
		if (ImGui::InputText(label, &current)) {
			value = std::move(current);
			changed = true;
		}
	} else if (value.is_object()) {
		if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) {
			for (auto it{ value.begin() }; it != value.end(); ++it) {
				ImGui::PushID(it.key().c_str());
				changed |= DrawScreenEffectJsonValue(it.key().c_str(), it.value());
				ImGui::PopID();
			}
			ImGui::TreePop();
		}
	} else if (value.is_array()) {
		if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) {
			for (std::size_t index{ 0 }; index < value.size(); ++index) {
				ImGui::PushID(static_cast<int>(index));
				const std::string item{ "[" + std::to_string(index) + "]" };
				changed |= DrawScreenEffectJsonValue(item.c_str(), value[index]);
				ImGui::PopID();
			}
			ImGui::TreePop();
		}
	}

	return changed;
}

template <typename T>
bool DrawTypedScreenEffectParameters(
	EditorContext& ctx,
	std::string_view type,
	json& parameters,
	bool& handled
) {
	if (type != ::ptgn::impl::EffectRegistration<T>::Get().type_name) {
		return false;
	}

	handled = true;

	if constexpr (!(JsonSerializable<T> && JsonDeserializable<T>)) {
		ImGui::TextDisabled("No editable parameters.");
		return false;
	} else {
		T value{};
		if (!parameters.is_null()) {
			parameters.get_to(value);
		}

		AutoLabelWidthScope label_width{ TypeLabel<T>() };
		if (!DrawDefaultContents(ctx, value)) {
			return false;
		}

		parameters = value;
		return true;
	}
}

bool DrawScreenEffectParameters(
	EditorContext& ctx,
	std::string_view type,
	json& parameters
) {
	bool handled{ false };
	bool changed{ false };

	changed |= DrawTypedScreenEffectParameters<Bloom>(ctx, type, parameters, handled);
	changed |= DrawTypedScreenEffectParameters<Blur>(ctx, type, parameters, handled);
	changed |= DrawTypedScreenEffectParameters<GaussianBlur>(ctx, type, parameters, handled);
	changed |= DrawTypedScreenEffectParameters<Grayscale>(ctx, type, parameters, handled);
	changed |= DrawTypedScreenEffectParameters<InverseColor>(ctx, type, parameters, handled);
	changed |= DrawTypedScreenEffectParameters<Sharpen>(ctx, type, parameters, handled);
	changed |= DrawTypedScreenEffectParameters<EdgeDetection>(ctx, type, parameters, handled);

	if (handled) {
		return changed;
	}

	if (parameters.is_object()) {
		if (parameters.empty()) {
			ImGui::TextDisabled("No editable parameters.");
			return false;
		}

		for (auto it{ parameters.begin() }; it != parameters.end(); ++it) {
			ImGui::PushID(it.key().c_str());
			changed |= DrawScreenEffectJsonValue(it.key().c_str(), it.value());
			ImGui::PopID();
		}
		return changed;
	}

	return DrawScreenEffectJsonValue("Value", parameters);
}

} // namespace

void DrawScreenEffectInspector(EditorContext& ctx, const ScreenEffectSelection& selection) {
	const bool runtime{ ctx.editor.IsPlaying() || ctx.editor.IsDirectRuntime() || selection.runtime };
	Entity entity{ ctx.editor.ResolveScreenEffect(selection) };

	if (!runtime && !selection.runtime) {
		const auto* settings{ ctx.editor.GetProjectScreenEffects() };
		const auto* current{ settings ? FindScreenEffect(*settings, selection.id) : nullptr };
		if (!current) {
			ImGui::TextDisabled("The selected project screen effect no longer exists.");
			return;
		}

		SerializedScreenEffect updated{ *current };
		const auto* registration{ ::ptgn::impl::EffectRegistry::Find(updated.type) };

		ImGui::TextUnformatted(
			registration ? registration->display_name.c_str() : updated.type.c_str()
		);
		if (registration && registration->hdr) {
			ImGui::SameLine();
			ImGui::TextDisabled("HDR");
		}
		ImGui::Separator();

		if (DrawScreenEffectParameters(ctx, updated.type, updated.parameters)) {
			ctx.editor.UpdateProjectScreenEffect(
				selection.id,
				updated,
				"Change Screen Effect",
				0x5346580000000000ULL ^ selection.id
			);
		}

		ImGui::Separator();
		if (ImGui::Button("Remove Screen Effect", ImVec2{ -FLT_MIN, 0.0f })) {
			ctx.editor.DeleteProjectScreenEffect(selection.id);
		}
		return;
	}

	if (!entity || !entity.Has<::ptgn::impl::ScreenEffectInstance>()) {
		ImGui::TextDisabled("The selected runtime screen effect no longer exists.");
		return;
	}

	auto& instance{ entity.Get<::ptgn::impl::ScreenEffectInstance>() };
	const auto* registration{ ::ptgn::impl::EffectRegistry::Find(instance.type) };
	ImGui::TextUnformatted(
		registration ? registration->display_name.c_str() : instance.type.c_str()
	);
	if (registration && registration->hdr) {
		ImGui::SameLine();
		ImGui::TextDisabled("HDR");
	}
	ImGui::SameLine();
	ImGui::TextDisabled("Runtime");
	ImGui::Separator();

	if (!registration || !registration->serialize || !registration->deserialize) {
		ImGui::TextDisabled("This effect type is not registered.");
	} else {
		json parameters = registration->serialize(entity);
		if (DrawScreenEffectParameters(ctx, instance.type, parameters)) {
			ctx.editor.UpdateRuntimeScreenEffect(
				instance.runtime_id,
				parameters,
				"Change Runtime Screen Effect",
				0x5253465800000000ULL ^ instance.runtime_id
			);
		}
	}

	ImGui::Separator();
	if (ImGui::Button("Remove Runtime Effect", ImVec2{ -FLT_MIN, 0.0f })) {
		ctx.editor.DeleteRuntimeScreenEffect(instance.runtime_id);
	}
}

} // namespace ptgn::editor::inspector
