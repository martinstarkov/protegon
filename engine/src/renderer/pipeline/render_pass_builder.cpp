#include "renderer/pipeline/render_pass_builder.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/util/hash.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/resources/id.h"
#include "renderer/resources/render_target_object.h"

namespace ptgn {

RenderPass::RenderPass(
	RenderPassBuilder& render_pass_builder, std::size_t pass_index, RenderPassHandle output
) :
	render_pass_builder_{ render_pass_builder }, pass_index_{ pass_index }, output_{ output } {}

RenderPass& RenderPass::Read(
	RenderPassHandle handle, std::uint32_t slot, std::string_view uniform
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
			.handle		   = handle,
			.desc		   = std::move(desc),
			.render_target = current_target,
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

	PTGN_ASSERT(*resource.writer < passes_.size(), "Writer index out of range");

	auto& pass{ passes_[*resource.writer] };
	pass.used = true;

	for (const auto& input : pass.reads) {
		MarkUsed(input.handle);
	}
}

bool RenderPassBuilder::IsImported(const Resource& resource) const {
	return resource.render_target.has_value() && ctx_.RenderTargetPoolHas(*resource.render_target);
}

impl::RenderTargetObject RenderPassBuilder::Execute(RenderPassHandle final_handle) {
	MarkUsed(final_handle);

	for (auto& resource : resources_) {
		if (!resource.used && IsImported(resource)) {
			PTGN_ASSERT(resource.render_target.has_value());
			ctx_.ReleaseRenderTarget(*resource.render_target);
		}
		resource.last_use = std::nullopt;
	}

	for (auto i{ 0uz }; i < passes_.size(); ++i) {
		const auto& pass{ passes_[i] };

		if (!pass.used) {
			continue;
		}

		for (const auto& input : pass.reads) {
			GetResource(input.handle).last_use = i;
		}
	}

	GetResource(final_handle).last_use = passes_.size();

	for (auto i{ 0uz }; i < passes_.size(); ++i) {
		auto& pass{ passes_[i] };

		if (!pass.used) {
			continue;
		}

		std::vector<impl::BoundInput> bound_inputs;

		for (const auto& input : pass.reads) {
			auto render_target{ GetRenderTargetId(input.handle) };

			bound_inputs.emplace_back(
				impl::BoundInput{
					.render_target = render_target,
					.binding	   = input.binding,
				}
			);
		}

		auto output{ ctx_.AcquireRenderTarget(pass.output_desc) };

		GetResource(pass.output).render_target = output;

		// TODO: Consider making pipeline customizable in the future.
		constexpr auto pipeline{ Hash("texture") };

		ctx_.DrawRenderPass(pass.shader, pipeline, bound_inputs, output);

		for (const auto& input : pass.reads) {
			ReleaseIfLastUse(input.handle, i);
		}
	}

	return ctx_.ExtractRenderTarget(GetRenderTargetId(final_handle));
}

impl::RenderTargetId RenderPassBuilder::GetRenderTargetId(RenderPassHandle handle) const {
	const auto& resource{ GetResource(handle) };

	PTGN_ASSERT(
		resource.render_target.has_value(), "RenderPassHandle has no assigned render target yet"
	);

	return *resource.render_target;
}

void RenderPassBuilder::ReleaseIfLastUse(RenderPassHandle target, std::size_t pass_index) {
	auto& resource{ GetResource(target) };

	if (resource.last_use.has_value() && *resource.last_use != pass_index) {
		return;
	}

	PTGN_ASSERT(
		resource.render_target.has_value(), "RenderPassHandle has no assigned render target yet"
	);

	ctx_.ReleaseRenderTarget(*resource.render_target);
}

} // namespace ptgn