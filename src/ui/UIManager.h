#pragma once

#include <cstddef> // std::size_t
#include <unordered_map> // std::unordered_map
#include <memory> // std::shared_ptr

namespace ptgn {

namespace interfaces {

class UIManager {
public:
    virtual void LoadUI(const char* ui_key, const char* ui_path) = 0;
    virtual void UnloadUI(const char* ui_key) = 0;
};

} // namespace interface

namespace impl {

class DefaultUIManager : public interfaces::UIManager {
public:
    DefaultUIManager();
    ~DefaultUIManager();
    virtual void LoadUI(const char* ui_key, const char* ui_path) override;
    virtual void UnloadUI(const char* ui_key) override;
private:
    // friend class SDLRenderer;
    // std::shared_ptr<SDL_UI> GetUI(const char* ui_key);
	std::unordered_map<std::size_t, void*> ui_map_;
};

DefaultUIManager& GetDefaultUIManager();

} // namespace impl

namespace services {

interfaces::UIManager& GetUIManager();

} // namespace services

} // namespace ptgn