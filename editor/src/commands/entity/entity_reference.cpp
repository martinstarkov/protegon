#include "commands/entity/entity_reference.h"

#include <ranges>

#include "editor/editor.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

namespace ptgn::editor {

Scene* EntityReference::ResolveScene(Editor& editor) const {
	auto& scenes{ editor.GetSceneManager().GetScenes() };
	const auto it{ std::ranges::find_if(
		scenes,
		[this](const auto& scene) {
			return scene &&
				scene->GetTag() == scene_key &&
				scene->IsRuntime() == runtime;
		}
	) };

	return it == scenes.end() ? nullptr : it->get();
}

Entity EntityReference::Resolve(Editor& editor) const {
	auto* scene{ ResolveScene(editor) };
	return scene ? scene->GetEntity(entity_uuid) : Entity{};
}

EntityReference MakeEntityReference(Entity entity) {
	if (!entity || !entity.Has<UUID>()) {
		return {};
	}

	auto& scene{ entity.GetScene() };
	return EntityReference{
		.scene_key = scene.GetTag(),
		.runtime = scene.IsRuntime(),
		.entity_uuid = entity.Get<UUID>(),
	};
}

} // namespace ptgn::editor
