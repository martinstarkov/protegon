// TODO: Fix this demo.

#include "runtime/scripting/script.h"

#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/log.h"
#include "platform/events.h"
#include "core/input/key.h"
#include "runtime/ecs/manager.h"


using namespace ptgn;

struct TestScript : public Script {
	void OnEvent(Event d) override {
		d.Dispatch<event::KeyPressed>(&TestScript::OnKeyPressed, this);
		d.Dispatch<event::MouseMove>(&TestScript::OnMouseMove, this);
	}

	void OnMouseMove() {
		PTGN_LOG("Mouse moved 1");
	}

	void OnKeyPressed(Key k) {
		PTGN_LOG("Key down 1: ", k);
	}
};

struct TestScript2 : public Script {
	void OnEvent(Event d) override {
		d.Dispatch<event::MouseMove>(&TestScript2::OnMouseMove, this);
	}

	void OnMouseMove() {
		PTGN_LOG("Mouse moved 2");
	}
};

struct TestScript3 : public Script {};

struct TestScript4 : public Script {};

int main() {
	// Instead of storing scripts in one container, store them in a unordered map of vectors of
	// scripts. When a script is registered, it uses a similar method to Serialize() to retrieve a
	// list of Enums of what maps that scripts should be inserted into.
	// Then, when cycling we simply cycle through that enum type of the unordered map.
	// When removing a script, we again retrieve all the enum types and remove from all those maps.
	// This changes every dynamic cast to simply a found in map check or not. And then we can static
	// cast as well.

	// TODO: Consider using script_types as a hash type thing instead of having a map with separate
	// function pointers.

	/*
	Manager m;

	auto e1{ m.CreateEntity() };
	auto e2{ m.CreateEntity() };
	auto e3{ m.CreateEntity() };
	auto e4{ m.CreateEntity() };

	Scripts test;

	auto& script1 = AddScript<TestScript>(e1);
	auto& script2 = AddScript<TestScript2>(e1);
	auto& script3 = AddScript<TestScript3>(e1);
	auto& script4 = AddScript<TestScript4>(e1);

	// script3.test		= 69.0f;
	// script4.test		= 79.0f;
	// script4.mouse_index = 33.0f;

	test.AddAction(&GlobalMouseScript::OnMouseMove);
	test.AddAction(&KeyScript::OnKeyPressed, Key::W);
	test.AddAction(&GlobalMouseScript::OnMouseMove);

	test.InvokeActions();

	json j1 = script1.Serialize();
	PTGN_LOG("script1: ", j1.dump(4));

	json j2 = script2.Serialize();
	PTGN_LOG("script2: ", j2.dump(4));

	json j3 = script3.Serialize();
	PTGN_LOG("script3: ", j3.dump(4));

	json j4 = script4.Serialize();
	PTGN_LOG("script4: ", j4.dump(4));

	const auto create_script_from_json = [&](const json& j) {
		std::string class_name{ j.at("type").get<std::string>() };
		auto instance{ impl::ScriptRegistry<impl::IScript>::Instance().Create(class_name) };
		if (instance) {
			instance->entity = m.CreateEntity();
			instance->Deserialize(j);
		}
		return instance;
	};

	auto script1_remade = std::dynamic_pointer_cast<TestScript>(create_script_from_json(j1));
	auto script2_remade = std::dynamic_pointer_cast<TestScript2>(create_script_from_json(j2));
	auto script3_remade = std::dynamic_pointer_cast<TestScript3>(create_script_from_json(j3));
	auto script4_remade = std::dynamic_pointer_cast<TestScript4>(create_script_from_json(j4));

	PTGN_ASSERT(script1_remade);
	PTGN_ASSERT(script2_remade);
	PTGN_ASSERT(script3_remade);
	PTGN_ASSERT(script4_remade);

	// PTGN_ASSERT(script1_remade->mouse_index == 0.0f);
	// PTGN_ASSERT(script2_remade->mouse_index == 0.0f);
	// PTGN_ASSERT(script3_remade->test == 69.0f);
	// PTGN_ASSERT(script4_remade->test == 79.0f);
	// PTGN_ASSERT(script4_remade->mouse_index == 33.0f);

	PTGN_LOG("Scripts deserialized correctly");
	*/
	/*
	std::weak_ptr<CollisionScript> weak = entity.GetComponent<CollisionScript>();

	std::unordered_map<ScriptType, std::vector<std::function<void()>>> queues;

	queues[ScriptType::Collision].emplace_back([weak]() {
		if (auto script = weak.lock()) {
			script->OnCollisionStart(...);
		}
	});

	for (auto& [type, queue] : queues) {
		for (auto& cb : queue) {
			cb();
		}
		queue.clear();
	}*/
}