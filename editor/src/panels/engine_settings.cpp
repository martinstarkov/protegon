#include "panels/engine_settings.h"

#include <imgui.h>

#include "core/editor_context.h"

namespace ptgn::editor {

void EngineSettingsPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Engine Settings");
	ImGui::TextUnformatted("Engine settings panel");
	ImGui::End();
}

} // namespace ptgn::editor