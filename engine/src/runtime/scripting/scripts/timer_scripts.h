#pragma once

#include "runtime/scripting/script_common.h"

namespace ptgn {

struct TimerStartScript : public Script {
	void OnStart() override;
	TimerKey timer{};
	PTGN_REFLECT(TimerStartScript, timer)
};

struct TimerRestartScript : public Script {
	void OnStart() override;
	TimerKey timer{};
	PTGN_REFLECT(TimerRestartScript, timer)
};

struct TimerStopScript : public Script {
	void OnStart() override;
	TimerKey timer{};
	PTGN_REFLECT(TimerStopScript, timer)
};

struct TimerResetScript : public Script {
	void OnStart() override;
	TimerKey timer{};
	PTGN_REFLECT(TimerResetScript, timer)
};

struct TimerPauseScript : public Script {
	void OnStart() override;
	TimerKey timer{};
	PTGN_REFLECT(TimerPauseScript, timer)
};

struct TimerResumeScript : public Script {
	void OnStart() override;
	TimerKey timer{};
	PTGN_REFLECT(TimerResumeScript, timer)
};

struct TimerTogglePausedScript : public Script {
	void OnStart() override;
	TimerKey timer{};
	PTGN_REFLECT(TimerTogglePausedScript, timer)
};

struct TimerAdvanceScript : public Script {
	void OnStart() override;
	TimerKey timer{};
	millisecondsf amount{ 1000.0f };
	PTGN_REFLECT(TimerAdvanceScript, timer, amount)
};

struct TimerRewindScript : public Script {
	void OnStart() override;
	TimerKey timer{};
	millisecondsf amount{ 1000.0f };
	PTGN_REFLECT(TimerRewindScript, timer, amount)
};

struct TimerSetDurationScript : public Script {
	void OnStart() override;
	TimerKey timer{};
	millisecondsf amount{ 1000.0f };
	PTGN_REFLECT(TimerSetDurationScript, timer, amount)
};

struct TimerAddDurationScript : public Script {
	void OnStart() override;
	TimerKey timer{};
	millisecondsf amount{ 1000.0f };
	PTGN_REFLECT(TimerAddDurationScript, timer, amount)
};

struct TimerRemoveDurationScript : public Script {
	void OnStart() override;
	TimerKey timer{};
	millisecondsf amount{ 1000.0f };
	PTGN_REFLECT(TimerRemoveDurationScript, timer, amount)
};

struct LifetimeStartScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(LifetimeStartScript)
};

struct LifetimeRestartScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(LifetimeRestartScript)
};

struct LifetimeStopScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(LifetimeStopScript)
};

struct LifetimeResetScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(LifetimeResetScript)
};

struct LifetimePauseScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(LifetimePauseScript)
};

struct LifetimeResumeScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(LifetimeResumeScript)
};

struct LifetimeTogglePausedScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(LifetimeTogglePausedScript)
};

struct LifetimeAdvanceScript : public Script {
	void OnStart() override;
	millisecondsf amount{ 1000.0f };
	PTGN_REFLECT(LifetimeAdvanceScript, amount)
};

struct LifetimeRewindScript : public Script {
	void OnStart() override;
	millisecondsf amount{ 1000.0f };
	PTGN_REFLECT(LifetimeRewindScript, amount)
};

struct LifetimeSetDurationScript : public Script {
	void OnStart() override;
	millisecondsf amount{ 1000.0f };
	PTGN_REFLECT(LifetimeSetDurationScript, amount)
};

struct LifetimeAddDurationScript : public Script {
	void OnStart() override;
	millisecondsf amount{ 1000.0f };
	PTGN_REFLECT(LifetimeAddDurationScript, amount)
};

struct LifetimeRemoveDurationScript : public Script {
	void OnStart() override;
	millisecondsf amount{ 1000.0f };
	PTGN_REFLECT(LifetimeRemoveDurationScript, amount)
};

} // namespace ptgn
