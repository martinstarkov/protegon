#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/hash.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/render_batcher.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "renderer/vertex/vertex.h"

namespace ptgn {

class Application;
class DrawContext;
class Window;

namespace impl {

class ApplicationContext;
class Surface;
class RenderPipelineManager;

namespace gl {

class GLContext;

} // namespace gl

struct EffectParams {
	std::function<void(DrawContext&)> draw_callback;

	int margin{ 0 };
};

// struct DrawPlacement {
//	std::span<const RenderQuad<TextureVertex>> vertices;
//	std::array<V2_float, 4> final_quad;
//	std::array<V2_float, 4> final_tex_coords;
//	float depth;
//	Color tint;
//	int entity_id;
// };

class Renderer {
public:
	/*
	class RenderTargetTemp {
	public:
		RenderTargetTemp() = default;

		RenderTargetTemp(RenderTargetId id, TextureId texture, RenderTargetDesc desc) :
			id_{ id }, texture_{ texture }, desc_{ desc } {}

		RenderTargetId GetId() const {
			return id_;
		}

		TextureId GetTextureRef() const {
			return texture_;
		}

		V2_int GetSize() const {
			return desc_.size;
		}

		TextureFormat GetFormat() const {
			return desc_.format;
		}

		RenderTargetDesc GetDesc() const {
			return desc_;
		}

		bool IsValid() const {
			return id_ != 0 && texture_ != 0;
		}

	private:
		RenderTargetId id_{};
		TextureId texture_{};
		RenderTargetDesc desc_{};
	};

	struct RenderTargetHandle {
		RenderTargetTemp target{};
		bool valid{};

		RenderTargetHandle() = default;

		explicit RenderTargetHandle(RenderTargetTemp target_object) :
			target{ target_object }, valid{ target_object.IsValid() } {}

		RenderTargetHandle(const RenderTargetHandle&)			 = delete;
		RenderTargetHandle& operator=(const RenderTargetHandle&) = delete;

		RenderTargetHandle(RenderTargetHandle&& other) noexcept :
			target{ other.target }, valid{ other.valid } {
			other.target = {};
			other.valid	 = false;
		}

		RenderTargetHandle& operator=(RenderTargetHandle&& other) noexcept {
			if (this == &other) {
				return *this;
			}

			target = other.target;
			valid  = other.valid;

			other.target = {};
			other.valid	 = false;

			return *this;
		}

		explicit operator bool() const {
			return valid;
		}

		TextureId GetTextureRef() const {
			return target.GetTextureRef();
		}

		V2_int GetSize() const {
			return target.GetSize();
		}

		RenderTargetId GetId() const {
			return target.GetId();
		}

		RenderTargetDesc GetDesc() const {
			return target.GetDesc();
		}
	};

	struct ImageRef {
		TextureId texture{};
		V2_int size{};

		bool IsValid() const {
			return texture != 0 && size.BothAboveZero();
		}
	};

	struct ImageHandle {
		ImageRef image{};
		RenderTargetHandle transient{};

		bool OwnsTransient() const {
			return static_cast<bool>(transient);
		}
	};

	RenderTargetHandle AcquireTransient(RenderTargetDesc desc) {
		const auto& output{ target_pool_.Acquire(desc, FramebufferId{ 0 }) };
		auto o{ output.operator RenderTargetId() };
		return RenderTargetHandle{ RenderTargetTemp{ o,
													 GetRenderTargetTexture(o),
													 {
														 .size	 = GetRenderTargetSize(o),
														 .format = GetRenderTargetTextureFormat(o),
													 } } };
	}

	void ReleaseTransient(RenderTargetHandle handle) {
		target_pool_.Release(handle.GetId());
	}

	ImageRef CurrentEffectImage() const {
		return ActiveEffectChain().current.image;
	}

	ImageHandle EffectPass(
		std::span<const ImageRef> inputs, RenderTargetDesc output_desc, MaterialState material
	) {
		PTGN_ASSERT(!inputs.empty(), "Effect pass requires at least one input image");
		PTGN_ASSERT(output_desc.size.BothAboveZero(), "Effect pass output size must be valid");

		auto output = AcquireTransient(output_desc);

		FlushBatch();
		RenderFullscreenPass(inputs, output.target, material);

		// TODO: Fix.
		// WithState(material.state_delta, [&]() {
		//	FlushBatch();
		//	// Immediate fullscreen pass.
		//	// This is where your Pass().Read(...).Output(...).Draw(...) implementation goes.
		//	RenderFullscreenPass(inputs, output.target, material);
		//});

		return ImageHandle{
			.image =
				ImageRef{
					.texture = output.GetTextureRef(),
					.size	 = output_desc.size,
				},
			.transient = std::move(output),
		};
	}

	RenderTargetTemp BoundTarget() const {
		auto bound{ GetCurrentTarget() };
		return RenderTargetTemp{ bound, GetRenderTargetTexture(bound),
								 RenderTargetDesc{
									 .size	 = GetRenderTargetSize(bound),
									 .format = GetRenderTargetTextureFormat(bound),
								 } };
	}

	V2_int BoundTargetSize() const {
		auto bound{ GetCurrentTarget() };
		return GetRenderTargetSize(bound);
	}

	TextureId BoundTargetTexture() const {
		auto bound{ GetCurrentTarget() };
		return GetRenderTargetTexture(bound);
	}

	ImageRef EffectScratchPass(
		std::span<const ImageRef> inputs, RenderTargetDesc output_desc, MaterialState material
	) {
		auto image = EffectPass(inputs, output_desc, material);
		auto ref   = image.image;

		ActiveEffectChain().scratch.push_back(std::move(image));

		return ref;
	}

	void ReplaceCurrentEffectImage(ImageHandle image) {
		auto& chain = ActiveEffectChain();

		if (chain.current.transient) {
			ReleaseTransient(std::move(chain.current.transient));
		}

		for (auto& scratch : chain.scratch) {
			if (scratch.transient) {
				ReleaseTransient(std::move(scratch.transient));
			}
		}

		chain.scratch.clear();
		chain.current = std::move(image);
	}

	void EffectApplyFullscreenPass(std::string_view shader_name) {
		auto input = CurrentEffectImage();

		auto output = EffectPass(
			std::array{ input },
			RenderTargetDesc{
				.size	= input.size,
				.format = TextureFormat::RGBA8,
			},
			MaterialState{
				.shader = GetShader(shader_name),
			}
		);

		ReplaceCurrentEffectImage(std::move(output));
	}

	void EffectApplyFullscreenPass(std::string_view shader_name, TextureFormat format) {
		auto input = CurrentEffectImage();

		auto output = EffectPass(
			std::array{ input },
			RenderTargetDesc{
				.size	= input.size,
				.format = format,
			},
			MaterialState{
				.shader = GetShader(shader_name),
			}
		);

		ReplaceCurrentEffectImage(std::move(output));
	}

	std::vector<RenderTargetHandle> retained_transients{};

	struct EffectChainState {
		ImageHandle current{};
		std::vector<ImageHandle> scratch{};
	};

	struct PreparedEffectSeed {
		ImageHandle seed{};
		DrawPlacement placement{};
	};

	// --------------------------------------------------------
	// Top-level draw paths
	// --------------------------------------------------------

	void DrawFullscreenTextureEffects(
		std::span<const RenderQuad<TextureVertex>> quads, std::span<const TextureId> local_textures,
		const EffectParams& effects, std::span<const TextureBinding> extra_textures
	) {
		FlushBatch();

		auto seed = ImageHandle{
			.image =
				ImageRef{
					.texture = BoundTargetTexture(),
					.size	 = BoundTargetSize(),
				},
			.transient = {},
		};

		BeginEffectChain(std::move(seed));

		DrawContext ctx{ *this };

		if (effects.draw_callback) {
			effects.draw_callback(ctx);
		} else {
			auto input = ctx.CurrentImage();

			auto output = ctx.Pass(
				std::array{ input },
				RenderTargetDesc{
					.size	= input.size,
					.format = TextureFormat::RGBA8,
				},
				request.material
			);

			ctx.ReplaceCurrent(std::move(output));
		}

		auto final_image = EndEffectChain();

		ReplaceBoundTarget(std::move(final_image));
	}

	void DrawTextureGeometryEffects(
		std::span<const RenderQuad<TextureVertex>> quads, std::span<const TextureId> local_textures,
		const EffectParams& effects, std::span<const TextureBinding> extra_textures
	) {
		auto prepared = PrepareTextureEffectSeed(quads, local_textures, effects, extra_textures);

		BeginEffectChain(std::move(prepared.seed));

		DrawContext ctx{ *this };

		PTGN_ASSERT(effects.draw_callback, "Geometry effects require a draw callback");

		effects.draw_callback(ctx);

		auto final_image = EndEffectChain();

		BatchEffectResult(std::move(final_image), prepared.placement);
	}

	void DrawTexturesNormally(
		std::span<const RenderQuad<TextureVertex>> quads, std::span<const TextureId> local_textures,
		const EffectParams& effects, std::span<const TextureBinding> extra_textures
	) {
		// TODO: Fix.
		// WithState(request.material.state_delta, [&]() { AddVerticesToBatch(request.vertices); });
	}

	// --------------------------------------------------------
	// Bound-target detection
	// --------------------------------------------------------

	bool ReferencesBoundTarget(
		std::span<const RenderQuad<TextureVertex>> quads, std::span<const TextureId> local_textures,
		const EffectParams& effects, std::span<const TextureBinding> extra_textures
	) const {
		auto bound_texture = BoundTargetTexture();

		if (bound_texture == 0) {
			return false;
		}

		if (std::ranges::contains(local_textures, bound_texture)) {
			return true;
		}

		if (std::ranges::any_of(extra_textures, [bound_texture](const auto& entry) {
				return entry.source == bound_texture;
			})) {
			return true;
		}

		return false;
	}

	bool IsFullscreenCompatible(
		std::span<const RenderQuad<TextureVertex>> quads, std::span<const TextureId> local_textures,
		const EffectParams& effects, std::span<const TextureBinding> extra_textures
	) const {
		if (quads.empty()) {
			return true;
		}

		return IsFullscreenQuad(quads);
	}

	bool IsFullscreenQuad(std::span<const RenderQuad<TextureVertex>> quads) const {
		if (quads.size() != 1) {
			return false;
		}

		auto bounds		 = ComputeVertexBounds(quads.front()).bounds;
		auto target_size = BoundTargetSize();

		auto expected = Rect{
			V2_float{ 0.0f, 0.0f },
			V2_float{
				static_cast<float>(target_size.x),
				static_cast<float>(target_size.y),
			},
		};

		auto epsilon = 0.5f;

		return std::abs(bounds.GetMin().x - expected.GetMin().x) <= epsilon &&
			   std::abs(bounds.GetMin().y - expected.GetMin().y) <= epsilon &&
			   std::abs(bounds.GetMax().x - expected.GetMax().x) <= epsilon &&
			   std::abs(bounds.GetMax().y - expected.GetMax().y) <= epsilon;
	}

	// --------------------------------------------------------
	// Effect seed preparation
	// --------------------------------------------------------

	struct BoundsInfo {
		Rect bounds{};
		Rect expanded_bounds{};
		V2_int target_size{};
	};

	V2_int CeilToInt(V2_float value) const {
		return {
			std::max(1, static_cast<int>(std::ceil(value.x))),
			std::max(1, static_cast<int>(std::ceil(value.y))),
		};
	}

	BoundsInfo ComputeVertexBounds(
		std::span<const RenderQuad<TextureVertex>> vertices, float margin_px = 0.0f
	) const {
		PTGN_ASSERT(!vertices.empty(), "Cannot compute bounds for empty vertices");

		std::vector<V2_float> points{};
		points.reserve(vertices.size());

		for (auto& quad : vertices) {
			for (auto& vertex : quad) {
				points.emplace_back(vertex.position[0], vertex.position[1]);
			}
		}

		auto bounds	  = Rect::FromPoints(points);
		auto expanded = bounds.Expanded(V2_float{ std::max(0.0f, margin_px) });
		auto size	  = CeilToInt(expanded.GetSize());

		return {
			.bounds			 = bounds,
			.expanded_bounds = expanded,
			.target_size	 = size,
		};
	}

	BoundsInfo ComputeVertexBounds(std::span<const TextureVertex> vertices, float margin_px = 0.0f)
		const {
		PTGN_ASSERT(!vertices.empty(), "Cannot compute bounds for empty vertices");

		std::vector<V2_float> points{};
		points.reserve(vertices.size());

		for (auto& vertex : vertices) {
			points.emplace_back(vertex.position[0], vertex.position[1]);
		}

		auto bounds	  = Rect::FromPoints(points);
		auto expanded = bounds.Expanded(V2_float{ std::max(0.0f, margin_px) });
		auto size	  = CeilToInt(expanded.GetSize());

		return {
			.bounds			 = bounds,
			.expanded_bounds = expanded,
			.target_size	 = size,
		};
	}

	PreparedEffectSeed PrepareTextureEffectSeed(
		std::span<const RenderQuad<TextureVertex>> quads, std::span<const TextureId> local_textures,
		const EffectParams& effects, std::span<const TextureBinding> extra_textures
	) {
		if (CanUseTextureDirectlyAsEffectSeed(quads, local_textures, effects, extra_textures)) {
			return PrepareDirectTextureSeed(quads, local_textures, effects, extra_textures);
		}

		return RasterizeGeometryToEffectSeed(quads, local_textures, effects, extra_textures);
	}

	bool CanUseTextureDirectlyAsEffectSeed(
		std::span<const RenderQuad<TextureVertex>> quads, std::span<const TextureId> local_textures,
		const EffectParams& effects, std::span<const TextureBinding> extra_textures
	) const {
		if (effects.margin > 0.0f) {
			return false;
		}

		if (quads.size() != 1) {
			return false;
		}

		if (!extra_textures.empty()) {
			return false;
		}

		if (!UsesWholeTextureQuad(quads.front())) {
			return false;
		}

		return true;
	}

	bool UsesWholeTextureQuad(std::span<const TextureVertex> vertices) const {
		if (vertices.size() != 4) {
			return false;
		}

		auto min_uv = V2_float{
			vertices.front().tex_coord[0],
			vertices.front().tex_coord[1],
		};

		auto max_uv = min_uv;

		for (auto& vertex : vertices) {
			min_uv.x = std::min(min_uv.x, vertex.tex_coord[0]);
			min_uv.y = std::min(min_uv.y, vertex.tex_coord[1]);
			max_uv.x = std::max(max_uv.x, vertex.tex_coord[0]);
			max_uv.y = std::max(max_uv.y, vertex.tex_coord[1]);
		}

		auto epsilon = 0.0001f;

		return std::abs(min_uv.x - 0.0f) <= epsilon && std::abs(min_uv.y - 0.0f) <= epsilon &&
			   std::abs(max_uv.x - 1.0f) <= epsilon && std::abs(max_uv.y - 1.0f) <= epsilon;
	}

	PreparedEffectSeed PrepareDirectTextureSeed(
		std::span<const RenderQuad<TextureVertex>> quads, std::span<const TextureId> local_textures,
		const EffectParams& effects, std::span<const TextureBinding> extra_textures
	) {
		PTGN_ASSERT(local_textures.size() == 1);
		auto texture_size = GetTextureSize(local_textures.front());
		auto bounds		  = ComputeVertexBounds(quads);

		return {
			.seed =
				ImageHandle{
					.image =
						ImageRef{
							.texture = local_textures.front(),
							.size	 = texture_size,
						},
					.transient = {},
				},
			.placement =
				DrawPlacement{
					.vertices		  = quads,
					.final_quad		  = MakeQuadPositions(bounds.bounds),
					.final_tex_coords = FullQuadTexCoords(),
					.depth			  = quads.front().at(0).position[2],
					// TODO: Fix.
					.tint	   = color::White,
					.entity_id = -1,
				},
		};
	}

	PreparedEffectSeed RasterizeGeometryToEffectSeed(
		std::span<const RenderQuad<TextureVertex>> quads, std::span<const TextureId> local_textures,
		const EffectParams& effects, std::span<const TextureBinding> extra_textures
	) {
		auto bounds = ComputeVertexBounds(request.vertices, request.effect_margin_px);

		auto desc = RenderTargetDesc{
			.size	= bounds.target_size,
			.format = TextureFormat::RGBA8,
		};

		auto target = AcquireTransient(desc);

		auto local_vertices = BuildLocalVertices(request.vertices, bounds.expanded_bounds.GetMin());

		FlushBatch();

		// TODO: Fix.
		// WithState(request.material.state_delta, [&]() {
		RenderGeometryToTarget(
			local_vertices, request.texture, request.extra_textures, target.target,
			request.material, color::Transparent
		);
		//});

		return {
			.seed =
				ImageHandle{
					.image =
						ImageRef{
							.texture = target.GetTextureRef(),
							.size	 = bounds.target_size,
						},
					.transient = std::move(target),
				},
			.placement =
				DrawPlacement{
					.positions	= MakeQuadPositions(bounds.expanded_bounds),
					.tex_coords = FullQuadTexCoords(),
					.depth		= request.vertices.front().depth,
					.tint		= request.tint,
					.entity_id	= request.entity_id,
				},
		};
	}

	std::vector<TextureVertex> BuildLocalVertices(
		std::span<const TextureVertex> vertices, V2_float origin
	) const {
		std::vector<TextureVertex> result{};
		result.reserve(vertices.size());

		for (auto vertex : vertices) {
			vertex.position = vertex.position - origin;
			result.push_back(vertex);
		}

		return result;
	}

	std::array<V2_float, 4> MakeQuadPositions(Rect rect) const {
		return {
			V2_float{ rect.GetMin().x, rect.GetMin().y },
			V2_float{ rect.GetMax().x, rect.GetMin().y },
			V2_float{ rect.GetMax().x, rect.GetMax().y },
			V2_float{ rect.GetMin().x, rect.GetMax().y },
		};
	}

	std::array<V2_float, 4> FullQuadTexCoords() const {
		return {
			V2_float{ 0.0f, 0.0f },
			V2_float{ 1.0f, 0.0f },
			V2_float{ 1.0f, 1.0f },
			V2_float{ 0.0f, 1.0f },
		};
	}

	// --------------------------------------------------------
	// Effect-chain state
	// --------------------------------------------------------

	void BeginEffectChain(ImageHandle seed) {
		effect_stack_.push_back(EffectChainState{
			.current = std::move(seed),
			.scratch = {},
		});
	}

	ImageHandle EndEffectChain() {
		auto& chain = ActiveEffectChain();

		for (auto& scratch : chain.scratch) {
			if (scratch.transient) {
				ReleaseTransient(std::move(scratch.transient));
			}
		}

		chain.scratch.clear();

		auto final_image = std::move(chain.current);

		effect_stack_.pop_back();

		return final_image;
	}

	EffectChainState& ActiveEffectChain() {
		PTGN_ASSERT(!effect_stack_.empty(), "No active effect chain");
		return effect_stack_.back();
	}

	const EffectChainState& ActiveEffectChain() const {
		PTGN_ASSERT(!effect_stack_.empty(), "No active effect chain");
		return effect_stack_.back();
	}

	// --------------------------------------------------------
	// Final consumers
	// --------------------------------------------------------

	void BatchEffectResult(ImageHandle image, DrawPlacement placement) {
		auto vertices = MakeQuadVertices(
			placement.positions, placement.tex_coords, placement.depth, placement.tint,
			placement.entity_id
		);

		AddTexturedQuadToBatch(image.image.texture, vertices);

		if (image.transient) {
			current_batch_.retained_transients.push_back(std::move(image.transient));
		}
	}

	void ReplaceBoundTarget(ImageHandle image) {
		FlushBatch();

		// Old scene target release policy depends on how you own your main scene target.
		// If scene targets are also transient ping-pong targets, release the previous one here.
		// In this sketch, scene_target_ becomes the transient output target.
		PTGN_ASSERT(image.transient, "Fullscreen effect output should be a transient target");

		scene_target_ = image.transient.target;

		// Ownership transfer note:
		// We intentionally do not release image.transient here, because scene_target_
		// now represents the active target. In a real implementation, store a proper
		// owning SceneTarget handle instead of copying RenderTargetObject.
		image.transient.valid = false;
	}

	std::vector<EffectChainState> effect_stack_{};
	*/

public:
	void SetMaterial(const MaterialState& material);

