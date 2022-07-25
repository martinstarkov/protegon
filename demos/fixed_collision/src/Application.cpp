#include "core/Engine.h"
#include "utility/Log.h"
#include "collision/Fixed.h"

using namespace ptgn;

class FixedCollisionTest : public Engine {
public:
	virtual void Init() {}
	virtual void Update(double dt) {}
};

int main(int c, char** v) {
	FixedCollisionTest test;
	test.Start("Fixed Collision Test", { 600, 600 }, true);
	return 0;
}