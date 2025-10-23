#include <string>

#include "audio/audio.h"
#include "core/ecs/components/draw.h"
#include "core/ecs/components/interactive.h"
#include "core/ecs/components/lifetime.h"
#include "core/ecs/components/offsets.h"
#include "core/ecs/entity.h"
#include "core/app/game.h"
#include "core/app/manager.h"
#include "math/geometry/circle.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "renderer/text/font.h"
#include "renderer/materials/texture.h"
#include "serialization/json/fwd.h"
#include "serialization/json/json.h"
#include "serialization/json/json_manager.h"
#include "serialization/json/serializable.h"

using namespace ptgn;

class MyData {
public:
	MyData() : id(0), message(""), value(0.0f) {}

	int id;
	std::string message;
	float value;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(MyData, id, message, value)
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Manager manager;
	Entity entity{ manager.CreateEntity() };
	SetPosition(entity, { 30, 50 });

	Manager m;

	auto e0 = m.CreateEntity();
	SetPosition(e0, V2_float{ -69, -69 });

	auto e1 = m.CreateEntity();
	SetTransform(e1, { V2_float{ 30, 50 }, 2.14f, V2_float{ 2.0f } });
	Show(e1);
	SetDepth(e1, 22);
	auto tint_color{ color::Blue };
	SetTint(e1, tint_color);
	e1.Add<LineWidth>(3.5f);
	e1.Add<TextureHandle>("sheep1");
	e1.Add<TextureCrop>(V2_float{ 1, 2 }, V2_float{ 11, 12 });
	e1.Add<RigidBody>();
	SetInteractive(e1);
	// auto child = m.CreateEntity();
	// child.Add<Circle>(30.0f);
	// AddInteractable(e1, std::move(child));
	e1.Add<Draggable>();
	e1.Add<impl::Offsets>(); // Transforms will be serialized as nulls because they are default
							 // values.
	e1.Add<Lifetime>(milliseconds{ 300 }).Start();

	{
		json j = e1.Serialize();

		SaveJson(j, "resources/mydata.json");

		PTGN_LOG("Successfully serialized all entity components: ", j.dump(4));

		RNG<float> rng{ 3, 0.5f, 1.5f };
		json j2 = rng;

		PTGN_LOG("Successfully serialized rng: ", j2.dump(4));

		RNG<float> rng2;
		j2.get_to(rng2);

		PTGN_ASSERT(rng2.GetSeed() == 3);
		PTGN_ASSERT(rng2.GetMin() == 0.5f);
		PTGN_ASSERT(rng2.GetMax() == 1.5f);
	}

	{
		auto j = LoadJson("resources/mydata.json");

		Entity e2{ m.CreateEntity(j) };

		PTGN_ASSERT(e2.Has<Transform>());
		PTGN_ASSERT(e2.Has<UUID>());
		PTGN_ASSERT(e2.Has<Draggable>());
		PTGN_ASSERT(e2.Has<TextureCrop>());
		PTGN_ASSERT(e2.Has<Visible>());
		PTGN_ASSERT(e2.Has<Depth>());
		PTGN_ASSERT(e2.Has<Tint>());
		PTGN_ASSERT(e2.Get<Tint>() == tint_color);
		PTGN_ASSERT(e2.Has<LineWidth>());
		PTGN_ASSERT(e2.Has<TextureHandle>());
		PTGN_ASSERT(e2.Has<RigidBody>());
		PTGN_ASSERT(e2.Has<Interactive>());
		PTGN_ASSERT(e2.Has<impl::Offsets>());
		PTGN_ASSERT(e2.Get<impl::Offsets>().bounce == Transform{});
		PTGN_ASSERT(e2.Has<Lifetime>());

		PTGN_LOG("Successfully deserialized all entity components");
	}

	const auto test_manager_serialization = [](const std::string& manager_name,
											   auto& resource_manager, const path& resource1_path,
											   const path& resource2_path, bool is_music = false) {
		LoadResource(manager_name + "1", resource1_path, is_music);
		LoadResource(manager_name + "2", resource2_path, is_music);

		PTGN_ASSERT(resource_manager.Has(manager_name + "1"));
		PTGN_ASSERT(resource_manager.Has(manager_name + "2"));

		json manager_json = resource_manager;

		PTGN_LOG("Successfully serialized the ", manager_name, " manager");

		PTGN_LOG(manager_json.dump(4));

		resource_manager.Unload(manager_name + "1");
		resource_manager.Unload(manager_name + "2");

		PTGN_ASSERT(!resource_manager.Has(manager_name + "1"));
		PTGN_ASSERT(!resource_manager.Has(manager_name + "2"));

		resource_manager = manager_json;

		PTGN_ASSERT(resource_manager.Has(manager_name + "1"));
		PTGN_ASSERT(resource_manager.Has(manager_name + "2"));

		PTGN_ASSERT(resource_manager.GetPath(manager_name + "1") == resource1_path);
		PTGN_ASSERT(resource_manager.GetPath(manager_name + "2") == resource2_path);

		PTGN_LOG("Successfully deserialized the ", manager_name, " manager");
	};

