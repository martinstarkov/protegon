#pragma once

#include "core/util/type_info.h"
#include "runtime/ecs/component_registry.h"

#define PTGN_IMPL_COMPONENT_REGISTRATION_JOIN_IMPL(lhs, rhs) lhs##rhs
#define PTGN_IMPL_COMPONENT_REGISTRATION_JOIN(lhs, rhs) \
	PTGN_IMPL_COMPONENT_REGISTRATION_JOIN_IMPL(lhs, rhs)

#define PTGN_IMPL_REGISTER_ENGINE_COMPONENT_IMPL(Type, Id)                                        \
	namespace {                                                                                   \
	[[maybe_unused]] const bool PTGN_IMPL_COMPONENT_REGISTRATION_JOIN(                            \
		ptgn_registered_engine_component_, Id                                                     \
	){ ::ptgn::ComponentRegistry::Register<Type>(::ptgn::type_name_without_namespaces<Type>()) }; \
	}

#define PTGN_IMPL_REGISTER_ENGINE_COMPONENT(Type) \
	PTGN_IMPL_REGISTER_ENGINE_COMPONENT_IMPL(Type, __COUNTER__)

/// @brief Registers an ECS component with the engine.
///
/// The optional arguments are intentionally discarded in an engine-only translation unit. This
/// allows the same registration call to contain editor options without introducing an editor
/// dependency when PTGN_EDITOR is unavailable.
#define PTGN_REGISTER_COMPONENT(Type) PTGN_IMPL_REGISTER_ENGINE_COMPONENT(Type)
