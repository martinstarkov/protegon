#pragma once

#define PTGN_EXPAND(x)	  x
#define PTGN_STRINGIFY(x) #x

#define PTGN_EXPAND(x) x
#define PTGN_DEF_AUX_NARGS(                                                                        \
	x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20,     \
	x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, \
	x40, x41, x42, x43, x44, x45, x46, x47, x48, x49, x50, x51, x52, x53, x54, x55, x56, x57, x58, \
	x59, x60, x61, x62, x63, x64, VAL, ...                                                         \
)                                                                                                  \
	VAL
#define PTGN_NARGS(...)                                                                          \
	PTGN_EXPAND(PTGN_DEF_AUX_NARGS(                                                              \
		__VA_ARGS__, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, \
		45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24,  \
		23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0     \
	))

// --------------------------------------------------
#define PTGN_FE0_1(what, x)		 PTGN_EXPAND(what(x))
#define PTGN_FE0_2(what, x, ...) PTGN_EXPAND(what(x) PTGN_FE0_1(what, __VA_ARGS__))
#define PTGN_FE0_3(what, x, ...) PTGN_EXPAND(what(x) PTGN_FE0_2(what, __VA_ARGS__))
#define PTGN_FE0_4(what, x, ...) PTGN_EXPAND(what(x) PTGN_FE0_3(what, __VA_ARGS__))
#define PTGN_FE0_5(what, x, ...) PTGN_EXPAND(what(x) PTGN_FE0_4(what, __VA_ARGS__))
#define PTGN_FE0_6(what, x, ...) PTGN_EXPAND(what(x) PTGN_FE0_5(what, __VA_ARGS__))
#define PTGN_FE0_7(what, x, ...) PTGN_EXPAND(what(x) PTGN_FE0_6(what, __VA_ARGS__))
#define PTGN_FE0_8(what, x, ...) PTGN_EXPAND(what(x) PTGN_FE0_7(what, __VA_ARGS__))

#define PTGN_REPEAT0_(...)                                                                   \
	PTGN_EXPAND(PTGN_DEF_AUX_NARGS(                                                          \
		__VA_ARGS__, PTGN_FE0_8, PTGN_FE0_7, PTGN_FE0_6, PTGN_FE0_5, PTGN_FE0_4, PTGN_FE0_3, \
		PTGN_FE0_2, PTGN_FE0_1, 0                                                            \
	))
#define PTGN_FOR_EACH(what, ...) PTGN_EXPAND(PTGN_REPEAT0_(__VA_ARGS__)(what, __VA_ARGS__))

// --------------------------------------------------
#define PTGN_FE1_1(what, x, y)		PTGN_EXPAND(what(x, y))
#define PTGN_FE1_2(what, x, y, ...) PTGN_EXPAND(what(x, y) PTGN_FE1_1(what, x, __VA_ARGS__))
#define PTGN_FE1_3(what, x, y, ...) PTGN_EXPAND(what(x, y) PTGN_FE1_2(what, x, __VA_ARGS__))
#define PTGN_FE1_4(what, x, y, ...) PTGN_EXPAND(what(x, y) PTGN_FE1_3(what, x, __VA_ARGS__))
#define PTGN_FE1_5(what, x, y, ...) PTGN_EXPAND(what(x, y) PTGN_FE1_4(what, x, __VA_ARGS__))
#define PTGN_FE1_6(what, x, y, ...) PTGN_EXPAND(what(x, y) PTGN_FE1_5(what, x, __VA_ARGS__))
#define PTGN_FE1_7(what, x, y, ...) PTGN_EXPAND(what(x, y) PTGN_FE1_6(what, x, __VA_ARGS__))
#define PTGN_FE1_8(what, x, y, ...) PTGN_EXPAND(what(x, y) PTGN_FE1_7(what, x, __VA_ARGS__))

#define PTGN_REPEAT1_(...)                                                                   \
	PTGN_EXPAND(PTGN_DEF_AUX_NARGS(                                                          \
		__VA_ARGS__, PTGN_FE1_8, PTGN_FE1_7, PTGN_FE1_6, PTGN_FE1_5, PTGN_FE1_4, PTGN_FE1_3, \
		PTGN_FE1_2, PTGN_FE1_1, 0                                                            \
	))
#define PTGN_FOR_EACH_PIVOT_1ST_ARG(what, arg0, ...) \
	PTGN_EXPAND(PTGN_REPEAT1_(__VA_ARGS__)(what, arg0, __VA_ARGS__))

