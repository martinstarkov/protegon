#include "app/application.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "runtime/graphics/shape.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class CapsuleEntityScene : public Scene {
	void OnEnter() override {
		CreateCapsule(*this, { -120, 0 }, { -50, 0 }, { 50, 0 }, 16.0f, color::Yellow, 1.0f);
		CreateCapsule(*this, { 0, 0 }, { -50, 0 }, { 50, 0 }, 16.0f, color::LightGold, Solid{});
		CreateCapsule(*this, { 120, 0 }, { -50, 0 }, { 50, 0 }, 16.0f, color::Orange, 5.0f);
	}
};

int main(int, char**) {
	Application app{ "capsule_entity" };
	app.StartWith<CapsuleEntityScene>();
}
