#pragma once

#include "polygon.h"
#include "vector2.h"
#include "color.h"
#include "handle.h"
#include "file.h"
#include "renderer.h"

struct SDL_Texture;
struct SDL_Surface;

namespace ptgn {

class Texture : public Handle<SDL_Texture> {
public:
	enum class Flip {
		// Source: https://wiki.libsdl.org/SDL2/SDL_RendererFlip

		NONE       = 0x00000000,
		HORIZONTAL = 0x00000001,
		VERTICAL   = 0x00000002
	};

	enum class AccessType : int {
		STATIC = 0,	   // SDL_TEXTUREACCESS_STATIC    /* Changes rarely, not lockable */
		STREAMING = 1, // SDL_TEXTUREACCESS_STREAMING /* Changes frequently, lockable */
		TARGET = 2,    // SDL_TEXTUREACCESS_TARGET
	};

	Texture() = default;
	Texture(const path& image_path);
	Texture(AccessType access, const V2_int& size);

	// Rotation in degrees. Positive clockwise.
	void Draw(const Rectangle<float>& texture,
			  const Rectangle<int>& source = {},
			  float angle = 0.0f,
			  Flip flip = Flip::NONE,
			  V2_int* center_of_rotation = nullptr) const;

	[[nodiscard]] V2_int GetSize() const;

	void SetBlendMode(BlendMode mode);

	void SetAlpha(std::uint8_t alpha);

	void SetColor(const Color& color);

	AccessType GetAccessType() const;
private:
	friend class Text;
	Texture(const std::shared_ptr<SDL_Surface>& surface);
};

} // namespace ptgn