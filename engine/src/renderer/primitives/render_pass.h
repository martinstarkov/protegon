#pragma once

#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/resource.h"
#include "renderer/primitives/texture_format.h"

namespace ptgn {

class Renderer;

namespace impl {

class RenderPass {
public:
	void Bind();

private:
	friend class ptgn::Renderer;

	RenderTargetId source_;

	RenderTargetId ping_;
	RenderTargetId pong_;

	bool has_ping_{ false };
	bool has_pong_{ false };

	// "latest output" tracking
	bool has_written_once_{ false }; // false -> latest is source
	bool latest_is_ping_{ true };	 // valid only if has_written_once == true

	Renderer* renderer_{ nullptr };
};

class RenderTargetObject : public Resource<RenderTargetId> {
public:
	using Base = Resource<RenderTargetId>;
	using Base::Base;

	V2_int GetSize() const;
	TextureFormat GetFormat() const;

	void Resize(V2_int new_size);

	void Bind() const;

	void Clear(Color color, bool set_viewport) const;

	impl::TextureId GetTextureId() const;

private:
	friend class ptgn::Renderer;

	RenderTargetObject() = default;
	RenderTargetObject(Renderer* renderer, V2_int size, TextureFormat format);
};

} // namespace impl

} // namespace ptgn