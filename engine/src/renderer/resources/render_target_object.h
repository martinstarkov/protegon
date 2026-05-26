#pragma once

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

class Renderer;

struct RenderTargetDesc {
	V2_int size;
	TextureFormat format{ TextureFormat::RGBA8 };
	TextureParams params;

	bool operator==(const RenderTargetDesc&) const = default;
};

namespace impl {

class RenderTargetObject : public Resource<RenderTargetId> {
public:
	using Base = Resource<RenderTargetId>;
	using Base::Base;

	V2_int GetSize() const;
	TextureFormat GetFormat() const;

	void Resize(V2_int new_size);

	void Bind();

	void Clear(Color color, bool set_viewport, bool restore_bind) const;

	impl::TextureId GetTextureId() const;

private:
	friend class ptgn::Renderer;

	RenderTargetObject() = default;
	RenderTargetObject(Renderer* renderer, const RenderTargetDesc& desc);
};

} // namespace impl

} // namespace ptgn