	void BeginScene(const RenderTargetObject& scene_target, Color clear_color);

	void EndScene();

	void FlushBatch();

	RenderTargetPool& GetTargetPool();

	RenderPipeline& GetPipeline(PipelineId id);

	const RenderPipeline& GetPipeline(PipelineId id) const;

	template <VertexType TVertex, typename TAccessor = DefaultTextureIndexAccessor<TVertex>>
	void DrawQuads(
		std::span<const RenderQuad<TVertex>> quads, std::span<const TextureId> local_textures = {},
		TAccessor texture_index = {}
	) {
		PTGN_ASSERT(pipeline_manager_.GetCurrentPipelineId() != 0);

		RenderState current_state{ GetCurrentState() };

		batcher_.SubmitQuads<TVertex>(
			pipeline_manager_.GetCurrentPipelineId(), GetCurrentTarget(), GetCurrentMaterial(),
			current_state, quads, local_textures, texture_index
		);
	}

	template <VertexType TVertex, typename TAccessor = DefaultTextureIndexAccessor<TVertex>>
	void DrawTriangles(
		std::span<const RenderTriangle<TVertex>> triangles,
		std::span<const TextureId> local_textures = {}, TAccessor texture_index = {}
	) {
		PTGN_ASSERT(pipeline_manager_.GetCurrentPipelineId() != 0);

		RenderState current_state{ GetCurrentState() };

		batcher_.SubmitTriangles<TVertex>(
			pipeline_manager_.GetCurrentPipelineId(), GetCurrentTarget(), GetCurrentMaterial(),
			current_state, triangles, local_textures, texture_index
		);
	}

