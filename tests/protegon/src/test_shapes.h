#pragma once

#include "protegon/polygon.h"
#include "protegon/circle.h"
#include "protegon/line.h"
#include "utility/debug.h"

using namespace ptgn;

bool TestShapes() {
	PTGN_INFO("Starting shape tests...");

	// Test Rectangle unions

	Rectangle<int> test11{ { 60, 20 }, { 40, 20 } };
	Rectangle<float> test12{ { 60.0f, 30.0f }, { 40.0f, 50.0f } };
	Rectangle<int> test13{ { 110, 10 }, { 50, 10 } };

	PTGN_ASSERT((test11.pos ==   V2_int{    60, 20 }));
	PTGN_ASSERT((test12.pos == V2_float{ 60.0f, 30.0f }));
	PTGN_ASSERT((test13.pos ==   V2_int{   110, 10 }));

	PTGN_ASSERT((test11.size ==   V2_int{    40, 20 }));
	PTGN_ASSERT((test12.size == V2_float{ 40.0f, 50.0f }));
	PTGN_ASSERT((test13.size ==   V2_int{    50, 10 }));

	PTGN_ASSERT(test11.x == 60    && test11.y == 20);
	PTGN_ASSERT(test12.x == 60.0f && test12.y == 30.0f);
	PTGN_ASSERT(test13.x == 110   && test13.y == 10);

	PTGN_ASSERT(test11.w == 40    && test11.h == 20);
	PTGN_ASSERT(test12.w == 40.0f && test12.h == 50.0f);
	PTGN_ASSERT(test13.w == 50    && test13.h == 10);
	
	PTGN_INFO("All shape tests passed!");
	return true;
}