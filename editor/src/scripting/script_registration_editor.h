#pragma once

#include "runtime/scripting/script_registration_engine.h"
#include "scripting/script_editor_registry.h"

#define PTGN_IMPL_REGISTER_EDITOR_SCRIPT_IMPL(Type, Id, ...)                             \
	namespace {                                                                           \
	[[maybe_unused]] const bool PTGN_IMPL_SCRIPT_REGISTRATION_JOIN(                       \
		ptgn_registered_editor_script_, Id                                                  \
	){ ::ptgn::editor::script::RegisterScriptEditors<Type>(__VA_ARGS__) };                 \
	}

#define PTGN_IMPL_REGISTER_EDITOR_SCRIPT(Type, ...) \
	PTGN_IMPL_REGISTER_EDITOR_SCRIPT_IMPL(Type, __COUNTER__, __VA_ARGS__)

#define PTGN_IMPL_REGISTER_EDITOR_EVENT_IMPL(Type, Id, ...)                              \
	namespace {                                                                           \
	[[maybe_unused]] const bool PTGN_IMPL_SCRIPT_REGISTRATION_JOIN(                       \
		ptgn_registered_editor_event_, Id                                                   \
	){ ::ptgn::editor::script::EventEditorRegistry::Register<Type>(__VA_ARGS__) };         \
	}

#define PTGN_IMPL_REGISTER_EDITOR_EVENT(Type, ...) \
	PTGN_IMPL_REGISTER_EDITOR_EVENT_IMPL(Type, __COUNTER__, __VA_ARGS__)

#undef PTGN_REGISTER_SCRIPT
#undef PTGN_REGISTER_EVENT

/// @brief Registers the runtime Script metadata and any editor definitions supplied after it.
///
/// Each editor definition should be a parenthesized SequenceStepEditor(...) or
/// RootScriptEditor(...) expression.
#define PTGN_REGISTER_SCRIPT(Type, RegisterArgs, ...) \
	PTGN_IMPL_REGISTER_ENGINE_SCRIPT(Type, RegisterArgs) \
	__VA_OPT__(PTGN_IMPL_REGISTER_EDITOR_SCRIPT(Type, __VA_ARGS__))

/// @brief Registers the runtime event matcher and an optional editor definition.
#define PTGN_REGISTER_EVENT(Type, RegisterArgs, ...) \
	PTGN_IMPL_REGISTER_ENGINE_EVENT(Type, RegisterArgs) \
	__VA_OPT__(PTGN_IMPL_REGISTER_EDITOR_EVENT(Type, __VA_ARGS__))
