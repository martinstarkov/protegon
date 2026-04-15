#pragma once

#include <vector>

#include "app/layer.h"
#include "renderer/resources/id.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Application;

namespace editor {

class EditorLayer : public Layer {
public:
	EditorLayer(Application& app);

	void OnUpdate() override;
	void OnRender() override;

	std::size_t GetEntityCount() const;

	impl::TextureId GetScreenTargetId() const;

	Entity FindEntityById(int id) const;
	std::vector<int> GetChildrenIds(int parent_id) const;
	bool IsDescendantOf(int entity_id, int possible_parent_id) const;

	void AddEntityAtRoot();
	void DeleteEntityRecursive(int entity_id);
	void DeleteSelectedEntityAndSelectPrevious();
	void MoveEntityToRoot(int entity_id);
	void ReparentEntity(int entity_id, int new_parent_id);

	void ReparentEntityBefore(int entity_id, int new_parent_id, int before_entity_id);
	std::vector<int> CollectSubtreeIds(int root_entity_id) const;
	void ReparentEntityAtEnd(int entity_id, int new_parent_id);

	void BeginRenameEntityInline(Entity entity);
	void BeginRenameSceneInline(int scene_index);
	void CommitInlineRename();
	void CancelInlineRename();
	void DeleteScene(int scene_index);

	Entity MakeEntity(
		int id, int parent_id, const char* name, bool transform, bool sprite, bool camera,
		bool rigidbody, bool script
	);

	void InitializeProject();
	void BuildDefaultDockLayout(unsigned int dockspace_id);

	int GetLastEntityInHierarchyOrder() const;

	const char* glsl_version_ = nullptr;

	bool dock_layout_built_ = false;

	int current_scene_index_ = 0;
	int selected_entity_id_	 = -1;
	int next_entity_id_		 = 1000;

	int renaming_entity_id_	  = -1;
	int renaming_scene_index_ = -1;
	char rename_buffer_[128]  = {};

	// add this field near the other editor state fields
	char hierarchy_filter_[128] = {};

	int expand_entity_once_ = -1;

	bool hierarchy_drag_active_ = false;

	float clear_color_[4] = { 0.10f, 0.10f, 0.12f, 1.0f };

	enum class ScalingMode {
		Letterbox,
		Stretch,
	};

	struct PresentationSettings {
		bool use_logical_resolution = false;
		int logical_width			= 320;
		int logical_height			= 180;
		ScalingMode scaling_mode	= ScalingMode::Letterbox;
	};

	PresentationSettings presentation_;

	Application& app;
};

} // namespace editor

} // namespace ptgn