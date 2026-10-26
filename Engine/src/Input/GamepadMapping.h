#pragma once

#include "Joysticks.h"

// The browser's "standard" gamepad layout (https://w3c.github.io/gamepad/#remapping) in terms of
// the engine's GAMEPAD_BUTTON_* and GAMEPAD_AXIS_* layout
namespace GamepadMapping
{
	// Standard layout button index for an engine button, or -1
	constexpr int StandardButton(int button)
	{
		switch (button)
		{
		case GAMEPAD_BUTTON_A:            return 0;
		case GAMEPAD_BUTTON_B:            return 1;
		case GAMEPAD_BUTTON_X:            return 2;
		case GAMEPAD_BUTTON_Y:            return 3;
		case GAMEPAD_BUTTON_LEFT_BUMPER:  return 4;
		case GAMEPAD_BUTTON_RIGHT_BUMPER: return 5;
		case GAMEPAD_BUTTON_BACK:         return 8;
		case GAMEPAD_BUTTON_START:        return 9;
		case GAMEPAD_BUTTON_GUIDE:        return 16;
		case GAMEPAD_BUTTON_LEFT_THUMB:   return 10;
		case GAMEPAD_BUTTON_RIGHT_THUMB:  return 11;
		case GAMEPAD_BUTTON_DPAD_UP:      return 12;
		case GAMEPAD_BUTTON_DPAD_RIGHT:   return 15;
		case GAMEPAD_BUTTON_DPAD_DOWN:    return 13;
		case GAMEPAD_BUTTON_DPAD_LEFT:    return 14;
		default:                          return -1;
		}
	}

	// Standard layout axis index for an engine stick axis, or -1 (triggers are buttons in the standard layout)
	constexpr int StandardAxis(int axis)
	{
		switch (axis)
		{
		case GAMEPAD_AXIS_LEFT_X:  return 0;
		case GAMEPAD_AXIS_LEFT_Y:  return 1;
		case GAMEPAD_AXIS_RIGHT_X: return 2;
		case GAMEPAD_AXIS_RIGHT_Y: return 3;
		default:                   return -1;
		}
	}

	// Standard layout button index whose analogue value (0 to 1) is an engine trigger axis, or -1
	constexpr int StandardTriggerButton(int axis)
	{
		switch (axis)
		{
		case GAMEPAD_AXIS_LEFT_TRIGGER:  return 6;
		case GAMEPAD_AXIS_RIGHT_TRIGGER: return 7;
		default:                         return -1;
		}
	}
}
