#pragma once

#include <utility> // std::forward
#include <type_traits> // std::enable_if_t
#include <vector> // std::vector
#include <memory> // std::shared_ptr

#include "scene/Scene.h"

namespace ptgn {

namespace scene {

bool Exists(const char* scene_key);

void Load(const char* scene_key, Scene* scene);

// TODO: Check that T is convertible to S
template <typename T, typename ...TArgs, 
	std::enable_if_t<std::is_convertible_v<T*, Scene*>, bool> = true>
void Load(const char* scene_key, TArgs&&... scene_constructor_arguments) {
	if (!Exists(scene_key)) {
		Load(scene_key, new T{ std::forward<TArgs>(scene_constructor_arguments)... });
	}
}

void Unload(const char* scene_key);

void SetActive(const char* scene_key);

void AddActive(const char* scene_key);

void RemoveActive(const char* scene_key);

std::vector<std::shared_ptr<Scene>> GetActive();

void Update(double dt);

} // namespace scene

} // namespace ptgn