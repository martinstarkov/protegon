#pragma once

#include <filesystem>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>

#include "utility/type_traits.h"

namespace ptgn {

namespace impl {

template <typename... T>
inline constexpr size_t NumberOfArgs(T... a) {
	return sizeof...(a);
}

} // namespace impl

} // namespace ptgn

#define PTGN_NUMBER_OF_ARGS(...) ptgn::impl::NumberOfArgs(__VA_ARGS__)

namespace ptgn {

namespace impl {

template <typename... TArgs, type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void PrintImpl(std::ostream& ostream, TArgs&&... items) {
	((ostream << std::forward<TArgs>(items)), ...);
}

template <typename... TArgs, type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void PrintLineImpl(std::ostream& ostream, TArgs&&... items) {
	PrintImpl(ostream, std::forward<TArgs>(items)...);
	ostream << '\n';
}

inline void PrintLineImpl(std::ostream& ostream) {
	ostream << '\n';
}

} // namespace impl

// Print desired items to the console. If a newline is desired, use PrintLine()
// instead.
template <typename... TArgs, type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void Print(TArgs&&... items) {
	impl::PrintImpl(std::cout, std::forward<TArgs>(items)...);
}

// Print desired items to the console and add a newline. If no newline is
// desired, use Print() instead.
template <typename... TArgs, type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void PrintLine(TArgs&&... items) {
	impl::PrintLineImpl(std::cout, std::forward<TArgs>(items)...);
}

inline void PrintLine() {
	impl::PrintLineImpl(std::cout);
}

namespace debug {

// Print desired items to the console. If a newline is desired, use PrintLine()
// instead.
template <typename... TArgs, type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void Print(TArgs&&... items) {
	ptgn::impl::PrintImpl(std::cerr, std::forward<TArgs>(items)...);
}

// Print desired items to the console and add a newline. If no newline is
// desired, use Print() instead.
template <typename... TArgs, type_traits::stream_writable<std::ostream, TArgs...> = true>
inline void PrintLine(TArgs&&... items) {
	ptgn::impl::PrintLineImpl(std::cerr, std::forward<TArgs>(items)...);
}

inline void PrintLine() {
	ptgn::impl::PrintLineImpl(std::cerr);
}

} // namespace debug

namespace impl {

// This class exists so that printline(ss, __VA_ARGS__) does not fail with 0 args.
struct StringStreamWriter {
	StringStreamWriter() = default;

	template <typename... TArgs, type_traits::stream_writable<std::ostream, TArgs...> = true>
	void Write(TArgs&&... items) {
		((ss << std::forward<TArgs>(items)), ...);
	}

	template <typename... TArgs, type_traits::stream_writable<std::ostream, TArgs...> = true>
	void WriteLine(TArgs&&... items) {
		Write(std::forward<TArgs>(items)...);
		ss << std::endl;
	}

	std::string Get() const {
		return ss.str();
	}

	std::stringstream ss;
};

} // namespace impl

} // namespace ptgn

#define PTGN_INTERNAL_WRITE_STREAM(internal_stream_writer, ...)                 \
	{                                                                           \
		internal_stream_writer.Write(                                           \
			std::filesystem::path(__FILE__).filename().string(), ":", __LINE__, \
			[&]() -> const char* {                                              \
				if (PTGN_NUMBER_OF_ARGS(__VA_ARGS__) > 0) {                     \
					return ": ";                                                \
				} else {                                                        \
					return "";                                                  \
				}                                                               \
			}()                                                                 \
		);                                                                      \
		internal_stream_writer.Write(__VA_ARGS__);                              \
	}

#define PTGN_LOG(...)                                          \
	{                                                          \
		ptgn::impl::StringStreamWriter internal_stream_writer; \
		internal_stream_writer.Write(__VA_ARGS__);             \
		ptgn::PrintLine(internal_stream_writer.Get());         \
	}
#define PTGN_INFO(...)                                           \
	{                                                            \
		ptgn::impl::StringStreamWriter internal_stream_writer;   \
		internal_stream_writer.Write(__VA_ARGS__);               \
		ptgn::PrintLine("INFO: ", internal_stream_writer.Get()); \
	}
#define PTGN_WARN(...)                                                   \
	{                                                                    \
		ptgn::impl::StringStreamWriter internal_stream_writer;           \
		PTGN_INTERNAL_WRITE_STREAM(internal_stream_writer, __VA_ARGS__); \
		ptgn::debug::PrintLine("WARN: ", internal_stream_writer.Get());  \
	}

#define PTGN_ERROR(...)                                                  \
	{                                                                    \
		ptgn::impl::StringStreamWriter internal_stream_writer;           \
		PTGN_INTERNAL_WRITE_STREAM(internal_stream_writer, __VA_ARGS__); \
		ptgn::debug::PrintLine("ERROR: ", internal_stream_writer.Get()); \
		PTGN_EXCEPTION(internal_stream_writer.Get());                    \
	}