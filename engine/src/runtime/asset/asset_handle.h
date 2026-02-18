#pragma once

#include <cstdint>

#include "runtime/ecs/entity.h"

namespace ptgn::impl {

struct RefCount {
	std::uint32_t value{ 0 };
};

struct PersistentTag {};

template <typename Derived>
class RefCountedAsset {
public:
	RefCountedAsset() = default;

	explicit RefCountedAsset(Entity e) : entity_{ e } {
		AddRef();
	}

	RefCountedAsset(const RefCountedAsset& other) : entity_{ other.entity_ } {
		AddRef();
	}

	RefCountedAsset(RefCountedAsset&& other) noexcept : entity_{ other.entity_ } {
		other.entity_ = {};
	}

	RefCountedAsset& operator=(const RefCountedAsset& other) {
		if (this != &other) {
			Release();
			entity_ = other.entity_;
			AddRef();
		}
		return *this;
	}

	RefCountedAsset& operator=(RefCountedAsset&& other) noexcept {
		if (this != &other) {
			Release();
			entity_		  = other.entity_;
			other.entity_ = {};
		}
		return *this;
	}

	~RefCountedAsset() noexcept {
		Release();
	}

protected:
	Entity entity_;

private:
	void AddRef() {
		if (!Valid()) {
			return;
		}
		entity_.Get<RefCount>().value++;
	}

	void Release() noexcept {
		if (!Valid()) {
			return;
		}

		auto& rc = entity_.Get<RefCount>();

		if (--rc.value == 0 && !entity_.Has<PersistentTag>()) {
			static_cast<Derived*>(this)->Destroy();
		}
	}

	bool Valid() const {
		return entity_.Has<RefCount>();
	}
};

} // namespace ptgn::impl