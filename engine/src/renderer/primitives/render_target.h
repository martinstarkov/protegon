#pragma once

#include <optional>

#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/resource.h"
#include "renderer/primitives/texture_format.h"

namespace ptgn {

class Renderer;

namespace impl {

class RenderTargetData {
public:
	FramebufferId framebuffer_;
	std::optional<TextureId> color_;
	std::optional<RenderbufferId> depth_;
	// TODO: Consider using the cache values instead to prevent synchronization issues.
	V2_int size_;
	TextureFormat format_{ TextureFormat::RGBA8 };

	RenderTargetData() = default;

	RenderTargetData(
		FramebufferId framebuffer, const std::optional<TextureId>& color,
		const std::optional<RenderbufferId>& depth, V2_int size, TextureFormat format
	);

	bool operator==(const RenderTargetData&) const = default;

	operator TextureId() const;
};

class RenderPass {
public:
	void Bind();

private:
	friend class Renderer;

	RenderTargetData source_;

	RenderTargetData ping_;
	RenderTargetData pong_;

	bool has_ping_{ false };
	bool has_pong_{ false };

	// "latest output" tracking
	bool has_written_once_{ false }; // false -> latest is source
	bool latest_is_ping_{ true };	 // valid only if has_written_once == true

	Renderer* renderer_{ nullptr };
};

class RenderTargetObject : public Resource<RenderTargetData> {
public:
	using Base = Resource<RenderTargetData>;
	using Base::Base;

	V2_int GetSize() const;
	TextureFormat GetFormat() const;
	void Resize(V2_int new_size);

	void Bind() const;

	void Clear(Color color, bool set_viewport) const;

	operator TextureId() const;

private:
	friend class Renderer;

	RenderTargetObject() = default;
	RenderTargetObject(Renderer* renderer, V2_int size, TextureFormat format);
};

} // namespace impl

} // namespace ptgn