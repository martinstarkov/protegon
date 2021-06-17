#include "LevelManager.h"

#include "debugging/FileManagement.h"
#include "debugging/Logger.h"
#include "math/Math.h"

namespace ptgn {

void LevelManager::Load(const char* level_key, 
						const char* level_path) {
	assert(level_path != "" && "Cannot load empty level path");
	assert(FileExists(level_path) && "Cannot load level with non-existent file path");
	assert(level_key != "" && "Cannot load invalid level key");
	auto& instance{ GetInstance() };
	const auto key{ math::Hash(level_key) };
	auto it{ instance.levels_.find(key) };
	if (it == std::end(instance.levels_)) {
		Level level{ level_path };
		instance.levels_.emplace(key, level);
	} else {
		PrintLine("Warning: Cannot load level key which already exists in the LevelManager");
	}
}

void LevelManager::Unload(const char* level_key) {
	auto& instance{ GetInstance() };
	const auto key{ math::Hash(level_key) };
	auto it{ instance.levels_.find(key) };
	if (it != std::end(instance.levels_)) {
		it->second.Destroy();
		instance.levels_.erase(it);
	}
}

const Level& LevelManager::GetLevel(const char* level_key) {
	const auto& instance{ GetInstance() };
	const auto key{ math::Hash(level_key) };
	const auto it{ instance.levels_.find(key) };
	assert(it != std::end(instance.levels_) && 
		   "Cannot retrieve level key which does not exist in LevelManager");
	return it->second;
}

void LevelManager::Destroy() {
	auto& instance{ GetInstance() };
	for (auto& pair : instance.levels_) {
		pair.second.Destroy();
	}
	instance.levels_.clear();
}

} // namespace ptgn