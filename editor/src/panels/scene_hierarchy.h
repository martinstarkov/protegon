#pragma once

#include "core/editor_context.h"
#include "runtime/ecs/entity.h"

namespace ptgn::editor {

class SceneHierarchyPanel {
public:
	void OnRender(EditorContext& ctx);

	Entity GetSelectedEntity() const;
	void SetSelectedEntity(Entity entity);

private:
	Entity selected_entity_;
};

} // namespace ptgn::editor