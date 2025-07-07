#include "rendering/batching/render_data.h"

#include <array>
#include <cstdint>
#include <functional>
#include <list>
#include <numeric>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "components/common.h"
#include "components/drawable.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "events/event_handler.h"
#include "events/events.h"
#include "math/geometry.h"
#include "math/hash.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/api/flip.h"
#include "rendering/batching/vertex.h"
#include "rendering/buffers/buffer.h"
#include "rendering/buffers/frame_buffer.h"
#include "rendering/buffers/vertex_array.h"
#include "rendering/gl/gl_renderer.h"
#include "rendering/gl/gl_types.h"
#include "rendering/renderer.h"
#include "rendering/resources/render_target.h"
#include "rendering/resources/shader.h"
#include "rendering/resources/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

// TODO: Implement following behavior:
// Batched objects with no filters render directly to the main framebuffer.
// Objects with the same filters are rendered to an offscreen framebuffer (Render Target).
// The filter shader is then applied to this buffer, and the result is composited back into the main
// framebuffer.
// Multiple filters can be chained in sequence, with each producing an intermediate buffer for the
// next
// If multiple objects share the same filter instance, we can batch them together within that filter
// pipeline, assuming their textures and states are also compatible.

// clang-format off
/*
| **Aspect**                  | **Internal Filter (Inline Pipeline)**                   | **External Filter (PostFXPipeline)**                                           |
| --------------------------- | ------------------------------------------------------- | ------------------------------------------------------------------------------ |
| **Performance**             | 🔥 Fast — integrates with batching                      | 🐢 Slower — render-to-texture overhead per object/group                        |
| **Batches**                 | ✅ Maintains batching                                    | ❌ Breaks batching due to framebuffer isolation                                 |
| **Memory usage**            | Low — no framebuffers needed                            | Higher — offscreen framebuffers                                                |
| **Effect Scope**            | Per-fragment; operates on UVs, texture, vertex data     | Full-object or full-frame; works on the composite image of the object or group |
| **Sampling beyond UV**      | ❌ Not possible; shader sees only local texels           | ✅ Possible; can access neighbors, edges, alpha boundaries                      |
| **Effect Examples**         | Tint, pixelate, color shift, wave distortion, scanlines | Blur, glow, outline, drop shadow, bloom, CRT, vignette                         |
| **Group/Container effects** | ❌ Cannot apply to a group easily                        | ✅ Apply once to a container, entire scene, or camera                           |
| **Order Sensitivity**       | Works per object; respects z-order                      | Works after object render; can process groups together                         |
*/
// clang-format on

// TODO: For PreFX, scissor the frame buffer. Use a frame buffer pool which allocates new frame
// buffers with the same size.
// TODO: For PostFX, check if post FX are different and if so, flush.

#define HDR_ENABLED 0

