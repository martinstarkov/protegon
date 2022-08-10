#include "core/Engine.h"
#include "utility/Log.h"
#include "collision/Dynamic.h"

using namespace ptgn;

class DynamicCollisionTest : public Engine {
public:
	virtual void Init() {}
	virtual void Update(float dt) {}
};

int main(int c, char** v) {
	DynamicCollisionTest test;
	test.Start("Dynamic Collision Test", { 600, 600 }, true, V2_int{}, window::Flags::NONE, true, false);
	return 0;
}