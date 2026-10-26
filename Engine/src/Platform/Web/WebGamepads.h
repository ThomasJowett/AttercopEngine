#pragma once

// Gamepads through the browser's Gamepad API, in the engine's GAMEPAD_* layout when the browser
// reports the standard mapping
class WebGamepads
{
public:
	// Samples every gamepad and registers connections and disconnections; call once per frame before reading input
	static void Poll();

	static bool IsButtonPressed(int joystickSlot, int button);
	// Sticks from -1 to 1 with the dead zone applied, triggers from 0 to 1
	static double GetAxis(int joystickSlot, int axis);
};
