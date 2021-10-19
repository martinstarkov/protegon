#pragma once

#include <cstdint> // std::size_t
#include <unordered_map> // std::unordered_map
#include <memory> // std::shared_ptr

namespace ptgn {

namespace interfaces {

class TextManager {
public:
    virtual void LoadText(const char* text_key, const char* text_path) = 0;
    virtual void UnloadText(const char* text_key) = 0;
};

} // namespace interface

namespace impl {

class DefaultTextManager : public interfaces::TextManager {
public:
    DefaultTextManager() = default;
    ~DefaultTextManager();
    virtual void LoadText(const char* text_key, const char* text_path) override;
    virtual void UnloadText(const char* text_key) override;
private:
    //std::shared_ptr<SDL_Text> GetText(const char* text_key);
	std::unordered_map<std::size_t, void*> text_map_;
};

DefaultTextManager& GetDefaultTextManager();

} // namespace impl

namespace services {

interfaces::TextManager& GetTextManager();

} // namespace services

} // namespace ptgn