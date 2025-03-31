#pragma once

#define PTGN_HAS_TEMPLATE_FUNCTION(NAME)                                                           \
	namespace ptgn {                                                                               \
	namespace impl {                                                                               \
	template <typename T>                                                                          \
	struct has_template_function_##NAME {                                                          \
		template <typename U>                                                                      \
		static auto check(U* ptr                                                                   \
		) -> decltype(ptr->template NAME<int>(std::declval<int&>()), std::true_type{});            \
		template <typename U>                                                                      \
		static std::false_type check(...);                                                         \
		static constexpr bool value = std::is_same_v<decltype(check<T>(nullptr)), std::true_type>; \
	};                                                                                             \
	}                                                                                              \
	template <typename T>                                                                          \
	inline constexpr bool has_template_function_##NAME##_v{                                        \
		impl::has_template_function_##NAME<std::decay_t<T>>::value                                 \
	};                                                                                             \
	}

#define PTGN_HAS_MEMBER_FUNCTION(NAME)                                        \
	namespace ptgn {                                                          \
	namespace impl {                                                          \
	template <class Class, typename Type = void>                              \
	struct has_member_function_##NAME {                                       \
		typedef char (&yes)[2];                                               \
		template <unsigned long>                                              \
		struct exists;                                                        \
		template <typename V>                                                 \
		static yes Check(exists<sizeof(static_cast<Type>(&V::NAME))>*);       \
		template <typename>                                                   \
		static char Check(...);                                               \
		static const bool value = (sizeof(Check<Class>(0)) == sizeof(yes));   \
	};                                                                        \
	template <class Class>                                                    \
	struct has_member_function_##NAME<Class, void> {                          \
		typedef char (&yes)[2];                                               \
		template <unsigned long>                                              \
		struct exists;                                                        \
		template <typename V>                                                 \
		static yes Check(exists<sizeof(&V::NAME)>*);                          \
		template <typename>                                                   \
		static char Check(...);                                               \
		static const bool value = (sizeof(Check<Class>(0)) == sizeof(yes));   \
	};                                                                        \
	}                                                                         \
	template <typename T, typename FunctionPtr>                               \
	inline constexpr bool has_member_function_##NAME_v{                       \
		impl::has_member_function_##NAME<std::decay_t<T>, FunctionPtr>::value \
	};                                                                        \
	}

#define PTGN_EXPAND(x)	  x
#define PTGN_STRINGIFY(x) #x

// Source: https://github.com/Erlkoenig90/map-macro/tree/msvc_varmacro_fix

#define PTGN_EVAL0(...) __VA_ARGS__
#define PTGN_EVAL1(...) PTGN_EVAL0(PTGN_EVAL0(PTGN_EVAL0(__VA_ARGS__)))
#define PTGN_EVAL2(...) PTGN_EVAL1(PTGN_EVAL1(PTGN_EVAL1(__VA_ARGS__)))
#define PTGN_EVAL3(...) PTGN_EVAL2(PTGN_EVAL2(PTGN_EVAL2(__VA_ARGS__)))
#define PTGN_EVAL4(...) PTGN_EVAL3(PTGN_EVAL3(PTGN_EVAL3(__VA_ARGS__)))
#define PTGN_EVAL5(...) PTGN_EVAL4(PTGN_EVAL4(PTGN_EVAL4(__VA_ARGS__)))

#ifdef _MSC_VER
// MSVC needs more evaluations
#define PTGN_EVAL6(...) PTGN_EVAL5(PTGN_EVAL5(PTGN_EVAL5(__VA_ARGS__)))
#define PTGN_EVAL(...)	PTGN_EVAL6(PTGN_EVAL6(__VA_ARGS__))
#else
#define PTGN_EVAL(...) PTGN_EVAL5(__VA_ARGS__)
#endif

#define PTGN_MAP_END(...)
#define PTGN_MAP_OUT

#define PTGN_EMPTY()
#define PTGN_DEFER(id) id PTGN_EMPTY()

