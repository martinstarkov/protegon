#pragma once

#include <cstdlib>

namespace ptgn {

enum class Key : std::size_t {
	K_0 = 39,				// SDL_SCANCODE_0
	K_1 = 30,				// SDL_SCANCODE_1
	K_2 = 31,				// SDL_SCANCODE_2
	K_3 = 32,				// SDL_SCANCODE_3
	K_4 = 33,				// SDL_SCANCODE_4
	K_5 = 34,				// SDL_SCANCODE_5
	K_6 = 35,				// SDL_SCANCODE_6
	K_7 = 36,				// SDL_SCANCODE_7
	K_8 = 37,				// SDL_SCANCODE_8
	K_9 = 38,				// SDL_SCANCODE_9

	ZERO  = 39,				// SDL_SCANCODE_0
	ONE	  = 30,				// SDL_SCANCODE_1
	TWO	  = 31,				// SDL_SCANCODE_2
	THREE = 32,				// SDL_SCANCODE_3
	FOUR  = 33,				// SDL_SCANCODE_4
	FIVE  = 34,				// SDL_SCANCODE_5
	SIX	  = 35,				// SDL_SCANCODE_6
	SEVEN = 36,				// SDL_SCANCODE_7
	EIGHT = 37,				// SDL_SCANCODE_8
	NINE  = 38,				// SDL_SCANCODE_9

	KP_0 = 98,				// SDL_SCANCODE_KP_0
	KP_1 = 89,				// SDL_SCANCODE_KP_1
	KP_2 = 90,				// SDL_SCANCODE_KP_2
	KP_3 = 91,				// SDL_SCANCODE_KP_3
	KP_4 = 92,				// SDL_SCANCODE_KP_4
	KP_5 = 93,				// SDL_SCANCODE_KP_5
	KP_6 = 94,				// SDL_SCANCODE_KP_6
	KP_7 = 95,				// SDL_SCANCODE_KP_7
	KP_8 = 96,				// SDL_SCANCODE_KP_8
	KP_9 = 97,				// SDL_SCANCODE_KP_9

	KP_ZERO	 = 98,			// SDL_SCANCODE_KP_0
	KP_ONE	 = 89,			// SDL_SCANCODE_KP_1
	KP_TWO	 = 90,			// SDL_SCANCODE_KP_2
	KP_THREE = 91,			// SDL_SCANCODE_KP_3
	KP_FOUR	 = 92,			// SDL_SCANCODE_KP_4
	KP_FIVE	 = 93,			// SDL_SCANCODE_KP_5
	KP_SIX	 = 94,			// SDL_SCANCODE_KP_6
	KP_SEVEN = 95,			// SDL_SCANCODE_KP_7
	KP_EIGHT = 96,			// SDL_SCANCODE_KP_8
	KP_NINE	 = 97,			// SDL_SCANCODE_KP_9

	KP_AMPERSAND	 = 199, // SDL_SCANCODE_KP_AMPERSAND
	KP_PERIOD		 = 99,	// SDL_SCANCODE_KP_PERIOD
	KP_DELETE		 = 99,	// SDL_SCANCODE_KP_PERIOD
	KP_ENTER		 = 88,	// SDL_SCANCODE_KP_ENTER
	KP_PLUS			 = 87,	// SDL_SCANCODE_KP_PLUS
	KP_MINUS		 = 86,	// SDL_SCANCODE_KP_MINUS
	KP_MULTIPLY		 = 85,	// SDL_SCANCODE_KP_MULTIPLY
	KP_STAR			 = 85,	// SDL_SCANCODE_KP_MULTIPLY
	KP_ASTERISK		 = 85,	// SDL_SCANCODE_KP_MULTIPLY
	KP_DIVIDE		 = 84,	// SDL_SCANCODE_KP_DIVIDE
	KP_SLASH		 = 84,	// SDL_SCANCODE_KP_DIVIDE
	KP_FORWARD_SLASH = 84,	// SDL_SCANCODE_KP_DIVIDE

	A = 4,					// SDL_SCANCODE_A
	B = 5,					// SDL_SCANCODE_B
	C = 6,					// SDL_SCANCODE_C
	D = 7,					// SDL_SCANCODE_D
	E = 8,					// SDL_SCANCODE_E
	F = 9,					// SDL_SCANCODE_F
	G = 10,					// SDL_SCANCODE_G
	H = 11,					// SDL_SCANCODE_H
	I = 12,					// SDL_SCANCODE_I
	J = 13,					// SDL_SCANCODE_J
	K = 14,					// SDL_SCANCODE_K
	L = 15,					// SDL_SCANCODE_L
	M = 16,					// SDL_SCANCODE_M
	N = 17,					// SDL_SCANCODE_N
	O = 18,					// SDL_SCANCODE_O
	P = 19,					// SDL_SCANCODE_P
	Q = 20,					// SDL_SCANCODE_Q
	R = 21,					// SDL_SCANCODE_R
	S = 22,					// SDL_SCANCODE_S
	T = 23,					// SDL_SCANCODE_T
	U = 24,					// SDL_SCANCODE_U
	V = 25,					// SDL_SCANCODE_V
	W = 26,					// SDL_SCANCODE_W
	X = 27,					// SDL_SCANCODE_X
	Y = 28,					// SDL_SCANCODE_Y
	Z = 29,					// SDL_SCANCODE_Z

