#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

#include "core/assert.h"
#include "core/math/vector2.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

namespace impl {

using RenderResourceId = std::uint32_t;

struct TextureNode {
	RenderResourceId id{ 0 };
};

struct TargetNode {
	RenderResourceId id{ 0 };
};

struct ImportedRenderTargetResource {
	RenderTargetId target;
};

struct TransientRenderTargetResource {
	std::optional<RenderTargetId> assigned_target;
};

using RenderResourceStorage =
	std::variant<ImportedRenderTargetResource, TransientRenderTargetResource>;

struct RenderResource {
	std::string name;

	V2_int size;
	TextureFormat format{ TextureFormat::RGBA8 };

	RenderResourceStorage storage{ TransientRenderTargetResource{} };

	[[nodiscard]] bool IsImported() const {
		return std::holds_alternative<ImportedRenderTargetResource>(storage);
	}

	[[nodiscard]] bool IsTransient() const {
		return std::holds_alternative<TransientRenderTargetResource>(storage);
	}

	[[nodiscard]] RenderTargetId GetTarget() const {
		return std::visit(
			[]<typename T>(const T& s) -> RenderTargetId {
				if constexpr (std::is_same_v<T, ImportedRenderTargetResource>) {
					return s.target;
				} else {
					PTGN_ASSERT(s.assigned_target.has_value(), "Transient target was not compiled");
					return *s.assigned_target;
				}
			},
			storage
		);
	}

	void AssignTransientTarget(RenderTargetId target) {
		PTGN_ASSERT(IsTransient(), "Cannot assign a transient target to an imported resource");
		std::get<TransientRenderTargetResource>(storage).assigned_target = target;
	}
};

} // namespace impl

} // namespace ptgn