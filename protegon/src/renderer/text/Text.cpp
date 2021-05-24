#include "Text.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include "math/Hasher.h"
#include "renderer/Renderer.h"
#include "renderer/text/FontManager.h"

namespace engine {

Text::~Text() {
	texture_.Destroy();
}

Text::Text(const char* content,
		   const Color& color,
		   const char* font_name,
		   const V2_double& position,
		   const V2_double& area) :
	content_{ content },
	color_{ color },
	font_name_{ font_name },
	font_key_{ Hasher::HashCString(font_name_) },
	position_{ position },
	area_{ area } {
	RefreshTexture();
}

Text& Text::operator=(Text&& obj) noexcept {
	style_ = obj.style_;
	mode_ = obj.mode_;
	shading_background_color_ = std::move(obj.shading_background_color_);
	texture_.Destroy();
	texture_ = obj.texture_;
	obj.texture_ = nullptr;
	content_ = std::move(obj.content_);
	color_ = std::move(obj.color_);
	font_name_ = std::move(obj.font_name_);
	font_key_ = std::move(obj.font_key_);
	position_ = std::move(obj.position_);
	area_ = std::move(obj.area_);
	return *this;
}

void Text::RefreshTexture() {
	Font font{ FontManager::GetFont(font_key_) };
	TTF_SetFontStyle(font, style_);
	internal::Surface temp_surface;
	switch (mode_) {
		case FontRenderMode::SOLID:
			temp_surface = TTF_RenderText_Solid(font, content_, color_);
			break;
		case FontRenderMode::SHADED:
			temp_surface = TTF_RenderText_Shaded(font, content_, color_, shading_background_color_);
			break;
		case FontRenderMode::BLENDED:
			temp_surface = TTF_RenderText_Blended(font, content_, color_);
			break;
		default:
			assert(!"Unrecognized render mode when creating surface for text texture");
			break;
	}
	assert(temp_surface.IsValid() && "Failed to load text onto surface");
	// Destroy old texture.
	texture_.Destroy();
	texture_ = Renderer::CreateTexture(temp_surface);
	TTF_SetFontStyle(font, static_cast<int>(FontStyle::NORMAL));
	temp_surface.Destroy();
}

void Text::SetContent(const char* new_content) {
	content_ = new_content;
	RefreshTexture();
}

void Text::SetColor(const Color& new_color) {
	color_ = new_color;
	RefreshTexture();
}

void Text::SetFont(const char* new_font_name) {
	font_key_ = Hasher::HashCString(new_font_name);
	assert(FontManager::HasFont(font_key_) &&
		   "Cannot set text font which has not been loaded into FontManager");
	RefreshTexture();
}


void Text::SetSolidRenderMode() {
	mode_ = FontRenderMode::SOLID;
	RefreshTexture();
}

void Text::SetShadedRenderMode(const Color& shading_background_color) {
	shading_background_color_ = shading_background_color;
	mode_ = FontRenderMode::SHADED;
	RefreshTexture();
}

void Text::SetBlendedRenderMode() {
	mode_ = FontRenderMode::BLENDED;
	RefreshTexture();
}

void Text::SetPosition(const V2_double& new_position) {
	position_ = new_position;
}

void Text::SetArea(const V2_double& new_area) {
	area_ = new_area;
}

const char* Text::GetContent() const {
	return content_;
}

Color Text::GetColor() const {
	return color_;
}

const char* Text::GetFont() const {
	return font_name_;
}

V2_double Text::GetPosition() const {
	return position_;
}

V2_double Text::GetArea() const {
	return area_;
}

Texture Text::GetTexture() const {
	return texture_;
}

} // namespace engine