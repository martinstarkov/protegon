#pragma once

#include <ctime>
#include <string>
#include <string_view>
#include <type_traits>

#include "common/assert.h"
#include "core/time.h"
#include "core/timer.h"
#include "debug/log.h"
#include "resources/resource_manager.h"

namespace ptgn::impl {

class Game;

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

class Profiler : protected MapManager<nanoseconds, std::string, std::string, false> {
public:
	Profiler()								 = default;
	~Profiler() override					 = default;
	Profiler(Profiler&&) noexcept			 = default;
	Profiler& operator=(Profiler&&) noexcept = default;
	Profiler(const Profiler&)				 = delete;
	Profiler& operator=(const Profiler&)	 = delete;

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

	template <Duration D = milliseconds>
	void PrintAll() const {
		for (const auto& [name, time] : GetMap()) {
			PrintInfo<D>(name, to_duration<D>(time));
		}
	}

	template <Duration D = milliseconds>
	void Print(const std::string& name) const {
		PTGN_ASSERT(Has(name), "Cannot print profiling info for name which is not being profiled");
		auto& time{ Get(name) };
		PrintInfo<D>(name, to_duration<D>(time));
	}

private:
	friend class Game;
	friend class ProfileInstance;

	bool enabled_{ false };

	template <Duration D = milliseconds>
	void PrintInfo(std::string_view name, const D& time) const {
		PrintLine("PROFILING: ", impl::TrimFunctionSignature(name), ": ", time);
	}
};

} // namespace ptgn::impl

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