// --------------------------------------------------
#define PTGN_APPLY1(FN, x, y)		PTGN_EXPAND(FN(x, y))
#define PTGN_APPLY2(FN, x, y, ...)	PTGN_EXPAND(FN(x, y) PTGN_APPLY1(FN, __VA_ARGS__))
#define PTGN_APPLY3(FN, x, y, ...)	PTGN_EXPAND(FN(x, y) PTGN_APPLY2(FN, __VA_ARGS__))
#define PTGN_APPLY4(FN, x, y, ...)	PTGN_EXPAND(FN(x, y) PTGN_APPLY3(FN, __VA_ARGS__))
#define PTGN_APPLY5(FN, x, y, ...)	PTGN_EXPAND(FN(x, y) PTGN_APPLY4(FN, __VA_ARGS__))
#define PTGN_APPLY6(FN, x, y, ...)	PTGN_EXPAND(FN(x, y) PTGN_APPLY5(FN, __VA_ARGS__))
#define PTGN_APPLY7(FN, x, y, ...)	PTGN_EXPAND(FN(x, y) PTGN_APPLY6(FN, __VA_ARGS__))
#define PTGN_APPLY8(FN, x, y, ...)	PTGN_EXPAND(FN(x, y) PTGN_APPLY7(FN, __VA_ARGS__))
#define PTGN_APPLY9(FN, x, y, ...)	PTGN_EXPAND(FN(x, y) PTGN_APPLY8(FN, __VA_ARGS__))
#define PTGN_APPLY10(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY9(FN, __VA_ARGS__))
#define PTGN_APPLY11(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY10(FN, __VA_ARGS__))
#define PTGN_APPLY12(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY11(FN, __VA_ARGS__))
#define PTGN_APPLY13(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY12(FN, __VA_ARGS__))
#define PTGN_APPLY14(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY13(FN, __VA_ARGS__))
#define PTGN_APPLY15(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY14(FN, __VA_ARGS__))
#define PTGN_APPLY16(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY15(FN, __VA_ARGS__))
#define PTGN_APPLY17(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY16(FN, __VA_ARGS__))
#define PTGN_APPLY18(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY17(FN, __VA_ARGS__))
#define PTGN_APPLY19(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY18(FN, __VA_ARGS__))
#define PTGN_APPLY20(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY19(FN, __VA_ARGS__))
#define PTGN_APPLY21(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY20(FN, __VA_ARGS__))
#define PTGN_APPLY22(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY21(FN, __VA_ARGS__))
#define PTGN_APPLY23(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY22(FN, __VA_ARGS__))
#define PTGN_APPLY24(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY23(FN, __VA_ARGS__))
#define PTGN_APPLY25(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY24(FN, __VA_ARGS__))
#define PTGN_APPLY26(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY25(FN, __VA_ARGS__))
#define PTGN_APPLY27(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY26(FN, __VA_ARGS__))
#define PTGN_APPLY28(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY27(FN, __VA_ARGS__))
#define PTGN_APPLY29(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY28(FN, __VA_ARGS__))
#define PTGN_APPLY30(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY29(FN, __VA_ARGS__))
#define PTGN_APPLY31(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY30(FN, __VA_ARGS__))
#define PTGN_APPLY32(FN, x, y, ...) PTGN_EXPAND(FN(x, y) PTGN_APPLY31(FN, __VA_ARGS__))

#define PTGN_NPAIRARGS(...)                                                                      \
	PTGN_EXPAND(PTGN_DEF_AUX_NARGS(                                                              \
		__VA_ARGS__, 32, 32, 31, 31, 30, 30, 29, 29, 28, 28, 27, 27, 26, 26, 25, 25, 24, 24, 23, \
		23, 22, 22, 21, 21, 20, 20, 19, 19, 18, 18, 17, 17, 16, 16, 15, 15, 14, 14, 13, 13, 12,  \
		12, 11, 11, 10, 10, 9, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1                 \
	))
#define PTGN_APPLYNPAIRARGS(...)                                                                   \
	PTGN_EXPAND(PTGN_DEF_AUX_NARGS(                                                                \
		__VA_ARGS__, PTGN_APPLY32, PTGN_APPLY32, PTGN_APPLY31, PTGN_APPLY31, PTGN_APPLY30,         \
		PTGN_APPLY30, PTGN_APPLY29, PTGN_APPLY29, PTGN_APPLY28, PTGN_APPLY28, PTGN_APPLY27,        \
		PTGN_APPLY27, PTGN_APPLY26, PTGN_APPLY26, PTGN_APPLY25, PTGN_APPLY25, PTGN_APPLY24,        \
		PTGN_APPLY24, PTGN_APPLY23, PTGN_APPLY23, PTGN_APPLY22, PTGN_APPLY22, PTGN_APPLY21,        \
		PTGN_APPLY21, PTGN_APPLY20, PTGN_APPLY20, PTGN_APPLY19, PTGN_APPLY19, PTGN_APPLY18,        \
		PTGN_APPLY18, PTGN_APPLY17, PTGN_APPLY17, PTGN_APPLY16, PTGN_APPLY16, PTGN_APPLY15,        \
		PTGN_APPLY15, PTGN_APPLY14, PTGN_APPLY14, PTGN_APPLY13, PTGN_APPLY13, PTGN_APPLY12,        \
		PTGN_APPLY12, PTGN_APPLY11, PTGN_APPLY11, PTGN_APPLY10, PTGN_APPLY10, PTGN_APPLY9,         \
		PTGN_APPLY9, PTGN_APPLY8, PTGN_APPLY8, PTGN_APPLY7, PTGN_APPLY7, PTGN_APPLY6, PTGN_APPLY6, \
		PTGN_APPLY5, PTGN_APPLY5, PTGN_APPLY4, PTGN_APPLY4, PTGN_APPLY3, PTGN_APPLY3, PTGN_APPLY2, \
		PTGN_APPLY2, PTGN_APPLY1, PTGN_APPLY1                                                      \
	))
#define PTGN_FOR_EACH_PAIR(FN, ...) PTGN_EXPAND(PTGN_APPLYNPAIRARGS(__VA_ARGS__)(FN, __VA_ARGS__))