#define PTGN_MAP_GET_END2()				0, PTGN_MAP_END
#define PTGN_MAP_GET_END1(...)			PTGN_MAP_GET_END2
#define PTGN_MAP_GET_END(...)			PTGN_MAP_GET_END1
#define PTGN_MAP_NEXT0(test, next, ...) next PTGN_MAP_OUT
#define PTGN_MAP_NEXT1(test, next)		PTGN_DEFER(PTGN_MAP_NEXT0)(test, next, 0)
#define PTGN_MAP_NEXT(test, next)		PTGN_MAP_NEXT1(PTGN_MAP_GET_END test, next)
#define PTGN_MAP_INC(X)					PTGN_MAP_INC_##X

#define PTGN_MAP0(f, x, peek, ...) \
	f(x) PTGN_DEFER(PTGN_MAP_NEXT(peek, PTGN_MAP1))(f, peek, __VA_ARGS__)
#define PTGN_MAP1(f, x, peek, ...) \
	f(x) PTGN_DEFER(PTGN_MAP_NEXT(peek, PTGN_MAP0))(f, peek, __VA_ARGS__)

#define PTGN_MAP0_UD(f, userdata, x, peek, ...) \
	f(x, userdata) PTGN_DEFER(PTGN_MAP_NEXT(peek, PTGN_MAP1_UD))(f, userdata, peek, __VA_ARGS__)
#define PTGN_MAP1_UD(f, userdata, x, peek, ...) \
	f(x, userdata) PTGN_DEFER(PTGN_MAP_NEXT(peek, PTGN_MAP0_UD))(f, userdata, peek, __VA_ARGS__)

#define PTGN_MAP0_UD_I(f, userdata, index, x, peek, ...)                   \
	f(x, userdata, index) PTGN_DEFER(PTGN_MAP_NEXT(peek, PTGN_MAP1_UD_I))( \
		f, userdata, PTGN_MAP_INC(index), peek, __VA_ARGS__                \
	)
#define PTGN_MAP1_UD_I(f, userdata, index, x, peek, ...)                   \
	f(x, userdata, index) PTGN_DEFER(PTGN_MAP_NEXT(peek, PTGN_MAP0_UD_I))( \
		f, userdata, PTGN_MAP_INC(index), peek, __VA_ARGS__                \
	)

#define PTGN_MAP_LIST0(f, x, peek, ...) \
	, f(x) PTGN_DEFER(PTGN_MAP_NEXT(peek, PTGN_MAP_LIST1))(f, peek, __VA_ARGS__)
#define PTGN_MAP_LIST1(f, x, peek, ...) \
	, f(x) PTGN_DEFER(PTGN_MAP_NEXT(peek, PTGN_MAP_LIST0))(f, peek, __VA_ARGS__)
#define PTGN_MAP_LIST2(f, x, peek, ...) \
	f(x) PTGN_DEFER(PTGN_MAP_NEXT(peek, PTGN_MAP_LIST1))(f, peek, __VA_ARGS__)

#define PTGN_MAP_LIST0_UD(f, userdata, x, peek, ...) \
	, f(x, userdata)                                 \
		  PTGN_DEFER(PTGN_MAP_NEXT(peek, PTGN_MAP_LIST1_UD))(f, userdata, peek, __VA_ARGS__)
#define PTGN_MAP_LIST1_UD(f, userdata, x, peek, ...) \
	, f(x, userdata)                                 \
		  PTGN_DEFER(PTGN_MAP_NEXT(peek, PTGN_MAP_LIST0_UD))(f, userdata, peek, __VA_ARGS__)
#define PTGN_MAP_LIST2_UD(f, userdata, x, peek, ...) \
	f(x, userdata)                                   \
		PTGN_DEFER(PTGN_MAP_NEXT(peek, PTGN_MAP_LIST1_UD))(f, userdata, peek, __VA_ARGS__)

#define PTGN_MAP_LIST0_UD_I(f, userdata, index, x, peek, ...)                     \
	, f(x, userdata, index) PTGN_DEFER(PTGN_MAP_NEXT(peek, PTGN_MAP_LIST1_UD_I))( \
		  f, userdata, PTGN_MAP_INC(index), peek, __VA_ARGS__                     \
	  )
