#include "panels/render_graph_visualizer.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <unordered_map>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/editor_state.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "panels/viewport.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/texture_format.h"
#include "runtime/scene/scene_camera.h"

namespace ptgn::editor {

void RenderGraphVisualizer::Draw(const impl::DebugRenderGraphSnapshot& snapshot, bool* open) {
	if (!ImGui::Begin("Render Graph", open)) {
		ImGui::End();
		return;
	}

	DrawContents(snapshot);

	ImGui::End();
}

void RenderGraphVisualizer::DrawContents(const impl::DebugRenderGraphSnapshot& snapshot) {
	if (!snapshot.valid) {
		ImGui::TextUnformatted("No compiled render graph snapshot available.");
		return;
	}

	ImGui::Text("Frame: %llu", static_cast<unsigned long long>(snapshot.frame_index));
	ImGui::Text(
		"Nodes: %zu | Resources: %zu | Edges: %zu", snapshot.nodes.size(),
		snapshot.resources.size(), snapshot.edges.size()
	);

	if (ImGui::CollapsingHeader("Resources", ImGuiTreeNodeFlags_DefaultOpen)) {
		DrawResourceTable(snapshot);
	}

	ImGui::Separator();

	ImGui::TextUnformatted("Graph");
	DrawGraphCanvas(snapshot);
}

const impl::DebugRenderResourceSnapshot* RenderGraphVisualizer::FindResource(
	const impl::DebugRenderGraphSnapshot& snapshot, impl::RenderResourceId id
) {
	auto it = std::ranges::find_if(snapshot.resources, [&](const auto& r) { return r.id == id; });

	if (it == snapshot.resources.end()) {
		return nullptr;
	}

	return &*it;
}

const impl::DebugRenderNodeSnapshot* RenderGraphVisualizer::FindNode(
	const impl::DebugRenderGraphSnapshot& snapshot, impl::RenderNodeId id
) {
	auto it = std::ranges::find_if(snapshot.nodes, [&](const auto& n) { return n.id == id; });

	if (it == snapshot.nodes.end()) {
		return nullptr;
	}

	return &*it;
}

Color RenderGraphVisualizer::NodeColor(impl::RenderNodeType type) {
	switch (type) {
		case impl::RenderNodeType::DrawLayer:	   return Color(70, 105, 160, 255);

		case impl::RenderNodeType::FullscreenPass: return Color(95, 125, 75, 255);

		case impl::RenderNodeType::Clear:		   return Color(120, 90, 60, 255);

		case impl::RenderNodeType::Present:		   return Color(130, 80, 140, 255);

		default:								   return Color(90, 90, 90, 255);
	}
}

void RenderGraphVisualizer::DrawResourceTable(const impl::DebugRenderGraphSnapshot& snapshot) {
	if (!ImGui::BeginTable(
			"RenderGraphResources", 8,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
				ImGuiTableFlags_SizingStretchProp
		)) {
		return;
	}

	ImGui::TableSetupColumn("Id");
	ImGui::TableSetupColumn("Name");
	ImGui::TableSetupColumn("Kind");
	ImGui::TableSetupColumn("Size");
	ImGui::TableSetupColumn("Format");
	ImGui::TableSetupColumn("Physical Target");
	ImGui::TableSetupColumn("Lifetime");
	ImGui::TableSetupColumn("Used");
	ImGui::TableHeadersRow();

	for (const auto& r : snapshot.resources) {
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text("%u", r.id);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(r.name.c_str());

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(r.imported ? "Imported" : "Transient");

		ImGui::TableNextColumn();
		ImGui::Text("%d x %d", r.size.x, r.size.y);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(std::string{ ToString(r.format) }.c_str());

		ImGui::TableNextColumn();
		if (r.physical_target.has_value()) {
			ImGui::Text("%u", *r.physical_target);
		} else {
			ImGui::TextUnformatted("-");
		}

		ImGui::TableNextColumn();
		if (r.used) {
			ImGui::Text("%zu -> %zu", r.first_use, r.last_use);
		} else {
			ImGui::TextUnformatted("-");
		}

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(r.used ? "yes" : "no");
	}

	ImGui::EndTable();
}

void RenderGraphVisualizer::DrawGraphCanvas(const impl::DebugRenderGraphSnapshot& snapshot) {
	constexpr float node_w		   = 280.0f;
	constexpr float base_node_h	   = 150.0f;
	constexpr float node_gap_x	   = 90.0f;
	constexpr float node_gap_y	   = 90.0f;
	constexpr float canvas_padding = 24.0f;

	const ImVec2 canvas_origin = ImGui::GetCursorScreenPos();
	const float canvas_h	   = 520.0f;
	const float canvas_w =
		std::max(900.0f, canvas_padding * 2.0f + snapshot.nodes.size() * (node_w + node_gap_x));

	ImGui::BeginChild(
		"RenderGraphCanvas", ImVec2(0.0f, canvas_h), true, ImGuiWindowFlags_HorizontalScrollbar
	);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	const ImVec2 origin	  = ImGui::GetCursorScreenPos();

	std::unordered_map<impl::RenderNodeId, NodeLayout> layouts;
	layouts.reserve(snapshot.nodes.size());

	for (std::size_t i = 0; i < snapshot.nodes.size(); ++i) {
		const auto& node = snapshot.nodes[i];

		const float x = origin.x + canvas_padding + static_cast<float>(i) * (node_w + node_gap_x);

		// Slight zigzag so edges are easier to see.
		const float y = origin.y + canvas_padding + static_cast<float>(i % 2) * node_gap_y;

		const float node_h = base_node_h + static_cast<float>(node.reads.size()) * 18.0f +
							 static_cast<float>(node.uniform_count > 0 ? 18.0f : 0.0f);

		layouts[node.id] = NodeLayout{ .min = { x, y }, .max = { x + node_w, y + node_h } };
	}

	draw_list->ChannelsSplit(2);

	// Edges behind nodes.
	draw_list->ChannelsSetCurrent(0);
	for (const auto& edge : snapshot.edges) {
		auto from_it = layouts.find(edge.from);
		auto to_it	 = layouts.find(edge.to);

		if (from_it == layouts.end() || to_it == layouts.end()) {
			continue;
		}

		const auto& from = from_it->second;
		const auto& to	 = to_it->second;

		ImVec2 p1{ from.max.x, (from.min.y + from.max.y) * 0.5f };
		ImVec2 p2{ to.min.x, (to.min.y + to.max.y) * 0.5f };

		ImVec2 c1{ p1.x + 50.0f, p1.y };
		ImVec2 c2{ p2.x - 50.0f, p2.y };

		draw_list->AddBezierCubic(p1, c1, c2, p2, IM_COL32(180, 180, 180, 180), 2.0f);

		const ImVec2 label_pos{ (p1.x + p2.x) * 0.5f - 30.0f, (p1.y + p2.y) * 0.5f - 10.0f };

		draw_list->AddText(label_pos, IM_COL32(220, 220, 220, 220), edge.label.c_str());
	}

	// Nodes.
	draw_list->ChannelsSetCurrent(1);
	for (const auto& node : snapshot.nodes) {
		DrawNode(snapshot, node, layouts.at(node.id));
	}

	draw_list->ChannelsMerge();

	ImGui::Dummy(ImVec2(canvas_w, canvas_h - 32.0f));
	ImGui::EndChild();
}

void RenderGraphVisualizer::DrawNode(
	const impl::DebugRenderGraphSnapshot& snapshot, const impl::DebugRenderNodeSnapshot& node,
	const NodeLayout& layout
) {
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	const ImU32 bg	   = IM_COL32(38, 38, 42, 255);
	const ImU32 border = IM_COL32(180, 180, 185, 255);
	auto color{ NodeColor(node.type) };
	const ImU32 header = IM_COL32(color.r, color.g, color.b, color.a);
	const ImU32 text   = IM_COL32(240, 240, 240, 255);
	const ImU32 dim	   = IM_COL32(190, 190, 190, 255);

	draw_list->AddRectFilled(
		{ layout.min.x, layout.min.y }, { layout.max.x, layout.max.y }, bg, 8.0f
	);
	draw_list->AddRect(
		{ layout.min.x, layout.min.y }, { layout.max.x, layout.max.y }, border, 8.0f, 0, 1.5f
	);

	const ImVec2 header_min = { layout.min.x, layout.min.y };
	const ImVec2 header_max{ layout.max.x, layout.min.y + 30.0f };
	draw_list->AddRectFilled(header_min, header_max, header, 8.0f, ImDrawFlags_RoundCornersTop);

	ImVec2 p{ layout.min.x + 10.0f, layout.min.y + 7.0f };
	draw_list->AddText(p, text, node.name.c_str());

	p.y += 34.0f;
	draw_list->AddText(
		p, dim,
		("#" + std::to_string(node.id) + " " + std::string{ magic_enum::enum_name(node.type) })
			.c_str()
	);

	p.y += 18.0f;
	draw_list->AddText(p, dim, ("pipeline: " + std::to_string(node.pipeline)).c_str());

	p.y += 18.0f;
	draw_list->AddText(p, dim, ("shader: " + std::to_string(node.shader)).c_str());

	if (node.state.blend_mode.has_value()) {
		p.y += 18.0f;
		draw_list->AddText(
			p, dim,
			("blend: " + std::string{ magic_enum::enum_name(*node.state.blend_mode) }).c_str()
		);
	}

	p.y += 18.0f;
	draw_list->AddText(p, dim, ("draw items: " + std::to_string(node.draw_item_count)).c_str());

	p.y += 18.0f;
	draw_list->AddText(p, dim, ("uniforms: " + std::to_string(node.uniform_count)).c_str());

	if (node.has_output) {
		const auto* resource  = FindResource(snapshot, node.output);
		p.y					 += 18.0f;

		std::string output_text = "out: #" + std::to_string(node.output);

		if (resource) {
			output_text += " " + resource->name;

			if (resource->physical_target.has_value()) {
				output_text += " -> RT " + std::to_string(*resource->physical_target);
			}
		}

		draw_list->AddText(p, dim, output_text.c_str());
	}

	for (const auto& read : node.reads) {
		const auto* resource  = FindResource(snapshot, read.resource);
		p.y					 += 18.0f;

		std::string read_text = "read: #" + std::to_string(read.resource);

		if (resource) {
			read_text += " " + resource->name;
		}

		read_text += " as " + read.uniform_name;

		draw_list->AddText(p, dim, read_text.c_str());
	}

	// Invisible button for tooltip.
	ImGui::SetCursorScreenPos({ layout.min.x, layout.min.y });
	ImGui::InvisibleButton(
		("node_hover_" + std::to_string(node.id)).c_str(),
		ImVec2{ layout.max.x - layout.min.x, layout.max.y - layout.min.y }
	);

	if (ImGui::IsItemHovered()) {
		DrawNodeTooltip(snapshot, node);
	}
}

void RenderGraphVisualizer::DrawNodeTooltip(
	const impl::DebugRenderGraphSnapshot& snapshot, const impl::DebugRenderNodeSnapshot& node
) {
	ImGui::BeginTooltip();

	ImGui::Text("Node #%u", node.id);
	ImGui::Text("Name: %s", node.name.c_str());
	ImGui::Text("Type: %s", std::string{ magic_enum::enum_name(node.type) }.c_str());
	ImGui::Separator();

	ImGui::Text("Pipeline: %zu", node.pipeline);
	ImGui::Text("Shader: %u", node.shader);
	ImGui::Text("Uniform count: %zu", node.uniform_count);
	ImGui::Text("Draw item count: %zu", node.draw_item_count);
	if (node.state.blend_mode.has_value()) {
		ImGui::Text(
			"Blend: %s", std::string{ magic_enum::enum_name(*node.state.blend_mode) }.c_str()
		);
	}

	if (node.has_output) {
		ImGui::Separator();

		const auto* resource = FindResource(snapshot, node.output);
		ImGui::Text("Output resource: #%u", node.output);

		if (resource) {
			ImGui::Text("Name: %s", resource->name.c_str());
			ImGui::Text("Size: %d x %d", resource->size.x, resource->size.y);
			ImGui::Text("Format: %s", std::string{ ToString(resource->format) }.c_str());
			ImGui::Text("Kind: %s", resource->imported ? "Imported" : "Transient");

			if (resource->physical_target.has_value()) {
				ImGui::Text("Physical target: %u", *resource->physical_target);
			}

			if (resource->used) {
				ImGui::Text("Lifetime: %zu -> %zu", resource->first_use, resource->last_use);
			}
		}
	}

	if (!node.reads.empty()) {
		ImGui::Separator();
		ImGui::TextUnformatted("Reads:");

		for (const auto& read : node.reads) {
			const auto* resource = FindResource(snapshot, read.resource);

			if (resource) {
				ImGui::BulletText(
					"#%u %s as %s", read.resource, resource->name.c_str(), read.uniform_name.c_str()
				);
			} else {
				ImGui::BulletText("#%u as %s", read.resource, read.uniform_name.c_str());
			}
		}
	}

	ImGui::EndTooltip();
}

} // namespace ptgn::editor