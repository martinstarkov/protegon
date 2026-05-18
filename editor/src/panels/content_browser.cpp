#include "panels/content_browser.h"

#include <imgui.h>

#include "core/editor.h"
#include "core/editor_context.h"
#include "panels/render_graph_visualizer.h"

namespace ptgn::editor {

void ContentBrowserPanel::OnRender(EditorContext& ctx) {
	if (!ImGui::Begin("Assets")) {
		ImGui::End();
		return;
	}

	if (ImGui::BeginTabBar("AssetsTabs")) {
		if (ImGui::BeginTabItem("Content Browser")) {
			DrawContentBrowser(ctx);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Render Graph")) {
			// render_graph_visualizer_.DrawContents(ctx.editor.GetRenderGraphSnapshot());
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

void ContentBrowserPanel::DrawContentBrowser(EditorContext& ctx) {
	ImGui::TextUnformatted("Assets panel");
}

} // namespace ptgn::editor