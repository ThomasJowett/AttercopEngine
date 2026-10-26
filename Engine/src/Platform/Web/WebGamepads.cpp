#include "WebGamepads.h"

#include "Core/Application.h"
#include "Input/GamepadMapping.h"
#include "Input/Joysticks.h"
#include "Events/JoystickEvent.h"
#include "Logging/Instrumentor.h"

#include <emscripten/html5.h>

#include <array>
#include <cmath>
#include <cstring>

namespace
{
	std::array<EmscriptenGamepadEvent, MAX_JOYSTICKS> s_States = {};
	std::array<bool, MAX_JOYSTICKS> s_Connected = {};

	// Latest state of the gamepad in a joystick slot, or nullptr
	const EmscriptenGamepadEvent* GetState(int joystickSlot)
	{
		if (joystickSlot < 0 || joystickSlot >= Joysticks::GetJoystickCount())
			return nullptr;

		int id = Joysticks::GetJoystick(joystickSlot).id;
		if (id < 0 || id >= MAX_JOYSTICKS || !s_Connected[id])
			return nullptr;

		return &s_States[id];
	}

	bool IsStandardMapping(const EmscriptenGamepadEvent& state)
	{
		return std::strcmp(state.mapping, "standard") == 0;
	}
}

void WebGamepads::Poll()
{
	PROFILE_FUNCTION();

	if (emscripten_sample_gamepad_data() != EMSCRIPTEN_RESULT_SUCCESS)
		return;

	int count = emscripten_get_num_gamepads();
	for (int id = 0; id < MAX_JOYSTICKS; id++)
	{
		EmscriptenGamepadEvent state;
		bool connected = id < count
			&& emscripten_get_gamepad_status(id, &state) == EMSCRIPTEN_RESULT_SUCCESS
			&& state.connected;

		if (connected)
			s_States[id] = state;

		if (connected && !s_Connected[id])
		{
			s_Connected[id] = true;

			Joysticks::Joystick joystick;
			joystick.id = id;
			joystick.name = state.id;
			joystick.isMapped = IsStandardMapping(state);
			joystick.axes = state.numAxes;
			joystick.buttons = state.numButtons;
			Joysticks::AddJoystick(joystick);

			JoystickConnected event(id);
			Application::CallEvent(event);
		}
		else if (!connected && s_Connected[id])
		{
			s_Connected[id] = false;
			Joysticks::RemoveJoystick(id);

			JoystickDisconnected event(id);
			Application::CallEvent(event);
		}
	}
}

bool WebGamepads::IsButtonPressed(int joystickSlot, int button)
{
	const EmscriptenGamepadEvent* state = GetState(joystickSlot);
	if (!state)
		return false;

	int index = IsStandardMapping(*state) ? GamepadMapping::StandardButton(button) : button;
	return index >= 0 && index < state->numButtons && state->digitalButton[index];
}

double WebGamepads::GetAxis(int joystickSlot, int axis)
{
	const EmscriptenGamepadEvent* state = GetState(joystickSlot);
	if (!state)
		return 0.0;

	if (!IsStandardMapping(*state))
		return (axis >= 0 && axis < state->numAxes) ? state->axis[axis] : 0.0;

	if (int trigger = GamepadMapping::StandardTriggerButton(axis); trigger >= 0)
		return trigger < state->numButtons ? state->analogButton[trigger] : 0.0;

	int index = GamepadMapping::StandardAxis(axis);
	if (index < 0 || index >= state->numAxes)
		return 0.0;

	double value = state->axis[index];
	return std::fabs(value) >= Joysticks::GetDeadZone() ? value : 0.0;
}
