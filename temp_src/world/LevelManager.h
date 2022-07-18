//#pragma once
//
//#include <unordered_map> // std::unordered_map
//
//#include "world/Level.h"
//
//namespace ptgn {
//
//class LevelManager {
//public:
//	/*
//	* Load level of given path into the LevelManager.
//	* @param level_key Unique identifier associated with the loaded level.
//	* @param level_path File path to load level from (must end in .png or .jpg).
//	*/
//	static void Load(const char* level_key, 
//					 const char* level_path);
//
//	// Remove level from LevelManager.
//	static void Unload(const char* level_key);
//
//	/*
//	* @param level_key Unique identifying key associated with the level.
//	* @return Reference to a const level object with the given key.
//	* If LevelManager does not contain the requested level, assertion is called.
//	*/
//	static const Level& GetLevel(const char* level_key);
//private:
//	friend class Engine;
//	LevelManager() = default;
//
//	/*
//	* Destroys all levels in the manager and clears the level map.
//	*/
//	static void Destroy();
//
//	// Level storage container.
//	std::unordered_map<std::size_t, Level> levels_;
//};
//
//} // namespace ptgn