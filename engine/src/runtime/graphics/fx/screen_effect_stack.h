#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "runtime/ecs/entity.h"
#include "serialization/json/json.h"
#include "serialization/serialize.h"

namespace ptgn {

using ScreenEffectId = std::uint64_t;

struct SerializedScreenEffect {
	ScreenEffectId id{ 0 };
	std::string type{};
	bool enabled{ true };
	json parameters = json::object();

	PTGN_REFLECT(SerializedScreenEffect, id, type, enabled, parameters)
};

struct ScreenEffectSettings {
	std::vector<SerializedScreenEffect> effects{};

	PTGN_REFLECT(ScreenEffectSettings, effects)
};

[[nodiscard]] inline SerializedScreenEffect* FindScreenEffect(
	ScreenEffectSettings& settings,
	ScreenEffectId id
) {
	for (auto& effect : settings.effects) {
		if (effect.id == id) {
			return std::addressof(effect);
		}
	}
	return nullptr;
}

[[nodiscard]] inline const SerializedScreenEffect* FindScreenEffect(
	const ScreenEffectSettings& settings,
	ScreenEffectId id
) {
	for (const auto& effect : settings.effects) {
		if (effect.id == id) {
			return std::addressof(effect);
		}
	}
	return nullptr;
}

[[nodiscard]] inline ScreenEffectId NextScreenEffectId(
	const ScreenEffectSettings& settings
) {
	ScreenEffectId next{ 1 };
	for (const auto& effect : settings.effects) {
		next = std::max(next, effect.id + 1);
	}
	return next;
}

namespace impl {

class ApplicationContext;

struct ScreenEffectInstance {
	std::uint64_t runtime_id{ 0 };
	ScreenEffectId source_id{ 0 };
	std::string type{};
};

struct RuntimeScreenEffectSnapshot {
	std::uint64_t runtime_id{ 0 };
	ScreenEffectId source_id{ 0 };
	std::string type{};
	bool enabled{ true };
	json parameters = json::object();
};

Entity RegisterScreenEffectEntity(
	ApplicationContext& ctx,
	Entity effect,
	std::string_view type,
	ScreenEffectId source_id = 0,
	std::optional<std::uint64_t> runtime_id = std::nullopt
);

Entity CreateScreenEffectEntity(
	ApplicationContext& ctx,
	std::string_view type,
	const json& parameters = json::object(),
	ScreenEffectId source_id = 0,
	std::optional<std::uint64_t> runtime_id = std::nullopt
);

void RebuildScreenEffects(
	ApplicationContext& ctx,
	const ScreenEffectSettings& settings
);

void ClearScreenEffects(ApplicationContext& ctx);
void RefreshScreenEffectOrder(ApplicationContext& ctx);

[[nodiscard]] Entity FindScreenEffectByRuntimeId(
	ApplicationContext& ctx,
	std::uint64_t runtime_id
);

[[nodiscard]] Entity FindScreenEffectBySourceId(
	ApplicationContext& ctx,
	ScreenEffectId source_id
);

[[nodiscard]] std::optional<std::size_t> FindScreenEffectIndex(
	const ApplicationContext& ctx,
	std::uint64_t runtime_id
);

[[nodiscard]] std::optional<RuntimeScreenEffectSnapshot> CaptureScreenEffect(
	Entity entity
);

Entity RestoreScreenEffect(
	ApplicationContext& ctx,
	const RuntimeScreenEffectSnapshot& snapshot,
	std::optional<std::size_t> index = std::nullopt
);

bool RemoveScreenEffect(
	ApplicationContext& ctx,
	std::uint64_t runtime_id
);

bool MoveScreenEffect(
	ApplicationContext& ctx,
	std::uint64_t runtime_id,
	std::size_t to_index
);

} // namespace impl

} // namespace ptgn
