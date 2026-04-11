#pragma once

#include <ostream>
#include <utility>

#include "core/log.h"
#include "platform/platform.h"
#include "serialization/json/enum.h"

namespace ptgn {

enum class Key {
	Space	   = 32, /*   */
	Apostrophe = 39, /* ' */
	Comma	   = 44, /* , */
	Minus	   = 45, /* - */
	Period	   = 46, /* . */
	Slash	   = 47, /* / */

	K_0 = 48,		 // 0
	K_1 = 49,		 // 1
	K_2 = 50,		 // 2
	K_3 = 51,		 // 3
	K_4 = 52,		 // 4
	K_5 = 53,		 // 5
	K_6 = 54,		 // 6
	K_7 = 55,		 // 7
	K_8 = 56,		 // 8
	K_9 = 57,		 // 9

	Semicolon = 59,	 /* ; */
	Equal	  = 61,	 /* = */

	A = 65,
	B = 66,
	C = 67,
	D = 68,
	E = 69,
	F = 70,
	G = 71,
	H = 72,
	I = 73,
	J = 74,
	K = 75,
	L = 76,
	M = 77,
	N = 78,
	O = 79,
	P = 80,
	Q = 81,
	R = 82,
	S = 83,
	T = 84,
	U = 85,
	V = 86,
	W = 87,
	X = 88,
	Y = 89,
	Z = 90,

	LeftBracket	 = 91, /* [ */
	Backslash	 = 92, /* \ */
	RightBracket = 93, /* ] */
	GraveAccent	 = 96, /* ` */

	World1 = 161,	   /* non-US #1 */
	World2 = 162,	   /* non-US #2 */

	/* Function keys */
	Escape		= 256,
	Enter		= 257,
	Tab			= 258,
	Backspace	= 259,
	Insert		= 260,
	Delete		= 261,
	Right		= 262,
	Left		= 263,
	Down		= 264,
	Up			= 265,
	PageUp		= 266,
	PageDown	= 267,
	Home		= 268,
	End			= 269,
	CapsLock	= 280,
	ScrollLock	= 281,
	NumLock		= 282,
	PrintScreen = 283,
	Pause		= 284,
	F1			= 290,
	F2			= 291,
	F3			= 292,
	F4			= 293,
	F5			= 294,
	F6			= 295,
	F7			= 296,
	F8			= 297,
	F9			= 298,
	F10			= 299,
	F11			= 300,
	F12			= 301,
	F13			= 302,
	F14			= 303,
	F15			= 304,
	F16			= 305,
	F17			= 306,
	F18			= 307,
	F19			= 308,
	F20			= 309,
	F21			= 310,
	F22			= 311,
	F23			= 312,
	F24			= 313,
	F25			= 314,

	/* Keypad */
	KP_0		= 320,
	KP_1		= 321,
	KP_2		= 322,
	KP_3		= 323,
	KP_4		= 324,
	KP_5		= 325,
	KP_6		= 326,
	KP_7		= 327,
	KP_8		= 328,
	KP_9		= 329,
	KP_Decimal	= 330,
	KP_Divide	= 331,
	KP_Multiply = 332,
	KP_Subtract = 333,
	KP_Add		= 334,
	KP_Enter	= 335,
	KP_Equal	= 336,

