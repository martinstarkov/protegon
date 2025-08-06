#pragma once

#include <cstdint>
#include <ostream>

#include "debug/log.h"
#include "serialization/enum.h"
#include "utility/platform.h"

namespace ptgn {

namespace impl {

enum class KeyState : std::uint8_t {
	Up		 = 1,
	Down	 = 2,
	Released = 3,
	Pressed	 = 4
};

} // namespace impl

enum class Key {
	Invalid = 0,		// SDL_SCANCODE_UNKNOWN

	K_0 = 39,			// Key 0 // SDL_SCANCODE_0
	K_1 = 30,			// Key 1 // SDL_SCANCODE_1
	K_2 = 31,			// Key 2 // SDL_SCANCODE_2
	K_3 = 32,			// Key 3 // SDL_SCANCODE_3
	K_4 = 33,			// Key 4 // SDL_SCANCODE_4
	K_5 = 34,			// Key 5 // SDL_SCANCODE_5
	K_6 = 35,			// Key 6 // SDL_SCANCODE_6
	K_7 = 36,			// Key 7 // SDL_SCANCODE_7
	K_8 = 37,			// Key 8 // SDL_SCANCODE_8
	K_9 = 38,			// Key 9 // SDL_SCANCODE_9

	Zero  = 39,			// Key 0 // SDL_SCANCODE_0
	One	  = 30,			// Key 1 // SDL_SCANCODE_1
	Two	  = 31,			// Key 2 // SDL_SCANCODE_2
	Three = 32,			// Key 3 // SDL_SCANCODE_3
	Four  = 33,			// Key 4 // SDL_SCANCODE_4
	Five  = 34,			// Key 5 // SDL_SCANCODE_5
	Six	  = 35,			// Key 6 // SDL_SCANCODE_6
	Seven = 36,			// Key 7 // SDL_SCANCODE_7
	Eight = 37,			// Key 8 // SDL_SCANCODE_8
	Nine  = 38,			// Key 9 // SDL_SCANCODE_9

	KP_0 = 98,			// Keypad Key 0 SDL_SCANCODE_KP_0
	KP_1 = 89,			// Keypad Key 1 SDL_SCANCODE_KP_1
	KP_2 = 90,			// Keypad Key 2 SDL_SCANCODE_KP_2
	KP_3 = 91,			// Keypad Key 3 SDL_SCANCODE_KP_3
	KP_4 = 92,			// Keypad Key 4 SDL_SCANCODE_KP_4
	KP_5 = 93,			// Keypad Key 5 SDL_SCANCODE_KP_5
	KP_6 = 94,			// Keypad Key 6 SDL_SCANCODE_KP_6
	KP_7 = 95,			// Keypad Key 7 SDL_SCANCODE_KP_7
	KP_8 = 96,			// Keypad Key 8 SDL_SCANCODE_KP_8
	KP_9 = 97,			// Keypad Key 9 SDL_SCANCODE_KP_9

	KP_Zero	 = 98,		// SDL_SCANCODE_KP_0
	KP_One	 = 89,		// SDL_SCANCODE_KP_1
	KP_Two	 = 90,		// SDL_SCANCODE_KP_2
	KP_Three = 91,		// SDL_SCANCODE_KP_3
	KP_Four	 = 92,		// SDL_SCANCODE_KP_4
	KP_Five	 = 93,		// SDL_SCANCODE_KP_5
	KP_Six	 = 94,		// SDL_SCANCODE_KP_6
	KP_Seven = 95,		// SDL_SCANCODE_KP_7
	KP_Eight = 96,		// SDL_SCANCODE_KP_8
	KP_Nine	 = 97,		// SDL_SCANCODE_KP_9

