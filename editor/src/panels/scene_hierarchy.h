#pragma once

#include "core/editor_context.h"
#include "runtime/ecs/entity.h"

namespace ptgn::editor {

class SceneHierarchyPanel {
public:
	void OnRender(EditorContext& ctx);

private:
	Entity selected_entity;
};

} // namespace ptgn::editor