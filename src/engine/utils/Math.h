#pragma once

#include <cmath>
#include <type_traits>
#include <random>

namespace internal {

// source: https://stackoverflow.com/a/2450157
// typedef either to A or B, depending on what integer is passed.
template<int, typename A, typename B>
struct cond;

#define CCASE(N, typed) \
  template<typename A, typename B> \
  struct cond<N, A, B> { \
    typedef typed type; \
  }

CCASE(1, A); CCASE(2, B);
CCASE(3, int); CCASE(4, unsigned int);
CCASE(5, long); CCASE(6, unsigned long);
CCASE(7, float); CCASE(8, double);
CCASE(9, long double);

#undef CCASE

// for a better syntax...
template<typename T> struct identity { typedef T type; };

}

namespace engine {

namespace math {

template <typename Floating, std::enable_if_t<std::is_floating_point<Floating>::value, int> = 0>
Floating GetRandomValue(Floating min_range, Floating max_range) {
	std::random_device rd;
	std::mt19937 rng(rd());
	std::uniform_real_distribution<Floating> distribution(min_range, max_range);
	return distribution(rng);
}
template <typename Integer, std::enable_if_t<std::is_integral<Integer>::value, int> = 0>
Integer GetRandomValue(Integer min_range, Integer max_range) {
	std::random_device rd;
	std::mt19937 rng(rd());
	std::uniform_int_distribution<Integer> distribution(min_range, max_range);
	return distribution(rng);
}

// different type => figure out common type
template<typename A, typename B>
struct promote {
private:
	static A a;
	static B b;

	// in case A or B is a promoted arithmetic type, the template
	// will make it less preferred than the nontemplates below
	template<typename T>
	static internal::identity<char[1]>::type& check(A, T);
	template<typename T>
	static internal::identity<char[2]>::type& check(B, T);

	// "promoted arithmetic types"
	static internal::identity<char[3]>::type& check(int, int);
	static internal::identity<char[4]>::type& check(unsigned int, int);
	static internal::identity<char[5]>::type& check(long, int);
	static internal::identity<char[6]>::type& check(unsigned long, int);
	static internal::identity<char[7]>::type& check(float, int);
	static internal::identity<char[8]>::type& check(double, int);
	static internal::identity<char[9]>::type& check(long double, int);

public:
	typedef typename internal::cond<sizeof check(0 ? a : b, 0), A, B>::type
		type;
};

// same type => finished
template<typename A>
struct promote<A, A> {
	typedef A type;
};

template <typename ...Ts>
using is_number = std::enable_if_t<(std::is_arithmetic_v<Ts> && ...), int>;

template <typename T, typename S, is_number<T, S> = 0>
inline T RoundCast(S value) {
	return static_cast<T>(std::round(value));
}
template <typename T, is_number<T> = 0>
inline T Round(T value) {
	return static_cast<T>(std::round(value));
}
template <typename T, is_number<T> = 0>
inline T Floor(T value) {
	return static_cast<T>(std::floor(value));
}

// Find the sign of a numeric type
template <typename T>
inline int sgn(T val) {
	static_assert(std::is_arithmetic<T>::value, "sgn can only accept numeric types");
	return (T(0) < val) - (val < T(0));
}

} // namespace math

} // namespace engine