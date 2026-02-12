#pragma once

#include <memory>

#include "core/event/dispatcher.h"

namespace ptgn {

class Application;
class EventHandler;
class Window;

namespace impl::gl {

class Renderer;

} // namespace impl::gl

class Renderer {
public:
	Renderer() = delete;
	explicit Renderer(Window& window);
	~Renderer() noexcept;
	Renderer(const Renderer&)				 = delete;
	Renderer(Renderer&&) noexcept			 = delete;
	Renderer& operator=(const Renderer&)	 = delete;
	Renderer& operator=(Renderer&&) noexcept = delete;

private:
	friend class Application;
	friend class EventHandler;

	// struct PresentationResized : public Event<PresentationResized> {
	//	V2_int size;
	// };
	//  void Renderer::SetPresentation(std::optional<V2_int> presentation_size, ScalingMode
	//  scaling_mode) {
	//	app_.UpdateScalingConfig(presentation_size, scaling_mode);
	//	PresentationResized e;
	//	e.size = presentation_size.value();
	//	events.Emit(e);
	//  }
	//
	//  void Renderer::SetPresentationScalingMode(ScalingMode scaling_mode) {
	//	app_.UpdateScalingConfig(app_.presentation_size_, scaling_mode);
	//  }
	// V2_int Renderer::GetPresentationSize() const {
	//	if (app_.presentation_size_) {
	//		return *app_.presentation_size_;
	//	}
	//	return window.GetSize();
	// }

	void OnEvent(EventDispatcher d);

	std::shared_ptr<impl::gl::Renderer> gl_renderer_;
};

} // namespace ptgn

//
// #include <array>
// #include <cstdint>
// #include <functional>
// #include <memory>
// #include <optional>
// #include <span>
// #include <vector>
//
// #include "app/context.h"
// #include "core/event/dispatcher.h"
// #include "core/graphics/blend_mode.h"
// #include "core/graphics/color.h"
// #include "core/graphics/flip.h"
// #include "core/math/vector2.h"
// #include "core/math/vector3.h"
// #include "renderer/backend/gl/gl_handle.h"
// #include "renderer/backend/gl/gl_state.h"
// #include "renderer/resources/texture_format.h"
// #include "renderer/resources/vertex.h"
//
// namespace ptgn {
//
// class Application;
// class Window;
// class Renderer;
// class EventHandler;
//
// template <class State, class Func>
// void UpdateStateIfChanged(Renderer&, State&, const State&, Func&&);
//
//// TODO: Move somewhere else.
///*
// struct LightParams {
//	V2_float position;
//	float radius;
//	Color color;
//	float intensity;
//	float falloff;
//	V3_float ambient_color;
//	float ambient_intensity;
//	V3_float attenuation;
// };
//*/
//
// struct RenderTarget {
//	impl::gl::Framebuffer framebuffer;
//	impl::gl::Texture color;
//	impl::gl::Renderbuffer depth; // optional
//	V2_int size;
//	TextureFormat format{ TextureFormat::RGBA8 };
//
//	bool operator==(const RenderTarget&) const = default;
// };
//
// namespace impl {
//
// namespace gl {
//
// class GLContext;
//
// } // namespace gl
//
// using Index = std::uint32_t;
//
// constexpr std::size_t batch_capacity{ 10000 };
// constexpr std::size_t vertex_capacity{ batch_capacity * 4 };
// constexpr std::size_t index_capacity{ batch_capacity * 6 };
//
//[[nodiscard]] static constexpr std::array<V2_float, 4> GetDefaultTextureCoordinates() {
//	return {
//		V2_float{ 0.0f, 0.0f },
//		V2_float{ 1.0f, 0.0f },
//		V2_float{ 1.0f, 1.0f },
//		V2_float{ 0.0f, 1.0f },
//	};
// }
//
//[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(
//	V2_float source_position, V2_float source_size, V2_float texture_size,
//	bool offset_texels = false
//);
//
// void FlipTextureCoordinates(std::array<V2_float, 4>& texture_coords, Flip flip);
//
// struct QuadDesc {
//	std::array<V2_float, 4> positions;
//	std::array<V2_float, 4> tex_coords;
//	Color color	   = color::White;
//	float rotation = 0.0f;
//	std::array<float, 4> user_data{};
// };
//
// struct PooledTarget {
//	RenderTarget target;
//	std::uint64_t last_used_tick = 0;
//	bool in_use					 = false;
// };
//
// } // namespace impl
//
// struct QuadParams {
//	V2_float center{};
//	V2_float size{ 0.0f, 0.0f };
//	float rotation = 0.0f;
//
//	bool flip_y = false;
//
//	Color tint = color::White;
//
//	std::optional<impl::gl::TextureId> texture;
//
//	std::optional<std::array<V2_float, 4>> tex_coords;
// };
//
// struct RenderPass {
//	RenderTarget source;
//
//	RenderTarget ping;
//	RenderTarget pong;
//
//	bool has_ping = false;
//	bool has_pong = false;
//
//	// "latest output" tracking
//	bool has_written_once = false; // false -> latest is source
//	bool latest_is_ping	  = true;  // valid only if has_written_once == true
// };
//
// class Renderer {
// public:
//	// TODO: Add display viewport.
//
//	Renderer() = delete;
//	Renderer(Window& window);
//	~Renderer() noexcept;
//	Renderer(const Renderer&)				 = delete;
//	Renderer(Renderer&&) noexcept			 = delete;
//	Renderer& operator=(const Renderer&)	 = delete;
//	Renderer& operator=(Renderer&&) noexcept = delete;
//
//	RenderTarget CreateRenderTarget(V2_int size, TextureFormat format) const;
//
//	void ResizeRenderTarget(RenderTarget& rt, V2_int new_size) const;
//
//	using UniformSetup = std::function<void(impl::gl::ShaderId)>;
//	using QuadSetup	   = std::function<void(impl::gl::ShaderId, impl::QuadDesc&)>;
//
//	// void DrawLightQuad(const LightParams& light);
//	void DrawTexturedQuad(
//		impl::gl::ShaderId shader, impl::gl::TextureId texture, V2_float center, V2_float size,
//		Color tint = color::White, bool flip_y = false
//	);
//	void DrawTexture(impl::gl::TextureId texture, V2_float center, V2_float size);
//	void DrawTexture(impl::gl::ShaderId shader, RenderPass& pass, const RenderTarget& scene_target);
//	void DrawQuadEx(impl::gl::ShaderId shader, const QuadParams& p, const UniformSetup& u = {});
//	void DrawQuadEx(impl::gl::ShaderId shader, const QuadParams& p, const QuadSetup& q);
//
//	void BindRenderTarget(const RenderTarget& rt);
//	void BindRenderTarget(RenderPass& pass);
//
//	void SetShader(impl::gl::ShaderId shader);
//	void SetBlend(BlendMode mode, bool enabled = true);
//	void SetDepth(const DepthState& depth);
//	void SetStencil(const StencilState& stencil);
//	void SetRaster(const RasterState& raster);
//	void SetColorMask(const ColorMaskState& color_mask);
//
// private:
//	friend class Application;
//	friend class EventHandler;
//
//	void OnEvent(EventDispatcher d);
// };
//
// } // namespace ptgn