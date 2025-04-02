#pragma once

#include <string_view>

namespace ptgn {

template <typename T>
constexpr std::string_view type_name();

template <>
constexpr std::string_view type_name<void>() {
	return "void";
}

namespace impl {

using type_name_prober = void;

template <typename T>
constexpr std::string_view wrapped_type_name() {
#ifdef __clang__
	return __PRETTY_FUNCTION__;
#elif defined(__GNUC__)
	return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
	return __FUNCSIG__;
#else
#error "Unsupported compiler"
#endif
}

constexpr std::string_view remove_class_or_struct_prefix(std::string_view input) {
	constexpr std::string_view class_prefix	 = "class ";
	constexpr std::string_view struct_prefix = "struct ";

	if (input.size() >= class_prefix.size() &&
		std::equal(class_prefix.begin(), class_prefix.end(), input.begin())) {
		return input.substr(class_prefix.size());
	} else if (input.size() >= struct_prefix.size() &&
			   std::equal(struct_prefix.begin(), struct_prefix.end(), input.begin())) {
		return input.substr(struct_prefix.size());
	} else {
		return input;
	}
}

constexpr std::size_t wrapped_type_name_prefix_length() {
	return wrapped_type_name<type_name_prober>().find(type_name<type_name_prober>());
}

constexpr std::size_t wrapped_type_name_suffix_length() {
	return wrapped_type_name<type_name_prober>().length() - wrapped_type_name_prefix_length() -
		   type_name<type_name_prober>().length();
}

constexpr std::string_view trim_to_last_colon(std::string_view input) {
	if (std::size_t last_colon_pos{ input.rfind(':') }; last_colon_pos != std::string_view::npos) {
		return input.substr(last_colon_pos + 1);
	} else {
		return input;
	}
}

} // namespace impl

template <typename T>
constexpr std::string_view type_name() {
	constexpr auto wrapped_name		= impl::wrapped_type_name<T>();
	constexpr auto prefix_length	= impl::wrapped_type_name_prefix_length();
	constexpr auto suffix_length	= impl::wrapped_type_name_suffix_length();
	constexpr auto type_name_length = wrapped_name.length() - prefix_length - suffix_length;
	return impl::remove_class_or_struct_prefix(wrapped_name.substr(prefix_length, type_name_length)
	);
}

template <typename T>
constexpr std::string_view type_name_without_namespaces() {
	return impl::trim_to_last_colon(type_name<T>());
}

} // namespace ptgn