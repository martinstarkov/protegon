#pragma once

#include <ctime>
#include <string>
#include <string_view>
#include <type_traits>

#include "core/manager.h"
#include "protegon/log.h"
#include "protegon/timer.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

class ProfileInstance {
public:
	ProfileInstance() = default;
	ProfileInstance(std::string_view function_name, std::string_view custom_name);
	~ProfileInstance();

private:
	std::string name_;
};

} // namespace impl

class Profiler : protected Manager<Timer, std::string> {
private:
	Profiler()							 = default;
	~Profiler() override				 = default;
	Profiler(const Profiler&)			 = delete;
	Profiler(Profiler&&)				 = default;
	Profiler& operator=(const Profiler&) = delete;
	Profiler& operator=(Profiler&&)		 = default;

public:
	void Enable() {
		enabled_ = true;
	}

	void Disable() {
		enabled_ = false;
	}

	[[nodiscard]] bool IsEnabled() const {
		return enabled_;
	}

	void PrintAll() const {
		PrintAll<>();
	}

	template <typename T = milliseconds>
	void PrintAll() const {
		for (const auto& [name, timer] : GetMap()) {
			PrintInfo<T>(name, timer);
		}
	}

	template <typename T = milliseconds>
	void Print(const std::string& name) const {
		PTGN_ASSERT(Has(name), "Cannot print profiling info for name which is not being profiled");
		auto& timer{ Get(name) };
		PrintInfo<T>(name, timer);
	}

private:
	friend class impl::ProfileInstance;
	friend class Game;

	bool enabled_{ false };

	template <typename T>
	void PrintInfo(std::string_view name, const Timer& timer) const {
		static_assert(tt::is_duration_v<T>, "Type must be duration");
		PTGN_LOG(
			"PROFILING: ", impl::TrimFunctionSignature(name), ": ",
			timer.Elapsed<duration<double, typename T::period>>()
		);
	}
};

} // namespace ptgn

#define PTGN_PROFILE_FUNCTION(...)                                                               \
	ptgn::impl::ProfileInstance ptgn_profile_instance_##__LINE__(                                \
		PTGN_EXPAND_MACRO(PTGN_FULL_FUNCTION_SIGNATURE), std::invoke([&]() -> std::string_view { \
			if constexpr (PTGN_NUMBER_OF_ARGS(__VA_ARGS__) > 0) {                                \
				return __VA_ARGS__;                                                              \
			} else {                                                                             \
				return "";                                                                       \
			}                                                                                    \
		})                                                                                       \
                                                                                                 \
	)

// Optional: In the future profiling could be disabled for distribution builds.
// #ifdef PTGN_DISTRIBUTION
//
// #define PTGN_PROFILE_FUNCTION(...) ((void)0)
//
// #endif