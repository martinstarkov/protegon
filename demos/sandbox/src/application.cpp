#include "protegon/protegon.h"

using namespace ptgn;

class Sandbox : public Engine {
	void Create() final {

	}
	void Update(float dt) final {

	}
};

int main(int c, char** v) {
	Sandbox game;
	game.Construct();
	return 0;
}