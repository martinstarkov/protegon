#pragma once

#include <string>

#include "math/Vector2.h"

#include "renderer/Color.h"
#include "renderer/Texture.h"
#include "renderer/Renderer.h"

#include "renderer/text/FontStyle.h"

#include "core/Engine.h"

namespace engine {

class Text {
public:
	Text() = default;
	Text(const char* content,
		 const Color& color,
		 const char* font_name,
		 const V2_double& position,
		 const V2_double& area,
		 FontStyle style = FontStyle::NORMAL,
		 RenderMode mode = RenderMode::SOLID,
		 const Renderer& renderer = Engine::GetRenderer());
	void Draw() const;
	void SetRenderer(const Renderer& new_renderer);
	void SetContent(const char* new_content);
	void SetColor(const Color& new_color);
	void SetFont(const char* new_font_name);
	void SetShaded(const Color& shading_background_color);
	void SetPosition(const V2_double& new_position);
	void SetArea(const V2_double& new_area);
	void SetStyle(const FontStyle new_style);
private:
	void RefreshTexture();
	Renderer renderer;
	std::string content;
	Color color;
	std::size_t font_key{ 0 };
	V2_double position;
	V2_double area;
	FontStyle style{ FontStyle::NORMAL };
	Texture texture;
	RenderMode mode{ RenderMode::SOLID };
	Color shading_background_color;
};

} // namespace engine