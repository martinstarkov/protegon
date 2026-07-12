#pragma once

#include "panels/component_editor_registry.h"
#include "runtime/ecs/component_registration_engine.h"

#define PTGN_IMPL_REGISTER_EDITOR_COMPONENT_IMPL(Type, Id, ...)        \
	namespace {                                                        \
	[[maybe_unused]] const bool PTGN_IMPL_COMPONENT_REGISTRATION_JOIN( \
		ptgn_registered_editor_component_, Id                          \
	){ ::ptgn::editor::ComponentEditorRegistry::Register<Type>(        \
		::ptgn::editor::MakeComponentEditorRegistration(__VA_ARGS__)   \
	) };                                                               \
	}

#define PTGN_IMPL_REGISTER_EDITOR_COMPONENT(Type, ...) \
	PTGN_IMPL_REGISTER_EDITOR_COMPONENT_IMPL(Type, __COUNTER__ __VA_OPT__(, ) __VA_ARGS__)

#undef PTGN_REGISTER_COMPONENT

/// @brief Registers an ECS component with both the engine and editor.
///
/// PTGN_REGISTER_COMPONENT(MyComponent);
///
/// PTGN_REGISTER_COMPONENT(
///     MyComponent,
///     {
///         .group = "Gameplay Components",
///         .default_open = true,
///     }
/// );
#define PTGN_REGISTER_COMPONENT(Type, ...)    \
	PTGN_IMPL_REGISTER_ENGINE_COMPONENT(Type) \
	PTGN_IMPL_REGISTER_EDITOR_COMPONENT(Type __VA_OPT__(, ) __VA_ARGS__)
