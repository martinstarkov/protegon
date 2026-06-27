#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"

namespace ptgn {

class RenderPassBuilder;
class DrawContext;

class RenderPassHandle {
public:
	explicit operator std::size_t() const {
		return id;
	}

private:
	RenderPassHandle() = default;

	RenderPassHandle(std::size_t id) : id{ id } {} // NOSONAR

	friend class RenderPassBuilder;

	std::size_t id{ 0 };

	explicit operator bool() const {
		return id;
	}

	bool operator==(const RenderPassHandle&) const = default;
};

} // namespace ptgn

template <>
struct std::hash<ptgn::RenderPassHandle> {
	std::size_t operator()(const ptgn::RenderPassHandle& handle) const noexcept {
		return static_cast<std::size_t>(handle);
	}
};

namespace ptgn {

namespace impl {

struct BoundInput {
	impl::FramebufferId framebuffer;
	TextureBinding binding;
};

struct HandleInput {
	RenderPassHandle handle;
	TextureBinding binding;
};

struct RenderPassData {
	MaterialState material;
	Color tint{ color::White };
	std::vector<HandleInput> reads;
	RenderPassHandle output;
	TextureDesc output_desc;
	bool used{ false };
	std::optional<Color> clear_color;
};

struct DrawPassRequest {
	MaterialState material;
	std::size_t pipeline{ 0 };
	std::span<const impl::BoundInput> inputs;
	impl::FramebufferId output;
	Viewport viewport;
	Color tint{ color::White };
	bool scissor_to_viewport{ false };
};

} // namespace impl

class RenderPass {
public:
	RenderPass(
		RenderPassBuilder& render_pass_builder, std::size_t pass_index, RenderPassHandle output
	);

	RenderPass& Read(
		RenderPassHandle handle, std::uint32_t slot = 0, std::string_view uniform = kTextureUniform
	);

	RenderPass& Uniform(std::string_view name, float value);
	RenderPass& Uniform(std::string_view name, int value);
	RenderPass& Uniform(std::string_view name, V2_float value);
	RenderPass& Uniform(std::string_view name, Color value);

	RenderPass& Tint(Color tint);

	RenderPass& ClearColor(Color value);

	operator RenderPassHandle() const;

private:
	friend class RenderPassBuilder;

	impl::RenderPassData& GetPassData();

	RenderPassBuilder& render_pass_builder_;
	std::size_t pass_index_{ 0 };
	RenderPassHandle output_;
};

class RenderPassBuilder {
public:
	explicit RenderPassBuilder(DrawContext& ctx);

	RenderPassHandle BoundTarget();

	RenderPass CreateTarget(TextureDesc desc);

	RenderPass CreateLike(
		TextureDesc desc, std::string_view shader,
		std::optional<std::size_t> texture_slot_capacity = std::nullopt
	);

	RenderPass CreateLike(
		RenderPassHandle handle, std::string_view shader,
		std::optional<std::size_t> texture_slot_capacity = std::nullopt
	);

	/// @param input Optional input to the shader. If nullopt, uses the currently bound target as
	/// input.
	RenderPass Apply(std::string_view shader, std::optional<RenderPassHandle> input = std::nullopt);

private:
	friend class DrawContext;
	friend class RenderPass;

	struct Resource {
		RenderPassHandle handle;
		TextureDesc desc;

		bool imported{ false };

		std::optional<std::size_t> writer;
		std::optional<std::size_t> last_use;
		std::optional<impl::FramebufferId> framebuffer;
		bool used{ false };
	};

	RenderPassHandle NextTargetHandle();

	void Materialize(RenderPassHandle handle);

	Resource& GetResource(RenderPassHandle handle);

	const Resource& GetResource(RenderPassHandle handle) const;

	void MarkUsed(RenderPassHandle handle);

	void Execute(RenderPassHandle final_handle);

	impl::FramebufferId GetFramebufferId(RenderPassHandle handle) const;

	void ReleaseIfLastUse(RenderPassHandle handle, std::size_t pass_index);

	DrawContext& ctx_;

	impl::FramebufferId destination_id_;
	Viewport destination_;
	TextureDesc destination_desc_;

	std::vector<Resource> resources_;
	std::vector<impl::RenderPassData> passes_;

	std::optional<RenderPassHandle> bound_;

	std::size_t next_target_handle_{ 0 };
};

} // namespace ptgn