#include "panels/inspector.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <string>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/math/transform.h"
#include "core/util/hash.h"
#include "core/util/type_info.h"
#include "panels/scene_hierarchy.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/tint.h"

namespace ptgn::editor {

template <typename T>
inline constexpr int type_id_value = 0;

template <typename T>
bool DrawComponentHeader(Entity selected_entity, bool non_removable) {
	ImGui::PushID(&type_id_value<T>);

	const char* popup_id = "ComponentContextMenu";
	bool popup_open		 = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);

	if (popup_open) {
		ImVec4 active = ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive);
		ImGui::PushStyleColor(ImGuiCol_Header, active);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, active);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, active);
	}

	std::string name{ type_name_without_namespaces<T>() };

	bool open = ImGui::CollapsingHeader(name.c_str());

	if (popup_open) {
		ImGui::PopStyleColor(3);
	}

	if (!non_removable) {
		if (ImGui::BeginPopupContextItem(popup_id)) {
			if (ImGui::MenuItem("Remove Component")) {
				selected_entity.Remove<T>();
			}
			ImGui::EndPopup();
		}
	}

	ImGui::PopID();

	return open;
}

static void DrawComponentImpl(impl::Tint& tint) {
	constexpr float kLabelWidth = 70.0f;
	constexpr float kSpacing	= 6.0f;

	auto DrawCenteredLabel = [](const char* text, float width) {
		float text_width = ImGui::CalcTextSize(text).x;
		float cursor_x	 = ImGui::GetCursorPosX();
		float offset	 = (width - text_width) * 0.5f;
		if (offset > 0.0f) {
			ImGui::SetCursorPosX(cursor_x + offset);
		}
		ImGui::TextUnformatted(text);
	};

	auto ClampByte = [](int v) -> int {
		return std::clamp(v, 0, 255);
	};

	float available_width = ImGui::GetContentRegionAvail().x;
	float right_width	  = available_width - kLabelWidth;

	// 4 sliders + spacing + color button
	constexpr float kPickerWidth = 36.0f;
	float field_width			 = (right_width - 3.0f * kSpacing - kPickerWidth - kSpacing) / 4.0f;
	float sublabel_height		 = ImGui::GetTextLineHeight();

	ImGui::BeginGroup();
	ImGui::Dummy(ImVec2(0.0f, sublabel_height));
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Tint");
	ImGui::EndGroup();

	ImGui::SameLine(kLabelWidth);

	ImGui::BeginGroup();

	int r = static_cast<int>(tint.r);
	int g = static_cast<int>(tint.g);
	int b = static_cast<int>(tint.b);
	int a = static_cast<int>(tint.a);

	float color[4]{ static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
					static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f };

	bool changed_from_sliders{ false };
	bool changed_from_picker{ false };

	ImGui::BeginGroup();
	DrawCenteredLabel("R", field_width);
	ImGui::SetNextItemWidth(field_width);
	changed_from_sliders |=
		ImGui::DragInt("##TintR", &r, 1.0f, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);
	ImGui::EndGroup();

	ImGui::SameLine(0.0f, kSpacing);

	ImGui::BeginGroup();
	DrawCenteredLabel("G", field_width);
	ImGui::SetNextItemWidth(field_width);
	changed_from_sliders |=
		ImGui::DragInt("##TintG", &g, 1.0f, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);
	ImGui::EndGroup();

	ImGui::SameLine(0.0f, kSpacing);

	ImGui::BeginGroup();
	DrawCenteredLabel("B", field_width);
	ImGui::SetNextItemWidth(field_width);
	changed_from_sliders |=
		ImGui::DragInt("##TintB", &b, 1.0f, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);
	ImGui::EndGroup();

	ImGui::SameLine(0.0f, kSpacing);

	ImGui::BeginGroup();
	DrawCenteredLabel("A", field_width);
	ImGui::SetNextItemWidth(field_width);
	changed_from_sliders |=
		ImGui::DragInt("##TintA", &a, 1.0f, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);
	ImGui::EndGroup();

	ImGui::SameLine(0.0f, kSpacing);

	ImGui::BeginGroup();
	DrawCenteredLabel("", kPickerWidth);
	ImGui::SetNextItemWidth(kPickerWidth);
	changed_from_picker |= ImGui::ColorEdit4(
		"##TintPicker", color,
		ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar |
			ImGuiColorEditFlags_AlphaPreviewHalf
	);
	ImGui::EndGroup();

	if (changed_from_sliders) {
		r = ClampByte(r);
		g = ClampByte(g);
		b = ClampByte(b);
		a = ClampByte(a);

		tint.r = static_cast<std::uint8_t>(r);
		tint.g = static_cast<std::uint8_t>(g);
		tint.b = static_cast<std::uint8_t>(b);
		tint.a = static_cast<std::uint8_t>(a);
	} else if (changed_from_picker) {
		tint.r = static_cast<std::uint8_t>(std::round(color[0] * 255.0f));
		tint.g = static_cast<std::uint8_t>(std::round(color[1] * 255.0f));
		tint.b = static_cast<std::uint8_t>(std::round(color[2] * 255.0f));
		tint.a = static_cast<std::uint8_t>(std::round(color[3] * 255.0f));
	}

	ImGui::EndGroup();
}

