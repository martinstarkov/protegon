#pragma once

#include <vector>

#include "math/matrix4.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/batch.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/origin.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"

namespace ptgn {

struct TextureInfo {
	TextureInfo() = default;

	TextureInfo(
		const Rect& source, Flip flip = Flip::None, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f,
		const Color& tint = color::White, std::size_t render_layer = 0
	) :
		source{ source },
		flip{ flip },
		rotation{ rotation },
		rotation_center{ rotation_center },
		z_index{ z_index },
		tint{ tint },
		render_layer{ render_layer } {}

	TextureInfo(
		const V2_float& source_pos, const V2_float& source_size,
		Origin draw_origin = Origin::Center, Flip flip = Flip::None, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f,
		const Color& tint = color::White, std::size_t render_layer = 0
	) :
		TextureInfo{ Rect{ source_pos, source_size, draw_origin },
					 flip,
					 rotation,
					 rotation_center,
					 z_index,
					 tint,
					 render_layer } {}

	/*
	source.pos Top left pixel to start drawing texture from within the texture (defaults to { 0, 0
	}). source.size Number of pixels of the texture to draw (defaults to {} which corresponds to the
	remaining texture size to the bottom right of source_position). source.origin Relative  to
	destination_position the direction from which the texture is.
	*/
	Rect source{ {}, {}, Origin::Center };
	// Mirror the texture along an axis (default to Flip::None).
	Flip flip{ Flip::None };
	// Angle in radians to rotate the texture (defaults to 0).
	float rotation{ 0.0f };
	// Fraction of the source_size around which the texture is rotated (defaults to{ 0.5f, 0.5f }
	// which corresponds to the center of the texture).
	V2_float rotation_center{ 0.5f, 0.5f };
	// Z-coordinate to draw the texture at.
	float z_index{ 0.0f };
	// Color to tint the texture. Allows to change the transparency of a texture. (default:
	// color::White corresponds to no tint effect ).
	Color tint{ color::White };
	std::size_t render_layer{ 0 };
};

struct TextInfo {
	TextInfo() = default;

	TextInfo(
		float rotation, FontStyle font_style = FontStyle::Normal,
		FontRenderMode render_mode = FontRenderMode::Solid,
		const Color& shading_color = color::White, std::uint32_t wrap_after_pixels = 0,
		bool visible = true, Flip flip = Flip::None,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f,
		std::size_t render_layer = 0
	) :
		font_style{ font_style },
		render_mode{ render_mode },
		shading_color{ shading_color },
		wrap_after_pixels{ wrap_after_pixels },
		visible{ visible },
		rotation{ rotation },
		flip{ flip },
		rotation_center{ rotation_center },
		z_index{ z_index },
		render_layer{ render_layer } {}

	FontStyle font_style{ FontStyle::Normal };
	FontRenderMode render_mode{ FontRenderMode::Solid };
	Color shading_color{ color::White };
	// 0 indicates only wrapping on newline characters.
	std::uint32_t wrap_after_pixels{ 0 };
	bool visible{ true };

	// Angle in radians to rotate the text (defaults to 0).
	float rotation{ 0.0f };
	// Mirror the text along an axis (default to Flip::None).
	Flip flip{ Flip::None };
	// Fraction of the source_size around which the text is rotated (defaults to{ 0.5f, 0.5f }
	// which corresponds to the center of the text).
	V2_float rotation_center{ 0.5f, 0.5f };
	// Z-coordinate to draw the text at.
	float z_index{ 0.0f };
	std::size_t render_layer{ 0 };
};

// Same as TextInfo but only includes draw related information.
struct TextDrawInfo {
	TextDrawInfo() = default;

	TextDrawInfo(
		float rotation, Flip flip = Flip::None, const V2_float& rotation_center = { 0.5f, 0.5f },
		float z_index = 0.0f, std::size_t render_layer = 0
	) :
		rotation{ rotation },
		flip{ flip },
		rotation_center{ rotation_center },
		z_index{ z_index },
		render_layer{ render_layer } {}

	// Angle in radians to rotate the text (defaults to 0).
	float rotation{ 0.0f };
	// Mirror the text along an axis (default to Flip::None).
	Flip flip{ Flip::None };
	// Fraction of the source_size around which the text is rotated (defaults to{ 0.5f, 0.5f }
	// which corresponds to the center of the text).
	V2_float rotation_center{ 0.5f, 0.5f };
	// Z-coordinate to draw the text at.
	float z_index{ 0.0f };
	std::size_t render_layer{ 0 };
};

namespace impl {

class CameraManager;
class Game;
struct RenderLayer;

class Renderer {
public:
	Renderer()							 = default;
	~Renderer()							 = default;
	Renderer(const Renderer&)			 = delete;
	Renderer(Renderer&&)				 = default;
	Renderer& operator=(const Renderer&) = delete;
	Renderer& operator=(Renderer&&)		 = default;

	void Clear() const;

	// Flushes all render layers.
	void Present();

