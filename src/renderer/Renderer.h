#pragma once

#include <cstdint> // std::uint32_t

#include "math/Vector2.h"
#include "renderer/Colors.h"
#include "texture/Flip.h"
#include "texture/TextureManager.h"
#include "text/TextManager.h"
#include "ui/UIManager.h"

class SDL_Renderer;
class SDL_Window;

namespace ptgn {

namespace interfaces {

class Renderer {
public:
    virtual void Present() = 0;
    virtual void Clear() = 0;
    virtual void SetDrawColor(const Color& color) const = 0;
    virtual void DrawTexture(const char* texture_key,
                         const V2_int& position,
                         const V2_int& size,
                         const V2_int& source_position = {},
                         const V2_int& source_size = {}) const = 0;
    virtual void DrawTexture(const char* texture_key,
                         const V2_int& position,
                         const V2_int& size,
                         const V2_int& source_position,
                         const V2_int& source_size,
                         const V2_int* center_of_rotation,
                         const double angle,
                         Flip flip = Flip::NONE) const = 0;
    virtual void DrawText(const char* text_key,
                      const V2_int& position,
                      const V2_int& size) const = 0;
    virtual void DrawUI(const char* ui_key,
                    const V2_int& position,
                    const V2_int& size) const = 0;
    virtual void DrawPoint(const V2_int& point,
                       const Color& color) const = 0;
    virtual void DrawLine(const V2_int& origin,
                      const V2_int& destination,
                      const Color& color) const = 0;
    virtual void DrawCircle(const V2_int& center,
                        const double radius,
                        const Color& color) const = 0;
    virtual void DrawSolidCircle(const V2_int& center,
                             const double radius,
                             const Color& color) const = 0;
    virtual void DrawRectangle(const V2_int& center,
                           const V2_int& size,
                           const Color& color) const = 0;
    virtual void DrawSolidRectangle(const V2_int& center,
                                const V2_int& size,
                                const Color& color) const = 0;
};

} // namespace interfaces

namespace impl {

class SDLTextureManager;

class SDLRenderer : public interfaces::Renderer {
public:
	SDLRenderer(SDL_Window* sdl_window, int index, std::uint32_t flags);
    ~SDLRenderer();
    virtual void Present() override;
    virtual void Clear() override;
    virtual void SetDrawColor(const Color& color) const override;
    virtual void DrawTexture(const char* texture_key,
                         const V2_int& position,
                         const V2_int& size,
                         const V2_int& source_position = {},
                         const V2_int& source_size = {}) const override;
    virtual void DrawTexture(const char* texture_key,
                         const V2_int& position,
                         const V2_int& size,
                         const V2_int& source_position,
                         const V2_int& source_size,
                         const V2_int* center_of_rotation = nullptr,
                         const double angle = 0.0,
                         Flip flip = Flip::NONE) const override;
    virtual void DrawText(const char* text_key,
                      const V2_int& position,
                      const V2_int& size) const override;
    virtual void DrawUI(const char* ui_key,
                    const V2_int& position,
                    const V2_int& size) const override;
    virtual void DrawPoint(const V2_int& point,
                       const Color& color = colors::DEFAULT) const override;
    virtual void DrawLine(const V2_int& origin,
                      const V2_int& destination,
                      const Color& color = colors::DEFAULT) const override;
    virtual void DrawCircle(const V2_int& center,
                        const double radius,
                        const Color& color = colors::DEFAULT) const override;
    virtual void DrawSolidCircle(const V2_int& center,
                             const double radius,
                             const Color& color = colors::DEFAULT) const override;
    virtual void DrawRectangle(const V2_int& center,
                           const V2_int& size,
                           const Color& color = colors::DEFAULT) const override;
    virtual void DrawSolidRectangle(const V2_int& center,
                                const V2_int& size,
                                const Color& color = colors::DEFAULT) const override;
private:
    friend class SDLTextureManager;
	SDL_Renderer* renderer_{ nullptr };
};

SDLRenderer& GetSDLRenderer();

} // namespace impl

namespace services {

interfaces::Renderer& GetRenderer();

} // namespace services

} // namespace ptgn