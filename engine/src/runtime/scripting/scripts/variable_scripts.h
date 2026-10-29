#pragma once

#include "runtime/scripting/script_common.h"

namespace ptgn {

struct CounterSetScript : public Script {
	void OnStart() override;
	CounterKey counter{};
	std::int64_t counter_amount{ 1 };
	PTGN_REFLECT(CounterSetScript, counter, counter_amount)
};

struct CounterIncrementScript : public Script {
	void OnStart() override;
	CounterKey counter{};
	std::int64_t counter_amount{ 1 };
	PTGN_REFLECT(CounterIncrementScript, counter, counter_amount)
};

struct CounterDecrementScript : public Script {
	void OnStart() override;
	CounterKey counter{};
	std::int64_t counter_amount{ 1 };
	PTGN_REFLECT(CounterDecrementScript, counter, counter_amount)
};

struct CounterResetScript : public Script {
	void OnStart() override;
	CounterKey counter{};
	PTGN_REFLECT(CounterResetScript, counter)
};

struct BooleanSetTrueScript : public Script {
	void OnStart() override;
	BooleanKey boolean{};
	PTGN_REFLECT(BooleanSetTrueScript, boolean)
};

struct BooleanSetFalseScript : public Script {
	void OnStart() override;
	BooleanKey boolean{};
	PTGN_REFLECT(BooleanSetFalseScript, boolean)
};

struct BooleanToggleScript : public Script {
	void OnStart() override;
	BooleanKey boolean{};
	PTGN_REFLECT(BooleanToggleScript, boolean)
};

struct BooleanResetScript : public Script {
	void OnStart() override;
	BooleanKey boolean{};
	PTGN_REFLECT(BooleanResetScript, boolean)
};

} // namespace ptgn