	{
		test_manager_serialization(
			"texture", game.texture, "resources/texture1.png", "resources/texture2.png"
		);
		test_manager_serialization("font", game.font, "resources/font1.ttf", "resources/font2.ttf");
		test_manager_serialization(
			"sound", game.sound, "resources/sound1.ogg", "resources/sound2.ogg"
		);
		test_manager_serialization(
			"music", game.music, "resources/sound1.ogg", "resources/sound2.ogg", true
		);
		test_manager_serialization(
			"json", game.json, "resources/json1.json", "resources/json2.json"
		);
	}

	/*{
		JsonOutputArchive json_output("resources/mydata.json");
		MyData data3;
		data3.id	  = 456;
		data3.message = "JSON Data";
		data3.value	  = 2.71f;

		json_output.Write("data3", data3);
	}*/

	/*{
		JsonInputArchive json_input("resources/mydata.json");
		MyData data4;

		json_input.Read("data3", data4);

		std::cout << "JSON: id=" << data4.id << ", message=\"" << data4.message
				  << "\", value=" << data4.value << std::endl;
	}*/

	/*
	// Script serialization and deserialization tests

	class TweenScript1 : public TweenScript<TweenScript1> {
	public:
		TweenScript1() {}

		void OnUpdate(float progress) override {
			std::cout << "TweenScript1: " << entity << " updated with " << progress << "\n";
		}
	};

	class TweenScript2 : public TweenScript<TweenScript2> {
	public:
		TweenScript2() {}

		void OnUpdate(float progress) override {
			std::cout << "TweenScript2: " << entity << " updated with " << progress << "\n";
		}
	};

	{
		std::unique_ptr<TweenScript> test;

		test = TweenScript::create<TweenScript1>(10);

		test->OnUpdate(0.1f);

		json j = *test;

		PTGN_LOG("Serialized script with name: ", test->GetName(), "\n", j.dump(4));

		SaveJson(j, "resources/myscripts.json");
	}
	{
		auto j{ LoadJson("resources/myscripts.json") };

		std::unique_ptr<TweenScript> test2{ std::make_unique<TweenScript1>() };
		j.get_to(*test2);

		test2->OnUpdate(0.5f);

		std::unique_ptr<TweenScript> test;

		test = TweenScript::create(j);

		PTGN_LOG("Deserialized script with name: ", test->GetName());

		test->OnUpdate(0.9f);

		// test = TweenScript::create(from_file, 10);
	}

	json entity_json = entity.Serialize<Transform, UUID>();

	PTGN_LOG(entity_json.dump(4));

	Entity test{ manager.CreateEntity() };
	test.Deserialize<Transform, UUID>(entity_json);

	json entity_json2 = test.Serialize<Transform, UUID>();

	PTGN_LOG(entity_json2.dump(4));*/

	/*
	entity.Add<BoxCollider>(V2_float{ 100, 120 });
	manager.Refresh();

	json j = manager;

	std::string s = j.dump(4);

	PTGN_LOG("Manager: ", s);

	Manager manager2;

	j.get_to(manager2);
	*/

	/*json j;
	j["type"] = "TweenMove";
	j["data"] = { { "target_x", 20.0 }, { "target_y", 25.0 }, { "duration", 3.0 } };*/

	/*
	json j = json::array();
	j.push_back(
		{ { "type", "TweenMove" },
		  { "data", { { "target_x", 20.0 }, { "target_y", 25.0 }, { "duration", 3.0 } } } }
	);
	j.push_back({ { "type", "TweenMove2" } });

	PTGN_LOG(j.dump(4));

	j.get_to(script_container);

	// script_container.AddScript<TweenMove>(30.0f, 35.0f, 3.0f);
	// script_container.AddScript<TweenMove2>();

	for (const auto& script : script_container.scripts) {
		script->OnCreate(entity);
	}

	float dt = 0.1f; // Simulated delta time

	// Simulate update loop
	for (int i = 0; i <= 30; ++i) {
		V2_float pos = GetPosition(entity);
		PTGN_LOG("Time: ", static_cast<float>(i) * dt, "s - Position: ", pos);
		for (const auto& script : script_container.scripts) {
			script->OnUpdate(entity, dt);
		}
	}

	json j2 = script_container;
	PTGN_LOG(j2.dump(4));
	*/

	return 0;
}