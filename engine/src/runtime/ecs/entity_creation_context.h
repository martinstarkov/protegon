#pragma once

#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"

namespace ptgn {

class AssetManager;
class Manager;
class Renderer;
class Scene;

/// @brief Services available to entity factories without requiring an owning Scene.
///
/// The context borrows all services and never owns the entities it creates.
/// A Scene implicitly provides a context. Independent managers can create
/// prefab authoring entities without constructing a Scene.
class EntityCreationContext {
public:
	EntityCreationContext(Scene& scene);
	EntityCreationContext(
		Manager& manager, AssetManager* assets = nullptr, Renderer* renderer = nullptr
	);

	[[nodiscard]] Entity CreateEntity(Tag tag = Tag{ "Entity" }) const;

	[[nodiscard]] Scene* GetScene() const {
		return scene_;
	}

	[[nodiscard]] AssetManager* GetAssetManager() const;
	[[nodiscard]] Renderer* GetRenderer() const;

	[[nodiscard]] bool HasScene() const {
		return scene_ != nullptr;
	}

private:
	Scene* scene_{};
	Manager* manager_{};
	AssetManager* assets_{};
	Renderer* renderer_{};
};

} // namespace ptgn
