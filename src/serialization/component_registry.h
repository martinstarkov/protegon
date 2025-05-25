#pragma once

#include <string_view>
#include <unordered_map>

#include "common/type_info.h"
#include "core/manager.h"
#include "debug/log.h"
#include "math/hash.h"

namespace ptgn::impl {

class ComponentRegistry {
public:
	using ComponentRegistrationFunc = void (*)(Manager& manager);

	static auto& GetData() {
		static std::unordered_map<std::size_t, ComponentRegistrationFunc> s;
		return s;
	}

	template <typename T>
	static bool Register() {
		constexpr auto class_name{ type_name<T>() };
		PTGN_LOG("Registering component: ", class_name);
		auto& registry{ GetData() };
		registry[Hash(class_name)] = [](Manager& manager) -> void {
			manager.RegisterType<T>();
		};
		return true;
	}

	static void AddTypes(Manager& manager) {
		const auto& registry{ GetData() };
		for (const auto& [type_name, component_func] : registry) {
			std::invoke(component_func, manager);
		}
	}
};

} // namespace ptgn::impl