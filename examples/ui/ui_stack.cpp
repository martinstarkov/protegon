#include <array>
#include <string>
#include <string_view>
#include <utility>

#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/group_stack.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"
#include "runtime/ui/button.h"

using namespace ptgn;

namespace {

constexpr std::string_view kFont{ "habitat_font" };

constexpr Color kCyan{ 0, 181, 189, 255 };
constexpr Color kTeal{ 0, 79, 84, 255 };
constexpr Color kDarkTeal{ 0, 42, 48, 255 };

struct DemoPlanet {
	std::string_view screen;
	std::string_view name;
	std::string_view texture;
	std::string_view traits;
	float x;
};

constexpr std::array<DemoPlanet, 3> kPlanets{
	DemoPlanet{ "KA-7", "Planet KA-7", "planet_1",
				"- Mild temperatures\n- Liquid water\n- Rich vegetation\n- High oxygen levels\n- "
				"Variable weather\n- Rocky terrain",
				-92.0f },
	DemoPlanet{ "Q-19", "Planet Q-19", "planet_2",
				"- Weak ozone layer\n- Long winters\n- Average oxygen\n  levels\n- Flat terrain\n- "
				"Low wind speeds\n- Lifeless",
				0.0f },
	DemoPlanet{ "ZP-3", "Planet ZP-3", "planet_3",
				"- Dry atmosphere\n- Intense radiation\n- Large dust storms\n- Little surface "
				"water\n- High temperatures\n- Iron rich terrain",
				92.0f }
};

void LoadAssets(Scene& scene) {
	auto& assets{ scene.ctx().asset };
	constexpr std::string_view root{ "assets/last_habitat/" };

	assets.Load("habitat_font", std::string{ root } + "font/monacottf.otf");
	assets.Load("space_background", std::string{ root } + "tile/space_background.png");
	assets.Load("planet_glow", std::string{ root } + "tile/glow.png");
	assets.Load("planet_popup", std::string{ root } + "ui/planet_popup.png");
	assets.Load("human_popup", std::string{ root } + "ui/human_popup.png");
	assets.Load("human_button", std::string{ root } + "ui/human_button.png");
	assets.Load("planet_exit", std::string{ root } + "ui/exit_popup_button2.png");
	assets.Load("human_exit", std::string{ root } + "ui/exit_popup_button.png");
	assets.Load("confirm_button", std::string{ root } + "ui/confirm_button.png");
	assets.Load("back_button", std::string{ root } + "ui/back_button.png");

	for (int i{ 1 }; i <= 3; ++i) {
		auto key{ "planet_" + std::to_string(i) };
		assets.Load(key, std::string{ root } + "entity/" + key + ".png");
	}
}

void ConfigureDisplay(Scene& scene) {
	// The original game used a 320 x 180 canvas with pixel perfect scaling.
	scene.ctx().renderer.SetLogicalSize(V2_int{ 320, 180 }, ScalingMode::IntegerScale);
}

Transform At(V2_float position) {
	Transform transform;
	transform.position = position;
	return transform;
}

Sprite MakeSprite(
	Scene& scene, Entity parent, std::string_view texture, V2_float position, float depth = 0.0f
) {
	Sprite sprite{ CreateSprite(scene, At(position), TextureKey{ texture }, Origin::Center) };
	SetUI(sprite, true);
	SetDepth(sprite, depth);
	if (parent) {
		AddChild(parent, sprite);
	}
	return sprite;
}

Text MakeText(
	Scene& scene, Entity parent, std::string_view content, V2_float position, float font_size,
	Origin origin = Origin::Center, float depth = 4.0f
) {
	Text text{
		CreateText(scene, At(position), content, color::White, font_size, origin, FontKey{ kFont })
	};
	SetUI(text, true);
	SetDepth(text, depth);
	AddChild(parent, text);
	return text;
}

Button MakeImageButton(
	Scene& scene, Entity parent, std::string_view texture, V2_float position, V2_float size,
	float depth = 5.0f
) {
	Button button{ CreateButton(scene, At(position), size, Origin::Center) };
	SetUI(button, true);
	button.Sprite().Texture(TextureKey{ texture }).Size(size);
	button.Sprite(ButtonVisualState::Hover).Texture(TextureKey{ texture }).Tint(color::LightGray);
	button.Sprite(ButtonVisualState::Press).Texture(TextureKey{ texture }).Tint(color::DarkGray);
	SetDepth(button, depth);
	AddChild(parent, button);
	return button;
}

Button MakeTextButton(
	Scene& scene, Entity parent, std::string_view caption, V2_float position, V2_float size
) {
	Button button{ CreateButton(scene, At(position), size, Origin::Center) };
	SetUI(button, true);
	button.Background().Color(kTeal);
	button.Background(ButtonVisualState::Hover).Color(kCyan);
	button.Background(ButtonVisualState::Press).Color(kDarkTeal);
	button.Text().Content(caption).Font(FontKey{ kFont }).Size(7.0f).Color(color::White);
	SetDepth(button, 5.0f);
	AddChild(parent, button);
	return button;
}

// Group Stack navigation through serializable ScriptSequence actions.
// Without an explicit target, actions resolve the button's containing Group Stack.
enum class DemoNavigation {
	Push,
	Pop,
	Set
};

void AttachNavigation(
	Button button, std::string_view description, DemoNavigation action, GroupEntity destination = {}
) {
	ScriptSequence sequence{ std::string{ description } };
	sequence.StartOn<event::ButtonPress>();

	switch (action) {
		case DemoNavigation::Push: {
			GroupStackPushScript script;
			script.group = destination ? destination.Get<UUID>() : UUID{ 0 };
			sequence.Then(std::move(script));
			break;
		}
		case DemoNavigation::Pop: sequence.Then(GroupStackPopScript{}); break;
		case DemoNavigation::Set: {
			GroupStackSetScript script;
			script.group = destination ? destination.Get<UUID>() : UUID{ 0 };
			sequence.Then(std::move(script));
			break;
		}
	}

	auto& script{ AddScript<Script>(button) };
	script.sequence = std::move(sequence);
}

void CreatePlanetDetails(
	Scene& scene, GroupEntity screen, const DemoPlanet& planet, GroupEntity confirm
) {
	// Opening Confirm leaves this screen visible but prevents interactions.
	screen.SetOverlayMode(OverlayMode::Disable);
	screen.SetShowDisabledStyle(true);

	MakeSprite(scene, screen, "planet_popup", { 0.0f, -1.0f });
	MakeSprite(scene, screen, planet.texture, { -38.5f, 2.5f }, 2.0f);
	MakeText(scene, screen, planet.name, { -36.5f, -46.5f }, 9.0f);
	MakeText(scene, screen, planet.traits, { 5.0f, -40.0f }, 5.0f, Origin::TopLeft);

	Button exit{
		MakeImageButton(scene, screen, "planet_exit", { -35.5f, -62.5f }, { 23.0f, 9.0f })
	};
	AttachNavigation(exit, "Close planet details", DemoNavigation::Pop);

	// Demonstrates an advanced cover override: the Select button disappears
	// when the confirmation screen is pushed, but the rest of this popup stays.
	Button select{
		MakeImageButton(scene, screen, "confirm_button", { -39.5f, 51.5f }, { 47.0f, 14.0f })
	};
	select.Add<OverlayOverride>(OverlayOverride{ OverlayMode::Hide });
	AttachNavigation(select, "Confirm planet selection", DemoNavigation::Push, confirm);
}

void CreateSpeciesScreen(Scene& scene, GroupEntity species) {
	species.SetOverlayMode(OverlayMode::Hide);

	MakeSprite(scene, species, "human_popup", { 0.0f, -1.0f });
	MakeText(
		scene, species,
		"- Radiation resistant\n- Strong lungs\n- Low insulation\n- Wood based society\n- Coal and "
		"oil powered\n  technology\n- Poor climbing skills\n- Technologically\n  disaster "
		"resistant",
		{ -80.0f, -40.0f }, 6.0f, Origin::TopLeft
	);

	Button close{
		MakeImageButton(scene, species, "human_exit", { -35.5f, -62.5f }, { 23.0f, 9.0f })
	};
	AttachNavigation(close, "Return to planet selection", DemoNavigation::Pop);
}

void CreateConfirmationScreen(Scene& scene, GroupEntity confirm, GroupEntity complete) {
	confirm.SetBlockBackgroundInput(true);
	confirm.SetOverlayMode(OverlayMode::Hide);

	Entity border{ CreateRect(scene, At({ 0.0f, 0.0f }), { 190.0f, 82.0f }, kCyan) };
	SetUI(border, true);
	AddChild(confirm, border);
	SetDepth(border, 0.0f);

	Entity panel{ CreateRect(scene, At({ 0.0f, 0.0f }), { 186.0f, 78.0f }, kDarkTeal) };
	SetUI(panel, true);
	AddChild(confirm, panel);
	SetDepth(panel, 1.0f);

	MakeText(scene, confirm, "CONFIRM DESTINATION", { 0.0f, -26.0f }, 10.0f);
	MakeText(scene, confirm, "Launch an expedition to this world?", { 0.0f, -4.0f }, 6.0f);

	Button cancel{ MakeTextButton(scene, confirm, "CANCEL", { -46.0f, 23.0f }, { 72.0f, 18.0f }) };
	AttachNavigation(cancel, "Cancel and return to planet", DemoNavigation::Pop);

	Button accept{ MakeTextButton(scene, confirm, "LAUNCH", { 46.0f, 23.0f }, { 72.0f, 18.0f }) };
	AttachNavigation(accept, "Confirm destination", DemoNavigation::Set, complete);
}

void CreateCompletionScreen(Scene& scene, GroupEntity complete, GroupEntity choose) {
	MakeSprite(scene, complete, "human_popup", { 0.0f, -1.0f });
	MakeText(scene, complete, "DESTINATION SELECTED", { 0.0f, -25.0f }, 10.0f);
	MakeText(scene, complete, "Your expedition is ready!", { 0.0f, -5.0f }, 7.0f);
	MakeText(scene, complete, "This demo uses Group Stack navigation.", { 0.0f, 13.0f }, 5.0f);

	Button restart{
		MakeImageButton(scene, complete, "back_button", { 0.0f, 46.0f }, { 48.0f, 25.0f })
	};
	AttachNavigation(restart, "Choose another planet", DemoNavigation::Set, choose);
}

} // namespace