static void DrawComponentImpl(Transform& transform, Depth& depth) {
	constexpr float kLabelWidth	 = 70.0f;
	constexpr float kSpacing	 = 6.0f;
	constexpr float kMinScaleAbs = 0.001f;

	auto ClampScaleAwayFromZero = [](float& value) {
		constexpr float kMin = 0.001f;

		if (value == 0.0f) {
			value = kMin; // default to positive side
		} else if (value > 0.0f && value < kMin) {
			value = kMin;
		} else if (value < 0.0f && value > -kMin) {
			value = -kMin;
		}
	};

	auto DrawCenteredLabel = [](const char* text, float width) {
		float text_width = ImGui::CalcTextSize(text).x;
		float cursor_x	 = ImGui::GetCursorPosX();
		float offset	 = (width - text_width) * 0.5f;
		if (offset > 0.0f) {
			ImGui::SetCursorPosX(cursor_x + offset);
		}
		ImGui::TextUnformatted(text);
	};

	// Position
	{
		float available_width = ImGui::GetContentRegionAvail().x;
		float right_width	  = available_width - kLabelWidth;
		float field_width	  = (right_width - 2.0f * kSpacing) / 3.0f;
		float sublabel_height = ImGui::GetTextLineHeight();

		ImGui::BeginGroup();
		ImGui::Dummy(ImVec2(0.0f, sublabel_height));
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Position");
		ImGui::EndGroup();

		ImGui::SameLine(kLabelWidth);

		ImGui::BeginGroup();

		ImGui::BeginGroup();
		DrawCenteredLabel("X", field_width);
		ImGui::SetNextItemWidth(field_width);
		float x{ transform.GetPosition().x };
		ImGui::DragFloat("##PositionX", &x, 1.0f, 0.0f, 0.0f, "%.0f");
		transform.SetPositionX(x);
		ImGui::EndGroup();

		ImGui::SameLine(0.0f, kSpacing);

		ImGui::BeginGroup();
		DrawCenteredLabel("Y", field_width);
		ImGui::SetNextItemWidth(field_width);
		float y{ transform.GetPosition().y };
		ImGui::DragFloat("##PositionY", &y, 1.0f, 0.0f, 0.0f, "%.0f");
		transform.SetPositionY(y);
		ImGui::EndGroup();

		ImGui::SameLine(0.0f, kSpacing);

		ImGui::BeginGroup();
		DrawCenteredLabel("Depth", field_width);
		ImGui::SetNextItemWidth(field_width);
		float d{ depth };
		ImGui::DragFloat("##PositionDepth", &d, 0.05f, -1000.0f, 1000.0f, "%.0f");
		depth = d;
		ImGui::EndGroup();

		ImGui::EndGroup();
	}

	ImGui::Spacing();

	// Scale
	{
		float available_width = ImGui::GetContentRegionAvail().x;
		float right_width	  = available_width - kLabelWidth;
		float field_width	  = (right_width - kSpacing) / 2.0f;
		float sublabel_height = ImGui::GetTextLineHeight();

		ImGui::BeginGroup();
		ImGui::Dummy(ImVec2(0.0f, sublabel_height));
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Scale");
		ImGui::EndGroup();

		ImGui::SameLine(kLabelWidth);

		ImGui::BeginGroup();

		ImGui::BeginGroup();
		DrawCenteredLabel("X", field_width);
		ImGui::SetNextItemWidth(field_width);
		float x{ transform.GetScale().x };
		if (ImGui::DragFloat(
				"##ScaleX", &x, 0.01f, -1000.0f, 1000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp
			)) {
			ClampScaleAwayFromZero(x);
		}
		transform.SetScaleX(x);
		ImGui::EndGroup();

		ImGui::SameLine(0.0f, kSpacing);

		ImGui::BeginGroup();
		DrawCenteredLabel("Y", field_width);
		ImGui::SetNextItemWidth(field_width);
		float y{ transform.GetScale().y };
		if (ImGui::DragFloat(
				"##ScaleY", &y, 0.01f, -1000.0f, 1000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp
			)) {
			ClampScaleAwayFromZero(y);
		}
		transform.SetScaleY(y);
		ImGui::EndGroup();

		ImGui::EndGroup();
	}

	ImGui::Spacing();

	// Rotation (now below Scale)
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Rotation");
	ImGui::SameLine(kLabelWidth);

	{
		float available_width = ImGui::GetContentRegionAvail().x;
		ImGui::SetNextItemWidth(available_width - kLabelWidth);
		float r{ transform.GetRotation().value };
		ImGui::DragFloat(
			"##Rotation", &r, 1.0f, 0.0f, 360.0f, "%.1f deg", ImGuiSliderFlags_AlwaysClamp
		);
		transform.SetRotation(Degrees{ r });
	}
}

void InspectorPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Inspector");

	auto& scene_hierarchy{ ctx.editor.GetSceneHierarchyPanel() };

	auto selected_entity{ scene_hierarchy.GetSelectedEntity() };

	if (!selected_entity) {
		ImGui::End();
		return;
	}

	static std::string name = selected_entity.GetTag();

	if (ImGui::InputText("Name", &name)) {
		selected_entity.SetTag(name);
	}

	ImGui::Separator();

	if (DrawComponentHeader<Transform>(selected_entity, true)) {
		ImGui::Spacing();
		DrawComponentImpl(selected_entity.TryAdd<Transform>(), selected_entity.TryAdd<Depth>());
		ImGui::Spacing();
	}

	if (selected_entity.Has<impl::Tint>()) {
		if (DrawComponentHeader<impl::Tint>(selected_entity, false)) {
			if (selected_entity.Has<impl::Tint>()) {
				ImGui::Spacing();
				DrawComponentImpl(selected_entity.Get<impl::Tint>());
				ImGui::Spacing();
			}
		}
	}

	ImGui::Separator();

	if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f))) {
		ImGui::OpenPopup("AddComponentPopupButton");
	}

	if (ImGui::BeginPopup("AddComponentPopupButton")) {
		if (ImGui::MenuItem("Tint")) {
			selected_entity.Add<impl::Tint>();
			// Add component to entity.
		}
		ImGui::EndPopup();
	}

	ImGui::End();
}

} // namespace ptgn::editor