#include "tests/test_ecs.h"
#include "test_math.h"
#include "test_vector2.h"
#include "test_rng.h"

int main(int c, char** v) {
	TestECS();
	TestMath();
	TestVector2();
	TestRNG();
	return 0;
}