	void DrawTextures(
		std::span<const RenderQuad<TextureVertex>> quads, std::span<const TextureId> local_textures,
		const EffectParams& effects = {}, std::span<const TextureBinding> extra_textures = {}
	);

	RenderPassBuilder Pass();

	TextureSource DrawPass(
		const MaterialState& material, TextureSource input, const RenderTargetDesc& output_desc,
		const RenderState& state, std::span<const TextureBinding> extra_textures
	);

	impl::TextureId GetCurrentTargetTexture() const;
	RenderTargetId GetCurrentTarget() const;

private:
	RenderState GetCurrentState() const;
	MaterialState GetCurrentMaterial() const;

	struct TargetSave {
		RenderTargetId target{ 0 };
		std::optional<Viewport> viewport;
		Matrix4 view_projection;
		bool transient{ false };
	};

	TargetSave SaveTarget() const;

	void RestoreTarget(TargetSave save);

	void DrawBoundTargetEffect(
		const RenderQuad<TextureVertex>& quad, std::span<const TextureBinding> extra_textures
	);

	void DrawTextureWithEffects(
		TextureId source, RenderQuad<TextureVertex> quad, const EffectParams& effects
	);

	void DrawImmediateTexturedQuad(
		TextureId primary, const RenderQuad<TextureVertex>& quad,
		std::span<const TextureBinding> extra_textures
	);

