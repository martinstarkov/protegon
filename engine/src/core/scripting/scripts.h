#ifndef ENGINE_SCRIPTS_H_
#define ENGINE_SCRIPTS_H_

#include <memory>
#include <vector>

#include "events.h"
#include "script_base.h"

namespace engine {

struct Scripts {
	std::vector<std::unique_ptr<BaseScript>> instances;
};

struct KeyScript {
	virtual ~KeyScript()							  = default;
	virtual void OnKeyDown(const KeyDownEvent& event) = 0;
};

struct CollisionScript {
	virtual ~CollisionScript()							  = default;
	virtual void OnCollision(const CollisionEvent& event) = 0;
};

class ExampleKeyScript : public Script<ExampleKeyScript, KeyScript> {
public:
	using Script::Script;
	void OnKeyDown(const KeyDownEvent& event) override;
	void SerializeImpl(nlohmann::json& json) const;
	void DeserializeImpl(const nlohmann::json& json);
};

class ExampleCollisionScript : public Script<ExampleCollisionScript, CollisionScript> {
public:
	using Script::Script;
	void OnCollision(const CollisionEvent& event) override;
	void SerializeImpl(nlohmann::json& json) const;
	void DeserializeImpl(const nlohmann::json& json);
};

} // namespace engine

#endif // ENGINE_SCRIPTS_H_
