#pragma once

namespace ptgn {

namespace state {

class State {
public:
	virtual void Enter() {}
	virtual void Exit() {}
};

} // namespace state

} // namespace ptgn