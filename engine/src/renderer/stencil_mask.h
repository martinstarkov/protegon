#pragma once

// TODO: This should just be render commands behind the GLRenderer API.

namespace ptgn {

class StencilMask {
public:
	// Start writing to stencil buffer.
	static void Enable();

	// Begin drawing only inside the mask.
	static void DrawInside();

	// Begin drawing only outside the mask.
	static void DrawOutside();

	// Reset stencil testing.
	static void Disable();
};

} // namespace ptgn