namespace ptgn::impl {

std::array<Vertex, 4> GetQuadVertices(
	const std::array<V2_float, 4>& quad_points, const Color& color, const Depth& depth,
	float texture_index, std::array<V2_float, 4> texture_coordinates, bool flip_vertices
) {
	std::array<Vertex, 4> vertices{};

	auto c{ color.Normalized() };

	if (flip_vertices) {
		FlipTextureCoordinates(texture_coordinates, Flip::Vertical);
	}

	PTGN_ASSERT(vertices.size() == quad_points.size());
	PTGN_ASSERT(vertices.size() == texture_coordinates.size());

	for (std::size_t i{ 0 }; i < vertices.size(); ++i) {
		vertices[i].position  = { quad_points[i].x, quad_points[i].y, static_cast<float>(depth) };
		vertices[i].color	  = { c.x, c.y, c.z, c.w };
		vertices[i].tex_coord = { texture_coordinates[i].x, texture_coordinates[i].y };
		vertices[i].tex_index = { texture_index };
	}

	return vertices;
}

ShaderPass::ShaderPass(const Shader& shader, UniformCallback uniform_callback) :
	shader_{ &shader }, uniform_callback_{ uniform_callback } {}

const Shader& ShaderPass::GetShader() const {
	PTGN_ASSERT(shader_ != nullptr);
	return *shader_;
}

void ShaderPass::Invoke(Entity entity) const {
	PTGN_ASSERT(shader_ != nullptr);
	if (uniform_callback_) {
		std::invoke(uniform_callback_, entity, *shader_);
	}
}

bool ShaderPass::operator==(const ShaderPass& other) const {
	return shader_ == other.shader_ && uniform_callback_ == other.uniform_callback_;
}

bool ShaderPass::operator!=(const ShaderPass& other) const {
	return !(*this == other);
}

FrameBufferContext::FrameBufferContext(const V2_int& size, TextureFormat format) :
	format_{ format }, frame_buffer_{ impl::Texture{ nullptr, size, format_ } }, timer_{ true } {}

bool FrameBufferContext::TimerCompleted(milliseconds duration) const {
	return timer_.Completed(duration);
}

V2_int FrameBufferContext::GetSize() const {
	return frame_buffer_.GetTexture().GetSize();
}

const FrameBuffer& FrameBufferContext::GetFrameBuffer() const {
	return frame_buffer_;
}

FrameBuffer& FrameBufferContext::GetFrameBuffer() {
	return frame_buffer_;
}

void FrameBufferContext::Resize(const V2_int& new_size) {
	if (GetSize() == new_size) {
		return;
	}

	frame_buffer_ = FrameBuffer{ impl::Texture{ nullptr, new_size, format_ } };
	timer_.Start();
}

FrameBufferPool::FrameBufferPool(milliseconds max_age, size_t max_pool_size) :
	max_age_{ max_age }, max_pool_size_{ max_pool_size } {}

std::size_t FrameBufferPool::KeyForSize(const V2_float& size) const {
	return Hash(size);
}

void FrameBufferPool::Add(std::shared_ptr<FrameBufferContext> ctx) {
	// TODO: Find use for this?
	if (std::find(age_pool_.begin(), age_pool_.end(), ctx) != age_pool_.end()) {
		return;
	}

	auto key{ KeyForSize(ctx->GetSize()) };
	size_pool_[key].push_back(ctx);
	age_pool_.push_back(ctx);
}

std::shared_ptr<FrameBufferContext> FrameBufferPool::Get(V2_float size, TextureFormat format) {
	// TODO: Fix this function and remove used_contexts.
	PTGN_ASSERT(size.x > 0 && size.y > 0);
	size.x = std::min(size.x, 4096.0f);
	size.y = std::min(size.y, 4096.0f);

	auto key		   = KeyForSize(size);
	auto& pool_by_size = size_pool_[key];

	// 1. Reuse same-size framebuffer
	if (!pool_by_size.empty()) {
		auto ctx = pool_by_size.back();
		pool_by_size.pop_back();
		age_pool_.erase(std::remove(age_pool_.begin(), age_pool_.end(), ctx), age_pool_.end());
		return ctx;
	}

	// 2. Reuse framebuffer old enough (based on its internal Timer)
	if (!age_pool_.empty()) {
		auto ctx = age_pool_.front();
		if (ctx->TimerCompleted(max_age_)) {
			age_pool_.erase(age_pool_.begin());

			auto old_key{ KeyForSize(ctx->GetSize()) };
			auto& old_list = size_pool_[old_key];
			old_list.erase(std::remove(old_list.begin(), old_list.end(), ctx), old_list.end());

			ctx->Resize(size);
			return ctx;
		}
	}

	// 3. Allocate new framebuffer (within pool size)
	if (age_pool_.size() < max_pool_size_) {
		return std::make_shared<FrameBufferContext>(size, format);
	}

	// 4. Reuse oldest framebuffer (resize)
	if (!age_pool_.empty()) {
		auto ctx = age_pool_.front();
		age_pool_.erase(age_pool_.begin());

		auto old_key{ KeyForSize(ctx->GetSize()) };
		auto& old_list = size_pool_[old_key];
		old_list.erase(std::remove(old_list.begin(), old_list.end(), ctx), old_list.end());

		ctx->Resize(size);
		return ctx;
	}

	// 5. Create new framebuffer (exceeding pool size)
	return std::make_shared<FrameBufferContext>(size, format);
}

void FrameBufferPool::SetMaxAge(milliseconds max_age) {
	max_age_ = max_age;
}

void FrameBufferPool::SetMaxPoolSize(size_t max_size) {
	max_pool_size_ = max_size;
}

void FrameBufferPool::Clear() {
	// TODO: Find use for this?
	age_pool_.clear();
	size_pool_.clear();
}

void FrameBufferPool::Prune() {
	// TODO: Find use for this?
	if (age_pool_.size() <= max_pool_size_) {
		return;
	}

	std::size_t excess{ age_pool_.size() - max_pool_size_ };
	for (std::size_t i = 0; i < excess; ++i) {
		auto ctx{ age_pool_[i] };
		auto key{ KeyForSize(ctx->GetSize()) };
		auto& pool{ size_pool_[key] };
		pool.erase(std::remove(pool.begin(), pool.end(), ctx), pool.end());
	}

	age_pool_.erase(age_pool_.begin(), age_pool_.begin() + excess);
}

void RenderData::Init() {
	GLRenderer::DisableGammaCorrection();

	max_texture_slots = GLRenderer::GetMaxTextureSlots();

	const auto& screen_shader{ game.shader.Get<ScreenShader::Default>() };
	PTGN_ASSERT(screen_shader.IsValid());
	screen_shader.Bind();
	screen_shader.SetUniform("u_Texture", 1);

	const auto& quad_shader{ game.shader.Get<ShapeShader::Quad>() };

	PTGN_ASSERT(quad_shader.IsValid());
	PTGN_ASSERT(game.shader.Get<ShapeShader::Circle>().IsValid());
	PTGN_ASSERT(game.shader.Get<ScreenShader::Default>().IsValid());
	PTGN_ASSERT(game.shader.Get<OtherShader::Light>().IsValid());

	std::vector<std::int32_t> samplers(max_texture_slots);
	std::iota(samplers.begin(), samplers.end(), 0);

	quad_shader.Bind();
	quad_shader.SetUniform(
		"u_Texture", samplers.data(), static_cast<std::int32_t>(samplers.size())
	);

	IndexBuffer quad_ib{ nullptr, index_capacity, static_cast<std::uint32_t>(sizeof(Index)),
						 BufferUsage::DynamicDraw };
	VertexBuffer quad_vb{ nullptr, vertex_capacity, static_cast<std::uint32_t>(sizeof(Vertex)),
						  BufferUsage::DynamicDraw };

	triangle_vao = VertexArray(
		PrimitiveMode::Triangles, std::move(quad_vb), quad_vertex_layout, std::move(quad_ib)
	);

	white_texture = Texture(static_cast<const void*>(&color::White), { 1, 1 });

	// TODO: Fix background color.

	screen_target = impl::CreateRenderTarget(
		render_manager.CreateEntity(), impl::CreateCamera(render_manager.CreateEntity()), { 1, 1 },
		color::Transparent, HDR_ENABLED ? TextureFormat::HDR_RGBA : TextureFormat::RGBA8888
	);
	ping_target = impl::CreateRenderTarget(
		render_manager.CreateEntity(), impl::CreateCamera(render_manager.CreateEntity()), { 1, 1 },
		color::Transparent, HDR_ENABLED ? TextureFormat::HDR_RGBA : TextureFormat::RGBA8888
	);
	pong_target = impl::CreateRenderTarget(
		render_manager.CreateEntity(), impl::CreateCamera(render_manager.CreateEntity()), { 1, 1 },
		color::Transparent, HDR_ENABLED ? TextureFormat::HDR_RGBA : TextureFormat::RGBA8888
	);
	screen_target.SetBlendMode(BlendMode::None);
	ping_target.SetBlendMode(BlendMode::Blend);
	pong_target.SetBlendMode(BlendMode::Blend);
	intermediate_target = {};

	// TODO: Once render target window resizing is implemented, get rid of this.
	game.event.window.Subscribe(
		WindowEvent::Resized, this, std::function([&](const WindowResizedEvent& e) {
			screen_target.GetTexture().Resize(e.size);
			ping_target.GetTexture().Resize(e.size);
			pong_target.GetTexture().Resize(e.size);
		})
	);

#ifdef PTGN_PLATFORM_MACOS
	// Prevents MacOS warning: "UNSUPPORTED (log once): POSSIBLE ISSUE: unit X
	// GLD_TEXTURE_INDEX_2D is unloadable and bound to sampler type (Float) - using zero
	// texture because texture unloadable."
	for (std::uint32_t slot{ 0 }; slot < max_texture_slots; slot++) {
		Texture::Bind(white_texture.GetId(), slot);
	}
#endif

	SetState(RenderState{ {}, BlendMode::None, {} });
}

void RenderData::AddTriangle(const std::array<Vertex, 3>& points, const RenderState& state) {
	SetState(state);

	AddVertices(points, triangle_indices);
}

float RenderData::GetTextureIndex(std::uint32_t texture_id) {
	PTGN_ASSERT(texture_id != white_texture.GetId());
	// Texture exists in batch, therefore do not add it again.
	for (std::size_t i{ 0 }; i < textures.size(); i++) {
		if (textures[i] == texture_id) {
			// i + 1 because first texture index is white texture.
			return static_cast<float>(i + 1);
		}
	}
	// Batch is at texture capacity.
	if (static_cast<std::uint32_t>(textures.size()) == max_texture_slots - 1) {
		Flush();
	}
	// i + 1 is implicit here because size is taken after emplacing.
	return static_cast<float>(textures.size() + 1);
}

void RenderData::AddTexturedQuad(
	const Transform& transform, const V2_float& size, Origin origin, const Color& tint,
	const Depth& depth, const std::array<V2_float, 4>& texture_coordinates, RenderState state,
	const Texture& texture
) {
	PTGN_ASSERT(texture.IsValid());

	auto points{ impl::GetQuadVertices(
		impl::GetVertices(transform, size, origin), tint, depth, 0.0f, texture_coordinates
	) };

	auto texture_id{ texture.GetId() };
	auto texture_size{ texture.GetSize() };

	PTGN_ASSERT(!texture_size.IsZero(), "Texture must have a non-zero size");

	bool pre_fx{ !state.pre_fx.pre_fx_.empty() };

	std::shared_ptr<FrameBufferContext> context;

	if (pre_fx) {
		V2_float extents{ texture_size * 0.5f };
		Matrix4 camera{ Matrix4::Orthographic(
			-extents.x, extents.x, extents.y, -extents.y, -std::numeric_limits<float>::infinity(),
			std::numeric_limits<float>::infinity()
		) };

		std::array<V2_float, 4> camera_positions{};
		for (std::size_t i{ 0 }; i < camera_positions.size(); i++) {
			camera_positions[i] = impl::default_texture_coordinates[i] * texture_size - extents;
		}

		auto texture_format{ TextureFormat::RGBA8888 };

		auto ping{ frame_buffer_pool.Get(texture_size, texture_format) };
		auto pong{ frame_buffer_pool.Get(texture_size, texture_format) };

		PTGN_ASSERT(ping != nullptr && pong != nullptr);
		PTGN_ASSERT(ping->GetSize() == texture_size);
		PTGN_ASSERT(pong->GetSize() == texture_size);

		for (auto it = state.pre_fx.pre_fx_.begin(); it != state.pre_fx.pre_fx_.end(); ++it) {
			const auto& fx{ *it };
			DrawTo(pong->GetFrameBuffer());
			PTGN_ASSERT(pong->GetFrameBuffer().IsBound());
			impl::GLRenderer::ClearToColor(color::Transparent);

			const auto& shader_pass{ fx.Get<ShaderPass>() };
			const auto& shader{ shader_pass.GetShader() };

			BindCamera(shader, camera);

			// assert that vertices is screen vertices.
			GLRenderer::SetViewport({ 0, 0 }, texture_size);
			GLRenderer::SetBlendMode(fx.GetBlendMode());

			if (it == state.pre_fx.pre_fx_.begin()) {
				ReadFrom(texture);
			} else {
				ReadFrom(ping->GetFrameBuffer());
			}

			// TODO: Cache this somehow?
			SetCameraVertices(camera_positions, depth);

			shader.SetUniform("u_Texture", 1);
			shader.SetUniform("u_Resolution", V2_float{ texture_size });

			shader_pass.Invoke(fx);

			DrawVertexArray(quad_indices.size());

			std::swap(ping, pong);
		}

		state.pre_fx.pre_fx_.clear();
		context	   = ping;
		texture_id = context->GetFrameBuffer().GetTexture().GetId();

		white_texture.Bind(0);
	}

	float texture_index{ GetTextureIndex(texture_id) };

	for (auto& v : points) {
		v.tex_index = { texture_index };
	}

	SetState(state);

	AddVertices(points, quad_indices);
	// Must be done after because SetState may Flush the current batch, which will
	// clear textures.
	textures.emplace_back(texture_id);

	if (pre_fx) {
		force_flush = true;
		PTGN_ASSERT(context != nullptr);
		// Must be done after because SetState may Flush the current batch, which will
		// clear used contexts.
		used_contexts.emplace_back(context);
	}
}

void RenderData::AddQuad(
	const Transform& transform, const V2_float& size, Origin origin, const Color& tint,
	const Depth& depth, const RenderState& state, float texture_index
) {
	SetState(state);

	auto points{ impl::GetQuadVertices(
		impl::GetVertices(transform, size, origin), tint, depth, texture_index,
		impl::default_texture_coordinates
	) };

	AddVertices(points, quad_indices);
}

void RenderData::AddThinQuad(
	const Transform& transform, const V2_float& size, Origin origin, const Color& tint,
	const Depth& depth, float line_width, const RenderState& state
) {
	std::array<V2_float, 4> quad_points{ impl::GetVertices(transform, size, origin) };

	auto quad_vertices{
		impl::GetQuadVertices(quad_points, tint, depth, 0.0f, impl::default_texture_coordinates)
	};

	PTGN_ASSERT(line_width >= min_line_width, "Invalid line width for Rect");

	// Compose the quad out of 4 lines, each made up of line_width wide quads.
	for (std::size_t i{ 0 }; i < quad_points.size(); i++) {
		auto start{ state.camera.ZoomIfNeeded(quad_points[i]) };
		auto end{ state.camera.ZoomIfNeeded(quad_points[(i + 1) % quad_points.size()]) };

		auto line_points{ impl::GetLineQuadVertices(start, end, line_width) };

		for (std::size_t j{ 0 }; j < line_points.size(); j++) {
			quad_vertices[j].position[0] = line_points[j].x;
			quad_vertices[j].position[1] = line_points[j].y;
		}

		SetState(state);

		AddVertices(quad_vertices, quad_indices);
	}
}

bool RenderData::SetState(const RenderState& new_render_state) {
	PTGN_ASSERT(
		new_render_state.pre_fx.pre_fx_.empty(),
		"PreFX must be applied before setting the state: Possibly an unsupported shape with PreFX"
	);
	if (new_render_state == render_state && !force_flush) {
		return false;
	}
	// New render state or PreFX present.
	Flush();
	render_state = new_render_state;
	return true;
}

void RenderData::BindCamera(const Shader& shader, const Matrix4& view_projection) {
	shader.Bind();
	shader.SetUniform("u_ViewProjection", view_projection);
}

void RenderData::UpdateVertexArray(
	const Vertex* data_vertices, std::size_t vertex_count, const Index* data_indices,
	std::size_t index_count
) {
	triangle_vao.Bind();

	triangle_vao.GetVertexBuffer().SetSubData(
		data_vertices, 0, static_cast<std::uint32_t>(vertex_count), sizeof(Vertex), false, true
	);

	triangle_vao.GetIndexBuffer().SetSubData(
		data_indices, 0, static_cast<std::uint32_t>(index_count), sizeof(Index), false, true
	);
}

void RenderData::SetRenderParameters(const Camera& camera, BlendMode blend_mode) {
	SetViewport(camera);
	GLRenderer::SetBlendMode(blend_mode);
}

void RenderData::SetViewport(const Camera& camera) {
	GLRenderer::SetViewport(camera.GetViewportPosition(), camera.GetViewportSize());
}

void RenderData::SetCameraVertices(const std::array<V2_float, 4>& positions, const Depth& depth) {
	camera_vertices =
		GetQuadVertices(positions, color::White, depth, 1.0f, default_texture_coordinates, true);

	UpdateVertexArray(camera_vertices, quad_indices);
}

void RenderData::SetCameraVertices(const Camera& camera) {
	SetCameraVertices(camera.GetVertices(), camera.GetDepth());
}

void RenderData::DrawTo(const FrameBuffer& frame_buffer) {
	PTGN_ASSERT(frame_buffer.IsValid());
	frame_buffer.Bind();
}

void RenderData::DrawTo(const RenderTarget& render_target) {
	PTGN_ASSERT(render_target);
	DrawTo(render_target.GetFrameBuffer());
}

void RenderData::ReadFrom(const Texture& texture) {
	PTGN_ASSERT(texture.IsValid());
	texture.Bind(1);
}

void RenderData::ReadFrom(const FrameBuffer& frame_buffer) {
	PTGN_ASSERT(frame_buffer.IsValid());
	ReadFrom(frame_buffer.GetTexture());
}

void RenderData::ReadFrom(const RenderTarget& render_target) {
	PTGN_ASSERT(render_target);
	ReadFrom(render_target.GetFrameBuffer());
}

RenderTarget RenderData::GetPingPongTarget() const {
	PTGN_ASSERT(ping_target && pong_target);
	if (intermediate_target == ping_target) {
		return pong_target;
	}
	return ping_target;
}

void RenderData::AddShader(
	Entity entity, const RenderState& state, BlendMode target_blend_mode,
	const Color& target_clear_color, bool uses_scene_texture
) {
	auto old_blend_mode{ intermediate_target.GetBlendMode() };
	if (bool state_changed{ SetState(state) }; state_changed || uses_scene_texture) {
		intermediate_target = GetPingPongTarget();
		DrawTo(intermediate_target);
		intermediate_target.ClearToColor(target_clear_color);
		intermediate_target.SetBlendMode(target_blend_mode);
		ReadFrom(screen_target);
	} else {
		PTGN_ASSERT(intermediate_target);
	}

	auto camera{ GetCamera(intermediate_target.GetCamera()) };
	PTGN_ASSERT(camera);

	SetCameraVertices(camera);

	GLRenderer::SetBlendMode(render_state.blend_mode);

	const auto& shader{ render_state.shader_passes.GetShader() };
	PTGN_ASSERT(shader != game.shader.Get<ShapeShader::Quad>());
	// TODO: Only update these if shader bind is dirty.
	BindCamera(shader, camera);
	shader.SetUniform("u_Texture", 1);
	shader.SetUniform("u_Resolution", camera.GetViewportSize());
	render_state.shader_passes.Invoke(entity);

	DrawVertexArray(quad_indices.size());

	intermediate_target.SetBlendMode(old_blend_mode);
}

void RenderData::BindTextures() const {
	PTGN_ASSERT(textures.size() <= max_texture_slots);

	for (std::uint32_t i{ 0 }; i < static_cast<std::uint32_t>(textures.size()); i++) {
		// Save first texture slot for empty white texture.
		std::uint32_t slot{ i + 1 };
		Texture::Bind(textures[i], slot);
	}
}

Camera RenderData::GetCamera(const Camera& fallback) const {
	if (render_state.camera) {
		return render_state.camera;
	}
	PTGN_ASSERT(fallback);
	return fallback;
}

void RenderData::Flush() {
	if (!game.scene.HasCurrent()) {
		return;
	}

	const auto draw_vertices_to = [&](const auto& camera, const auto& target,
									  const ShaderPass& shader_pass) {
		const auto& camera_vp{ camera.GetViewProjection() };

		DrawTo(target);
		UpdateVertexArray(vertices, indices);
		SetRenderParameters(camera, render_state.blend_mode);
		BindTextures();

		// TODO: Only set uniform if camera changed.
		BindCamera(shader_pass.GetShader(), camera_vp);

		// TODO: Call shader pass uniform.

		DrawVertexArray(indices.size());
	};

	PTGN_ASSERT(
		render_state.pre_fx.pre_fx_.empty(), "Pre fx must be applied internally before flushing: "
											 "Possibly an unsupported shape with PreFX"
	);

	if (!render_state.post_fx.post_fx_.empty()) {
		if (!vertices.empty() && !indices.empty()) {
			PTGN_ASSERT(!intermediate_target);
			intermediate_target = GetPingPongTarget();
			intermediate_target.ClearToColor(color::Transparent);
			draw_vertices_to(
				GetCamera(game.scene.GetCurrent().camera.primary), intermediate_target,
				render_state.shader_passes
			);
		}
		PTGN_ASSERT(
			intermediate_target, "Intermediate target must be used before rendering post fx"
		);
		for (const auto& fx : render_state.post_fx.post_fx_) {
			auto camera{
				game.scene.GetCurrent().camera.window
			}; // Scene camera or render target camera.
			PTGN_ASSERT(camera);

			auto ping{ intermediate_target };
			auto pong{ GetPingPongTarget() };

			DrawTo(pong);
			pong.ClearToColor(color::Transparent);

			const auto& shader_pass{ fx.Get<ShaderPass>() };
			const auto& shader{ shader_pass.GetShader() };

			BindCamera(shader, camera);

			/*PTGN_ASSERT(vertices.size() == 0);
			PTGN_ASSERT(indices.size() == 0);*/
			// assert that vertices is screen vertices.
			SetRenderParameters(camera, fx.GetBlendMode());

			ReadFrom(ping);

			// TODO: Cache this somehow?
			SetCameraVertices(camera);

			shader.SetUniform("u_Texture", 1);
			shader.SetUniform("u_Resolution", camera.GetViewportSize());

			shader_pass.Invoke(fx);

			DrawVertexArray(quad_indices.size());

			intermediate_target = pong;
		}
	} else {
		//	PTGN_LOG("No post fx");
	}

	if (intermediate_target) {
		PTGN_ASSERT(intermediate_target);

		auto camera{ game.scene.GetCurrent().camera.window };

		PTGN_ASSERT(camera);

		DrawTo(screen_target);
		//	PTGN_LOG("PreDraw: ", screen_target.GetFrameBuffer().GetPixel({ 200, 200 }));

		const auto& shader{ game.shader.Get<ScreenShader::Default>() };

		BindCamera(shader, camera);
		/*PTGN_ASSERT(vertices.size() == 0);
		PTGN_ASSERT(indices.size() == 0);*/
		// assert that vertices is screen vertices.
		SetRenderParameters(camera, intermediate_target.GetBlendMode());

		ReadFrom(intermediate_target);
		// PTGN_LOG("Intermediate: ", intermediate_target.GetFrameBuffer().GetPixel({ 200, 200 }));
		//	PTGN_LOG("Blend mode: ", intermediate_target.GetBlendMode());

		// PTGN_LOG(intermediate_target.GetFrameBuffer().GetPixel({ 300, 300 }));
		// TODO: Cache this somehow?
		SetCameraVertices(camera);

		DrawVertexArray(quad_indices.size());
		//	PTGN_LOG("PostDraw: ", screen_target.GetFrameBuffer().GetPixel({ 200, 200 }));

	} else if (!vertices.empty() && !indices.empty()) {
		draw_vertices_to(
			GetCamera(game.scene.GetCurrent().camera.primary), screen_target,
			render_state.shader_passes
		);
	}

	intermediate_target = {};

	Reset();
}

void RenderData::Reset() {
	vertices.clear();
	indices.clear();
	textures.clear();
	index_offset = 0;
	force_flush	 = false;
	used_contexts.clear();
}

void RenderData::DrawVertexArray(std::size_t index_count) const {
	GLRenderer::DrawElements(triangle_vao, index_count, false);
}

void RenderData::InvokeDrawable(const Entity& entity) {
	PTGN_ASSERT(entity.Has<IDrawable>(), "Cannot render entity without drawable component");

	const auto& drawable{ entity.Get<IDrawable>() };

	const auto& drawable_functions{ IDrawable::data() };

	const auto it{ drawable_functions.find(drawable.hash) };

	PTGN_ASSERT(it != drawable_functions.end(), "Failed to identify drawable hash");

	const auto& draw_function{ it->second };

	std::invoke(draw_function, *this, entity);
}

void RenderData::DrawEntities(const std::vector<Entity>& entities) {
	for (const auto& entity : entities) {
		InvokeDrawable(entity);
	}
}

void RenderData::DrawScene(Scene& scene) {
	std::vector<Entity> regular_entities;
	regular_entities.reserve(scene.Size());

	// TODO: Fix render target entities.

	// std::vector<Entity> rt_entities;

	/*for (auto [e, rt] : scene.EntitiesWith<Visible, IDrawable, RenderTarget>()) {
		rt_entities.emplace_back(e);
	}*/

	for (auto [entity, visible, drawable] : scene.EntitiesWith<Visible, IDrawable>()) {
		if (!visible || entity.Has<RenderTarget>()) {
			continue;
		}
		if (entity.Has<QuadVertices>()) {
			// TODO: Update dirty vertices here?
			regular_entities.emplace_back(entity);
		} else {
			// TODO: Add general vertices.
			regular_entities.emplace_back(entity);
		}
	}

	// SortEntities<true>(rt_entities);
	SortEntities<false>(regular_entities);

	// for (auto e : rt_entities) {
	//	auto& rt = e.Get<RenderTarget>();
	//	// rt.Draw(e);
	// }

	DrawEntities(regular_entities);
}

void RenderData::DrawToScreen() {
	FrameBuffer::Unbind();

	auto camera{ game.scene.GetCurrent().camera.window };

	SetCameraVertices(camera);
	SetRenderParameters(camera, screen_target.GetBlendMode());

	const Shader* shader{ nullptr };

	if constexpr (HDR_ENABLED) {
		shader = &game.shader.Get<OtherShader::ToneMapping>();
	} else {
		shader = &game.shader.Get<ScreenShader::Default>();
	}

	PTGN_ASSERT(shader != nullptr);

	BindCamera(*shader, camera);

	if constexpr (HDR_ENABLED) {
		shader->SetUniform("u_Texture", 1);
		shader->SetUniform("u_Exposure", 1.0f);
		shader->SetUniform("u_Gamma", 2.2f);
	}

	ReadFrom(screen_target);

	DrawVertexArray(quad_indices.size());
}

void RenderData::ClearRenderTargets(Scene& scene) {
	screen_target.Clear();
	ping_target.Clear();
	pong_target.Clear();

	// TODO: Clear all render target entities.
}

void RenderData::Draw(Scene& scene) {
	ClearRenderTargets(scene);

	white_texture.Bind(0);

	DrawScene(scene);

	Flush();
	render_state		= {};
	intermediate_target = {};

	DrawToScreen();

	Reset();

	// TODO: Check if this is needed.
}

} // namespace ptgn::impl