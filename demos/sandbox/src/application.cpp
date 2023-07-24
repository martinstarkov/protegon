#include "protegon/protegon.h"

using namespace ptgn;

class Sandbox : public Engine {
	/*Button button{ Rectangle<int>{ { 60, 60 }, { 300, 300 } }, {
		{ Button::State::IDLE_UP,       Texture{ "resources/ui/q0.png" } },
		{ Button::State::HOVER,         Texture{ "resources/ui/q1.png" } },
		{ Button::State::PRESSED,       Texture{ "resources/ui/q2.png" } },
		{ Button::State::HELD_OUTSIDE,  Texture{ "resources/ui/q3.png" } },
		{ Button::State::IDLE_DOWN,     Texture{ "resources/ui/q4.png" } },
		{ Button::State::HOVER_PRESSED, Texture{ "resources/ui/q5.png" } },
		{ Button::State::FOCUSED,       Texture{ "resources/ui/q6.png" } }
	}};*/
	Button button{ Rectangle<int>{ { 60, 60 }, { 300, 300 } }, {
		{ Button::State::IDLE_UP,       Texture{ "resources/ui/idle.png" } },
		{ Button::State::HOVER,         Texture{ "resources/ui/hover.png" } },
		{ Button::State::HOVER_PRESSED, Texture{ "resources/ui/hover.png" } },
		{ Button::State::PRESSED,       Texture{ "resources/ui/pressed.png" } },
		{ Button::State::FOCUSED,       Texture{ "resources/ui/focused.png" } }
	}, []() {
		PrintLine("Bye!");
	} };
	Button button2{ Rectangle<int>{ { 390, 390 }, { 30, 30 } }, {
		{ Button::State::IDLE_UP,       Texture{ "resources/ui/idle.png" } },
		{ Button::State::HOVER,         Texture{ "resources/ui/hover.png" } },
		{ Button::State::HOVER_PRESSED, Texture{ "resources/ui/hover.png" } },
		{ Button::State::PRESSED,       Texture{ "resources/ui/pressed.png" } },
		{ Button::State::FOCUSED,       Texture{ "resources/ui/focused.png" } }
	} };
	void Create() final {
		button2.SetOnActivate([]() {
			PrintLine("Hi!");
		});
	}
	void Update(float dt) final {
		button.Draw();
		button2.Draw();
	}
};

int main(int c, char** v) {
	Sandbox game;
	game.Construct("sandbox", { 720, 720 });
	return 0;
}