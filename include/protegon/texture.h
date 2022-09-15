#pragma once

#include <memory> // std::shared_ptr

#include "rectangle.h"
#include "vector2.h"

struct SDL_Texture;
struct SDL_Surface;

namespace ptgn {

class Texture {
public:
	Texture(const char* texture_path);
	~Texture() = default;
	Texture(const Texture&) = default;
	Texture& operator=(const Texture&) = default;
	Texture(Texture&&) = default;
	Texture& operator=(Texture&&) = default;
	bool IsValid() const;
	void Draw(const Rectangle<int>& texture,
			  const Rectangle<int>& source = {}) const;

	/*
	void DrawTexture(const char* texture_key,
					 const ptgn::AABB<int>& texture,
					 const ptgn::AABB<int>& source,
					 const V2_int* center_of_rotation,
					 const float angle,
					 Flip flip) {
		assert(Exists() && "Cannot draw texture with nonexistent renderer");
		const auto& texture_manager{ manager::Get<TextureManager>() };
		const auto key{ math::Hash(texture_key) };
		assert(texture_manager.Has(key) && "Cannot draw nonexistent texture");
		auto renderer = Renderer::Get().renderer_;
		SDL_Rect* src{ NULL };
		SDL_Rect source_rectangle;
		if (!source.p.IsZero() && !source.s.IsZero()) {
			source_rectangle = { source.p.x, source.p.y, source.s.x, source.s.y };
			src = &source_rectangle;
		}
		SDL_Rect destination{ texture.p.x, texture.p.y, texture.s.x, texture.s.y };
		if (center_of_rotation != nullptr) {
			SDL_Point center{ center_of_rotation->x, center_of_rotation->y };
			SDL_RenderCopyEx(renderer, *texture_manager.Get(key), src, &destination,
							 angle, &center, static_cast<SDL_RendererFlip>(static_cast<int>(flip)));
		} else {
			SDL_RenderCopyEx(renderer, *texture_manager.Get(key), src, &destination,
							 angle, NULL, static_cast<SDL_RendererFlip>(static_cast<int>(flip)));
		}
	}
	*/
private:
	Texture() = default;
	friend class Text;
	// Takes ownership of surface pointer.
	Texture(SDL_Surface* surface);
	std::shared_ptr<SDL_Texture> texture_{ nullptr };
};

} // namespace ptgn