#define PTGN_MAP_LIST1_UD_I(f, userdata, index, x, peek, ...)                     \
	, f(x, userdata, index) PTGN_DEFER(PTGN_MAP_NEXT(peek, PTGN_MAP_LIST0_UD_I))( \
		  f, userdata, PTGN_MAP_INC(index), peek, __VA_ARGS__                     \
	  )
#define PTGN_MAP_LIST2_UD_I(f, userdata, index, x, peek, ...)                   \
	f(x, userdata, index) PTGN_DEFER(PTGN_MAP_NEXT(peek, PTGN_MAP_LIST0_UD_I))( \
		f, userdata, PTGN_MAP_INC(index), peek, __VA_ARGS__                     \
	)

/**
 * Applies the function macro `f` to each of the remaining parameters.
 */
#define PTGN_MAP(f, ...) PTGN_EVAL(PTGN_MAP1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/**
 * Applies the function macro `f` to each of the remaining parameters and
 * inserts commas between the results.
 */
#define PTGN_MAP_LIST(f, ...) PTGN_EVAL(PTGN_MAP_LIST2(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/**
 * Applies the function macro `f` to each of the remaining parameters and passes userdata as the
 * second parameter to each invocation, e.g. PTGN_MAP_UD(f, x, a, b, c) evaluates to f(a, x) f(b, x)
 * f(c, x)
 */