	KP_Ampersand = 199, // SDL_SCANCODE_KP_AMPERSAND
	KP_Period	 = 99,	// SDL_SCANCODE_KP_PERIOD
	KP_Delete	 = 99,	// SDL_SCANCODE_KP_PERIOD
	KP_Enter	 = 88,	// SDL_SCANCODE_KP_ENTER
	KP_Plus		 = 87,	// SDL_SCANCODE_KP_PLUS
	KP_Minus	 = 86,	// SDL_SCANCODE_KP_MINUS
	KP_Multiply	 = 85,	// SDL_SCANCODE_KP_MULTIPLY
	KP_Star		 = 85,	// SDL_SCANCODE_KP_MULTIPLY
	KP_Asterisk	 = 85,	// SDL_SCANCODE_KP_MULTIPLY
	KP_Divide	 = 84,	// SDL_SCANCODE_KP_DIVIDE
	KP_Slash	 = 84,	// SDL_SCANCODE_KP_SLASY

	A = 4,				// SDL_SCANCODE_A
	B = 5,				// SDL_SCANCODE_B
	C = 6,				// SDL_SCANCODE_C
	D = 7,				// SDL_SCANCODE_D
	E = 8,				// SDL_SCANCODE_E
	F = 9,				// SDL_SCANCODE_F
	G = 10,				// SDL_SCANCODE_G
	H = 11,				// SDL_SCANCODE_H
	I = 12,				// SDL_SCANCODE_I
	J = 13,				// SDL_SCANCODE_J
	K = 14,				// SDL_SCANCODE_K
	L = 15,				// SDL_SCANCODE_L
	M = 16,				// SDL_SCANCODE_M
	N = 17,				// SDL_SCANCODE_N
	O = 18,				// SDL_SCANCODE_O
	P = 19,				// SDL_SCANCODE_P
	Q = 20,				// SDL_SCANCODE_Q
	R = 21,				// SDL_SCANCODE_R
	S = 22,				// SDL_SCANCODE_S
	T = 23,				// SDL_SCANCODE_T
	U = 24,				// SDL_SCANCODE_U
	V = 25,				// SDL_SCANCODE_V
	W = 26,				// SDL_SCANCODE_W
	X = 27,				// SDL_SCANCODE_X
	Y = 28,				// SDL_SCANCODE_Y
	Z = 29,				// SDL_SCANCODE_Z

	F1	= 58,			// SDL_SCANCODE_F1
	F2	= 59,			// SDL_SCANCODE_F2
	F3	= 60,			// SDL_SCANCODE_F3
	F4	= 61,			// SDL_SCANCODE_F4
	F5	= 62,			// SDL_SCANCODE_F5
	F6	= 63,			// SDL_SCANCODE_F6
	F7	= 64,			// SDL_SCANCODE_F7
	F8	= 65,			// SDL_SCANCODE_F8
	F9	= 66,			// SDL_SCANCODE_F9
	F10 = 67,			// SDL_SCANCODE_F10
	F11 = 68,			// SDL_SCANCODE_F11
	F12 = 69,			// SDL_SCANCODE_F12
	F13 = 104,			// SDL_SCANCODE_F13
	F14 = 105,			// SDL_SCANCODE_F14
	F15 = 106,			// SDL_SCANCODE_F15
	F16 = 107,			// SDL_SCANCODE_F16
	F17 = 108,			// SDL_SCANCODE_F17
	F18 = 109,			// SDL_SCANCODE_F18
	F19 = 110,			// SDL_SCANCODE_F19
	F20 = 111,			// SDL_SCANCODE_F20
	F21 = 112,			// SDL_SCANCODE_F21
	F22 = 113,			// SDL_SCANCODE_F22
	F23 = 114,			// SDL_SCANCODE_F23
	F24 = 115,			// SDL_SCANCODE_F24

	Right = 79,			// SDL_SCANCODE_RIGHT
	Left  = 80,			// SDL_SCANCODE_LEFT
	Down  = 81,			// SDL_SCANCODE_DOWN
	Up	  = 82,			// SDL_SCANCODE_UP

	LeftAlt	   = 226,	// SDL_SCANCODE_LALT
	AltGr	   = 230,	// SDL_SCANCODE_RALT
	RightAlt   = 230,	// SDL_SCANCODE_RALT
	LeftCtrl   = 224,	// SDL_SCANCODE_LCTRL
	RightCtrl  = 228,	// SDL_SCANCODE_RCTRL
	LeftShift  = 225,	// SDL_SCANCODE_LSHIFT
	RightShift = 229,	// SDL_SCANCODE_RSHIFT

