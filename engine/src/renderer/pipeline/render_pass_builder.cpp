#include "renderer/pipeline/render_pass_builder.h"

#include <algorithm>
#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/resources/id.h"
#include "renderer/resources/render_target_object.h"

namespace ptgn {

RenderPass::RenderPass(
	RenderPassBuilder& render_pass_builder, std::size_t pass_index, RenderPassHandle output
) :
	render_pass_builder_{ render_pass_builder }, pass_index_{ pass_index }, output_{ output } {}

RenderPass& RenderPass::Read(
	RenderPassHandle handle, std::uint32_t slot = 0, std::string_view uniform = "u_Texture"
) {
	PTGN_ASSERT(
		pass_index_ < render_pass_builder_.passes_.size(),
		"Pass index must be in range of builder passes"
	);

	render_pass_builder_.passes_[pass_index_].reads.emplace_back(
		RenderPassBuilder::HandleInput{
			.handle	 = handle,
			.binding = TextureBinding{ .slot = slot, .uniform = std::string{ uniform } },
		}
	);

	return *this;
}

RenderPass::operator RenderPassHandle() const {
	return output_;
}

RenderPassBuilder::RenderPassBuilder(DrawContext& ctx) : ctx_{ ctx } {}

RenderPassHandle RenderPassBuilder::BoundTarget() {
	if (bound_) {
		return *bound_;
	}

	auto handle{ NextTargetHandle() };

	const auto& current_target{ ctx_.GetRenderTarget() };

	auto desc{ current_target.GetDesc() };

	resources_.emplace_back(
		Resource{
			.handle = handle,
			.desc	= std::move(desc),
			.id		= current_target,
		}
	);

	bound_ = handle;
	return handle;
}

RenderPass RenderPassBuilder::CreateLike(RenderTargetDesc desc, std::string_view shader) {
	auto handle{ NextTargetHandle() };

	auto pass_index{ passes_.size() };

	resources_.emplace_back(
		Resource{
			.handle = handle,
			.desc	= desc,
			.writer = pass_index,
		}
	);

	passes_.emplace_back(
		PassData{
			.shader		 = ctx_.GetShader(shader),
			.output		 = handle,
			.output_desc = desc,
		}
	);

	return RenderPass{ *this, pass_index, handle };
}

RenderPass RenderPassBuilder::CreateLike(RenderPassHandle like, std::string_view shader) {
	return CreateLike(GetResource(like).desc, shader);
}

RenderPassHandle RenderPassBuilder::Apply(
	std::string_view shader, std::optional<RenderPassHandle> input
) {
	auto input_handle{ input.has_value() ? *input : BoundTarget() };

	auto output{ CreateLike(input_handle, shader).Read(input_handle).operator RenderPassHandle() };

	return output;
}

RenderPassHandle RenderPassBuilder::NextTargetHandle() {
	return RenderPassHandle{ next_target_handle_++ };
}

RenderPassBuilder::Resource& RenderPassBuilder::GetResource(RenderPassHandle target) {
	auto it{ std::ranges::find_if(resources_, [&](const Resource& resource) {
		return resource.handle == target;
	}) };

	PTGN_ASSERT(it != resources_.end(), "Target not found");

	return *it;
}

const RenderPassBuilder::Resource& RenderPassBuilder::GetResource(RenderPassHandle target) const {
	auto it{ std::ranges::find_if(resources_, [&](const Resource& resource) {
		return resource.handle == target;
	}) };

	PTGN_ASSERT(it != resources_.end(), "Target not found");

	return *it;
}

void RenderPassBuilder::MarkUsed(RenderPassHandle target) {
	auto& resource{ GetResource(target) };
	resource.used = true;

	if (!resource.writer.has_value()) {
		return;
	}

	auto& pass = passes_[*resource.writer];
	pass.used  = true;

	for (const auto& input : pass.reads) {
		MarkUsed(input.handle);
	}
}

void RenderPassBuilder::PruneTo(RenderPassHandle final_handle) {
	MarkUsed(final_handle);

	for (auto& resource : resources_) {
		if (resource.imported && resource.releasable_import && !resource.used) {
			ctx_.Release(resource.imported_physical);
			resource.releasable_import = false;
		}
	}
}

void RenderPassBuilder::ComputeLastUses(RenderPassHandle final_handle) {
	for (auto& resource : resources_) {
		resource.last_use = std::nullopt;
	}

	for (auto i{ 0uz }; i < passes_.size(); ++i) {
		const auto& pass = passes_[i];

		if (!pass.used) {
			continue;
		}

		for (const auto& input : pass.reads) {
			GetResource(input.handle).last_use = i;
		}
	}

	GetResource(final_handle).last_use = passes_.size();
}

impl::RenderTargetObject RenderPassBuilder::Execute(RenderPassHandle final_handle) {
	PruneTo(final_handle);
	ComputeLastUses(final_handle);

	for (auto i{ 0uz }; i < passes_.size(); ++i) {
		auto& pass = passes_[i];

		if (!pass.used) {
			continue;
		}

		std::vector<BoundInput> bound_inputs;

		for (const auto& input : pass.reads) {
			auto id{ Physical(input.handle) };

			bound_inputs.emplace_back(
				BoundInput{
					.id		 = id,
					.binding = input.binding,
				}
			);
		}

		auto output{ ctx_.Acquire(pass.output_desc) };
		physical_by_logical_[pass.output.id] = output;

		ctx_.DrawFullscreen(pass.shader, bound_inputs, output, pass.output_desc);

		for (const auto& input : pass.reads) {
			ReleaseIfLastUse(input.handle, i);
		}
	}
}

impl::RenderTargetId RenderPassBuilder::Physical(RenderPassHandle target) const {
	if (const auto& resource{ GetResource(target) }; resource.imported) {
		return resource.imported_physical;
	}

	auto it{ physical_by_logical_.find(target.id) };

	PTGN_ASSERT(
		it != physical_by_logical_.end(), "RenderPassHandle has no physical allocation yet"
	);

	return it->second;
}

void RenderPassBuilder::ReleaseIfLastUse(RenderPassHandle target, std::size_t pass_index) {
	auto& resource{ GetResource(target) };

	if (resource.last_use.has_value() && *resource.last_use != pass_index) {
		return;
	}

	if (resource.imported && !resource.releasable_import) {
		return;
	}

	auto physical{ Physical(target) };
	ctx_.Release(physical);

	if (resource.imported) {
		resource.releasable_import = false;
	}
}

} // namespace ptgn