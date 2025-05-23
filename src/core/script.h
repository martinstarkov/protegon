#pragma once

#include <functional>
#include <memory>
#include <string_view>
#include <unordered_map>

#include "common/type_info.h"
#include "math/hash.h"
#include "serialization/json.h"

namespace ptgn {

// Script Registry to hold and create scripts
template <typename Base>
class ScriptRegistry {
public:
	static ScriptRegistry& Instance() {
		static ScriptRegistry instance;
		return instance;
	}

	void Register(std::string_view type_name, std::function<std::unique_ptr<Base>()> factory) {
		registry_[Hash(type_name)] = std::move(factory);
	}

	std::unique_ptr<Base> Create(std::string_view type_name) {
		auto it = registry_.find(Hash(type_name));
		return it != registry_.end() ? it->second() : nullptr;
	}

private:
	std::unordered_map<std::size_t, std::function<std::unique_ptr<Base>()>> registry_;
};

template <typename Derived, typename Base>
class Script : public Base {
public:
	// Constructor ensures that the static variable 'is_registered_' is initialized
	Script() {
		// Ensuring static variable is initialized to trigger registration
		(void)is_registered_;
	}

	json Serialize() const override {
		json j	  = *static_cast<const Derived*>(this); // Cast to Derived and serialize
		j["type"] = type_name<Derived>();
		return j;
	}

	void Deserialize(const json& j) override {
		*static_cast<Derived*>(this) = j.get<Derived>(); // Deserialize into derived type
	}

private:
	// Static variable for ensuring class is registered once and for all
	static bool is_registered_;

	// The static Register function handles the actual registration of the class
	static bool Register() {
		ScriptRegistry<Base>::Instance().Register(
			type_name<Derived>(),
			[]() -> std::unique_ptr<Base> { return std::make_unique<Derived>(); }
		);
		return true;
	}
};

// Initialize static variable, which will trigger the Register function
template <typename Derived, typename Base>
bool Script<Derived, Base>::is_registered_ = Script<Derived, Base>::Register();

} // namespace ptgn