#define PTGN_MAP_DATA(f, userdata, ...) \
	PTGN_EVAL(PTGN_MAP1_UD(f, userdata, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/**
 * Applies the function macro `f` to each of the remaining parameters, inserts commas between the
 * results, and passes userdata as the second parameter to each invocation, e.g. PTGN_MAP_LIST_UD(f,
 * x, a, b, c) evaluates to f(a, x), f(b, x), f(c, x)
 */
#define PTGN_MAP_LIST_DATA(f, userdata, ...) \
	PTGN_EVAL(PTGN_MAP_LIST2_UD(f, userdata, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/**
 * Applies the function macro `f` to each of the remaining parameters, passes userdata as the second
 * parameter to each invocation, and the index of the invocation as the third parameter, e.g.
 * PTGN_MAP_UD_I(f, x, a, b, c) evaluates to f(a, x, 0) f(b, x, 1) f(c, x, 2)
 */
#define PTGN_MAP_DATA_INDEX(f, userdata, ...) \
	PTGN_EVAL(PTGN_MAP1_UD_I(f, userdata, 0, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/**
 * Applies the function macro `f` to each of the remaining parameters, inserts commas between the
 * results, passes userdata as the second parameter to each invocation, and the index of the
 * invocation as the third parameter, e.g. PTGN_MAP_LIST_UD_I(f, x, a, b, c) evaluates to f(a, x,
 * 0), f(b, x, 1), f(c, x, 2)
 */
#define PTGN_MAP_LIST_DATA_INDEX(f, userdata, ...) \
	PTGN_EVAL(PTGN_MAP_LIST2_UD_I(f, userdata, 0, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/*
 * Because the preprocessor can't do arithmetic that produces integer literals for the *_I macros,
 * we have to do it manually. Since the number of parameters is limited anyways, this is sufficient
 * for all cases. If extra PTGN_EVAL layers are added, these definitions have to be extended. This
 * is equivalent to the way Boost.preprocessor does it:
 * https://github.com/boostorg/preprocessor/blob/develop/include/boost/preprocessor/arithmetic/inc.hpp
 * The *_I macros could alternatively pass C expressions such as (0), (0+1), (0+1+1...) to the user
 * macro, but passing 0, 1, 2 ... allows the user to incorporate the index into C identifiers, e.g.
 * to define a function like test_##index () for each macro invocation.
 */
#define PTGN_MAP_INC_0	 1
#define PTGN_MAP_INC_1	 2
#define PTGN_MAP_INC_2	 3
#define PTGN_MAP_INC_3	 4
#define PTGN_MAP_INC_4	 5
#define PTGN_MAP_INC_5	 6
#define PTGN_MAP_INC_6	 7
#define PTGN_MAP_INC_7	 8
#define PTGN_MAP_INC_8	 9
#define PTGN_MAP_INC_9	 10
#define PTGN_MAP_INC_10	 11
#define PTGN_MAP_INC_11	 12
#define PTGN_MAP_INC_12	 13
#define PTGN_MAP_INC_13	 14
#define PTGN_MAP_INC_14	 15
#define PTGN_MAP_INC_15	 16
#define PTGN_MAP_INC_16	 17
#define PTGN_MAP_INC_17	 18
#define PTGN_MAP_INC_18	 19
#define PTGN_MAP_INC_19	 20
#define PTGN_MAP_INC_20	 21
#define PTGN_MAP_INC_21	 22
#define PTGN_MAP_INC_22	 23
#define PTGN_MAP_INC_23	 24
#define PTGN_MAP_INC_24	 25
#define PTGN_MAP_INC_25	 26
#define PTGN_MAP_INC_26	 27
#define PTGN_MAP_INC_27	 28
#define PTGN_MAP_INC_28	 29
#define PTGN_MAP_INC_29	 30
#define PTGN_MAP_INC_30	 31
#define PTGN_MAP_INC_31	 32
#define PTGN_MAP_INC_32	 33
#define PTGN_MAP_INC_33	 34
#define PTGN_MAP_INC_34	 35
#define PTGN_MAP_INC_35	 36
#define PTGN_MAP_INC_36	 37
#define PTGN_MAP_INC_37	 38
#define PTGN_MAP_INC_38	 39
#define PTGN_MAP_INC_39	 40
#define PTGN_MAP_INC_40	 41
#define PTGN_MAP_INC_41	 42
#define PTGN_MAP_INC_42	 43
#define PTGN_MAP_INC_43	 44
#define PTGN_MAP_INC_44	 45
#define PTGN_MAP_INC_45	 46
#define PTGN_MAP_INC_46	 47
#define PTGN_MAP_INC_47	 48
#define PTGN_MAP_INC_48	 49
#define PTGN_MAP_INC_49	 50
#define PTGN_MAP_INC_50	 51
#define PTGN_MAP_INC_51	 52
#define PTGN_MAP_INC_52	 53
#define PTGN_MAP_INC_53	 54
#define PTGN_MAP_INC_54	 55
#define PTGN_MAP_INC_55	 56
#define PTGN_MAP_INC_56	 57
#define PTGN_MAP_INC_57	 58
#define PTGN_MAP_INC_58	 59
#define PTGN_MAP_INC_59	 60
#define PTGN_MAP_INC_60	 61
#define PTGN_MAP_INC_61	 62
#define PTGN_MAP_INC_62	 63
#define PTGN_MAP_INC_63	 64
#define PTGN_MAP_INC_64	 65
#define PTGN_MAP_INC_65	 66
#define PTGN_MAP_INC_66	 67
#define PTGN_MAP_INC_67	 68
#define PTGN_MAP_INC_68	 69
#define PTGN_MAP_INC_69	 70
#define PTGN_MAP_INC_70	 71
#define PTGN_MAP_INC_71	 72
#define PTGN_MAP_INC_72	 73
#define PTGN_MAP_INC_73	 74
#define PTGN_MAP_INC_74	 75
#define PTGN_MAP_INC_75	 76
#define PTGN_MAP_INC_76	 77
#define PTGN_MAP_INC_77	 78
#define PTGN_MAP_INC_78	 79
#define PTGN_MAP_INC_79	 80
#define PTGN_MAP_INC_80	 81
#define PTGN_MAP_INC_81	 82
#define PTGN_MAP_INC_82	 83
#define PTGN_MAP_INC_83	 84
#define PTGN_MAP_INC_84	 85
#define PTGN_MAP_INC_85	 86
#define PTGN_MAP_INC_86	 87
#define PTGN_MAP_INC_87	 88
#define PTGN_MAP_INC_88	 89
#define PTGN_MAP_INC_89	 90
#define PTGN_MAP_INC_90	 91
#define PTGN_MAP_INC_91	 92
#define PTGN_MAP_INC_92	 93
#define PTGN_MAP_INC_93	 94
#define PTGN_MAP_INC_94	 95
#define PTGN_MAP_INC_95	 96
#define PTGN_MAP_INC_96	 97
#define PTGN_MAP_INC_97	 98
#define PTGN_MAP_INC_98	 99
#define PTGN_MAP_INC_99	 100
#define PTGN_MAP_INC_100 101
#define PTGN_MAP_INC_101 102
#define PTGN_MAP_INC_102 103
#define PTGN_MAP_INC_103 104
#define PTGN_MAP_INC_104 105
#define PTGN_MAP_INC_105 106
#define PTGN_MAP_INC_106 107
#define PTGN_MAP_INC_107 108
#define PTGN_MAP_INC_108 109
#define PTGN_MAP_INC_109 110
#define PTGN_MAP_INC_110 111
#define PTGN_MAP_INC_111 112
#define PTGN_MAP_INC_112 113
#define PTGN_MAP_INC_113 114
#define PTGN_MAP_INC_114 115
#define PTGN_MAP_INC_115 116
#define PTGN_MAP_INC_116 117
#define PTGN_MAP_INC_117 118
#define PTGN_MAP_INC_118 119
#define PTGN_MAP_INC_119 120
#define PTGN_MAP_INC_120 121
#define PTGN_MAP_INC_121 122
#define PTGN_MAP_INC_122 123
#define PTGN_MAP_INC_123 124
#define PTGN_MAP_INC_124 125
#define PTGN_MAP_INC_125 126
#define PTGN_MAP_INC_126 127
#define PTGN_MAP_INC_127 128
#define PTGN_MAP_INC_128 129
#define PTGN_MAP_INC_129 130
#define PTGN_MAP_INC_130 131
#define PTGN_MAP_INC_131 132
#define PTGN_MAP_INC_132 133
#define PTGN_MAP_INC_133 134
#define PTGN_MAP_INC_134 135
#define PTGN_MAP_INC_135 136
#define PTGN_MAP_INC_136 137
#define PTGN_MAP_INC_137 138
#define PTGN_MAP_INC_138 139
#define PTGN_MAP_INC_139 140
#define PTGN_MAP_INC_140 141
#define PTGN_MAP_INC_141 142
#define PTGN_MAP_INC_142 143
#define PTGN_MAP_INC_143 144
#define PTGN_MAP_INC_144 145
#define PTGN_MAP_INC_145 146
#define PTGN_MAP_INC_146 147
#define PTGN_MAP_INC_147 148
#define PTGN_MAP_INC_148 149
#define PTGN_MAP_INC_149 150
#define PTGN_MAP_INC_150 151
#define PTGN_MAP_INC_151 152
#define PTGN_MAP_INC_152 153
#define PTGN_MAP_INC_153 154
#define PTGN_MAP_INC_154 155
#define PTGN_MAP_INC_155 156
#define PTGN_MAP_INC_156 157
#define PTGN_MAP_INC_157 158
#define PTGN_MAP_INC_158 159
#define PTGN_MAP_INC_159 160
#define PTGN_MAP_INC_160 161
#define PTGN_MAP_INC_161 162
#define PTGN_MAP_INC_162 163
#define PTGN_MAP_INC_163 164
#define PTGN_MAP_INC_164 165
#define PTGN_MAP_INC_165 166
#define PTGN_MAP_INC_166 167
#define PTGN_MAP_INC_167 168
#define PTGN_MAP_INC_168 169
#define PTGN_MAP_INC_169 170
#define PTGN_MAP_INC_170 171
#define PTGN_MAP_INC_171 172
#define PTGN_MAP_INC_172 173
#define PTGN_MAP_INC_173 174
#define PTGN_MAP_INC_174 175
#define PTGN_MAP_INC_175 176
#define PTGN_MAP_INC_176 177
#define PTGN_MAP_INC_177 178
#define PTGN_MAP_INC_178 179
#define PTGN_MAP_INC_179 180
#define PTGN_MAP_INC_180 181
#define PTGN_MAP_INC_181 182
#define PTGN_MAP_INC_182 183
#define PTGN_MAP_INC_183 184
#define PTGN_MAP_INC_184 185
#define PTGN_MAP_INC_185 186
#define PTGN_MAP_INC_186 187
#define PTGN_MAP_INC_187 188
#define PTGN_MAP_INC_188 189
#define PTGN_MAP_INC_189 190
#define PTGN_MAP_INC_190 191
#define PTGN_MAP_INC_191 192
#define PTGN_MAP_INC_192 193
#define PTGN_MAP_INC_193 194
#define PTGN_MAP_INC_194 195
#define PTGN_MAP_INC_195 196
#define PTGN_MAP_INC_196 197
#define PTGN_MAP_INC_197 198
#define PTGN_MAP_INC_198 199
#define PTGN_MAP_INC_199 200
#define PTGN_MAP_INC_200 201
#define PTGN_MAP_INC_201 202
#define PTGN_MAP_INC_202 203
#define PTGN_MAP_INC_203 204
#define PTGN_MAP_INC_204 205
#define PTGN_MAP_INC_205 206
#define PTGN_MAP_INC_206 207
#define PTGN_MAP_INC_207 208
#define PTGN_MAP_INC_208 209
#define PTGN_MAP_INC_209 210
#define PTGN_MAP_INC_210 211
#define PTGN_MAP_INC_211 212
#define PTGN_MAP_INC_212 213
#define PTGN_MAP_INC_213 214
#define PTGN_MAP_INC_214 215
#define PTGN_MAP_INC_215 216
#define PTGN_MAP_INC_216 217
#define PTGN_MAP_INC_217 218
#define PTGN_MAP_INC_218 219
#define PTGN_MAP_INC_219 220
#define PTGN_MAP_INC_220 221
#define PTGN_MAP_INC_221 222
#define PTGN_MAP_INC_222 223
#define PTGN_MAP_INC_223 224
#define PTGN_MAP_INC_224 225
#define PTGN_MAP_INC_225 226
#define PTGN_MAP_INC_226 227
#define PTGN_MAP_INC_227 228
#define PTGN_MAP_INC_228 229
#define PTGN_MAP_INC_229 230
#define PTGN_MAP_INC_230 231
#define PTGN_MAP_INC_231 232
#define PTGN_MAP_INC_232 233
#define PTGN_MAP_INC_233 234
#define PTGN_MAP_INC_234 235
#define PTGN_MAP_INC_235 236
#define PTGN_MAP_INC_236 237
#define PTGN_MAP_INC_237 238
#define PTGN_MAP_INC_238 239
#define PTGN_MAP_INC_239 240
#define PTGN_MAP_INC_240 241
#define PTGN_MAP_INC_241 242
#define PTGN_MAP_INC_242 243
#define PTGN_MAP_INC_243 244
#define PTGN_MAP_INC_244 245
#define PTGN_MAP_INC_245 246
#define PTGN_MAP_INC_246 247
#define PTGN_MAP_INC_247 248
#define PTGN_MAP_INC_248 249
#define PTGN_MAP_INC_249 250
#define PTGN_MAP_INC_250 251
#define PTGN_MAP_INC_251 252
#define PTGN_MAP_INC_252 253
#define PTGN_MAP_INC_253 254
#define PTGN_MAP_INC_254 255
#define PTGN_MAP_INC_255 256
#define PTGN_MAP_INC_256 257
#define PTGN_MAP_INC_257 258
#define PTGN_MAP_INC_258 259
#define PTGN_MAP_INC_259 260
#define PTGN_MAP_INC_260 261
#define PTGN_MAP_INC_261 262
#define PTGN_MAP_INC_262 263
#define PTGN_MAP_INC_263 264
#define PTGN_MAP_INC_264 265
#define PTGN_MAP_INC_265 266
#define PTGN_MAP_INC_266 267
#define PTGN_MAP_INC_267 268
#define PTGN_MAP_INC_268 269
#define PTGN_MAP_INC_269 270
#define PTGN_MAP_INC_270 271
#define PTGN_MAP_INC_271 272
#define PTGN_MAP_INC_272 273
#define PTGN_MAP_INC_273 274
#define PTGN_MAP_INC_274 275
#define PTGN_MAP_INC_275 276
#define PTGN_MAP_INC_276 277
#define PTGN_MAP_INC_277 278
#define PTGN_MAP_INC_278 279
#define PTGN_MAP_INC_279 280
#define PTGN_MAP_INC_280 281
#define PTGN_MAP_INC_281 282
#define PTGN_MAP_INC_282 283
#define PTGN_MAP_INC_283 284
#define PTGN_MAP_INC_284 285
#define PTGN_MAP_INC_285 286
#define PTGN_MAP_INC_286 287
#define PTGN_MAP_INC_287 288
#define PTGN_MAP_INC_288 289
#define PTGN_MAP_INC_289 290
#define PTGN_MAP_INC_290 291
#define PTGN_MAP_INC_291 292
#define PTGN_MAP_INC_292 293
#define PTGN_MAP_INC_293 294
#define PTGN_MAP_INC_294 295
#define PTGN_MAP_INC_295 296
#define PTGN_MAP_INC_296 297
#define PTGN_MAP_INC_297 298
#define PTGN_MAP_INC_298 299
#define PTGN_MAP_INC_299 300
#define PTGN_MAP_INC_300 301
#define PTGN_MAP_INC_301 302
#define PTGN_MAP_INC_302 303
#define PTGN_MAP_INC_303 304
#define PTGN_MAP_INC_304 305
#define PTGN_MAP_INC_305 306
#define PTGN_MAP_INC_306 307
#define PTGN_MAP_INC_307 308
#define PTGN_MAP_INC_308 309
#define PTGN_MAP_INC_309 310
#define PTGN_MAP_INC_310 311
#define PTGN_MAP_INC_311 312
#define PTGN_MAP_INC_312 313
#define PTGN_MAP_INC_313 314
#define PTGN_MAP_INC_314 315
#define PTGN_MAP_INC_315 316
#define PTGN_MAP_INC_316 317
#define PTGN_MAP_INC_317 318
#define PTGN_MAP_INC_318 319
#define PTGN_MAP_INC_319 320
#define PTGN_MAP_INC_320 321
#define PTGN_MAP_INC_321 322
#define PTGN_MAP_INC_322 323
#define PTGN_MAP_INC_323 324
#define PTGN_MAP_INC_324 325
#define PTGN_MAP_INC_325 326
#define PTGN_MAP_INC_326 327
#define PTGN_MAP_INC_327 328
#define PTGN_MAP_INC_328 329
#define PTGN_MAP_INC_329 330
#define PTGN_MAP_INC_330 331
#define PTGN_MAP_INC_331 332
#define PTGN_MAP_INC_332 333
#define PTGN_MAP_INC_333 334
#define PTGN_MAP_INC_334 335
#define PTGN_MAP_INC_335 336
#define PTGN_MAP_INC_336 337
#define PTGN_MAP_INC_337 338
#define PTGN_MAP_INC_338 339
#define PTGN_MAP_INC_339 340
#define PTGN_MAP_INC_340 341
#define PTGN_MAP_INC_341 342
#define PTGN_MAP_INC_342 343
#define PTGN_MAP_INC_343 344
#define PTGN_MAP_INC_344 345
#define PTGN_MAP_INC_345 346
#define PTGN_MAP_INC_346 347
#define PTGN_MAP_INC_347 348
#define PTGN_MAP_INC_348 349
#define PTGN_MAP_INC_349 350
#define PTGN_MAP_INC_350 351
#define PTGN_MAP_INC_351 352
#define PTGN_MAP_INC_352 353
#define PTGN_MAP_INC_353 354
#define PTGN_MAP_INC_354 355
#define PTGN_MAP_INC_355 356
#define PTGN_MAP_INC_356 357
#define PTGN_MAP_INC_357 358
#define PTGN_MAP_INC_358 359
#define PTGN_MAP_INC_359 360
#define PTGN_MAP_INC_360 361
#define PTGN_MAP_INC_361 362
#define PTGN_MAP_INC_362 363
#define PTGN_MAP_INC_363 364
#define PTGN_MAP_INC_364 365
#define PTGN_MAP_INC_365 366