class GroupStackDemoScene : public Scene {
public:
	void OnNew() override {
		LoadAssets(*this);
		ConfigureDisplay(*this);
		SetBackgroundColor(color::Black);

		// Keep all demo artwork and controls on the fixed UI camera.
		// Otherwise sprites from the world camera may cover button visuals.
		// The star field is shared by all screens and is never hidden.
		MakeSprite(*this, {}, "space_background", { 0.0f, 0.0f }, -100.0f);

		GroupStack stack{ CreateGroupStack(*this) };
		GroupEntity choose{ stack.AddGroup("Choose") };
		choose.SetOverlayMode(OverlayMode::Hide);
		stack.SetInitialGroup(choose);

		// Create the direct child groups before wiring navigation actions.
		GroupEntity species{ stack.AddGroup("Species") };
		GroupEntity confirm{ stack.AddGroup("Confirm") };
		GroupEntity complete{ stack.AddGroup("Complete") };
		std::array<GroupEntity, kPlanets.size()> planet_groups{};
		for (std::size_t i{}; i < kPlanets.size(); ++i) {
			planet_groups[i] = stack.AddGroup(kPlanets[i].screen);
		}

		// This is the entire interactive planet selection screen. No manual
		// Show(), Hide(), Enable(), or Disable() calls are required.
		for (std::size_t i{}; i < kPlanets.size(); ++i) {
			const auto& planet{ kPlanets[i] };
			MakeSprite(*this, choose, "planet_glow", { planet.x, 0.0f }, 1.0f);
			Button button{ MakeImageButton(
				*this, choose, planet.texture, { planet.x, 0.0f }, { 60.0f, 60.0f }, 2.0f
			) };
			AttachNavigation(button, "Open planet details", DemoNavigation::Push, planet_groups[i]);
		}

		Button species_button{
			MakeImageButton(*this, choose, "human_button", { -38.0f, 83.0f }, { 80.0f, 13.0f })
		};
		MakeText(
			*this, species_button, "Species Traits", { 0.0f, 0.0f }, 6.0f, Origin::Center, 1.0f
		);
		AttachNavigation(species_button, "Open species traits", DemoNavigation::Push, species);

		CreateSpeciesScreen(*this, species);
		for (std::size_t i{}; i < kPlanets.size(); ++i) {
			CreatePlanetDetails(*this, planet_groups[i], kPlanets[i], confirm);
		}
		CreateConfirmationScreen(*this, confirm, complete);
		CreateCompletionScreen(*this, complete, choose);
	}

	void OnLoad() override {
		LoadAssets(*this);
		ConfigureDisplay(*this);
	}
};

PTGN_REGISTER_SCENE(GroupStackDemoScene);

int main(int, char**) {
	Application app{ "Group Stack Demo", { 1280, 720 } };

	PTGN_WITH_EDITOR(app, true);

	app.StartProject<GroupStackDemoScene>("UIStackProject/UIStack.ptgnproj");
}
