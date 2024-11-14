#pragma once

#include <functional>

#include "math/geometry/polygon.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "renderer/batch.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/font.h"
#include "renderer/layer_info.h"
#include "renderer/shader.h"
#include "renderer/text.h"
#include "renderer/texture.h"

namespace ptgn {

class Scene;
class Texture;
class Text;
class RenderTexture;
class VertexArray;

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

	void Clear();

	// Flushes all render layers.
	void Present();

	// Flush all render layers.
	void Flush();

	// Flush only a specific render layer. If the specified render layer does not have a primary
	// camera, the model view projection matrix will be an identity matrix.
	void Flush(std::size_t render_layer);

	void VertexArray(const VertexArray& vertex_array) const;

	// @param destination_size Default: {}, which corresponds to the unscaled size of the text.
	// @param font Default: {}, which corresponds to the default font (use game.font.SetDefault(...)
	// to change.
	void Text(
		const std::string_view& text_content, const Color& text_color,
		const ptgn::Rect& destination, const FontOrKey& font = {}, TextInfo text_info = {},
		LayerInfo layer_info = {}
	);

	// Setting destination.size {} corresponds to the unscaled size of the text.
	void Text(const ptgn::Text& text, ptgn::Rect destination = {}, LayerInfo layer_info = {});

	// @param texture Setting texture to {} will use the entire current rendering target.
	void Shader(
		ScreenShader screen_shader, const ptgn::Texture& texture = {},
		BlendMode blend_mode = BlendMode::Blend, LayerInfo layer_info = {}
	);

	// Default size results in fullscreen shader.
	// @param texture Setting texture to {} will use the entire current rendering target.
	void Shader(
		const ptgn::Shader& shader, const ptgn::Texture& texture = {}, ptgn::Rect destination = {},
		BlendMode blend_mode = BlendMode::Blend, Flip flip = Flip::None,
		const V2_float& rotation_center = { 0.5f, 0.5f }, LayerInfo layer_info = {}
	);

	// If destination.size is {} it will be fullscreen.
	void Texture(
		const Texture& texture, ptgn::Rect destination = {}, TextureInfo texture_info = {},
		LayerInfo layer_info = {}
	);

	void Point(
		const V2_float& position, const Color& color, float radius = 1.0f, LayerInfo layer_info = {}
	);

	void Points(
		const V2_float* points, std::size_t point_count, const Color& color, float radius = 1.0f,
		LayerInfo layer_info = {}
	);

	void Line(
		const V2_float& p0, const V2_float& p1, const Color& color, float line_width = 1.0f,
		LayerInfo layer_info = {}
	);

	// Draw an axis line (magnitude of window) along the given direction vector.
	void Axis(
		const V2_float& point, const V2_float& direction, const Color& color,
		float line_width = 1.0f, LayerInfo layer_info = {}
	);

	void Triangle(
		const V2_float& vertex1, const V2_float& vertex2, const V2_float& vertex3,
		const Color& color, float line_width = -1.0f, LayerInfo layer_info = {}
	);

	void Rect(
		const ptgn::Rect& rect, const Color& color, float line_width = -1.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, LayerInfo layer_info = {}
	);

	void Polygon(
		const V2_float* vertices, std::size_t vertex_count, const Color& color,
		float line_width = -1.0f, LayerInfo layer_info = {}
	);

	void Circle(
		const V2_float& position, float radius, const Color& color, float line_width = -1.0f,
		LayerInfo layer_info = {}
	);

	void RoundedRect(
		const ptgn::Rect& rect, float radius, const Color& color, float line_width = -1.0f,
		const V2_float& rotation_center = { 0.5f, 0.5f }, LayerInfo layer_info = {}
	);

	void Ellipse(
		const V2_float& position, const V2_float& radius, const Color& color,
		float line_width = -1.0f, LayerInfo layer_info = {}
	);

	// Angles in radians, counter-clockwise from the right.
	void Arc(
		const V2_float& position, float arc_radius, float start_angle, float end_angle,
		bool clockwise, const Color& color, float line_width = -1.0f, LayerInfo layer_info = {}
	);

	void Capsule(
		const V2_float& p0, const V2_float& p1, float radius, const Color& color,
		float line_width = -1.0f, LayerInfo layer_info = {}
	);

	// @return Pixel at a specific coordinate on the current render target.
	[[nodiscard]] Color GetPixel(const V2_int& coordinate) const;

	// Loop through each pixel on the current render target.
	void ForEachPixel(const std::function<void(V2_int, Color)>& func) const;

	// Calling with default argument {} will reset render target to window.
	// Keep in mind that calling this function will draw the previously set render target to the
	// screen. This can lead to shaders being drawn twice.
	void SetTarget(
		const ptgn::RenderTexture& render_target = {}, bool draw_previously_bound_target = true,
		bool force_draw = false
	);
	[[nodiscard]] ptgn::RenderTexture GetTarget() const;

	void SetBlendMode(BlendMode blend_mode);
	[[nodiscard]] BlendMode GetBlendMode() const;

	void SetClearColor(const Color& color);
	[[nodiscard]] Color GetClearColor() const;

	void FlushImpl(std::size_t render_layer, const M4_float& shader_view_projection);
	void FlushImpl(const M4_float& shader_view_projection);

private:
	friend class CameraManager;
	friend class Game;
	friend class RenderTexture;
	friend class Scene;

	constexpr static const float default_fade_{ 0.005f };

	void UpdateLayer(std::size_t layer_number, RenderLayer& layer, CameraManager& camera_manager)
		const;

	void Init();
	void Shutdown();
	void Reset();

	RendererData data_;

	BlendMode blend_mode_{ BlendMode::Blend };
	ptgn::RenderTexture default_target_;
	ptgn::RenderTexture current_target_;
};

} // namespace impl

} // namespace ptgn