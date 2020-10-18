#pragma once

namespace engine {

class Event {
public:
	virtual void Invoke() = 0;
};

} // namespace engine