	FramebufferId ResolveTarget(TextureSource source) const;
	TextureId ResolveTexture(TextureSource source) const;

	static std::array<V2_float, 4> FullscreenQuad(V2_int size);

	static std::array<V2_float, 4> QuadInsidePaddedTarget(V2_int source_size);

	static void ExpandQuadByPixels(RenderQuad<TextureVertex>& quad, V2_int source_size, int margin);

public:
	void SetViewProjection(const Matrix4& view_projection);
	void SetFramebuffer(FramebufferId framebuffer);
	void SetViewport(Viewport viewport);
	void SetBlendMode(BlendMode blend_mode);
	void SetShader(ShaderId shader);
	void SetDepthTesting(bool enabled);
	void SetDepthMask(const DepthMaskState& mask);
	void SetStencil(const StencilState& stencil);
	void SetRaster(const RasterState& raster);
	void SetScissor(const ScissorState& scissor);
	void SetColorMask(const ColorMaskState& color_mask);

	[[nodiscard]] ShaderObject CreateShader(
		const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
	);
	[[nodiscard]] TextureObject CreateTexture(
		const Surface& surface, TextureFormat format, TextureParameters params = {}
	);
	[[nodiscard]] TextureObject CreateTexture(
		const std::uint8_t* pixel_data, V2_int size, TextureFormat format,
		TextureParameters params = {}
	);
	[[nodiscard]] RenderTargetObject CreateRenderTarget(const RenderTargetDesc& desc);

