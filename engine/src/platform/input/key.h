#pragma once

#include <ostream>

#include "core/log.h"
#include "platform/platform.h"
#include "serialization/json/enum.h"

namespace ptgn {

enum class Key {
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

	KP_0 = 98,			// Keypad Key 0 // SDL_SCANCODE_KP_0
	KP_1 = 89,			// Keypad Key 1 // SDL_SCANCODE_KP_1
	KP_2 = 90,			// Keypad Key 2 // SDL_SCANCODE_KP_2
	KP_3 = 91,			// Keypad Key 3 // SDL_SCANCODE_KP_3
	KP_4 = 92,			// Keypad Key 4 // SDL_SCANCODE_KP_4
	KP_5 = 93,			// Keypad Key 5 // SDL_SCANCODE_KP_5
	KP_6 = 94,			// Keypad Key 6 // SDL_SCANCODE_KP_6
	KP_7 = 95,			// Keypad Key 7 // SDL_SCANCODE_KP_7
	KP_8 = 96,			// Keypad Key 8 // SDL_SCANCODE_KP_8
	KP_9 = 97,			// Keypad Key 9 // SDL_SCANCODE_KP_9

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

	LeftWindows = 227,	// SDL_SCANCODE_LGUI
	LGUI		= 227,	// SDL_SCANCODE_LGUI
	LeftCommand = 227,	// SDL_SCANCODE_LGUI

	RightWindows = 231, // SDL_SCANCODE_RGUI
	RGUI		 = 231, // SDL_SCANCODE_RGUI
	RightCommand = 231, // SDL_SCANCODE_RGUI

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
	switch (key) {
		using enum Key;
		case W:			 return os << "W";
		case A:			 return os << "A";
		case S:			 return os << "S";
		case D:			 return os << "D";

		case Right:		 return os << "Right";
		case Left:		 return os << "Left";
		case Down:		 return os << "Down";
		case Up:		 return os << "Up";

		case Space:		 return os << "Space";
		case Escape:	 return os << "Escape";
		case Enter:		 return os << "Enter";
		case Tab:		 return os << "Tab";

		case Q:			 return os << "Q";
		case E:			 return os << "E";
		case R:			 return os << "R";
		case T:			 return os << "T";

		case F:			 return os << "F";
		case G:			 return os << "G";

		case Z:			 return os << "Z";
		case X:			 return os << "X";
		case C:			 return os << "C";
		case V:			 return os << "V";
		case B:			 return os << "B";

		case LeftAlt:	 return os << "Left Alt";
		case RightAlt:	 return os << "Right Alt";
		case LeftCtrl:	 return os << "Left Ctrl";
		case RightCtrl:	 return os << "Right Ctrl";
		case LeftShift:	 return os << "Left Shift";
		case RightShift: return os << "Right Shift";

		case H:			 return os << "H";
		case I:			 return os << "I";
		case J:			 return os << "J";
		case K:			 return os << "K";
		case L:			 return os << "L";
		case M:			 return os << "M";
		case N:			 return os << "N";
		case O:			 return os << "O";
		case P:			 return os << "P";
		case U:			 return os << "U";
		case Y:			 return os << "Y";

		case K_0:		 return os << "0";
		case K_1:		 return os << "1";
		case K_2:		 return os << "2";
		case K_3:		 return os << "3";
		case K_4:		 return os << "4";
		case K_5:		 return os << "5";
		case K_6:		 return os << "6";
		case K_7:		 return os << "7";
		case K_8:		 return os << "8";
		case K_9:		 return os << "9";

		case F1:		 return os << "F1";
		case F2:		 return os << "F2";
		case F3:		 return os << "F3";
		case F4:		 return os << "F4";
		case F5:		 return os << "F5";
		case F6:		 return os << "F6";
		case F7:		 return os << "F7";
		case F8:		 return os << "F8";
		case F9:		 return os << "F9";
		case F10:		 return os << "F10";
		case F11:		 return os << "F11";
		case F12:		 return os << "F12";
		case F13:		 return os << "F13";
		case F14:		 return os << "F14";
		case F15:		 return os << "F15";
		case F16:		 return os << "F16";
		case F17:		 return os << "F17";
		case F18:		 return os << "F18";
		case F19:		 return os << "F19";
		case F20:		 return os << "F20";
		case F21:		 return os << "F21";
		case F22:		 return os << "F22";
		case F23:		 return os << "F23";
		case F24:		 return os << "F24";

		case Enter2:	 return os << "Enter2";
		case Home:		 return os << "Home";
		case End:		 return os << "End";
		case Insert:	 return os << "Insert";
		case Delete:	 return os << "Delete";

		case Apostrophe: return os << "'";
		case Comma:		 return os << ",";
		case Period:	 return os << ".";
		case Semicolon:	 return os << ";";
		case Equals:	 return os << "=";
		case Grave:		 return os << "`";
		case Minus:		 return os << "-";
		case Slash:		 return os << "/";
		case Backslash:	 return os << "\\";

		case Backspace:	 return os << "Backspace";
		case Capslock:	 return os << "Capslock";

#if defined(PTGN_PLATFORM_WINDOWS)
		case LeftWindows:  return os << "Left Windows";
		case RightWindows: return os << "Right Windows";
#elif defined(PTGN_PLATFORM_MACOS) || defined(PTGN_PLATFORM_LINUX)
		case Key::LeftCommand:	return os << "Left Command";
		case Key::RightCommand: return os << "Right Command";
#else
		case Key::LGUI: return os << "LGUI";
		case Key::RGUI: return os << "RGUI";
#endif

		case PrintScreen:  return os << "PrintScreen";
		case PageDown:	   return os << "PageDown";
		case PageUp:	   return os << "PageUp";
		case LeftBracket:  return os << "LeftBracket";
		case RightBracket: return os << "RightBracket";
		case ScrollLock:   return os << "ScrollLock";
		case Select:	   return os << "Select";
		case Separator:	   return os << "Separator";
		case Sleep:		   return os << "Sleep";

		case Numlock:	   return os << "Numlock";

		case KP_0:		   return os << "Keypad 0";
		case KP_1:		   return os << "Keypad 1";
		case KP_2:		   return os << "Keypad 2";
		case KP_3:		   return os << "Keypad 3";
		case KP_4:		   return os << "Keypad 4";
		case KP_5:		   return os << "Keypad 5";
		case KP_6:		   return os << "Keypad 6";
		case KP_7:		   return os << "Keypad 7";
		case KP_8:		   return os << "Keypad 8";
		case KP_9:		   return os << "Keypad 9";

		case KP_Ampersand: return os << "Keypad &";
		case KP_Period:	   return os << "Keypad .";
		case KP_Enter:	   return os << "Keypad Enter";
		case KP_Plus:	   return os << "Keypad +";
		case KP_Minus:	   return os << "Keypad -";
		case KP_Multiply:  return os << "Keypad *";
		case KP_Divide:	   return os << "Keypad /";

		default:		   PTGN_ERROR("Invalid key enum value");
	}

	return os;
}

PTGN_SERIALIZE_ENUM(
	Key, { { Key::K_0, "k_0" },
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

		   { Key::RightWindows, "right_windows" },
		   { Key::RGUI, "rgui" },
		   { Key::RightCommand, "right_command" },

		   { Key::LeftWindows, "left_windows" },
		   { Key::LGUI, "lgui" },
		   { Key::LeftCommand, "left_command" },

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