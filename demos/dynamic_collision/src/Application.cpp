#include "core/Engine.h"
#include "utility/Log.h"
#include "collision/Dynamic.h"

using namespace ptgn;

class DynamicCollisionTest : public Engine {
public:
	virtual void Init() {}
	virtual void Update(double dt) {}
};

int main(int c, char** v) {
	DynamicCollisionTest test;
	test.Start("Dynamic Collision Test", { 600, 600 }, true);
	return 0;
}