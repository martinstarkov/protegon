#pragma once

#include "runtime/scripting/script_registration_engine.h"
#include "scripting/script_editor_registry.h"

#define PTGN_IMPL_REGISTER_EDITOR_SCRIPT_IMPL(Type, Id, ...)                              \
	namespace {                                                                            \
	[[maybe_unused]] const bool PTGN_IMPL_SCRIPT_REGISTRATION_JOIN(                        \
		ptgn_registered_editor_script_, Id                                                   \
	){ ::ptgn::editor::ScriptEditorRegistry::Register<Type>(__VA_ARGS__) };                \
	}

#define PTGN_IMPL_REGISTER_EDITOR_SCRIPT(Type, ...)                                       \
	PTGN_IMPL_REGISTER_EDITOR_SCRIPT_IMPL(                                                 \
		Type, __COUNTER__ __VA_OPT__(, ) __VA_ARGS__                                        \
	)

#define PTGN_IMPL_REGISTER_EDITOR_EVENT_IMPL(Type, Id, ...)                               \
	namespace {                                                                            \
	[[maybe_unused]] const bool PTGN_IMPL_SCRIPT_REGISTRATION_JOIN(                        \
		ptgn_registered_editor_event_, Id                                                    \
	){ ::ptgn::editor::EventEditorRegistry::Register<Type>(__VA_ARGS__) };                 \
	}

#define PTGN_IMPL_REGISTER_EDITOR_EVENT(Type, ...)                                        \
	PTGN_IMPL_REGISTER_EDITOR_EVENT_IMPL(                                                  \
		Type, __COUNTER__ __VA_OPT__(, ) __VA_ARGS__                                        \
	)

#undef PTGN_REGISTER_SCRIPT
#undef PTGN_REGISTER_EVENT

#define PTGN_REGISTER_SCRIPT(Type, ...)                                                   \
	PTGN_IMPL_REGISTER_ENGINE_SCRIPT(Type)                                                \
	PTGN_IMPL_REGISTER_EDITOR_SCRIPT(Type __VA_OPT__(, ) __VA_ARGS__)

#define PTGN_REGISTER_EVENT(Type, ...)                                                    \
	PTGN_IMPL_REGISTER_ENGINE_EVENT(Type)                                                 \
	PTGN_IMPL_REGISTER_EDITOR_EVENT(Type __VA_OPT__(, ) __VA_ARGS__)
