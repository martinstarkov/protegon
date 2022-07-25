#include "core/Engine.h"
#include "utility/Log.h"
#include "collision/Overlap.h"

using namespace ptgn;

class OverlapCollisionTest : public Engine {
public:
	virtual void Init() {}
	virtual void Update(double dt) {}
};

int main(int c, char** v) {
	OverlapCollisionTest test;
	test.Start("Overlap Collision Test", { 600, 600 }, true);
	return 0;
}