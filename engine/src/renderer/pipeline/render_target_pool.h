#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

struct RenderTargetDesc {
	V2_int size;
	TextureFormat format{ TextureFormat::RGBA8 };
	TextureParameters params;

	bool operator==(const RenderTargetDesc&) const = default;
};

namespace impl {

class Renderer;

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

class RenderTargetPool {
public:
	explicit RenderTargetPool(Renderer& renderer);

	RenderTargetObject& Acquire(RenderTargetDesc desc, FramebufferId exclude);

	RenderTargetObject& AcquireLike(const RenderTargetObject& target, int margin = 0);

	void Release(RenderTargetId id);
	void Release(const RenderTargetObject& target);

	[[nodiscard]] bool Owns(const RenderTargetObject& target) const;

private:
	struct PooledTarget {
		RenderTargetObject target;
		std::uint64_t last_used_tick{ 0 };
		bool in_use{ false };
	};

	Renderer& renderer_;
	std::vector<PooledTarget> pool_;
	std::uint64_t tick_{ 0 };
};

struct BoundTarget {};

} // namespace impl

using TextureSource = std::variant<impl::TextureId, impl::FramebufferId, impl::BoundTarget>;

namespace impl {

struct TextureBinding {
	std::string name;
	impl::TextureId source;
};

} // namespace impl

} // namespace ptgn