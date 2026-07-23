#pragma once

#include "runtime/scripting/script.h"

#define PTGN_IMPL_SCRIPT_REGISTRATION_JOIN_IMPL(lhs, rhs) lhs##rhs
#define PTGN_IMPL_SCRIPT_REGISTRATION_JOIN(lhs, rhs) \
	PTGN_IMPL_SCRIPT_REGISTRATION_JOIN_IMPL(lhs, rhs)

#define PTGN_IMPL_REGISTER_ENGINE_SCRIPT_IMPL(Type, Id, RegisterArgs)                    \
	namespace {                                                                           \
	[[maybe_unused]] const bool PTGN_IMPL_SCRIPT_REGISTRATION_JOIN(                       \
		ptgn_registered_engine_script_, Id                                                  \
	){ ::ptgn::ScriptRegistry::Register<Type> RegisterArgs };                             \
	}

#define PTGN_IMPL_REGISTER_ENGINE_SCRIPT(Type, RegisterArgs) \
	PTGN_IMPL_REGISTER_ENGINE_SCRIPT_IMPL(Type, __COUNTER__, RegisterArgs)

#define PTGN_IMPL_REGISTER_ENGINE_EVENT_IMPL(Type, Id, RegisterArgs)                     \
	namespace {                                                                           \
	[[maybe_unused]] const bool PTGN_IMPL_SCRIPT_REGISTRATION_JOIN(                       \
		ptgn_registered_engine_event_, Id                                                   \
	){ ::ptgn::SequenceEventRegistry::Register<Type> RegisterArgs };                      \
	}

#define PTGN_IMPL_REGISTER_ENGINE_EVENT(Type, RegisterArgs) \
	PTGN_IMPL_REGISTER_ENGINE_EVENT_IMPL(Type, __COUNTER__, RegisterArgs)

/// @brief Registers a Script type with the runtime registry.
///
/// RegisterArgs must be parenthesized because it is appended directly to
/// ScriptRegistry::Register<Type>. Any optional editor definitions are discarded in an
/// engine-only translation unit.
///
/// PTGN_REGISTER_SCRIPT(
///     MoveToScript,
///     (ScriptRegistrationOptions{
///         .supports_timing = true,
///         .default_timing = ScriptTiming{ .duration_ms = 300.0f },
///         .completion = ScriptCompletion::Duration,
///     })
/// );
#define PTGN_REGISTER_SCRIPT(Type, RegisterArgs, ...) \
	PTGN_IMPL_REGISTER_ENGINE_SCRIPT(Type, RegisterArgs)

/// @brief Registers an event type with the sequence-event runtime registry.
///
/// RegisterArgs must be parenthesized because it is appended directly to
/// SequenceEventRegistry::Register<Type>. Any optional editor definition is discarded in an
/// engine-only translation unit.
#define PTGN_REGISTER_EVENT(Type, RegisterArgs, ...) \
	PTGN_IMPL_REGISTER_ENGINE_EVENT(Type, RegisterArgs)
