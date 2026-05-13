#include "panels/render_graph_visualizer.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <queue>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

constexpr float kNodeW		   = 280.0f;
constexpr float kBaseNodeH	   = 150.0f;
constexpr float kNodeGapX	   = 120.0f;
constexpr float kNodeGapY	   = 80.0f;
constexpr float kCanvasPadding = 48.0f;
constexpr float kCanvasH	   = 520.0f;

ImVec2 ToImVec2(V2_float v) {
	return { v.x, v.y };
}

V2_float ToV2Float(ImVec2 v) {
	return { v.x, v.y };
}

struct GraphBounds {
	V2_float min;
	V2_float max;
};

static float ClampPanAxis(float pan, float viewport_size, float bounds_min, float bounds_max) {
	const float content_size = bounds_max - bounds_min;

	if (content_size <= viewport_size) {
		return (viewport_size - content_size) * 0.5f - bounds_min;
	}

	const float min_pan = viewport_size - bounds_max;
	const float max_pan = -bounds_min;

	return std::clamp(pan, min_pan, max_pan);
}

static V2_float ClampGraphPan(V2_float pan, V2_float viewport_size, const GraphBounds& bounds) {
	return {
		ClampPanAxis(pan.x, viewport_size.x, bounds.min.x, bounds.max.x),
		ClampPanAxis(pan.y, viewport_size.y, bounds.min.y, bounds.max.y),
	};
}

static GraphBounds CalculateGraphBounds(
	const impl::DebugRenderGraphSnapshot& snapshot,
	const std::unordered_map<impl::RenderNodeId, V2_float>& node_positions
) {
	if (snapshot.nodes.empty()) {
		return {};
	}

	GraphBounds bounds;
	bool first{ true };

	for (const auto& node : snapshot.nodes) {
		auto it = node_positions.find(node.id);
		if (it == node_positions.end()) {
			continue;
		}

		const V2_float node_min = it->second;
		const V2_float node_max{ node_min.x + kNodeW,
								 node_min.y + RenderGraphVisualizer::GetNodeHeight(node) };

		if (first) {
			bounds.min = node_min;
			bounds.max = node_max;
			first	   = false;
		} else {
			bounds.min.x = std::min(bounds.min.x, node_min.x);
			bounds.min.y = std::min(bounds.min.y, node_min.y);
			bounds.max.x = std::max(bounds.max.x, node_max.x);
			bounds.max.y = std::max(bounds.max.y, node_max.y);
		}
	}

	bounds.min.x -= kCanvasPadding;
	bounds.min.y -= kCanvasPadding;
	bounds.max.x += kCanvasPadding;
	bounds.max.y += kCanvasPadding;

	return bounds;
}

