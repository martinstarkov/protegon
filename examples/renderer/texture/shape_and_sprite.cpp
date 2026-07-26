
#include <chrono>

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/math/angle.h"
#include "core/math/geometry/arc.h"
#include "core/math/vector2.h"
#include "runtime/animation/tween.h"
#include "runtime/animation/scripted_animation.h"
#include "runtime/animation/tween_event.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

struct ShapeAndSpriteScene : public Scene {
	static constexpr Degrees arc_start_angle{ 113.0f };
	static constexpr Degrees arc_end_angle{ -156.0f };
	static constexpr float arc_radius{ 23.0f };
	static constexpr float scale{ 4.0f };

	void OnEnter() override {
		SetBackgroundColor(color::LightCyan);

		ctx().asset.Load("combo_meter", "assets/combo_meter.png");

		auto arc{ CreateArc(
			*this, { 3, 3 }, arc_radius, arc_start_angle, arc_end_angle, false, color::Red
		) };
		auto combo{ CreateSprite(*this, { 6, 6 }, "combo_meter") };

		// TODO: Fix text.
		// auto text {CreateText(
		//	*this, combo_meter_center + V2_float{ 5, 4 }, "0",
		//	color::Black, 14, {}, Origin::Center, TextProperties{ .justify = TextJustify::Right }
		//) };
		//
		// SetParent(text, combo);

		SetParent(arc, combo);

		SetScale(combo, scale);

		struct ArcTween {};

		GetOrCreateTween<ArcTween>(arc)
			.During(1s)
			.Repeat()
			.OnProgress([this](auto p) {
				auto& arc_shape{ p.parent.template Get<Arc>() };

				arc_shape.start_angle =
					Lerp(arc_start_angle, arc_end_angle + 360.0f, p.progress).ToRad();
			})
			.Start();
	}
};

int main(int, char**) {
	Application app{ "ShapeAndSpriteScene", { 640, 360 } };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<ShapeAndSpriteScene>();
}