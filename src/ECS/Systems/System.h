#pragma once
#include "../Types.h"
#include "../Components.h"
#include "BaseSystem.h"
#include "../Entity.h"

static void printSignature(const Signature& s) {
	for (auto& signature : s) {
		std::cout << signature << ",";
	}
	std::cout << std::endl;
}

template <class... Components>
class System : public BaseSystem {
public:
	System() {
		_signature.insert(_signature.end(), { typeid(Components).hash_code()... });
	}
	virtual void onEntityChanged(Entity* entity) override final {
		bool existingEntity = containsEntity(entity->getID());
		if (signaturesMatch(entity)) {
			if (!existingEntity) {
				std::cout << "Adding entity " << entity->getID() << " to system signature: ";
				printSignature(_signature);
				_entities.emplace(entity->getID(), entity);
			}
		} else {
			if (existingEntity) {
				std::cout << "Removing entity " << entity->getID() << " from system signature: ";
				printSignature(_signature);
				_entities.erase(entity->getID());
			}
		}
	}
	virtual void onEntityDestroyed(EntityID entityID) override final {
		auto it = _entities.find(entityID);
		if (it != _entities.end()) {
			_entities.erase(it);
		}
	}
	virtual bool containsEntity(EntityID entityID) override final {
		if (_entities.find(entityID) != _entities.end()) {
			return true;
		}
		return false;
	}
private:
	bool signaturesMatch(Entity* entity) {
		for (const auto& componentID : _signature) {
			const Signature entitySignature = entity->getSignature();
			if (!std::count(entitySignature.begin(), entitySignature.end(), componentID)) {
				return false; // if entity signatures do not contain a system signature, they do not match
			}
		}
		return true;
	}
protected:
	Entities _entities;
	Signature _signature;
};

