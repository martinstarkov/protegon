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
private:
	Texture() = default;
	friend class Text;
	// Takes ownership of surface pointer.
	Texture(SDL_Surface* surface);
	struct SDL_Texture_Deleter {
		void operator()(SDL_Texture* texture);
	};
	std::shared_ptr<SDL_Texture> texture_{ nullptr };
};

} // namespace ptgn