#include "panels/content_browser.h"

#include <imgui.h>

#include "core/editor_context.h"

namespace ptgn::editor {

void ContentBrowserPanel::OnRender(EditorContext& ctx) {
	ImGui::Begin("Assets");
	ImGui::TextUnformatted("Assets panel");
	ImGui::End();
}

} // namespace ptgn::editor