	// Flush all render layers.
	void Flush();

	// Flush only a specific render layer. If the specified render layer does not have a primary
	// camera, the model view projection matrix will be an identity matrix.
	void Flush(std::size_t render_layer);

	void VertexElements(const VertexArray& va, std::size_t index_count) const;
	void VertexArray(const VertexArray& va, std::size_t vertex_count) const;

	// @param destination_size Default: {}, which corresponds to the unscaled size of the text.
	// @param font Default: {}, which corresponds to the default font (use game.font.SetDefault(...)
	// to change.
	void Text(
		const std::string_view& text_content, const V2_float& destination_position,
		const Color& text_color, Origin draw_origin = Origin::Center,
		V2_float destination_size = {}, const FontOrKey& font = {}, const TextInfo& info = {}
	);

	// @param destination_size Default: {}, which corresponds to the unscaled size of the text.
	void Text(
		const ptgn::Text& text, const V2_float& destination_position,
		Origin draw_origin = Origin::Center, V2_float destination_size = {},
		const TextDrawInfo& info = {}
	);

	void Texture(
		const Texture& texture, const V2_float& destination_position,
		const V2_float& destination_size, const TextureInfo& info = {}
	);

	void Point(
		const V2_float& position, const Color& color, float radius = 1.0f, float z_index = 0.0f,
		std::size_t render_layer = 0
	);

	void Points(
		const std::vector<V2_float>& points, const Color& color, float radius = 1.0f,
		float z_index = 0.0f, std::size_t render_layer = 0
	);

	void Line(
		const V2_float& p0, const V2_float& p1, const Color& color, float line_width = 1.0f,
		float z_index = 0.0f, std::size_t render_layer = 0
	);

	// Draw an axis line (magnitude of window) along the given direction vector.
	void Axis(
		const V2_float& point, const V2_float& direction, const Color& color,
		float line_width = 1.0f, float z_index = 0.0f, std::size_t render_layer = 0
	);

	void Triangle(
		const V2_float& vertex1, const V2_float& vertex2, const V2_float& vertex3,
		const Color& color, float line_width = -1.0f, float z_index = 0.0f,
		std::size_t render_layer = 0
	);

	// Rotation angle in radians.
	void Rectangle(
		const V2_float& position, const V2_float& size, const Color& color,
		Origin draw_origin = Origin::Center, float line_width = -1.0f,
		float rotation_radians = 0.0f, const V2_float& rotation_center = { 0.5f, 0.5f },
		float z_index = 0.0f, std::size_t render_layer = 0
	);

	void Polygon(
		const V2_float* vertices, std::size_t vertex_count, const Color& color,
		float line_width = -1.0f, float z_index = 0.0f, std::size_t render_layer = 0
	);

	void Circle(
		const V2_float& position, float radius, const Color& color, float line_width = -1.0f,
		float z_index = 0.0f, std::size_t render_layer = 0, float fade = 0.005f
	);

	// Rotation angle in radians.
	void RoundedRectangle(
		const V2_float& position, const V2_float& size, float radius, const Color& color,
		Origin draw_origin = Origin::Center, float line_width = -1.0f,
		float rotation_radians = 0.0f, const V2_float& rotation_center = { 0.5f, 0.5f },
		float z_index = 0.0f, std::size_t render_layer = 0
	);

	void Ellipse(
		const V2_float& position, const V2_float& radius, const Color& color,
		float line_width = -1.0f, float z_index = 0.0f, std::size_t render_layer = 0,
		float fade = 0.005f
	);

	// Angles in radians, counter-clockwise from the right.
	void Arc(
		const V2_float& position, float arc_radius, float start_angle, float end_angle,
		bool clockwise, const Color& color, float line_width = -1.0f, float z_index = 0.0f,
		std::size_t render_layer = 0
	);

	void Capsule(
		const V2_float& p0, const V2_float& p1, float radius, const Color& color,
		float line_width = -1.0f, float z_index = 0.0f, std::size_t render_layer = 0,
		float fade = 0.005f
	);

	void SetBlendMode(BlendMode mode);
	[[nodiscard]] BlendMode GetBlendMode() const;

	void SetClearColor(const Color& color);
	[[nodiscard]] Color GetClearColor() const;

	void SetViewportSize(const V2_int& viewport_size);
	[[nodiscard]] V2_int GetViewportSize() const;

	void SetViewportScale(const V2_float& viewport_scale);
	[[nodiscard]] V2_float GetViewportScale() const;

private:
	friend class CameraManager;
	friend class Game;

	void UpdateLayer(std::size_t layer_number, RenderLayer& layer, CameraManager& camera_manager);

	void Init();
	void Shutdown();
	void Reset();

	Color clear_color_{ color::Transparent };
	BlendMode blend_mode_{ BlendMode::Blend };
	V2_int viewport_size_;
	V2_float viewport_scale_{ 1.0f, 1.0f };
	RendererData data_;
};

} // namespace impl

} // namespace ptgn