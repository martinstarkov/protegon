#pragma once

#include <cstdint>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/texture_format.h"

namespace ptgn::impl {

class Renderer;

struct RenderTargetDesc {
	V2_int size;
	TextureFormat format{ TextureFormat::RGBA8 };
	TextureParameters params;

	bool operator==(const RenderTargetDesc&) const = default;
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
	friend class Renderer;

	RenderTargetObject() = default;
	RenderTargetObject(Renderer* renderer, const RenderTargetDesc& desc);
};

struct PooledRenderTarget {
	RenderTargetObject target;
	std::uint64_t last_used_tick{ 0 };
	bool in_use{ false };
};

class RenderTargetPool {
public:
private:
	std::vector<PooledRenderTarget> rt_pool_;
	std::uint64_t pool_tick_{ 0 };
	std::size_t max_pool_size_{ 32 };
};

} // namespace ptgn::impl