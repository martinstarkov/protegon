#include "protegon/protegon.h"

using namespace ptgn;

class Sandbox : public Engine {
	Button button0{ Rectangle<int>{ { 5, 5 }, { 50, 50 } }, {
		{ ButtonState::IDLE_UP,       Texture{ "resources/ui/idle.png" } },
		{ ButtonState::HOVER,         Texture{ "resources/ui/hover.png" } },
		{ ButtonState::HOVER_PRESSED, Texture{ "resources/ui/hover.png" } },
		{ ButtonState::PRESSED,       Texture{ "resources/ui/pressed.png" } },
		{ ButtonState::FOCUSED,       Texture{ "resources/ui/focused.png" } }
	}, [&]() {
		PrintLine(bye);
	} };
	Button button1{ Rectangle<int>{ { 60, 60 }, { 300, 300 } }, {
		{ ButtonState::IDLE_UP,       Texture{ "resources/ui/q0.png" } },
		{ ButtonState::HOVER,         Texture{ "resources/ui/q1.png" } },
		{ ButtonState::PRESSED,       Texture{ "resources/ui/q2.png" } },
		{ ButtonState::HELD_OUTSIDE,  Texture{ "resources/ui/q3.png" } },
		{ ButtonState::IDLE_DOWN,     Texture{ "resources/ui/q4.png" } },
		{ ButtonState::HOVER_PRESSED, Texture{ "resources/ui/q5.png" } },
		{ ButtonState::FOCUSED,       Texture{ "resources/ui/q6.png" } }
	}};
	std::string bye{ "bye" };
	Button button2{ Rectangle<int>{ { 390, 390 }, { 30, 30 } }, {
		{ ButtonState::IDLE_UP,       Texture{ "resources/ui/idle.png" } },
		{ ButtonState::HOVER,         Texture{ "resources/ui/hover.png" } },
		{ ButtonState::HOVER_PRESSED, Texture{ "resources/ui/hover.png" } },
		{ ButtonState::PRESSED,       Texture{ "resources/ui/pressed.png" } },
		{ ButtonState::FOCUSED,       Texture{ "resources/ui/focused.png" } }
	} };
	void Create() final {
		button2.SetOnActivate([]() {
			PrintLine("Hi!");
		});
	}
	void Update(float dt) final {
		button0.Draw();
		button1.GetRectangle().Draw(color::BLACK, 10);
		button1.Draw();
		button2.Draw();
	}
};

int main(int c, char** v) {
	Sandbox game;
	game.Construct("sandbox", { 720, 720 });
	return 0;
}