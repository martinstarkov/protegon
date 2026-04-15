#include "panels/scene_list.h"

#include <imgui.h>

#include "core/editor_context.h"

namespace ptgn::editor {

void SceneListPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Scenes");
	ImGui::TextUnformatted("Scenes panel");
	ImGui::End();
}

} // namespace ptgn::editor