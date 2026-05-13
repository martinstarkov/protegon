#pragma once

#include <unordered_map>

#include "core/editor_context.h"
#include "core/graphics/color.h"
#include "core/math/angle.h"
#include "core/math/matrix4.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"

namespace ptgn::editor {

class RenderGraphVisualizer {
public:
	void Draw(const impl::DebugRenderGraphSnapshot& snapshot, bool* open = nullptr);

	void DrawContents(const impl::DebugRenderGraphSnapshot& snapshot);

	static float GetNodeHeight(const impl::DebugRenderNodeSnapshot& node);

private:
	struct NodeLayout {
		V2_float min;
		V2_float max;
	};

	std::unordered_map<impl::RenderNodeId, V2_float> node_positions_;
	V2_float graph_pan_{ 0.0f, 0.0f };

	void EnsureGraphLayout(const impl::DebugRenderGraphSnapshot& snapshot);
	void AutoLayoutGraph(const impl::DebugRenderGraphSnapshot& snapshot, bool reset_existing);
	void PruneMissingNodePositions(const impl::DebugRenderGraphSnapshot& snapshot);

	static const impl::DebugRenderResourceSnapshot* FindResource(
		const impl::DebugRenderGraphSnapshot& snapshot, impl::RenderResourceId id
	);

	static const impl::DebugRenderNodeSnapshot* FindNode(
		const impl::DebugRenderGraphSnapshot& snapshot, impl::RenderNodeId id
	);

	static Color NodeColor(impl::RenderNodeType type);

	static void DrawResourceTable(const impl::DebugRenderGraphSnapshot& snapshot);

	void DrawGraphCanvas(const impl::DebugRenderGraphSnapshot& snapshot);

	void DrawNode(
		const impl::DebugRenderGraphSnapshot& snapshot, const impl::DebugRenderNodeSnapshot& node,
		const NodeLayout& layout
	);

	void DrawNodeTooltip(
		const impl::DebugRenderGraphSnapshot& snapshot, const impl::DebugRenderNodeSnapshot& node
	);
};

} // namespace ptgn::editor