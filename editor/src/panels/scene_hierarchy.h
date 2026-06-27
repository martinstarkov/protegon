#pragma once

#include "runtime/ecs/entity.h"

namespace ptgn::editor {

class EditorContext;

class SceneHierarchyPanel {
public:
	void OnRender(EditorContext& ctx);

	Entity GetSelectedEntity() const;
	void SetSelectedEntity(Entity entity);

private:
	Entity selected_entity_;
};

} // namespace ptgn::editor