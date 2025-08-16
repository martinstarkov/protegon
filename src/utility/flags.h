#pragma once

#include <type_traits>

#include "common/concepts.h"
#include "serialization/serializable.h"

#define PTGN_FLAGS_OPERATORS(EnumType)                                         \
	inline EnumType operator|(EnumType a, EnumType b) {                        \
		using UT = std::underlying_type_t<EnumType>;                           \
		return static_cast<EnumType>(static_cast<UT>(a) | static_cast<UT>(b)); \
	}                                                                          \
	inline EnumType operator&(EnumType a, EnumType b) {                        \
		using UT = std::underlying_type_t<EnumType>;                           \
		return static_cast<EnumType>(static_cast<UT>(a) & static_cast<UT>(b)); \
	}                                                                          \
	inline EnumType operator^(EnumType a, EnumType b) {                        \
		using UT = std::underlying_type_t<EnumType>;                           \
		return static_cast<EnumType>(static_cast<UT>(a) ^ static_cast<UT>(b)); \
	}                                                                          \
	inline EnumType operator~(EnumType a) {                                    \
		using UT = std::underlying_type_t<EnumType>;                           \
		return static_cast<EnumType>(~static_cast<UT>(a));                     \
	}                                                                          \
	inline EnumType& operator|=(EnumType& a, EnumType b) {                     \
		return a = a | b;                                                      \
	}                                                                          \
	inline EnumType& operator&=(EnumType& a, EnumType b) {                     \
		return a = a & b;                                                      \
	}                                                                          \
	inline EnumType& operator^=(EnumType& a, EnumType b) {                     \
		return a = a ^ b;                                                      \
	}

namespace ptgn {

template <ptgn::Enum Enum>
class Flags {
public:
	using underlying = std::underlying_type_t<Enum>;

	Flags() = default;

	explicit Flags(underlying bits) : bits_{ bits } {}

	explicit Flags(Enum flag) : bits_{ static_cast<underlying>(flag) } {}

	bool operator==(const Flags&) const = default;

	void Set(Enum flag) {
		bits_ |= static_cast<underlying>(flag);
	}

	void Clear(Enum flag) {
		bits_ &= ~static_cast<underlying>(flag);
	}

	void Toggle(Enum flag) {
		bits_ ^= static_cast<underlying>(flag);
	}

	[[nodiscard]] bool IsSet(Enum flag) const {
		return (bits_ & static_cast<underlying>(flag)) != 0;
	}

	void ClearAll() {
		bits_ = 0;
	}

	// @return True if any of the bits are set.
	[[nodiscard]] bool AnySet() const {
		return bits_ != 0;
	}

	[[nodiscard]] underlying GetBits() const {
		return bits_;
	}

	void SetBits(underlying bits) {
		bits_ = bits;
	}

	PTGN_SERIALIZER_REGISTER_NAMELESS(Flags<Enum>, bits_)

private:
	underlying bits_{ 0 };
};

} // namespace ptgn