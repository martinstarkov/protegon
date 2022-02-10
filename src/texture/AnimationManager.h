#pragma once

#include <cstddef> // std::size_t
#include <unordered_map> // std::unordered_map
#include <memory> // std::unqiue_ptr

namespace ptgn {

namespace interfaces {

class AnimationManager {
public:
    virtual void LoadAnimation(const char* animation_key, const char* animation_path) = 0;
    virtual void UnloadAnimation(const char* animation_key) = 0;
};

} // namespace interface

namespace impl {

class DefaultAnimationManager : public interfaces::AnimationManager {
public:
    DefaultAnimationManager() = default;
    ~DefaultAnimationManager();
    virtual void LoadAnimation(const char* animation_key, const char* animation_path) override;
    virtual void UnloadAnimation(const char* animation_key) override;
private:
	std::unordered_map<std::size_t, void*> animation_map_;
};

DefaultAnimationManager& GetDefaultAnimationManager();

} // namespace impl

namespace services {

interfaces::AnimationManager& GetAnimationManager();

} // namespace services

} // namespace ptgn