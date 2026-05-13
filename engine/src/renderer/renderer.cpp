
#include "renderer/renderer.h"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/graphics/surface.h"
#include "core/log.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/hash.h"
#include "platform/window.h"
#include "renderer/backend/gl/gl_buffer.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_framebuffer.h"
#include "renderer/backend/gl/gl_renderbuffer.h"
#include "renderer/backend/gl/gl_shader.h"
#include "renderer/backend/gl/gl_state.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/backend/gl/gl_vertex_array.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/primitive_mode.h"
#include "renderer/pipeline/render_batch.h"
#include "renderer/pipeline/render_packet.h"
#include "renderer/pipeline/render_pass.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_resource.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/render_graph.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "renderer/vertex/vertex.h"

namespace ptgn::impl {

Renderer::Renderer(Window& window, EventSink&& event_sink) :
	window_{ window },
	event_sink_{ std::move(event_sink) },
	gl_{ std::make_unique<gl::GLContext>() },
	pipeline_manager_{ *this } {
	pipeline_manager_.AddPipeline<TextureVertex>(
		"texture", kVertexCapacity, kIndexCapacity, PrimitiveMode::Triangles
	);
	pipeline_manager_.AddPipeline<ShapeVertex>(
		"shape", kVertexCapacity, kIndexCapacity, PrimitiveMode::Triangles
	);
	pipeline_manager_.AddPipeline<ColorVertex>(
		"color", kVertexCapacity, kIndexCapacity, PrimitiveMode::Triangles
	);
	pipeline_manager_.AddPipeline<TextureVertex>(
		"text", kVertexCapacity, kIndexCapacity, PrimitiveMode::Triangles
	);
	SetCurrentPipeline("texture");

	game_size_ = GetFullViewportSize();

	auto display{ RecalculateDisplayViewport() };

	display_viewport_		= display.viewport;
	display_viewport_dirty_ = false;

	auto display_size{ GetDisplaySize() };

	PTGN_ASSERT(display_size.BothAboveZero(), "Display size cannot be zero");

	screen_target_ = CreateRenderTarget(display_size, TextureFormat::RGBA8);
	BindScreenTarget();
	V2_float half_viewport{ display_size / 2.0f };
	auto view_projection{ Matrix4::Orthographic(-half_viewport, half_viewport) };
	SetViewProjection(view_projection);

	auto max_texture_slots{ GetMaxTextureSlots() };

	std::vector<std::int32_t> samplers(max_texture_slots);
	std::iota(samplers.begin(), samplers.end(), 0);

	auto text{ gl_->shaders.GetProgram("text") };
	auto _2 = gl_->Bind(text, false);
	SetUniform(text, "u_Textures", samplers);

	auto quad{ gl_->shaders.GetProgram("texture") };
	auto _1 = gl_->Bind(quad, false);
	SetUniform(quad, "u_Textures", samplers);

#ifdef PTGN_PLATFORM_MACOS
	//  Prevents MacOS warning: "UNSUPPORTED (log once): POSSIBLE ISSUE: unit X
	//  GLD_TEXTURE_INDEX_2D is unloadable and bound to sampler type (Float) - using zero
	//  texture because texture unloadable."
	for (std::uint32_t slot{ 0 }; slot < max_texture_slots; slot++) {
		gl_->SetActiveTextureSlot(slot);
		auto _3 = gl_->Bind(TextureId{ 0 }, false);
	}
#endif
}

Renderer::~Renderer() noexcept {
	// Guarantees that a vertex array object is bound before destroying any buffers.
	auto _{ gl_->Bind(VertexArrayId{ 0 }, false) };
}

RenderTargetObject Renderer::CreateRenderTarget(
	V2_int size, TextureFormat format, TextureParameters params
) {
	auto color = gl_->textures.CreateTexture(size, format, params);

	std::optional<RenderbufferId> depth;

	if (!IsColorFormat(format)) {
		depth = gl_->renderbuffers.CreateRenderbuffer(size, format);
	}

	using enum gl::Attachment;

	auto framebuffer = gl_->framebuffers.CreateFramebuffer(
		color, Color0, depth, IsDepthOnlyFormat(format) ? Depth : DepthStencil
	);

	return RenderTargetObject{ this, RenderTargetId{ framebuffer } };
}

TextureId Renderer::GetRenderTargetTexture(RenderTargetId render_target) const {
	const auto& color_attachment{ gl_->framebuffers.GetFramebufferAttachment(
		FramebufferId{ render_target }, gl::Attachment::Color0
	) };
	PTGN_ASSERT(
		color_attachment.id,
		"Render target must have a valid color attachment for its texture to be retrieved"
	);
	return TextureId{ color_attachment.id };
}

V2_int Renderer::GetRenderTargetSize(RenderTargetId render_target) const {
	auto id{ GetRenderTargetTexture(render_target) };

	auto size{ gl_->textures.GetTextureSize(id) };

	return size;
}

TextureFormat Renderer::GetRenderTargetTextureFormat(RenderTargetId render_target) const {
	auto id{ GetRenderTargetTexture(render_target) };

	auto texture_format{ gl_->textures.GetTextureFormat(id) };

	return texture_format;
}

void Renderer::ClearRenderTarget(RenderTargetId render_target, Color color, bool set_viewport)
	const {
	auto bind_guard = gl_->Bind(FramebufferId{ render_target }, true);

	std::optional<Viewport> viewport;
	if (set_viewport) {
		viewport = gl_->GetViewport();

		auto render_target_size{ GetRenderTargetSize(render_target) };

		gl_->SetViewport({ .position{}, .size{ render_target_size } });
	}

	gl_->framebuffers.ClearToColor(FramebufferId{ render_target }, color);

	if (set_viewport && viewport.has_value()) {
		gl_->SetViewport(*viewport);
	}
}

void Renderer::BindRenderTarget(RenderTargetId render_target) {
	SetFramebuffer(FramebufferId{ render_target });
}

void Renderer::SetCurrentPipeline(std::string_view name) {
	SetCurrentPipeline(Hash(name));
}

void Renderer::SetCurrentPipeline(std::size_t id) {
	if (pipeline_manager_.IsCurrentPipeline(id)) {
		return;
	}
	PTGN_ASSERT(pipeline_manager_.HasPipeline(id), "No matching pipeline found: ", id);
	FlushBatch();
	pipeline_manager_.SetCurrentPipeline(id);
}

void Renderer::SetViewport(Viewport viewport) {
	if (viewport == gl_->GetViewport()) {
		return;
	}
	FlushBatch();
	gl_->SetViewport(viewport);
}

void Renderer::SetShader(ShaderId shader) {
	if (shader == gl_->GetBoundState().shader_program) {
		return;
	}
	FlushBatch();
	auto _ = gl_->Bind(shader, false);
	gl_->shaders.SetUniform(shader, "u_ViewProjection", view_projection_);
}

void Renderer::SetBlendMode(BlendMode blend_mode) {
	if (blend_mode == gl_->GetBoundState().blend_mode) {
		return;
	}
	FlushBatch();
	gl_->SetBlendMode(blend_mode);
}

void Renderer::SetViewProjection(const Matrix4& view_projection) {
	if (view_projection_ != view_projection) {
		FlushBatch();
		view_projection_ = view_projection;
	}
	// TODO: Find a better way to do this. This is needed to ensure that the shader's
	// uniform is updated even if the shader itself doesn't change.
	if (auto shader{ gl_->GetBoundShader() }; shader.has_value() && *shader) {
		gl_->shaders.SetUniform(*shader, "u_ViewProjection", view_projection_);
	}
}

void Renderer::SetFramebuffer(FramebufferId framebuffer) {
	if (framebuffer == gl_->GetBoundFramebuffer()) {
		return;
	}
	FlushBatch();
	auto _ = gl_->Bind(framebuffer, false);
}

void Renderer::SetDepthTesting(bool enabled) {
	if (enabled == gl_->GetBoundState().depth_testing) {
		return;
	}
	FlushBatch();
	gl_->SetDepthTesting(enabled);
}

void Renderer::SetDepthMask(const DepthMaskState& mask) {
	if (mask == gl_->GetBoundState().depth_mask) {
		return;
	}
	FlushBatch();
	gl_->SetDepthMask(mask);
}

void Renderer::SetStencil(const StencilState& stencil) {
	if (stencil == gl_->GetBoundState().stencil) {
		return;
	}
	FlushBatch();
	gl_->SetStencil(stencil);
}

void Renderer::SetRaster(const RasterState& raster) {
	if (raster == gl_->GetBoundState().raster) {
		return;
	}
	FlushBatch();
	gl_->SetRaster(raster);
}

void Renderer::SetScissor(const ScissorState& scissor) {
	if (scissor == gl_->GetBoundState().scissor) {
		return;
	}
	FlushBatch();
	gl_->SetScissor(scissor);
}

void Renderer::SetColorMask(const ColorMaskState& color_mask) {
	if (color_mask == gl_->GetBoundState().color_mask) {
		return;
	}
	FlushBatch();
	gl_->SetColorMask(color_mask);
}

ShaderId Renderer::GetShader(std::string_view name) const {
	return gl_->shaders.GetProgram(name);
}

bool Renderer::IsTextureAttachedToCurrentFramebuffer(TextureId texture) const {
	auto bound{ gl_->GetBoundFramebuffer() };

	if (!bound.has_value() || *bound == FramebufferId{ 0 }) {
		return false;
	}

	return gl_->framebuffers.GetFramebufferAttachment(*bound, gl::Attachment::Color0).id == texture;
}

void Renderer::OnWindowResize(V2_int size) {
	if (presentation_viewport_.has_value()) {
		return;
	}

	if (!game_size_.has_value()) {
		event_sink_(size, ResizeType::Game);
	}

	event_sink_(size, impl::PresentationResizeType{});

	display_viewport_dirty_ = true;
}

void Renderer::SetGameSize(
	std::optional<V2_int> game_size, std::optional<ScalingMode> scaling_mode
) {
	if (game_size_ == game_size &&
		(!scaling_mode.has_value() || scaling_mode.has_value() && scaling_mode_ == scaling_mode)) {
		return;
	}

	PTGN_ASSERT(
		!game_size.has_value() || game_size.has_value() && game_size->BothAboveZero(),
		"Game size cannot be set to negative value or zero"
	);

	game_size_ = game_size;
	if (scaling_mode.has_value()) {
		scaling_mode_ = *scaling_mode;
	}

	auto size{ GetGameSize() };

	event_sink_(size, ResizeType::Game);

	display_viewport_dirty_ = true;
}

void Renderer::SetScalingMode(ScalingMode scaling_mode) {
	if (scaling_mode_ == scaling_mode) {
		return;
	}

	scaling_mode_ = scaling_mode;

	display_viewport_dirty_ = true;
}

void Renderer::SetPresentationViewport(std::optional<Viewport> presentation_viewport) {
	if (presentation_viewport_ == presentation_viewport) {
		return;
	}
	presentation_viewport_ = presentation_viewport;

	if (!presentation_viewport_.has_value()) {
		OnWindowResize(GetFullViewportSize());
		return;
	}

	if (!game_size_.has_value()) {
		event_sink_(presentation_viewport_->size, ResizeType::Game);
	}

	event_sink_(presentation_viewport_->size, impl::PresentationResizeType{});
	display_viewport_dirty_ = true;
}

bool Renderer::HasGameSize() const {
	return game_size_.has_value();
}

V2_int Renderer::GetGameSize() const {
	if (HasGameSize()) {
		return *game_size_;
	}
	return GetPresentationSize();
}

ScalingMode Renderer::GetScalingMode() const {
	return scaling_mode_;
}

Viewport Renderer::GetPresentationViewport() const {
	return { .position{ GetPresentationPosition() }, .size{ GetPresentationSize() } };
}

V2_int Renderer::GetPresentationPosition() const {
	if (presentation_viewport_.has_value()) {
		return presentation_viewport_->position;
	}
	return { 0, 0 };
}

V2_int Renderer::GetPresentationSize() const {
	if (presentation_viewport_.has_value()) {
		return presentation_viewport_->size;
	}
	return GetFullViewportSize();
}

Viewport Renderer::GetDisplayViewport() const {
	return display_viewport_;
}

V2_int Renderer::GetDisplayPosition() const {
	return display_viewport_.position;
}

V2_int Renderer::GetDisplaySize() const {
	return display_viewport_.size;
}

V2_float Renderer::GetScale() const {
	auto display_size{ GetDisplaySize() };
	auto game_size{ GetGameSize() };

	PTGN_ASSERT(display_size.BothAboveZero());
	PTGN_ASSERT(game_size.BothAboveZero());

	return V2_float{ display_size } / game_size;
}

V2_int Renderer::GetFullViewportSize() const {
	return window_.GetSize();
}

void Renderer::SetBackgroundColor(Color background_color) {
	background_color_ = background_color;
}

Color Renderer::GetBackgroundColor() const {
	return background_color_;
}

void Renderer::SetPrimaryWorldCamera(const std::optional<Camera>& primary_world_camera) {
	primary_world_camera_ = primary_world_camera;
}

const std::optional<Camera>& Renderer::GetPrimaryWorldCamera() const {
	return primary_world_camera_;
}

void Renderer::UpdateDisplayViewport(bool emit_events) {
	if (!display_viewport_dirty_) {
		return;
	}

	auto resize_info = RecalculateDisplayViewport();

	display_viewport_dirty_ = false;

	if (!resize_info.moved && !resize_info.resized) {
		return;
	}

	display_viewport_ = resize_info.viewport;

	if (resize_info.resized) {
		ResizeScreenTarget(display_viewport_.size);

		if (emit_events) {
			event_sink_(display_viewport_.size, ResizeType::Display);
		}
	}
}

Renderer::DisplayResizeInfo Renderer::RecalculateDisplayViewport() const {
	const auto presentation{ GetPresentationViewport() };

	PTGN_ASSERT(presentation.size.BothAboveZero());

	auto game_size{ game_size_.value_or(presentation.size) };

	PTGN_ASSERT(game_size.BothAboveZero());

	Viewport viewport{ .position{}, .size{ presentation.size } };

	auto compute_aspect_fit = [&viewport, game_size, presentation](bool letterbox_mode) {
		float presentation_aspect{ static_cast<float>(presentation.size.x) / presentation.size.y };
		float game_aspect{ static_cast<float>(game_size.x) / game_size.y };

		// In letterbox mode we need require presentation_aspect > game_aspect to fit
		// height, and in overscan we require presentation_aspect > game_aspect to fit
		// height.
		bool fit_height{ (presentation_aspect > game_aspect) == letterbox_mode };

		if (fit_height) {
			viewport.size.y = presentation.size.y;
			viewport.size.x =
				static_cast<int>(static_cast<float>(presentation.size.y) * game_aspect + 0.5f);
			viewport.position.x = (presentation.size.x - viewport.size.x) / 2; // left edge.
			viewport.position.y = 0;
		} else {
			// Fit width.
			viewport.size.x = presentation.size.x;
			viewport.size.y =
				static_cast<int>(static_cast<float>(presentation.size.x) / game_aspect + 0.5f);
			viewport.position.x = 0;
			viewport.position.y = (presentation.size.y - viewport.size.y) / 2; // top edge.
		}
	};

	switch (scaling_mode_) {
		case ScalingMode::Letterbox: compute_aspect_fit(true); break;
		case ScalingMode::Overscan:	 compute_aspect_fit(false); break;

		case ScalingMode::Stretch:
			PTGN_ASSERT(viewport.size == presentation.size);
			PTGN_ASSERT(viewport.position == V2_int{});
			// Viewport is full presentation area (default).
			break;

		case ScalingMode::IntegerScale: {
			V2_int ratio{ presentation.size / game_size };
			// Find which dimension limits the scaling factor.
			int scale{ std::max(1, std::min(ratio.x, ratio.y)) };
			viewport.size = game_size * scale;			 // scale up.
			viewport.position =
				(presentation.size - viewport.size) / 2; // center of presentation viewport.
			break;
		}

		case ScalingMode::Disabled:
			viewport.size = game_size;					 // no change.
			viewport.position =
				(presentation.size - viewport.size) / 2; // center of presentation viewport.
			break;

		default: PTGN_ERROR("Unsupported resolution mode");
	}

	bool resized{ viewport.size != display_viewport_.size };
	bool moved{ viewport.position != display_viewport_.position };

	PTGN_ASSERT(viewport.size.BothAboveZero());

	return { .moved = moved, .resized = resized, .viewport{ viewport } };
}

void Renderer::ResizeScreenTarget(V2_int size) {
	ResizeRenderTarget(screen_target_.resource_, size);
}

void Renderer::BindScreenTarget() {
	BindRenderTarget(screen_target_);
}

RenderTargetId Renderer::GetScreenTarget() const {
	return screen_target_.resource_;
}

void Renderer::InvalidateState() {
	gl_->InvalidateState();
}

void Renderer::BeginFrame() {
	InvalidateState();

	PTGN_ASSERT(batch_.vertices.empty());
	PTGN_ASSERT(batch_.indices.empty());

	if (!presentation_viewport_.has_value()) {
		auto presentation{ GetPresentationViewport() };
		Color window_background_color{ window_.GetBackgroundColor() };

		auto _1 = gl_->Bind(FramebufferId{ 0 }, false);
		gl_->SetClearColor(window_background_color);
		SetViewport(presentation);
		gl_->framebuffers.Clear();
	}

	BindScreenTarget();
	SetViewport({ .position{}, .size{ screen_target_.GetSize() } });
	gl_->framebuffers.ClearToColor(FramebufferId{ screen_target_.resource_ }, background_color_);
}

void Renderer::DrawPacketImmediate(
	const RenderPacket& packet, const RenderState& state,
	std::span<const ResolvedTextureBinding> texture_bindings
) {
	PTGN_ASSERT(packet.pipeline, "Immediate packet must have a pipeline");
	PTGN_ASSERT(packet.material.shader, "Immediate packet must have a shader");

	ApplyMaterial(packet.material);
	ApplyRenderStateToBackend(state);

	pipeline_manager_.SetCurrentPipeline(packet.pipeline);
	const auto& pipeline{ pipeline_manager_.GetCurrentPipeline() };

	PTGN_ASSERT(
		pipeline.vertex_size == packet.vertex_size,
		"Submitted packet vertex size does not match pipeline vertex size"
	);

	PTGN_ASSERT(
		packet.vertices.size() <=
			static_cast<std::size_t>(pipeline.vertex_capacity) * pipeline.vertex_size,
		"Immediate packet exceeds pipeline vertex capacity"
	);

	PTGN_ASSERT(
		packet.indices.size() <= pipeline.index_capacity,
		"Immediate packet exceeds pipeline index capacity"
	);

	ApplyTextureBindings(packet.material.shader, pipeline, texture_bindings);

	UploadVertices(pipeline, packet.vertices);
	UploadIndices(pipeline, packet.indices);

	DrawElements(pipeline, static_cast<std::uint32_t>(packet.indices.size()));
}

void Renderer::ExecuteFullscreenNode(const RenderGraph& graph, const RenderNode& node) {
	FlushBatch();

	PTGN_ASSERT(node.output.has_value(), "Fullscreen node must have an output target");
	PTGN_ASSERT(node.pipeline, "Fullscreen node must have a pipeline");
	PTGN_ASSERT(node.material.shader, "Fullscreen node must have a shader");

	auto target{ GetResourceTarget(graph, *node.output) };
	const auto& resource{ graph.Resource(TextureNode{ node.output->id }) };

	const auto& bound{ gl_->GetBoundState() };

	RenderState base{ .framebuffer{ target },
					  .viewport{ Viewport{ .position{}, .size = resource.size } },
					  .view_projection{ Matrix4::Orthographic(
						  V2_float{ -static_cast<float>(resource.size.x) * 0.5f,
									-static_cast<float>(resource.size.y) * 0.5f },
						  V2_float{ static_cast<float>(resource.size.x) * 0.5f,
									static_cast<float>(resource.size.y) * 0.5f }
					  ) },
					  .blend_mode{ node.state.blend_mode },
					  .depth_testing{ false },
					  .depth_mask{ bound.depth_mask },
					  .stencil{ bound.stencil },
					  .raster{ bound.raster },
					  .scissor{ node.state.scissor },
					  .color_mask{ bound.color_mask } };

	auto geometry{ MakeFullscreenTextureGeometry(resource.size, false) };

	auto packet{ MakeTexturedRenderPacket<TextureVertex>(
		node.pipeline, node.material, geometry.vertices, geometry.indices, geometry.primitives
	) };

	auto texture_bindings{ ResolveTextureBindings(graph, node.texture_bindings) };

	DrawPacketImmediate(packet, base, texture_bindings);
}

void Renderer::ExecuteClearNode(const RenderGraph& graph, const RenderNode& node) {
	PTGN_ASSERT(node.output.has_value(), "Render node must have output when executing clear");
	FlushBatch();
	auto target{ GetResourceTarget(graph, *node.output) };
	// TODO: Check if viewport should be set here?
	ClearRenderTarget(target, node.clear_color.value_or(color::Transparent), true);
}

IndexedPrimitiveGeometry<TextureVertex> Renderer::MakeFullscreenTextureGeometry(
	V2_int size, bool flip_y
) const {
	IndexedPrimitiveGeometry<TextureVertex> result;

	result.vertices.reserve(4);
	result.indices.reserve(6);
	result.primitives.reserve(1);

	auto half_width{ static_cast<float>(size.x) * 0.5f };
	auto half_height{ static_cast<float>(size.y) * 0.5f };

	auto depth{ 0.0f };
	auto color{ color::White };
	auto tex_index_placeholder{ 0.0f };
	auto entity_id{ -1 };

	auto tex_coords{ GetDefaultTextureCoordinates(flip_y) };

	auto color_n{ color.Normalized() };

	result.vertices.emplace_back(
		V2_float{ -half_width, -half_height }, depth, color_n, tex_coords[0], tex_index_placeholder,
		entity_id
	);

	result.vertices.emplace_back(
		V2_float{ half_width, -half_height }, depth, color_n, tex_coords[1], tex_index_placeholder,
		entity_id
	);

	result.vertices.emplace_back(
		V2_float{ half_width, half_height }, depth, color_n, tex_coords[2], tex_index_placeholder,
		entity_id
	);

	result.vertices.emplace_back(
		V2_float{ -half_width, half_height }, depth, color_n, tex_coords[3], tex_index_placeholder,
		entity_id
	);

	result.indices = { 0, 1, 2, 2, 3, 0 };

	result.primitives.push_back(PrimitiveRange{ .first_vertex = 0,
												.vertex_count = 4,
												.first_index  = 0,
												.index_count  = 6,
												.texture	  = std::nullopt });

	return result;
}

TextureId Renderer::GetResourceTexture(const RenderGraph& graph, TextureNode texture) const {
	return GetRenderTargetTexture(graph.Resource(texture).GetTarget());
}

TextureId Renderer::ResolveTextureSource(const RenderGraph& graph, const TextureSource& source)
	const {
	return std::visit(
		[&]<typename T>(const T& value) -> TextureId {
			if constexpr (std::same_as<T, TextureId>) {
				return value;
			} else {
				return GetResourceTexture(graph, value);
			}
		},
		source
	);
}

std::vector<ResolvedTextureBinding> Renderer::ResolveTextureBindings(
	const RenderGraph& graph, std::span<const TextureBinding> bindings
) const {
	std::vector<ResolvedTextureBinding> resolved;
	resolved.reserve(bindings.size());

	std::uint32_t next_texture_unit{ 0 };
	std::uint32_t next_array_index{ 0 };

	auto max_texture_units{ GetMaxTextureSlots() };

	for (const auto& binding : bindings) {
		PTGN_ASSERT(
			next_texture_unit < max_texture_units,
			"Pass uses more texture samplers than the backend supports"
		);

		std::uint32_t array_index{ 0 };
		if (binding.kind == TextureBindingKind::BatchSamplerArray) {
			array_index = next_array_index++;
		}

		resolved.push_back(ResolvedTextureBinding{ .kind = binding.kind,
												   .texture =
													   ResolveTextureSource(graph, binding.source),
												   .uniform_name = binding.uniform_name,
												   .texture_unit = next_texture_unit,
												   .array_index	 = array_index });

		++next_texture_unit;
	}

	return resolved;
}

std::uint32_t Renderer::GetBatchTextureCount() const {
	std::uint32_t count{ 0 };

	for (const auto& binding : batch_.texture_bindings) {
		if (binding.kind == TextureBindingKind::BatchSamplerArray) {
			++count;
		}
	}

	return count;
}

bool Renderer::HasBatchTexture(TextureId texture) const {
	return std::ranges::any_of(batch_.texture_bindings, [&](const auto& binding) {
		return binding.kind == TextureBindingKind::BatchSamplerArray && binding.texture == texture;
	});
}

std::uint32_t Renderer::GetCurrentVertexSize() const {
	PTGN_ASSERT(batch_.pipeline, "Cannot get vertex size without a current batch pipeline");
	return pipeline_manager_.GetPipeline(batch_.pipeline).vertex_size;
}

std::uint32_t Renderer::GetCurrentVertexCount() const {
	const auto& pipeline{ pipeline_manager_.GetPipeline(batch_.pipeline) };

	PTGN_ASSERT(pipeline.vertex_size > 0);
	PTGN_ASSERT(batch_.vertices.size() % pipeline.vertex_size == 0);

	return static_cast<std::uint32_t>(batch_.vertices.size() / pipeline.vertex_size);
}

void Renderer::AppendPrimitiveToBatch(const RenderPacket& packet, const PrimitiveRange& primitive) {
	auto vertex_size{ packet.vertex_size };

	PTGN_ASSERT(vertex_size > 0);
	PTGN_ASSERT(GetCurrentVertexSize() == vertex_size);

	PTGN_ASSERT(
		primitive.first_vertex + primitive.vertex_count <=
			static_cast<std::uint32_t>(packet.vertices.size() / vertex_size),
		"Primitive vertex range is out of bounds"
	);

	PTGN_ASSERT(
		primitive.first_index + primitive.index_count <= packet.indices.size(),
		"Primitive index range is out of bounds"
	);

	auto source_vertex_byte_offset{ static_cast<std::size_t>(primitive.first_vertex) *
									vertex_size };

	auto source_vertex_byte_count{ static_cast<std::size_t>(primitive.vertex_count) * vertex_size };

	auto old_batch_vertex_byte_size{ batch_.vertices.size() };

	batch_.vertices.insert(
		batch_.vertices.end(), packet.vertices.begin() + source_vertex_byte_offset,
		packet.vertices.begin() + source_vertex_byte_offset + source_vertex_byte_count
	);

	if (primitive.texture.has_value()) {
		PTGN_ASSERT(
			packet.texture_index_offset.has_value(),
			"Textured primitive submitted without texture-index offset"
		);

		auto texture_index{ static_cast<float>(AddBatchTexture(*primitive.texture)) };

		auto texture_index_offset{ *packet.texture_index_offset };

		PTGN_ASSERT(
			texture_index_offset + sizeof(float) <= vertex_size,
			"Texture index offset is outside the vertex"
		);

		for (std::uint32_t i{ 0 }; i < primitive.vertex_count; ++i) {
			auto destination_byte_offset{ old_batch_vertex_byte_size +
										  static_cast<std::size_t>(i) * vertex_size +
										  texture_index_offset };

			std::memcpy(
				batch_.vertices.data() + destination_byte_offset, &texture_index, sizeof(float)
			);
		}
	}

	auto base_vertex{ static_cast<std::uint32_t>(old_batch_vertex_byte_size / vertex_size) };

	for (std::uint32_t i{ 0 }; i < primitive.index_count; ++i) {
		auto source_index{ static_cast<std::uint32_t>(packet.indices[primitive.first_index + i]) };

		PTGN_ASSERT(
			source_index >= primitive.first_vertex &&
				source_index < primitive.first_vertex + primitive.vertex_count,
			"Primitive index refers to a vertex outside its primitive range"
		);

		auto local_index{ source_index - primitive.first_vertex };
		auto batch_index{ base_vertex + local_index };

		PTGN_ASSERT(
			batch_index <= static_cast<std::uint32_t>(std::numeric_limits<Index>::max()),
			"Batch index exceeds Index storage type"
		);

		batch_.indices.emplace_back(batch_index);
	}
}

void Renderer::EnsureBatchCanFit(
	PipelineId pipeline, const MaterialState& material, const RenderState& state,
	std::uint32_t vertex_size, bool has_texture_index_offset,
	const PrimitiveRequirements& requirements
) {
	PTGN_ASSERT(pipeline, "Pipeline must be non-zero");

	auto key_changed{ batch_.pipeline != pipeline || batch_.material != material ||
					  batch_.state != state };

	if (key_changed) {
		FlushBatch();

		batch_.pipeline = pipeline;
		batch_.material = material;
		batch_.state	= state;
	}

	auto& pipeline_object{ pipeline_manager_.GetPipeline(batch_.pipeline) };

	PTGN_ASSERT(
		pipeline_object.vertex_size == vertex_size,
		"Submitted packet vertex size does not match pipeline vertex size"
	);

	PTGN_ASSERT(
		requirements.vertex_count <= pipeline_object.vertex_capacity,
		"Single primitive exceeds pipeline vertex batch capacity"
	);

	PTGN_ASSERT(
		requirements.index_count <= pipeline_object.index_capacity,
		"Single primitive exceeds pipeline index batch capacity"
	);

	if (requirements.texture.has_value()) {
		PTGN_ASSERT(
			has_texture_index_offset, "Textured primitive submitted without texture-index offset"
		);
	}

	if (!CanFitInCurrentBatch(requirements)) {
		FlushBatch();

		batch_.pipeline = pipeline;
		batch_.material = material;
		batch_.state	= state;
	}
}

bool Renderer::CanFitInCurrentBatch(const PrimitiveRequirements& requirements) const {
	const auto& pipeline{ pipeline_manager_.GetPipeline(batch_.pipeline) };

	if (GetCurrentVertexCount() + requirements.vertex_count > pipeline.vertex_capacity) {
		return false;
	}

	if (batch_.indices.size() + requirements.index_count > pipeline.index_capacity) {
		return false;
	}

	if (requirements.texture.has_value() && !HasBatchTexture(*requirements.texture)) {
		if (GetBatchTextureCount() + 1 > GetMaxTextureSlots()) {
			return false;
		}
	}

	return true;
}

void Renderer::SubmitRenderPacket(const RenderPacket& packet, const RenderState& state) {
	for (auto primitive : packet.primitives) {
		auto requirements{ PrimitiveRequirements{ .vertex_count = primitive.vertex_count,
												  .index_count	= primitive.index_count,
												  .texture		= primitive.texture } };

		EnsureBatchCanFit(
			packet.pipeline, packet.material, state, packet.vertex_size,
			packet.texture_index_offset.has_value(), requirements
		);

		AppendPrimitiveToBatch(packet, primitive);
	}
}

RenderTargetId Renderer::GetResourceTarget(const RenderGraph& graph, TargetNode target) const {
	return graph.Resource(TextureNode{ target.id }).GetTarget();
}

std::uint32_t Renderer::AddBatchTexture(TextureId texture) {
	auto it{ std::ranges::find_if(batch_.texture_bindings, [&](const auto& binding) {
		return binding.kind == TextureBindingKind::BatchSamplerArray && binding.texture == texture;
	}) };

	if (it != batch_.texture_bindings.end()) {
		return it->array_index;
	}

	auto batch_texture_count{ GetBatchTextureCount() };

	PTGN_ASSERT(batch_texture_count < GetMaxTextureSlots(), "Exceeded maximum batch texture count");

	auto array_index{ batch_texture_count };
	auto texture_unit{ batch_texture_count };

	batch_.texture_bindings.push_back(ResolvedTextureBinding{
		.kind		  = TextureBindingKind::BatchSamplerArray,
		.texture	  = texture,
		.uniform_name = pipeline_manager_.GetPipeline(batch_.pipeline).batch_sampler_uniform,
		.texture_unit = texture_unit,
		.array_index  = array_index });

	return array_index;
}

RenderTargetId Renderer::AcquirePooledTarget(V2_int size, TextureFormat format) {
	++pool_tick_;

	for (auto& target : rt_pool_) {
		if (target.in_use) {
			continue;
		}
		if (target.size == size && target.format == format) {
			target.in_use		  = true;
			target.last_used_tick = pool_tick_;
			return target.target;
		}
	}

	RenderTargetObject created{ CreateRenderTarget(size, format) };

	RenderTargetId render_target_id{ created };

	rt_pool_.emplace_back(PooledTarget{ .target			= std::move(created),
										.size			= size,
										.format			= format,
										.last_used_tick = pool_tick_,
										.in_use			= true });
	return render_target_id;
}

void Renderer::ReleasePooledTarget(RenderTargetId render_target) {
	++pool_tick_;

	for (auto& e : rt_pool_) {
		if (e.target.operator RenderTargetId() == render_target) {
			e.in_use		 = false;
			e.last_used_tick = pool_tick_;
			TrimRenderTargetPool();
			return;
		}
	}

	PTGN_ERROR("Attempted to release a render target that is not in the pool");
}

void Renderer::TrimRenderTargetPool() {
	while (rt_pool_.size() > max_pool_size_) {
		auto victim = std::ranges::min_element(rt_pool_, {}, [](const PooledTarget& e) {
			return e.in_use ? std::numeric_limits<std::uint64_t>::max() : e.last_used_tick;
		});

		if (victim == rt_pool_.end() || victim->in_use) {
			return;
		}

		rt_pool_.erase(victim);
	}
}

void Renderer::EndFrame() {
	PTGN_ASSERT(display_viewport_.size.BothAboveZero());

	FlushBatch();

	SetFramebuffer({});

	if (presentation_viewport_.has_value()) {
		return;
	}

	V2_float half_viewport{ display_viewport_.size * 0.5f };
	auto view_projection{ Matrix4::Orthographic(-half_viewport, half_viewport) };

	PTGN_ASSERT(
		GetRenderTargetSize(screen_target_.resource_) == display_viewport_.size,
		"Screen target texture size must match display viewport size"
	);

	auto texture_shader{ GetShader("texture") };
	auto screen_texture{ GetRenderTargetTexture(screen_target_.resource_) };

	MaterialState material;
	material.shader = texture_shader;

	auto geometry{ MakeFullscreenTextureGeometry(display_viewport_.size, true) };

	auto packet{ MakeTexturedRenderPacket<TextureVertex>(
		Hash("texture"), material, geometry.vertices, geometry.indices, geometry.primitives
	) };

	RenderState state{ .framebuffer		= FramebufferId{},
					   .viewport		= display_viewport_,
					   .view_projection = view_projection,
					   .blend_mode		= BlendMode::ReplaceRGBA,
					   .depth_testing	= false,
					   .scissor			= ScissorState{ false },
					   .color_mask		= ColorMaskState{} };

	std::array texture_bindings{ ResolvedTextureBinding{ .kind =
															 TextureBindingKind::BatchSamplerArray,
														 .texture	   = screen_texture,
														 .uniform_name = "u_Textures",
														 .texture_unit = 0,
														 .array_index  = 0 } };

	DrawPacketImmediate(packet, state, texture_bindings);
}

bool Renderer::IsPresentationViewportVisible() const {
	return presentation_viewport_.has_value() && !presentation_viewport_->size.BothAboveZero();
}

ShaderObject Renderer::CreateShader(
	const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
) {
	return ShaderObject{ this, gl_->shaders.CreateProgram(source, shader_name) };
}

TextureObject Renderer::CreateTexture(
	const Surface& surface, TextureFormat format, TextureParameters params
) {
	PTGN_ASSERT(
		surface.GetChannelCount() == GetChannelCount(format),
		"Surface and texture format channel count must match"
	);
	return CreateTexture(surface.Data(), surface.GetSize(), format, params);
}

TextureObject Renderer::CreateTexture(
	const std::uint8_t* pixel_data, V2_int size, TextureFormat format, TextureParameters params
) {
	auto [pixel_format, pixel_type] = gl::GetPixelDataFormat(format);
	PTGN_ASSERT(
		pixel_type == gl::PixelDataType::UnsignedByte, "Texture format must have a type of bytes"
	);
	return TextureObject{ this, gl_->textures.CreateTexture(
									pixel_data, pixel_format, pixel_type, size, format, params
								) };
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, const Matrix4& v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V2_float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V3_float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V4_float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, std::span<const float> v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V2_int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V3_int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V4_int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, std::span<const int> v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, bool v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::Destroy(VertexBufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(ElementBufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(UniformBufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(ShaderId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(TextureId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(RenderbufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(FramebufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(VertexArrayId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(RenderTargetId id) {
	gl_->Destroy(id);
}

void Renderer::SetUniformValue(ShaderId id, const char* uniform_name, const UniformValue& v) {
	std::visit([&]<typename T>(T&& s) { SetUniform(id, uniform_name, std::forward<T>(s)); }, v);
};

V2_int Renderer::GetTextureSize(TextureId texture) const {
	return gl_->textures.GetTextureSize(texture);
}

TextureFormat Renderer::GetTextureFormat(TextureId texture) const {
	return gl_->textures.GetTextureFormat(texture);
}

void Renderer::ResizeRenderTarget(RenderTargetId render_target, V2_int new_size) {
	gl_->framebuffers.ResizeFramebuffer(FramebufferId{ render_target }, new_size);
}

std::size_t Renderer::GetMaxTextureSlots() const {
	return gl_->GetMaxTextureSlots();
}

void Renderer::Compile(RenderGraph& graph) {
	auto lifetimes = ComputeResourceLifetimes(graph);

	std::vector<RenderResourceId> transient_resources;

	const auto& resources{ graph.Resources() };

	transient_resources.reserve(resources.size());

	// This skips imported resources like the real scene target or screen target.
	// It also skips unused temporary resources.
	for (RenderResourceId id{ 0 }; id < resources.size(); ++id) {
		if (const auto& resource{ resources[id] }; !resource.IsTransient()) {
			continue;
		}
		if (!lifetimes[id].used) {
			continue;
		}

		transient_resources.push_back(id);
	}

	// This makes allocation happen in graph execution order. This matters because the compiler will
	// reuse targets as soon as earlier resources are no longer needed.
	std::ranges::sort(transient_resources, [&](RenderResourceId a, RenderResourceId b) {
		return lifetimes[a].first_use < lifetimes[b].first_use;
	});

	// Physical render targets that are no longer used by any active logical resource and can be
	// reused.
	std::vector<PhysicalTransient> free_targets;
	// Physical render targets currently assigned to logical resources that are still needed by
	// future nodes.
	std::vector<LiveTransient> live_targets;

	// This function checks each live target.
	// If its last_use is before the current node, the logical resource no longer needs that
	// physical target. So it moves that physical target into free_targets.
	auto expire_targets_before = [&](std::size_t node_index) {
		std::erase_if(live_targets, [&](const LiveTransient& live) {
			// This means a target cannot be reused on the exact same node where it is last used.
			// If a node reads Bright and writes BlurX, Bright must stay valid for that whole node.
			// So its target cannot be reused by BlurX in the same node. Only after that node
			// finishes can Bright’s target be reused.
			if (live.last_use < node_index) {
				free_targets.push_back(live.physical);
				return true;
			}
			return false;
		});
	};

	// This tries to find a render target that is:
	// same size, same texture format, currently free
	// If it finds one, reuse it.
	// If not, ask the renderer pool for a new physical render target.
	auto acquire_physical = [&](const RenderResource& resource) {
		if (auto compatible{ std::ranges::find_if(
				free_targets, [&](const PhysicalTransient& p
							  ) { return p.size == resource.size && p.format == resource.format; }
			) };
			compatible != free_targets.end()) {
			PhysicalTransient result = *compatible;
			free_targets.erase(compatible);
			return result;
		}

		return PhysicalTransient{ .target = AcquirePooledTarget(resource.size, resource.format),
								  .size	  = resource.size,
								  .format = resource.format };
	};

	// For each logical transient resource:
	// Free any targets whose resources are no longer needed.
	// Reuse a free compatible target, or allocate one from the pool.
	// Store that physical target inside the logical resource.
	// Mark it live until its last_use.
	for (RenderResourceId id : transient_resources) {
		auto& resource		 = graph.Resources()[id];
		const auto& lifetime = lifetimes[id];

		expire_targets_before(lifetime.first_use);

		PhysicalTransient physical = acquire_physical(resource);
		resource.AssignTransientTarget(physical.target);

		live_targets.push_back(LiveTransient{
			.resource = id, .physical = physical, .last_use = lifetime.last_use });
	}
}

std::vector<ResourceLifetime> Renderer::ComputeResourceLifetimes(const RenderGraph& graph) const {
	std::vector<ResourceLifetime> lifetimes(graph.Resources().size());

	// Every time a node reads or writes a resource, this updates the resource’s lifetime.
	auto mark_use = [&](RenderResourceId id, std::size_t node_index) {
		auto& lifetime	   = lifetimes[id];
		lifetime.used	   = true;
		lifetime.first_use = std::min(lifetime.first_use, node_index);
		lifetime.last_use  = std::max(lifetime.last_use, node_index);
	};

	const auto& nodes = graph.Nodes();

	// The graph nodes are assumed to be in execution order.
	// So node index also means time/order.
	for (std::size_t node_index = 0; node_index < nodes.size(); ++node_index) {
		const auto& node = nodes[node_index];

		// If a node samples a texture, that texture is in use at that node.
		for (const auto& binding : node.texture_bindings) {
			auto texture_node{ std::get_if<TextureNode>(&binding.source) };

			if (texture_node == nullptr) {
				continue;
			}

			mark_use(texture_node->id, node_index);
		}

		// If a node writes a target, that target is also used at that node.
		if (node.output.has_value()) {
			mark_use(node.output->id, node_index);
		}
	}

	return lifetimes;
}

void Renderer::ReleaseCompiledTransients(RenderGraph& graph) {
	std::vector<RenderTargetId> released;

	for (auto& resource : graph.Resources()) {
		if (!resource.IsTransient()) {
			continue;
		}

		auto& transient = std::get<TransientRenderTargetResource>(resource.storage);

		if (!transient.assigned_target.has_value()) {
			continue;
		}

		if (auto target{ *transient.assigned_target }; !std::ranges::contains(released, target)) {
			ReleasePooledTarget(target);
			released.push_back(target);
		}

		transient.assigned_target = std::nullopt;
	}
}

void Renderer::ExecuteNode(const RenderGraph& graph, const RenderNode& node) {
	switch (node.type) {
		case RenderNodeType::DrawLayer:		 ExecuteDrawLayerNode(graph, node); break;
		case RenderNodeType::FullscreenPass: ExecuteFullscreenNode(graph, node); break;
		case RenderNodeType::Clear:			 ExecuteClearNode(graph, node); break;
		case RenderNodeType::Present:		 ExecuteFullscreenNode(graph, node); break;
		default:							 PTGN_ERROR("Unsupported render node"); break;
	}
}

void Renderer::ExecuteDrawLayerNode(const RenderGraph& graph, const RenderNode& node) {
	PTGN_ASSERT(node.output.has_value(), "Render node must have output when executing draw layer");
	auto target{ GetResourceTarget(graph, *node.output) };
	const auto& resource{ graph.Resource(TextureNode{ node.output->id }) };

	const auto& bound{ gl_->GetBoundState() };

	auto viewport{ node.state.viewport.transform([&resource](auto& v) {
		if (v.size.IsZero()) {
			return Viewport{ {}, resource.size };
		}
		return v;
	}) };

	RenderState base{ .framebuffer{ target },
					  .viewport{ viewport },
					  .view_projection{ node.state.view_projection },
					  .blend_mode{ bound.blend_mode },
					  .depth_testing{ false },
					  .depth_mask{ bound.depth_mask },
					  .stencil{ bound.stencil },
					  .raster{ bound.raster },
					  .scissor{ bound.scissor },
					  .color_mask{ bound.color_mask } };

	for (const auto& packet : node.draw_packets) {
		auto state{ ApplyDelta(base, packet.state_delta) };
		SubmitRenderPacket(packet, state);
	}

	FlushBatch();
}

void Renderer::Execute(RenderGraph& graph) {
	Compile(graph);

	++graph_debug_frame_index_;
	last_graph_snapshot_ = BuildDebugSnapshot(graph);

	for (const auto& node : graph.Nodes()) {
		ExecuteNode(graph, node);
	}

	FlushBatch();
	ReleaseCompiledTransients(graph);
}

void Renderer::ApplyRenderStateToBackend(const RenderState& render_state) {
	if (render_state.framebuffer.has_value()) {
		auto _ = gl_->Bind(*render_state.framebuffer, false);
	}
	if (render_state.viewport.has_value()) {
		gl_->SetViewport(*render_state.viewport);
	}
	if (render_state.blend_mode.has_value()) {
		gl_->SetBlendMode(*render_state.blend_mode);
	}
	if (render_state.depth_testing.has_value()) {
		gl_->SetDepthTesting(*render_state.depth_testing);
	}
	if (render_state.depth_mask.has_value()) {
		gl_->SetDepthMask(*render_state.depth_mask);
	}
	if (render_state.view_projection.has_value()) {
		view_projection_ = *render_state.view_projection;
		if (auto shader{ gl_->GetBoundShader() }; shader.has_value() && *shader) {
			gl_->shaders.SetUniform(*shader, "u_ViewProjection", view_projection_);
		}
	}
	if (render_state.stencil.has_value()) {
		gl_->SetStencil(*render_state.stencil);
	}
	if (render_state.raster.has_value()) {
		gl_->SetRaster(*render_state.raster);
	}
	if (render_state.scissor.has_value()) {
		gl_->SetScissor(*render_state.scissor);
	}
	if (render_state.color_mask.has_value()) {
		gl_->SetColorMask(*render_state.color_mask);
	}
}

void Renderer::UploadVertices(const RenderPipeline& pipeline, std::span<const std::byte> vertices) {
	auto _0{ gl_->Bind(pipeline.vao, false) };
	auto _{ gl_->Bind(pipeline.vbo, false) };

	auto vertex_count{ static_cast<std::uint32_t>(vertices.size() / pipeline.vertex_size) };

	gl_->buffers.SetBufferSubData<VertexBufferId>(
		pipeline.vbo, gl::BufferTarget::ArrayBuffer, vertices.data(), 0, vertex_count,
		pipeline.vertex_size
	);
}

void Renderer::UploadIndices(const RenderPipeline& pipeline, std::span<const Index> indices) {
	auto _0{ gl_->Bind(pipeline.vao, false) };
	auto _{ gl_->Bind(pipeline.ebo, false) };

	gl_->buffers.SetBufferSubData<ElementBufferId>(
		pipeline.ebo, gl::BufferTarget::ElementArrayBuffer, indices.data(), 0,
		static_cast<std::uint32_t>(indices.size()), sizeof(Index)
	);
}

void Renderer::DrawElements(const RenderPipeline& pipeline, std::uint32_t index_count) {
	auto _0{ gl_->Bind(pipeline.vao, false) };

	gl_->vertex_arrays.DrawElements(
		pipeline.vao, index_count, gl::IndexType::UnsignedInt, pipeline.primitive_mode
	);
}

void Renderer::FlushBatch() {
	if (batch_.indices.empty()) {
		return;
	}

	PTGN_ASSERT(batch_.pipeline, "Batch must have a non-zero pipeline");

	ApplyMaterial(batch_.material);
	ApplyRenderStateToBackend(batch_.state);

	pipeline_manager_.SetCurrentPipeline(batch_.pipeline);
	const auto& pipeline{ pipeline_manager_.GetCurrentPipeline() };

	ApplyTextureBindings(batch_.material.shader, pipeline, batch_.texture_bindings);

	UploadVertices(pipeline, batch_.vertices);
	UploadIndices(pipeline, batch_.indices);

	DrawElements(pipeline, static_cast<std::uint32_t>(batch_.indices.size()));

	batch_.vertices.clear();
	batch_.indices.clear();
	batch_.texture_bindings.clear();
}

void Renderer::BindTextureUnit(TextureId texture, std::uint32_t texture_unit) {
	gl_->SetActiveTextureSlot(texture_unit);
	auto _3{ gl_->Bind(texture, false) };
}

void Renderer::ApplyTextureBindings(
	ShaderId shader, const RenderPipeline& pipeline,
	std::span<const ResolvedTextureBinding> bindings
) {
	std::vector<int> batch_texture_units;
	std::string batch_sampler_uniform{ pipeline.batch_sampler_uniform };

	auto max_texture_slots{ GetMaxTextureSlots() };

	for (const auto& binding : bindings) {
		PTGN_ASSERT(binding.texture != TextureId{}, "Cannot bind an empty texture");

		PTGN_ASSERT(
			binding.texture_unit < max_texture_slots, "Texture unit is outside backend limit"
		);

		BindTextureUnit(binding.texture, binding.texture_unit);

		if (binding.kind == TextureBindingKind::NamedSampler) {
			SetUniform(
				shader, binding.uniform_name.c_str(), static_cast<int>(binding.texture_unit)
			);
			continue;
		}

		if (binding.kind == TextureBindingKind::BatchSamplerArray) {
			if (!binding.uniform_name.empty()) {
				batch_sampler_uniform = binding.uniform_name;
			}

			if (batch_texture_units.size() <= binding.array_index) {
				batch_texture_units.resize(binding.array_index + 1);
			}

			batch_texture_units[binding.array_index] = static_cast<int>(binding.texture_unit);
		}
	}

	if (!batch_texture_units.empty()) {
		SetUniform(shader, batch_sampler_uniform.c_str(), batch_texture_units);
	}
}

void Renderer::ApplyMaterialUniforms(const MaterialState& material) {
	for (const auto& uniform : material.uniforms) {
		SetUniformValue(material.shader, uniform.name.c_str(), uniform.value);
	}
}

void Renderer::ApplyMaterial(const MaterialState& material) {
	auto _ = gl_->Bind(material.shader, false);
	gl_->shaders.SetUniform(material.shader, "u_ViewProjection", view_projection_);
	ApplyMaterialUniforms(material);
}

static std::optional<RenderResourceId> TryGetResourceId(const TextureSource& source) {
	return std::visit(
		[]<typename T>(const T& value) -> std::optional<RenderResourceId> {
			using U = std::remove_cvref_t<T>;

			if constexpr (std::is_same_v<U, TextureNode>) {
				return value.id;
			} else if constexpr (std::is_same_v<U, TargetNode>) {
				return value.id;
			} else {
				// Raw TextureId or other external texture source.
				return std::nullopt;
			}
		},
		source
	);
}

static std::string GetResourceDebugName(
	const DebugRenderGraphSnapshot& snapshot, RenderResourceId id
) {
	auto resource_it =
		std::ranges::find_if(snapshot.resources, [&](const auto& r) { return r.id == id; });

	if (resource_it != snapshot.resources.end()) {
		return resource_it->name;
	}

	return "resource " + std::to_string(id);
}

void Renderer::BuildDebugEdges(DebugRenderGraphSnapshot& snapshot, const RenderGraph& graph) const {
	const auto& nodes = graph.Nodes();

	for (std::size_t to_index = 0; to_index < nodes.size(); ++to_index) {
		const auto& to_node = nodes[to_index];

		for (const auto& binding : to_node.texture_bindings) {
			auto read_resource = TryGetResourceId(binding.source);

			// Raw TextureId/external texture bindings do not represent graph edges.
			if (!read_resource.has_value()) {
				continue;
			}

			std::optional<RenderNodeId> producer;

			for (std::size_t from_index = to_index; from_index > 0; --from_index) {
				const auto& candidate = nodes[from_index - 1];

				if (!candidate.output.has_value()) {
					continue;
				}

				if (candidate.output->id == *read_resource) {
					producer = candidate.id;
					break;
				}
			}

			if (!producer.has_value()) {
				continue;
			}

			snapshot.edges.push_back(DebugRenderGraphEdge{
				.from	  = *producer,
				.to		  = to_node.id,
				.resource = *read_resource,
				.label	  = GetResourceDebugName(snapshot, *read_resource) });
		}
	}
}

DebugRenderGraphSnapshot Renderer::BuildDebugSnapshot(const RenderGraph& graph) const {
	DebugRenderGraphSnapshot snapshot;
	snapshot.valid		 = true;
	snapshot.frame_index = graph_debug_frame_index_;

	auto lifetimes = ComputeResourceLifetimes(graph);

	snapshot.resources.reserve(graph.Resources().size());

	for (RenderResourceId id = 0; id < graph.Resources().size(); ++id) {
		const auto& resource = graph.Resources()[id];
		const auto& lifetime = lifetimes[id];

		DebugRenderResourceSnapshot out;
		out.id		  = id;
		out.name	  = resource.name;
		out.size	  = resource.size;
		out.format	  = resource.format;
		out.imported  = resource.IsImported();
		out.used	  = lifetime.used;
		out.first_use = lifetime.used ? lifetime.first_use : 0;
		out.last_use  = lifetime.used ? lifetime.last_use : 0;

		if (auto imported = std::get_if<ImportedRenderTargetResource>(&resource.storage)) {
			out.imported_target = imported->target;
			out.physical_target = imported->target;
		} else if (auto transient = std::get_if<TransientRenderTargetResource>(&resource.storage)) {
			out.physical_target = transient->assigned_target;
		}

		snapshot.resources.push_back(std::move(out));
	}

	snapshot.nodes.reserve(graph.Nodes().size());

	for (const auto& node : graph.Nodes()) {
		DebugRenderNodeSnapshot out;
		out.id	 = node.id;
		out.type = node.type;
		out.name = node.name;

		out.has_output = node.output.has_value();
		out.output	   = node.output.has_value() ? node.output->id : 0;

		out.pipeline = node.pipeline;
		out.shader	 = node.material.shader;

		out.uniform_count	= node.material.uniforms.size();
		out.draw_item_count = node.draw_packets.size();

		out.state = node.state;

		for (const auto& binding : node.texture_bindings) {
			auto read_resource = TryGetResourceId(binding.source);

			// The graph visualizer's edge/read display is graph-resource based.
			// Raw TextureId bindings are not graph resources, so skip them here.
			if (!read_resource.has_value()) {
				continue;
			}

			out.reads.push_back(DebugTextureReadSnapshot{ .resource		= *read_resource,
														  .uniform_name = binding.uniform_name });
		}

		snapshot.nodes.push_back(std::move(out));
	}

	BuildDebugEdges(snapshot, graph);

	return snapshot;
}

} // namespace ptgn::impl