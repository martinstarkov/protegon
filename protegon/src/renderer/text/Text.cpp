#include "Text.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <cassert>

#include "renderer/TextureManager.h"
#include "renderer/text/Font.h"
#include "renderer/text/FontManager.h"

#include "math/Hasher.h"

namespace engine {

Text::Text(const char* content,
		   const Color& color,
		   const char* font_name,
		   const V2_double& position,
		   const V2_double& area,
		   FontStyle style,
		   RenderMode mode,
		   const Renderer& renderer) :
	content{ content },
	color{ color },
	font_key{ Hasher::HashCString(font_name) },
	position{ position },
	area{ area },
	style{ style },
	mode{ mode },
	renderer{ renderer } {
	RefreshTexture();
}

void Text::RefreshTexture() {
	Font font{ FontManager::GetFont(font_key) };
	Surface temp_surface;
	switch (mode) {
		case RenderMode::SOLID:
			temp_surface = TTF_RenderText_Solid(font, content.c_str(), color);
			break;
		case RenderMode::SHADED:
			temp_surface = TTF_RenderText_Shaded(font, content.c_str(), color, shading_background_color);
			break;
		case RenderMode::BLENDED:
			temp_surface = TTF_RenderText_Blended(font, content.c_str(), color);
			break;
		default:
			assert(!"Unrecognized render mode when creating surfaace for text texture");
			break;
	}
	assert(temp_surface.IsValid() && "Failed to load text onto surface");
	texture.Destroy();
	texture = { renderer, temp_surface };
	temp_surface.Destroy();
}

void Text::Draw() const {
	assert(texture.IsValid() && "Cannot draw text that has only been default constructed (no texture created)");
	renderer.DrawTexture(texture, position, area);
}

void Text::SetRenderer(const Renderer& new_renderer) {
	renderer = new_renderer;
	RefreshTexture();
}

void Text::SetContent(const char* new_content) {
	content = new_content;
	RefreshTexture();
}

void Text::SetColor(const Color& new_color) {
	color = new_color;
	RefreshTexture();
}

void Text::SetFont(const char* new_font_name) {
	font_key = Hasher::HashCString(new_font_name);
	RefreshTexture();
}

void Text::SetShaded(const Color& shading_background_color) {
	this->shading_background_color = shading_background_color;
	mode = RenderMode::SHADED;
	RefreshTexture();
}

void Text::SetPosition(const V2_double& new_position) {
	position = new_position;
}

void Text::SetArea(const V2_double& new_area) {
	area = new_area;
}

void Text::SetStyle(const FontStyle new_style) {
	style = new_style;
}

} // namespace engine