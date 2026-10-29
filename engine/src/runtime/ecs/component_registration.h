#pragma once

#include "core/util/type_info.h"
#include "runtime/ecs/component_registry.h"

#define PTGN_IMPL_STRING_JOIN(a, b) a##b
#define PTGN_IMPL_JOIN(a, b)		PTGN_IMPL_STRING_JOIN(a, b)

/// @brief Registers an ECS component with the engine.
#define PTGN_REGISTER_COMPONENT(Type)                                                           \
	namespace {                                                                                 \
	[[maybe_unused]] const bool PTGN_IMPL_JOIN(ptgn_registered_component_, __COUNTER__){        \
		::ptgn::ComponentRegistry::Register<Type>(::ptgn::type_name_without_namespaces<Type>()) \
	};                                                                                          \
	}
