#pragma once

#include "protegon/matrix4.h"
#include "protegon/log.h"
#include "protegon/debug.h"

using namespace ptgn;

bool TestMatrix4() {
	PTGN_INFO("Starting Matrix4 tests...");

	M4_int test1;
	PTGN_ASSERT(test1[0] == 0);
	PTGN_ASSERT(test1[1] == 0);
	PTGN_ASSERT(test1[2] == 0);
	PTGN_ASSERT(test1[3] == 0);
	PTGN_ASSERT(test1[4] == 0);
	PTGN_ASSERT(test1[5] == 0);
	PTGN_ASSERT(test1[6] == 0);
	PTGN_ASSERT(test1[7] == 0);
	PTGN_ASSERT(test1[8] == 0);
	PTGN_ASSERT(test1[9] == 0);
	PTGN_ASSERT(test1[10] == 0);
	PTGN_ASSERT(test1[11] == 0);
	PTGN_ASSERT(test1[12] == 0);
	PTGN_ASSERT(test1[13] == 0);
	PTGN_ASSERT(test1[14] == 0);
	PTGN_ASSERT(test1[15] == 0);

	PTGN_ASSERT(test1.IsZero());

	M4_float test2;
	PTGN_ASSERT(test2[0] == 0.0f);
	PTGN_ASSERT(test2[1] == 0.0f);
	PTGN_ASSERT(test2[2] == 0.0f);
	PTGN_ASSERT(test2[3] == 0.0f);
	PTGN_ASSERT(test2[4] == 0.0f);
	PTGN_ASSERT(test2[5] == 0.0f);
	PTGN_ASSERT(test2[6] == 0.0f);
	PTGN_ASSERT(test2[7] == 0.0f);
	PTGN_ASSERT(test2[8] == 0.0f);
	PTGN_ASSERT(test2[9] == 0.0f);
	PTGN_ASSERT(test2[10] == 0.0f);
	PTGN_ASSERT(test2[11] == 0.0f);
	PTGN_ASSERT(test2[12] == 0.0f);
	PTGN_ASSERT(test2[13] == 0.0f);
	PTGN_ASSERT(test2[14] == 0.0f);
	PTGN_ASSERT(test2[15] == 0.0f);

	PTGN_ASSERT(test2.IsZero());

	M4_int test3;

	test3(2, 1) = 6;
	test3(1, 3)	= 2;
	test3(3, 0) = 1;

	PTGN_ASSERT(test3 != test1);

	M4_int test4;

	test4(2, 1) = 6;
	test4(1, 3) = 2;
	test4(3, 0) = 1;

	PTGN_ASSERT(test3 == test4);

	M4_int test5;

	test5(0, 0) = 1;
	test5(1, 1) = 1;
	test5(2, 2) = 1;
	test5(3, 3) = 1;

	M4_int test6 = M4_int::Identity();

	PTGN_ASSERT(test5 == test6);

	M4_int test7;

	test7[2] = 7;
	test7[3] = 8;
	test7[4] = 9;

	M4_int test8;

	test8(2, 0) = 7;
	test8(3, 0) = 8;
	test8(0, 1) = 9;

	PTGN_ASSERT(test7 == test8);

	M4_int test9;

	test9(0, 1) = 1;
	test9(1, 1) = 1;
	test9(0, 2) = 1;
	test9(0, 3) = 1;
	test9(3, 3) = 1;

	M4_int test10;

	test10(0, 1) = 2;
	test10(1, 1) = 2;
	test10(0, 2) = 2;
	test10(0, 3) = 2;
	test10(3, 3) = 2;

	M4_int test11 = test9 + test10;

	PTGN_ASSERT(test11(0, 1) == 3);
	PTGN_ASSERT(test11(1, 1) == 3);
	PTGN_ASSERT(test11(0, 2) == 3);
	PTGN_ASSERT(test11(0, 3) == 3);
	PTGN_ASSERT(test11(3, 3) == 3);

	PTGN_ASSERT(test11(0, 0) == 0);
	PTGN_ASSERT(test11(1, 0) == 0);
	PTGN_ASSERT(test11(1, 2) == 0);
	PTGN_ASSERT(test11(1, 3) == 0);
	PTGN_ASSERT(test11(2, 0) == 0);
	PTGN_ASSERT(test11(2, 1) == 0);
	PTGN_ASSERT(test11(2, 2) == 0);
	PTGN_ASSERT(test11(2, 3) == 0);
	PTGN_ASSERT(test11(3, 0) == 0);
	PTGN_ASSERT(test11(3, 1) == 0);
	PTGN_ASSERT(test11(3, 2) == 0);

	M4_int test12;

	test12(0, 0) = 4;
	test12(1, 1) = 4;
	test12(2, 2) = 4;
	test12(3, 3) = 4;

	M4_int test13;

	test13(0, 1) = 5;
	test13(0, 2) = 5;
	test13(0, 3) = 5;
	test13(1, 0) = 5;
	test13(1, 2) = 5;
	test13(1, 3) = 5;
	test13(2, 0) = 5;
	test13(2, 1) = 5;
	test13(2, 3) = 5;
	test13(3, 0) = 5;
	test13(3, 1) = 5;
	test13(3, 2) = 5;

	M4_int test14 = test12 + test13;

	PTGN_ASSERT(test14(0, 0)  == 4);
	PTGN_ASSERT(test14(0, 1)  == 5);
	PTGN_ASSERT(test14(0, 2)  == 5);
	PTGN_ASSERT(test14(0, 3)  == 5);
	PTGN_ASSERT(test14(1, 0)  == 5);
	PTGN_ASSERT(test14(1, 1)  == 4);
	PTGN_ASSERT(test14(1, 2)  == 5);
	PTGN_ASSERT(test14(1, 3)  == 5);
	PTGN_ASSERT(test14(2, 0)  == 5);
	PTGN_ASSERT(test14(2, 1)  == 5);
	PTGN_ASSERT(test14(2, 2)  == 4);
	PTGN_ASSERT(test14(2, 3)  == 5);
	PTGN_ASSERT(test14(3, 0)  == 5);
	PTGN_ASSERT(test14(3, 1)  == 5);
	PTGN_ASSERT(test14(3, 2)  == 5);
	PTGN_ASSERT(test14(3, 3)  == 4);

	M4_float test15 = M4_float::Identity() - M4_float::Identity();

	PTGN_ASSERT(test15.IsZero());

	M4_int test16;

	test16.m = { 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15 };

	M4_int test17;

	test17.m = { 15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0 };

	M4_int test18 = test16 * test17;

	PTGN_ASSERT(test18(0, 0) == 34);
	PTGN_ASSERT(test18(0, 1) == 28);
	PTGN_ASSERT(test18(0, 2) == 22);
	PTGN_ASSERT(test18(0, 3) == 16);
	PTGN_ASSERT(test18(1, 0) == 178);
	PTGN_ASSERT(test18(1, 1) == 156);
	PTGN_ASSERT(test18(1, 2) == 134);
	PTGN_ASSERT(test18(1, 3) == 112);
	PTGN_ASSERT(test18(2, 0) == 322);
	PTGN_ASSERT(test18(2, 1) == 284);
	PTGN_ASSERT(test18(2, 2) == 246);
	PTGN_ASSERT(test18(2, 3) == 208);
	PTGN_ASSERT(test18(3, 0) == 466);
	PTGN_ASSERT(test18(3, 1) == 412);
	PTGN_ASSERT(test18(3, 2) == 358);
	PTGN_ASSERT(test18(3, 3) == 304);

	M4_int test19;
	test19.m = { -5, -1, 2, -3, -9, 6, 6, 3, -3, 9, -6, -3, 6, 2, 4, -2 };

	M4_int test20 = test19;
	PTGN_ASSERT(test20 == test19);

	M4_int test21 = test19 * test20;
	M4_int test22 = test20 * test19;

	PTGN_ASSERT(test21 == test22);

	PTGN_ASSERT(test21(0, 0) == 10);
	PTGN_ASSERT(test21(0, 1) == -9);
	PTGN_ASSERT(test21(0, 2) == -66);
	PTGN_ASSERT(test21(0, 3) == -72);
	PTGN_ASSERT(test21(1, 0) == 11);
	PTGN_ASSERT(test21(1, 1) == 105);
	PTGN_ASSERT(test21(1, 2) == -3);
	PTGN_ASSERT(test21(1, 3) == 38);
	PTGN_ASSERT(test21(2, 0) == -40);
	PTGN_ASSERT(test21(2, 1) == -6);
	PTGN_ASSERT(test21(2, 2) == 72);
	PTGN_ASSERT(test21(2, 3) == -8);
	PTGN_ASSERT(test21(3, 0) == 12);
	PTGN_ASSERT(test21(3, 1) == 21);
	PTGN_ASSERT(test21(3, 2) == 60);
	PTGN_ASSERT(test21(3, 3) == -20);

	M4_int test23;
	test23.m = { -8, -4, -7, 6, -6, -8, 9, -5, -3, -8, 8, -8, 4, 3, -8, -3 };
	M4_int test24;
	test24.m = { -5, -1, 2, -3, -9, 6, 6, 3, -3, 9, -6, -3, 6, 2, 4, -2 };

	M4_int test25 = test23 * test24;
	M4_int test26 = test24 * test23;

	PTGN_ASSERT(test25 != test26);

	PTGN_ASSERT(test25(0, 0) == 28);
	PTGN_ASSERT(test25(0, 1) == 30);
	PTGN_ASSERT(test25(0, 2) == -24);
	PTGN_ASSERT(test25(0, 3) == -80);
	PTGN_ASSERT(test25(1, 0) == 3);
	PTGN_ASSERT(test25(1, 1) == -51);
	PTGN_ASSERT(test25(1, 2) == -21);
	PTGN_ASSERT(test25(1, 3) == -78);
	PTGN_ASSERT(test25(2, 0) == 66);
	PTGN_ASSERT(test25(2, 1) == 141);
	PTGN_ASSERT(test25(2, 2) == 78);
	PTGN_ASSERT(test25(2, 3) == 24);
	PTGN_ASSERT(test25(3, 0) == -32);
	PTGN_ASSERT(test25(3, 1) == -141);
	PTGN_ASSERT(test25(3, 2) == -6);
	PTGN_ASSERT(test25(3, 3) == 0);

	PTGN_ASSERT(test26(0, 0) == 133);
	PTGN_ASSERT(test26(0, 1) == 45);
	PTGN_ASSERT(test26(0, 2) == 15);
	PTGN_ASSERT(test26(0, 3) == -41);
	PTGN_ASSERT(test26(1, 0) == -67);
	PTGN_ASSERT(test26(1, 1) == 29);
	PTGN_ASSERT(test26(1, 2) == 11);
	PTGN_ASSERT(test26(1, 3) == -64);
	PTGN_ASSERT(test26(2, 0) == 26);
	PTGN_ASSERT(test26(2, 1) == -134);
	PTGN_ASSERT(test26(2, 2) == -134);
	PTGN_ASSERT(test26(2, 3) == 62);
	PTGN_ASSERT(test26(3, 0) == 21);
	PTGN_ASSERT(test26(3, 1) == -23);
	PTGN_ASSERT(test26(3, 2) == -23);
	PTGN_ASSERT(test26(3, 3) == 27);

	PTGN_INFO("All Matrix4 tests passed!");
	return true;
}