	Blank = 44,			// SDL_SCANCODE_SPACE
	Space = 44,			// SDL_SCANCODE_SPACE

	Enter	= 40,		// SDL_SCANCODE_RETURN
	Return	= 40,		// SDL_SCANCODE_RETURN
	Enter2	= 158,		// SDL_SCANCODE_RETURN2
	Return2 = 158,		// SDL_SCANCODE_RETURN2

	Home   = 74,		// SDL_SCANCODE_HOME
	End	   = 77,		// SDL_SCANCODE_END
	Insert = 73,		// SDL_SCANCODE_INSERT
	Delete = 76,		// SDL_SCANCODE_DELETE

	Apostrophe = 52,	// SDL_SCANCODE_APOSTROPHE
	Comma	   = 54,	// SDL_SCANCODE_COMMA
	Period	   = 55,	// SDL_SCANCODE_PERIOD
	Semicolon  = 51,	// SDL_SCANCODE_SEMICOLON
	Equals	   = 46,	// SDL_SCANCODE_EQUALS
	Tilde	   = 53,	// SDL_SCANCODE_GRAVE
	Grave	   = 53,	// SDL_SCANCODE_GRAVE
	Minus	   = 45,	// SDL_SCANCODE_MINUS
	Slash	   = 56,	// SDL_SCANCODE_SLASH
	Backslash  = 49,	// SDL_SCANCODE_BACKSLASH

	Escape	  = 41,		// SDL_SCANCODE_ESCAPE
	Backspace = 42,		// SDL_SCANCODE_BACKSPACE
	Capslock  = 57,		// SDL_SCANCODE_CAPSLOCK
	Tab		  = 43,		// SDL_SCANCODE_TAB

	Windows = 231,		// SDL_SCANCODE_RGUI
	RGUI	= 231,		// SDL_SCANCODE_RGUI
	Command = 231,		// SDL_SCANCODE_RGUI

	PrintScreen = 70,	// SDL_SCANCODE_PRINTSCREEN
	Numlock		= 83,	// SDL_SCANCODE_NUMLOCKCLEAR
	KP_Numlock	= 83,	// SDL_SCANCODE_NUMLOCKCLEAR
	PageDown	= 78,	// SDL_SCANCODE_PAGEDOWN
	PageUp		= 75,	// SDL_SCANCODE_PAGEUP

