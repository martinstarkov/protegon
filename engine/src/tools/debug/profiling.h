#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "core/assert.h"
#include "core/log.h"
#include "core/time/time.h"
#include "core/time/timer.h"
#include "core/util/function.h"
#include "core/util/macro.h"

namespace ptgn {

class DebugSystem;

namespace impl {

class ProfileInstance {
public:
	ProfileInstance(std::string_view function_name);
	ProfileInstance(ProfileInstance&&) noexcept			   = default;
	ProfileInstance& operator=(ProfileInstance&&) noexcept = default;
	ProfileInstance(const ProfileInstance&)				   = default;
	ProfileInstance& operator=(const ProfileInstance&)	   = default;
	~ProfileInstance();

private:
	ProfileInstance() = default;

	std::string name_{};
	Timer timer_;
};

class Profiler {
public:
	Profiler()								 = default;
	~Profiler() noexcept					 = default;
	Profiler(Profiler&&) noexcept			 = delete;
	Profiler& operator=(Profiler&&) noexcept = delete;
	Profiler(const Profiler&)				 = delete;
	Profiler& operator=(const Profiler&)	 = delete;

	void PrintAll() const {
		PrintAll<>();
	}

	template <DurationType D = milliseconds>
	void PrintAll() const {
		for (const auto& [name, time] : timings_) {
			PrintInfo<D>(name, to_duration<D>(time));
		}
	}

	template <DurationType D = milliseconds>
	void Print(const std::string& name) const {
		PTGN_ASSERT(
			timings_.contains(name),
			"Cannot print profiling info for name which is not being profiled"
		);
		auto& time{ timings_.find(name)->second };
		PrintInfo<D>(name, to_duration<D>(time));
	}

private:
	friend class ptgn::DebugSystem;
	friend class ProfileInstance;

	template <DurationType D = milliseconds>
	void PrintInfo(std::string_view name, const D& time) const {
		PrintLine("PROFILING: ", impl::TrimFunctionSignature(name), ": ", time);
	}

	std::unordered_map<std::string, nanoseconds> timings_;
};

static Profiler& GetProfiler() {
	static Profiler profiler;
	return profiler;
}

} // namespace impl

} // namespace ptgn

#define PTGN_PROFILE_FUNCTION()                                   \
	ptgn::impl::ProfileInstance ptgn_profile_instance_##__LINE__( \
		PTGN_EXPAND(PTGN_FULL_FUNCTION_SIGNATURE)                 \
	)

#define PTGN_PROFILE_FUNCTION_NAMED(name) \
	ptgn::impl::ProfileInstance ptgn_profile_instance_##__LINE__(name)

// Optional: In the future profiling could be disabled for distribution builds.
// #ifdef PTGN_DISTRIBUTION
//
// #define PTGN_PROFILE_FUNCTION(...) ((void)0)
//
// #endif