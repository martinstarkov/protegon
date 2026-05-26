#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "renderer/resources/id.h"
#include "renderer/resources/render_target_object.h"

namespace ptgn {

class RenderPassBuilder;

class RenderPassHandle {
private:
	RenderPassHandle() = default;

	RenderPassHandle(std::size_t id) : id{ id } {}

	friend class RenderPassBuilder;

	std::size_t id{ 0 };

	explicit operator bool() const {
		return id;
	}

	bool operator==(const RenderPassHandle&) const = default;
};

struct TextureBinding {
	std::uint32_t slot{ 0 };
	std::string uniform{ "u_Texture" };
};

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
	explicit RenderPassBuilder(DrawContext& ctx) : ctx_{ ctx } {}

	RenderPassHandle BoundTarget();

	RenderPass CreateLike(RenderTargetDesc desc, std::string_view shader);

	RenderPass CreateLike(RenderPassHandle like, std::string_view shader);

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

	struct BoundInput {
		impl::RenderTargetId id;
		TextureBinding binding;
	};

	struct Resource {
		RenderPassHandle handle;
		RenderTargetDesc desc;

		impl::RenderTargetId id;

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

	Resource& GetResource(RenderPassHandle target);

	const Resource& GetResource(RenderPassHandle target) const;

	void MarkUsed(RenderPassHandle target);

	void PruneTo(RenderPassHandle final_handle);

	void ComputeLastUses(RenderPassHandle final_handle);

	impl::RenderTargetObject Execute(RenderPassHandle final_handle);

	impl::RenderTargetId Physical(RenderPassHandle target) const;

	void ReleaseIfLastUse(RenderPassHandle target, std::size_t pass_index);

	DrawContext& ctx_;

	std::vector<Resource> resources_;
	std::vector<PassData> passes_;
	std::unordered_map<std::size_t, impl::RenderTargetId> physical_by_logical_;

	std::optional<RenderPassHandle> bound_;
	std::size_t next_target_handle_{ 0 };
};

} // namespace ptgn