	ShaderId GetShader(std::string_view name) const;

	void SetGameSize(
		std::optional<V2_int> game_size			= std::nullopt,
		std::optional<ScalingMode> scaling_mode = ScalingMode::Letterbox
	);

	void SetScalingMode(ScalingMode scaling_mode = ScalingMode::Letterbox);

	void SetPresentationViewport(std::optional<Viewport> presentation_viewport = std::nullopt);

	[[nodiscard]] bool HasGameSize() const;

	V2_int GetGameSize() const;

	ScalingMode GetScalingMode() const;

	Viewport GetPresentationViewport() const;

	V2_int GetPresentationPosition() const;

	V2_int GetPresentationSize() const;

	Viewport GetDisplayViewport() const;

	V2_int GetDisplayPosition() const;

	V2_int GetDisplaySize() const;

	V2_float GetScale() const;

	V2_int GetFullViewportSize() const;

	void SetBackgroundColor(Color background_color);
	Color GetBackgroundColor() const;

	void SetPrimaryWorldCamera(const std::optional<Camera>& primary_world_camera = std::nullopt);

	const std::optional<Camera>& GetPrimaryWorldCamera() const;

	TextureId GetRenderTargetTexture(RenderTargetId render_target) const;

	V2_int GetRenderTargetSize(RenderTargetId render_target) const;
	TextureFormat GetRenderTargetTextureFormat(RenderTargetId render_target) const;
	void ResizeRenderTarget(RenderTargetId render_target, V2_int new_size);
	void ClearRenderTarget(RenderTargetId render_target, Color color, bool set_viewport) const;
	void BindRenderTarget(RenderTargetId render_target);
	void BindScreenTarget();

