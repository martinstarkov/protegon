#pragma once

#include <optional>
#include <string_view>

#include "runtime/graphics/drawable.h"

namespace ptgn::impl {

struct EffectRegistrationOptions {
	std::optional<std::string_view> name;
	bool hdr{ false };
};

struct EffectRegistrationData {
	std::string_view type_name;
	EffectRegistrationOptions options;
};

constexpr EffectRegistrationData MakeEffectRegistration(
	std::string_view type_name, EffectRegistrationOptions options = {}
) {
	return {
		.type_name = type_name,
		.options   = options,
	};
}

template <typename T>
struct EffectRegistration {
	static constexpr EffectRegistrationData Get() {
		return {};
	}
};

} // namespace ptgn::impl

#define PTGN_REGISTER_EFFECT(Type, ...)                                                   \
	template <>                                                                           \
	struct ::ptgn::impl::EffectRegistration<Type> {                                       \
		static constexpr ::ptgn::impl::EffectRegistrationData Get() {                     \
			return ::ptgn::impl::MakeEffectRegistration(                                  \
				#Type __VA_OPT__(, ::ptgn::impl::EffectRegistrationOptions __VA_ARGS__)   \
			);                                                                            \
		}                                                                                 \
	};                                                                                    \
	template <>                                                                           \
	struct ::ptgn::impl::DrawableRegistration<Type> {                                     \
		static constexpr ::ptgn::impl::DrawableRegistrationData Get() {                   \
			constexpr auto registration{ ::ptgn::impl::EffectRegistration<Type>::Get() }; \
                                                                                          \
			return ::ptgn::impl::MakeDrawableRegistration(                                \
				registration.type_name,                                                   \
				{                                                                         \
					.name  = registration.options.name,                                   \
					.group = "Effects",                                                   \
				}                                                                         \
			);                                                                            \
		}                                                                                 \
	};                                                                                    \
	template class ::ptgn::impl::DrawableRegistrar<Type>
