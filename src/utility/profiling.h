#pragma once

#include <string>
#include <unordered_map>

#include "core/manager.h"
#include "protegon/timer.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

class ProfileInstance {
public:
	ProfileInstance() = default;
	ProfileInstance(const std::string& function_name, const std::string& custom_name);
	~ProfileInstance();

private:
	std::string name_;
};

} // namespace impl

class Profiler : protected Manager<Timer, std::string> {
private:
	Profiler()							 = default;
	~Profiler()							 = default;
	Profiler(const Profiler&)			 = delete;
	Profiler(Profiler&&)				 = default;
	Profiler& operator=(const Profiler&) = delete;
	Profiler& operator=(Profiler&&)		 = default;

public:
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

	template <typename T>
	void PrintInfo(const std::string& name, const Timer& timer) const {
		static_assert(type_traits::is_duration_v<T>, "Type must be duration");
		const std::string begin_trim{ "__cdecl" }; // cutoff return type
		const std::string end_trim{ "(" };		   // cutoff function parameter list
		std::string n{ name };
		if (const auto pos = name.find(begin_trim); pos != std::string::npos) {
			const auto start{ pos + begin_trim.length() + 1 };
			const auto stop = name.find(end_trim);

			n = name.substr(start, stop == std::string::npos ? stop : stop - start);
		}
		PTGN_LOG("PROFILING: ", n, ": ", timer.Elapsed<duration<double, typename T::period>>());
	}
};

} // namespace ptgn

#define PTGN_PROFILE_FUNCTION(...)                                \
	ptgn::impl::ProfileInstance ptgn_profile_instance_##__LINE__( \
		PTGN_EXPAND_MACRO(PTGN_FUNCTION_SIGNATURE),               \
		[&]() -> const char* {                                    \
			if constexpr (PTGN_NUMBER_OF_ARGS(__VA_ARGS__) > 0) { \
				return __VA_ARGS__;                               \
			} else {                                              \
				return "";                                        \
			}                                                     \
		}()                                                       \
                                                                  \
	)

// Optional: Disable profiling in Release builds
// #ifndef PTGN_DEBUG
//
// #define PTGN_PROFILE_FUNCTION(...) ((void)0)
//
// #endif