	RenderTargetId GetScreenTarget() const;

	V2_int GetTextureSize(TextureId id) const;
	TextureFormat GetTextureFormat(TextureId id) const;

	void SetUniform(ShaderId id, const char* uniform_name, const Matrix4& v);
	void SetUniform(ShaderId id, const char* uniform_name, float v);
	void SetUniform(ShaderId id, const char* uniform_name, V2_float v);
	void SetUniform(ShaderId id, const char* uniform_name, V3_float v);
	void SetUniform(ShaderId id, const char* uniform_name, V4_float v);
	void SetUniform(ShaderId id, const char* uniform_name, std::span<const float> v);
	void SetUniform(ShaderId id, const char* uniform_name, int v);
	void SetUniform(ShaderId id, const char* uniform_name, V2_int v);
	void SetUniform(ShaderId id, const char* uniform_name, V3_int v);
	void SetUniform(ShaderId id, const char* uniform_name, V4_int v);
	void SetUniform(ShaderId id, const char* uniform_name, std::span<const int> v);
	void SetUniform(ShaderId id, const char* uniform_name, bool v);

	void Destroy(VertexBufferId id);
	void Destroy(ElementBufferId id);
	void Destroy(UniformBufferId id);
	void Destroy(ShaderId id);
	void Destroy(TextureId id);
	void Destroy(RenderbufferId id);
	void Destroy(FramebufferId id);
	void Destroy(VertexArrayId id);
	void Destroy(RenderTargetId id);

