#include "runtime/graphics/fx/screen_effect_stack.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>

#include "app/application_context.h"
#include "core/log.h"
#include "runtime/graphics/fx/effect_registry.h"
#include "runtime/graphics/visible.h"

namespace ptgn::impl {

Entity RegisterScreenEffectEntity(
	ApplicationContext& ctx,
	Entity effect,
	std::string_view type,
	ScreenEffectId source_id,
	std::optional<std::uint64_t> runtime_id
) {
	if (!effect) {
		return {};
	}

	const std::uint64_t id{
		runtime_id.value_or(ctx.next_screen_effect_runtime_id++)
	};
	ctx.next_screen_effect_runtime_id =
		std::max(ctx.next_screen_effect_runtime_id, id + 1);

	ScreenEffectInstance instance{
		.runtime_id = id,
		.source_id = source_id,
		.type = std::string{ type },
	};

	if (effect.Has<ScreenEffectInstance>()) {
		effect.Get<ScreenEffectInstance>() = std::move(instance);
	} else {
		effect.Add<ScreenEffectInstance>(std::move(instance));
	}

	if (!std::ranges::contains(ctx.screen_effect_order, effect)) {
		ctx.screen_effect_order.emplace_back(effect);
	}

	return effect;
}

Entity CreateScreenEffectEntity(
	ApplicationContext& ctx,
	std::string_view type,
	const json& parameters,
	ScreenEffectId source_id,
	std::optional<std::uint64_t> runtime_id
) {
	const auto* registration{ EffectRegistry::Find(type) };
	if (!registration || !registration->create) {
		PTGN_WARN("Cannot create unregistered screen effect: ", type);
		return {};
	}

	Entity effect{ ctx.screen_effect_manager.CreateEntity() };
	registration->create(effect, parameters);
	return RegisterScreenEffectEntity(
		ctx,
		effect,
		type,
		source_id,
		runtime_id
	);
}

void RebuildScreenEffects(
	ApplicationContext& ctx,
	const ScreenEffectSettings& settings
) {
	ctx.screen_effect_manager.Reset();
	ctx.screen_effect_order.clear();

	for (const auto& serialized : settings.effects) {
		Entity effect{
			CreateScreenEffectEntity(
				ctx,
				serialized.type,
				serialized.parameters,
				serialized.id
			)
		};

		if (!effect) {
			continue;
		}

		effect.Get<Visible>().visible = serialized.enabled;
	}
}

void ClearScreenEffects(ApplicationContext& ctx) {
	ctx.screen_effect_manager.Reset();
	ctx.screen_effect_order.clear();
}

void RefreshScreenEffectOrder(ApplicationContext& ctx) {
	std::erase_if(ctx.screen_effect_order, [](Entity entity) {
		return !entity;
	});

	for (Entity entity : ctx.screen_effect_manager.Entities()) {
		if (!std::ranges::contains(ctx.screen_effect_order, entity)) {
			ctx.screen_effect_order.emplace_back(entity);
		}
	}
}

Entity FindScreenEffectByRuntimeId(
	ApplicationContext& ctx,
	std::uint64_t runtime_id
) {
	for (Entity entity : ctx.screen_effect_order) {
		if (!entity || !entity.Has<ScreenEffectInstance>()) {
			continue;
		}

		if (entity.Get<ScreenEffectInstance>().runtime_id == runtime_id) {
			return entity;
		}
	}
	return {};
}

Entity FindScreenEffectBySourceId(
	ApplicationContext& ctx,
	ScreenEffectId source_id
) {
	if (source_id == 0) {
		return {};
	}

	for (Entity entity : ctx.screen_effect_order) {
		if (!entity || !entity.Has<ScreenEffectInstance>()) {
			continue;
		}

		if (entity.Get<ScreenEffectInstance>().source_id == source_id) {
			return entity;
		}
	}
	return {};
}

std::optional<std::size_t> FindScreenEffectIndex(
	const ApplicationContext& ctx,
	std::uint64_t runtime_id
) {
	for (std::size_t index{ 0 }; index < ctx.screen_effect_order.size(); ++index) {
		Entity entity{ ctx.screen_effect_order[index] };
		if (!entity || !entity.Has<ScreenEffectInstance>()) {
			continue;
		}

		if (entity.Get<ScreenEffectInstance>().runtime_id == runtime_id) {
			return index;
		}
	}
	return std::nullopt;
}

std::optional<RuntimeScreenEffectSnapshot> CaptureScreenEffect(Entity entity) {
	if (!entity || !entity.Has<ScreenEffectInstance>()) {
		return std::nullopt;
	}

	const auto& instance{ entity.Get<ScreenEffectInstance>() };
	const auto* registration{ EffectRegistry::Find(instance.type) };

	return RuntimeScreenEffectSnapshot{
		.runtime_id = instance.runtime_id,
		.source_id = instance.source_id,
		.type = instance.type,
		.enabled = entity.Has<Visible>() && entity.Get<Visible>().visible,
		.parameters = registration && registration->serialize
			? registration->serialize(entity)
			: json::object(),
	};
}

Entity RestoreScreenEffect(
	ApplicationContext& ctx,
	const RuntimeScreenEffectSnapshot& snapshot,
	std::optional<std::size_t> index
) {
	Entity effect{ FindScreenEffectByRuntimeId(ctx, snapshot.runtime_id) };

	if (effect && effect.Has<ScreenEffectInstance>() &&
		effect.Get<ScreenEffectInstance>().type != snapshot.type) {
		std::erase(ctx.screen_effect_order, effect);
		effect.Destroy();
		effect = {};
	}

	const auto* registration{ EffectRegistry::Find(snapshot.type) };
	if (!registration || !registration->create) {
		return {};
	}

	if (!effect) {
		effect = CreateScreenEffectEntity(
			ctx,
			snapshot.type,
			snapshot.parameters,
			snapshot.source_id,
			snapshot.runtime_id
		);
	} else {
		auto& instance{ effect.Get<ScreenEffectInstance>() };
		instance.source_id = snapshot.source_id;
		instance.type = snapshot.type;

		if (registration->deserialize) {
			registration->deserialize(effect, snapshot.parameters);
		}
	}

	if (!effect) {
		return {};
	}

	if (effect.Has<Visible>()) {
		effect.Get<Visible>().visible = snapshot.enabled;
	} else {
		effect.Add<Visible>(snapshot.enabled);
	}

	if (index.has_value()) {
		std::erase(ctx.screen_effect_order, effect);
		const std::size_t target{
			std::min(index.value(), ctx.screen_effect_order.size())
		};
		ctx.screen_effect_order.insert(
			ctx.screen_effect_order.begin() + static_cast<std::ptrdiff_t>(target),
			effect
		);
	}

	return effect;
}

bool RemoveScreenEffect(
	ApplicationContext& ctx,
	std::uint64_t runtime_id
) {
	Entity entity{ FindScreenEffectByRuntimeId(ctx, runtime_id) };
	if (!entity) {
		return false;
	}

	std::erase(ctx.screen_effect_order, entity);
	entity.Destroy();
	return true;
}

bool MoveScreenEffect(
	ApplicationContext& ctx,
	std::uint64_t runtime_id,
	std::size_t to_index
) {
	const auto from_index{ FindScreenEffectIndex(ctx, runtime_id) };
	if (!from_index.has_value() || to_index >= ctx.screen_effect_order.size()) {
		return false;
	}

	if (from_index.value() == to_index) {
		return true;
	}

	Entity entity{ ctx.screen_effect_order[from_index.value()] };
	ctx.screen_effect_order.erase(
		ctx.screen_effect_order.begin() + static_cast<std::ptrdiff_t>(from_index.value())
	);
	ctx.screen_effect_order.insert(
		ctx.screen_effect_order.begin() + static_cast<std::ptrdiff_t>(to_index),
		entity
	);
	return true;
}

} // namespace ptgn::impl
