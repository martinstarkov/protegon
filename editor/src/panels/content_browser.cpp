#include "panels/content_browser.h"

#include <imgui.h>

#include <string>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/util/string.h"
#include "tools/debug/stats.h"

namespace ptgn::editor {

void ContentBrowserPanel::OnRender(EditorContext& ctx) {
	if (ImGui::Begin("Content Browser")) {
		DrawContentBrowser(ctx);
	}
	ImGui::End();

	if (ImGui::Begin("Render Graph")) {
		ImGui::TextUnformatted("Render graph...");
		// render_graph_visualizer_.DrawContents(ctx.editor.GetRenderGraphSnapshot());
	}
	ImGui::End();

	if (ImGui::Begin("Render Stats")) {
		const auto& stats{ ctx.editor.GetStats() };
		auto draw_calls{ "Draw calls: " + ToString(stats.GetCount("draw_calls")) };
		ImGui::TextUnformatted(draw_calls.c_str());
	}
	ImGui::End();
}

void ContentBrowserPanel::DrawContentBrowser(EditorContext& ctx) {
	ImGui::TextUnformatted("Content...");
}

} // namespace ptgn::editor