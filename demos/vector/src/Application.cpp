#include "utility/Log.h"
#include "math/Vector2.h"

#include <vector>
#include <string>

using namespace ptgn;
using namespace math;

struct MyCustom {
	int i{};
};

int main(int c, char** v) {
	Vector2<int> t2;
	Vector2<double> t1{ 0, 1 };
	Vector2<MyCustom> test;
	static_assert(!tt::is_narrowing_conversion_v<std::int8_t, std::int16_t>);
	static_assert(!tt::is_narrowing_conversion_v<std::uint8_t, std::int16_t>);
	static_assert(!tt::is_narrowing_conversion_v<float, double>);
	static_assert(tt::is_narrowing_conversion_v<double, float>);
	static_assert(tt::is_narrowing_conversion_v<int, uint32_t>);
	static_assert(!tt::is_narrowing_conversion_v<int, double>);
}