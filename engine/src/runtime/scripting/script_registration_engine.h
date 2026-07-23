#pragma once

#include "runtime/scripting/script.h"

#define PTGN_IMPL_SCRIPT_REGISTRATION_JOIN_IMPL(lhs, rhs) lhs##rhs
#define PTGN_IMPL_SCRIPT_REGISTRATION_JOIN(lhs, rhs) \
	PTGN_IMPL_SCRIPT_REGISTRATION_JOIN_IMPL(lhs, rhs)

#define PTGN_IMPL_REGISTER_ENGINE_SCRIPT_IMPL(Type, Id, ...)                              \
	namespace {                                                                            \
	[[maybe_unused]] const bool PTGN_IMPL_SCRIPT_REGISTRATION_JOIN(                        \
		ptgn_registered_engine_script_, Id                                                   \
	){ ::ptgn::ScriptRegistry::Register<Type>(__VA_ARGS__) };                              \
	}

#define PTGN_IMPL_REGISTER_ENGINE_SCRIPT(Type, ...)                                       \
	PTGN_IMPL_REGISTER_ENGINE_SCRIPT_IMPL(                                                 \
		Type, __COUNTER__ __VA_OPT__(, ) __VA_ARGS__                                        \
	)

#define PTGN_IMPL_REGISTER_ENGINE_EVENT_IMPL(Type, Id, ...)                               \
	namespace {                                                                            \
	[[maybe_unused]] const bool PTGN_IMPL_SCRIPT_REGISTRATION_JOIN(                        \
		ptgn_registered_engine_event_, Id                                                    \
	){ ::ptgn::SequenceEventRegistry::Register<Type>(__VA_ARGS__) };                       \
	}

#define PTGN_IMPL_REGISTER_ENGINE_EVENT(Type, ...)                                        \
	PTGN_IMPL_REGISTER_ENGINE_EVENT_IMPL(                                                  \
		Type, __COUNTER__ __VA_OPT__(, ) __VA_ARGS__                                        \
	)

/// @brief Registers a Script type with optional runtime metadata.
///
/// PTGN_REGISTER_SCRIPT(MyScript);
///
/// PTGN_REGISTER_SCRIPT(
///     MoveToScript,
///     {
///         .completion = ScriptCompletion::Duration,
///         .supports_timing = true,
///     }
/// );
#define PTGN_REGISTER_SCRIPT(Type, ...)                                                   \
	PTGN_IMPL_REGISTER_ENGINE_SCRIPT(Type __VA_OPT__(, ) __VA_ARGS__)

/// @brief Registers an event type with optional runtime matching metadata.
///
/// With no options, every dispatched event of the registered type matches.
#define PTGN_REGISTER_EVENT(Type, ...)                                                    \
	PTGN_IMPL_REGISTER_ENGINE_EVENT(Type __VA_OPT__(, ) __VA_ARGS__)
