#include "panels/inspector.h"

#include <imgui.h>

#include "core/editor_context.h"

namespace ptgn::editor {

void InspectorPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Inspector");
	ImGui::TextUnformatted("Inspector panel");
	ImGui::End();
}

} // namespace ptgn::editor