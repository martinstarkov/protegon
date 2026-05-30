#include "renderer/pipeline/render_pass_builder.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "renderer/resources/render_target_object.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"

namespace ptgn {

RenderPass::RenderPass(
	RenderPassBuilder& render_pass_builder, std::size_t pass_index, RenderPassHandle output
) :
	render_pass_builder_{ render_pass_builder }, pass_index_{ pass_index }, output_{ output } {}

impl::RenderPassData& RenderPass::GetPassData() {
	PTGN_ASSERT(
		pass_index_ < render_pass_builder_.passes_.size(),
		"Pass index must be in range of builder passes"
	);

	return render_pass_builder_.passes_[pass_index_];
}

RenderPass& RenderPass::Read(
	RenderPassHandle handle, std::uint32_t slot, std::string_view uniform
) {
	GetPassData().reads.emplace_back(
		impl::HandleInput{
			.handle	 = handle,
			.binding = TextureBinding{ .slot = slot, .uniform = std::string{ uniform } },
		}
	);

	return *this;
}

RenderPass& RenderPass::Uniform(std::string_view name, int value) {
	auto& pass{ GetPassData() };
	PTGN_ASSERT(
		!std::ranges::contains(pass.material.uniforms, name, &UniformWrite::name),
		"Cannot set the same uniform more than once per pass"
	);
	pass.material.uniforms.emplace_back(name, value);
	return *this;
}

RenderPass& RenderPass::Uniform(std::string_view name, float value) {
	auto& pass{ GetPassData() };
	PTGN_ASSERT(
		!std::ranges::contains(pass.material.uniforms, name, &UniformWrite::name),
		"Cannot set the same uniform more than once per pass"
	);
	pass.material.uniforms.emplace_back(name, value);
	return *this;
}

RenderPass& RenderPass::Uniform(std::string_view name, V2_float value) {
	auto& pass{ GetPassData() };
	PTGN_ASSERT(
		!std::ranges::contains(pass.material.uniforms, name, &UniformWrite::name),
		"Cannot set the same uniform more than once per pass"
	);
	pass.material.uniforms.emplace_back(name, value);
	return *this;
}

RenderPass& RenderPass::Uniform(std::string_view name, Color value) {
	auto& pass{ GetPassData() };
	PTGN_ASSERT(
		!std::ranges::contains(pass.material.uniforms, name, &UniformWrite::name),
		"Cannot set the same uniform more than once per pass"
	);
	pass.material.uniforms.emplace_back(name, value.Normalized());
	return *this;
}

RenderPass& RenderPass::Tint(Color tint) {
	GetPassData().tint = tint;
	return *this;
}

RenderPass::operator RenderPassHandle() const {
	return output_;
}

RenderPassBuilder::RenderPassBuilder(DrawContext& ctx) : ctx_{ ctx } {
	const auto& bound{ ctx_.GetBoundRenderTarget() };

	auto viewport{ ctx_.GetRenderState().viewport };

	PTGN_ASSERT(viewport.has_value(), "Viewport must be set before building render passes");

	destination_	  = *viewport;
	destination_desc_ = bound.GetDesc();
	destination_id_	  = bound;

	PTGN_ASSERT(
		destination_id_, "A non-zero render target must be bound before building render passes"
	);
}

RenderPassHandle RenderPassBuilder::BoundTarget() {
	if (bound_) {
		return *bound_;
	}

	auto handle{ NextTargetHandle() };

	auto desc{ destination_desc_ };
	desc.size = destination_.size;

	resources_.emplace_back(
		Resource{
			.handle	  = handle,
			.desc	  = desc,
			.imported = true,
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
		impl::RenderPassData{
			.material	 = { .shader = ctx_.GetShader(shader) },
			.output		 = handle,
			.output_desc = desc,
		}
	);

	return RenderPass{ *this, pass_index, handle };
}

RenderPass RenderPassBuilder::CreateLike(RenderPassHandle like, std::string_view shader) {
	return CreateLike(GetResource(like).desc, shader);
}

RenderPass RenderPassBuilder::Apply(
	std::string_view shader, std::optional<RenderPassHandle> input
) {
	auto input_handle{ input.has_value() ? *input : BoundTarget() };

	auto output{ CreateLike(input_handle, shader) };

	output.Read(input_handle);

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

void RenderPassBuilder::Materialize(RenderPassHandle handle) {
	auto& resource{ GetResource(handle) };

	if (resource.render_target.has_value()) {
		return;
	}

	PTGN_ASSERT(resource.imported, "Only imported regions can be materialized without a writer");

	auto scratch{ ctx_.AcquireRenderTarget(resource.desc) };

	ctx_.WithRenderState(
		{
			.scissor = ScissorState{ false },
		},
		[this, scratch]() {
			constexpr V2_int offset{};

			ctx_.CopyRenderTargetRegion(destination_id_, scratch, destination_, offset);
		}
	);

	resource.render_target = scratch;
}

void RenderPassBuilder::Execute(RenderPassHandle final_handle) {
	MarkUsed(final_handle);

	for (auto& resource : resources_) {
		if (!resource.used && resource.imported) {
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
		bound_inputs.reserve(pass.reads.size());

		for (const auto& input : pass.reads) {
			Materialize(input.handle);

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

		auto output_size{ ctx_.GetRenderTargetSize(output) };

		ctx_.DrawRenderPass(
			impl::DrawPassRequest{
				.material = pass.material,
				.pipeline = pipeline,
				.inputs	  = bound_inputs,
				.output	  = output,
				.viewport = Viewport{ .position{}, .size{ V2_float{ output_size } } },
				.tint	  = pass.tint,
			}
		);

		for (const auto& input : pass.reads) {
			ReleaseIfLastUse(input.handle, i);
		}
	}

	// Needed in case passes_ is empty, e.g. if pass returns pass.BoundTarget() immediately.
	Materialize(final_handle);

	auto final_target{ GetRenderTargetId(final_handle) };

	ctx_.CompositeRenderPassResult(final_target, destination_id_, destination_);

	ctx_.ReleaseRenderTarget(final_target);
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