	void SetCurrentPipeline(std::size_t id);
	void SetCurrentPipeline(std::string_view name);

private:
	friend class ptgn::Application;
	friend class ApplicationContext;
	friend class RenderPipelineManager;
	friend class RenderBatcher;

	struct DisplayResizeInfo {
		bool moved{ false };
		bool resized{ false };
		Viewport viewport;
	};

	using EventSink =
		std::function<void(V2_int, std::variant<ResizeType, impl::PresentationResizeType>)>;

	Renderer() = delete;
	explicit Renderer(Window& window, EventSink&& event_sink);
	~Renderer() noexcept;
	Renderer(const Renderer&)				 = delete;
	Renderer(Renderer&&) noexcept			 = delete;
	Renderer& operator=(const Renderer&)	 = delete;
	Renderer& operator=(Renderer&&) noexcept = delete;

	void UploadVertices(const RenderPipeline& pipeline, std::span<const std::byte> vertices);
	void UploadIndices(const RenderPipeline& pipeline, std::span<const Index> indices);
	void DrawElements(const RenderPipeline& pipeline, std::uint32_t index_count);

	void ApplyRenderTarget(RenderTargetId id);

	void ApplyRenderState(const RenderState& state);

	void ApplyMaterial(const MaterialState& material);

	void BeginFrame();
	void EndFrame();

