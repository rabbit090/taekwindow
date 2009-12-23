#include "ignore.hpp"
#include "debug.hpp"

IgnoreState::IgnoreState(MouseButton button)
:
	MouseDownState(button)
{
}

void IgnoreState::enter() {
	MouseDownState::enter();
	DEBUGLOG("Starting ignore action");
	cursorWindow().setCursor(crIgnore);
}

void IgnoreState::exit() {
	DEBUGLOG("Ending ignore action");
	MouseDownState::exit();
}

bool IgnoreState::onMouseDown(MouseDownEvent const &event) {
	MouseDownState::onMouseDown(event);
	return true;
}

bool IgnoreState::onMouseUp(MouseUpEvent const &event) {
	MouseDownState::onMouseUp(event);
	return true;
}

bool IgnoreState::onMouseMove(MouseMoveEvent const &event) {
	MouseDownState::onMouseMove(event);
	return true;
}
