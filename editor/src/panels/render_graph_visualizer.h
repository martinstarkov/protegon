#pragma once

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
	static void Draw(const impl::DebugRenderGraphSnapshot& snapshot, bool* open = nullptr);

	static void DrawContents(const impl::DebugRenderGraphSnapshot& snapshot);

private:
	struct NodeLayout {
		V2_float min;
		V2_float max;
	};

	static const impl::DebugRenderResourceSnapshot* FindResource(
		const impl::DebugRenderGraphSnapshot& snapshot, impl::RenderResourceId id
	);

	static const impl::DebugRenderNodeSnapshot* FindNode(
		const impl::DebugRenderGraphSnapshot& snapshot, impl::RenderNodeId id
	);

	static Color NodeColor(impl::RenderNodeType type);

	static void DrawResourceTable(const impl::DebugRenderGraphSnapshot& snapshot);

	static void DrawGraphCanvas(const impl::DebugRenderGraphSnapshot& snapshot);

	static void DrawNode(
		const impl::DebugRenderGraphSnapshot& snapshot, const impl::DebugRenderNodeSnapshot& node,
		const NodeLayout& layout
	);

	static void DrawNodeTooltip(
		const impl::DebugRenderGraphSnapshot& snapshot, const impl::DebugRenderNodeSnapshot& node
	);
};

} // namespace ptgn::editor