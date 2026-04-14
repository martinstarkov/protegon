#pragma once

#include <type_traits>
#include <utility>

#include "core/util/concepts.h"
#include "serialization/serialize.h"

#define PTGN_FLAGS_OPERATORS(EnumName)                                         \
	inline EnumName operator|(EnumName a, EnumName b) {                        \
		using UT = std::underlying_type_t<EnumName>;                           \
		return static_cast<EnumName>(static_cast<UT>(a) | static_cast<UT>(b)); \
	}                                                                          \
	inline EnumName operator&(EnumName a, EnumName b) {                        \
		using UT = std::underlying_type_t<EnumName>;                           \
		return static_cast<EnumName>(static_cast<UT>(a) & static_cast<UT>(b)); \
	}                                                                          \
	inline EnumName operator^(EnumName a, EnumName b) {                        \
		using UT = std::underlying_type_t<EnumName>;                           \
		return static_cast<EnumName>(static_cast<UT>(a) ^ static_cast<UT>(b)); \
	}                                                                          \
	inline EnumName operator~(EnumName a) {                                    \
		using UT = std::underlying_type_t<EnumName>;                           \
		return static_cast<EnumName>(~static_cast<UT>(a));                     \
	}                                                                          \
	inline EnumName& operator|=(EnumName& a, EnumName b) {                     \
		return a = a | b;                                                      \
	}                                                                          \
	inline EnumName& operator&=(EnumName& a, EnumName b) {                     \
		return a = a & b;                                                      \
	}                                                                          \
	inline EnumName& operator^=(EnumName& a, EnumName b) {                     \
		return a = a ^ b;                                                      \
	}

namespace ptgn {

template <EnumType TEnum>
class Flags {
public:
	using underlying = std::underlying_type_t<TEnum>;

	Flags() = default;

	explicit Flags(underlying bits) : bits_{ bits } {}

	explicit Flags(TEnum flag) : bits_{ std::to_underlying(flag) } {}

	bool operator==(const Flags&) const = default;

	void Set(TEnum flag) {
		bits_ |= std::to_underlying(flag);
	}

	void Clear(TEnum flag) {
		bits_ &= ~std::to_underlying(flag);
	}

	void Toggle(TEnum flag) {
		bits_ ^= std::to_underlying(flag);
	}

	[[nodiscard]] bool IsSet(TEnum flag) const {
		return (bits_ & std::to_underlying(flag)) != 0;
	}

	void ClearAll() {
		bits_ = 0;
	}

	/// @return True if any of the bits are set.
	[[nodiscard]] bool AnySet() const {
		return bits_ != 0;
	}

	underlying GetBits() const {
		return bits_;
	}

	void SetBits(underlying bits) {
		bits_ = bits;
	}

	PTGN_REFLECT(Flags<TEnum>, bits_)

private:
	underlying bits_{ 0 };
};

} // namespace ptgn