	F1	= 58,				// SDL_SCANCODE_F1
	F2	= 59,				// SDL_SCANCODE_F2
	F3	= 60,				// SDL_SCANCODE_F3
	F4	= 61,				// SDL_SCANCODE_F4
	F5	= 62,				// SDL_SCANCODE_F5
	F6	= 63,				// SDL_SCANCODE_F6
	F7	= 64,				// SDL_SCANCODE_F7
	F8	= 65,				// SDL_SCANCODE_F8
	F9	= 66,				// SDL_SCANCODE_F9
	F10 = 67,				// SDL_SCANCODE_F10
	F11 = 68,				// SDL_SCANCODE_F11
	F12 = 69,				// SDL_SCANCODE_F12
	F13 = 104,				// SDL_SCANCODE_F13
	F14 = 105,				// SDL_SCANCODE_F14
	F15 = 106,				// SDL_SCANCODE_F15
	F16 = 107,				// SDL_SCANCODE_F16
	F17 = 108,				// SDL_SCANCODE_F17
	F18 = 109,				// SDL_SCANCODE_F18
	F19 = 110,				// SDL_SCANCODE_F19
	F20 = 111,				// SDL_SCANCODE_F20
	F21 = 112,				// SDL_SCANCODE_F21
	F22 = 113,				// SDL_SCANCODE_F22
	F23 = 114,				// SDL_SCANCODE_F23
	F24 = 115,				// SDL_SCANCODE_F24

	RIGHT = 79,				// SDL_SCANCODE_RIGHT
	LEFT  = 80,				// SDL_SCANCODE_LEFT
	DOWN  = 81,				// SDL_SCANCODE_DOWN
	UP	  = 82,				// SDL_SCANCODE_UP

	LEFT_ALT	= 226,		// SDL_SCANCODE_LALT
	ALT_GR		= 230,		// SDL_SCANCODE_RALT
	RIGHT_ALT	= 230,		// SDL_SCANCODE_RALT
	LEFT_CTRL	= 224,		// SDL_SCANCODE_LCTRL
	RIGHT_CTRL	= 228,		// SDL_SCANCODE_RCTRL
	LEFT_SHIFT	= 225,		// SDL_SCANCODE_LSHIFT
	RIGHT_SHIFT = 229,		// SDL_SCANCODE_RSHIFT

	BLANK	  = 44,			// SDL_SCANCODE_SPACE
	SPACE	  = 44,			// SDL_SCANCODE_SPACE
	SPACE_BAR = 44,			// SDL_SCANCODE_SPACE
	SPACEBAR  = 44,			// SDL_SCANCODE_SPACE

	ENTER	= 40,			// SDL_SCANCODE_RETURN
	RETURN	= 40,			// SDL_SCANCODE_RETURN
	RETURN2 = 158,			// SDL_SCANCODE_RETURN2

	HOME   = 74,			// SDL_SCANCODE_HOME
	END	   = 77,			// SDL_SCANCODE_END
	INSERT = 73,			// SDL_SCANCODE_INSERT
	DELETE = 76,			// SDL_SCANCODE_DELETE

	APOSTROPHE	  = 52,		// SDL_SCANCODE_APOSTROPHE
	COMMA		  = 54,		// SDL_SCANCODE_COMMA
	PERIOD		  = 55,		// SDL_SCANCODE_PERIOD
	SEMICOLON	  = 51,		// SDL_SCANCODE_SEMICOLON
	EQUALS		  = 46,		// SDL_SCANCODE_EQUALS
	TILDE		  = 53,		// SDL_SCANCODE_GRAVE
	GRAVE		  = 53,		// SDL_SCANCODE_GRAVE
	MINUS		  = 45,		// SDL_SCANCODE_MINUS
	FORWARD_SLASH = 56,		// SDL_SCANCODE_SLASH
	BACK_SLASH	  = 49,		// SDL_SCANCODE_BACKSLASH

	ESCAPE	  = 41,			// SDL_SCANCODE_ESCAPE
	BACKSPACE = 42,			// SDL_SCANCODE_BACKSPACE
	CAPSLOCK  = 57,			// SDL_SCANCODE_CAPSLOCK
	TAB		  = 43,			// SDL_SCANCODE_TAB

	WINDOWS = 231,			// SDL_SCANCODE_RGUI
	RGUI	= 231,			// SDL_SCANCODE_RGUI
	COMMAND = 231,			// SDL_SCANCODE_RGUI

	PRINT_SCREEN = 70,		// SDL_SCANCODE_PRINTSCREEN
	PRTSC		 = 70,		// SDL_SCANCODE_PRINTSCREEN
	NUMLOCK		 = 83,		// SDL_SCANCODE_NUMLOCKCLEAR
	KP_NUMLOCK	 = 83,		// SDL_SCANCODE_NUMLOCKCLEAR
	PAGE_DOWN	 = 78,		// SDL_SCANCODE_PAGEDOWN
	PAGE_UP		 = 75,		// SDL_SCANCODE_PAGEUP

	LEFT_BRACKET  = 47,		// SDL_SCANCODE_LEFTBRACKET
	RIGHT_BRACKET = 48,		// SDL_SCANCODE_RIGHTBRACKET
	SCROLL_LOCK	  = 71,		// SDL_SCANCODE_SCROLLLOCK
	SELECT		  = 119,	// SDL_SCANCODE_SELECT
	SEPARATOR	  = 159,	// SDL_SCANCODE_SEPARATOR
	SLEEP		  = 282		// SDL_SCANCODE_SLEEP
};

} // namespace ptgn