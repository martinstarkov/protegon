#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "renderer/resources/id.h"
#include "renderer/resources/render_target_object.h"
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
	impl::RenderTargetId render_target;
	TextureBinding binding;
};

} // namespace impl

class RenderPass {
public:
	RenderPass(
		RenderPassBuilder& render_pass_builder, std::size_t pass_index, RenderPassHandle output
	);

	RenderPass& Read(
		RenderPassHandle handle, std::uint32_t slot = 0, std::string_view uniform = "u_Texture"
	);

	operator RenderPassHandle() const;

private:
	RenderPassBuilder& render_pass_builder_;
	std::size_t pass_index_{ 0 };
	RenderPassHandle output_;
};

class RenderPassBuilder {
public:
	explicit RenderPassBuilder(DrawContext& ctx);

	RenderPassHandle BoundTarget();

	RenderPass CreateLike(RenderTargetDesc desc, std::string_view shader);

	RenderPass CreateLike(RenderPassHandle handle, std::string_view shader);

	/// @param input Optional input to the shader. If nullopt, uses the currently bound target as
	/// input.
	RenderPassHandle Apply(
		std::string_view shader, std::optional<RenderPassHandle> input = std::nullopt
	);

private:
	friend class DrawContext;
	friend class RenderPass;

	struct HandleInput {
		RenderPassHandle handle;
		TextureBinding binding;
	};

	struct Resource {
		RenderPassHandle handle;
		RenderTargetDesc desc;

		/// @brief The render target ID if this resource is imported from the context, or nullopt if
		/// it is created by a pass in this builder.
		std::optional<impl::RenderTargetId> render_target;

		std::optional<std::size_t> writer;
		std::optional<std::size_t> last_use;
		bool used{ false };
	};

	struct PassData {
		impl::ShaderId shader;
		std::vector<HandleInput> reads;
		RenderPassHandle output;
		RenderTargetDesc output_desc;
		bool used{ false };
	};

	RenderPassHandle NextTargetHandle();

	Resource& GetResource(RenderPassHandle handle);

	const Resource& GetResource(RenderPassHandle handle) const;

	void MarkUsed(RenderPassHandle handle);

	[[nodiscard]] bool IsImported(const Resource& resource) const;

	impl::RenderTargetObject Execute(RenderPassHandle final_handle);

	impl::RenderTargetId GetRenderTargetId(RenderPassHandle handle) const;

	void ReleaseIfLastUse(RenderPassHandle handle, std::size_t pass_index);

	DrawContext& ctx_;

	std::vector<Resource> resources_;
	std::vector<PassData> passes_;

	std::optional<RenderPassHandle> bound_;

	std::size_t next_target_handle_{ 0 };
};

} // namespace ptgn