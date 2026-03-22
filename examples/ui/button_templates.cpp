
#include <utility>

#include "app/application.h"
#include "app/context.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/window/window.h"
#include "renderer/primitives/color.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/ui/button.h"

using namespace ptgn;

class ButtonTemplatesScene : public Scene {
public:
	void OnEnter() override {
		input.SetSettings({ .debug_draw_enabled = true });
		SetBackgroundColor(color::LightGray);

		// CreateButton(*this);

		// app().asset.Load("idle", "assets/bell.png");
		// app().asset.Load("animation_hover", "assets/bell_hover_animation.png");
		// app().asset.Load("animation_activate", "assets/bell_click_animation.png");
		// app().asset.LoadAudio("hover", "assets/hover.ogg");
		// app().asset.LoadAudio("click", "assets/bell.ogg");
		// auto hover_animation{ CreateAnimation(
		//	*this, "animation_hover", V2_int{}, { 3, milliseconds{ 400 }, V2_int{ 253, 167 }, -1 }
		//) };
		// auto activate_animation{ CreateAnimation(
		//	*this, "animation_activate", V2_int{}, { 3, milliseconds{ 200 }, V2_int{ 253, 167 }, 1 }
		//) };
		// b1 = CreateButton(*this, *GetDisplaySize(activate_animation));
		// b1.SetTexture("idle")
		//	.SetAnimation(std::move(hover_animation), ButtonState::Hover)
		//	.SetAnimation(std::move(activate_animation), ButtonState::Press)
		//	.SetSound("hover", ButtonState::Hover)
		//	.SetSound("click", ButtonState::Press);
		// SetScale(b1, 1.0f);
		// b1.OnActivate([]() { PTGN_LOG("Clicked bell!"); });
	}
};

int main(int, char**) {
	Application app{ "ButtonTemplatesScene" };
	app.StartWith<ButtonTemplatesScene>();
}