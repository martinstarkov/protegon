#pragma once

#include "renderer/batch.h"
#include "renderer/origin.h"

// TODO: If batch size is very small, z_indexes get drawn in 2 calls which completely defeats the
// purpose of it.

namespace ptgn {

struct TextureInfo {
	TextureInfo() = default;

	TextureInfo(
		const Rectangle<float>& source, Flip flip = Flip::None, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f,
		const Color& tint = color::White
	) :
		source{ source },
		flip{ flip },
		rotation{ rotation },
		rotation_center{ rotation_center },
		z_index{ z_index },
		tint{ tint } {}

	TextureInfo(
		const V2_float& source_pos, const V2_float& source_size,
		Origin draw_origin = Origin::Center, Flip flip = Flip::None, float rotation = 0.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, float z_index = 0.0f,
		const Color& tint = color::White
	) :
		TextureInfo{ Rectangle<float>{ source_pos, source_size, draw_origin },
					 flip,
					 rotation,
					 rotation_center,
					 z_index,
					 tint } {}

	/*
	source.pos Top left pixel to start drawing texture from within the texture (defaults to { 0, 0
	}). source.size Number of pixels of the texture to draw (defaults to {} which corresponds to the
	remaining texture size to the bottom right of source_position). source.origin Relative  to
	destination_position the direction from which the texture is.
	*/
	Rectangle<float> source{ {}, {}, Origin::Center };
	// Mirror the texture along an axis (default to Flip::None).
	Flip flip{ Flip::None };
	// Angle in radians to rotate the texture (defaults to 0).
	float rotation{ 0.0f };
	// Fraction of the source_size around which the texture is rotated (Defaults to{ 0.5f, 0.5f }
	// which corresponds to the center of the texture).
	V2_float rotation_center{ 0.5f, 0.5f };
	// Z-coordinate to draw the texture at.
	float z_index{ 0.0f };
	// Color to tint the texture. Allows to change the transparency of a texture. (Default:
	// color::White corresponds to no tint effect ).
	Color tint{ color::White };
};

namespace impl {

class Renderer {
public:
	Renderer()							 = default;
	~Renderer()							 = default;
	Renderer(const Renderer&)			 = delete;
	Renderer(Renderer&&)				 = default;
	Renderer& operator=(const Renderer&) = delete;
	Renderer& operator=(Renderer&&)		 = default;

	void Clear() const;

	void Present();

	void Flush();

	void VertexElements(const VertexArray& va, std::size_t index_count) const;
	void VertexArray(const VertexArray& va, std::size_t vertex_count) const;

	void Texture(
		const Texture& texture, const V2_float& destination_position,
		const V2_float& destination_size, const TextureInfo& info = {}
	);

	void Point(
		const V2_float& position, const Color& color, float radius = 1.0f, float z_index = 0.0f
	);

	void Line(
		const V2_float& p0, const V2_float& p1, const Color& color, float line_width = 1.0f,
		float z_index = 0.0f
	);

	void Triangle(
		const V2_float& vertex1, const V2_float& vertex2, const V2_float& vertex3,
		const Color& color, float line_width = -1.0f, float z_index = 0.0f
	);

	// Rotation angle in radians.
	void Rectangle(
		const V2_float& position, const V2_float& size, const Color& color,
		Origin draw_origin = Origin::Center, float line_width = -1.0f,
		float rotation_radians = 0.0f, const V2_float& rotation_center = { 0.5f, 0.5f },
		float z_index = 0.0f
	);

	void Polygon(
		const V2_float* vertices, std::size_t vertex_count, const Color& color,
		float line_width = -1.0f, float z_index = 0.0f
	);

	void Circle(
		const V2_float& position, float radius, const Color& color, float line_width = -1.0f,
		float z_index = 0.0f, float fade = 0.005f
	);

	// Rotation angle in radians.
	void RoundedRectangle(
		const V2_float& position, const V2_float& size, float radius, const Color& color,
		Origin draw_origin = Origin::Center, float line_width = -1.0f,
		float rotation_radians = 0.0f, const V2_float& rotation_center = { 0.5f, 0.5f },
		float z_index = 0.0f
	);

	void Ellipse(
		const V2_float& position, const V2_float& radius, const Color& color,
		float line_width = -1.0f, float z_index = 0.0f, float fade = 0.005f
	);

	// Angles in radians, counter-clockwise from the right.
	void Arc(
		const V2_float& position, float arc_radius, float start_angle, float end_angle,
		bool clockwise, const Color& color, float line_width = -1.0f, float z_index = 0.0f
	);

	void Capsule(
		const V2_float& p0, const V2_float& p1, float radius, const Color& color,
		float line_width = -1.0f, float z_index = 0.0f, float fade = 0.005f
	);

	void SetBlendMode(BlendMode mode);
	BlendMode GetBlendMode() const;

	void SetClearColor(const Color& color);
	Color GetClearColor() const;

	void SetViewport(const V2_int& size);

	void UpdateViewProjection(const M4_float& view_projection);

private:
	friend class CameraManager;
	friend class Game;

	void Init();
	void Shutdown();
	void Reset();

	Color clear_color_{ color::Transparent };
	BlendMode blend_mode_{ BlendMode::Blend };
	V2_int viewport_size_;
	RendererData data_;
};

} // namespace impl

} // namespace ptgn