static float NodeHeight(const impl::DebugRenderNodeSnapshot& node) {
	return kBaseNodeH + static_cast<float>(node.reads.size()) * 18.0f +
		   static_cast<float>(node.uniform_count > 0 ? 18.0f : 0.0f);
}

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

	if (ImGui::Button("Auto layout")) {
		AutoLayoutGraph(snapshot, true);
	}

	ImGui::SameLine();
	ImGui::TextDisabled("Left-drag nodes. Middle/right-drag canvas.");

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
	EnsureGraphLayout(snapshot);

	ImGui::BeginChild(
		"RenderGraphCanvas", ImVec2(0.0f, kCanvasH), true,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
	);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	const V2_float canvas_min  = ToV2Float(ImGui::GetCursorScreenPos());
	const V2_float canvas_size = ToV2Float(ImGui::GetContentRegionAvail());
	const V2_float canvas_max{
		canvas_min.x + canvas_size.x,
		canvas_min.y + canvas_size.y,
	};

	draw_list->AddRectFilled(ToImVec2(canvas_min), ToImVec2(canvas_max), IM_COL32(24, 24, 28, 255));
	draw_list->AddRect(ToImVec2(canvas_min), ToImVec2(canvas_max), IM_COL32(70, 70, 76, 255));

	const auto bounds = CalculateGraphBounds(snapshot, node_positions_);
	graph_pan_		  = ClampGraphPan(graph_pan_, canvas_size, bounds);

	std::unordered_map<impl::RenderNodeId, NodeLayout> layouts;
	layouts.reserve(snapshot.nodes.size());

	for (const auto& node : snapshot.nodes) {
		auto position_it = node_positions_.find(node.id);

		if (position_it == node_positions_.end()) {
			continue;
		}

		const V2_float screen_min{
			canvas_min.x + graph_pan_.x + position_it->second.x,
			canvas_min.y + graph_pan_.y + position_it->second.y,
		};

		const V2_float screen_max{
			screen_min.x + kNodeW,
			screen_min.y + GetNodeHeight(node),
		};

		layouts[node.id] = NodeLayout{ .min = screen_min, .max = screen_max };
	}

	draw_list->PushClipRect(ToImVec2(canvas_min), ToImVec2(canvas_max), true);
	draw_list->ChannelsSplit(2);

	draw_list->ChannelsSetCurrent(0);

	for (const auto& edge : snapshot.edges) {
		auto from_it = layouts.find(edge.from);
		auto to_it	 = layouts.find(edge.to);

		if (from_it == layouts.end() || to_it == layouts.end()) {
			continue;
		}

		const auto& from = from_it->second;
		const auto& to	 = to_it->second;

		V2_float p1{ from.max.x, (from.min.y + from.max.y) * 0.5f };
		V2_float p2{ to.min.x, (to.min.y + to.max.y) * 0.5f };

		V2_float c1{ p1.x + 60.0f, p1.y };
		V2_float c2{ p2.x - 60.0f, p2.y };

		draw_list->AddBezierCubic(
			ToImVec2(p1), ToImVec2(c1), ToImVec2(c2), ToImVec2(p2), IM_COL32(180, 180, 180, 180),
			2.0f
		);

		const V2_float label_pos{
			(p1.x + p2.x) * 0.5f - 30.0f,
			(p1.y + p2.y) * 0.5f - 10.0f,
		};

		draw_list->AddText(ToImVec2(label_pos), IM_COL32(220, 220, 220, 220), edge.label.c_str());
	}

	draw_list->ChannelsSetCurrent(1);

	for (const auto& node : snapshot.nodes) {
		auto layout_it = layouts.find(node.id);

		if (layout_it == layouts.end()) {
			continue;
		}

		DrawNode(snapshot, node, layout_it->second);
	}

	draw_list->ChannelsMerge();
	draw_list->PopClipRect();

	const bool canvas_hovered =
		ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

	const bool panning = canvas_hovered && !ImGui::IsAnyItemActive() &&
						 (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
						  ImGui::IsMouseDragging(ImGuiMouseButton_Right));

	if (panning) {
		const V2_float delta = ToV2Float(ImGui::GetIO().MouseDelta);

		graph_pan_.x += delta.x;
		graph_pan_.y += delta.y;

		graph_pan_ = ClampGraphPan(graph_pan_, canvas_size, bounds);
	}

	if (canvas_hovered && !ImGui::IsAnyItemHovered()) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	}

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

	draw_list->AddRectFilled(ToImVec2(layout.min), ToImVec2(layout.max), bg, 8.0f);
	draw_list->AddRect(ToImVec2(layout.min), ToImVec2(layout.max), border, 8.0f, 0, 1.5f);

	const V2_float header_min = layout.min;
	const V2_float header_max{ layout.max.x, layout.min.y + 30.0f };

	draw_list->AddRectFilled(
		ToImVec2(header_min), ToImVec2(header_max), header, 8.0f, ImDrawFlags_RoundCornersTop
	);

	V2_float p{ layout.min.x + 10.0f, layout.min.y + 7.0f };
	draw_list->AddText(ToImVec2(p), text, node.name.c_str());

	p.y += 34.0f;
	draw_list->AddText(
		ToImVec2(p), dim,
		("#" + std::to_string(node.id) + " " + std::string{ magic_enum::enum_name(node.type) })
			.c_str()
	);

	p.y += 18.0f;
	draw_list->AddText(ToImVec2(p), dim, ("pipeline: " + std::to_string(node.pipeline)).c_str());

	p.y += 18.0f;
	draw_list->AddText(ToImVec2(p), dim, ("shader: " + std::to_string(node.shader)).c_str());

	if (node.state.blend_mode.has_value()) {
		p.y += 18.0f;
		draw_list->AddText(
			ToImVec2(p), dim,
			("blend: " + std::string{ magic_enum::enum_name(*node.state.blend_mode) }).c_str()
		);
	}

	p.y += 18.0f;
	draw_list->AddText(
		ToImVec2(p), dim, ("draw items: " + std::to_string(node.draw_item_count)).c_str()
	);

	p.y += 18.0f;
	draw_list->AddText(
		ToImVec2(p), dim, ("uniforms: " + std::to_string(node.uniform_count)).c_str()
	);

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

		draw_list->AddText(ToImVec2(p), dim, output_text.c_str());
	}

	for (const auto& read : node.reads) {
		const auto* resource  = FindResource(snapshot, read.resource);
		p.y					 += 18.0f;

		std::string read_text = "read: #" + std::to_string(read.resource);

		if (resource) {
			read_text += " " + resource->name;
		}

		read_text += " as " + read.uniform_name;

		draw_list->AddText(ToImVec2(p), dim, read_text.c_str());
	}

	ImGui::SetCursorScreenPos(ToImVec2(layout.min));
	ImGui::InvisibleButton(
		("node_drag_" + std::to_string(node.id)).c_str(),
		ImVec2{ layout.max.x - layout.min.x, layout.max.y - layout.min.y },
		ImGuiButtonFlags_MouseButtonLeft
	);

	if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
		const V2_float delta = ToV2Float(ImGui::GetIO().MouseDelta);

		auto& position	= node_positions_[node.id];
		position.x	   += delta.x;
		position.y	   += delta.y;
	}

	if (ImGui::IsItemHovered()) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
		DrawNodeTooltip(snapshot, node);
	}
}

