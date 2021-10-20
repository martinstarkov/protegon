#pragma once

#include <cstddef> // std::size_t
#include <unordered_map> // std::unordered_map
#include <memory> // std::shared_ptr

namespace ptgn {

namespace interfaces {

class SceneManager {
public:
    virtual void LoadScene(const char* scene_key, const char* scene_path) = 0;
    virtual void UnloadScene(const char* scene_key) = 0;
};

} // namespace interface

namespace impl {

//class SDLRenderer;

class DefaultSceneManager : public interfaces::SceneManager {
public:
    DefaultSceneManager() = default;
    ~DefaultSceneManager();
    virtual void LoadScene(const char* scene_key, const char* scene_path) override;
    virtual void UnloadScene(const char* scene_key) override;
private:
    //friend class SDLRenderer;
    //std::shared_ptr<SDL_Scene> GetScene(const char* scene_key);
	std::unordered_map<std::size_t, void*> scene_map_;
};

DefaultSceneManager& GetDefaultSceneManager();

} // namespace impl

namespace services {

interfaces::SceneManager& GetSceneManager();

} // namespace services

} // namespace ptgn