	LeftShift  = 340,
	LeftCtrl   = 341,
	LeftAlt	   = 342,
	LeftSuper  = 343,
	RightShift = 344,
	RightCtrl  = 345,
	RightAlt   = 346,
	RightSuper = 347,
	Menu	   = 348
};

inline std::ostream& operator<<(std::ostream& os, Key key) {
	switch (key) {
		using enum Key;
		case W:			   return os << "W";
		case A:			   return os << "A";
		case S:			   return os << "S";
		case D:			   return os << "D";

		case Right:		   return os << "Right";
		case Left:		   return os << "Left";
		case Down:		   return os << "Down";
		case Up:		   return os << "Up";

		case Space:		   return os << "Space";
		case Escape:	   return os << "Escape";
		case Enter:		   return os << "Enter";
		case Tab:		   return os << "Tab";

		case Q:			   return os << "Q";
		case E:			   return os << "E";
		case R:			   return os << "R";
		case T:			   return os << "T";

		case F:			   return os << "F";
		case G:			   return os << "G";

		case Z:			   return os << "Z";
		case X:			   return os << "X";
		case C:			   return os << "C";
		case V:			   return os << "V";
		case B:			   return os << "B";

		case LeftAlt:	   return os << "Left Alt";
		case RightAlt:	   return os << "Right Alt";
		case LeftCtrl:	   return os << "Left Ctrl";
		case RightCtrl:	   return os << "Right Ctrl";
		case LeftShift:	   return os << "Left Shift";
		case RightShift:   return os << "Right Shift";

		case H:			   return os << "H";
		case I:			   return os << "I";
		case J:			   return os << "J";
		case K:			   return os << "K";
		case L:			   return os << "L";
		case M:			   return os << "M";
		case N:			   return os << "N";
		case O:			   return os << "O";
		case P:			   return os << "P";
		case U:			   return os << "U";
		case Y:			   return os << "Y";

		case K_0:		   return os << "0";
		case K_1:		   return os << "1";
		case K_2:		   return os << "2";
		case K_3:		   return os << "3";
		case K_4:		   return os << "4";
		case K_5:		   return os << "5";
		case K_6:		   return os << "6";
		case K_7:		   return os << "7";
		case K_8:		   return os << "8";
		case K_9:		   return os << "9";

		case F1:		   return os << "F1";
		case F2:		   return os << "F2";
		case F3:		   return os << "F3";
		case F4:		   return os << "F4";
		case F5:		   return os << "F5";
		case F6:		   return os << "F6";
		case F7:		   return os << "F7";
		case F8:		   return os << "F8";
		case F9:		   return os << "F9";
		case F10:		   return os << "F10";
		case F11:		   return os << "F11";
		case F12:		   return os << "F12";
		case F13:		   return os << "F13";
		case F14:		   return os << "F14";
		case F15:		   return os << "F15";
		case F16:		   return os << "F16";
		case F17:		   return os << "F17";
		case F18:		   return os << "F18";
		case F19:		   return os << "F19";
		case F20:		   return os << "F20";
		case F21:		   return os << "F21";
		case F22:		   return os << "F22";
		case F23:		   return os << "F23";
		case F24:		   return os << "F24";
		case F25:		   return os << "F25";

		case Home:		   return os << "Home";
		case End:		   return os << "End";
		case Insert:	   return os << "Insert";
		case Delete:	   return os << "Delete";

		case Apostrophe:   return os << "'";
		case Comma:		   return os << ",";
		case Period:	   return os << ".";
		case Semicolon:	   return os << ";";
		case Equal:		   return os << "=";
		case GraveAccent:  return os << "`";
		case Minus:		   return os << "-";
		case Slash:		   return os << "/";
		case Backslash:	   return os << "\\";
		case LeftBracket:  return os << "[";
		case RightBracket: return os << "]";

		case Backspace:	   return os << "Backspace";
		case CapsLock:	   return os << "CapsLock";

		case PrintScreen:  return os << "PrintScreen";
		case PageDown:	   return os << "PageDown";
		case PageUp:	   return os << "PageUp";
		case ScrollLock:   return os << "ScrollLock";
		case Pause:		   return os << "Pause";

		case NumLock:	   return os << "NumLock";

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

		case KP_Enter:	   return os << "Keypad Enter";
		case KP_Equal:	   return os << "Keypad =";
		case KP_Add:	   return os << "Keypad +";
		case KP_Subtract:  return os << "Keypad -";
		case KP_Multiply:  return os << "Keypad *";
		case KP_Divide:	   return os << "Keypad /";
		case KP_Decimal:   return os << "Keypad .";

		case LeftSuper:	   return os << "Left Super";
		case RightSuper:   return os << "Right Super";
		case Menu:		   return os << "Menu";

		case World1:	   return os << "World1";
		case World2:	   return os << "World2";

		default:		   PTGN_ERROR("Unknown Key: ", std::to_underlying(key));
	}
}

PTGN_SERIALIZE_ENUM(
	Key, { { Key::Space, "space" },
		   { Key::Apostrophe, "apostrophe" },
		   { Key::Comma, "comma" },
		   { Key::Minus, "minus" },
		   { Key::Period, "period" },
		   { Key::Slash, "slash" },

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

		   { Key::Semicolon, "semicolon" },
		   { Key::Equal, "equal" },

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

		   { Key::LeftBracket, "left_bracket" },
		   { Key::Backslash, "backslash" },
		   { Key::RightBracket, "right_bracket" },
		   { Key::GraveAccent, "grave_accent" },

		   { Key::World1, "world1" },
		   { Key::World2, "world2" },

		   { Key::Escape, "escape" },
		   { Key::Enter, "enter" },
		   { Key::Tab, "tab" },
		   { Key::Backspace, "backspace" },
		   { Key::Insert, "insert" },
		   { Key::Delete, "delete" },
		   { Key::Right, "right" },
		   { Key::Left, "left" },
		   { Key::Down, "down" },
		   { Key::Up, "up" },
		   { Key::PageUp, "pageup" },
		   { Key::PageDown, "pagedown" },
		   { Key::Home, "home" },
		   { Key::End, "end" },
		   { Key::CapsLock, "caps_lock" },
		   { Key::ScrollLock, "scroll_lock" },
		   { Key::NumLock, "num_lock" },
		   { Key::PrintScreen, "print_screen" },
		   { Key::Pause, "pause" },

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
		   { Key::F25, "f25" },

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
		   { Key::KP_Decimal, "kp_decimal" },
		   { Key::KP_Divide, "kp_divide" },
		   { Key::KP_Multiply, "kp_multiply" },
		   { Key::KP_Subtract, "kp_subtract" },
		   { Key::KP_Add, "kp_add" },
		   { Key::KP_Enter, "kp_enter" },
		   { Key::KP_Equal, "kp_equal" },

		   { Key::LeftShift, "left_shift" },
		   { Key::LeftCtrl, "left_ctrl" },
		   { Key::LeftAlt, "left_alt" },
		   { Key::LeftSuper, "left_super" },
		   { Key::RightShift, "right_shift" },
		   { Key::RightCtrl, "right_ctrl" },
		   { Key::RightAlt, "right_alt" },
		   { Key::RightSuper, "right_super" },
		   { Key::Menu, "menu" } }
);

} // namespace ptgn