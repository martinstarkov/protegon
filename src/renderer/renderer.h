#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "components/transform.h"
#include "math/geometry.h"
#include "math/vector2.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/render_data.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "scene/camera.h"

namespace ptgn {

namespace impl {

[[nodiscard]] RenderState GetDebugRenderState(const Camera& camera);

}

void DrawDebugTexture(
	const TextureHandle& texture_key, const V2_float& position, const V2_float& size = {},
	Origin origin = Origin::Center, float rotation = 0.0f, const Camera& camera = {}
);

// @param size {} results in unscaled size of text based on font.
void DrawDebugText(
	const std::string& content, const V2_float& position, const TextColor& color = color::White,
	Origin origin = Origin::Center, const FontSize& font_size = {}, bool hd_text = true,
	const ResourceHandle& font_key = {}, const TextProperties& properties = {},
	const V2_float& size = {}, float rotation = 0.0f, const Camera& camera = {}
);

void DrawDebugLine(
	const V2_float& line_start, const V2_float& line_end, const Color& color,
	float line_width = 1.0f, const Camera& camera = {}
);

void DrawDebugLines(
	const std::vector<V2_float>& points, const Color& color, float line_width = 1.0f,
	bool connect_last_to_first = false, const Camera& camera = {}
);

void DrawDebugTriangle(
	const std::array<V2_float, 3>& vertices, const Color& color, float line_width = 1.0f,
	const Camera& camera = {}
);

void DrawDebugRect(
	const V2_float& position, const V2_float& size, const Color& color,
	Origin origin = Origin::Center, float line_width = 1.0f, float rotation = 0.0f,
	const Camera& camera = {}
);

void DrawDebugEllipse(
	const V2_float& center, const V2_float& radii, const Color& color, float line_width = 1.0f,
	float rotation = 0.0f, const Camera& camera = {}
);

void DrawDebugCircle(
	const V2_float& center, float radius, const Color& color, float line_width = 1.0f,
	const Camera& camera = {}
);

void DrawDebugCapsule(
	const V2_float& start, const V2_float& end, float radius, const Color& color,
	float line_width = 1.0f, const Camera& camera = {}
);

void DrawDebugPolygon(
	const std::vector<V2_float>& vertices, const Color& color, float line_width = 1.0f,
	const Camera& camera = {}
);

void DrawDebugPoint(const V2_float position, const Color& color, const Camera& camera = {});

void DrawDebugShape(
	const Transform& transform, const Shape& shape, const Color& color, float line_width = 1.0f,
	const Camera& camera = {}
);

class Shader;
class RenderTarget;

namespace impl {

class FrameBuffer;
class VertexArray;
class Game;
class SceneManager;
class SceneCamera;
class Batch;
class GLRenderer;
class InputHandler;
class RenderData;

class Renderer {
public:
	Renderer()							 = default;
	~Renderer()							 = default;
	Renderer(const Renderer&)			 = delete;
	Renderer(Renderer&&)				 = default;
	Renderer& operator=(const Renderer&) = delete;
	Renderer& operator=(Renderer&&)		 = default;

	void SetBackgroundColor(const Color& background_color);
	[[nodiscard]] Color GetBackgroundColor() const;

	// @param logical_resolution Setting to {} will use window size.
	void SetLogicalResolution(
		const V2_int& logical_resolution			  = {},
		LogicalResolutionMode logical_resolution_mode = LogicalResolutionMode::Letterbox
	);

	void SetLogicalResolutionMode(
		LogicalResolutionMode logical_resolution_mode = LogicalResolutionMode::Letterbox
	);

	// @return The physical resolution of the renderer.
	[[nodiscard]] V2_int GetPhysicalResolution() const;

	// @return The logical resolution of the renderer.
	[[nodiscard]] V2_int GetLogicalResolution() const;

	// @return The logical resolution scaling mode.
	[[nodiscard]] LogicalResolutionMode GetLogicalResolutionMode() const;

	// @return The render data associated with the current render queue.
	[[nodiscard]] RenderData& GetRenderData();

private:
	friend class ptgn::Shader;
	friend class ptgn::RenderTarget;
	friend class VertexArray;
	friend class FrameBuffer;
	friend class GLRenderer;
	friend class Game;
	friend class Batch;
	friend class RenderData;
	friend class SceneManager;

	// Present the screen target to the window.
	void PresentScreen();

	// Clears the window buffer.
	void ClearScreen() const;

	void Init();
	void Shutdown();
	void Reset();

	// Renderer keeps track of what is bound.
	struct BoundStates {
		std::uint32_t frame_buffer_id{ 0 };
		std::uint32_t shader_id{ 0 };
		std::uint32_t vertex_array_id{ 0 };
		BlendMode blend_mode{ BlendMode::None };
		V2_int viewport_position;
		V2_int viewport_size;
	};

	BoundStates bound_;

	RenderData render_data_;
};

} // namespace impl

} // namespace ptgn