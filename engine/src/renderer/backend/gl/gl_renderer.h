#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "app/context.h"
#include "core/event/dispatcher.h"
#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/graphics/flip.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "renderer/backend/gl/gl_handle.h"
#include "renderer/backend/gl/gl_state.h"
#include "renderer/resources/texture_format.h"
#include "renderer/resources/vertex.h"

namespace ptgn::impl::gl {

template <class State, class Func>
void UpdateStateIfChanged(Renderer&, State&, const State&, Func&&);

// TODO: Move somewhere else.
/*
struct LightParams {
	V2_float position;
	float radius;
	Color color;
	float intensity;
	float falloff;
	V3_float ambient_color;
	float ambient_intensity;
	V3_float attenuation;
};
*/

struct RenderTarget {
	Framebuffer framebuffer;
	Texture color;
	Renderbuffer depth; // optional
	V2_int size;
	TextureFormat format{ TextureFormat::RGBA8 };

	bool operator==(const RenderTarget&) const = default;
};

class GLContext;

using Index = std::uint32_t;

constexpr std::size_t batch_capacity{ 10000 };
constexpr std::size_t vertex_capacity{ batch_capacity * 4 };
constexpr std::size_t index_capacity{ batch_capacity * 6 };

[[nodiscard]] static constexpr std::array<V2_float, 4> GetDefaultTextureCoordinates() {
	return {
		V2_float{ 0.0f, 0.0f },
		V2_float{ 1.0f, 0.0f },
		V2_float{ 1.0f, 1.0f },
		V2_float{ 0.0f, 1.0f },
	};
}

[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(
	V2_float source_position, V2_float source_size, V2_float texture_size,
	bool offset_texels = false
);

void FlipTextureCoordinates(std::array<V2_float, 4>& texture_coords, Flip flip);

struct QuadDesc {
	std::array<V2_float, 4> positions;
	std::array<V2_float, 4> tex_coords;
	Color color	   = color::White;
	float rotation = 0.0f;
	std::array<float, 4> user_data{};
};

struct PooledTarget {
	RenderTarget target;
	std::uint64_t last_used_tick = 0;
	bool in_use					 = false;
};

struct QuadParams {
	V2_float center{};
	V2_float size{ 0.0f, 0.0f };
	float rotation = 0.0f;

	bool flip_y = false;

	Color tint = color::White;

	std::optional<TextureId> texture;

	std::optional<std::array<V2_float, 4>> tex_coords;
};

struct RenderPass {
	RenderTarget source;

	RenderTarget ping;
	RenderTarget pong;

	bool has_ping = false;
	bool has_pong = false;

	// "latest output" tracking
	bool has_written_once = false; // false -> latest is source
	bool latest_is_ping	  = true;  // valid only if has_written_once == true
};

class Renderer {
public:
	// TODO: Add display viewport.

	Renderer() = delete;
	Renderer(Window& window);
	~Renderer() noexcept;
	Renderer(const Renderer&)				 = delete;
	Renderer(Renderer&&) noexcept			 = delete;
	Renderer& operator=(const Renderer&)	 = delete;
	Renderer& operator=(Renderer&&) noexcept = delete;

	RenderTarget CreateRenderTarget(V2_int size, TextureFormat format) const;

	void ResizeRenderTarget(RenderTarget& rt, V2_int new_size) const;

	// TODO: Move to private.
	std::unique_ptr<GLContext> gl_;

	// TODO: Move to private.
	void BeginFrame();
	// TODO: Move to private.
	void EndFrame();

	using UniformSetup = std::function<void(ShaderId)>;
	using QuadSetup	   = std::function<void(ShaderId, QuadDesc&)>;

	// void DrawLightQuad(const LightParams& light);
	void DrawTexturedQuad(
		ShaderId shader, TextureId texture, V2_float center, V2_float size,
		Color tint = color::White, bool flip_y = false
	);
	void DrawTexture(TextureId texture, V2_float center, V2_float size);
	void DrawTexture(ShaderId shader, RenderPass& pass, const RenderTarget& scene_target);
	void DrawQuadEx(ShaderId shader, const QuadParams& p, const UniformSetup& u = {});
	void DrawQuadEx(ShaderId shader, const QuadParams& p, const QuadSetup& q);

	void BindRenderTarget(FramebufferId framebuffer, const Viewport& viewport);
	void BindRenderTarget(const RenderTarget& rt);
	void BindRenderTarget(RenderPass& pass);

	void SetShader(ShaderId shader);
	void SetBlend(BlendMode mode, bool enabled = true);
	void SetFramebuffer(FramebufferId framebuffer, const Viewport& viewport);
	void SetDepth(const DepthState& depth);
	void SetStencil(const StencilState& stencil);
	void SetRaster(const RasterState& raster);
	void SetColorMask(const ColorMaskState& color_mask);

	// TODO: Move to private.
	RenderTarget screen_target;

	// TODO: Move to some debug system instead.
	void SavePNG(
		const path& path, FramebufferId framebuffer, GLenum attachment /* = GL_COLOR_ATTACHMENT0 */
	);

	RenderPass BeginPass(const RenderTarget& scene_target);

private:
	friend class Application;
	friend struct RenderPass;
	friend class EventHandler;
	template <class State, class Func>
	friend void UpdateStateIfChanged(Renderer&, State&, const State&, Func&&);

	void OnEvent(EventDispatcher d);

	void FlushBatch();

	std::uint32_t GetTextureSlot(TextureId tex);

	RenderTarget AcquirePooledTarget(V2_int size, TextureFormat format);
	void ReleasePooledTarget(const RenderTarget& target);

	static constexpr std::uint32_t MaxQuads	   = 1024;
	static constexpr std::uint32_t MaxVertices = MaxQuads * 4;
	static constexpr std::uint32_t MaxIndices  = MaxQuads * 6;

	std::vector<Vertex> batch_vertices;
	std::vector<Index> batch_indices;

	std::vector<TextureId> batch_textures;

	Window& window_;

	VertexBuffer vbo;
	ElementBuffer ebo;
	VertexArray vao;
	Texture white_texture;

	std::vector<PooledTarget> rt_pool;
	std::uint64_t pool_tick	  = 0;
	std::size_t max_pool_size = 16;
};

} // namespace ptgn::impl::gl