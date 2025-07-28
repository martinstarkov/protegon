#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "math/vector2.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/render_data.h"
#include "rendering/resources/text.h"
#include "resources/texture.h"
#include "scene/camera.h"
#include "serialization/enum.h"

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
	Origin origin = Origin::Center, const FontSize& font_size = {}, const ResourceHandle& font_key = {},
	const TextProperties& properties = {}, const V2_float& size = {}, float rotation = 0.0f,
	const Camera& camera = {}
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

void DrawDebugPolygon(
	const std::vector<V2_float>& vertices, const Color& color, float line_width = 1.0f,
	const Camera& camera = {}
);

void DrawDebugPoint(const V2_float position, const Color& color, const Camera& camera = {});

// How the renderer resolution is scaled to the window size.
enum class ResolutionMode {
	Disabled,  /**< There is no scaling in effect */
	Stretch,   /**< The rendered content is stretched to the output resolution */
	Letterbox, /**< The rendered content is fit to the largest dimension and the other dimension is
				  letterboxed with black bars */
	Overscan,  /**< The rendered content is fit to the smallest dimension and the other dimension
				  extends beyond the output bounds */
	IntegerScale, /**< The rendered content is scaled up by integer multiples to fit the output
					 resolution */
};

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

void GetRenderArea(
	const V2_float& screen_size, const V2_float& target_size, ResolutionMode mode,
	V2_float& out_position, V2_float& out_size
);

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

	// @param resolution The resolution size to which the renderer will be displayed.
	// Note: If no resolution mode is set, setting the resolution will default it to
	// ResolutionMode::Stretch.
	// Note: Setting this will override a set viewport.
	void SetResolution(const V2_int& resolution);

	// @param mode The mode in which to fit the resolution to the window. If
	// ResolutionMode::Disabled, the resolution is ignored.
	// Note: Setting this will override a set viewport.
	void SetResolutionMode(ResolutionMode mode);

	// @return The resolution size of the renderer. If resolution has not been set, returns window
	// size.
	[[nodiscard]] V2_int GetResolution() const;

	// @return The resolution scaling mode.
	[[nodiscard]] ResolutionMode GetResolutionMode() const;

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

	Color background_color_{ color::Transparent };

	// Default value results in fullscreen.
	V2_int resolution_;
	ResolutionMode scaling_mode_{ ResolutionMode::Disabled };
};

} // namespace impl

PTGN_SERIALIZER_REGISTER_ENUM(
	ResolutionMode, { { ResolutionMode::Disabled, "disabled" },
					  { ResolutionMode::Stretch, "stretch" },
					  { ResolutionMode::Letterbox, "letterbox" },
					  { ResolutionMode::Overscan, "overscan" },
					  { ResolutionMode::IntegerScale, "integer_scale" } }
);

} // namespace ptgn