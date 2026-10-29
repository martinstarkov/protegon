#pragma once

#include "runtime/scripting/script_common.h"

namespace ptgn {

enum class ConsoleLogLevel : std::uint8_t {
	Debug,
	Info,
	Warning,
	Error
};
PTGN_REFLECT_ENUM(ConsoleLogLevel);

[[nodiscard]] std::string ResolveConsoleMessage(std::string_view message, Entity target);

struct PrintConsoleScript : public Script {
	std::string message{};
	ConsoleLogLevel level{ ConsoleLogLevel::Info };
	bool include_entity{ false };
	bool include_sequence{ false };
	bool include_timestamp{ false };

	void OnStart() override;
	PTGN_REFLECT(
		PrintConsoleScript, message, level, include_entity, include_sequence, include_timestamp
	)
};

struct EmitSignalScript : public Script {
	SignalKey signal{ "sequence.completed" };
	EmitSignalScript() = default;

	explicit EmitSignalScript(SignalKey signal_key) : signal{ std::move(signal_key) } {}

	void OnStart() override;
	PTGN_REFLECT(EmitSignalScript, signal)
};

using SequenceFunction = std::variant<std::function<void()>, std::function<void(SequenceHandle)>>;
using DuringSequenceFunction =
	std::variant<std::function<void()>, std::function<void(Entity, float)>>;
ScriptSequence& EmitSignal(ScriptSequence& sequence, SignalKey signal);
SequenceHandle After(Scene& scene, milliseconds duration, SequenceFunction function);
SequenceHandle During(Scene& scene, milliseconds duration, DuringSequenceFunction function);

} // namespace ptgn
