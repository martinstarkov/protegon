
#include <chrono>
#include <optional>

#include "app/application.h"
#include "core/math/angle.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "runtime/animation/tween.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

constexpr V2_int game_size{ 320, 180 };

struct ShapeAndSpriteScene : public Scene {
	Entity arc;

	Degrees start_angle{ 252.0f };
	Degrees end_angle{ 153.0f };

	void OnEnter() override {
		SetBackgroundColor(color::LightCyan);
		ctx().asset.Load("combo_meter", "examples/assets/combo_meter.png");
		ctx().asset.Load("combo_meter_arc", "examples/assets/combo_meter_arc.png");

		V2_float combo_meter_pos{ V2_float{ 6, 6 } - game_size / 2.0f };
		CreateText(*this, { 100, 0 }, "1", color::Black);
		auto arc_meter = CreateSprite(*this, "combo_meter_arc", combo_meter_pos, Origin::TopLeft);
		arc			   = CreateArc(*this, {}, 17.0f, start_angle, end_angle, false, color::Red);
		auto meter	   = CreateSprite(*this, "combo_meter", combo_meter_pos, Origin::TopLeft);
		CreateText(*this, { 200, 0 }, "2", color::Black);

		float scale{ 4.0f };

		SetScale(arc_meter, scale);
		SetScale(arc, scale);
		SetScale(meter, scale);
		SetPosition(arc, combo_meter_pos + *GetDisplaySize(arc_meter) / 2.0f);

		struct ArcTween {};

		GetOrCreateTween<ArcTween>(arc)
			.During(1s)
			.Repeat()
			.OnProgress([this](auto p) {
				auto& arc_shape{ p.parent.template Get<Arc>() };

				arc_shape.SetStartAngle(Lerp(start_angle, end_angle, p.progress));
			})
			.Start();
	}
};

int main(int, char**) {
	Application app{ "ShapeAndSpriteScene", game_size * 2.0f };
	app.StartWith<ShapeAndSpriteScene>();
}