#pragma once

#include <algorithm>
#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "renderer/pipeline/effect_params.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/visible.h"
#include "serialization/serialize.h"

namespace ptgn::impl {

struct RegisteredEffect {
	std::string type_name;
	std::string display_name;
	bool hdr{ false };

	Entity (*create)(Entity entity, const json& parameters){ nullptr };
	json (*serialize)(Entity entity){ nullptr };
	void (*deserialize)(Entity entity, const json& parameters){ nullptr };
	json (*make_default)(){ nullptr };
};

class EffectRegistry {
public:
	template <typename T, bool Hdr, typename... TArgs>
	static Entity Initialize(Entity entity, TArgs&&... args) {
		entity.Add<T>(std::forward<TArgs>(args)...);
		entity.Add<EffectTag>();

		if constexpr (Hdr) {
			entity.Add<HDREffectTag>();
		}

		SetDraw<T>(entity);
		entity.Add<Visible>(true);
		return entity;
	}

	template <typename T, bool Hdr, typename Registration>
	static bool Register(const Registration& registration) {
		RegisteredEffect effect{
			.type_name = std::string{ registration.type_name },
			.display_name = DisplayName(registration),
			.hdr = Hdr,
		};

		constexpr bool serializable_parameters{
			JsonSerializable<T> && JsonDeserializable<T>
		};

		effect.serialize = [](Entity entity) {
			if constexpr (serializable_parameters) {
				json value = entity.Get<T>();
				return value;
			} else {
				return json::object();
			}
		};

		effect.deserialize = [](Entity entity, const json& parameters) {
			if constexpr (serializable_parameters) {
				if (!parameters.is_null()) {
					parameters.get_to(entity.Get<T>());
				}
			}
		};

		if constexpr (std::default_initializable<T>) {
			effect.create = [](Entity entity, const json& parameters) {
				if constexpr (serializable_parameters) {
					T value{};
					if (!parameters.is_null()) {
						parameters.get_to(value);
					}
					return Initialize<T, Hdr>(entity, std::move(value));
				} else {
					return Initialize<T, Hdr>(entity);
				}
			};

			effect.make_default = []() {
				if constexpr (serializable_parameters) {
					json value = T{};
					return value;
				} else {
					return json::object();
				}
			};
		}

		return RegisterEntry(std::move(effect));
	}

	[[nodiscard]] static const RegisteredEffect* Find(std::string_view type_name) {
		const auto& entries{ Entries() };
		const auto it{ std::ranges::find_if(entries, [type_name](const RegisteredEffect& entry) {
			return entry.type_name == type_name;
		}) };

		return it == entries.end() ? nullptr : std::addressof(*it);
	}

	[[nodiscard]] static const std::vector<RegisteredEffect>& Entries() {
		return MutableEntries();
	}

private:
	template <typename Registration>
	[[nodiscard]] static std::string DisplayName(const Registration& registration) {
		if (registration.options.name.has_value()) {
			return std::string{ registration.options.name.value() };
		}

		const std::string_view type_name{ registration.type_name };
		const auto separator{ type_name.rfind("::") };
		return separator == std::string_view::npos
			? std::string{ type_name }
			: std::string{ type_name.substr(separator + 2) };
	}

	static bool RegisterEntry(RegisteredEffect effect) {
		auto& entries{ MutableEntries() };

		if (std::ranges::any_of(entries, [&effect](const RegisteredEffect& entry) {
			return entry.type_name == effect.type_name;
		})) {
			return false;
		}

		entries.emplace_back(std::move(effect));
		return true;
	}

	[[nodiscard]] static std::vector<RegisteredEffect>& MutableEntries() {
		static std::vector<RegisteredEffect> entries;
		return entries;
	}
};

} // namespace ptgn::impl