	LeftBracket	 = 47,	// SDL_SCANCODE_LEFTBRACKET
	RightBracket = 48,	// SDL_SCANCODE_RIGHTBRACKET
	ScrollLock	 = 71,	// SDL_SCANCODE_SCROLLLOCK
	Select		 = 119, // SDL_SCANCODE_SELECT
	Separator	 = 159, // SDL_SCANCODE_SEPARATOR
	Sleep		 = 282	// SDL_SCANCODE_SLEEP
};

inline std::ostream& operator<<(std::ostream& os, Key key) {
	// TODO: Consider switching to using if constexpr or an unordered_map.
	switch (key) {
		case Key::W:		  os << "W"; break;
		case Key::A:		  os << "A"; break;
		case Key::S:		  os << "S"; break;
		case Key::D:		  os << "D"; break;

		case Key::Right:	  os << "Right"; break;
		case Key::Left:		  os << "Left"; break;
		case Key::Down:		  os << "Down"; break;
		case Key::Up:		  os << "Up"; break;

		case Key::Space:	  os << "Space"; break;
		case Key::Escape:	  os << "Escape"; break;
		case Key::Enter:	  os << "Enter"; break;
		case Key::Tab:		  os << "Tab"; break;

		case Key::Q:		  os << "Q"; break;
		case Key::E:		  os << "E"; break;
		case Key::R:		  os << "R"; break;
		case Key::T:		  os << "T"; break;

		case Key::F:		  os << "F"; break;
		case Key::G:		  os << "G"; break;

		case Key::Z:		  os << "Z"; break;
		case Key::X:		  os << "X"; break;
		case Key::C:		  os << "C"; break;
		case Key::V:		  os << "V"; break;
		case Key::B:		  os << "B"; break;

		case Key::LeftAlt:	  os << "Left Alt"; break;
		case Key::RightAlt:	  os << "Right Alt"; break;
		case Key::LeftCtrl:	  os << "Left Ctrl"; break;
		case Key::RightCtrl:  os << "Right Ctrl"; break;
		case Key::LeftShift:  os << "Left Shift"; break;
		case Key::RightShift: os << "Right Shift"; break;

		case Key::H:		  os << "H"; break;
		case Key::I:		  os << "I"; break;
		case Key::J:		  os << "J"; break;
		case Key::K:		  os << "K"; break;
		case Key::L:		  os << "L"; break;
		case Key::M:		  os << "M"; break;
		case Key::N:		  os << "N"; break;
		case Key::O:		  os << "O"; break;
		case Key::P:		  os << "P"; break;
		case Key::U:		  os << "U"; break;
		case Key::Y:		  os << "Y"; break;

		case Key::K_0:		  os << "0"; break;
		case Key::K_1:		  os << "1"; break;
		case Key::K_2:		  os << "2"; break;
		case Key::K_3:		  os << "3"; break;
		case Key::K_4:		  os << "4"; break;
		case Key::K_5:		  os << "5"; break;
		case Key::K_6:		  os << "6"; break;
		case Key::K_7:		  os << "7"; break;
		case Key::K_8:		  os << "8"; break;
		case Key::K_9:		  os << "9"; break;

		case Key::F1:		  os << "F1"; break;
		case Key::F2:		  os << "F2"; break;
		case Key::F3:		  os << "F3"; break;
		case Key::F4:		  os << "F4"; break;
		case Key::F5:		  os << "F5"; break;
		case Key::F6:		  os << "F6"; break;
		case Key::F7:		  os << "F7"; break;
		case Key::F8:		  os << "F8"; break;
		case Key::F9:		  os << "F9"; break;
		case Key::F10:		  os << "F10"; break;
		case Key::F11:		  os << "F11"; break;
		case Key::F12:		  os << "F12"; break;
		case Key::F13:		  os << "F13"; break;
		case Key::F14:		  os << "F14"; break;
		case Key::F15:		  os << "F15"; break;
		case Key::F16:		  os << "F16"; break;
		case Key::F17:		  os << "F17"; break;
		case Key::F18:		  os << "F18"; break;
		case Key::F19:		  os << "F19"; break;
		case Key::F20:		  os << "F20"; break;
		case Key::F21:		  os << "F21"; break;
		case Key::F22:		  os << "F22"; break;
		case Key::F23:		  os << "F23"; break;
		case Key::F24:		  os << "F24"; break;

		case Key::Enter2:	  os << "Enter2"; break;
		case Key::Home:		  os << "Home"; break;
		case Key::End:		  os << "End"; break;
		case Key::Insert:	  os << "Insert"; break;
		case Key::Delete:	  os << "Delete"; break;

		case Key::Apostrophe: os << "'"; break;
		case Key::Comma:	  os << ","; break;
		case Key::Period:	  os << "."; break;
		case Key::Semicolon:  os << ";"; break;
		case Key::Equals:	  os << "="; break;
		case Key::Grave:	  os << "`"; break;
		case Key::Minus:	  os << "-"; break;
		case Key::Slash:	  os << "/"; break;
		case Key::Backslash:  os << "\\"; break;

		case Key::Backspace:  os << "Backspace"; break;
		case Key::Capslock:	  os << "Capslock"; break;

#if defined(PTGN_PLATFORM_WINDOWS)
		case Key::Windows: os << "Windows"; break;
#elif defined(PTGN_PLATFORM_MACOS) || defined(PTGN_PLATFORM_LINUX)
		case Key::Command: os << "Command"; break;
#else
		case Key::RGUI: os << "RGUI"; break;
#endif

		case Key::PrintScreen:	os << "PrintScreen"; break;
		case Key::PageDown:		os << "PageDown"; break;
		case Key::PageUp:		os << "PageUp"; break;
		case Key::LeftBracket:	os << "LeftBracket"; break;
		case Key::RightBracket: os << "RightBracket"; break;
		case Key::ScrollLock:	os << "ScrollLock"; break;
		case Key::Select:		os << "Select"; break;
		case Key::Separator:	os << "Separator"; break;
		case Key::Sleep:		os << "Sleep"; break;

		case Key::Numlock:		os << "Numlock"; break;

		case Key::KP_0:			os << "Keypad 0"; break;
		case Key::KP_1:			os << "Keypad 1"; break;
		case Key::KP_2:			os << "Keypad 2"; break;
		case Key::KP_3:			os << "Keypad 3"; break;
		case Key::KP_4:			os << "Keypad 4"; break;
		case Key::KP_5:			os << "Keypad 5"; break;
		case Key::KP_6:			os << "Keypad 6"; break;
		case Key::KP_7:			os << "Keypad 7"; break;
		case Key::KP_8:			os << "Keypad 8"; break;
		case Key::KP_9:			os << "Keypad 9"; break;

		case Key::KP_Ampersand: os << "Keypad &"; break;
		case Key::KP_Period:	os << "Keypad ."; break;
		case Key::KP_Enter:		os << "Keypad Enter"; break;
		case Key::KP_Plus:		os << "Keypad +"; break;
		case Key::KP_Minus:		os << "Keypad -"; break;
		case Key::KP_Multiply:	os << "Keypad *"; break;
		case Key::KP_Divide:	os << "Keypad /"; break;

		case Key::Invalid:		[[fallthrough]];
		default:				PTGN_ERROR("Invalid key enum value")
	}

	return os;
}

PTGN_SERIALIZER_REGISTER_ENUM(
	Key, { { Key::Invalid, nullptr },
		   { Key::K_0, "k_0" },
		   { Key::K_1, "k_1" },
		   { Key::K_2, "k_2" },
		   { Key::K_3, "k_3" },
		   { Key::K_4, "k_4" },
		   { Key::K_5, "k_5" },
		   { Key::K_6, "k_6" },
		   { Key::K_7, "k_7" },
		   { Key::K_8, "k_8" },
		   { Key::K_9, "k_9" },

		   { Key::Zero, "zero" },
		   { Key::One, "one" },
		   { Key::Two, "two" },
		   { Key::Three, "three" },
		   { Key::Four, "four" },
		   { Key::Five, "five" },
		   { Key::Six, "six" },
		   { Key::Seven, "seven" },
		   { Key::Eight, "eight" },
		   { Key::Nine, "nine" },

		   { Key::KP_0, "kp_0" },
		   { Key::KP_1, "kp_1" },
		   { Key::KP_2, "kp_2" },
		   { Key::KP_3, "kp_3" },
		   { Key::KP_4, "kp_4" },
		   { Key::KP_5, "kp_5" },
		   { Key::KP_6, "kp_6" },
		   { Key::KP_7, "kp_7" },
		   { Key::KP_8, "kp_8" },
		   { Key::KP_9, "kp_9" },

		   { Key::KP_Zero, "kp_zero" },
		   { Key::KP_One, "kp_one" },
		   { Key::KP_Two, "kp_two" },
		   { Key::KP_Three, "kp_three" },
		   { Key::KP_Four, "kp_four" },
		   { Key::KP_Five, "kp_five" },
		   { Key::KP_Six, "kp_six" },
		   { Key::KP_Seven, "kp_seven" },
		   { Key::KP_Eight, "kp_eight" },
		   { Key::KP_Nine, "kp_nine" },

		   { Key::KP_Ampersand, "kp_ampersand" },
		   { Key::KP_Period, "kp_period" },
		   { Key::KP_Delete, "kp_delete" },
		   { Key::KP_Enter, "kp_enter" },
		   { Key::KP_Plus, "kp_plus" },
		   { Key::KP_Minus, "kp_minus" },
		   { Key::KP_Multiply, "kp_multiply" },
		   { Key::KP_Star, "kp_star" },
		   { Key::KP_Asterisk, "kp_asterisk" },
		   { Key::KP_Divide, "kp_divide" },
		   { Key::KP_Slash, "kp_slash" },

		   { Key::A, "a" },
		   { Key::B, "b" },
		   { Key::C, "c" },
		   { Key::D, "d" },
		   { Key::E, "e" },
		   { Key::F, "f" },
		   { Key::G, "g" },
		   { Key::H, "h" },
		   { Key::I, "i" },
		   { Key::J, "j" },
		   { Key::K, "k" },
		   { Key::L, "l" },
		   { Key::M, "m" },
		   { Key::N, "n" },
		   { Key::O, "o" },
		   { Key::P, "p" },
		   { Key::Q, "q" },
		   { Key::R, "r" },
		   { Key::S, "s" },
		   { Key::T, "t" },
		   { Key::U, "u" },
		   { Key::V, "v" },
		   { Key::W, "w" },
		   { Key::X, "x" },
		   { Key::Y, "y" },
		   { Key::Z, "z" },

		   { Key::F1, "f1" },
		   { Key::F2, "f2" },
		   { Key::F3, "f3" },
		   { Key::F4, "f4" },
		   { Key::F5, "f5" },
		   { Key::F6, "f6" },
		   { Key::F7, "f7" },
		   { Key::F8, "f8" },
		   { Key::F9, "f9" },
		   { Key::F10, "f10" },
		   { Key::F11, "f11" },
		   { Key::F12, "f12" },
		   { Key::F13, "f13" },
		   { Key::F14, "f14" },
		   { Key::F15, "f15" },
		   { Key::F16, "f16" },
		   { Key::F17, "f17" },
		   { Key::F18, "f18" },
		   { Key::F19, "f19" },
		   { Key::F20, "f20" },
		   { Key::F21, "f21" },
		   { Key::F22, "f22" },
		   { Key::F23, "f23" },
		   { Key::F24, "f24" },

		   { Key::Right, "right" },
		   { Key::Left, "left" },
		   { Key::Down, "down" },
		   { Key::Up, "up" },

		   { Key::LeftAlt, "left_alt" },
		   { Key::AltGr, "altgr" },
		   { Key::RightAlt, "right_alt" },
		   { Key::LeftCtrl, "left_ctrl" },
		   { Key::RightCtrl, "right_ctrl" },
		   { Key::LeftShift, "left_shift" },
		   { Key::RightShift, "right_shift" },

		   { Key::Blank, "blank" },
		   { Key::Space, "space" },

		   { Key::Enter, "enter" },
		   { Key::Return, "return" },
		   { Key::Enter2, "enter2" },
		   { Key::Return2, "return2" },

		   { Key::Home, "home" },
		   { Key::End, "end" },
		   { Key::Insert, "insert" },
		   { Key::Delete, "delete" },

		   { Key::Apostrophe, "apostrophe" },
		   { Key::Comma, "comma" },
		   { Key::Period, "period" },
		   { Key::Semicolon, "semicolon" },
		   { Key::Equals, "equals" },
		   { Key::Tilde, "tilde" },
		   { Key::Grave, "grave" },
		   { Key::Minus, "minus" },
		   { Key::Slash, "slash" },
		   { Key::Backslash, "backslash" },

		   { Key::Escape, "escape" },
		   { Key::Backspace, "backspace" },
		   { Key::Capslock, "capslock" },
		   { Key::Tab, "tab" },

		   { Key::Windows, "windows" },
		   { Key::RGUI, "rgui" },
		   { Key::Command, "command" },

		   { Key::PrintScreen, "printscreen" },
		   { Key::Numlock, "numlock" },
		   { Key::KP_Numlock, "kp_numlock" },
		   { Key::PageDown, "pagedown" },
		   { Key::PageUp, "pageup" },

		   { Key::LeftBracket, "left_bracket" },
		   { Key::RightBracket, "right_bracket" },
		   { Key::ScrollLock, "scrolllock" },
		   { Key::Select, "select" },
		   { Key::Separator, "separator" },
		   { Key::Sleep, "sleep" } }
);

} // namespace ptgn