	void SetUniformValue(ShaderId id, const char* uniform_name, const UniformValue& v);

	[[nodiscard]] bool IsPresentationViewportVisible() const;

	std::size_t GetMaxTextureSlots() const;

	/// @return True if the given texture is currently attached to the framebuffer that is currently
	/// bound.
	[[nodiscard]] bool IsTextureAttachedToCurrentFramebuffer(TextureId texture) const;

	void ResizeScreenTarget(V2_int size);

	void OnWindowResize(V2_int size);

	void InvalidateState();

	// emit_events = false is used to prevent emitting events when initializing the window and
	// scene.
	void UpdateDisplayViewport(bool emit_events = true);

	void BindTextureSlot(std::uint32_t slot, TextureId texture);

	[[nodiscard]] DisplayResizeInfo RecalculateDisplayViewport() const;

	Window& window_;

	EventSink event_sink_;

	std::unique_ptr<gl::GLContext> gl_;

	/// @brief Currently set view projection.
	Matrix4 view_projection_;

	Color background_color_;
	RenderTargetObject screen_target_;

	std::vector<UniformWrite> current_uniforms_;
	bool current_target_is_transient_{ false };

	RenderBatcher batcher_;
	RenderTargetPool target_pool_;
	RenderPipelineManager pipeline_manager_;

	std::optional<V2_int> game_size_;
	Viewport display_viewport_;
	/// @brief Flag to indicate whether the display viewport needs to be recalculated.
	bool display_viewport_dirty_{ true };
	ScalingMode scaling_mode_{ ScalingMode::Letterbox };

	/// @brief The viewport used for presentation (i.e. the final output to the screen). This
	/// may be different from the window if using the editor, which has a separate viewport for
	/// the game view.
	std::optional<Viewport> presentation_viewport_;

	std::optional<Camera> primary_world_camera_;
};

} // namespace impl

} // namespace ptgn