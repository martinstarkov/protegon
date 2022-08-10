#include "Scene.h"

#include "manager/SceneManager.h"
#include "math/Hash.h"

namespace ptgn {

namespace scene {

bool Exists(const char* scene_key) {
	const auto& scene_manager{ manager::Get<SceneManager>() };
	return scene_manager.Has(math::Hash(scene_key));
}

void Load(const char* scene_key, Scene* scene) {
	if (!Exists(scene_key)) {
		auto& scene_manager{ manager::Get<SceneManager>() };
		assert(scene != nullptr && "Cannot load nullptr scene into scene manager");
		scene_manager.LoadPointer(math::Hash(scene_key), scene);
	}
}

void Unload(const char* scene_key) {
	auto& scene_manager{ manager::Get<SceneManager>() };
	scene_manager.Unload(math::Hash(scene_key));
}

void SetActive(const char* scene_key) {
	assert(Exists(scene_key) && "Cannot set active scene if it has not been loaded into the scene manager");
	auto& scene_manager{ manager::Get<SceneManager>() };
	scene_manager.SetActive(math::Hash(scene_key));
}

void AddActive(const char* scene_key) {
	assert(Exists(scene_key) && "Cannot add active scene if it has not been loaded into the scene manager");
	auto& scene_manager{ manager::Get<SceneManager>() };
	scene_manager.AddActive(math::Hash(scene_key));
}

void RemoveActive(const char* scene_key) {
	assert(Exists(scene_key) && "Cannot remove active scene if it has not been loaded into the scene manager");
	auto& scene_manager{ manager::Get<SceneManager>() };
	scene_manager.RemoveActive(math::Hash(scene_key));
}

std::vector<std::shared_ptr<Scene>> GetActive() {
	auto& scene_manager{ manager::Get<SceneManager>() };
	return scene_manager.GetActive();
}

void Update(float dt) {
	auto& scene_manager{ manager::Get<SceneManager>() };
	scene_manager.Update(dt);
}

} // namespace scene

} // namespace ptgn