float RenderGraphVisualizer::GetNodeHeight(const impl::DebugRenderNodeSnapshot& node) {
	return NodeHeight(node);
}

void RenderGraphVisualizer::PruneMissingNodePositions(const impl::DebugRenderGraphSnapshot& snapshot
) {
	std::unordered_set<impl::RenderNodeId> live_nodes;
	live_nodes.reserve(snapshot.nodes.size());

	for (const auto& node : snapshot.nodes) {
		live_nodes.emplace(node.id);
	}

	std::erase_if(node_positions_, [&](const auto& entry) {
		return !live_nodes.contains(entry.first);
	});
}

void RenderGraphVisualizer::EnsureGraphLayout(const impl::DebugRenderGraphSnapshot& snapshot) {
	PruneMissingNodePositions(snapshot);

	const bool reset_existing = node_positions_.empty();
	AutoLayoutGraph(snapshot, reset_existing);
}

void RenderGraphVisualizer::AutoLayoutGraph(
	const impl::DebugRenderGraphSnapshot& snapshot, bool reset_existing
) {
	if (snapshot.nodes.empty()) {
		node_positions_.clear();
		graph_pan_ = {};
		return;
	}

	std::unordered_map<impl::RenderNodeId, std::size_t> node_index;
	node_index.reserve(snapshot.nodes.size());

	for (std::size_t i{ 0 }; i < snapshot.nodes.size(); ++i) {
		node_index.emplace(snapshot.nodes[i].id, i);
	}

	std::unordered_map<impl::RenderNodeId, int> in_degree;
	std::unordered_map<impl::RenderNodeId, int> level;
	std::unordered_map<impl::RenderNodeId, std::vector<impl::RenderNodeId>> outgoing;

	in_degree.reserve(snapshot.nodes.size());
	level.reserve(snapshot.nodes.size());
	outgoing.reserve(snapshot.nodes.size());

	for (const auto& node : snapshot.nodes) {
		in_degree[node.id] = 0;
		level[node.id]	   = 0;
	}

	for (const auto& edge : snapshot.edges) {
		if (!node_index.contains(edge.from) || !node_index.contains(edge.to)) {
			continue;
		}

		outgoing[edge.from].push_back(edge.to);
		++in_degree[edge.to];
	}

	if (snapshot.edges.empty()) {
		for (std::size_t i{ 0 }; i < snapshot.nodes.size(); ++i) {
			level[snapshot.nodes[i].id] = static_cast<int>(i);
		}
	} else {
		std::queue<impl::RenderNodeId> queue;

		for (const auto& node : snapshot.nodes) {
			if (in_degree[node.id] == 0) {
				queue.push(node.id);
			}
		}

		while (!queue.empty()) {
			const auto from = queue.front();
			queue.pop();

			for (const auto to : outgoing[from]) {
				level[to] = std::max(level[to], level[from] + 1);

				if (--in_degree[to] == 0) {
					queue.push(to);
				}
			}
		}

		int fallback_level{ 0 };

		for (const auto& node : snapshot.nodes) {
			if (in_degree[node.id] > 0) {
				level[node.id] = fallback_level++;
			}
		}
	}

	int max_level{ 0 };

	for (const auto& [_, node_level] : level) {
		max_level = std::max(max_level, node_level);
	}

	std::vector<std::vector<const impl::DebugRenderNodeSnapshot*>> columns(
		static_cast<std::size_t>(max_level + 1)
	);

	for (const auto& node : snapshot.nodes) {
		columns[static_cast<std::size_t>(level[node.id])].push_back(&node);
	}

	float max_node_h{ kBaseNodeH };

	for (const auto& node : snapshot.nodes) {
		max_node_h = std::max(max_node_h, GetNodeHeight(node));
	}

	const float x_pitch = kNodeW + kNodeGapX;
	const float y_pitch = max_node_h + kNodeGapY;

	for (std::size_t column_index{ 0 }; column_index < columns.size(); ++column_index) {
		auto& column = columns[column_index];

		std::ranges::sort(column, [&](const auto* a, const auto* b) {
			return node_index[a->id] < node_index[b->id];
		});

		for (std::size_t row_index{ 0 }; row_index < column.size(); ++row_index) {
			const auto& node = *column[row_index];

			if (!reset_existing && node_positions_.contains(node.id)) {
				continue;
			}

			node_positions_[node.id] = V2_float{
				kCanvasPadding + static_cast<float>(column_index) * x_pitch,
				kCanvasPadding + static_cast<float>(row_index) * y_pitch,
			};
		}
	}

	if (reset_existing) {
		graph_pan_ = {};
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