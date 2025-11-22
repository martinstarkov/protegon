#pragma once

namespace ptgn {

class Entity;

class BaseScript {
public:
	explicit BaseScript(Entity entity) : entity_(entity) {}

	virtual ~BaseScript()								 = default;
	virtual std::string_view TypeName() const			 = 0;
	virtual void Serialize(nlohmann::json& json) const	 = 0;
	virtual void Deserialize(const nlohmann::json& json) = 0;

	Entity GetEntity() const {
		return entity_;
	}

protected:
	Entity entity_;
};

template <typename Derived, typename... Interfaces>
class Script : public BaseScript, public Interfaces... {
public:
	explicit Script(Entity entity) : BaseScript(entity) {}

	std::string_view TypeName() const override {
		return type_name<Derived>();
	}

	void Serialize(nlohmann::json& json) const override {
		static_cast<const Derived*>(this)->SerializeImpl(json);
	}

	void Deserialize(const nlohmann::json& json) override {
		static_cast<Derived*>(this)->DeserializeImpl(json);
	}
};

} // namespace ptgn